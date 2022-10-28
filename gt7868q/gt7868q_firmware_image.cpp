#include "gt7868q_firmware_image.h"
#include "../gtp_util.h"

#define GT7868Q_SUB_FW_INFO_OFFSET 32
#define GT7868Q_SUB_FW_DATA_OFFSET 256 // x8=256

#define GT7868Q_FW_IMAGE_PID_OFFSET 15
#define GT7868Q_FW_IMAGE_PID_LEN 8
#define GT7868Q_FW_IMAGE_VID_OFFSET 24
#define GT7868Q_FW_IMAGE_CID_OFFSET 23
#define GT7868Q_FW_IMAGE_SUB_FWNUM_OFFSET 27

GT7868QFirmwareImage::GT7868QFirmwareImage() {}

GT7868QFirmwareImage::~GT7868QFirmwareImage() {}

int GT7868QFirmwareImage::GetFirmwareSubFwDataOffset()
{
	return GT7868Q_SUB_FW_DATA_OFFSET;
}

int GT7868QFirmwareImage::GetFirmwareSubFwNum()
{
	if (!m_firmwareData)
		return 0;

	return m_firmwareData[GT7868Q_FW_IMAGE_SUB_FWNUM_OFFSET];
}

int GT7868QFirmwareImage::InitPid()
{
	gdix_dbg("GT7868QFirmwareImage::InitPid run\n");
	int ret = -1;
	int i = 0;
	int j = 0;

	if (!m_firmwareData) {
		goto exit;
	}
	for (i = 0, j = 0; i < GT7868Q_FW_IMAGE_PID_LEN; i++)
		if (m_firmwareData[GT7868Q_FW_IMAGE_PID_OFFSET + i] != 0)
			m_pid[j++] = m_firmwareData[GT7868Q_FW_IMAGE_PID_OFFSET + i];
	m_pid[j] = '\0';
	ret = 0;

exit:
	gdix_dbg("GT7868QFirmwareImage::InitPid exit\n");
	return ret;
}

int GT7868QFirmwareImage::InitVid()
{
	gdix_dbg("GT7868QFirmwareImage InitVid run\n");
	int ret = -1;

	if (!m_firmwareData)
		goto exit;

	m_firmwareVersionMajor = m_firmwareData[GT7868Q_FW_IMAGE_VID_OFFSET];
	/* |--vid2--|--vid3--|--cfg_id--|
	 * reserve the last byte for config ID.
	 */
	m_firmwareVersionMinor =
		(m_firmwareData[GT7868Q_FW_IMAGE_VID_OFFSET + 1] << 16) |
		(m_firmwareData[GT7868Q_FW_IMAGE_VID_OFFSET + 2] << 8);
	gdix_dbg("cid:0x%02x,vid 0x%02X,0x%02X,0x%02X\n",
			 m_firmwareData[GT7868Q_FW_IMAGE_CID_OFFSET],
			 m_firmwareData[GT7868Q_FW_IMAGE_VID_OFFSET],
			 m_firmwareData[GT7868Q_FW_IMAGE_VID_OFFSET + 1],
			 m_firmwareData[GT7868Q_FW_IMAGE_VID_OFFSET + 2]);
	ret = 0;
exit:
	gdix_dbg("GT7868QFirmwareImage InitVid exit,exit code:%d\n", ret);
	return ret;
}
