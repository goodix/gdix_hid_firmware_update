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
#include "gtx2.h"



int GTx2Device::SetBasicProperties()
{
	int ret;
	unsigned char fw_info[12] = {0};
        m_firmwareVersionMajor = 20;
	m_firmwareVersionMinor = 20;
        m_sensorID = 2;
	int retry = 10;

	if (!m_deviceOpen) {
		gdix_err("Please open device first\n");
		return -1;
	}
	
	do {
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
	gdix_dbg("pid:%s\n",m_pid);
	m_sensorID = 16;

	m_firmwareVersionMajor = fw_info[4];
	m_firmwareVersionMinor = ((fw_info[5] << 8) | (fw_info[6])) ;

	gdix_dbg("version:0x%x,0x%x\n",m_firmwareVersionMajor,m_firmwareVersionMinor);

    return 0;  
}
