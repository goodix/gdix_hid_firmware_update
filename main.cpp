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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <linux/hidraw.h>
#include <regex.h>
#include <sstream>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "firmware_image.h"
#include "gt7868q/gt7868q.h"
#include "gt7868q/gt7868q_firmware_image.h"
#include "gt7868q/gt7868q_update.h"
#include "gt_update.h"
#include "gtmodel.h"
#include "gtp_util.h"
#include "gtx2/gtx2.h"
#include "gtx2/gtx2_firmware_image.h"
#include "gtx2/gtx2_update.h"
#include "gtx3/gtx3.h"
#include "gtx3/gtx3_firmware_image.h"
#include "gtx3/gtx3_update.h"
#include "gtx5/gtx5.h"
#include "gtx5/gtx5_firmware_image.h"
#include "gtx5/gtx5_update.h"
#include "gtx8/gtx8.h"
#include "gtx8/gtx8_firmware_image.h"
#include "gtx8/gtx8_update.h"
#include "gtx9/gtx9.h"
#include "gtx9/gtx9_firmware_image.h"
#include "gtx9/gtx9_update.h"
#include "berlin_a/brla.h"
#include "berlin_a/brla_firmware_image.h"
#include "berlin_a/brla_update.h"

#define GTPUPDATE_GETOPTS "hfd:pvt:s:ima:"

#define VERSION "1.7.9"

enum IC_TYPE {
	TYPE_PHOENIX,
	TYPE_NANJING,
	TYPE_MOUSEPAD,
	TYPE_NORMANDYL,
	TYPE_YELLOWSTONE,
	TYPE_BERLINA,
	TYPE_BERLINB,
};

bool pdebug = false;

extern int gdix_do_fw_update(const char *devname, const char *filename,
							 uint8_t i2c_addr);

static void printHelp(const char *prog_name)
{
	fprintf(stdout, "Usage: %s [OPTIONS] FIRMWAREFILE\n", prog_name);
	fprintf(stdout, "\t-h, --help\tPrint this message\n");
	fprintf(stdout,
			"\t-f, --force\tForce updating firmware with check PID and VID\n");
	fprintf(stdout,
			"\t-d, --device\thidraw device file associated with the device "
			"being updated.\n");
	fprintf(stdout,
			"\t-p, --fw-props\tPrint the firmware properties, format like PID "
			"7589:VID 1.1.\n");
	fprintf(stdout, "\t-v, --version\tPrint version number.\n");
	fprintf(stdout, "\t-t, --type\t device pid number.\n");
	fprintf(stdout,
			"\t-s, --series\t device series type number like 8589 or 7288.\n");
	fprintf(stdout,
			"\t-i, --info\t print detail info while the tool is running.\n");
	fprintf(stdout, "\t-m, --module\t print module/sensor ID.\n");
	fprintf(stdout,
			"\t-a, --i2c-addr\t if this option is set, will be upgraded by "
			"i2c.(only support berlinB)\n");
}

static void printVersion()
{
	fprintf(stdout, "goodixupdate version %s\n", VERSION);
}

