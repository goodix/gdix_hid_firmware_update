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
#include "gtx2_update.h"
#include "gtx2.h"
#include "gtx2_firmware_image.h"

//cfg addr
#define CFG_FLASH_ADDR 0x3e000

GTx2Update::GTx2Update()
{
    
}

GTx2Update::~GTx2Update()
{
    
}



int GTx2Update::Run(void *para)
{
    int ret = 0;
    int retry = 0;
	updateFlag flag = none;
    if(!m_Initialized){
        gdix_err("Can't Process Update Before Initialized.\n");
        return -1;
    }
    //get all parameter
    pGTUpdatePara parameter = (pGTUpdatePara)para;

	//check if the image has config
	if(image->HasConfig()){
		flag = image->GetUpdateFlag();
	}
	else{
		//default for firmware
		flag = firmware;
	}

	gdix_dbg("Flag = %d\n",flag);

    if(!parameter->force)
    {
        ret = check_update();
		if (ret) {
			gdix_err("Doesn't meet the update conditions\n");
            return -3;
		}
    }

	if(flag & firmware){
		retry = 0;
		do {
			ret = fw_update(parameter->firmwareFalg);
			if (ret) {
				gdix_dbg("Update failed\n");
				usleep(200000);
			} else {
				usleep(300000);
				gdix_dbg("Update success\n");
				break;
			}
		} while (retry++ < 3);
		if(ret){
			gdix_err("Firmware update err:ret=%d\n", ret);
			return ret;
		}
	}

	//before go forward,Set Basic Properties to refresh SensorID
	dev->SetBasicProperties();

	if(flag & config){
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
		if(ret){
			gdix_err("config update err:ret=%d\n", ret);
			return ret;
		}
	}
    return 0;
}

int GTx2Update::load_sub_firmware(unsigned int flash_addr,unsigned char *fw_data, unsigned int len)
{
    int ret;
	int retry;
	unsigned int i;
	unsigned int unitlen = 0;
	unsigned char temp_buf[65] = {0};
	unsigned int load_data_len = 0;
	unsigned char buf_load_flash[15] = {0x0e, 0x12, 0x00, 0x00, 0x06};
	unsigned short check_sum = 0;
	unsigned char dummy = 0;
	int retry_load = 0;

	while (retry_load < GDIX_RETRY_TIMES && load_data_len != len) {
		unitlen = (len - load_data_len > RAM_BUFFER_SIZE) ?
			  RAM_BUFFER_SIZE : (len - load_data_len);
		ret = dev->Write(FLASH_BUFFER_ADDR, &fw_data[load_data_len], unitlen);
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

		ret = dev->Write(buf_load_flash, 11);
		if (ret < 0) {
			gdix_err("Failed write load flash command, ret =%d\n", ret);
			goto load_fail;
		}
		
		usleep(80000);
		retry = 100;
		do {
			memset(temp_buf, 0, sizeof(temp_buf));
			ret = dev->Read(FLASH_RESULT_ADDR, temp_buf, 1);
			if (ret < 0)
				gdix_dbg("Failed read 0x%x, ret=%d\n", FLASH_RESULT_ADDR, ret);
			if (temp_buf[0] == 0xAA)
				break;
			
			usleep(2000);
		} while(--retry);

		if (!retry) {
			gdix_dbg("Read back 0x%x(0x%x) != 0xAA\n", FLASH_RESULT_ADDR, temp_buf[0]);
			gdix_dbg("Reload(%d) subFW:addr:0x%x\n", retry_load, flash_addr);
			/* firmware chechsum err */
			retry_load++;
			ret = -1;
		} else {
			load_data_len += unitlen;
			flash_addr += unitlen;
			retry_load = 0;
			dev->Write(0x5096,&dummy,1);
			ret = 0;
		}
	}

load_fail:
	return ret;
}

int GTx2Update::fw_update(unsigned int firmware_flag)
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
		/*sub_fw_len = (fw_data[sub_fw_info_pos + 1] << 24) |
			     (fw_data[sub_fw_info_pos + 2] << 16) |
			     (fw_data[sub_fw_info_pos + 3] << 8) | 
			     fw_data[sub_fw_info_pos + 4];*/
		sub_fw_len = (fw_data[sub_fw_info_pos + 1] << 8) | 
			       fw_data[sub_fw_info_pos + 2];
		
		sub_fw_flash_addr = (fw_data[sub_fw_info_pos + 3] << 8) |
				      fw_data[sub_fw_info_pos + 4];
		sub_fw_flash_addr = sub_fw_flash_addr << 8;
		if(!(firmware_flag & (0x01 << sub_fw_type ))){
			gdix_info("Sub firmware type does not math:type=%d\n",
				  sub_fw_type);
			fw_image_offset += sub_fw_len;
			sub_fw_info_pos += 8;
			continue;
		}
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

int GTx2Update::cfg_update()
{
	int retry;
	int ret, i;
	unsigned char temp_buf[65];
	unsigned char *fw_data = NULL;
	unsigned char cfg_ver_after;
	unsigned char cfg_ver_before;
	unsigned char cfg_ver_infile;
	bool findMatchCfg = false;
	unsigned char* cfg0x8050 = NULL;//2 frame of config in memory
	unsigned char* cfg0xBF7B = NULL;

	int sub_cfg_num = image->GetConfigSubCfgNum();
	unsigned char sub_cfg_id;
	unsigned int sub_cfg_len;
	unsigned int sub_cfg_info_pos = image->GetConfigSubCfgInfoOffset();
	unsigned int cfg_offset = image->GetConfigSubCfgDataOffset();

	//before update config,read curr config version
	dev->Read(0x8050,temp_buf,1);
	cfg_ver_before = temp_buf[0];
	gdix_dbg("Before update,cfg version is %d\n",cfg_ver_before);
	

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
			cfg0x8050 = &fw_data[cfg_offset];
			cfg0xBF7B = &cfg0x8050[0x813f-0x8050];
			cfg_ver_infile = cfg0x8050[0];
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
		retry = 3;
		do{
			usleep(5000);
			//start download cfg
			ret = dev->Write(0xBF7B,cfg0xBF7B,0xBFFA-0xBF7B+1);
			if(ret<0){
				gdix_err("Failed to Write cfg to cfg0xBF7B\n");
				continue;
			}
			ret = dev->Write(0x8050,cfg0x8050,0x813F-0x8050+1);//cfg start addr = 0x8050
			if(ret<0){
				gdix_err("Failed to Write cfg to 0x8050\n");
				continue;
			}
			break;
			
		}while(retry-->0);
	}
	usleep(1000000);

	dev->Read(0x8050,temp_buf,1);
	cfg_ver_after = temp_buf[0];
	gdix_dbg("After update,cfg version is %d\n",cfg_ver_after);

	if(cfg_ver_after != cfg_ver_infile){
		gdix_dbg("After update,cfg version is no equal to cg version in file.\n");
		ret = -1;
		goto update_err;
	}
	return 0;

	
update_err:
	return ret;
}