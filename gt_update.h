#ifndef _GTUPDATE_H_
#define _GTUPDATE_H_

#include <memory.h>
#include "gtp_util.h"
#include "gtmodel.h"
#include "firmware_image.h"

#define FLASH_BUFFER_ADDR	0xc000  //X8=0XDE24

class GTupdate
{
public:
    GTupdate();
    virtual ~GTupdate();

    virtual int Initialize(GTmodel *dev,FirmwareImage *image);
    virtual int Run(void* para){return -1;}

protected:
    GTmodel* dev = NULL;
    FirmwareImage* image = NULL;
    bool m_Initialized;
    virtual int check_update();
    virtual int load_sub_firmware(unsigned int flash_addr,
			unsigned char *fw_data, unsigned int len){return -1;}; 
    virtual int fw_update(unsigned int firmware_flag){return -1;};
    virtual int cfg_update(){return -1;}
};

typedef struct{
    bool force;
    unsigned int firmwareFlag;
}GTUpdatePara,*pGTUpdatePara;

#endif