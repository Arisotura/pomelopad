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
#include "Video.h"
#include "Platform.h"

using Platform::Log;
using Platform::LogLevel;

namespace Video
{

const int kWidth = 854;
const int kHeight = 480;

u32* Framebuffer;

u32 FBXOffset;
u32 FBWidth;
u32 FBYOffset;
u32 FBHeight;
u32 FBStride;
u32 FBAddr;

u32 DisplayCnt;
u32 PixelFormat;

u32 PaletteAddr;
u32 Palette[256];


bool Init()
{
    Framebuffer = new u32[kWidth * kHeight];
    if (!Framebuffer) return false;

    return true;
}

void DeInit()
{
    delete[] Framebuffer;
}

void Reset()
{
    memset(Framebuffer, 0, kWidth*kHeight*sizeof(u32));

    FBXOffset = 0;
    FBWidth = 0;
    FBYOffset = 0;
    FBHeight = 0;
    FBStride = 0;
    FBAddr = 0;

    DisplayCnt = 0;
    PixelFormat = 0;

    PaletteAddr = 0;
    memset(Palette, 0, sizeof(Palette));
}


void RenderFrame()
{
    // TODO most of the features

    memset(Framebuffer, 0, kWidth*kHeight*sizeof(u32));

    if ((DisplayCnt & 0x12) != 0x12) // display not enabled
        return;

    // TODO add offset (offset depends on other parameters)
    int xstart = 0;
    int ystart = 0;

    int width = (int)FBWidth;
    if ((xstart + width) > kWidth)
        width = kWidth - xstart;

    int height = (int)FBHeight;
    if ((ystart + height) > kHeight)
        height = kHeight - ystart;

    u32 addr = FBAddr;
    u32* dst = &Framebuffer[(ystart * kWidth) + xstart];

    u32 pixelfmt = PixelFormat & 0x3;
    if (pixelfmt == 0)
    {
        for (int y = 0; y < height; y++)
        {
            u32 rowaddr = addr;
            u32* dstrow = dst;

            for (int x = 0; x < width; x++)
            {
                u8 pixel = WUP::MainRAM[rowaddr++ & 0x3FFFFF];
                *dstrow++ = Palette[pixel];
            }

            addr += FBStride;
            dst += kWidth;
        }
    }
    else
    {
        // TODO
    }
}

u32* GetFramebuffer()
{
    return Framebuffer;
}


u32 Read(u32 addr)
{
    switch (addr)
    {
    case 0xF0009460: return FBXOffset;
    case 0xF0009464: return FBWidth;
    case 0xF0009468: return FBYOffset;
    case 0xF000946C: return FBHeight;
    case 0xF0009470: return FBStride;
    case 0xF0009474: return FBAddr;
    case 0xF0009480: return DisplayCnt;
    case 0xF00094B0: return PixelFormat;

    case 0xF0009500: return PaletteAddr;
    case 0xF0009504: return Palette[PaletteAddr];
    }

    printf("unknown video read %08X\n", addr);
    return 0;
}

void Write(u32 addr, u32 val)
{
    switch (addr)
    {
    case 0xF0009460:
        FBXOffset = val;
        return;
    case 0xF0009464:
        FBWidth = val;
        return;
    case 0xF0009468:
        FBYOffset = val;
        return;
    case 0xF000946C:
        FBHeight = val;
        return;
    case 0xF0009470:
        FBStride = val;
        return;
    case 0xF0009474:
        FBAddr = val & 0x3FFFFF;
        return;

    case 0xF0009480:
        printf("DISPLAY CNT = %08X\n", val);
        DisplayCnt = val;
        return;

    case 0xF00094B0:
        PixelFormat = val;
        return;

    case 0xF0009500:
        PaletteAddr = val & 0xFF;
        return;
    case 0xF0009504:
        Palette[PaletteAddr++] = val;
        PaletteAddr &= 0xFF;
        return;
    }

    printf("unknown video write %08X %08X\n", addr, val);
}

}
