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

#include "gtp_util.h"
#include "gtx5.h"
#include "firmware_image.h"

#define RAM_BUFFER_SIZE	    4096
#define GOODIX_FLASH_BUFFER 0xDE24

#define GTPUPDATE_GETOPTS	"hfd:pv"

#define VERSION_MAJOR		1
#define VERSION_MINOR		4
#define VERSION_SUBMINOR	4

void printHelp(const char *prog_name)
{
	fprintf(stdout, "Usage: %s [OPTIONS] FIRMWAREFILE\n", prog_name);
	fprintf(stdout, "\t-h, --help\tPrint this message\n");
	fprintf(stdout, "\t-f, --force\tForce updating firmware with check PID and VID\n");
	fprintf(stdout, "\t-d, --device\thidraw device file associated with the device being updated.\n");
	fprintf(stdout, "\t-p, --fw-props\tPrint the firmware properties, format like PID 7589:VID 1.1.\n");
	fprintf(stdout, "\t-v, --version\tPrint version number.\n");
}

void printVersion()
{
	fprintf(stdout, "goodixupdate version %d.%d.%d\n",
		VERSION_MAJOR, VERSION_MINOR, VERSION_SUBMINOR);
}

int GetFirmwareProps(const char *deviceName, char *props_buf, int len)
{
	int ret;
	GTx5Device gtx5_dev;

	ret = gtx5_dev.Open(deviceName);
	if (ret) {
		gdix_dbg("failed open device:%s\n", deviceName);
		return -1;
	}

	snprintf(props_buf, len, "%d.%d",
		 gtx5_dev.GetFirmwareVersionMajor(),
		 gtx5_dev.GetFirmwareVersionMinor());
	return 0;
}

int check_update(GTx5Device &dev, FirmwareImage &fw_image)
{
	int ret;
	int active_vid;
	int firmware_vid;

	gdix_dbg("dev PID is %s, image PID is %s\n",
		 dev.GetProductID(), fw_image.GetProductID());

	/* compare PID */
	ret = memcmp(dev.GetProductID(), fw_image.GetProductID(), 4);
	if (ret)
		return -1;

	active_vid = dev.GetFirmwareVersionMajor() * 100 +
			dev.GetFirmwareVersionMinor();
	firmware_vid = fw_image.GetFirmwareVersionMajor() * 100 +
			fw_image.GetFirmwareVersionMinor();
	
	gdix_dbg("dev VID: %d, image VID:%d\n", active_vid, firmware_vid);	
	/* compare VID */
	if (active_vid < firmware_vid)
		return 0;
	else
		return -2;
}

int load_sub_firmware(GTx5Device &dev, unsigned int flash_addr,
			unsigned char *fw_data, unsigned int len)
{
	int ret;
	int retry;
	unsigned int i;
	unsigned int unitlen = 0;
	unsigned char temp_buf[65] = {0};
	unsigned int load_data_len = 0;
	unsigned char buf_load_flash[15] = {0x0e, 0x12, 0x00, 0x00, 0x06};
	unsigned short check_sum = 0;
	int retry_load = 0;

	while (retry_load < GDIX_RETRY_TIMES && load_data_len != len) {
		unitlen = (len - load_data_len > RAM_BUFFER_SIZE) ?
			  RAM_BUFFER_SIZE : (len - load_data_len);
		ret = dev.Write(GOODIX_FLASH_BUFFER, &fw_data[load_data_len], unitlen);
		if (ret < 0) {
			gdix_err("Failed load fw, len %d : addr 0x%x, ret=%d\n",
				 unitlen, flash_addr, ret);
			goto load_fail;
		}

		/* inform IC to load 4K data to flash */
		for (check_sum = 0, i = 0; i < unitlen; i += 2) {
			check_sum += (fw_data[load_data_len + i] << 8) +
					fw_data[load_data_len + i + 1];
		}
		buf_load_flash[5] = (unitlen >> 8)&0xFF;
		buf_load_flash[6] = unitlen & 0xFF;
		buf_load_flash[7] = (flash_addr >> 16) & 0xFF;
		buf_load_flash[8] = (flash_addr >> 8) & 0xFF;
		buf_load_flash[9] = (check_sum >> 8) & 0xFF;
		buf_load_flash[10] = check_sum & 0xFF;

		ret = dev.Write(buf_load_flash, 11);
		if (ret < 0) {
			gdix_err("Failed write load flash command, ret =%d\n", ret);
			goto load_fail;
		}
		
		usleep(80000);
		retry = 100;
		do {
			memset(temp_buf, 0, sizeof(temp_buf));
			ret = dev.Read(0x5096, temp_buf, 1);
			if (ret < 0)
				gdix_dbg("Failed read 0x5096, ret=%d\n", ret);
			if (temp_buf[0] == 0xAA)
				break;
			
			usleep(2000);
		} while(--retry);

		if (!retry) {
			gdix_dbg("Read back 0x5096(0x%x) != 0xAA\n", temp_buf[0]);
			gdix_dbg("Reload(%d) subFW:addr:0x%x\n",retry_load, flash_addr);
			/* firmware chechsum err */
			retry_load++;
			ret = -1;
		} else {
			load_data_len += unitlen;
			flash_addr += unitlen;
			retry_load = 0;
			ret = 0;
		}
	}

load_fail:
	return ret;
}

