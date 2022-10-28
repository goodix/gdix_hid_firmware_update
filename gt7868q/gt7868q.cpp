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

#include "gt7868q.h"
#include "../gtp_util.h"

#define CFG_START_ADDR 0x96F8
#define VER_ADDR 0x4014

GT7868QDevice::GT7868QDevice() { gdix_dbg("Enter GT7868QDevice\n"); }

GT7868QDevice::~GT7868QDevice() {}

unsigned short GT7868QDevice::ChecksumU8_ys(unsigned char *data, int len)
{
	unsigned short chksum = 0;
	int i = 0;

	for (i = 0; i < len - 2; i++)
		chksum += data[i];
	gdix_dbg("calu chksum:0x%x, chksum in fw:0x%x\n", chksum,
			 ((data[len - 2] << 8) | data[len - 1]));
	return chksum - ((data[len - 2] << 8) | data[len - 1]);
}

int GT7868QDevice::SetBasicProperties()
{
	int ret;
	unsigned char fw_info[32] = {0};
	m_firmwareVersionMajor = 20;
	m_firmwareVersionMinor = 20;
	m_sensorID = 0xF;
	unsigned char cfg_ver = 0;
	int retry = 10;
	unsigned short chksum;
	unsigned char buf_dis_report_coor[] = {0x33, 0x00, 0x00, 0x00, 0x33};
	unsigned char buf_en_report_coor[] = {0x34, 0x00, 0x00, 0x00, 0x34};

	if (!m_deviceOpen) {
		gdix_err("Please open device first\n");
		return -1;
	}
	/*dis report coor*/
	gdix_info("disable report coor in read version\n");
	retry = 3;
	do {
		ret = Write(CMD_ADDR, buf_dis_report_coor, sizeof(buf_dis_report_coor));
		if (ret < 0) {
			gdix_err("Failed disable report coor\n");
		}
	} while (--retry);
	// read cfg ver
	retry = 10;
	do {
		ret = Read(CFG_START_ADDR, &cfg_ver, 1);
		if (ret < 0) {
			gdix_dbg("Failed read cfg VERSION, retry=%d\n", retry);
			continue;
		}
		gdix_dbg("cfg ver:%d\n", cfg_ver);
		ret = Read(VER_ADDR, fw_info, sizeof(fw_info));
		if (ret < 0)
			gdix_dbg("Failed read VERSION, retry=%d\n", retry);
		else
			break;
	} while (--retry);
	gdix_dbg("Fw_info array\n");
	gdix_dbg("0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n", fw_info[0], fw_info[1],
			 fw_info[2], fw_info[3], fw_info[4]);
	gdix_dbg("0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n", fw_info[5],
			 fw_info[6], fw_info[7], fw_info[8], fw_info[9], fw_info[10]);
	gdix_dbg("0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n", fw_info[11],
			 fw_info[12], fw_info[13], fw_info[14], fw_info[15], fw_info[16]);
	gdix_dbg("0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n", fw_info[17], fw_info[18],
			 fw_info[19], fw_info[20], fw_info[21]);
	gdix_dbg("0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n", fw_info[22],
			 fw_info[23], fw_info[24], fw_info[25], fw_info[26], fw_info[27]);
	if (!retry) {
		ret = -1;
		goto ver_end;
	}

	/*check fw version*/
	chksum = ChecksumU8_ys(fw_info, sizeof(fw_info));
	if (chksum) {
		gdix_err("fw version check sum error:0x%x\n", chksum);
		ret = -2;
		goto ver_end;
	}

	memcpy(m_pid, &fw_info[14], 4);
	if (!memcmp(m_pid, "7869", 4)) {
		memcpy(m_pid, "7868Q", 5);
		m_pid[5] = '\0';
	} else {
		m_pid[4] = '\0';
	}
	gdix_dbg("pid:%s\n", m_pid);
	m_sensorID = fw_info[27] & 0xff;
	gdix_dbg("sensorID:%d\n", m_sensorID);

	m_firmwareVersionMajor = fw_info[23];
	m_firmwareVersionMinor =
		((fw_info[24] << 16) | (fw_info[25]) << 8) | cfg_ver;

	gdix_dbg("version:0x%x,0x%x\n", m_firmwareVersionMajor,
			 m_firmwareVersionMinor);

	ret = 0;
ver_end:
	/*en report coor*/
	gdix_info("enable report coor\n");
	if (Write(CMD_ADDR, buf_en_report_coor, sizeof(buf_en_report_coor)) < 0) {
		gdix_err("Failed enable report coor in read version\n");
	}
	return ret;
}
