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

/*
#define SUB_FW_INFO_OFFSET 32
#define SUB_FW_DATA_OFFSET 128 //x8=256

#define FW_IMAGE_PID_OFFSET 15 
#define FW_IMAGE_VID_OFFSET 21 //x8=23

#define FW_IMAGE_SUB_FWNUM_OFFSET 24 //x8=26
*/
#include <cstddef>

//update type
enum updateFlag {none = 0,firmware=1,config=2};

class FirmwareImage
{
public:
	FirmwareImage();

	virtual int Initialize(const char * filename);

	virtual unsigned int GetFirmwareSize() { return m_firmwareSize; }
	virtual unsigned char *GetProductID() { return m_pid; }
	virtual int GetFirmwareVersionMajor() { return m_firmwareVersionMajor; }
	virtual int GetFirmwareVersionMinor() { return m_firmwareVersionMinor; }
	virtual unsigned char *GetFirmwareData() {
		if (m_initialized)
			return m_firmwareData;
		else
			return NULL;
	}
	virtual void Close();
	virtual ~FirmwareImage() { Close(); }
	virtual int GetFirmwareSubFwNum(){return 0;}
	virtual int GetFirmwareSubFwInfoOffset(){return 0;}
	virtual int GetFirmwareSubFwDataOffset(){return 0;}
	virtual int GetConfigSubCfgNum();
	virtual int GetConfigSubCfgInfoOffset();
	virtual int GetConfigSubCfgDataOffset();
	virtual bool IsOpened(){return m_initialized;}
	virtual bool HasConfig(){return hasConfig;}
	virtual updateFlag GetUpdateFlag();
protected:
	virtual int GetDataFromFile(const char* filename);
	virtual int InitPid();
	virtual int InitVid();

protected:
	bool m_initialized;
	int m_totalSize;
	int m_firmwareSize;
	int m_configSize;
	bool hasConfig;
	unsigned char m_pid[8];
	int m_firmwareVersionMajor; 
	int m_firmwareVersionMinor; 
	unsigned char *m_firmwareData;
};


#endif


