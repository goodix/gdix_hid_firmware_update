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
#include "gtx8_update.h"
#include "gtx8.h"
#include "gtx8_firmware_image.h"

#define CFG_FLASH_ADDR 0x1E000
#define CFG_START_ADDR 0X60DC
#define CMD_ADDR       0x60cc


GTx8Update::GTx8Update()
{
	is_cfg_flashed_with_isp = false;
}

GTx8Update::~GTx8Update()
{
	
}

int GTx8Update::Run(void *para)
{
    int ret = 0;
    int retry = 0;
	updateFlag flag = NO_NEED_UPDATE;

    if(!m_Initialized){
        gdix_err("Can't Process Update Before Initialized.\n");
        return -1;
    }
    //get all parameter
    pGTUpdatePara parameter = (pGTUpdatePara)para;

	//check if the image has config
	if (image->HasConfig()) {
		flag = image->GetUpdateFlag();
	} else {
		//default for firmware
		flag = NEED_UPDATE_FW;
	}

	gdix_dbg("fw update flag is 0x%x\n",flag);

    if (!parameter->force) {
        ret = check_update();
		if (ret) {
			gdix_err("Doesn't meet the update conditions\n");
            return -3;
		}
    }

	if (flag & NEED_UPDATE_FW) {
		retry = 0;
		do {
			ret = fw_update(parameter->firmwareFlag);
			if (ret) {
				gdix_dbg("Update failed\n");
				usleep(200000);
			} else {
				usleep(300000);
				gdix_dbg("Update success\n");
				break;
			}
		} while (retry++ < 3);
		if (ret) {
			gdix_err("Firmware update err:ret=%d\n", ret);
			return ret;
		}
	}

	/* do interactive config update when is_cfg_flashed_with_isp is false.
	 * Otherwise the config is written to flash by ISP.
	 */
	if(this->is_cfg_flashed_with_isp == false &&
		flag & NEED_UPDATE_CONFIG) {
		gdix_dbg("Update config interactively");
		retry = 0;
		do {
			ret = cfg_update();
			if (ret) {
				gdix_dbg("Update cfg failed\n");
				usleep(200000);
			} else {
				usleep(300000);
				gdix_dbg("Update cfg success\n");
				break;
			}
		} while (retry++ < 3);
		if (ret) {
			gdix_err("config update err:ret=%d\n", ret);
			return ret;
		}
	}
    return 0;
}

int GTx8Update::fw_update(unsigned int firmware_flag)
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
		goto update_err;
	}
	
	usleep(250000);
	retry = GDIX_RETRY_TIMES;
	do {
		ret = dev->Read(BL_STATE_ADDR, temp_buf, 1);
		gdix_dbg("BL_STATE_ADDR:0x%x\n", BL_STATE_ADDR);
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
	sub_fw_info_pos = image->GetFirmwareSubFwInfoOffset();// SUB_FW_INFO_OFFSET;
	fw_image_offset = image->GetFirmwareSubFwDataOffset() ; //SUB_FW_DATA_OFFSET;
	gdix_dbg("load sub firmware, sub_fw_num=%d\n", sub_fw_num);
	if(sub_fw_num == 0)
		return -5;

	/* load normal firmware package */
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
		if (!(firmware_flag & (0x01 << sub_fw_type ))){
			gdix_info("Sub firmware type does not math:type=%d\n",
				  sub_fw_type);
			fw_image_offset += sub_fw_len;
			sub_fw_info_pos += 8;
			continue;
		}

		/* if sub fw type is HID subsystem we need compare version before update */
		// TODO update hid subsystem
		gdix_dbg("load sub firmware addr:0x%x,len:0x%x\n",sub_fw_flash_addr,sub_fw_len);
		ret = load_sub_firmware(sub_fw_flash_addr,&fw_data[fw_image_offset], sub_fw_len);
		if (ret < 0) {
			gdix_dbg("Failed load sub firmware, ret=%d\n", ret);
			goto update_err;
		}
		fw_image_offset += sub_fw_len;
		sub_fw_info_pos += 8;
	}

	/* flash config with isp if NEED_UPDATE_CONFIG_WITH_ISP flag is setted or
	 * hid subsystem updated.
	 */
	if (image->GetUpdateFlag() & NEED_UPDATE_CONFIG_WITH_ISP) {
		this->is_cfg_flashed_with_isp = true;
		ret = flash_cfg_with_isp();
		if (ret < 0) {
			gdix_err("failed flash config with isp, ret %d\n", ret);
			goto update_err;
		}
	}

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

