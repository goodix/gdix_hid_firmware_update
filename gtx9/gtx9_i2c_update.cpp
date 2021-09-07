#include <linux/i2c-dev.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>   /*for type of bool*/
#include <unistd.h>    /*system calls, read, write, close...*/
#include <string.h>
#include <time.h>      /*for nanosleep()*/
#include <fcntl.h>     /*O_RDONLY, O_RDWR etc...*/
#include <libgen.h>    /*for readlink()*/
#include <errno.h>
#include <sys/stat.h>  /*for struct stat*/
#include <sys/types.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include "../gtp_util.h"


#define CLIENT_ADDR             	0x5D
#define TS_ADDR_LENGTH          	4
#define I2C_MAX_TRANSFER_SIZE   	256
#define GOODIX_BUS_RETRY_TIMES  	1

enum CHECKSUM_MODE {
	CHECKSUM_MODE_U8_LE,
	CHECKSUM_MODE_U16_LE,
};

uint16_t g_client_addr = CLIENT_ADDR;
int g_fd;

struct fw_subsys_info {
	uint8_t type;
	uint32_t size;
	uint32_t flash_addr;
	uint8_t *data;
};

#pragma pack(1)
struct goodix_fw_version {
	uint8_t rom_pid[6];               /* rom PID */
	uint8_t rom_vid[3];               /* Mask VID */
	uint8_t rom_vid_reserved;
	uint8_t patch_pid[8];              /* Patch PID */
	uint8_t patch_vid[4];              /* Patch VID */
	uint8_t patch_vid_reserved;
	uint8_t sensor_id;
	uint8_t reserved[2];
	uint16_t checksum;
};

struct  firmware_summary {
	int size;
	uint32_t checksum;
	uint8_t hw_pid[6];
	uint8_t hw_vid[3];
	uint8_t fw_pid[8];
	uint8_t fw_vid[4];
	uint8_t subsys_num;
	uint8_t chip_type;
	uint8_t protocol_ver;
	uint8_t bus_type;
	uint8_t flash_protect;
	// uint8_t reserved[8];
	struct fw_subsys_info subsys[47];
};
#pragma pack()

struct firmware {
	int size;
	uint8_t data[256 * 1024];
} g_firmware;

struct goodix_ic_config {
	int len;
	uint8_t data[4096];
} g_config;

struct firmware_data {
	struct  firmware_summary fw_summary;
	struct firmware *firmware;
};

#pragma pack(1)
struct goodix_flash_cmd {
	union {
		struct {
			uint8_t status;
			uint8_t ack;
			uint8_t len;
			uint8_t cmd;
			uint8_t fw_type;
			uint16_t fw_len;
			uint32_t fw_addr;
			//uint16_t checksum;
		};
		uint8_t buf[16];
	};
};
#pragma pack()

struct fw_update_ctrl {
	struct firmware_data fw_data;
	struct goodix_ic_config *ic_config;
};
static struct fw_update_ctrl goodix_fw_update_ctrl;

int checksum_cmp(const uint8_t *data, int size, int mode)
{
	uint32_t cal_checksum = 0;
	uint32_t r_checksum = 0;
	int i;

	if (mode == CHECKSUM_MODE_U8_LE) {
		if (size < 2)
			return 1;
		for (i = 0; i < size - 2; i++)
			cal_checksum += data[i];

		r_checksum = data[size - 2] + (data[size - 1] << 8);
		return (cal_checksum & 0xFFFF) == r_checksum ? 0 : 1;
	}

	if (size < 4)
		return 1;
	for (i = 0; i < size - 4; i += 2)
		cal_checksum += data[i] + (data[i + 1] << 8);
	r_checksum = data[size - 4] + (data[size - 3] << 8) +
		(data[size - 2] << 16) + (data[size - 1] << 24);
	return cal_checksum == r_checksum ? 0 : 1;
}

