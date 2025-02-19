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

#ifndef SPI_H
#define SPI_H

#include "types.h"

namespace SPI
{

bool Init();
void DeInit();
void Reset();

void SetCnt(u32 val);
void UpdateChipSelect();

void ScheduleTransfer(bool write);
void StartRead();
void WriteData(u8 val);
u8 ReadData();
void OnWrite(u32 param);
void OnRead(u32 param);

u32 Read(u32 addr);
void Write(u32 addr, u32 val);

void StartDMA(bool write);
/*bool DMAStart(bool write);
void DMAFinish();
u8 DMARead();
void DMAWrite(u8 val);*/

}

#endif // SPI_H
