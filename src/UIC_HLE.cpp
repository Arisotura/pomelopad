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
#include "UIC.h"
#include "Platform.h"

using Platform::Log;
using Platform::LogLevel;

namespace UIC
{

u8 Cmd;
u32 ByteCount;

u32 CurAddr;

u8 AccessSize;

const u8 FWVersion[4] = {0x28, 0x00, 0x00, 0x58};

u8 EEPROM[0x700];

u8 InputData[0x80];
u8 InputSeq;

// input stuff
bool Touching;
u16 TouchX, TouchY;


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
    ByteCount = 0;

    CurAddr = 0;
    AccessSize = 0;

    memset(InputData, 0, sizeof(InputData));
    InputSeq = 0;

    FILE* f = fopen("uic_config.bin", "rb");
    if (f)
    {
        fread(EEPROM, 0x700, 1, f);
        fclose(f);
        printf("UIC_HLE: loaded config data\n");
    }
    else
        printf("UIC_HLE: failed to open uic_config.bin\n");

    // pretend the battery is all good
    InputData[0x04] = 0x21;

    Touching = false;
    TouchX = 0;
    TouchY = 0;
}


void SetTouchCoords(bool touching, int x, int y)
{
    Touching = touching;
    if (touching)
    {
        if (x < 0) x = 0;
        else if (x > 853) x = 853;
        if (y < 0) y = 0;
        else if (y > 479) y = 479;

        TouchX = ((x << 12) / 854);
        TouchY = 0xFFF - ((y << 12) / 480);
    }
    else
    {
        TouchX = 0;
        TouchY = 0;
    }
}


void PrepareInputData()
{
    InputData[0] = FWVersion[0];
    InputData[1] = InputSeq++;
    InputData[127] = ~InputData[0];

    // touch data
    u8* touchdata = &InputData[0x24];
    if (Touching)
    {
        for (int i = 0; i < 10; i++)
        {
            u16 x = TouchX;
            u16 y = TouchY;

            x |= 0x8000;
            if (i != 9) y |= 0x8000;

            *touchdata++ = (x & 0xFF);
            *touchdata++ = (x >> 8);
            *touchdata++ = (y & 0xFF);
            *touchdata++ = (y >> 8);
        }
    }
    else
    {
        memset(touchdata, 0, 40);
    }
}


void Select()
{
    Cmd = 0;
    ByteCount = 0;
}

void Release()
{
}

u8 Read()
{
    if (ByteCount == 0) return 0;

    switch (Cmd)
    {
    case 0x03: // read EEPROM
        if (AccessSize)
        {
            AccessSize--;
            u32 addr = CurAddr - 0x1100;
            CurAddr++;
            if (addr < 0x700) return EEPROM[addr];
        }
        return 0;

    case 0x05: // get UIC state
        return 5;
        return 0; // normal
        return 7; // debug mode

    case 0x07: // get input data
        if (CurAddr < 0x80) return InputData[CurAddr++];
        return 0;

    case 0x0B: // get firmware version
        if (CurAddr < 4) return FWVersion[CurAddr++];
        return 0;

    case 0x13: // ??
        return 1;

    case 0x7F: // firmware ID/type of sorts
        return 0x3F;
    }

    return 0;
}

void Write(u8 val)
{
    if (ByteCount == 0)
    {
        Cmd = val;
        printf("UIC: cmd %02X\n", val);

        switch (Cmd)
        {
        case 0x03:
            CurAddr = 0;
            AccessSize = 0;
            break;

        case 0x07:
            CurAddr = 0;
            PrepareInputData();
            break;

        case 0x0B:
            CurAddr = 0;
            break;
        }

        ByteCount++;
        return;
    }

    switch (Cmd)
    {
    case 0x01:
        printf("UIC state = %d\n", val);
        // TODO
        break;

    case 0x03:
        if (ByteCount == 1) CurAddr = val << 8;
        else if (ByteCount == 2) CurAddr |= val;
        else if (ByteCount == 3)
        {
            AccessSize = val;
            if (CurAddr < 0x1100 || (CurAddr + AccessSize) > 0x1800)
                AccessSize = 0;
        }
        break;
    }

    ByteCount++;
}

}
