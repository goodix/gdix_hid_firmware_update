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
#include "gtx9_update.h"
#include "gtx9.h"

int GTx9Update::Run(void *para)
{
    int ret;

    if(!m_Initialized){
        gdix_err("Can't Process Update Before Initialized.\n");
        return -EINVAL;
    }
    //get all parameter
    pGTUpdatePara parameter = (pGTUpdatePara)para;

    if(!parameter->force) {
        ret = check_update();
        if (ret < 0) {
            gdix_info("Upgrades are not allowed\n");
            return ret;
        } else if (ret == 0) {
            gdix_info("Skip upgrade\n");
            return 0;
        }
        gdix_info("Ready to upgrade\n");
    } else {
        gdix_info("Force to upgrade\n");
    }

    ret = prepareUpdate();
    if (ret < 0) {
        gdix_err("Failed prepare update\n");
        return ret;
    }

    ret = fw_update(parameter->firmwareFlag);
    if (ret < 0) {
        gdix_err("Failed update\n");
        return ret;
    }

    gdix_info("Success update\n");
    
    return 0;
}

int GTx9Update::check_update()
{
    int ret;

    ret = memcmp(dev->GetProductID(), image->GetProductID(), 8);
    if (ret != 0) {
        gdix_info("Product ID mismatch %s != %s\n",
            dev->GetProductID(), image->GetProductID());
        return -1;
    }

    if ((dev->GetFirmwareVersionMajor() != image->GetFirmwareVersionMajor()) ||
        (dev->GetFirmwareVersionMinor() != image->GetFirmwareVersionMinor())) {
        gdix_info("Current version:%d.%d\n", dev->GetFirmwareVersionMajor(), dev->GetFirmwareVersionMinor());
        gdix_info("Firmware version:%d.%d\n", image->GetFirmwareVersionMajor(), image->GetFirmwareVersionMinor());
        return 1;
    }

    gdix_info("No need to upgrade\n");
    return 0;
}

int GTx9Update::prepareUpdate()
{
    uint8_t tempBuf[5] = {0};
    uint8_t recvBuf[5] = {0};
    int retry = 3;
    int ret = -1;

    gdix_info("IN\n");

    /* step 1. switch mini system */
    tempBuf[0] = 0x01;
    ret = dev->SendCmd(0x10, tempBuf, 1);
    if (ret < 0) {
        gdix_err("Failed send minisystem cmd\n");
        return ret;
    }
    while (retry--) {
        usleep(200000);
        ret = dev->Read(0x10010, tempBuf, 1);
        if (ret == 1 && tempBuf[0] == 0xDD)
            break;
    }
    if (retry < 0) {
        gdix_err("Failed switch minisystem ret=%d flag=0x%02x\n",
            ret, tempBuf[0]);
        return -EINVAL;
    }
    gdix_info("Switch mini system successfully\n");

    /* step 2. erase flash */
    tempBuf[0] = 0x01;
    ret = dev->SendCmd(0x11, tempBuf, 1);
    if (ret < 0) {
        gdix_err("Failed send erase flash cmd\n");
        return ret;
    }
  
    retry = 10;
    memset(tempBuf, 0x55, 5);
    while (retry--) {
        usleep(10000);
        ret = dev->Write(0x14000, tempBuf, 5);
        if (ret < 0) {
            gdix_err("Failed write sram, ret=%d\n", ret);
            return ret;
        }
        ret = dev->Read(0x14000, recvBuf, 5);
        if (!memcmp(tempBuf, recvBuf, 5))
            break;
    }
    if (retry < 0) {
        gdix_err("Read back failed, ret=%d buf:%02x %02x %02x %02x %02x\n", ret,
            recvBuf[0], recvBuf[1], recvBuf[2], recvBuf[3], recvBuf[4]);
        return -EINVAL;
    }

    gdix_info("Updata prepare OK\n");
    return 0;
}

