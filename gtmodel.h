/*
 * Copyright (C) 2017 Goodix Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _GTMODEL_H_
#define _GTMODEL_H_

#include <string>

class GTmodel
{
public:
    GTmodel(){};
    virtual ~GTmodel(){};

    virtual int Open(const char *filename){return 0;}
    virtual bool IsOpened(){return m_deviceOpen;}
    virtual int Read(unsigned short addr, unsigned char *buf, unsigned int len){return 0;}
    virtual int GetReport(unsigned char reportId, unsigned char *buf){return 0;}

    virtual int Write(unsigned short addr, const unsigned char *buf, unsigned int len){return 0;}
    virtual int Write(const unsigned char *buf, unsigned int len){return 0;}

    virtual int GetFirmwareProps(const char *deviceName, char *props_buf, int len){return 0;}
    virtual int GetFirmwareVersionMajor() { return 0; }
    virtual int GetFirmwareVersionMinor() { return 0; }
    virtual unsigned char *GetProductID() { return 0; }
    virtual unsigned char GetSensorID(){return -1;}
    virtual int QueryBasicProperties(){return 0;}
    virtual int SetBasicProperties(){return 0;}
    virtual void Close(){return;}
    virtual int GetFd() { return 0; }
    
protected:
    bool m_deviceOpen;
};

#endif
