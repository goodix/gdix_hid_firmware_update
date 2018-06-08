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

#ifndef _FIRMWARE_IMAGE_H_
#define _FIRMWARE_IMAGE_H_

#define SUB_FW_INFO_OFFSET 32
#define SUB_FW_DATA_OFFSET 128 //x8=256

#define FW_IMAGE_PID_OFFSET 15 
#define FW_IMAGE_VID_OFFSET 21 //x8=23

#define FW_IMAGE_SUB_FWNUM_OFFSET 24 //x8=26

class FirmwareImage
{
public:
	FirmwareImage();

	int Initialize(const char * filename);

	unsigned int GetFirmwareSize() { return m_firmwareSize; }
	unsigned char *GetProductID() { return m_pid; }
	int GetFirmwareVersionMajor() { return m_firmwareVersionMajor; }
	int GetFirmwareVersionMinor() { return m_firmwareVersionMinor; }
	unsigned char *GetFirmwareData() {
		if (m_initialized)
			return m_firmwareData;
		else
			return NULL;
	}
	void Close();
	~FirmwareImage() { Close(); }
private:
	bool m_initialized;
	int m_firmwareSize;
	unsigned char m_pid[8];
	int m_firmwareVersionMajor; 
	int m_firmwareVersionMinor; 
	unsigned char *m_firmwareData;
};
#endif


