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
#include "LCD.h"
#include "Platform.h"

using Platform::Log;
using Platform::LogLevel;

namespace LCD
{

u8 Cmd;
bool GotCmd;
u32 CurAddr;

const u8 ID[5] = {0x01, 0x22, 0x92, 0x08, 0xA0};

u8 Status;


bool Init()
{
    return true;
}

void DeInit()
{
}

void Reset()
{
    Cmd = 0;
    GotCmd = false;
    CurAddr = 0;

    Status = 0;
}


void Start()
{
    CurAddr = 0;
}

void Stop()
{
    Cmd = 0;
    GotCmd = false;
}

u8 Read()
{
    switch (Cmd)
    {
    case 0xBF:
        if (CurAddr < 5)
            return ID[CurAddr++];
        return 0;

    case 0x0A:
        return Status;
    }

    return 0;
}

void Write(u8 val)
{
    if (!GotCmd)
    {
        Cmd = val;
        GotCmd = true;

        switch (Cmd)
        {
        case 0x28: Status &= ~(1<<2); break;
        case 0x29: Status |= (1<<2); break;
        }

        return;
    }
}

}