uint32_t gdix_append_checksum(uint8_t *data, int len, int mode)
{
	uint32_t checksum = 0;
	int i;

	checksum = 0;
	if (mode == CHECKSUM_MODE_U8_LE) {
		for (i = 0; i < len; i++)
			checksum += data[i];
	} else {
		for (i = 0; i < len; i+=2)
			checksum += (data[i] + (data[i+1] << 8));
	}

	if (mode == CHECKSUM_MODE_U8_LE) {
		data[len] = checksum & 0xff;
		data[len + 1] = (checksum >> 8) & 0xff;
		return 0xFFFF & checksum;
	}
	data[len] = checksum & 0xff;
	data[len + 1] = (checksum >> 8) & 0xff;
	data[len + 2] = (checksum >> 16) & 0xff;
	data[len + 3] = (checksum >> 24) & 0xff;
	return checksum;
}

static int i2c_read(uint32_t reg, unsigned char *data, int len)
{
    struct i2c_rdwr_ioctl_data packets;
	int transfer_length = 0;
	int pos = 0, address = reg;
	unsigned char get_buf[128], addr_buf[TS_ADDR_LENGTH];
	int retry, r = 0;

	struct i2c_msg msgs[] = {
		{
			.addr = g_client_addr,
			.flags = !I2C_M_RD,
			.len = TS_ADDR_LENGTH,
			.buf = addr_buf,
		},
		{
			.addr = g_client_addr,
			.flags = I2C_M_RD,
		},
	};

	if (len < (int)sizeof(get_buf)) {
		/* code optimize, use stack memory */
		msgs[1].buf = &get_buf[0];
	} else {
		msgs[1].buf = (uint8_t *)malloc(len);
		if (msgs[1].buf == NULL) {
			gdix_dbg("Malloc failed\n");
			return -1;
		}
	}
	while (pos != len) {
		if (len - pos > I2C_MAX_TRANSFER_SIZE)
			transfer_length = I2C_MAX_TRANSFER_SIZE;
		else
			transfer_length = len - pos;
		msgs[0].buf[0] = (address >> 24) & 0xFF;
		msgs[0].buf[1] = (address >> 16) & 0xFF;
		msgs[0].buf[2] = (address >> 8) & 0xFF;
		msgs[0].buf[3] = address & 0xFF;
		msgs[1].len = transfer_length;

		packets.msgs = msgs;
		packets.nmsgs = 2;
		for (retry = 0; retry < GOODIX_BUS_RETRY_TIMES; retry++) {
			if (ioctl(g_fd, I2C_RDWR, &packets) > 0) {
				memcpy(&data[pos], msgs[1].buf, transfer_length);
				pos += transfer_length;
				address += transfer_length;
				break;
			}
			gdix_err("I2c read retry[%d]:0x%x\n", retry + 1, reg);
		}
		if (retry == GOODIX_BUS_RETRY_TIMES) {
			gdix_err("I2c read failed\n");
			r = -1;
			goto read_exit;
		}
	}

read_exit:
	if (len >= (int)sizeof(get_buf))
		free(msgs[1].buf);
	return r < 0 ? r : 0;
}

