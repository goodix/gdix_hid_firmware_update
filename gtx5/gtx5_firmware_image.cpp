#include "gtx5_firmware_image.h"
#include "../gtp_util.h"

#define GTX5_SUB_FW_INFO_OFFSET 32
#define GTX5_SUB_FW_DATA_OFFSET 256 //x8=256
#define GTX5_FW_IMAGE_SUB_FWNUM_OFFSET 26

#define GTX5_FW_IMAGE_PID_OFFSET 15 
#define GTX5_FW_IMAGE_PID_LEN    8
#define GTX5_FW_IMAGE_VID_OFFSET 24
#define GTX5_FW_IMAGE_CID_OFFSET 23    

GTX5FirmwareImage::GTX5FirmwareImage(){

}

GTX5FirmwareImage::~GTX5FirmwareImage(){
    
}

int GTX5FirmwareImage::GetFirmwareSubFwNum(){
    if(!m_firmwareData)
            return 0;

    return m_firmwareData[GTX5_FW_IMAGE_SUB_FWNUM_OFFSET];
}

int GTX5FirmwareImage::GetFirmwareSubFwInfoOffset(){
    return GTX5_SUB_FW_INFO_OFFSET;
}

int GTX5FirmwareImage::GetFirmwareSubFwDataOffset(){
    return GTX5_SUB_FW_DATA_OFFSET;
}

int GTX5FirmwareImage::InitPid(){
    gdix_dbg("GTX5FirmwareImage::InitPid run\n");
    int ret = -1;
    int i = 0;
    int j = 0;

    if(!m_firmwareData){
        goto exit;
    }
    for (i = 0, j = 0; i < GTX5_FW_IMAGE_PID_LEN; i++)
		if (m_firmwareData[GTX5_FW_IMAGE_PID_OFFSET + i] != 0)
			m_pid[j++] = m_firmwareData[GTX5_FW_IMAGE_PID_OFFSET + i];
	m_pid[j] = '\0';
    ret = 0;

exit:
    gdix_dbg("GTX5FirmwareImage::InitPid exit\n");
    return ret;
}

int GTX5FirmwareImage::InitVid(){
    gdix_dbg("GTX5FirmwareImage InitVid run\n");
    int ret = -1;

    if(!m_firmwareData)
        goto exit;

    m_firmwareVersionMajor = m_firmwareData[GTX5_FW_IMAGE_CID_OFFSET];
    m_firmwareVersionMinor = ((m_firmwareData[GTX5_FW_IMAGE_VID_OFFSET] << 16) |
                               (m_firmwareData[GTX5_FW_IMAGE_VID_OFFSET + 1] << 8) |
                               (m_firmwareData[GTX5_FW_IMAGE_VID_OFFSET + 2]));
    gdix_dbg("cid:0x%02x,vid 0x%02X,0x%02X,0x%02X\n",
            m_firmwareData[GTX5_FW_IMAGE_CID_OFFSET],
            m_firmwareData[GTX5_FW_IMAGE_VID_OFFSET],
            m_firmwareData[GTX5_FW_IMAGE_VID_OFFSET+1],
            m_firmwareData[GTX5_FW_IMAGE_VID_OFFSET+2]);
    ret = 0;
exit:
    gdix_dbg("GTX5FirmwareImage InitVid exit,exit code:%d\n",ret);
    return ret;
}