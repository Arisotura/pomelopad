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
#include "SDIO.h"
#include "Wifi.h"
#include "FIFO.h"
#include "Platform.h"

using Platform::Log;
using Platform::LogLevel;

namespace SDIO
{

u32 DMAAddr;
u16 BlockSize;
u16 BlockCount;
u16 TransferMode;
u16 CurBlock;
bool Transferring;

u32 Arg;
u16 Cmd;
u16 Resp[8];
FIFO<u8, 0x200> DataBuffer;
u32 PresentState;

u8 HostCnt;
//
u16 ClockCnt;

u16 IRQFlags;
u16 ErrorIRQFlags;
u16 IRQEnable;
u16 ErrorIRQEnable;
u16 IRQSignalEnable;
u16 ErrorIRQSignalEnable;


bool Init()
{
    return true;
}

void DeInit()
{
    //
}

void Reset()
{
    DMAAddr = 0;
    BlockSize = 0;
    BlockCount = 0;
    TransferMode = 0;
    CurBlock = 0;
    Transferring = false;

    Arg = 0;
    Cmd = 0;
    memset(Resp, 0, sizeof(Resp));
    DataBuffer.Clear();
    PresentState = 0;

    HostCnt = 0;
    //
    ClockCnt = 0;

    IRQFlags = 0;
    ErrorIRQFlags = 0;
    IRQEnable = 0;
    ErrorIRQEnable = 0;
    IRQSignalEnable = 0;
    ErrorIRQSignalEnable = 0;
}


void SetIRQ(int irq)
{
    if (!(IRQEnable & (1<<irq)))
        return;

    IRQFlags |= (1<<irq);
    if (IRQFlags & IRQEnable & IRQSignalEnable)
        WUP::SetIRQ(WUP::IRQ_SDIO);
}

void SetErrorIRQ(int irq)
{
    if (!(ErrorIRQEnable & (1<<irq)))
        return;

    ErrorIRQFlags |= (1<<irq);
    if (ErrorIRQFlags & ErrorIRQEnable && ErrorIRQSignalEnable)
        SetIRQ(IRQ_Error);
}


void SendResponse(u32 resp)
{
    *(u32*)&Resp[6] = *(u32*)&Resp[4];
    *(u32*)&Resp[4] = *(u32*)&Resp[2];
    *(u32*)&Resp[2] = *(u32*)&Resp[0];
    *(u32*)&Resp[0] = resp;
}

void RequestBlockRead()
{
    u8 tmp[0x200];

    if (!Transferring) return;
    if (CurBlock >= BlockCount) return;

    if (!(TransferMode & (1<<4)))
    {
        printf("RequestBlockRead: wrong transfer mode %04X\n", TransferMode);
        return;
    }

    if (TransferMode & (1<<0))
    {
        // DMA
        for (int i = 0; i < BlockCount; i++)
        {
            Wifi::ReadBlock(tmp, BlockSize);

            for (int j = 0; j < BlockSize; j++)
            {
                WUP::MainRAM[DMAAddr] = tmp[j];
                DMAAddr = (DMAAddr + 1) & 0x3FFFFF;
            }

            CurBlock++;
        }

        SetIRQ(IRQ_TransferDone);
        Transferring = false;
    }
    else
    {
        // manual
        if (DataBuffer.Level() > 0)
        {
            // ??
            return;
        }

        Wifi::ReadBlock(tmp, BlockSize);

        for (int i = 0; i < BlockSize; i++)
            DataBuffer.Write(tmp[i]);

        CurBlock++;
        PresentState |= (1<<11);
        SetIRQ(IRQ_DataReadReady);

        if (CurBlock >= BlockCount)
        {
            SetIRQ(IRQ_TransferDone);
            Transferring = false;
        }
    }
}

void RequestBlockWrite()
{
    u8 tmp[0x200];

    if (!Transferring) return;
    if (CurBlock >= BlockCount) return;

    if (TransferMode & (1<<4))
    {
        printf("RequestBlockWrite: wrong transfer mode %04X\n", TransferMode);
        return;
    }

    if (TransferMode & (1<<0))
    {
        // DMA
        for (int i = 0; i < BlockCount; i++)
        {
            for (int j = 0; j < BlockSize; j++)
            {
                tmp[j] = WUP::MainRAM[DMAAddr];
                DMAAddr = (DMAAddr + 1) & 0x3FFFFF;
            }

            Wifi::WriteBlock(tmp, BlockSize);

            CurBlock++;
        }

        SetIRQ(IRQ_TransferDone);
        Transferring = false;
    }
    else
    {
        // manual
        if (DataBuffer.Level() < BlockSize)
        {
            PresentState |= (1<<10);
            SetIRQ(IRQ_DataWriteReady);
            return;
        }

        for (int i = 0; i < BlockSize; i++)
            tmp[i] = DataBuffer.Read();

        Wifi::WriteBlock(tmp, BlockSize);

        CurBlock++;

        if (CurBlock >= BlockCount)
        {
            SetIRQ(IRQ_TransferDone);
            Transferring = false;
        }
        else
        {
            PresentState |= (1<<10);
            SetIRQ(IRQ_DataWriteReady);
        }
    }
}

u8 ReadDataBuffer()
{
    if (DataBuffer.IsEmpty())
        return 0;

    u8 ret = DataBuffer.Read();
    if (DataBuffer.IsEmpty())
    {
        PresentState &= ~(1<<11);
        RequestBlockRead();
    }

    return ret;
}

void WriteDataBuffer(u8 val)
{
    if (DataBuffer.Level() >= BlockSize)
        return;

    DataBuffer.Write(val);
    if (DataBuffer.Level() >= BlockSize)
    {
        PresentState &= ~(1<<10);
        RequestBlockWrite();
    }
}

void StartTransfer(bool write)
{
    if (write)
    {
        CurBlock = 0;
        Transferring = true;
        RequestBlockWrite();
    }
    else
    {
        CurBlock = 0;
        Transferring = true;
        RequestBlockRead();
    }
}

void WriteCmd(u16 val)
{
    Cmd = val;

    // TODO check CMD flags and raise error if needed

    Wifi::SendCommand(Cmd >> 8, Arg);

    SetIRQ(IRQ_CommandDone);
}

void WriteClockCnt(u16 val)
{
    if (!(ClockCnt & (1<<2)))
    {
        ClockCnt = (ClockCnt & 0x00FF) | (val & 0xFF00);
        if (val & (1<<0))
            ClockCnt |= (1<<1);
        if (val & (1<<2))
            ClockCnt |= (1<<2);
    }
    else
    {
        if (!(val & (1<<2)))
        {
            ClockCnt &= ~(1<<2);
            ClockCnt &= ~(1<<1);
        }
    }
}


u8 Read8(u32 addr)
{
    switch (addr & 0xFF)
    {
    case 0x20: return ReadDataBuffer();
    }

    u16 val = Read16(addr & ~1);
    if (addr & 1) return val >> 8;
    else return val & 0xFF;
}

u16 Read16(u32 addr)
{
    switch (addr & 0xFF)
    {
    case 0x0E: return Cmd;
    case 0x10: return Resp[0];
    case 0x12: return Resp[1];
    case 0x14: return Resp[2];
    case 0x16: return Resp[3];
    case 0x18: return Resp[4];
    case 0x1A: return Resp[5];
    case 0x1C: return Resp[6];
    case 0x1E: return Resp[7];
    case 0x20:
        addr = ReadDataBuffer();
        addr |= (ReadDataBuffer() << 8);
        return addr;

    case 0x2C: return ClockCnt;

    case 0x30: return IRQFlags;
    case 0x32: return ErrorIRQFlags;
    case 0x34: return IRQEnable;
    case 0x36: return ErrorIRQEnable;
    case 0x38: return IRQSignalEnable;
    case 0x3A: return ErrorIRQSignalEnable;

    case 0xFE: return 0x8901; // host controller version
    }

    printf("unknown SDIO read16 %08X\n", addr);
    return 0;
}

u32 Read32(u32 addr)
{
    switch (addr & 0xFF)
    {
    case 0x08: return Arg;

    case 0x10: return Resp[0] | (Resp[1] << 16);
    case 0x14: return Resp[2] | (Resp[3] << 16);
    case 0x18: return Resp[4] | (Resp[5] << 16);
    case 0x1C: return Resp[6] | (Resp[7] << 16);
    case 0x20:
        addr = ReadDataBuffer();
        addr |= (ReadDataBuffer() << 8);
        addr |= (ReadDataBuffer() << 16);
        addr |= (ReadDataBuffer() << 24);
        return addr;

    case 0x24: return PresentState;

    case 0x40: return 0x69EF30B0; // capabilities
    case 0x44: return 0;
    }

    printf("unknown SDIO read32 %08X\n", addr);
    return 0;
}

void Write8(u32 addr, u8 val)
{
    switch (addr & 0xFF)
    {
    case 0x20:
        WriteDataBuffer(val);
        return;
    }

    printf("unknown SDIO write8 %08X %02X\n", addr, val);
}

void Write16(u32 addr, u16 val)
{
    switch (addr & 0xFF)
    {
    case 0x04:
        BlockSize = val & 0x3FF;
        if (BlockSize > 0x200) BlockSize = 0x200; // checkme
        return;
    case 0x06:
        BlockCount = val;
        return;
    case 0x0C:
        TransferMode = val;
        return;

    case 0x0E:
        WriteCmd(val);
        return;

    case 0x20:
        WriteDataBuffer(val & 0xFF);
        WriteDataBuffer(val >> 8);
        return;

    case 0x2C:
        WriteClockCnt(val);
        return;

    case 0x30:
        IRQFlags &= ~val;
        return;
    case 0x32:
        ErrorIRQFlags &= ~val;
        return;
    case 0x34:
        IRQEnable = val;
        return;
    case 0x36:
        ErrorIRQEnable = val;
        return;
    case 0x38:
        IRQSignalEnable = val;
        return;
    case 0x3A:
        ErrorIRQSignalEnable = val;
        return;
    }

    printf("unknown SDIO write16 %08X %04X\n", addr, val);
}

void Write32(u32 addr, u32 val)
{
    switch (addr & 0xFF)
    {
    case 0x00:
        DMAAddr = val & 0x3FFFFF;
        return;

    case 0x08:
        Arg = val;
        return;

    case 0x20:
        WriteDataBuffer(val & 0xFF);
        WriteDataBuffer((val >> 8) & 0xFF);
        WriteDataBuffer((val >> 16) & 0xFF);
        WriteDataBuffer(val >> 24);
        return;
    }

    printf("unknown SDIO write32 %08X %08X\n", addr, val);
}

}
