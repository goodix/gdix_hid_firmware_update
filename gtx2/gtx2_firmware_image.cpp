#include "gtx2_firmware_image.h"
#include "../gtp_util.h"

#define GTX2_FW_IMAGE_PID_OFFSET    15
#define GTX2_FW_IMAGE_PID_LEN       6
#define GTX2_FW_IMAGE_VID_OFFSET 21 //x8=23 
#define GTX2_FW_IMAGE_SUB_FWNUM_OFFSET 24 //x8=26
#define GTX2_SUB_FW_INFO_OFFSET 32
#define GTX2_SUB_FW_DATA_OFFSET 128 //x8=256

GTX2FirmwareImage::GTX2FirmwareImage(){
    
}

GTX2FirmwareImage::~GTX2FirmwareImage(){

}

int GTX2FirmwareImage::GetFirmwareSubFwNum(){
    if(!m_firmwareData)
        return 0;

    return m_firmwareData[GTX2_FW_IMAGE_SUB_FWNUM_OFFSET];
}
int GTX2FirmwareImage::GetFirmwareSubFwInfoOffset(){
    return GTX2_SUB_FW_INFO_OFFSET;
}
int GTX2FirmwareImage::GetFirmwareSubFwDataOffset(){
    return GTX2_SUB_FW_DATA_OFFSET;
}

int GTX2FirmwareImage::InitPid(){
    gdix_dbg("GTX2FirmwareImage %s run\n",__func__);
    int ret = -1;
    int i = 0;
    int j = 0;

    if(!m_firmwareData){
        goto exit;

    }
       

	for (i = 0, j = 0; i < GTX2_FW_IMAGE_PID_LEN; i++)
		if (m_firmwareData[GTX2_FW_IMAGE_PID_OFFSET + i] != 0)
			m_pid[j++] = m_firmwareData[GTX2_FW_IMAGE_PID_OFFSET + i];
	m_pid[j] = '\0';
    ret = 0;
exit:
    gdix_dbg("GTX2FirmwareImage %s exit,exit code:%d\n",__func__,ret);
    return ret;
}

int GTX2FirmwareImage::InitVid(){
    gdix_dbg("GTX2FirmwareImage %s run\n",__func__);
    int ret = -1;

    if(!m_firmwareData)
        goto exit;

    m_firmwareVersionMajor = 0;
    m_firmwareVersionMinor = ((m_firmwareData[GTX2_FW_IMAGE_VID_OFFSET] << 16) |
                               (m_firmwareData[GTX2_FW_IMAGE_VID_OFFSET + 1] << 8) |
                               (m_firmwareData[GTX2_FW_IMAGE_VID_OFFSET + 2]));
    gdix_dbg("vid 0x%02X,0x%02X,0x%02X\n",
            m_firmwareData[GTX2_FW_IMAGE_VID_OFFSET],
            m_firmwareData[GTX2_FW_IMAGE_VID_OFFSET+1],
            m_firmwareData[GTX2_FW_IMAGE_VID_OFFSET+2]);
    ret = 0;
exit:
    gdix_dbg("GTX2FirmwareImage %s exit,exit code:%d\n",__func__,ret);
    return ret;
}