update_err:
	/* reset IC */
	gdix_dbg("reset ic\n");
	retry = 3;
	do {
		if (dev->Write(buf_restart, sizeof(buf_restart)) < 0)
			gdix_dbg("Failed write restart command\n");
		usleep(20000);
	} while(--retry);

	usleep(300000);
	return ret;
}

int GTx8Update::flash_cfg_with_isp()
{
	int ret = -1, i = 0;
	updateFlag flag = NO_NEED_UPDATE;
	unsigned char *fw_data = NULL;
	unsigned char* cfg = NULL;
	unsigned char sub_cfg_id;
	unsigned int sub_cfg_len;
	unsigned char cfg_ver_infile;
	int sub_cfg_num = image->GetConfigSubCfgNum();
	unsigned int sub_cfg_info_pos = image->GetConfigSubCfgInfoOffset();
	unsigned int cfg_offset = image->GetConfigSubCfgDataOffset();

	if (image->HasConfig() == false || sub_cfg_num <= 0) {
		/* no config found in the bin file */
		return 0;
	}

	flag = image->GetUpdateFlag();
	if (!(flag & NEED_UPDATE_CONFIG)) {
		/* config update flag not set */
		gdix_dbg("flag UPDATE_CONFIG unset\n");
		return 0;
	}

	/* Start load config */
	fw_data = image->GetFirmwareData();
	if (!fw_data) {
		gdix_err("No valid fw data \n");
		return -4;
	}

	//fw_data[FW_IMAGE_SUB_FWNUM_OFFSET];
	gdix_dbg("load sub config, sub_cfg_num=%d\n", sub_cfg_num);
	for (i = 0; i < sub_cfg_num; i++) {
		sub_cfg_id = fw_data[sub_cfg_info_pos];
		gdix_dbg("load sub config, sub_cfg_id=0x%x\n", sub_cfg_id);

		sub_cfg_len = (fw_data[sub_cfg_info_pos + 1] << 8) | 
			       fw_data[sub_cfg_info_pos + 2];
		
		if(dev->GetSensorID() == sub_cfg_id){
			cfg = &fw_data[cfg_offset];
			cfg_ver_infile = cfg[0];
			gdix_dbg("Find a cfg match sensorID:ID=%d,cfg version=%d\n",
				  dev->GetSensorID(),cfg_ver_infile);
			break;
		}
		cfg_offset += sub_cfg_len;
		sub_cfg_info_pos += 3;
	}

	if (!cfg) {
		/* failed found config for sensorID */
		gdix_dbg("Failed found config for sensorID %d, sub_cfg_num %d\n",
				dev->GetSensorID(), sub_cfg_num);
		return -5;
	}

	gdix_dbg("load cfg addr:0x%x,len:0x%x\n",CFG_FLASH_ADDR, sub_cfg_len);
	ret = load_sub_firmware(CFG_FLASH_ADDR, cfg, sub_cfg_len);
	if (ret < 0) {
		gdix_err("Failed flash cofig with ISP, ret=%d\n", ret);
		return ret;
	}
	return 0;
}