static int i2c_write(uint32_t reg, unsigned char *data, int len)
{
	struct i2c_rdwr_ioctl_data packets;
	int pos = 0, transfer_length = 0;
	int address = reg;
	unsigned char put_buf[128];
	int retry, r = 0;
	struct i2c_msg msgs = {
		.addr = g_client_addr,
		.flags = !I2C_M_RD,
	};

	if (len + TS_ADDR_LENGTH < (int)sizeof(put_buf)) {
		/* code optimize,use stack memory*/
		msgs.buf = &put_buf[0];
	} else {
		msgs.buf = (uint8_t *)malloc(len + TS_ADDR_LENGTH);
		if (msgs.buf == NULL) {
			gdix_err("Malloc failed\n");
			return -1;
		}
	}
	while (pos != len) {
		if (len - pos > I2C_MAX_TRANSFER_SIZE - TS_ADDR_LENGTH)
			transfer_length = I2C_MAX_TRANSFER_SIZE - TS_ADDR_LENGTH;
		else
			transfer_length = len - pos;
		msgs.buf[0] = (address >> 24) & 0xFF;
		msgs.buf[1] = (address >> 16) & 0xFF;
		msgs.buf[2] = (address >> 8) & 0xFF;
		msgs.buf[3] = address & 0xFF;
		msgs.len = transfer_length + TS_ADDR_LENGTH;
		memcpy(&msgs.buf[TS_ADDR_LENGTH], &data[pos], transfer_length);
		packets.msgs = &msgs;
		packets.nmsgs = 1;

		for (retry = 0; retry < GOODIX_BUS_RETRY_TIMES; retry++) {
			if (ioctl(g_fd, I2C_RDWR, &packets) > 0) {
				pos += transfer_length;
				address += transfer_length;
				break;
			}
			gdix_err("I2c write retry[%d]\n", retry + 1);
		}
		if (retry == GOODIX_BUS_RETRY_TIMES) {
			gdix_err("I2c write failed\n");
			r = -1;
			goto write_exit;
		}
	}

write_exit:
	if (len + TS_ADDR_LENGTH >= (int)sizeof(put_buf))
		free(msgs.buf);
	return r < 0 ? r : 0;	
}

static void gdix_soft_reset(int delay)
{
	uint8_t val = 0;

	i2c_write(0xD808, &val, 1);
	if (delay > 0)
		usleep(delay * 1000);
}

static int switch_i2c_mode(void)
{
	uint32_t addr = 0x3030aabb;
    uint8_t data = 0xCC;
    int ret, i, retry = 5;

    for (i = 0; i < retry; i++) {
        ret = i2c_write(addr, &data, 1);
        if (ret == 0) {
			gdix_dbg("switch i2c mode success\n");
			usleep(50000);
			return 0;
        }
    }

	gdix_err("switch i2c mode failed\n");
    return -1;
}

static int gdix_read_firmware(const char *filename)
{
	int ret;
	int fw_fd;
	int totalSize;
	int fwSize;
	int cfgSize;
	uint8_t *firmwareData;

	fw_fd = open(filename, O_RDONLY);
	if (fw_fd < 0){
		gdix_err("file:%s, ret:%d\n", filename, fw_fd);
		return fw_fd;
	}

	totalSize = lseek(fw_fd, 0, SEEK_END);
	lseek(fw_fd, 0, SEEK_SET);

    firmwareData = new unsigned char[totalSize]();
	ret = read(fw_fd, firmwareData, totalSize);
	if (ret != totalSize) {
		gdix_err("Failed read file: %s, ret=%d\n", filename, ret);
		ret = -1;
		goto err_out;
	}

	fwSize = ((firmwareData[3] << 24) | (firmwareData[2] << 16) |
			(firmwareData[1] << 8) | firmwareData[0]) + 8;

	g_firmware.size = fwSize;
	memcpy(g_firmware.data, firmwareData, fwSize);
	goodix_fw_update_ctrl.fw_data.firmware = &g_firmware;

	if (fwSize < totalSize) {
		gdix_dbg("Check firmware size:%d < file size:%d\n",
			fwSize, totalSize);
		gdix_dbg("This bin file may contain a config bin.\n");
        cfgSize = totalSize - fwSize - 64;
        gdix_dbg("config size:%d\n", cfgSize);
		g_config.len = cfgSize;
		memcpy(g_config.data, firmwareData + fwSize + 64, cfgSize);
		goodix_fw_update_ctrl.ic_config = &g_config;
	}

	gdix_dbg("read firmware success\n");
	close(fw_fd);
	return 0;
err_out:
	delete[] firmwareData;
	close(fw_fd);
	return ret;	
}

