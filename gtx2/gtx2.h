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

#ifndef _GTX2_H_
#define _GTX2_H_

#include <string>
#include "../gtx5/gtx5.h"

/*
#define GTP_PRODUCT_ID_LENGTH		6
#define HID_ARM				0
#define HID_MACHINE			1

#define _STOP_READ_ALL			0
#define _STOP_READ                      0
#define _READ_COORD                     1
#define _READ_RAWDATA			2
#define _READ_RAWDATA_LAST		3
#define _READ_BASE                      4
#define _READ_DIFF                      6
#define _READ_TEST_VAL			7
#define _READ_VERSION			8
#define _SEND_CFG                       9
#define _READ_CFG                       0x0a
#define _I2C_DIRECT_RW			0x20
#define _I2C_INDIRECT_READ		0x21

#define HID_GDIX_REPORT_ID		0
#define GDIX_ATTN_REPORT_ID		0x0c
#define GDIX_READ_DATA_REPORT_ID	0x0b

#define GDIX_PRE_HEAD_LEN		5
#define GDIX_DATA_HEAD_LEN		5
#define GDIX_RETRY_TIMES		6


*/

#define GTX2_VERSION_ADDR		0x8140

class GTx2Device : public GTx5Device
{
public:
    int QueryBasicProperties(){ return 0; };
    int SetBasicProperties();
    virtual ~GTx2Device(){}
};
#endif
