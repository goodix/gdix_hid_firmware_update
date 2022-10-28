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
#include "gtx9.h"
#include "../gtp_util.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/hidraw.h>
#include <linux/input.h>
#include <linux/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

GTx9Device::GTx9Device()
{
	m_fd = -1;
	m_firmwareVersionMajor = 0;
	m_firmwareVersionMinor = 0;
	m_sensorID = 0;
	m_configID = 0;
	memset(m_pid, 0, sizeof(m_pid));
	memset(m_vid, 0, sizeof(m_vid));
	m_deviceOpen = false;
}

int GTx9Device::Open(const char *filename)
{
	if (!filename)
		return -EINVAL;

	m_fd = open(filename, O_RDWR);
	if (m_fd < 0)
		return -EINVAL;

	m_deviceOpen = true;

	return SetBasicProperties();
}

void GTx9Device::Close()
{
	if (!m_deviceOpen)
		return;
	close(m_fd);
	m_fd = -1;
	m_deviceOpen = false;
}

bool GTx9Device::IsOpened() { return m_deviceOpen; }

int GTx9Device::GetFd() { return m_fd; }

int GTx9Device::ReadPkg(unsigned int addr, unsigned char *buf, unsigned int len)
{
	uint8_t HidBuf[PACKAGE_LEN] = {0};
	int ret;
	int retry = 5;

	while (retry--) {
		HidBuf[0] = REPORT_ID;			 /* report id */
		HidBuf[1] = I2C_DIRECT_RW;		 /* cmd type */
		HidBuf[2] = 0;					 /* data flag */
		HidBuf[3] = 0;					 /* frame num */
		HidBuf[4] = 7;					 /* data length */
		HidBuf[5] = I2C_READ_FLAG;		 /* read operation flag */
		HidBuf[6] = (addr >> 24) & 0xFF; /* 32 bit addr */
		HidBuf[7] = (addr >> 16) & 0xFF;
		HidBuf[8] = (addr >> 8) & 0xFF;
		HidBuf[9] = addr & 0xFF;
		HidBuf[10] = (len >> 8) & 0xFF; /* 16 bit length */
		HidBuf[11] = len & 0xFF;
		ret = SetReport(HidBuf, 12);
		if (ret < 0) {
			gdix_err("Failed set report, ret:%d\n", ret);
			return ret;
		}
		ret = GetReport(HidBuf);
		if (ret < 0) {
			gdix_err("Failed get report, ret:%d\n", ret);
			return ret;
		}
		if (HidBuf[3] == 0 && HidBuf[4] == len) {
			memcpy(buf, &HidBuf[5], HidBuf[4]);
			return len;
		}
		usleep(1000);
	}

	return -EINVAL;
}

int GTx9Device::Read(unsigned int addr, unsigned char *buf, unsigned int len)
{
	int ret = 0;
	int i = 0;
	uint32_t tmp_addr = addr;
	int offset = 0;
	int pkg_size = PACKAGE_LEN - 12;
	int remain_size;
	int pkg_num;

	if (!m_deviceOpen) {
		gdix_err("Device not open\n");
		return -EINVAL;
	}

	pkg_num = len / pkg_size;
	remain_size = len % pkg_size;

	for (i = 0; i < pkg_num; i++) {
		ret = ReadPkg(tmp_addr + offset, &buf[offset], pkg_size);
		if (ret < 0)
			return ret;
		offset += pkg_size;
	}

	if (remain_size > 0) {
		ret = ReadPkg(tmp_addr + offset, &buf[offset], remain_size);
		if (ret < 0)
			return ret;
	}

	return len;
}