static int gdix_parse_firmware(struct firmware_data *fw_data)
{
	struct firmware *firmware;
	struct  firmware_summary *fw_summary;
	int i, fw_offset, info_offset;
	int r = 0;
	uint32_t checksum;

	fw_summary = &fw_data->fw_summary;

	/* copy firmware head info */
	firmware = fw_data->firmware;
	if (firmware->size < 42) {
		gdix_err("Invalid firmware size:%d\n", firmware->size);
		r = -EINVAL;
		goto err_size;
	}
	memcpy(fw_summary, firmware->data, sizeof(*fw_summary));

	/* check firmware size */
	if (firmware->size != fw_summary->size + 8) {
		gdix_err("Bad firmware, size not match, %d != %d\n",
			firmware->size, fw_summary->size + 8);
		r = -EINVAL;
		goto err_size;
	}

	for (i = 8, checksum = 0; i < firmware->size; i += 2)
		checksum += firmware->data[i] + (firmware->data[i+1] << 8);

	/* byte order change, and check */
	if (checksum != fw_summary->checksum) {
		gdix_err("Bad firmware, cheksum error\n");
		r = -EINVAL;
		goto err_size;
	}

	if (fw_summary->subsys_num > 47) {
		gdix_err("Bad firmware, invalid subsys num: %d\n",
		       fw_summary->subsys_num);
		r = -EINVAL;
		goto err_size;
	}

	/* parse subsystem info */
	fw_offset = 512;
	for (i = 0; i < fw_summary->subsys_num; i++) {
		info_offset = 42 + i * 10;
		fw_summary->subsys[i].type = firmware->data[info_offset];
		fw_summary->subsys[i].size = *(uint32_t *)&firmware->data[info_offset + 1];
		fw_summary->subsys[i].flash_addr = *(uint32_t *)&firmware->data[info_offset + 5];
		if (fw_offset > firmware->size) {
			gdix_err("Sybsys offset exceed Firmware size\n");
			r = -EINVAL;
			goto err_size;
		}

		fw_summary->subsys[i].data = firmware->data + fw_offset;
		fw_offset += fw_summary->subsys[i].size;
	}

	gdix_dbg("Firmware package protocol: V%u\n", fw_summary->protocol_ver);
	gdix_dbg("Firmware PID:GT%s\n", fw_summary->fw_pid);
	gdix_dbg("Firmware VID:%*ph\n", 4, fw_summary->fw_vid);
	gdix_dbg("Firmware chip type:0x%02X\n", fw_summary->chip_type);
	gdix_dbg("Firmware bus type:%s\n",
		(fw_summary->bus_type & 1) ? "SPI" : "I2C");
	gdix_dbg("Firmware size:%u\n", fw_summary->size);
	gdix_dbg("Firmware subsystem num:%u\n", fw_summary->subsys_num);

/*
	for (i = 0; i < fw_summary->subsys_num; i++) {
		gdix_dbg("------------------------------------------\n");
		gdix_dbg("Index:%d\n", i);
		gdix_dbg("Subsystem type:%02X\n", fw_summary->subsys[i].type);
		gdix_dbg("Subsystem size:%u\n", fw_summary->subsys[i].size);
		gdix_dbg("Subsystem flash_addr:%08X\n",
				fw_summary->subsys[i].flash_addr);
	}
*/

err_size:
	return r;
}

