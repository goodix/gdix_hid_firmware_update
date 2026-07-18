#include "marsei_update.h"
#include "marsei.h"
#include "marsei_firmware_image.h"
#include "../gtp_util.h"

#include <unistd.h>

int MarseiUpdate::DisableReport()
{
    uint8_t tempBuf[5] = {0};
	unsigned int readChecksum = 0;
	unsigned int calChecksum = 0;    

    tempBuf[0] = 0x33;
    if (dev->WriteSpeCmd(tempBuf, 1) < 0)
        gdix_err("send disable report cmd failed\n");
    tempBuf[0] = 0x35;
    if (dev->WriteSpeCmd(tempBuf, 1) < 0)
        gdix_err("send report query cmd failed\n");

    dev->Read(0x10184, tempBuf, 4);
	readChecksum = (tempBuf[3] << 8) + tempBuf[2];
	calChecksum = tempBuf[0] + tempBuf[1];
	if (calChecksum != readChecksum) {
		gdix_err("readChecksum(0x%X) not equal to calChecksum(0x%X)\n",
					readChecksum, calChecksum);
		return -1;
	}

    return 0;
}

int MarseiUpdate::prepareUpdate()
{
	uint8_t tempBuf[5] = {0};
	uint8_t recvBuf[5] = {0};
	int retry = 3;
	int ret = -1;

    gdix_info("MarseiUpdate IN\n");

    ret = this->DisableReport();
    if (ret < 0) {
        gdix_err("disabled report failed\n");
        return ret;
    }

	/* step 1. switch mini system */
	tempBuf[0] = 0x01;
	ret = dev->SendCmd(0x10, tempBuf, 1);
	if (ret < 0) {
		gdix_err("Failed send minisystem cmd\n");
		return ret;
	}

    usleep(250000);
    this->DisableReport();

	while (retry--) {
		ret = dev->Read(0x5095, tempBuf, 1);
		if (ret == 1 && tempBuf[0] == 0xDD)
			break;
        usleep(100000);
	}
	if (retry < 0) {
		gdix_err("Failed switch minisystem ret=%d flag=0x%02x\n", ret,
				 tempBuf[0]);
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
		ret = dev->Write(0x49800, tempBuf, 5);
		if (ret < 0) {
			gdix_err("Failed write sram, ret=%d\n", ret);
			return ret;
		}
		ret = dev->Read(0x49800, recvBuf, 5);
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

int MarseiUpdate::flashSubSystem(struct fw_subsys_info *subsys)
{
	uint32_t data_size = 0;
	uint32_t offset = 0;
	uint32_t temp_addr = subsys->flash_addr;
	uint32_t total_size = subsys->size;
	uint32_t checksum;
	uint8_t cmdBuf[10] = {0};
	uint8_t flag;
	int resend_rty;
	int retry;
	uint32_t i;
	int ret;

	while (total_size > 0) {
		data_size = total_size > 4096 ? 4096 : total_size;
        resend_rty = 3;
        while (resend_rty--) {
            /* send fw data to dram */
            ret = dev->Write(0x49800, &subsys->data[offset], data_size);
            if (ret < 0) {
                gdix_err("Write fw data failed\n");
                return ret;
            }

            usleep(20000);

            /* send checksum */
            for (i = 0, checksum = 0; i < data_size; i += 2) {
                checksum +=
                    subsys->data[offset + i] + (subsys->data[offset + i + 1] << 8);
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
                usleep(100000);
                ret = dev->Read(0x5096, &flag, 1);
                if (ret == 1 && flag == 0xAA) {
                    break;
                } else if (ret == 1 && flag == 0xBB) { //checksum error
                    gdix_err("checksum error of the data received by host\n");
                    retry = -1;
                    break;
                } else if (ret == 1 && flag == 0xCC) {
                    gdix_err("checksum error of the data read back from flash\n");
                    retry = -1;
                    break;
                }
            }
            if (retry >= 0)
                break;
            gdix_err("Failed get valid ack, addr:0x%04x ret=%d flag=0x%02x rty:%d\n",
                    temp_addr, ret, flag, 3 - resend_rty);
        }
        if (resend_rty < 0) {
            gdix_err("Flash package failed, exit\n");
            return -EINVAL;
        }

		gdix_info("Flash package ok, addr:0x%06x\n", temp_addr);
		offset += data_size;
		temp_addr += data_size;
		total_size -= data_size;
	}

	return 0;
}

#define CFG_MAX_SIZE 4096
int MarseiUpdate::fw_update(unsigned int firmware_flag)
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
	uint8_t temp_buf[CFG_MAX_SIZE] = {0};

	gdix_info("IN\n");
	/* flash config */
	if (image->HasConfig()) {
		memcpy(temp_buf,
			   image->GetFirmwareData() + image->GetFirmwareSize() + 64,
			   image->GetConfigSize());

		subsys_cfg.data = temp_buf;
		subsys_cfg.size = CFG_MAX_SIZE;
		subsys_cfg.flash_addr = 0x3F000;
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
		if (fw_x->type == (uint8_t)firmware_flag) {
			gdix_info("skip type[%02X] subsystem[%d]\n", fw_x->type, i);
			continue;
		}
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
	usleep(200000);

	/* compare version */
	dev->SetBasicProperties();
	majorVer = image->GetFirmwareVersionMajor();
	minorVer = image->GetFirmwareVersionMinor();
	if (image->HasConfig() == false) {
		cfgVer = (uint8_t)dev->GetFirmwareVersionMinor();
		minorVer &= 0xffffff00;
		minorVer |= cfgVer;
	}

	if ((dev->GetFirmwareVersionMajor() != majorVer) ||
		(dev->GetFirmwareVersionMinor() != minorVer)) {
		gdix_err("update failed\n");
		gdix_info("Current version:%d.%d\n", dev->GetFirmwareVersionMajor(),
				  dev->GetFirmwareVersionMinor());
		gdix_info("Firmware version:%d.%d\n", majorVer, minorVer);
		return -1;
	}

	return 0;
}