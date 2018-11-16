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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>

#include <linux/types.h>
#include <linux/input.h>
#include <linux/hidraw.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/inotify.h>

#include "../gtp_util.h"
#include "gtx3_update.h"
#include "gtx3.h"
#include "gtx3_firmware_image.h"

#define CFG_FLASH_ADDR 0x3e000

GTx3Update::GTx3Update()
{

}

GTx3Update::~GTx3Update()
{
	
}

int GTx3Update::fw_update(unsigned int firmware_flag)
{
    int retry;
	int ret, i;
	unsigned char temp_buf[65];
	unsigned char *fw_data = NULL;
	unsigned char buf_switch_to_patch[] = {0x00, 0x10, 0x00, 0x00, 0x01, 0x01};
	unsigned char buf_start_update[] = {0x00, 0x11, 0x00, 0x00, 0x01, 0x01};
	unsigned char buf_restart[] = {0x0E, 0x13, 0x00, 0x00, 0x01, 0x01};

	int sub_fw_num = 0;
	unsigned char sub_fw_type;
	unsigned int sub_fw_len;
	unsigned int sub_fw_flash_addr;
	unsigned int sub_fw_info_pos = image->GetFirmwareSubFwInfoOffset();//SUB_FW_INFO_OFFSET;
	unsigned int fw_image_offset = image->GetFirmwareSubFwDataOffset();// SUB_FW_DATA_OFFSET;

	ret = dev->Write(buf_switch_to_patch, sizeof(buf_switch_to_patch)) ;
	if (ret < 0) {
		gdix_err("Failed switch to patch\n");
		return ret;
	}
	
	usleep(250000);
	retry = GDIX_RETRY_TIMES;
	do {
		ret = dev->Read(BL_STATE_ADDR, temp_buf, 1);
		gdix_dbg("BL_STATE_ADDR\n");
		if (ret < 0) {
			gdix_err("Failed read 0x%x, ret = %d\n", BL_STATE_ADDR, ret);
			goto update_err;
		} 
		if (temp_buf[0] == 0xDD)
			break;
		gdix_info("0x%x value is 0x%x != 0xDD, retry\n", BL_STATE_ADDR, temp_buf[0]);
		usleep(30000);
	} while (--retry);

	if (!retry) {
		gdix_err("Reg 0x%x != 0xDD\n", BL_STATE_ADDR);
		ret = -2;
		goto update_err;
	}

	/* Start update */
	ret = dev->Write(buf_start_update, sizeof(buf_start_update)) ;
	if (ret < 0) {
		gdix_err("Failed start update, ret=%d\n", ret);
		goto update_err;
	}
	usleep(100000);

	/* Start load firmware */
	fw_data = image->GetFirmwareData();
	if (!fw_data) {
		gdix_err("No valid fw data \n");
		ret = -4;
		goto update_err;
	}

	sub_fw_num = image->GetFirmwareSubFwNum();//fw_data[FW_IMAGE_SUB_FWNUM_OFFSET];
	// sub_fw_info_pos = image->GetFirmwareSubFwInfoOffset();// SUB_FW_INFO_OFFSET;
	// fw_image_offset = image->GetFirmwareSubFwDataOffset() ; //SUB_FW_DATA_OFFSET;
	gdix_dbg("load sub firmware, sub_fw_num=%d\n", sub_fw_num);
	for (i = 0; i < sub_fw_num; i++) {
		sub_fw_type = fw_data[sub_fw_info_pos];
		gdix_dbg("load sub firmware, sub_fw_type=0x%x\n", sub_fw_type);
		sub_fw_len = (fw_data[sub_fw_info_pos + 1] << 24) |
			     (fw_data[sub_fw_info_pos + 2] << 16) |
			     (fw_data[sub_fw_info_pos + 3] << 8) | 
			     fw_data[sub_fw_info_pos + 4];
		
		sub_fw_flash_addr = (fw_data[sub_fw_info_pos + 5] << 8) |
				      fw_data[sub_fw_info_pos + 6];
		sub_fw_flash_addr = sub_fw_flash_addr << 8;
		if(!(firmware_flag & (0x01 << sub_fw_type ))){
			gdix_info("Sub firmware type does not math:type=%d\n",
				  sub_fw_type);
			fw_image_offset += sub_fw_len;
			sub_fw_info_pos += 8;
			continue;
		}
		gdix_dbg("load sub firmware addr:0x%x,len:0x%x\n",sub_fw_flash_addr,sub_fw_len);
		ret = load_sub_firmware(sub_fw_flash_addr,&fw_data[fw_image_offset], sub_fw_len);
		if (ret < 0) {
			gdix_dbg("Failed load sub firmware, ret=%d\n", ret);
			goto update_err;
		}
		fw_image_offset += sub_fw_len;
		sub_fw_info_pos += 8;
	}
	if(sub_fw_num == 0)
		return -5;
	/* reset IC */
	gdix_dbg("reset ic\n");
	retry = 3;
	do {
		ret = dev->Write(buf_restart, sizeof(buf_restart));
		if (ret < 0)
			gdix_dbg("Failed write restart command, ret=%d\n", ret);
		usleep(20000);
	} while(--retry);
	usleep(300000);
	return 0;
	ret = -5; /* No valid firmware data found */
update_err:
	return ret;
}


