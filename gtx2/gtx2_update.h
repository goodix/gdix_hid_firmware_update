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

#ifndef _GTX2_UPDATE_H
#define _GTX2_UPDATE_H

#include "../gt_update.h"
#include "../gtmodel.h"
#include "../firmware_image.h"

#define RAM_BUFFER_SIZE	    4096
#define BL_STATE_ADDR		0x5095  //X8=5095
#define FLASH_RESULT_ADDR	0x5096  //x8=0x5096

class GTx2Update : public GTupdate
{
public:
    GTx2Update();
    virtual int Run(void* para);
    virtual ~GTx2Update();

protected:
    virtual int load_sub_firmware(unsigned int flash_addr,
			unsigned char *fw_data, unsigned int len); 
    virtual int fw_update(unsigned int firmware_flag);

};


#endif