static void lowercase_str(unsigned char *str)
{
	unsigned char *ptr = str;
	while (*ptr) {
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
	FirmwareImage *fw_image = NULL;
	GTupdate *gt_update = NULL;
	GTUpdatePara *gt_update_para = NULL;
	unsigned int firmware_flag = 0xFFFFFFFF;
	uint8_t i2cAddr = 0;

	regex_t reg_x3xx;
	regex_t reg_x5xx;
	regex_t reg_x2xx;
	regex_t reg_x8xx;
	regex_t reg_x9xx;
	regex_t reg_7868;
	regex_t reg_brla;
	regmatch_t pamtch[1]; // match container

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
		{"series", 1, NULL, 's'},
		{"info", 0, NULL, 'i'},
		{"module", 0, NULL, 'm'},
		{"i2c-addr", 1, NULL, 'a'},
		{0, 0, 0, 0},
	};
	bool printFirmwareProps = false;
	bool printModuleId = false;
	while ((opt = getopt_long(argc, argv, GTPUPDATE_GETOPTS, long_options,
							  &index)) != -1) {
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
			lowercase_str((unsigned char *)optarg);
			pid = optarg;
			gdix_dbg("pid is %s\n", pid);
			break;
		case 's':
			productionTypeName = optarg;
			gdix_dbg("product type is %s\n", productionTypeName);
			break;
		case 'i':
			pdebug = true;
			break;
		case 'm':
			printModuleId = true;
			break;
		case 'a':
			i2cAddr = strtol(optarg, NULL, 16);
			break;
		default:
			break;
		}
	}

	if (optind < argc) {
		firmwareName = argv[optind];
		gdix_dbg("firmware name:%s\n", firmwareName);
	}
	if (NULL == productionTypeName && NULL == pid) {
		gdix_err("please input pid or product type\n");
		return -1;
	}
	if (!printModuleId && !printFirmwareProps && NULL == firmwareName) {
		gdix_err("file name not found\n");
		return -1;
	}

	// check chip type
	if (pid != NULL) {
		regex_t reg_pid;
		regcomp(&reg_pid, "^011[0-9A-Fa-f]$", REG_EXTENDED); // phoenix pid
															 // 011x
		if (REG_NOERROR == regexec(&reg_pid, pid, 1, pamtch, 0)) {
			gdix_dbg("pid match phoenix pid 011x\n");
			chipType = TYPE_PHOENIX;
		}
		regfree(&reg_pid);

		regcomp(&reg_pid, "^0[eE][0-9A-Fa-f]{2}$",
				REG_EXTENDED); // phoenix pid 0exx
		if (REG_NOERROR == regexec(&reg_pid, pid, 1, pamtch, 0)) {
			gdix_dbg("pid match phoenix pid 0exx\n");
			chipType = TYPE_PHOENIX;
		}
		regfree(&reg_pid);

		regcomp(&reg_pid, "^01[fF][0-9A-Fa-f]$",
				REG_EXTENDED); // mouse pad pid 01fx
		if (REG_NOERROR == regexec(&reg_pid, pid, 1, pamtch, 0)) {
			gdix_dbg("pid match mousepad pid 01fx\n");
			chipType = TYPE_MOUSEPAD;
		}
		regfree(&reg_pid);

		regcomp(&reg_pid, "^0[fF][0-9A-Fa-f]{2}$",
				REG_EXTENDED); // mouse pad pid 0fxx
		if (REG_NOERROR == regexec(&reg_pid, pid, 1, pamtch, 0)) {
			gdix_dbg("pid match mousepad pid 0fxx\n");
			chipType = TYPE_MOUSEPAD;
		}
		regfree(&reg_pid);

		regcomp(&reg_pid, "^01[eE][0-7]$",
				REG_EXTENDED); // 7863 windows pid 01e0~01e7
		if (REG_NOERROR == regexec(&reg_pid, pid, 1, pamtch, 0)) {
			gdix_dbg("pid match GT7863 pid 01e0_01e7\n");
			chipType = TYPE_NORMANDYL;
		}
		regfree(&reg_pid);

		regcomp(&reg_pid, "^0[dD][0-7][0-9A-Fa-f]$",
				REG_EXTENDED); // 7863 chrome pid 0d00~0d7f
		if (REG_NOERROR == regexec(&reg_pid, pid, 1, pamtch, 0)) {
			gdix_dbg("pid match GT7863 pid 0d00_0d7f\n");
			chipType = TYPE_NORMANDYL;
		}
		regfree(&reg_pid);

		regcomp(&reg_pid, "^0[dD][8-9A-Ba-b][0-9A-Fa-f]$",
				REG_EXTENDED); // 7868Q chrome pid 0d80~0dbf
		if (REG_NOERROR == regexec(&reg_pid, pid, 1, pamtch, 0)) {
			gdix_dbg("pid match GT7868Q pid 0d80_0dbf\n");
			chipType = TYPE_YELLOWSTONE;
		}
		regfree(&reg_pid);

		/* 0EB* 0EC* is BerlinB */
		regcomp(&reg_pid, "^0[eE][bBcC][0-9A-Fa-f]$", REG_EXTENDED);
		if (REG_NOERROR == regexec(&reg_pid, pid, 1, pamtch, 0)) {
			gdix_dbg("pid match BerlinB pid 0EBx\n");
			chipType = TYPE_BERLINB;
		}
		regfree(&reg_pid);

		/* 0EA5~0EAF is BerlinB */
		regcomp(&reg_pid, "^0[eE][aA][5-9A-Fa-f]$", REG_EXTENDED);
		if (REG_NOERROR == regexec(&reg_pid, pid, 1, pamtch, 0)) {
			gdix_dbg("pid match BerlinB pid 0EA5~0EAF\n");
			chipType = TYPE_BERLINB;
		}
		regfree(&reg_pid);

		/* 0Cxx is BerlinB */
		regcomp(&reg_pid, "^0[cC][0-9A-Fa-f]{2}$", REG_EXTENDED);
		if (REG_NOERROR == regexec(&reg_pid, pid, 1, pamtch, 0)) {
			gdix_dbg("pid match BerlinB pid 0Cxx\n");
			chipType = TYPE_BERLINB;
		}
		regfree(&reg_pid);

		/* 0F60~0F7F is BerlinA */
		regcomp(&reg_pid, "^0[fF][6-7][0-9A-Fa-f]$", REG_EXTENDED);
		if (REG_NOERROR == regexec(&reg_pid, pid, 1, pamtch, 0)) {
			gdix_dbg("pid match BerlinA pid 0F60~0F7F\n");
			chipType = TYPE_BERLINA;
		}
		regfree(&reg_pid);		
	} else if (productionTypeName != NULL) {
		regcomp(&reg_x3xx, "^[0-9]3[0-9]{2}", REG_EXTENDED);
		regcomp(&reg_x5xx, "^[0-9]5[0-9]{2}", REG_EXTENDED);
		regcomp(&reg_x2xx, "^[0-9]2[0-9]{2}", REG_EXTENDED);
		regcomp(&reg_x8xx, "^[0-9]8[0-9]{2}", REG_EXTENDED);
		regcomp(&reg_x9xx, "^[0-9]9[0-9]{2}", REG_EXTENDED);
		regcomp(&reg_7868, "^7868", REG_EXTENDED);
		regcomp(&reg_brla, "^7726", REG_EXTENDED);

		if (REG_NOERROR == regexec(&reg_x3xx, productionTypeName, 1, pamtch, 0))
			chipType = TYPE_PHOENIX; // 7388 match
		else if (REG_NOERROR ==
				 regexec(&reg_x5xx, productionTypeName, 1, pamtch, 0))
			chipType = TYPE_NANJING; // 8589 match
		else if (REG_NOERROR ==
				 regexec(&reg_x2xx, productionTypeName, 1, pamtch, 0))
			chipType = TYPE_MOUSEPAD; // 7288 match
		else if (REG_NOERROR ==
				 regexec(&reg_x8xx, productionTypeName, 1, pamtch, 0))
			chipType = TYPE_NORMANDYL; // 7863 match
		else if (REG_NOERROR ==
				 regexec(&reg_x9xx, productionTypeName, 1, pamtch, 0))
			chipType = TYPE_BERLINB; // 9966 match
		else if (REG_NOERROR ==
				 regexec(&reg_7868, productionTypeName, 1, pamtch, 0))
			chipType = TYPE_YELLOWSTONE; // 7868 match
		else if (REG_NOERROR ==
				 regexec(&reg_brla, productionTypeName, 1, pamtch, 0))
			chipType = TYPE_BERLINA; // 7726 match
		else
			chipType = -1; // no match

		regfree(&reg_x3xx);
		regfree(&reg_x5xx);
		regfree(&reg_x2xx);
		regfree(&reg_x8xx);
		regfree(&reg_x9xx);
		regfree(&reg_7868);
		regfree(&reg_brla);
	}

	if (chipType < 0) {
		gdix_err("Find No match pid or product\n");
		delete gt_model;
		return -6;
	}

	/* i2c update */
	if (i2cAddr > 0 && chipType == TYPE_BERLINB) {
		gdix_do_fw_update(deviceName, firmwareName, i2cAddr);
		return 0;
	}

	if (chipType == TYPE_MOUSEPAD) {
		gt_model = new GTx2Device;
		fw_image = new GTX2FirmwareImage;
		gt_update = new GTx2Update;
		firmware_flag = 0x1400C; // update type:0x02,0x03,0x0e,0x10
	} else if (chipType == TYPE_NANJING) {
		gt_model = new GTx5Device;
		fw_image = new GTX5FirmwareImage;
		gt_update = new GTx5Update;
		firmware_flag = 0x1400C; // update type:0x02,0x03,0x03,0x10;
	} else if (chipType == TYPE_PHOENIX) {
		gt_model = new GTx3Device;
		fw_image = new GTX3FirmwareImage;
		gt_update = new GTx3Update;
		firmware_flag = 0x844; // update type:0x02,0x03,0x03,0x10;
	} else if (chipType == TYPE_NORMANDYL) {
		gt_model = new GTx8Device;
		fw_image = new GTX8FirmwareImage;
		gt_update = new GTx8Update;
		firmware_flag = 0x0C; // update type:0x02,0x03;
	} else if (chipType == TYPE_BERLINB) {
		gt_model = new GTx9Device;
		fw_image = new GTX9FirmwareImage;
		gt_update = new GTx9Update;
		firmware_flag = 0x0B; // don't update type:0x0B;
	} else if (chipType == TYPE_YELLOWSTONE) {
		gt_model = new GT7868QDevice;
		fw_image = new GT7868QFirmwareImage;
		gt_update = new GT7868QUpdate;
		firmware_flag = 0x0C; // update type:0x02,0x03;
	} else if (chipType == TYPE_BERLINA) {
		gt_model = new BrlADevice;
		fw_image = new BrlAFirmwareImage;
		gt_update = new BrlAUpdate;
	} else {
		if (pid != NULL)
			gdix_err("unsupported pid number:%s\n", pid);
		else
			gdix_err("unsupported product type number:%s\n",
					 productionTypeName);
		return -1;
	}

	/* get and print active FW version */
	if (printFirmwareProps) {
		char props_buf[60] = {0};
		ret = gt_model->GetFirmwareProps(deviceName, props_buf,
										 sizeof(props_buf));
		if (ret) {
			printf("Failed to read properties from device %s\n", deviceName);
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

	ret = gt_update->Initialize(gt_model, fw_image);
	if (ret) {
		// gdix_err("Failed read firmware file:%s\n", firmwareName);
		delete gt_model;
		return -2;
	}

	// run update
	gt_update_para = new GTUpdatePara;
	gt_update_para->force = force;
	gt_update_para->firmwareFlag = firmware_flag;

	ret = gt_update->Run(gt_update_para);
	if (ret) {
		gdix_err("Firmware update err:ret=%d\n", ret);
		delete gt_model;
		return -4;
	}

	delete gt_model;
	delete fw_image;
	delete gt_update;

	return 0;
}
