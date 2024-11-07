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
#include "UART.h"
#include "Platform.h"

using Platform::Log;
using Platform::LogLevel;

namespace UART
{

bool datain;


bool Init()
{
    return true;
}

void DeInit()
{
}

void Reset()
{
    datain = false;
}


void hax(u32 param)
{
    printf("IRQ04\n");
    WUP::SetIRQ(0x04);
}


u32 Read(u32 addr)
{
    // F0004C78: probably, bit8-12 = send FIFO level
    switch (addr)
    {
    case 0xF0004C4C: return 2;
    case 0xF0004C5C: return 1;
    case 0xF0004C78: return 0;
    }

    printf("unknown UART read %08X @ %08X\n", addr, WUP::GetPC());
    return 0;
}

void Write(u32 addr, u32 val)
{
    switch (addr)
    {
    case 0xF0004C44:
        // data input
        val &= 0xFF;
        if (val != 0x0D)
            putchar(val);
        datain = true;
        return;

    case 0xF0004C48:
        // control register of sorts
        printf("UART ctl = %08X @ %08X\n", val, WUP::GetPC());
        if (val == 5)
        {
            //if (datain) WUP::ScheduleEvent(WUP::Event_UART, false, 1024, hax, 0);
            datain = false;
        }
        return;
    }

    printf("unknown UART write %08X %08X\n", addr, val);
}

}
