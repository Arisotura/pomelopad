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

#ifndef SDIO_H
#define SDIO_H

#include "types.h"

namespace SDIO
{

enum
{
    IRQ_CommandDone = 0,
    IRQ_TransferDone,
    IRQ_BlockGap,
    IRQ_DMA,
    IRQ_DataWriteReady,
    IRQ_DataReadReady,
    IRQ_CardInsert,
    IRQ_CardRemove,
    IRQ_CardInterrupt,
    IRQ_Error = 15
};

enum
{
    Error_CmdTimeout = 0,
    Error_CmdCRC,
    Error_CmdEndBit,
    Error_CmdIndex,
    Error_DataTimeout,
    Error_DataCRC,
    Error_DataEndBit,
    Error_CurrentLimit,
    Error_AutoCmd12,
    Error_Vendor = 12
};

bool Init();
void DeInit();
void Reset();

void SetIRQ(int irq);
void SetErrorIRQ(int irq);

void SendResponse(u32 resp);
void StartTransfer(bool write);

u8 Read8(u32 addr);
u16 Read16(u32 addr);
u32 Read32(u32 addr);
void Write8(u32 addr, u8 val);
void Write16(u32 addr, u16 val);
void Write32(u32 addr, u32 val);

}

#endif // SDIO_H
