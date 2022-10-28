/*
 * @Author: your name
 * @Date: 2021-01-26 18:32:09
 * @LastEditTime: 2021-05-11 10:51:11
 * @LastEditors: your name
 * @Description: In User Settings Edit
 * @FilePath: \gdix_hid_firmware_update-master\gtx9\gtx9_update.h
 */
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

#ifndef _GTX9_UPDATE_H
#define _GTX9_UPDATE_H

#include "../firmware_image.h"
#include "../gt_update.h"
#include "../gtmodel.h"
#include "gtx9_firmware_image.h"

class GTx9Update : public GTupdate
{
public:
	int Run(void *para);

protected:
	int check_update();
	int fw_update(unsigned int firmware_flag);

private:
	int prepareUpdate();
	int flashSubSystem(struct fw_subsys_info *subsys);
};

#endif