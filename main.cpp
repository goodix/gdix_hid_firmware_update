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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>
#include <string>
#include <sstream>
#include <linux/hidraw.h>
#include <regex.h>

#include "gtp_util.h"
#include "gtmodel.h"
#include "gtx2/gtx2.h"
#include "gtx3/gtx3.h"
#include "gtx5/gtx5.h"
#include "firmware_image.h"
#include "gtx2/gtx2_firmware_image.h"
#include "gtx3/gtx3_firmware_image.h"
#include "gtx5/gtx5_firmware_image.h"
#include "gt_update.h"
#include "gtx2/gtx2_update.h"
#include "gtx3/gtx3_update.h"
#include "gtx5/gtx5_update.h"

#define RAM_BUFFER_SIZE	    4096

#define GTPUPDATE_GETOPTS	"hfd:pvt:s:im"

#define VERSION_MAJOR		1
#define VERSION_MINOR		7
#define VERSION_SUBMINOR	0

#define TYPE_PHOENIX 0
#define TYPE_NANJING 1
#define TYPE_MOUSEPAD 2

bool pdebug = false;

void printHelp(const char *prog_name)
{
	fprintf(stdout, "Usage: %s [OPTIONS] FIRMWAREFILE\n", prog_name);
	fprintf(stdout, "\t-h, --help\tPrint this message\n");
	fprintf(stdout, "\t-f, --force\tForce updating firmware with check PID and VID\n");
	fprintf(stdout, "\t-d, --device\thidraw device file associated with the device being updated.\n");
	fprintf(stdout, "\t-p, --fw-props\tPrint the firmware properties, format like PID 7589:VID 1.1.\n");
	fprintf(stdout, "\t-v, --version\tPrint version number.\n");
	fprintf(stdout, "\t-t, --type\t device pid number.\n");
	fprintf(stdout, "\t-s, --series\t device series type number like 8589 or 7288.\n");
	fprintf(stdout, "\t-i, --info\t print detail info while the tool is running.\n");
	fprintf(stdout, "\t-m, --module\t print module/sensor ID.\n");
}

void printVersion()
{
	fprintf(stdout, "goodixupdate version %d.%d.%d\n",
		VERSION_MAJOR, VERSION_MINOR, VERSION_SUBMINOR);
}

void lowercase_str(unsigned char* str)
{
	unsigned char* ptr = str;
	while(*ptr){
		*ptr = tolower(*ptr);
		ptr++;
	}
}