int GTx9Device::Write(unsigned int addr, const unsigned char *buf,
					  unsigned int len)
{
	uint8_t HidBuf[PACKAGE_LEN] = {0};
	uint32_t current_addr = addr;
	uint32_t transfer_length = 0;
	uint32_t pos = 0;
	uint8_t pkg_num = 0;
	int ret;

	if (!m_deviceOpen) {
		gdix_err("Device not open\n");
		return -EINVAL;
	}

	while (pos != len) {
		HidBuf[0] = REPORT_ID;
		HidBuf[1] = I2C_DIRECT_RW;
		if (len - pos > PACKAGE_LEN - 12) {
			transfer_length = PACKAGE_LEN - 12;
			HidBuf[2] = 0x01;
		} else {
			transfer_length = len - pos;
			HidBuf[2] = 0x00;
		}
		HidBuf[3] = pkg_num++;
		HidBuf[4] = transfer_length + 7;
		HidBuf[5] = I2C_WRITE_FLAG;
		HidBuf[6] = (current_addr >> 24) & 0xFF;
		HidBuf[7] = (current_addr >> 16) & 0xFF;
		HidBuf[8] = (current_addr >> 8) & 0xFF;
		HidBuf[9] = current_addr & 0xFF;
		HidBuf[10] = (transfer_length >> 8) & 0xFF;
		HidBuf[11] = transfer_length & 0xFF;
		memcpy(&HidBuf[12], &buf[pos], transfer_length);
		ret = SetReport(HidBuf, transfer_length + 12);
		if (ret < 0) {
			gdix_err("Failed write data to addr=0x%x, len=%d,ret = %d\n",
					 current_addr, transfer_length, ret);
			return ret;
		}
		pos += transfer_length;
		current_addr += transfer_length;
	}

	return len;
}

int GTx9Device::WriteSpeCmd(const unsigned char *buf, unsigned int len)
{
	unsigned char cmdBuf[16] = {0};
	uint16_t checksum = 0;
	uint32_t i;
	int ret;

	if (len > 11) {
		gdix_err("Cmd data len is invalid len=%d\n", len);
		return -EINVAL;
	}

	cmdBuf[0] = 0;
	cmdBuf[1] = 0;
	cmdBuf[2] = len + 3;
	memcpy(&cmdBuf[3], buf, len);
	for (i = 2; i < len + 3; i++)
		checksum += cmdBuf[i];

	cmdBuf[len + 3] = checksum & 0xFF;
	cmdBuf[len + 4] = (checksum >> 8) & 0XFF;
	ret = Write(0x10174, cmdBuf, len + 5);
	if (ret < 0) {
		gdix_err("Failed write cmd\n");
		return ret;
	}
	for (i = 0; i < 20; i++) {
		ret = Read(0x10174, cmdBuf, 2);
		if (ret < 0) {
			gdix_err("Failed read cmd status\n");
			return ret;
		}
		if (cmdBuf[1] == 0x80 && cmdBuf[0] == 0x80) {
			usleep(5000);
			return 0;
		}
		usleep(15000);
	}

	gdix_err("Failed get valid cmd ack:0x%02x sta:0x%02x\n", cmdBuf[1],
			 cmdBuf[0]);
	return -EINVAL;
}

int GTx9Device::SendConfig(unsigned char *config, int len)
{
	int ret;
	uint8_t cmdBuf[5] = {0};
	uint8_t tmpBuf[4096] = {0};

	/* start write cfg */
	cmdBuf[0] = 0x04;
	ret = WriteSpeCmd(cmdBuf, 1);
	if (ret < 0) {
		gdix_err("Failed write cfg prepare cmd\n");
		return ret;
	}

	/* try send config */
	ret = Write(0x13B74, config, len);
	if (ret < 0) {
		gdix_err("Failed write config data\n");
		goto exit;
	}

	/* read back config */
	ret = Read(0x13B74, tmpBuf, len);
	if (ret < 0) {
		gdix_err("Failed read back config\n");
		goto exit;
	}

	if (memcmp(config, tmpBuf, len)) {
		gdix_err("Config data read back compare file\n");
		ret = -EINVAL;
		goto exit;
	}
	cmdBuf[0] = 0x05;
	ret = WriteSpeCmd(cmdBuf, 1);
	if (ret < 0)
		gdix_err("Failed send config data ready cmd\n");

exit:
	cmdBuf[0] = 0x06;
	ret = WriteSpeCmd(cmdBuf, 1);
	if (ret < 0)
		gdix_err("Failed send config write end command\n");

	return ret;
}