int GTx8Update::cfg_update()
{
	int retry;
	int ret = -1, i;
	unsigned char temp_buf[65];
	unsigned char *fw_data = NULL;
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

	if(sub_cfg_num == 0)
		return -5;

	//before update config,read curr config version
	dev->Read(CFG_START_ADDR,cfg_ver_before,3);
	gdix_dbg("Before update,cfg version is 0x%02x 0x%02x 0x%02x\n",
		cfg_ver_before[0],cfg_ver_before[1],cfg_ver_before[2]);

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

	if(findMatchCfg)
	{
		//wait untill ic is free
		retry = 10;
		do {
			ret = dev->Read(CMD_ADDR, temp_buf, 1);
			if (ret < 0) {
				gdix_err("Failed read cfg cmd, ret = %d\n", ret);
				return ret;
			}
			if (temp_buf[0] == 0xff)
				break;
			gdix_info("0x%x value is 0x%x != 0xff, retry\n", CMD_ADDR, temp_buf[0]);
			usleep(10000);
		} while (--retry);
		if (!retry) {
			gdix_err("Reg 0x%x != 0xff\n", CMD_ADDR);
			ret = -2;
			goto update_err;
		}

		//tell ic i want to send cfg
		temp_buf[0] = 0x80;
		temp_buf[1] = 0;
		temp_buf[2] = 0x80;
		ret = dev->Write(CMD_ADDR,temp_buf, 3) ;
		if (ret < 0) {
			gdix_err("Failed write send cfg cmd\n");
			return ret;
		}

		//wait ic to comfirm
		usleep(250000);
		retry = GDIX_RETRY_TIMES;
		do {
			ret = dev->Read(CMD_ADDR, temp_buf, 1);
			gdix_dbg("Wait CMD_ADDR == 0x82...\n");
			if (ret < 0) {
				gdix_err("Failed read 0x%x, ret = %d\n", CMD_ADDR, ret);
			} 
			if (temp_buf[0] == 0x82)
				break;
			gdix_info("0x%x value is 0x%x != 0x82, retry\n", CMD_ADDR, temp_buf[0]);
			usleep(30000);
		} while (--retry);

		if (!retry) {
			gdix_err("Reg 0x%x != 0x82\n", CMD_ADDR);
			ret = -2;
			goto update_err;
		}
		gdix_dbg("Wait CMD_ADDR == 0x82 success.\n");

		/* Start load config */
		ret = dev->Write(CFG_START_ADDR, &fw_data[cfg_offset],sub_cfg_len) ;
		if (ret < 0) {
			gdix_err("Failed write cfg to xdata, ret=%d\n", ret);
			goto update_err;
		}
		usleep(100000);

		//tell ic cfg is ready in xdata
		temp_buf[0] = 0x83;
		ret = dev->Write(CMD_ADDR,temp_buf, 1) ;
		if (ret < 0) {
			gdix_err("Failed write send cfg finish cmd\n");
			return ret;
		}

		//check if ic is ok with the cfg
		usleep(80000);
		retry = GDIX_RETRY_TIMES;
		do {
			ret = dev->Read(CMD_ADDR, temp_buf, 1);
			gdix_dbg("Wait CMD_ADDR == 0xFF...\n");
			if (ret < 0) {
				gdix_err("Failed read 0x%x, ret = %d\n", CMD_ADDR, ret);
			} 
			if (temp_buf[0] == 0xff)
				break;
			gdix_info("0x%x value is 0x%x != 0xFF, retry\n", CMD_ADDR, temp_buf[0]);
			usleep(30000);
		} while (--retry);

		if (!retry) {
			gdix_err("Reg 0x%x != 0xFF\n", CMD_ADDR);
			ret = -2;
			goto update_err;
		}
		gdix_dbg("Wait CMD_ADDR == 0xFF success.\n");

		//before update config,read curr config version
		dev->Read(CFG_START_ADDR,cfg_ver_after,3);
		gdix_dbg("After update,cfg version is 0x%02x 0x%02x 0x%02x\n",
			cfg_ver_after[0],cfg_ver_after[1],cfg_ver_after[2]);

		return 0;
	}
update_err:
	return ret;
}

