#include "marsei.h"



MarseiDevice::MarseiDevice() { gdix_dbg("Enter MarseiDevice\n"); }

MarseiDevice::~MarseiDevice() {}

int MarseiDevice::SetBasicProperties()
{
	uint8_t tempBuf[14] = {0};
	int ret;
	// uint8_t main_ver;
	uint8_t vice_ver;
	uint8_t inter_ver;
	uint8_t cfg_ver;

	/* read pid/vid/sensorid */
	ret = Read(0x1001E, tempBuf, 14);
	if (ret < 0) {
		gdix_err("Failed read PID/VID\n");
		return ret;
	}
	gdix_dbg_array(tempBuf, 14);
	memcpy(m_pid, tempBuf, 8);
    if (!memcmp(m_pid, "7864", 4)) {
        memcpy(m_pid, "7866D", 5);
    }
	memcpy(m_vid, tempBuf + 8, 4);
	m_sensorID = tempBuf[13];

	// main_ver = tempBuf[9];
	vice_ver = tempBuf[10];
	inter_ver = tempBuf[11];

	/* read config id/version */
	ret = Read(0x1A772, tempBuf, 1);
	if (ret < 0) {
		gdix_err("Failed read config id/version\n");
		return ret;
	}
	cfg_ver = tempBuf[0];

	// m_firmwareVersionMajor = main_ver;
	m_firmwareVersionMajor = 0;
	m_firmwareVersionMinor = (vice_ver << 16) | (inter_ver << 8) | cfg_ver;

	gdix_info("PID:%s\n", m_pid);
	gdix_info("VID:%02x %02x %02x %02x\n", m_vid[0], m_vid[1], m_vid[2],
			  m_vid[3]);
	gdix_info("sensorID:%d\n", m_sensorID);
	gdix_info("configVer:%02x\n", cfg_ver);
	gdix_info("version:%d.%d\n", m_firmwareVersionMajor,
			  m_firmwareVersionMinor);
    return 0;
}