static int gdix_read_version(struct goodix_fw_version *version)
{
	int ret;
	uint8_t temp_pid[8] = {0};
	uint8_t buf[sizeof(struct goodix_fw_version)] = {0};

	ret = i2c_read(0x10014, buf, sizeof(buf));
	if (ret < 0) {
		gdix_err("read version failed\n");
		return ret;
	}

	if (checksum_cmp(buf, sizeof(buf), CHECKSUM_MODE_U8_LE)) {
		gdix_err("invalid fw version, checksum err\n");
		return -1;
	}

	memcpy(version, buf, sizeof(*version));
	memcpy(temp_pid, version->rom_pid, sizeof(version->rom_pid));
	gdix_dbg("rom_pid:%s\n", temp_pid);
	gdix_dbg("rom_vid:%*ph\n", (int)sizeof(version->rom_vid),
		version->rom_vid);
	gdix_dbg("pid:%s\n", version->patch_pid);
	gdix_dbg("vid:%*ph\n", (int)sizeof(version->patch_vid),
		version->patch_vid);
	gdix_dbg("sensor_id:%d\n", version->sensor_id);	

	return 0;
}

static int gdix_load_isp(struct firmware_data *fw_data)
{
	struct goodix_fw_version isp_fw_version;
	struct fw_subsys_info *fw_isp;
	uint8_t reg_val[8] = {0x00};
	int r;

	fw_isp = &fw_data->fw_summary.subsys[0];

	gdix_dbg("Loading ISP start\n");
	r = i2c_write(0x57000, fw_isp->data, fw_isp->size);
	if (r < 0) {
		gdix_err("Loading ISP error\n");
		return r;
	}

	gdix_dbg("Success send ISP data\n");

	/* SET BOOT OPTION TO 0X55 */
	memset(reg_val, 0x55, 8);
	r = i2c_write(0x10000, reg_val, 8);
	if (r < 0) {
		gdix_err("Failed set REG_CPU_RUN_FROM flag\n");
		return r;
	}
	gdix_dbg("Success write [8]0x55 to 0x%x\n", 0x10000);

	gdix_soft_reset(100);
	/*check isp state */
	if (gdix_read_version(&isp_fw_version)) {
		gdix_err("failed read isp version\n");
		return -2;
	}
	if (memcmp(&isp_fw_version.patch_pid[3], "ISP", 3)) {
		gdix_err("patch id error %c%c%c != %s\n",
		isp_fw_version.patch_pid[3], isp_fw_version.patch_pid[4],
		isp_fw_version.patch_pid[5], "ISP");
		return -3;
	}
	gdix_dbg("ISP running successfully\n");
	return 0;
}

static int gdix_update_prepare(struct fw_update_ctrl *fwu_ctrl)
{
	uint8_t reg_val[4] = {0};
	uint8_t temp_buf[64] = {0};
	int retry = 20;
	int r;

	/* reset IC */
	gdix_dbg("firmware update, reset\n");
	gdix_soft_reset(5);

	retry = 100;
	/* Hold cpu*/
	do {
		reg_val[0] = 0x01;
		reg_val[1] = 0x00;
		r = i2c_write(0x0002, reg_val, 2);
		r |= i2c_read(0x2000, &temp_buf[0], 4);
		r |= i2c_read(0x2000, &temp_buf[4], 4);
		r |= i2c_read(0x2000, &temp_buf[8], 4);
		if (!r && !memcmp(&temp_buf[0], &temp_buf[4], 4) &&
			!memcmp(&temp_buf[4], &temp_buf[8], 4) &&
			!memcmp(&temp_buf[0], &temp_buf[8], 4)) {
			break;
		}
		usleep(1000);
		gdix_dbg("retry hold cpu %d\n", retry);
	} while (--retry);
	if (!retry) {
		gdix_err("Failed to hold CPU, return = %d\n", r);
		return -1;
	}
	gdix_dbg("Success hold CPU\n");

	/* enable misctl clock */
	reg_val[0] = 0x40;
	i2c_write(0xD80B, reg_val, 1);
	gdix_dbg("enbale misctl clock\n");

	/* disable watch dog */
	reg_val[0] = 0x00;
	r = i2c_write(0xD054, reg_val, 1);
	gdix_dbg("disable watch dog\n");

	/* load ISP code and run form isp */
	r = gdix_load_isp(&fwu_ctrl->fw_data);
	if (r < 0)
		gdix_err("Failed load and run isp\n");

	return r;
}

