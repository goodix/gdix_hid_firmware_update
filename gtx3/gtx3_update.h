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

#ifndef _GTX3_UPDATE_H
#define _GTX3_UPDATE_H

#include "../gtx2/gtx2_update.h"
#include "../gtmodel.h"
#include "../firmware_image.h"


class GTx3Update : public GTx2Update
{
public:
    GTx3Update();
    virtual ~GTx3Update();
    virtual int Run(void* para);

protected:
    virtual int fw_update(unsigned int firmware_flag);
    virtual int cfg_update();
    virtual int flash_cfg_with_isp();
private:
    bool is_cfg_flashed_with_isp;
};


#endif