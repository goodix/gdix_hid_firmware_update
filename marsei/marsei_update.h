#ifndef _MARSEI_UPDATE_H
#define _MARSEI_UPDATE_H

#include "../gtx9/gtx9_update.h"

class MarseiUpdate : public GTx9Update
{
protected:
	virtual int prepareUpdate();
    virtual int flashSubSystem(struct fw_subsys_info *subsys);
	virtual int fw_update(unsigned int firmware_flag);
private:
    int DisableReport();
};




#endif