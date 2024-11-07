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
#include "AudioAmp.h"
#include "Platform.h"

using Platform::Log;
using Platform::LogLevel;

namespace AudioAmp
{

u8 RegAddr;
bool GotAddr;


bool Init()
{
    return true;
}

void DeInit()
{
}

void Reset()
{
    RegAddr = 0;
    GotAddr = false;
}


void Start()
{
    GotAddr = false;
}

void Stop()
{
}

u8 Read()
{
    switch (RegAddr)
    {
    case 0x30: return 0x20;
    }

    return 0;
}

void Write(u8 val)
{
    if (!GotAddr)
    {
        RegAddr = val;
        GotAddr = true;
        printf("AudioAmp: I2C command %02X\n", val);

        return;
    }
}

}
