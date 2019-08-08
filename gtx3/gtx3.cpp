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

 #include "../gtp_util.h"
 #include "gtx3.h"

 GTx3Device::GTx3Device()
 {
    gdix_dbg("Enter GTx3Device\n");
 }

 GTx3Device::~GTx3Device()
 {
    
 }

 int GTx3Device::SetBasicProperties()
 {
    int ret;
    unsigned char fw_info[12] = {0};
    m_firmwareVersionMajor = 20;
    m_firmwareVersionMinor = 20;
    m_sensorID = 0xF;
    unsigned char cfg_ver = 0;
    int retry = 10;

     if (!m_deviceOpen) {
         gdix_err("Please open device first\n");
         return -1;
     }
     do {
        ret = Read(0x8050,&cfg_ver,1);
		if(ret < 0){
			gdix_dbg("Failed read cfg VERSION, retry=%d\n", retry);
			continue;
		}
        gdix_dbg("cfg ver:%d\n",cfg_ver);
         ret = Read(0x8240, fw_info, sizeof(fw_info));
         if (ret < 0)
             gdix_dbg("Failed read VERSION, retry=%d\n", retry);
         else
             break;
     } while (--retry);
     gdix_dbg("Fw_info array\n");
     gdix_dbg("0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",fw_info[0],fw_info[1],fw_info[2],fw_info[3],fw_info[4]);
     gdix_dbg("0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",fw_info[5],fw_info[6],fw_info[7],fw_info[8],fw_info[9],fw_info[10]);
     if (!retry)
         return -1;
     
    memcpy(m_pid, fw_info, 4);
	m_pid[4] = '\0';
	gdix_dbg("pid:%s\n",m_pid);
	m_sensorID = fw_info[10]&0x0f;
	gdix_dbg("sensorID:%d\n",m_sensorID);

	m_firmwareVersionMajor = fw_info[5];
	m_firmwareVersionMinor = ((fw_info[6] << 16) | (fw_info[7])<<8) | cfg_ver;

	gdix_dbg("version:0x%x,0x%x\n",m_firmwareVersionMajor,m_firmwareVersionMinor);

    return 0;  
 }

 