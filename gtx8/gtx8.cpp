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
 #include "gtx8.h"
 
 #define CFG_START_ADDR 0x60DC
 #define VER_ADDR	0x452C

 GTx8Device::GTx8Device()
 {
    gdix_dbg("Enter GTx8Device\n");
 }

 GTx8Device::~GTx8Device()
 {
    
 }

 int GTx8Device::SetBasicProperties()
 {
    int ret;
    unsigned char fw_info[72] = {0};
    m_firmwareVersionMajor = 20;
    m_firmwareVersionMinor = 20;
    m_sensorID = 0xF;
    unsigned char cfg_ver = 0;
    int retry = 10;
    unsigned char chksum;

     if (!m_deviceOpen) {
         gdix_err("Please open device first\n");
         return -1;
     }
     do {
        ret = Read(CFG_START_ADDR,&cfg_ver,1);
		if(ret < 0){
			gdix_dbg("Failed read cfg VERSION, retry=%d\n", retry);
			continue;
		}
        gdix_dbg("cfg ver:%d\n",cfg_ver);
         ret = Read(VER_ADDR, fw_info, sizeof(fw_info));
         if (ret < 0)
             gdix_dbg("Failed read VERSION, retry=%d\n", retry);
         else
             break;
     } while (--retry);
     gdix_dbg("Fw_info array\n");
     gdix_dbg("0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",fw_info[0],fw_info[1],fw_info[2],fw_info[3],fw_info[4]);
     gdix_dbg("0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",fw_info[5],fw_info[6],fw_info[7],fw_info[8],fw_info[9],fw_info[10]);
     gdix_dbg("0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",fw_info[11],fw_info[12],fw_info[13],fw_info[14],fw_info[15],fw_info[16]);
     gdix_dbg("0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",fw_info[17],fw_info[18],fw_info[19],fw_info[20],fw_info[21]);
     if (!retry)
         return -1;
    
     /*check fw version*/
     chksum = ChecksumU8(fw_info,sizeof(fw_info));
     if (chksum) {
     	gdix_err("fw version check sum error:%d\n",chksum);
	return -2;
     }
     
    memcpy(m_pid, &fw_info[9], 4);
	m_pid[4] = '\0';
	gdix_dbg("pid:%s\n",m_pid);
	m_sensorID = fw_info[21]&0x0f;
	gdix_dbg("sensorID:%d\n",m_sensorID);

	m_firmwareVersionMajor = fw_info[18];
	m_firmwareVersionMinor = ((fw_info[19] << 16) | (fw_info[20])<<8) | cfg_ver;

	gdix_dbg("version:0x%x,0x%x\n",m_firmwareVersionMajor,m_firmwareVersionMinor);

    return 0;  
 }

 
