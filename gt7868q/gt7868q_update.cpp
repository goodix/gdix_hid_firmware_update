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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <linux/hidraw.h>
#include <linux/input.h>
#include <linux/types.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/inotify.h>

#include "../gtp_util.h"
#include "gt7868q.h"
#include "gt7868q_firmware_image.h"
#include "gt7868q_update.h"

#define CFG_FLASH_ADDR 0x19000
#define CFG_START_ADDR 0X96F8

GT7868QUpdate::GT7868QUpdate() { is_cfg_flashed_with_isp = false; }

GT7868QUpdate::~GT7868QUpdate() {}

int GT7868QUpdate::Run(void *para)
{
	int ret = 0;
	int retry = 0;
	updateFlag flag = NO_NEED_UPDATE;

	if (!m_Initialized) {
		gdix_err("Can't Process Update Before Initialized.\n");
		return -1;
	}
	// get all parameter
	pGTUpdatePara parameter = (pGTUpdatePara)para;

	// check if the image has config
	if (image->HasConfig()) {
		flag = image->GetUpdateFlag();
	} else {
		// default for firmware
		flag = NEED_UPDATE_FW;
	}

	gdix_dbg("fw update flag is 0x%x\n", flag);

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
	if (this->is_cfg_flashed_with_isp == false && flag & NEED_UPDATE_CONFIG) {
		gdix_dbg("Update config interactively\n");
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

int GT7868QUpdate::fw_update(unsigned int firmware_flag)
{
	int retry;
	int ret, i;
	unsigned char temp_buf[65];
	unsigned char *fw_data = NULL;
	unsigned char buf_switch_to_patch[] = {0x00, 0x10, 0x00, 0x00, 0x01, 0x01};
	unsigned char buf_start_update[] = {0x00, 0x11, 0x00, 0x00, 0x01, 0x01};
	unsigned char buf_restart[] = {0x0E, 0x13, 0x00, 0x00, 0x01, 0x01};
	unsigned char buf_switch_ptp_mode[] = {0x32, 0x00, 0x00, 0x00, 0x32};
	unsigned char buf_dis_report_coor[] = {0x33, 0x00, 0x00, 0x00, 0x33};
	unsigned char buf_en_report_coor[] = {0x34, 0x00, 0x00, 0x00, 0x34};

	int sub_fw_num = 0;
	unsigned char sub_fw_type;
	unsigned int sub_fw_len;
	unsigned int sub_fw_flash_addr;
	unsigned int sub_fw_info_pos =
		image->GetFirmwareSubFwInfoOffset(); // SUB_FW_INFO_OFFSET;
	unsigned int fw_image_offset =
		image->GetFirmwareSubFwDataOffset(); // SUB_FW_DATA_OFFSET;

	ret = dev->Write(buf_switch_to_patch, sizeof(buf_switch_to_patch));
	if (ret < 0) {
		gdix_err("Failed switch to patch\n");
		goto update_err;
	}
	usleep(250000);

	/*dis report coor*/
	gdix_info("disable report coor\n");
	retry = 3;
	do {
		ret = dev->Write(CMD_ADDR, buf_dis_report_coor,
						 sizeof(buf_dis_report_coor));
		if (ret < 0) {
			gdix_err("Failed disable report coor\n");
		}
	} while (--retry);

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
		gdix_info("0x%x value is 0x%x != 0xDD, retry\n", BL_STATE_ADDR,
				  temp_buf[0]);
		usleep(30000);
	} while (--retry);

	if (!retry) {
		gdix_err("Reg 0x%x != 0xDD\n", BL_STATE_ADDR);
		ret = -2;
		goto update_err;
	}

	/* Start update */
	ret = dev->Write(buf_start_update, sizeof(buf_start_update));
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

	sub_fw_num =
		image->GetFirmwareSubFwNum(); // fw_data[FW_IMAGE_SUB_FWNUM_OFFSET];
	sub_fw_info_pos =
		image->GetFirmwareSubFwInfoOffset(); // SUB_FW_INFO_OFFSET;
	fw_image_offset =
		image->GetFirmwareSubFwDataOffset(); // SUB_FW_DATA_OFFSET;
	gdix_dbg("load sub firmware, sub_fw_num=%d\n", sub_fw_num);
	if (sub_fw_num == 0) {
		ret = -5;
		goto update_err;
	}

	/* load normal firmware package */
	for (i = 0; i < sub_fw_num; i++) {
		sub_fw_type = fw_data[sub_fw_info_pos];
		gdix_dbg("load sub firmware, sub_fw_type=0x%x\n", sub_fw_type);
		sub_fw_len = (fw_data[sub_fw_info_pos + 1] << 24) |
					 (fw_data[sub_fw_info_pos + 2] << 16) |
					 (fw_data[sub_fw_info_pos + 3] << 8) |
					 fw_data[sub_fw_info_pos + 4];

		sub_fw_flash_addr =
			(fw_data[sub_fw_info_pos + 5] << 8) | fw_data[sub_fw_info_pos + 6];
		sub_fw_flash_addr = sub_fw_flash_addr << 8;
		if (!(firmware_flag & (0x01 << sub_fw_type))) {
			gdix_info("Sub firmware type does not math:type=%d\n", sub_fw_type);
			fw_image_offset += sub_fw_len;
			sub_fw_info_pos += 8;
			continue;
		}

		/* if sub fw type is HID subsystem we need compare version before update
		 */
		// TODO update hid subsystem
		gdix_dbg("load sub firmware addr:0x%x,len:0x%x\n", sub_fw_flash_addr,
				 sub_fw_len);
		ret = load_sub_firmware(sub_fw_flash_addr, &fw_data[fw_image_offset],
								sub_fw_len);
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

	/*en report coor*/
	gdix_info("enable report coor\n");
	ret = dev->Write(CMD_ADDR, buf_en_report_coor, sizeof(buf_en_report_coor));
	if (ret < 0) {
		gdix_err("Failed enable report coor\n");
	}

	/* reset IC */
	gdix_dbg("reset ic\n");
	retry = 3;
	do {
		ret = dev->Write(buf_restart, sizeof(buf_restart));
		if (ret >= 0)
			break;
		usleep(20000);
	} while (--retry);
	if (retry == 0 && ret < 0)
		gdix_dbg("Failed write restart command, ret=%d\n", ret);
	else
		ret = 0;

	usleep(300000);

	if (dev->Write(CMD_ADDR, buf_switch_ptp_mode, sizeof(buf_switch_ptp_mode)) <
		0) {
		gdix_err("Failed switch to ptp mode\n");
	}
	return ret;

update_err:
	/*en report coor*/
	gdix_info("enable report coor\n");
	ret = dev->Write(CMD_ADDR, buf_en_report_coor, sizeof(buf_en_report_coor));
	if (ret < 0) {
		gdix_err("Failed enable report coor\n");
	}

	/* reset IC */
	gdix_dbg("reset ic\n");
	retry = 3;
	do {
		if (dev->Write(buf_restart, sizeof(buf_restart)) >= 0)
			break;
		usleep(20000);
	} while (--retry);
	if (retry == 0 && ret < 0)
		gdix_dbg("Failed write restart command, ret=%d\n", ret);

	usleep(300000);
	if (dev->Write(CMD_ADDR, buf_switch_ptp_mode, sizeof(buf_switch_ptp_mode)) <
		0) {
		gdix_err("Failed switch to ptp mode\n");
	}
	return ret;
}

int GT7868QUpdate::flash_cfg_with_isp()
{
	int ret = -1, i = 0;
	updateFlag flag = NO_NEED_UPDATE;
	unsigned char *fw_data = NULL;
	unsigned char *cfg = NULL;
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

	// fw_data[FW_IMAGE_SUB_FWNUM_OFFSET];
	gdix_dbg("load sub config, sub_cfg_num=%d\n", sub_cfg_num);
	for (i = 0; i < sub_cfg_num; i++) {
		sub_cfg_id = fw_data[sub_cfg_info_pos];
		gdix_dbg("load sub config, sub_cfg_id=0x%x\n", sub_cfg_id);

		sub_cfg_len = (fw_data[sub_cfg_info_pos + 1] << 8) |
					  fw_data[sub_cfg_info_pos + 2];

		if (dev->GetSensorID() == sub_cfg_id) {
			cfg = &fw_data[cfg_offset];
			cfg_ver_infile = cfg[0];
			gdix_dbg("Find a cfg match sensorID:ID=%d,cfg version=%d\n",
					 dev->GetSensorID(), cfg_ver_infile);
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

	gdix_dbg("load cfg addr:0x%x,len:0x%x\n", CFG_FLASH_ADDR, sub_cfg_len);
	ret = load_sub_firmware(CFG_FLASH_ADDR, cfg, sub_cfg_len);
	if (ret < 0) {
		gdix_err("Failed flash cofig with ISP, ret=%d\n", ret);
		return ret;
	}
	return 0;
}

void GT7868QUpdate::cmd_init(unsigned char *cmd_buf, unsigned char cmd,
							 unsigned short cmd_data)
{
	unsigned short chksum;
	cmd_buf[0] = cmd;
	cmd_buf[1] = (cmd_data >> 8) & 0xff;
	cmd_buf[2] = cmd_data & 0xff;
	chksum = cmd_buf[0] + cmd_buf[1] + cmd_buf[2];
	cmd_buf[3] = (chksum >> 8) & 0xff;
	cmd_buf[4] = chksum & 0xff;
}

int GT7868QUpdate::cfg_update()
{
	int retry;
	int ret = -1, i;
	unsigned char temp_buf[65];
	unsigned char *fw_data = NULL;
	unsigned char cfg_ver_after[3];
	unsigned char cfg_ver_before[3];
	unsigned char tmp_cmd_buf[5];
	unsigned char cfg_ver_infile;
	bool findMatchCfg = false;
	unsigned char *cfg = NULL;

	int sub_cfg_num = image->GetConfigSubCfgNum();
	unsigned char sub_cfg_id;
	unsigned int sub_cfg_len;
	unsigned char buf_dis_report_coor[] = {0x33, 0x00, 0x00, 0x00, 0x33};
	unsigned char buf_en_report_coor[] = {0x34, 0x00, 0x00, 0x00, 0x34};
	unsigned int sub_cfg_info_pos = image->GetConfigSubCfgInfoOffset();
	unsigned int cfg_offset = image->GetConfigSubCfgDataOffset();

	if (sub_cfg_num == 0)
		return -5;
	/*dis report coor*/
	gdix_info("disable report coor in cfg update\n");
	retry = 3;
	do {
		ret = dev->Write(CMD_ADDR, buf_dis_report_coor,
						 sizeof(buf_dis_report_coor));
		if (ret < 0) {
			gdix_err("Failed disable report coor\n");
		}
	} while (--retry);

	// before update config,read curr config version
	dev->Read(CFG_START_ADDR, cfg_ver_before, 3);
	gdix_dbg("Before update,cfg version is 0x%02x 0x%02x 0x%02x\n",
			 cfg_ver_before[0], cfg_ver_before[1], cfg_ver_before[2]);

	/* Start load config */
	fw_data = image->GetFirmwareData();
	if (!fw_data) {
		gdix_err("No valid fw data \n");
		ret = -4;
		goto update_err;
	}

	// fw_data[FW_IMAGE_SUB_FWNUM_OFFSET];
	gdix_dbg("load sub config, sub_cfg_num=%d\n", sub_cfg_num);
	for (i = 0; i < sub_cfg_num; i++) {
		sub_cfg_id = fw_data[sub_cfg_info_pos];
		gdix_dbg("load sub config, sub_cfg_id=0x%x\n", sub_cfg_id);

		sub_cfg_len = (fw_data[sub_cfg_info_pos + 1] << 8) |
					  fw_data[sub_cfg_info_pos + 2];

		if (dev->GetSensorID() == sub_cfg_id) {
			findMatchCfg = true;
			cfg = &fw_data[cfg_offset];
			cfg_ver_infile = cfg[0];
			gdix_info("Find a cfg match sensorID:ID=%d,cfg version=%d\n",
					  dev->GetSensorID(), cfg_ver_infile);
			break;
		}
		cfg_offset += sub_cfg_len;
		sub_cfg_info_pos += 3;
	}

	if (findMatchCfg) {
		// wait untill ic is free
		retry = 10;
		do {
			ret = dev->Read(CMD_ADDR, temp_buf, 1);
			if (ret < 0) {
				gdix_err("Failed read cfg cmd, ret = %d\n", ret);
				goto update_err;
			}
			if (temp_buf[0] == 0xff)
				break;
			gdix_info("0x%x value is 0x%x != 0xff, retry\n", CMD_ADDR,
					  temp_buf[0]);
			usleep(10000);
		} while (--retry);
		if (!retry) {
			gdix_err("Reg 0x%x != 0xff\n", CMD_ADDR);
			ret = -2;
			goto update_err;
		}

		// tell ic i want to send cfg
		cmd_init(temp_buf, 0x80, 0x0);
		ret = dev->Write(CMD_ADDR, temp_buf, 5);
		if (ret < 0) {
			gdix_err("Failed write send cfg cmd\n");
			goto update_err;
		}

		// wait ic to comfirm
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
			gdix_info("0x%x value is 0x%x != 0x82, retry\n", CMD_ADDR,
					  temp_buf[0]);
			usleep(30000);
		} while (--retry);

		if (!retry) {
			gdix_err("Reg 0x%x != 0x82\n", CMD_ADDR);
			ret = -2;
			goto update_err;
		}
		gdix_dbg("Wait CMD_ADDR == 0x82 success.\n");

		/* Start load config */
		ret = dev->Write(CFG_START_ADDR, &fw_data[cfg_offset], sub_cfg_len);
		if (ret < 0) {
			gdix_err("Failed write cfg to xdata, ret=%d\n", ret);
			goto update_err;
		}
		usleep(100000);

		// tell ic cfg is ready in xdata
		cmd_init(temp_buf, 0x83, 0);
		ret = dev->Write(CMD_ADDR, temp_buf, 5);
		if (ret < 0) {
			gdix_err("Failed write send cfg finish cmd\n");
			goto update_err;
		}

		// check if ic is ok with the cfg
		usleep(80000);
		retry = GDIX_RETRY_TIMES;
		do {
			ret = dev->Read(CMD_ADDR, temp_buf, 5);
			gdix_dbg("Wait CMD_ADDR == 0x7F...\n");
			if (ret < 0) {
				gdix_err("Failed read 0x%x, ret = %d\n", CMD_ADDR, ret);
			} else if (temp_buf[0] == 0x7f ||
					   (temp_buf[0] == 0x7e && temp_buf[1] == 0x00 &&
						temp_buf[2] == 0x07)) {
				break;
			}
			gdix_info("0x%x value is 0x%x, retry\n", CMD_ADDR, temp_buf[0]);
			usleep(30000);
		} while (--retry);
		gdix_info("check 0x%x value is: 0x%x,0x%x,0x%x,0x%x,0x%x.\n", CMD_ADDR,
				  temp_buf[0], temp_buf[1], temp_buf[2], temp_buf[3],
				  temp_buf[4]);

		/*set 0x7D to end send cfg */
		cmd_init(tmp_cmd_buf, 0x7D, 0);
		ret = dev->Write(CMD_ADDR, tmp_cmd_buf, 5);
		if (ret < 0) {
			gdix_err("Failed write send cfg end cmd\n");
			goto update_err;
		}

		if (!retry) {
			gdix_err("read 0x%x for config send end error\n", CMD_ADDR);
			ret = -2;
			goto update_err;
		}
		if (temp_buf[0] == 0x7e) {
			if (temp_buf[1] == 0x00 && temp_buf[2] == 0x07) {
				gdix_info("config data is equal with falsh\n");
			} else {
				gdix_err("failed send cfg\n");
				ret = -3;
				goto update_err;
			}
		} else if (temp_buf[0] != 0x7f) {
			gdix_err("failed send cfg\n");
			ret = -5;
			goto update_err;
		}

		gdix_info("send config data success!\n");
		// after update config,read curr config version
		dev->Read(CFG_START_ADDR, cfg_ver_after, 3);
		gdix_dbg("After update,cfg version is 0x%02x 0x%02x 0x%02x\n",
				 cfg_ver_after[0], cfg_ver_after[1], cfg_ver_after[2]);

		ret = 0;
	}
update_err:
	/*en report coor*/
	gdix_info("enable report coor\n");
	if (dev->Write(CMD_ADDR, buf_en_report_coor, sizeof(buf_en_report_coor)) <
		0) {
		gdix_err("Failed enable report coor in cfg update\n");
	}
	return ret;
}
