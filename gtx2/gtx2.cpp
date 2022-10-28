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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/hidraw.h>
#include <linux/input.h>
#include <linux/types.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/inotify.h>

#include "../gtp_util.h"
#include "gtx2.h"

int GTx2Device::SetBasicProperties()
{
	int ret;
	unsigned char fw_info[12] = {0};
	m_firmwareVersionMajor = 20;
	m_firmwareVersionMinor = 20;
	m_sensorID = 2;
	unsigned char cfg_ver = 0;
	int retry = 10;

	if (!m_deviceOpen) {
		gdix_err("Please open device first\n");
		return -1;
	}

	do {
		ret = Read(0x8050, &cfg_ver, 1);
		if (ret < 0) {
			gdix_dbg("Failed read cfg VERSION, retry=%d\n", retry);
			continue;
		}

		ret = Read(0x8140, fw_info, sizeof(fw_info));
		if (ret < 0)
			gdix_dbg("Failed read VERSION, retry=%d\n", retry);
		else
			break;
	} while (--retry);

	if (!retry)
		return -1;

	memcpy(m_pid, fw_info, 4);
	m_pid[4] = '\0';
	gdix_dbg("pid:%s\n", m_pid);
	m_sensorID = fw_info[10] & 0x0f;
	gdix_dbg("sensorID:%d\n", m_sensorID);

	if (!memcmp(m_pid, "7288", 4)) {
		m_firmwareVersionMajor = fw_info[5];
		m_firmwareVersionMinor = ((fw_info[6] << 16) | (0x00) << 8) | cfg_ver;
	} else {
		m_firmwareVersionMajor = fw_info[4];
		m_firmwareVersionMinor =
			((fw_info[5] << 16) | (fw_info[6]) << 8) | cfg_ver;
	}

	gdix_dbg("cfg version:%d\n", cfg_ver);
	gdix_dbg("version:0x%x,0x%x\n", m_firmwareVersionMajor,
			 m_firmwareVersionMinor);

	return 0;
}

int GTx2Device::GetCfgVersion()
{
	int ret;
	unsigned char cfg_ver;
	unsigned char pid[5] = {0};
	int sensor_id;
	unsigned char fw_info[12] = {0};

	ret = Read(0x8050, &cfg_ver, 1);
	if (ret < 0) {
		gdix_err("Failed read cfg VERSION\n");
		return ret;
	}
	ret = Read(0x8140, fw_info, sizeof(fw_info));
	if (ret < 0) {
		gdix_err("Failed read VERSION\n");
		return ret;
	}

	memcpy(pid, fw_info, 4);
	sensor_id = fw_info[10] & 0x0f;
	fprintf(stdout, "PID:%s\n", pid);
	fprintf(stdout, "sensorID:%d\n", sensor_id);
	fprintf(stdout, "cfg version:%d\n", cfg_ver);
	return 0;
}
