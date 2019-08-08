#include "gt_update.h"

GTupdate::GTupdate()
{}

GTupdate::~GTupdate()
{}

int GTupdate::Initialize(GTmodel *dev,FirmwareImage *image)
{
	m_Initialized = false;
    this->dev = dev;
    this->image = image;
	if(dev != NULL && dev->IsOpened() &&
		image != NULL && image->IsOpened())
	{	
		m_Initialized = true;
		return 0;
	}
	return -1;
}

/* NOTE: deprecated interface */
int GTupdate::check_update()
{
	int ret;
	if(!dev->IsOpened())
	{
		gdix_err("No dev is Opened.");
		return -2;
	}
	if(!image->IsOpened())
	{	
		gdix_err("No image is Opened.");
		return -2;
	}
	gdix_dbg("dev PID is %s, image PID is %s\n",
		 dev->GetProductID(), image->GetProductID());

	/* compare PID */
	ret = memcmp(dev->GetProductID(), image->GetProductID(), 4);
	if (ret)
		return -1;

	gdix_dbg("dev major ver:0x%x,Minor ver:0x%x\n",
			dev->GetFirmwareVersionMajor(),
			dev->GetFirmwareVersionMinor());
	gdix_dbg("fw_image major ver:0x%x,Minor ver:0x%x\n",
			image->GetFirmwareVersionMajor(),
			image->GetFirmwareVersionMinor());
	if(dev->GetFirmwareVersionMajor() != image->GetFirmwareVersionMajor() ||
	   dev->GetFirmwareVersionMinor() != image->GetFirmwareVersionMinor()){
		   return 0;
	}else{
		return -2;
	}
}