/*
 * Copyright (C) 2018 Goodix Inc
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

#ifndef _GT7868Q_UPDATE_H
#define _GT7868Q_UPDATE_H

#include "../firmware_image.h"
#include "../gtmodel.h"
#include "../gtx3/gtx3_update.h"

class GT7868QUpdate : public GTx3Update
{
public:
	GT7868QUpdate();
	virtual ~GT7868QUpdate();
	virtual int Run(void *para);

protected:
	virtual int fw_update(unsigned int firmware_flag);
	virtual int cfg_update();
	virtual int flash_cfg_with_isp();

private:
	bool is_cfg_flashed_with_isp;
	void cmd_init(unsigned char *cmd_buf, unsigned char cmd,
				  unsigned short cmd_data);
};

#endif
