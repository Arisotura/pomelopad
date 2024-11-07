/*
    Copyright 2024 Arisotura

    This file is part of pomelopad.

    pomelopad is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    pomelopad is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with pomelopad. If not, see http://www.gnu.org/licenses/.
*/

#include <stdio.h>
#include <string.h>
#include "WUP.h"
#include "I2C.h"
#include "AudioAmp.h"
#include "Camera.h"
#include "LCD.h"
#include "Platform.h"

using Platform::Log;
using Platform::LogLevel;

namespace I2C
{

u32 ChanEnable; // CHECKME -- might just be IRQ enable??
u32 ChanIRQ;

struct sDevice
{
    u8 Addr;
    void (*fnStart)();
    void (*fnStop)();
    u8 (*fnRead)();
    void (*fnWrite)(u8);
};

struct sHostChan
{
    int ID;

    u32 Unk00;
    u32 Cnt;
    u32 Unk10;
    u32 Status;
    u32 Unk20;
    u8 DataRead;

    static const int kMaxDevices = 3;
    sDevice Devices[kMaxDevices];
    int NumDevices;
    sDevice* CurDevice;

    void Init(int id)
    {
        ID = id;
        memset(Devices, 0, sizeof(Devices));
        NumDevices = 0;
    }

    void RegisterDevice(u8 addr, void (*fnstart)(), void (*fnstop)(), u8 (*fnread)(), void (*fnwrite)(u8))
    {
        sDevice* dev = &Devices[NumDevices++];
        dev->Addr = addr;
        dev->fnStart = fnstart;
        dev->fnStop = fnstop;
        dev->fnRead = fnread;
        dev->fnWrite = fnwrite;
    }

    void Reset()
    {
        Unk00 = 0;
        Cnt = 0;
        Unk10 = 0;
        Status = 0;
        Unk20 = 0;
        DataRead = 0;

        CurDevice = nullptr;
    }

    void WriteCnt(u32 val)
    {
        Cnt = val;

        if (Cnt & (1<<0))
        {
            // stop

            printf("-- I2C: stop\n");
            if (CurDevice)
            {
                CurDevice->fnStop();
                CurDevice = nullptr;
            }

            Status = (1<<0);

            SetIRQ(ID);
            Cnt &= ~(1<<0);
        }
        else if (Cnt & (1<<5))
        {
            // request read

            if (Cnt & (1<<2))
            {
                printf("-- I2C: read (%02X)\n", Cnt);
                if (CurDevice)
                {
                    DataRead = CurDevice->fnRead();
                }
                else
                    DataRead = 0xFF;
            }

            Status = 0;

            SetIRQ(ID);
            Cnt &= ~(1<<5);
        }
    }

    void WriteData(u8 val)
    {
        // checkme
        if (!(ChanEnable & (1<<ID))) return;

        if (Cnt & (1<<1))
        {
            // start
            u8 devaddr = val >> 1;
            bool read = !!(val & (1<<0));
            printf("-- I2C: start, dev=%02X read=%d\n", devaddr, read);

            CurDevice = nullptr;
            for (int i = 0; i < NumDevices; i++)
            {
                if (Devices[i].Addr == devaddr)
                {
                    CurDevice = &Devices[i];
                    break;
                }
            }

            if (CurDevice)
            {
                CurDevice->fnStart();
                Status = (1<<2) | (1<<1);
            }
            else
                Status = (1<<1); // NACK

            if (!read) Status |= (1<<3); // write mode

            SetIRQ(ID);
            Cnt &= ~(1<<1);
        }
        else
        {
            printf("-- I2C: write %02X\n", val);
            if (CurDevice)
            {
                CurDevice->fnWrite(val);
                Status = (1<<3) | (1<<2);
            }
            else
                Status = (1<<3);

            SetIRQ(ID);
        }
    }

    u32 Read(u32 addr)
    {
        switch (addr & 0x3FF)
        {
        case 0x000: return Unk00;
        case 0x004: return DataRead;
        case 0x008: return Cnt;
        case 0x010: return Unk10;
        case 0x018: return Status;
        case 0x020: return Unk20;
        }

        return 0;
    }

    void Write(u32 addr, u32 val)
    {
        switch (addr & 0x3FF)
        {
        case 0x000:
            Unk00 = val;
            return;

        case 0x004:
            WriteData(val & 0xFF);
            return;

        case 0x008:
            WriteCnt(val);
            return;

        case 0x010:
            Unk10 = val;
            return;

        case 0x020:
            Unk20 = val;
            return;
        }
    }
};

sHostChan HostChan[3];


bool Init()
{
    HostChan[0].Init(2);
    HostChan[1].Init(3);
    HostChan[2].Init(4);

    HostChan[1].RegisterDevice(0x18, AudioAmp::Start, AudioAmp::Stop, AudioAmp::Read, AudioAmp::Write);
    HostChan[1].RegisterDevice(0x21, Camera::Start, Camera::Stop, Camera::Read, Camera::Write);
    HostChan[1].RegisterDevice(0x39, LCD::Start, LCD::Stop, LCD::Read, LCD::Write);

    return true;
}

void DeInit()
{
}

void Reset()
{
    ChanEnable = 0;
    ChanIRQ = 0;

    for (int i = 0; i < 3; i++)
        HostChan[i].Reset();
}


void SetIRQ(int chan)
{
    ChanIRQ |= (1<<chan);
    WUP::SetIRQ(WUP::IRQ_I2C);
}


u32 Read(u32 addr)
{
    //printf("I2C read %08X - %08X\n", addr, WUP::GetPC());
    switch (addr & 0xFFFFFC00)
    {
    case 0xF0006000: return HostChan[0].Read(addr);
    case 0xF0006400: return HostChan[1].Read(addr);
    case 0xF0006800: return HostChan[2].Read(addr);
    }

    switch (addr)
    {
    case 0xF0005800: return ChanIRQ;
    case 0xF0005804: return ChanEnable;
    }

    return 0;
}

void Write(u32 addr, u32 val)
{
    //printf("I2C write %08X %08X - %08X\n", addr, val, WUP::GetPC());
    switch (addr & 0xFFFFFC00)
    {
    case 0xF0006000: HostChan[0].Write(addr, val); return;
    case 0xF0006400: HostChan[1].Write(addr, val); return;
    case 0xF0006800: HostChan[2].Write(addr, val); return;
    }

    switch (addr)
    {
    case 0xF0005804:
        ChanEnable = val & 0x1F;
        return;

    case 0xF0005808:
        ChanIRQ &= ~val;
        return;
    }
}

}
