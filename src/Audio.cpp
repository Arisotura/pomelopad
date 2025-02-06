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
#include "Audio.h"
#include "Platform.h"

using Platform::Log;
using Platform::LogLevel;

namespace Audio
{

/*
 * register mask:
 *  F0005400:  01FFFFFF 0000FFFF 0FFFFFF8 0FFFFFF8
    F0005410:  0FFFFFF8 00000000 0000FFFF 0000001F
    F0005420:  00000103 01FFFFFF 00000000 0000003F
    F0005430:  00000000 00008008 00000000 00000000
    F0005440:  00000000 C01FF3FD 01FFFFFF 00000000
    F0005450:  00000000 00000000 00000000 00000000
    F0005460:  00000000 00000000 00000000 00000000
    F0005470:  00000000 00000000 00000000 00000000
    F0005480:  00000000 00000000 00000000 00000000
    F0005490:  00000000 00000000 00000000 00000000
    F00054A0:  01FFFFFF 01FFFFFF 01FFFFFF 00000000
    F00054B0:  00000000 00000000 10100000 00000000
    F00054C0:  00000000 00000111 00000000 00000000
    F00054D0:  00000000 00000000 00000000 00000000
    F00054E0:  00000000 00000000 00000000 00000000
    F00054F0:  00000000 00000000 00000000 00000000
 */

// F0005400 = 8000
// F0005404 = 1F00
// F000542C = F
// F00054B8 = 10100000
// F00054C4 = 111
// rest is 0

u32 Unk00, Unk04;
u32 OutBufStart, OutBufEnd;
u32 OutBufNew, OutBufPos;
u32 Unk18, Unk1C;
u32 Unk20;
u32 EndAdvance;
u32 IRQEnable;
u32 IRQStatus;
u32 Unk34;
u32 Unk44;

u32 UnkA0, UnkA4, UnkA8;
bool playing;


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
    Unk00 = 0x8000;
    Unk04 = 0x1F00;
    OutBufStart = 0;
    OutBufEnd = 0;
    OutBufNew = 0;
    OutBufPos = 0;
    Unk18 = 0;
    Unk1C = 0;
    Unk20 = 0;
    EndAdvance = 0;
    IRQEnable = 0xF;
    IRQStatus = 0;
    Unk34 = 0;
    Unk44 = 0;

    UnkA0 = 0;
    UnkA4 = 0;
    UnkA8 = 0;
    playing = false;
}


void SetIRQ(int irq)
{
    if (!(IRQEnable & (1<<irq)))
        return;

    IRQStatus |= (1<<irq);
    WUP::SetIRQ(0x17);
}


void framehack()
{
    if (playing)
    {
        SetIRQ(3);
        playing = false;
        //printf("end of playback IRQ\n");
    }
}


u32 Read(u32 addr)
{
    //if (addr != 0xF00054B0)
    //    printf("audio read %08X  @ %08X\n", addr, WUP::GetPC());

    switch (addr)
    {
    case 0xF0005400: return Unk00;
    case 0xF0005404: return Unk04;
    case 0xF0005408: return OutBufStart;
    case 0xF000540C: return OutBufEnd;
    case 0xF0005410: return OutBufNew;
    case 0xF0005414: return OutBufPos;
    case 0xF0005418: return Unk18;
    case 0xF000541C: return Unk1C;
    case 0xF0005420: return Unk20;
    case 0xF0005424: return EndAdvance;
    case 0xF000542C: return IRQEnable;
    case 0xF0005430: return IRQStatus;
    case 0xF0005434: return Unk34;
    case 0xF0005444: return Unk44;

    case 0xF00054A0: return UnkA0;
    case 0xF00054A4: return UnkA4;
    case 0xF00054A8: return UnkA8;
    }

    //printf("unknown audio read %08X\n", addr);
    return 0;
}

void Write(u32 addr, u32 val)
{
    //printf("audio write %08X %08X  @ %08X\n", addr, val, WUP::GetPC());

    switch (addr)
    {
    case 0xF0005400:
        Unk00 = val;
        return;

    case 0xF0005404: Unk04 = val; return;
    case 0xF0005408: OutBufStart = val; return;
    case 0xF000540C: OutBufEnd = val; return;
    case 0xF0005410: OutBufNew = val; OutBufPos = val; playing = true; return;
    case 0xF0005418: Unk18 = val; return;
    case 0xF000541C: Unk1C = val; return;

    case 0xF0005420:
        Unk20 = val;
        if (val & (1<<2))
        {
            //Unk34 |= (1<<2);
            SetIRQ(2);
            playing = false;
        }
        return;

    case 0xF0005424: EndAdvance = val; return;
    case 0xF000542C: IRQEnable = val & 0x3F; return;
    case 0xF0005430: IRQStatus &= ~val; return;

    case 0xF0005444:
        Unk44 = val;
        return;

    case 0xF00054A0: UnkA0 = val; return;
    case 0xF00054A4: UnkA4 = val; return;
    case 0xF00054A8: UnkA8 = val; return;
    }

    //printf("unknown audio write %08X %08X\n", addr, val);
}

}