int fw_update(GTx5Device &dev, FirmwareImage &fw_image)
{
	int retry;
	int ret, i;
	unsigned char temp_buf[65];
	bool check_ok = false;
	unsigned char *fw_data = NULL;
	unsigned char buf_switch_to_patch[] = {0x00, 0x10, 0x00, 0x00, 0x01, 0x01};
	unsigned char buf_start_update[] = {0x00, 0x11, 0x00, 0x00, 0x01, 0x01};
	unsigned char buf_restart[] = {0x0E, 0x13, 0x00, 0x00, 0x01, 0x01};

	int sub_fw_num = 0;
	unsigned char sub_fw_type;
	unsigned int sub_fw_len;
	unsigned int sub_fw_flash_addr;
	unsigned int sub_fw_info_pos = SUB_FW_INFO_OFFSET;
	unsigned int fw_image_offset = SUB_FW_DATA_OFFSET;

	ret = dev.Write(buf_switch_to_patch, sizeof(buf_switch_to_patch)) ;
	if (ret < 0) {
		gdix_err("Failed switch to patch\n");
		return ret;
	}
	
	usleep(250000);
	retry = GDIX_RETRY_TIMES;
	do {
		ret = dev.Read(0x5095, temp_buf, 1);
		if (ret < 0) {
			gdix_err("Failed read 0x5095, ret = %d\n", ret);
			goto update_err;
		} 
		if (temp_buf[0] == 0xDD)
			break;
		gdix_info("0x5095 value is 0x%x != 0xDD, retry\n", temp_buf[0]);
		usleep(30000);
	} while (--retry);

	if (!retry) {
		gdix_err("Reg 0x5095 != 0xDD\n");
		ret = -2;
		goto update_err;
	}

	/* Start update */
	ret = dev.Write(buf_start_update, sizeof(buf_start_update)) ;
	if (ret < 0) {
		gdix_err("Failed start update, ret=%d\n", ret);
		goto update_err;
	}

	/* write five 0x55 to 0xC000 and read back */
	retry = GDIX_RETRY_TIMES;
	do {
		for (i = 0; i < 5; i++)
			temp_buf[i] = 0x55;
		ret = dev.Write(GOODIX_FLASH_BUFFER, temp_buf, 5) ;
		if (ret < 0) {
			gdix_err("Failed write flash buffer, ret=%d\n", ret);
			goto update_err;
		}

		memset(temp_buf, 0, 5);
		ret = dev.Read(GOODIX_FLASH_BUFFER, temp_buf, 5);
		if (ret > 0) {
			check_ok = true;
			for (i = 0; i < 5; i++) {
				if (temp_buf[i] != 0x55)
					check_ok = false;
			}
			if (check_ok)
				break;
		}
		usleep(2000);
	} while (--retry);

	if (!retry) {
		gdix_err("Flash buffer clear failed\n");
		ret = -3;
		goto update_err;
	}

	/* Start load firmware */
	fw_data = fw_image.GetFirmwareData();
	if (!fw_data) {
		gdix_err("No valid fw data \n");
		ret = -4;
		goto update_err;
	}

	sub_fw_num = fw_data[FW_IMAGE_SUB_FWNUM_OFFSET];
	sub_fw_info_pos = SUB_FW_INFO_OFFSET;
	fw_image_offset = SUB_FW_DATA_OFFSET;

	for (i = 0; i < sub_fw_num; i++) {
		sub_fw_type = fw_data[sub_fw_info_pos];
		sub_fw_len = (fw_data[sub_fw_info_pos + 1] << 24) |
			     (fw_data[sub_fw_info_pos + 2] << 16) |
			     (fw_data[sub_fw_info_pos + 3] << 8) | 
			     fw_data[sub_fw_info_pos + 4];
		
		sub_fw_flash_addr = (fw_data[sub_fw_info_pos + 5] << 8) |
				    fw_data[sub_fw_info_pos + 6];

		if (sub_fw_type != 0x02) {
			gdix_info("Sub firmware type does not math:type=%d\n",
				  sub_fw_type);
			fw_image_offset += sub_fw_len;
			sub_fw_info_pos += 8;
			continue;
		}

		ret = load_sub_firmware(dev, sub_fw_flash_addr,
					&fw_data[fw_image_offset], sub_fw_len);
		if (ret < 0) {
			gdix_dbg("Failed load sub firmware, ret=%d\n", ret);
			goto update_err;
		}

		/* reset IC */
		retry = 3;
		do {
			ret = dev.Write(buf_restart, sizeof(buf_restart));
			if (ret < 0)
				gdix_dbg("Failed write restart command, ret=%d\n", ret);
			usleep(20000);
		} while(--retry);
		usleep(300000);
	
		return 0;
	}
	ret = -5; /* No valid firmware data found */
update_err:
	return ret;
}

