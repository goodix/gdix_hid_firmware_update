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

#ifndef GTX3_FIRMWARE_IMAGE_H
#define GTX3_FIRMWARE_IMAGE_H

#include "../gtx2/gtx2_firmware_image.h"

class GTX3FirmwareImage : public GTX2FirmwareImage
{
public:
	GTX3FirmwareImage();
	~GTX3FirmwareImage();
	virtual int GetFirmwareSubFwNum();
	virtual int GetFirmwareSubFwDataOffset();

protected:
	virtual int InitPid();
	virtual int InitVid();
};

#endif