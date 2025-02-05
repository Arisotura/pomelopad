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
#include "Flash.h"
#include "Platform.h"

using Platform::Log;
using Platform::LogLevel;

namespace Flash
{

u8 Cmd;
u32 ByteCount;

u8 StatusReg;
u8 AddrLen;

const u32 kSize = 32*1024*1024;
const u32 kAddrMask = kSize-1;
u8* Data;
u32 CurAddr;

const u8 ChipID[20] = {0x20, 0xBA, 0x19, 0x10, 0x00, 0x00, 0x23, 0x21,
                       0x61, 0x34, 0x07, 0x00, 0x30, 0x00, 0x17, 0x17,
                       0x06, 0x12, 0x4B, 0x01};

u8 WriteBuffer[0x100];
u8 WriteStart;
u32 WriteLen;


bool Init()
{
    Data = new u8[kSize];
    return true;
}

void DeInit()
{
    delete[] Data;
}

void Reset()
{
    Cmd = 0;
    ByteCount = 0;

    StatusReg = 0;
    AddrLen = 3;

    CurAddr = 0;

    memset(WriteBuffer, 0, 0x100);
    WriteStart = 0;
    WriteLen = 0;
}


bool LoadFirmware(const char* filename)
{
    FILE* f = fopen(filename, "rb");
    if (!f)
    {
        printf("failed to open firmware\n");
        return false;
    }

    fseek(f, 0, SEEK_SET);
    fread(Data, kSize, 1, f);
    fclose(f);

    // HACK
    // TODO: do this more nicely
    // language bank in firmware should match UIC setting (or vice versa)
    memcpy(&Data[0x1100000], &Data[0x900000], 0x800000);

    /*memcpy(&Data[0x0100000], &Data[0x1C00000], 0x400000);
    Data[0xF000] = 0;
    // B5FE8 -> nop 0100 0110 1100 0000 46C0
    *(u16*)&Data[0x100074+0xB5FE8] = 0x46C0;
    *(u16*)&Data[0x100074+0xB6096] = 0x46C0;
    f = fopen("fwdebug.bin", "wb");
    fwrite(Data, kSize, 1, f);
    fclose(f);*/

    return true;
}

bool LoadBootAndFw(const char* boot, const char* fw)
{
    memset(Data, 0xFF, kSize);

    FILE* f;
    u32 len;

    f = fopen(boot, "rb");
    if (!f)
    {
        printf("failed to open bootloader\n");
        return false;
    }

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    if (len > 0xE000)
        len = 0xE000;
    fseek(f, 0, SEEK_SET);
    fread(Data, len, 1, f);
    fclose(f);

    Data[0xF000] = 0;
    u32 fwoffset = 0x100000;

    f = fopen(fw, "rb");
    if (!f)
    {
        printf("failed to open firmware\n");
        return false;
    }

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    if ((fwoffset + len) > kSize)
        len = kSize - fwoffset;
    fseek(f, 0, SEEK_SET);
    fread(&Data[fwoffset], len, 1, f);
    fclose(f);

    return true;
}

void SetupBootloader()
{
    u32 bootsize = *(u32*)&Data[0];
    printf("bootloader size: %08X\n", bootsize);

    // exception vectors
    memcpy(&WUP::MainRAM[0], &Data[4], 0x40);

    // bootloader
    memcpy(&WUP::MainRAM[0x3F0000], &Data[0x44], bootsize);
}


void Select()
{
    Cmd = 0;
    ByteCount = 0;
    CurAddr = 0;
}

void Release()
{
    bool writeback = false;

    if (Cmd == 0x02)
    {
        // page program
        u32 addr = CurAddr;
        for (u32 i = 0; i < WriteLen; i++)
        {
            u8 offset = WriteStart + i;
            Data[addr] = WriteBuffer[offset];
            addr = (addr & ~0xFF) | ((addr + 1) & 0xFF);
        }

        writeback = true;
    }
    else if (Cmd == 0x20)
    {
        // subsector erase
        u32 start = CurAddr & ~0xFFF;
        for (u32 i = start; i < start+0x1000; i++)
            Data[i] = 0xFF;

        writeback = true;
    }

    if (writeback)
    {
        StatusReg &= ~(1<<1);

        // TODO
    }
}

u8 Read()
{
    if (ByteCount == 0) return 0;

    switch (Cmd)
    {
    case 0x03:
        if (ByteCount <= AddrLen) return 0;
        return Data[CurAddr++ & kAddrMask];

    case 0x05: return StatusReg;

    case 0x9F:
        if (CurAddr < 20) return ChipID[CurAddr++];
        return 0;
    }

    return 0;
}

void Write(u8 val)
{
    if (ByteCount == 0)
    {
        Cmd = val;
        printf("FLASH: cmd %02X\n", val);

        switch (Cmd)
        {
        case 0x02:
            if (!(StatusReg & (1<<1))) Cmd = 0;
            WriteStart = 0;
            WriteLen = 0;
            break;

        case 0x04: StatusReg &= ~(1<<1); break;
        case 0x06: StatusReg |= (1<<1); break;

        case 0x20:
            if (!(StatusReg & (1<<1))) Cmd = 0;
            break;

        case 0xB7: AddrLen = 4; break;
        case 0xE9: AddrLen = 3; break;
        }

        ByteCount++;
        return;
    }

    switch (Cmd)
    {
    case 0x02:
        if (ByteCount <= AddrLen)
            CurAddr = (CurAddr << 8) | val;
        else
        {
            u8 offset = WriteStart + WriteLen;
            WriteBuffer[offset] = val;
            if (WriteLen == 0x100)
                WriteStart++;
            else
                WriteLen++;
        }
        printf("SPI02: byte=%d len=%d addr=%08X\n", ByteCount, AddrLen, CurAddr);
        break;

    case 0x03:
        if (ByteCount <= AddrLen)
            CurAddr = (CurAddr << 8) | val;
        printf("SPI03: byte=%d len=%d addr=%08X\n", ByteCount, AddrLen, CurAddr);
        break;

    case 0x20:
        if (ByteCount <= AddrLen)
            CurAddr = (CurAddr << 8) | val;
        printf("SPI20: byte=%d len=%d addr=%08X\n", ByteCount, AddrLen, CurAddr);
        break;
    }

    ByteCount++;
}

}