int main(int argc, char **argv)
{
	int ret = 0;
	int opt;
	int index;
	int retry;
	GTx5Device gtx5_dev;
	FirmwareImage fw_image;

        char *deviceName = NULL;
	const char *firmwareName = NULL;
	bool force = false;
	static struct option long_options[] = {
		{"help", 0, NULL, 'h'},
		{"force", 0, NULL, 'f'},
		{"device", 1, NULL, 'd'},
		{"fw-props", 0, NULL, 'p'},
		{"version", 0, NULL, 'v'},
		{0, 0, 0, 0},
	};
	bool printFirmwareProps = false;
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
			default:
				break;

		}
	}

	if (printFirmwareProps) {
		char props_buf[60] = {0};
		ret = GetFirmwareProps(deviceName, props_buf, sizeof(props_buf));
		if (ret) {
			printf("Failed to read properties from device %s\n",
				 deviceName);
			return 1;
		} else {
			printf("%s\n", props_buf);
			return 0;
		}
	}

	if (optind < argc) {
		firmwareName = argv[optind];
	} else {
		printHelp(argv[0]);
		return -1;
	}

	ret = gtx5_dev.Open(deviceName);
	if (ret) {
		gdix_err("failed open device:%s\n", deviceName);
		return -1;
	}

	ret = fw_image.Initialize(firmwareName);
	if (ret) {
		gdix_err("Failed read firmware file:%s\n", firmwareName);
		return -2;
	}

	if (!force) {
		ret = check_update(gtx5_dev, fw_image);
		if (ret) {
			gdix_err("Doesn't meet the update conditions\n");
			return -3;
		}
	}

	retry = 0;
	do {
		ret = fw_update(gtx5_dev, fw_image);
		if (ret) {
			gdix_dbg("Update failed\n");
			usleep(200000);
		} else {
			usleep(300000);
			gdix_dbg("Update success\n");
			return 0;
		}
	} while (retry++ < 3);
	gdix_err("Firmware update err:ret=%d\n", ret);
	return -4;
}