int main(int argc, char **argv)
{
	int ret = 0;
	int opt;
	int index;
	int chipType = -1;
	GTmodel *gt_model = NULL;
	FirmwareImage* fw_image = NULL;
	GTupdate* gt_update = NULL;
	GTUpdatePara* gt_update_para = NULL;
	unsigned int firmware_flag = 0;

	regex_t reg_x3xx;
	regex_t reg_x5xx;
	regex_t reg_x2xx;
	regmatch_t pamtch[1];//match container

    char *deviceName = NULL;
	const char *firmwareName = NULL;
	const char *pid = NULL;
	const char *productionTypeName = NULL;
	bool force = false;
	static struct option long_options[] = {
		{"help", 0, NULL, 'h'},
		{"force", 0, NULL, 'f'},
		{"device", 1, NULL, 'd'},
		{"fw-props", 0, NULL, 'p'},
		{"version", 0, NULL, 'v'},
		{"type", 1, NULL, 't'},
		{"series",1,NULL,'s'},
		{"info",0,NULL,'i'},
		{"module",0,NULL,'m'},
		{0, 0, 0, 0},
	};
	bool printFirmwareProps = false;
	bool printModuleId = false;
	while ((opt = getopt_long(argc, argv, GTPUPDATE_GETOPTS, long_options, &index)) != -1) {
		switch (opt) {
			case 'h':
				printHelp(argv[0]);
				return 0;
			case 'f':
				force = true;
				break;
			case 'd':
				deviceName = optarg;
				break;
			case 'p':
				printFirmwareProps = true;
				break;
			case 'v':
				printVersion();
				return 0;
			case 't':
				lowercase_str((unsigned char*)optarg);
				pid = optarg;
				gdix_dbg("pid is %s\n",pid);
				break;
			case 's':
				productionTypeName = optarg;
				gdix_dbg("product type is %s\n",productionTypeName);
				break;
			case 'i':
				pdebug = true;
				break;
			case 'm':
				printModuleId = true;
				break;
			default:
				break;

		}
	}

	if (optind < argc) {
		firmwareName = argv[optind];
		gdix_dbg("firmware name:%s\n",firmwareName);
	}
	if(NULL == productionTypeName && NULL == pid)
	{
		gdix_err("please input pid or product type\n");
		return -1;
	}
	if(!printModuleId && !printFirmwareProps && NULL == firmwareName)
	{
		gdix_err("file name not found\n");
		return -1;
	}

	//check chip type
	if(pid != NULL)
	{
		/*if(!strcmp(pid,"0111") || 
			!strcmp(pid,"0112") ||
			!strcmp(pid,"0113") || 
			!strcmp(pid,"0114") || 
			!strcmp(pid,"0118"))
				chipType = TYPE_PHOENIX;//7388 match
		else if(!strcmp(pid,"01f0"))
			chipType = TYPE_MOUSEPAD;*/

		regex_t reg_pid;
		regcomp(&reg_pid,"^011[0-9A-Fa-f]$",REG_EXTENDED);//phoenix pid 011x
		if(REG_NOERROR == regexec(&reg_pid,pid,1,pamtch,0))
		{
			gdix_dbg("pid match phoenix pid 011x\n");
			chipType = TYPE_PHOENIX;
		}
		regfree(&reg_pid);

		regcomp(&reg_pid,"^0[eE][0-9A-Fa-f]{2}$",REG_EXTENDED);//phoenix pid 0exx
		if(REG_NOERROR == regexec(&reg_pid,pid,1,pamtch,0))
		{
			gdix_dbg("pid match phoenix pid 0exx\n");
			chipType = TYPE_PHOENIX;
		}
		regfree(&reg_pid);

		regcomp(&reg_pid,"^01[fF][0-9A-Fa-f]$",REG_EXTENDED);//mouse pad pid 01fx
		if(REG_NOERROR == regexec(&reg_pid,pid,1,pamtch,0))
		{
			gdix_dbg("pid match mousepad pid 01fx\n");
			chipType = TYPE_MOUSEPAD;
		}
		regfree(&reg_pid);

		regcomp(&reg_pid,"^0[fF][0-9A-Fa-f]{2}$",REG_EXTENDED);//mouse pad pid 0fxx
		if(REG_NOERROR == regexec(&reg_pid,pid,1,pamtch,0))
		{
			gdix_dbg("pid match mousepad pid 0fxx\n");
			chipType = TYPE_MOUSEPAD;
		}
		regfree(&reg_pid);
		
		
	}else if(productionTypeName != NULL)
	{
		regcomp(&reg_x3xx,"^[0-9]3[0-9]{2}",REG_EXTENDED);
		regcomp(&reg_x5xx,"^[0-9]5[0-9]{2}",REG_EXTENDED);
		regcomp(&reg_x2xx,"^[0-9]2[0-9]{2}",REG_EXTENDED);

		if(REG_NOERROR == regexec(&reg_x3xx,productionTypeName,1,pamtch,0))
			chipType = TYPE_PHOENIX;//7388 match
		else if(REG_NOERROR == regexec(&reg_x5xx,productionTypeName,1,pamtch,0))
			chipType = TYPE_NANJING;//8589 match
		else if(REG_NOERROR == regexec(&reg_x2xx,productionTypeName,1,pamtch,0))
			chipType = TYPE_MOUSEPAD;//7288 match
		else
			chipType = -1;//no match

		regfree(&reg_x3xx);
		regfree(&reg_x5xx);
		regfree(&reg_x2xx);
	}

	if(chipType < 0)
	{
		gdix_err("Find No match pid or product\n");
		delete gt_model;
		return -6;
	}

	if(chipType == TYPE_MOUSEPAD)
	{
		gt_model = new GTx2Device;
		fw_image = new GTX2FirmwareImage;
		gt_update = new GTx2Update;
		firmware_flag = 0x1400C;//update type:0x02,0x03,0x0e,0x10
	}
	else if(chipType == TYPE_NANJING)
	{
		gt_model = new GTx5Device;
		fw_image = new GTX5FirmwareImage;
		gt_update = new GTx5Update;
		firmware_flag = 0x1400C;//update type:0x02,0x03,0x03,0x10;
	}
	else if(chipType == TYPE_PHOENIX)
	{
		gt_model = new GTx3Device;
		fw_image = new GTX3FirmwareImage;
		gt_update = new GTx3Update;
		firmware_flag = 0x844;//update type:0x02,0x03,0x03,0x10;
	}
	else
	{
		if(pid != NULL){
			gdix_err("unsupported pid number:%s\n",pid);
		}
		else{
			gdix_err("unsupported product type number:%s\n",productionTypeName);
		}
		return -1;
	}

	/* get and print active FW version */
	if (printFirmwareProps) {
		char props_buf[60] = {0};
		ret = gt_model->GetFirmwareProps(deviceName, props_buf, sizeof(props_buf));
		if (ret) {
			printf("Failed to read properties from device %s\n",
				 deviceName);
			delete gt_model;
            return 1;
		} else {
			printf("%s\n", props_buf);
			delete gt_model;
            return 0;
		}
	}

	ret = gt_model->Open(deviceName);
	if (ret) {
		gdix_err("failed open device:%s\n", deviceName);
		delete gt_model;
        return -1;
	}

	if (printModuleId) {
		printf("module_id:%d\n", gt_model->GetSensorID());
		delete gt_model;
		return 0;
	}

	ret = fw_image->Initialize(firmwareName);
	if (ret) {
		gdix_err("Failed read firmware file:%s\n", firmwareName);
		delete gt_model;
        return -2;
	}

	ret = gt_update->Initialize(gt_model,fw_image);
	if(ret){
		//gdix_err("Failed read firmware file:%s\n", firmwareName);
		delete gt_model;
    	return -2;
	}

	//run update
	gt_update_para = new GTUpdatePara;
	gt_update_para->force = force;
	gt_update_para->firmwareFlag = firmware_flag;
	
	ret = gt_update->Run(gt_update_para);
	if(ret)
	{
		gdix_err("Firmware update err:ret=%d\n", ret);
		delete gt_model;
    	return -4;
	}

	delete gt_model;
    return 0;
}