#define FLASH_CMD_TYPE_READ				0xAA
#define FLASH_CMD_TYPE_WRITE			0xBB
#define FLASH_CMD_ACK_CHK_PASS			0xEE
#define FLASH_CMD_ACK_CHK_ERROR			0x33
#define FLASH_CMD_ACK_IDLE				0x11
#define FLASH_CMD_W_STATUS_CHK_PASS		0x22
#define FLASH_CMD_W_STATUS_CHK_FAIL		0x33
#define FLASH_CMD_W_STATUS_ADDR_ERR		0x44
#define FLASH_CMD_W_STATUS_WRITE_ERR	0x55
#define FLASH_CMD_W_STATUS_WRITE_OK		0xEE
static int gdix_send_flash_cmd(struct goodix_flash_cmd *flash_cmd)
{
	int i, ret, retry;
	struct goodix_flash_cmd tmp_cmd;

	gdix_dbg("try send flash cmd:%*ph\n", (int)sizeof(flash_cmd->buf), flash_cmd->buf);
	memset(tmp_cmd.buf, 0, sizeof(tmp_cmd));
	ret = i2c_write(0x13400, flash_cmd->buf, sizeof(flash_cmd->buf));
	if (ret) {
		gdix_err("failed send flash cmd %d\n", ret);
		return ret;
	}

	retry = 5;
	for (i = 0; i < retry; i++) {
		ret = i2c_read(0x13400, tmp_cmd.buf, sizeof(tmp_cmd.buf));
		if (!ret && tmp_cmd.ack == FLASH_CMD_ACK_CHK_PASS)
			break;
		usleep(5000);
		gdix_dbg("flash cmd ack error retry %d, ack 0x%x, ret %d\n",
			i, tmp_cmd.ack, ret);
	}
	if (tmp_cmd.ack != FLASH_CMD_ACK_CHK_PASS) {
		gdix_err("flash cmd ack error, ack 0x%x, ret %d\n",
			tmp_cmd.ack, ret);
		gdix_err("data:%*ph\n", (int)sizeof(tmp_cmd.buf), tmp_cmd.buf);
		return -1;
	}
	gdix_dbg("flash cmd ack check pass\n");

	usleep(80000);
	retry = 20;
	for (i = 0; i < retry; i++) {
		ret = i2c_read(0x13400, tmp_cmd.buf, sizeof(tmp_cmd.buf));
		if (!ret && tmp_cmd.ack == FLASH_CMD_ACK_CHK_PASS &&
			tmp_cmd.status == FLASH_CMD_W_STATUS_WRITE_OK) {
			gdix_dbg("flash status check pass\n");
			return 0;
		}

		gdix_dbg("flash cmd status not ready, retry %d, ack 0x%x, status 0x%x, ret %d\n",
			i, tmp_cmd.ack, tmp_cmd.status, ret);
		usleep(20000);
	}

	gdix_err("flash cmd status error %d, ack 0x%x, status 0x%x, ret %d\n",
		i, tmp_cmd.ack, tmp_cmd.status, ret);
	if (ret) {
		gdix_err("reason: bus or paltform error\n");
		return -1;
	}

	switch (tmp_cmd.status) {
	case FLASH_CMD_W_STATUS_CHK_PASS:
		gdix_err("data check pass, but failed get follow-up results\n");
		return -1;
	case FLASH_CMD_W_STATUS_CHK_FAIL:
		gdix_err("data check failed, please retry\n");
		return -1;
	case FLASH_CMD_W_STATUS_ADDR_ERR:
		gdix_err("flash target addr error, please check\n");
		return -1;
	case FLASH_CMD_W_STATUS_WRITE_ERR:
		gdix_err("flash data write err, please retry\n");
		return -1;
	default:
		gdix_err("unknown status\n");
		return -1;
	}
}

