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
#ifndef _GT7868Q_H_
#define _GT7868Q_H_

#include "../gtx3/gtx3.h"
#include <memory.h>
#include <string>

#define CMD_ADDR 0x4160

class GT7868QDevice : public GTx3Device
{
public:
	GT7868QDevice();
	virtual ~GT7868QDevice();

	int SetBasicProperties();

private:
	unsigned short ChecksumU8_ys(unsigned char *data, int len);
};
#endif