int GTx9Update::flashSubSystem(struct fw_subsys_info *subsys)
{
    uint32_t data_size = 0;
    uint32_t offset = 0;
    uint32_t temp_addr = subsys->flash_addr;
    uint32_t total_size = subsys->size;
    uint32_t checksum;
    uint8_t cmdBuf[10] = {0};
    uint8_t flag;
    int retry;
    uint32_t i;
    int ret;

    while (total_size > 0) {
		data_size = total_size > 4096 ? 4096 : total_size;

        /* send fw data to dram */
        ret = dev->Write(0x14000, &subsys->data[offset], data_size);
        if (ret < 0) {
            gdix_err("Write fw data failed\n");
            return ret;
        }

        /* send checksum */
        for (i = 0, checksum = 0; i < data_size; i += 2) {
            checksum += subsys->data[offset + i] +
                    (subsys->data[offset + i + 1] << 8);
        }

        cmdBuf[0] = (data_size >> 8) & 0xFF;
        cmdBuf[1] = data_size & 0xFF;
        cmdBuf[2] = (temp_addr >> 24) & 0xFF;
        cmdBuf[3] = (temp_addr >> 16) & 0xFF;
        cmdBuf[4] = (temp_addr >> 8) & 0xFF;
        cmdBuf[5] = temp_addr & 0xFF;
        cmdBuf[6] = (checksum >> 24) & 0xFF;
        cmdBuf[7] = (checksum >> 16) & 0xFF;
        cmdBuf[8] = (checksum >> 8) & 0xFF;
        cmdBuf[9] = checksum & 0xFF;
        ret = dev->SendCmd(0x12, cmdBuf, 10);
        if (ret < 0) {
            gdix_err("Failed send start update cmd\n");
            return ret;
        }

        /* wait update finish */
        retry = 10;
        while (retry--) {
            usleep(20000);
            ret = dev->Read(0x10011, &flag, 1);
            if (ret == 1 && flag == 0xAA)
                break;
        }
        if (retry < 0) {
            gdix_err("Failed get valid ack, ret=%d flag=0x%02x\n", ret, flag);
            return -EINVAL;
        }

        gdix_info("Flash package ok, addr:0x%06x\n", temp_addr);

        offset += data_size;
        temp_addr += data_size;
        total_size -= data_size;
    }

    return 0;
}

int GTx9Update::fw_update(unsigned int firmware_flag)
{
    struct firmware_summary *fw_info =
        (struct firmware_summary *)image->GetFirmwareSummary();
    struct fw_subsys_info *fw_x;
    struct fw_subsys_info subsys_cfg;
    int i;
    int ret;
    uint8_t buf[1];
    int minorVer;
    int majorVer;
    uint8_t cfgVer;

    gdix_info("IN\n");
    /* flash config */
    if (image->HasConfig()) {
        subsys_cfg.data = image->GetFirmwareData() + image->GetFirmwareSize() + 64;
        subsys_cfg.size = image->GetConfigSize();
        subsys_cfg.flash_addr = 0x40000;
        subsys_cfg.type = 4;
        ret = flashSubSystem(&subsys_cfg);
        if (ret < 0) {
            gdix_err("failed flash config with ISP\n");
            return ret;
        }
        gdix_info("success flash config with ISP\n");
        usleep(20000);
    }

    for (i = 1; i < fw_info->subsys_num; i++) {
        fw_x = &fw_info->subsys[i];
        ret = flashSubSystem(fw_x);
        if (ret < 0) {
            gdix_err("-------- Failed flash subsystem %d --------\n", i);
            return ret;
        }
        gdix_info("-------- Success flash subsystem %d --------\n", i);
    }

    /* reset IC */
    gdix_info("Reset IC\n");
    buf[0] = 1;
    ret = dev->SendCmd(0x13, buf, 1);
    if (ret < 0) {
        gdix_err("Failed reset IC\n");
        return ret;
    }
    usleep(100000);

    /* compare version */
    dev->SetBasicProperties();
    majorVer = image->GetFirmwareVersionMajor();
    minorVer = image->GetFirmwareVersionMinor();
    if (image->HasConfig() == false) {
        cfgVer = (uint8_t)dev->GetFirmwareVersionMinor();
        minorVer &= 0xffffff00;
        minorVer |= cfgVer;
    }

    if ((dev->GetFirmwareVersionMajor() != majorVer) || (dev->GetFirmwareVersionMinor() != minorVer)) {
        gdix_err("update failed\n");
        gdix_info("Current version:%d.%d\n", dev->GetFirmwareVersionMajor(), dev->GetFirmwareVersionMinor());
        gdix_info("Firmware version:%d.%d\n", majorVer, minorVer);
        return -1;
    }

    return 0;
}