int GTx9Device::SendCmd(unsigned char cmd, unsigned char *data, int dataLen)
{
	uint8_t HidBuf[PACKAGE_LEN] = {0};
	int ret;

	if (!m_deviceOpen) {
		gdix_err("Device not open\n");
		return -EINVAL;
	}

	HidBuf[0] = REPORT_ID;
	HidBuf[1] = cmd;
	HidBuf[2] = 0x00;
	HidBuf[3] = 0x00;
	HidBuf[4] = (uint8_t)dataLen;
	memcpy(&HidBuf[5], data, dataLen);
	ret = SetReport(HidBuf, dataLen + 5);
	if (ret < 0)
		gdix_err("Failed send cmd 0x%02x\n", cmd);

	return ret;
}

int GTx9Device::GetReport(unsigned char *buf)
{
	int ret;
	int retry = 5;
	uint8_t rcv_buf[PACKAGE_LEN + 1] = {0};

	while (retry--) {
		rcv_buf[0] = REPORT_ID;
		ret = ioctl(m_fd, HIDIOCGFEATURE(PACKAGE_LEN), rcv_buf);
		if (ret == PACKAGE_LEN && rcv_buf[0] == REPORT_ID) {
			memcpy(buf, rcv_buf, PACKAGE_LEN);
			return 0;
		}
		usleep(1000);
	}

	gdix_err("Failed get report ret:%d rcvbuf[0]:0x%02x\n", ret, rcv_buf[0]);
	return -EINVAL;
}

int GTx9Device::SetReport(unsigned char *buf, int len)
{
	int retry = 5;
	int ret;

	while (retry--) {
		ret = ioctl(m_fd, HIDIOCSFEATURE(len), buf);
		if (ret == len)
			return 0;
		usleep(1000);
	}

	gdix_err("Failed set report, ret:%d\n", ret);
	return -EINVAL;
}

int GTx9Device::GetFirmwareProps(const char *deviceName, char *props_buf,
								 int len)
{
	int ret;

	ret = Open(deviceName);
	if (ret) {
		gdix_dbg("failed open device:%s\n", deviceName);
		return -1;
	}

	snprintf(props_buf, len, "%d.%d", GetFirmwareVersionMajor(),
			 GetFirmwareVersionMinor());
	return 0;
}

int GTx9Device::SetBasicProperties()
{
	uint8_t tempBuf[14] = {0};
	int ret;
	// uint8_t main_ver;
	uint8_t vice_ver;
	uint8_t inter_ver;
	uint8_t cfg_ver;

	/* read pid/vid/sensorid */
	ret = Read(0x1001E, tempBuf, 14);
	if (ret < 0) {
		gdix_err("Failed read PID/VID\n");
		return ret;
	}
	gdix_dbg_array(tempBuf, 14);
	memcpy(m_pid, tempBuf, 8);
	memcpy(m_vid, tempBuf + 8, 4);
	m_sensorID = tempBuf[13];

	// main_ver = tempBuf[9];
	vice_ver = tempBuf[10];
	inter_ver = tempBuf[11];

	/* read config id/version */
	ret = Read(0x10076, tempBuf, 5);
	if (ret < 0) {
		gdix_err("Failed read config id/version\n");
		return ret;
	}
	m_configID = *((uint32_t *)tempBuf);
	cfg_ver = tempBuf[4];

	// m_firmwareVersionMajor = main_ver;
	m_firmwareVersionMajor = 0;
	m_firmwareVersionMinor = (vice_ver << 16) | (inter_ver << 8) | cfg_ver;

	gdix_info("PID:%s\n", m_pid);
	gdix_info("VID:%02x %02x %02x %02x\n", m_vid[0], m_vid[1], m_vid[2],
			  m_vid[3]);
	gdix_info("sensorID:%d\n", m_sensorID);
	gdix_info("configID:%08x\n", m_configID);
	gdix_info("configVer:%02x\n", cfg_ver);
	gdix_info("version:%d.%d\n", m_firmwareVersionMajor,
			  m_firmwareVersionMinor);

	return 0;
}
