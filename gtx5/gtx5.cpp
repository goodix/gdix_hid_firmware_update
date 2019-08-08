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
#include "gtx5.h"

GTx5Device::GTx5Device()
{
    m_firmwareVersionMajor = 0;
    m_firmwareVersionMinor = 0;
    m_sensorID = 16; /* > 15 invalid ID */
    memset(m_pid, 0, sizeof(m_pid));
    m_hidDevType = HID_MACHINE;
    m_deviceOpen = false;
    m_bCancel = false;
    m_fd = -1;
}

 int GTx5Device::GetFirmwareProps(const char *deviceName, char *props_buf, int len)
 {
     int ret;
 
     ret = Open(deviceName);
     if (ret) {
         gdix_dbg("failed open device:%s\n", deviceName);
         return -1;
     }
 
     snprintf(props_buf, len, "%d.%d",
          GetFirmwareVersionMajor(),
          GetFirmwareVersionMinor());
     return 0;
 }

int GTx5Device::Open(const char *filename)
{
    int rc;

    if (!filename)
        return -EINVAL;
    m_fd = open(filename, O_RDWR);
    if (m_fd < 0)
        return -1;
    m_deviceOpen = true;
    m_inputReportSize = 65;
    m_outputReportSize = 65;
    m_inputReport = new unsigned char[m_inputReportSize]();
    if (!m_inputReport) {
        errno = -ENOMEM;
        rc = -1;
        goto error;
    }
    m_outputReport = new unsigned char[m_outputReportSize]();
    if (!m_outputReport) {
        errno = -ENOMEM;
        rc = -1;
        goto error;
    }
    /* get active firmware info */
    return SetBasicProperties();
error:
    Close();
    return rc;
}


 void GTx5Device::Close()
 {
         if (!m_deviceOpen)
                 return;
         close(m_fd);
         m_fd = -1;
         m_deviceOpen = false;
     delete[] m_inputReport;
     m_inputReport = NULL;
     delete[] m_outputReport;
     m_outputReport = NULL;
 }
 int GTx5Device::GetReport(unsigned char reportId, unsigned char *buf)
 {
     int ret;
     unsigned char rcv_buf[66] = {0};
     if (!m_deviceOpen)
         return -1;
     
     rcv_buf[0] = reportId;
     ret = ioctl(m_fd, HIDIOCGFEATURE(m_inputReportSize), rcv_buf);
     if (ret < 0) {
         gdix_dbg("failed get feature retry, ret=%d\n", ret);
         return ret;
     } else {
         if (rcv_buf[0] == reportId) {
             memcpy(buf, rcv_buf, 65);
             return 0;
         } else {
             gdix_dbg("Get Wrong reportId:id=0x%x\n", rcv_buf[0]);
             return -1;
         }
     }
 }
 int GTx5Device::Read(unsigned short addr, unsigned char *buf, unsigned int len)
 {
     int ret;
     int retry = 0;
     unsigned char HidBuf[65] =  {0x0e, _I2C_DIRECT_RW,0,0,5,  1};
     unsigned int pkg_index = 0;
     unsigned int read_data_len = 0;
 re_start:
     pkg_index = 0;
     read_data_len = 0;
     HidBuf[0] = 0x0e;
     HidBuf[1] = _I2C_DIRECT_RW;
     HidBuf[2] = 0;
     HidBuf[3] = 0;
     HidBuf[4] = 5;
     HidBuf[5] = 1; /* read operation flag */
     HidBuf[6] = (addr >> 8) & 0xff;
     HidBuf[7] = addr & 0xff;
     HidBuf[8] = (len >> 8) & 0xff;
     HidBuf[9] = len & 0xff;
     ret = Write(HidBuf, 10);

     if (ret < 0) {
         gdix_dbg("Failed send read start package, ret = %d\n", ret);
         return -1;
     }
     do {
         ret = GetReport(0x0e, HidBuf);
         if (ret) {
             gdix_dbg("Failed read addr=0x%x, len=%d\n", addr, len);
             break;
         } else {
             if (pkg_index != HidBuf[3]) {
                 if (retry++ < GDIX_RETRY_TIMES) {
                     gdix_dbg("Read retry %d, pkg_index %d != HidBuf[3](%d)\n",
                          retry,	pkg_index, HidBuf[3]);
                     usleep(1000);
                     goto re_start;
                 }
                 ret = -E_HID_PKG_INDEX;
                 break;
             } else {
                 if (HidBuf[4] == len - read_data_len) {
                     memcpy(buf + read_data_len, &HidBuf[5], HidBuf[4]);
                     read_data_len += HidBuf[4];
                     pkg_index++;
                 } else {
                     gdix_dbg("Data length err: %d != %d\n", 
                          HidBuf[4], len - read_data_len);
                     gdix_dbg_array(HidBuf,6);
                     if (retry++ < GDIX_RETRY_TIMES) {
                         gdix_dbg("Read retry: %d\n", retry);
                         usleep(1000);
                         goto re_start;
                     }
                     ret = -E_HID_PKG_LEN;
                     break;
                 }
             }
         }
     } while (read_data_len != len && (retry < GDIX_RETRY_TIMES));
     if (ret < 0)
         return ret;
     else
         return len;
 }
 int GTx5Device::Write(unsigned short addr, const unsigned char *buf,
                  unsigned int len)
 {
     unsigned char tmpBuf[65] =  {0x0e,_I2C_DIRECT_RW, 0, 0, 5};
     unsigned short current_addr = addr;
     unsigned int pos = 0, transfer_length = 0;
     bool has_follow_pkg = false;
     unsigned char pkg_num = 0;
     int ret = 0;
     while (pos != len) {
         if (len - pos > m_outputReportSize - GDIX_PRE_HEAD_LEN - GDIX_DATA_HEAD_LEN) {
             /* data len more than 55, need to send with subpackages */
             transfer_length = m_outputReportSize - GDIX_PRE_HEAD_LEN - GDIX_DATA_HEAD_LEN;
             has_follow_pkg = true;
         } else {
             transfer_length = len - pos;
             has_follow_pkg = false;
         }
         if (has_follow_pkg)
             tmpBuf[2] = 0x01; /* set follow-up package flag */
         else
             tmpBuf[2] = 0x00;
         tmpBuf[3] = pkg_num++; /* set pack num */
         /* set HID package length = data_len + GDIX_DATA_HEAD_LEN */
         tmpBuf[4] = (unsigned char)(transfer_length + GDIX_DATA_HEAD_LEN);
         tmpBuf[5] = 0;		/* write operation flag */
         tmpBuf[6] = (current_addr >> 8) & 0xff;
         tmpBuf[7] = current_addr & 0xff;
         tmpBuf[8] = (unsigned char)((transfer_length >> 8) & 0xff);
         tmpBuf[9] = (unsigned char)(transfer_length & 0xff);
         memcpy(&tmpBuf[GDIX_PRE_HEAD_LEN + GDIX_DATA_HEAD_LEN], &buf[pos], transfer_length);
         ret = Write(tmpBuf, transfer_length + GDIX_PRE_HEAD_LEN + GDIX_DATA_HEAD_LEN);
         if (ret < 0) {
             gdix_dbg("Failed write data to addr=0x%x, len=%d,ret = %d\n",
                 current_addr, transfer_length, ret);
             break;
         } else {
             pos += transfer_length;
             current_addr += transfer_length;
         }
     }
     if (ret < 0)
         return -1;
     else
         return len;
 }
 /* 
  * write data to IC directly buf_len <= 65 
  * return < 0 failed
 */
 int GTx5Device::Write(const unsigned char *buf,
                  unsigned int len)
 {
     unsigned char temp_buf[m_outputReportSize];
     int ret;
     int retry = GDIX_RETRY_TIMES;
     if (!m_deviceOpen)
         return -1;
     if (m_outputReportSize < len)
         return -1;
         memset(temp_buf, 0, m_outputReportSize);
         memcpy(&temp_buf[0], buf, len);
         temp_buf[0] = 0x0E;
     gdix_dbg_array(temp_buf, len);
         do {
         ret = ioctl(m_fd, HIDIOCSFEATURE(len), temp_buf);
         if (ret < 0) {
             if (!m_deviceOpen || m_bCancel) {
                 gdix_dbg("Operation beCancled or m_fd closed\n");
                 break;
             }
             gdix_dbg("failed set feature, retry: ret=%d,retry:%d\n", ret,retry);
             usleep(10000);
         } else {
             break;
         }
         } while(--retry);
         return ret;
 }
 
 int GTx5Device::SetBasicProperties()
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
         ret = Read(GTX5_VERSION_ADDR, fw_info, sizeof(fw_info));
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

