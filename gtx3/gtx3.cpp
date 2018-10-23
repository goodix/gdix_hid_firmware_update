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
         m_sensorID = 2;
     int retry = 10;
     if (!m_deviceOpen) {
         gdix_err("Please open device first\n");
         return -1;
     }
     do {
         ret = Read(0x8240, fw_info, sizeof(fw_info));
         if (ret < 0)
             gdix_dbg("Failed read VERSION, retry=%d\n", retry);
         else
             break;
     } while (--retry);
     if (!retry)
         return -1;
     memcpy(m_pid, fw_info, 4);
     m_pid[4] = '\0';
     m_sensorID = fw_info[10] & 0x0F;
     m_firmwareVersionMajor = (fw_info[5] >> 4) * 10 + (fw_info[5] & 0x0F);
     m_firmwareVersionMinor = (fw_info[6] >> 4) * 10 + (fw_info[6] & 0x0F);
         return 0;  
 }

 