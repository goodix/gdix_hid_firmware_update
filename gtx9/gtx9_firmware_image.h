/*
 * @Author: your name
 * @Date: 2021-01-27 10:07:27
 * @LastEditTime: 2021-05-11 11:05:02
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: \gdix_hid_firmware_update-master\gtx9\gtx9_firmware_image.h
 */
/*
 * @Author: your name
 * @Date: 2021-01-27 10:07:27
 * @LastEditTime: 2021-05-11 10:34:58
 * @LastEditors: your name
 * @Description: In User Settings Edit
 * @FilePath: \gdix_hid_firmware_update-master\gtx9\gtx9_firmware_image.h
 */
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

#ifndef GTX9_FIRMWARE_IMAGE_H
#define GTX9_FIRMWARE_IMAGE_H

#include "../firmware_image.h"

#define FW_HEADER_SIZE					512
#define FW_SUBSYS_INFO_SIZE				10
#define FW_SUBSYS_INFO_OFFSET			42
#define FW_SUBSYS_MAX_NUM				47

struct fw_subsys_info {
	unsigned char type;
	unsigned int size;
	unsigned int flash_addr;
	const unsigned char *data;
};

struct config_info {
	unsigned char data[2048];
	unsigned int size;
};

#pragma pack(1)
struct  firmware_summary {
	unsigned int size;
	unsigned int checksum;
	unsigned char hw_pid[6];
	unsigned char hw_vid[3];
	unsigned char fw_pid[8];
	unsigned char fw_vid[4];
	unsigned char subsys_num;
	unsigned char chip_type;
	unsigned char protocol_ver;
	unsigned char bus_type;
	unsigned char flash_protect;
	unsigned char reserved[8];
	struct fw_subsys_info subsys[FW_SUBSYS_MAX_NUM];
};
#pragma pack()

class GTX9FirmwareImage : public FirmwareImage
{
public:
    GTX9FirmwareImage();

    int Initialize(const char * filename);
    void Close();
    void *GetFirmwareSummary() {return &m_firmwareSummary;}
    unsigned char *GetProductID() {return m_firmwarePID;}
    unsigned char *GetVendorID() {return m_firmwareVID;}
    unsigned int GetConfigID() {return m_firmwareCfgID;}


protected:
    int GetDataFromFile(const char* filename);

private:
    int ParseFirmware();

    unsigned char m_firmwarePID[8];
    unsigned char m_firmwareVID[4];
    unsigned int m_firmwareCfgID;
    struct firmware_summary m_firmwareSummary;
};

#endif