static int gdix_flash_package(uint8_t subsys_type, uint8_t *pkg,
	uint32_t flash_addr, uint16_t pkg_len)
{
	int ret, retry;
	struct goodix_flash_cmd flash_cmd;

	retry = 2;
	do {
		ret = i2c_write(0x13410, pkg, pkg_len);
		if (ret < 0) {
			gdix_err("Failed to write firmware packet\n");
			return ret;
		}

		flash_cmd.status = 0;
		flash_cmd.ack = 0;
		flash_cmd.len = 11;
		flash_cmd.cmd = 0xBB;
		flash_cmd.fw_type = subsys_type;
		flash_cmd.fw_len = pkg_len;
		flash_cmd.fw_addr = flash_addr;

		gdix_append_checksum(&(flash_cmd.buf[2]),
				9, CHECKSUM_MODE_U8_LE);

		ret = gdix_send_flash_cmd(&flash_cmd);
		if (!ret) {
			gdix_dbg("success write package to 0x%x, len %d\n",
				flash_addr, pkg_len - 4);
			return 0;
		}
	} while (ret < 0 && --retry);

	return ret;
}

#define ISP_MAX_BUFFERSIZE	4096
static int gdix_flash_subsystem(struct fw_subsys_info *subsys)
{
	uint32_t data_size, offset;
	uint32_t total_size;
	//TODO: confirm flash addr ,<< 8??
	uint32_t subsys_base_addr = subsys->flash_addr;
	uint8_t *fw_packet = NULL;
	int r = 0;

	/*
	 * if bus(i2c/spi) error occued, then exit, we will do
	 * hardware reset and re-prepare ISP and then retry
	 * flashing
	 */
	total_size = subsys->size;
	fw_packet = (uint8_t *)malloc(ISP_MAX_BUFFERSIZE + 4);
	if (!fw_packet) {
		gdix_dbg("Failed alloc memory\n");
		return -1;
	}

	offset = 0;
	while (total_size > 0) {
		data_size = total_size > ISP_MAX_BUFFERSIZE ?
				ISP_MAX_BUFFERSIZE : total_size;
		gdix_dbg("Flash firmware to %08x,size:%u bytes\n",
			subsys_base_addr + offset, data_size);

		memcpy(fw_packet, &subsys->data[offset], data_size);
		/* set checksum for package data */
		gdix_append_checksum(fw_packet, data_size, CHECKSUM_MODE_U16_LE);

		r = gdix_flash_package(subsys->type, fw_packet,
				subsys_base_addr + offset, data_size + 4);
		if (r) {
			gdix_err("failed flash to %08x,size:%u bytes\n",
			subsys_base_addr + offset, data_size);
			break;
		}
		offset += data_size;
		total_size -= data_size;
	} /* end while */

	free(fw_packet);
	return r;
}

static int gdix_flash_firmware(struct fw_update_ctrl *fw_ctrl)
{
	struct firmware_data *fw_data = &fw_ctrl->fw_data;
	struct  firmware_summary  *fw_summary;
	struct fw_subsys_info *fw_x;
	struct fw_subsys_info subsys_cfg = {0};
	int retry = 3;
	int i, r = 0, fw_num;

	/*	start from subsystem 1,
	 *	subsystem 0 is the ISP program
	 */
	fw_summary = &fw_data->fw_summary;
	fw_num = fw_summary->subsys_num;

	/* flash config data first if we have */
	if (fw_ctrl->ic_config && fw_ctrl->ic_config->len) {
		subsys_cfg.data = fw_ctrl->ic_config->data;
		subsys_cfg.size = fw_ctrl->ic_config->len;
		subsys_cfg.flash_addr = 0x40000;
		subsys_cfg.type = 4;
		r = gdix_flash_subsystem(&subsys_cfg);
		if (r) {
			gdix_err("failed flash config with ISP, %d\n", r);
			return r;
		}
		gdix_dbg("success flash config with ISP\n");
	}

	for (i = 1; i < fw_num && retry;) {
		gdix_dbg("--- Start to flash subsystem[%d] ---\n", i);
		fw_x = &fw_summary->subsys[i];
		r = gdix_flash_subsystem(fw_x);
		if (r == 0) {
			gdix_dbg("--- End flash subsystem[%d]: OK ---\n", i);
			i++;
		} else if (r == -EAGAIN) {
			retry--;
			gdix_err("--- End flash subsystem%d: Fail, errno:%d, retry:%d ---\n",
				i, r, 3 - retry);
		} else if (r < 0) { /* bus error */
			gdix_err("--- End flash subsystem%d: Fatal error:%d exit ---\n",
				i, r);
			goto exit_flash;
		}
	}

exit_flash:
	return r;	
}

