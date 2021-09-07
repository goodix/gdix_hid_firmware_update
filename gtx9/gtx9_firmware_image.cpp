/*
 * Copyright (C) 2017 Goodix Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include "../gtp_util.h"

#include "gtx9_firmware_image.h"


GTX9FirmwareImage::GTX9FirmwareImage()
{
    m_firmwareCfgID = 0;
    memset(m_firmwarePID, 0, sizeof(m_firmwarePID));
    memset(m_firmwareVID, 0, sizeof(m_firmwareVID));
    memset(&m_firmwareSummary, 0, sizeof(m_firmwareSummary));
}

int GTX9FirmwareImage::Initialize(const char *filename)
{
    int ret;

	m_initialized = false;

	if (GetDataFromFile(filename) < 0) {
        gdix_err("Failed get firmware\n");
        return -EINVAL;
	}

    gdix_info("Parse firmware file\n");
    ret = ParseFirmware();
    if (ret < 0) {
        gdix_err("Failed parse firmware\n");
        return ret;
    }

    m_initialized = true;
    return 0;
}

void GTX9FirmwareImage::Close()
{
    m_initialized = false;
    delete[] m_firmwareData;
}

int GTX9FirmwareImage::GetDataFromFile(const char* filename)
{
    int fw_fd;
    int ret;

	fw_fd = open(filename, O_RDONLY);
	if (fw_fd < 0){
		gdix_err("file:%s, ret:%d\n", filename, fw_fd);
		return fw_fd;
	}

	m_totalSize = lseek(fw_fd, 0, SEEK_END);
	lseek(fw_fd, 0, SEEK_SET);

    m_firmwareData = new unsigned char[m_totalSize]();

	ret = read(fw_fd, m_firmwareData, m_totalSize);
	if (ret != m_totalSize) {
		gdix_err("Failed read file: %s, ret=%d\n", filename, ret);
		ret = -EINVAL;
		goto err_out;
	}

	m_firmwareSize = ((m_firmwareData[3] << 24) | (m_firmwareData[2] << 16) |
        (m_firmwareData[1] << 8) | m_firmwareData[0]) + 8;

	if (m_firmwareSize < m_totalSize) {
		gdix_dbg("Check firmware size:%d < file size:%d\n",
			m_firmwareSize, m_totalSize);
		gdix_dbg("This bin file may contain a config bin.\n");
        m_configSize = m_totalSize - m_firmwareSize - 64;
        gdix_dbg("config size:%d\n", m_configSize);
		hasConfig = true;
	}

	close(fw_fd);
	return 0;

err_out:
	delete[] m_firmwareData;
	close(fw_fd);
	return ret;
}

int GTX9FirmwareImage::ParseFirmware()
{
    struct firmware_summary *fw_summary = &m_firmwareSummary;
    uint32_t checksum = 0;
    uint32_t fw_offset;
    uint32_t info_offset;
    uint8_t cfg_ver = 0;
    uint8_t tmp_buf[9] = {0};
    int i;

    memcpy(fw_summary, m_firmwareData, sizeof(*fw_summary));

    /* check firmware size */
    if (m_firmwareSize != (int)(fw_summary->size + 8)) {
        gdix_err("Bad firmware, size not match, %d != %d\n",
            m_firmwareSize, fw_summary->size + 8);
        return -EINVAL;
    }

    for (i = 8; i < m_firmwareSize; i += 2)
        checksum += m_firmwareData[i] + (m_firmwareData[i + 1] << 8);

    /* byte order change, and check */
    if (checksum != fw_summary->checksum) {
        gdix_err("Bad firmware, checksum error\n");
        return -EINVAL;
    }

    if (fw_summary->subsys_num > FW_SUBSYS_MAX_NUM) {
        gdix_err("Bad firmware, invalid subsys num:%d\n", fw_summary->subsys_num);
        return -EINVAL;
    }

    /* parse subsystem info */
    fw_offset = FW_HEADER_SIZE;
    for (i = 0; i < fw_summary->subsys_num; i++) {
		info_offset = FW_SUBSYS_INFO_OFFSET + i * FW_SUBSYS_INFO_SIZE;
        fw_summary->subsys[i].type = m_firmwareData[info_offset];
        fw_summary->subsys[i].size = *(uint32_t *)&m_firmwareData[info_offset + 1];
        fw_summary->subsys[i].flash_addr = *(uint32_t *)&m_firmwareData[info_offset + 5];
		if ((int)fw_offset > m_firmwareSize) {
			gdix_err("Sybsys offset exceed Firmware size\n");
			return -EINVAL;
		}
		fw_summary->subsys[i].data = m_firmwareData + fw_offset;
		fw_offset += fw_summary->subsys[i].size;
    }

    memcpy(tmp_buf, fw_summary->fw_pid, 8);
	gdix_info("Firmware package protocol: V%u\n", fw_summary->protocol_ver);
	gdix_info("Firmware PID:GT%s\n", tmp_buf);
	gdix_info("Firmware VID:%02x %02x %02x %02x\n",
            fw_summary->fw_vid[0], fw_summary->fw_vid[1],
            fw_summary->fw_vid[2], fw_summary->fw_vid[3]);
	gdix_info("Firmware chip type:%02X\n", fw_summary->chip_type);
	gdix_info("Firmware size:%u\n", fw_summary->size);
	gdix_info("Firmware subsystem num:%u\n", fw_summary->subsys_num);

    memcpy(m_firmwarePID, fw_summary->fw_pid, 8);
    memcpy(m_firmwareVID, fw_summary->fw_vid, 4);

    if (hasConfig) {
        cfg_ver = m_firmwareData[m_firmwareSize + 64 + 34];
        gdix_info("cfg_ver:%02x\n", cfg_ver);
    }

    // m_firmwareVersionMajor = m_firmwareVID[1];
    m_firmwareVersionMajor = 0;
    m_firmwareVersionMinor = (m_firmwareVID[2] << 16) | (m_firmwareVID[3] << 8) | cfg_ver;

#if 0
	for (i = 0; i < fw_summary->subsys_num; i++) {
		gdix_dbg("------------------------------------------\n");
		gdix_dbg("Index:%d\n", i);
		gdix_dbg("Subsystem type:%02X\n", fw_summary->subsys[i].type);
		gdix_dbg("Subsystem size:%u\n", fw_summary->subsys[i].size);
		gdix_dbg("Subsystem flash_addr:%08X\n", fw_summary->subsys[i].flash_addr);
	}
#endif    

    return 0;
}



