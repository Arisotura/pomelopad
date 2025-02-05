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
#include "SPI.h"
#include "DMA.h"
#include "Flash.h"
#include "UIC.h"
#include "FIFO.h"
#include "Platform.h"

using Platform::Log;
using Platform::LogLevel;

namespace SPI
{

u32 ClockCnt;
u32 Cnt;
u32 IRQFlags;
u32 Unk14;
u32 IRQEnable;
u32 ReadLength;
u32 DeviceSel;

u32 GPIO_CS[2];

FIFO<u8, 16> WriteFIFO;
FIFO<u8, 16> ReadFIFO;

bool Busy;
u8 ManualSel;
u8 CurDevice;
u32 ReadRemaining;


bool Init()
{
    return true;
}

void DeInit()
{
}

void Reset()
{
    // CHECKME
    ClockCnt = 0;
    Cnt = 0x0300;
    IRQFlags = 0;
    Unk14 = 0;
    IRQEnable = 0;
    ReadLength = 0;
    DeviceSel = 0;

    GPIO_CS[0] = 0;
    GPIO_CS[1] = 0;

    WriteFIFO.Clear();
    ReadFIFO.Clear();

    Busy = false;
    ManualSel = 0;
    CurDevice = 0;
    ReadRemaining = 0;
}


void SetCnt(u32 val)
{
    Cnt = val;
    UpdateChipSelect();
}

void UpdateChipSelect()
{
    u8 oldsel = CurDevice;
    ManualSel = 0;
    u8 devsel = DeviceSel;

    /*if (GPIO_CS[0] & (1<<9))
        ManualSel |= (~(GPIO_CS[0] >> 8) & 0x1);
    if (GPIO_CS[1] & (1<<9))
        ManualSel |= (~(GPIO_CS[1] >> 7) & 0x2);*/
    if (GPIO_CS[0] & (1<<9))
    {
        devsel &= ~(1<<0);
        ManualSel |= (~(GPIO_CS[0] >> 8) & 0x1);
    }
    if (GPIO_CS[1] & (1<<9))
    {
        devsel &= ~(1<<1);
        ManualSel |= (~(GPIO_CS[1] >> 7) & 0x2);
    }

    if ((Cnt & 0x300) == 0x100) // manual CS, asserted
        ManualSel |= devsel;

    CurDevice = ManualSel;

    // auto chipselect
    if (Busy && (!(Cnt & (1<<8))))
        CurDevice |= devsel;

    if (CurDevice == oldsel)
        return;

    switch (CurDevice)
    {
    /*case 0: printf("SPI: no device selected\n"); break;
    case 1: printf("SPI: selected FLASH\n"); break;
    case 2: printf("SPI: selected UIC\n"); break;*/
    case 3: printf("SPI: SYNESTHESIA!!\n"); break;
    }

    u8 change = CurDevice ^ oldsel;
    if (change & (1<<0))
    {
        if (CurDevice & (1<<0)) Flash::Select();
        else                    Flash::Release();
    }
    if (change & (1<<1))
    {
        if (CurDevice & (1<<1)) UIC::Select();
        else                    UIC::Release();
    }
}


void ScheduleTransfer(bool write)
{
    // TODO: determine delay based on clock settings and system clock
    u32 delay = 32;
    auto cb = write ? OnWrite : OnRead;
    WUP::ScheduleEvent(WUP::Event_SPITransfer, false, delay, cb, 0);
}

void StartRead()
{
    if (!(Cnt & (1<<1))) return;
    if (ReadLength == 0) return;
    if (Busy) return;

    // TODO: is it possible to change this while a read is in progress?
    ReadRemaining = ReadLength;
    //printf("SPI: start read, length=%08X\n", ReadLength);

    Busy = true;
    UpdateChipSelect();
    ScheduleTransfer(false);
}

void WriteData(u8 val)
{
    if (Cnt & (1<<1)) return;

    if (WriteFIFO.IsFull())
    {
        // TODO: is any error raised?
        return;
    }

    WriteFIFO.Write(val);

    if (!Busy)
    {
        Busy = true;
        UpdateChipSelect();
        ScheduleTransfer(true);
    }
}

u8 ReadData()
{
    if (!(Cnt & (1<<1))) return 0;

    bool wasfull = ReadFIFO.IsFull();
    u8 ret = ReadFIFO.Read();

    if (wasfull && Busy)
        ScheduleTransfer(false);

    return ret;
}

void OnWrite(u32 param)
{
    u8 val = WriteFIFO.Read();
    if (CurDevice & (1<<0)) Flash::Write(val);
    else if (CurDevice & (1<<1)) UIC::Write(val);

    if (WriteFIFO.IsEmpty())
    {
        Busy = false;
        UpdateChipSelect();

        // CHECKME
        if (IRQEnable & (1<<7))
        {
            IRQFlags |= (1<<7);
            WUP::SetIRQ(WUP::IRQ_SPI);
        }
    }
    else
        ScheduleTransfer(true);

    DMA::CheckSPDMA(2, true, WriteFIFO.FreeSpace());
}

void OnRead(u32 param)
{
    u8 val = 0;
    if (CurDevice & (1<<0)) val = Flash::Read();
    else if (CurDevice & (1<<1)) val = UIC::Read();
    ReadFIFO.Write(val);

    ReadRemaining--;
    if (ReadRemaining == 0)
    {
        Busy = false;
        UpdateChipSelect();

        // CHECKME
        if (IRQEnable & (1<<6))
        {
            IRQFlags |= (1<<6);
            WUP::SetIRQ(WUP::IRQ_SPI);
        }
    }
    else if (!ReadFIFO.IsFull())
        ScheduleTransfer(false);

    DMA::CheckSPDMA(2, false, ReadFIFO.Level());
}


u32 Read(u32 addr)
{
    switch (addr)
    {
    case 0xF0004400: return ClockCnt;
    case 0xF0004404: return Cnt;
    case 0xF0004408: return IRQFlags;
    case 0xF000440C: return WriteFIFO.FreeSpace() | (ReadFIFO.Level() << 8);
    case 0xF0004410: return ReadData();
    case 0xF0004414: return Unk14;
    case 0xF0004418: return IRQEnable;
    case 0xF0004420: return ReadLength;
    case 0xF0004424: return DeviceSel;

    case 0xF00050F8: return GPIO_CS[0];
    case 0xF00050FC: return GPIO_CS[1];
    }

    return 0;
}

void Write(u32 addr, u32 val)
{
    switch (addr)
    {
    case 0xF0004400:
        ClockCnt = val;
        return;
    case 0xF0004404:
        SetCnt(val);
        return;
    case 0xF0004408:
        IRQFlags &= ~val;
        return;
    case 0xF0004410:
        WriteData(val & 0xFF);
        return;
    case 0xF0004414:
        Unk14 = val;
        return;
    case 0xF0004418:
        // TODO: should this trigger IRQ if flags were already set?
        IRQEnable = val;
        return;
    case 0xF0004420:
        ReadLength = val;
        StartRead();
        return;
    case 0xF0004424:
        DeviceSel = val & 0x3;
        UpdateChipSelect();
        return;

    case 0xF00050F8: // FLASH CS
        GPIO_CS[0] = val;
        UpdateChipSelect();
        return;
    case 0xF00050FC: // UIC CS
        GPIO_CS[1] = val;
        UpdateChipSelect();
        return;
    }
}


// TODO add delay to DMA transfers
// LLE UIC will need this

void StartDMA(bool write)
{
    if (write)
        DMA::CheckSPDMA(2, true, WriteFIFO.FreeSpace());
    else
        DMA::CheckSPDMA(2, false, ReadFIFO.Level());
}
/*
bool DMAStart(bool write)
{
    if (write)
    {
        if (Cnt & (1<<1)) return false;
    }
    else
    {
        if (!(Cnt & (1<<1))) return false;
    }

    if (!write)
        ReadRemaining = ReadLength;

    Busy = true;
    UpdateChipSelect();

    return true;
}

void DMAFinish()
{
    if (!(Cnt & (1<<1)))
    {
        Busy = false;
        UpdateChipSelect();

        // CHECKME
        if (IRQEnable & (1<<7))
        {
            IRQFlags |= (1<<7);
            WUP::SetIRQ(WUP::IRQ_SPI);
        }
    }
}

u8 DMARead()
{
    u8 val = 0;
    if (ReadRemaining > 0)
    {
        if (CurDevice & (1 << 0)) val = Flash::Read();
        else if (CurDevice & (1 << 1)) val = UIC::Read();
    }

    ReadRemaining--;
    if (ReadRemaining == 0)
    {
        Busy = false;
        UpdateChipSelect();

        // CHECKME
        if (IRQEnable & (1<<6))
        {
            IRQFlags |= (1<<6);
            WUP::SetIRQ(WUP::IRQ_SPI);
        }
    }

    return val;
}

void DMAWrite(u8 val)
{
    if (CurDevice & (1<<0)) Flash::Write(val);
    else if (CurDevice & (1<<1)) UIC::Write(val);
}
*/
}
