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
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "gtp_util.h"
#include "firmware_image.h"

FirmwareImage::FirmwareImage()
{
	m_initialized = false;
	m_firmwareSize = 0;
	memset(m_pid, 0, sizeof(m_pid));
	m_firmwareVersionMajor = 0;
	m_firmwareVersionMinor = 0;
	m_firmwareData = NULL;
}


int FirmwareImage::Initialize(const char *filename)
{
	int ret, i, j;
	int fw_fd;
	unsigned short check_sum = 0;
	unsigned int data_len;

	fw_fd = open(filename, O_RDONLY);

	if (fw_fd < 0){
		perror("Cannot open file\n");
		gdix_err("file:%s, ret:%d\n", filename, fw_fd);
		return fw_fd;
	}

	m_firmwareSize = lseek(fw_fd, 0, SEEK_END);
	lseek(fw_fd, 0, SEEK_SET);

	m_firmwareData = new unsigned char[m_firmwareSize]();

	ret = read(fw_fd, m_firmwareData, m_firmwareSize);
	if (ret != m_firmwareSize) {
		gdix_dbg("Failed read file: %s, ret=%d\n",
			filename, ret);
		ret = -1;
		goto err_out;
	} 

	for (i = 6, check_sum = 0; i < m_firmwareSize; i++)
		check_sum += m_firmwareData[i];

	if (check_sum != (m_firmwareData[4] << 8 | m_firmwareData[5])) {
		gdix_dbg("Check_sum err  0x%x != 0x%x\n",
			check_sum, (m_firmwareData[4] << 8 | m_firmwareData[5]));
		ret = -2;
		goto err_out;
	}

	data_len = m_firmwareData[0] << 24 | m_firmwareData[1] << 16 |
			m_firmwareData[2] << 8 | m_firmwareData[3];
	if (data_len + 6 !=  m_firmwareSize) {
		gdix_dbg("Check file len failed %d != %d\n",
			data_len + 6, m_firmwareSize);
		ret = -3;
		goto err_out;
	}

	/* get PID VID */
	for (i = 0, j = 0; i < sizeof(m_pid); i++)
		if (m_firmwareData[FW_IMAGE_PID_OFFSET + i] != 0)
			m_pid[j++] = m_firmwareData[FW_IMAGE_PID_OFFSET + i];

	m_firmwareVersionMajor = (m_firmwareData[FW_IMAGE_VID_OFFSET] >> 4) * 10
				+ (m_firmwareData[FW_IMAGE_VID_OFFSET] & 0x0F);
	m_firmwareVersionMinor = (m_firmwareData[FW_IMAGE_VID_OFFSET + 1] >> 4) * 10
				+ (m_firmwareData[FW_IMAGE_VID_OFFSET + 1] & 0x0F);
	//memcpy(m_pid, &m_firmwareData[FW_IMAGE_PID_OFFSET], sizeof(m_pid));
	m_initialized = true;
	close(fw_fd);
	return 0;

err_out:
	m_initialized = false;
	delete[] m_firmwareData;
	close(fw_fd);
	return ret;
}

void FirmwareImage::Close()
{
	if (!m_initialized)
		return;

	m_initialized = false;
	delete[] m_firmwareData;
}