int GTx3Update::cfg_update()
{
	int retry;
	int ret, i;
	unsigned char temp_buf[65];
	unsigned char *fw_data = NULL;
	unsigned char buf_switch_to_patch[] = {0x00, 0x10, 0x00, 0x00, 0x01, 0x01};
	unsigned char buf_start_update[] = {0x00, 0x11, 0x00, 0x00, 0x01, 0x01};
	unsigned char buf_restart[] = {0x0E, 0x13, 0x00, 0x00, 0x01, 0x01};
	unsigned char cfg_ver_after[3];
	unsigned char cfg_ver_before[3];
	unsigned char cfg_ver_infile;
	bool findMatchCfg = false;
	unsigned char* cfg = NULL;

	int sub_cfg_num = image->GetConfigSubCfgNum();
	unsigned char sub_cfg_id;
	unsigned int sub_cfg_len;
	unsigned int sub_cfg_info_pos = image->GetConfigSubCfgInfoOffset();
	unsigned int cfg_offset = image->GetConfigSubCfgDataOffset();

	//before update config,read curr config version
	dev->Read(0x8050,cfg_ver_before,3);
	gdix_dbg("Before update,cfg version is 0x%02x 0x%02x 0x%02x\n",
		cfg_ver_before[0],cfg_ver_before[1],cfg_ver_before[2]);
	unsigned char cks = cfg_ver_before[0]+cfg_ver_before[1]+cfg_ver_before[2];
	if(cks != 0)
		gdix_err("Warning : cfg before cks err!\n");

	/* Start load config */
	fw_data = image->GetFirmwareData();
	if (!fw_data) {
		gdix_err("No valid fw data \n");
		ret = -4;
		goto update_err;
	}

	//fw_data[FW_IMAGE_SUB_FWNUM_OFFSET];
	gdix_dbg("load sub config, sub_cfg_num=%d\n", sub_cfg_num);
	for (i = 0; i < sub_cfg_num; i++) {
		sub_cfg_id = fw_data[sub_cfg_info_pos];
		gdix_dbg("load sub config, sub_cfg_id=0x%x\n", sub_cfg_id);

		sub_cfg_len = (fw_data[sub_cfg_info_pos + 1] << 8) | 
			       fw_data[sub_cfg_info_pos + 2];
		
		if(dev->GetSensorID() == sub_cfg_id){
			findMatchCfg = true;
			cfg = &fw_data[cfg_offset];
			cfg_ver_infile = cfg[0];
			gdix_info("Find a cfg match sensorID:ID=%d,cfg version=%d\n",
				  dev->GetSensorID(),cfg_ver_infile);
			break;
		}
		cfg_offset += sub_cfg_len;
		sub_cfg_info_pos += 3;
	}

	if(sub_cfg_num == 0)
		return -5;
	if(findMatchCfg)
	{
		ret = dev->Write(buf_switch_to_patch, sizeof(buf_switch_to_patch)) ;
		if (ret < 0) {
			gdix_err("Failed switch to patch\n");
			return ret;
		}
		
		usleep(250000);
		retry = GDIX_RETRY_TIMES;
		do {
			ret = dev->Read(BL_STATE_ADDR, temp_buf, 1);
			gdix_dbg("BL_STATE_ADDR\n");
			if (ret < 0) {
				gdix_err("Failed read 0x%x, ret = %d\n", BL_STATE_ADDR, ret);
				goto update_err;
			} 
			if (temp_buf[0] == 0xDD)
				break;
			gdix_info("0x%x value is 0x%x != 0xDD, retry\n", BL_STATE_ADDR, temp_buf[0]);
			usleep(30000);
		} while (--retry);

		if (!retry) {
			gdix_err("Reg 0x%x != 0xDD\n", BL_STATE_ADDR);
			ret = -2;
			goto update_err;
		}

		/* Start update */
		ret = dev->Write(buf_start_update, sizeof(buf_start_update)) ;
		if (ret < 0) {
			gdix_err("Failed start update, ret=%d\n", ret);
			goto update_err;
		}
		usleep(100000);

		//start load cfg
		ret = load_sub_firmware(CFG_FLASH_ADDR,&fw_data[cfg_offset], sub_cfg_len);

		/* reset IC */
		gdix_dbg("reset ic\n");
		retry = 3;
		do {
			ret = dev->Write(buf_restart, sizeof(buf_restart));
			if (ret < 0)
				gdix_dbg("Failed write restart command, ret=%d\n", ret);
			usleep(20000);
		} while(--retry);
		usleep(300000);

		//before update config,read curr config version
		dev->Read(0x8050,cfg_ver_after,3);
		gdix_dbg("After update,cfg version is 0x%02x 0x%02x 0x%02x\n",
			cfg_ver_after[0],cfg_ver_after[1],cfg_ver_after[2]);
		unsigned char cks = cfg_ver_after[0]+cfg_ver_after[1]+cfg_ver_after[2];
		if(cks != 0)
		{	
			gdix_err("Error : cfg after cks err!\n");
			return -6;
		}

		return 0;
	}
update_err:
	return ret;
}