// static int gdix_fw_version_compare(struct fw_update_ctrl *fwu_ctrl)
// {
// 	int ret = 0;
// 	struct goodix_fw_version fw_version;
// 	struct firmware_summary *fw_summary = &fwu_ctrl->fw_data.fw_summary;

// 	/* compare fw_version */
// 	ret = gdix_read_version(&fw_version);
// 	if (ret < 0)
// 		return ret;

// 	if (memcmp(fw_version.patch_pid, fw_summary->fw_pid, 8)) {
// 		gdix_err("Product ID mismatch:%s != %s\n",
// 			fw_version.patch_pid, fw_summary->fw_pid);
// 		return -1;
// 	}

// 	ret = memcmp(fw_version.patch_vid, fw_summary->fw_vid, 4);
// 	if (ret) {
// 		gdix_dbg("active firmware version:%*ph\n", 4,
// 				fw_version.patch_vid);
// 		gdix_dbg("firmware file version: %*ph\n", 4,
// 				fw_summary->fw_vid);
// 		return -1;
// 	}
// 	gdix_dbg("fw_version equal\n");

// 	return 0;
// }

static int gdix_update_finish(struct fw_update_ctrl *fwu_ctrl)
{
	gdix_soft_reset(100);
	return 0;
}

static int gdix_fw_update_proc(struct fw_update_ctrl *fwu_ctrl)
{
	int ret;

	ret = gdix_parse_firmware(&fwu_ctrl->fw_data);
	if (ret < 0) {
		gdix_err("failed to parse firmware\n");
		return ret;
	}

	ret = gdix_update_prepare(fwu_ctrl);
	if (ret < 0) {
		gdix_err("failed prepare ISP\n");
		goto err_fw_prepare;
	}

	ret = gdix_flash_firmware(fwu_ctrl);
	if (ret < 0) {
		gdix_err("failed flash frimware\n");
		goto err_fw_prepare;
	}

	gdix_dbg("flash fw data success, need reset\n");

err_fw_prepare:
	gdix_update_finish(fwu_ctrl);
	return ret;
}

int gdix_do_fw_update(const char *devname, const char *filename, uint8_t i2c_addr)
{
	int ret;

	if (!devname || !filename) {
		gdix_err("devname or filename is NULL\n");
		return -1;
	}

	if (!strstr(devname, "dev/i2c")) {
		gdix_err("devname[%s] is invalid\n", devname);
		return -1;
	}

	g_client_addr = i2c_addr;
	gdix_dbg("i2c-addr:0x%02x\n", g_client_addr);
    g_fd = open(devname, O_RDWR);
    if (g_fd < 0) {
		gdix_err("failed to open %s\n", devname);
		return -1;
	}

	ret = switch_i2c_mode();
	if (ret < 0)
		goto err_out;

	ret = gdix_read_firmware(filename);
	if (ret < 0) {
		gdix_err("failed to read %s\n", filename);
		goto err_out;
	}

	gdix_fw_update_proc(&goodix_fw_update_ctrl);

err_out:
	close(g_fd);
	g_fd = 0;
	return 0;
}