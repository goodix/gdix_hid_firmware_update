/*
 * @Author: your name
 * @Date: 2021-01-26 10:57:40
 * @LastEditTime: 2021-05-11 12:00:37
 * @LastEditors: Please set LastEditors
 * @Description: In User Settings Edit
 * @FilePath: \gdix_hid_firmware_update-master\gtx9\gtx9.h
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
#ifndef _GTX9_H_
#define _GTX9_H_

#include "../gtmodel.h"
#include <memory.h>
#include <string>

#define REPORT_ID 0x0E
#define I2C_DIRECT_RW 0x20
#define I2C_READ_FLAG 1
#define I2C_WRITE_FLAG 0

#define PACKAGE_LEN 65 /* custom data len + report_id */

class GTx9Device : public GTmodel
{
public:
	GTx9Device();

	int Open(const char *filename);
	bool IsOpened();
	void Close();

	int Read(unsigned int addr, unsigned char *buf, unsigned int len);
	int Write(unsigned int addr, const unsigned char *buf, unsigned int len);
	int WriteSpeCmd(const unsigned char *buf, unsigned int len);
	int SendCmd(unsigned char cmd, unsigned char *data, int dataLen);
	int SendConfig(unsigned char *config, int len);
	int SetBasicProperties();
	int GetFirmwareProps(const char *deviceName, char *props_buf, int len);
	int GetFd();
	unsigned char *GetProductID() { return m_pid; }
	unsigned char *GetVendorID() { return m_vid; }
	unsigned int GetConfigID() { return m_configID; }
	unsigned char GetSensorID() { return m_sensorID; }
	int GetFirmwareVersionMajor() { return m_firmwareVersionMajor; }
	int GetFirmwareVersionMinor() { return m_firmwareVersionMinor; }

private:
	int m_fd;
	unsigned char m_pid[8];
	unsigned char m_vid[4];
	unsigned char m_sensorID;
	unsigned int m_configID;
	int m_firmwareVersionMajor;
	int m_firmwareVersionMinor;

	int ReadPkg(unsigned int addr, unsigned char *buf, unsigned int len);
	int GetReport(unsigned char *buf);
	int SetReport(unsigned char *buf, int len);
};

#endif