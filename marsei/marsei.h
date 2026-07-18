#ifndef _MARSEI_H_
#define _MARSEI_H_

#include "../gtp_util.h"
#include "../gtx9/gtx9.h"


class MarseiDevice : public GTx9Device
{
public:
	MarseiDevice();
	virtual ~MarseiDevice();

	virtual int SetBasicProperties();
};

#endif