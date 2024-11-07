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
#include "ARM.h"
#include "Platform.h"

using Platform::Log;
using Platform::LogLevel;

// NOTES
// * the ARM926EJ-S in the gamepad has no TCMs
// * also has a MMU (TODO)
// * gamepad firmware uses 1:1 mapping
// * exceptions seem hardwired to reboot the gamepad?

// access timing for cached regions
// this would be an average between cache hits and cache misses
// this was measured to be close to hardware average
// a value of 1 would represent a perfect cache, but that causes
// games to run too fast, causing a number of issues
const int kDataCacheTiming = 3;//2;
const int kCodeCacheTiming = 3;//5;


void ARMv5::CP15Reset()
{
    CP15Control = 0x50078; // dunno

    RNGSeed = 44203;

    memset(ICache, 0, 0x2000);
    ICacheInvalidateAll();
    memset(ICacheCount, 0, 64);

    CurICacheLine = NULL;
}


void ARMv5::UpdateRegionTimings(u32 addrstart, u32 addrend)
{
    for (u32 i = addrstart; i < addrend; i++)
    {
        //u8 pu = PU_Map[i];
        u8* bustimings = WUP::ARM9MemTimings[i >> 2];

        /*if (pu & 0x40)
        {
            MemTimings[i][0] = 0xFF;//kCodeCacheTiming;
        }
        else*/
        {
            MemTimings[i][0] = bustimings[2];
        }

        /*if (pu & 0x10)
        {
            MemTimings[i][1] = kDataCacheTiming;
            MemTimings[i][2] = kDataCacheTiming;
            MemTimings[i][3] = 1;
        }
        else*/
        {
            MemTimings[i][1] = bustimings[0];
            MemTimings[i][2] = bustimings[2];
            MemTimings[i][3] = bustimings[3];
        }
    }
}


u32 ARMv5::RandomLineIndex()
{
    // lame RNG, but good enough for this purpose
    u32 s = RNGSeed;
    RNGSeed ^= (s*17);
    RNGSeed ^= (s*7);

    return (RNGSeed >> 17) & 0x3;
}

void ARMv5::ICacheLookup(u32 addr)
{
    u32 tag = addr & 0xFFFFF800;
    u32 id = (addr >> 5) & 0x3F;

    id <<= 2;
    if (ICacheTags[id+0] == tag)
    {
        CodeCycles = 1;
        CurICacheLine = &ICache[(id+0) << 5];
        return;
    }
    if (ICacheTags[id+1] == tag)
    {
        CodeCycles = 1;
        CurICacheLine = &ICache[(id+1) << 5];
        return;
    }
    if (ICacheTags[id+2] == tag)
    {
        CodeCycles = 1;
        CurICacheLine = &ICache[(id+2) << 5];
        return;
    }
    if (ICacheTags[id+3] == tag)
    {
        CodeCycles = 1;
        CurICacheLine = &ICache[(id+3) << 5];
        return;
    }

    // cache miss

    u32 line;
    if (CP15Control & (1<<14))
    {
        line = ICacheCount[id>>2];
        ICacheCount[id>>2] = (line+1) & 0x3;
    }
    else
    {
        line = RandomLineIndex();
    }

    line += id;

    addr &= ~0x1F;
    u8* ptr = &ICache[line << 5];

    if (CodeMem.Mem)
    {
        memcpy(ptr, &CodeMem.Mem[addr & CodeMem.Mask], 32);
    }
    else
    {
        for (int i = 0; i < 32; i+=4)
            *(u32*)&ptr[i] = WUP::ARM9Read32(addr+i);
    }

    ICacheTags[line] = tag;

    // ouch :/
    //printf("cache miss %08X: %d/%d\n", addr, NDS::ARM9MemTimings[addr >> 14][2], NDS::ARM9MemTimings[addr >> 14][3]);
    CodeCycles = (WUP::ARM9MemTimings[addr >> 14][2] + (WUP::ARM9MemTimings[addr >> 14][3] * 7));
    CurICacheLine = ptr;
}

void ARMv5::ICacheInvalidateByAddr(u32 addr)
{
    u32 tag = addr & 0xFFFFF800;
    u32 id = (addr >> 5) & 0x3F;

    id <<= 2;
    if (ICacheTags[id+0] == tag)
    {
        ICacheTags[id+0] = 1;
        return;
    }
    if (ICacheTags[id+1] == tag)
    {
        ICacheTags[id+1] = 1;
        return;
    }
    if (ICacheTags[id+2] == tag)
    {
        ICacheTags[id+2] = 1;
        return;
    }
    if (ICacheTags[id+3] == tag)
    {
        ICacheTags[id+3] = 1;
        return;
    }
}

void ARMv5::ICacheInvalidateAll()
{
    for (int i = 0; i < 64*4; i++)
        ICacheTags[i] = 1;
}


void ARMv5::CP15Write(u32 id, u32 val)
{
    //if(id!=0x704)printf("CP15 write op %03X %08X %08X\n", id, val, R[15]);

    switch (id)
    {
    case 0x100:
        {
            u32 old = CP15Control;
            val &= 0x000FF085;          // CHECKME
            CP15Control &= ~0x000FF085;
            CP15Control |= val;
            //printf("CP15Control = %08X (%08X->%08X)\n", CP15Control, old, val);
            if (val & (1<<7)) printf("!!!! ARM9 BIG ENDIAN MODE. VERY BAD. SHIT GONNA ASPLODE NOW\n");
            if (val & (1<<13)) ExceptionBase = 0xFFFF0000;
            else               ExceptionBase = 0x00000000;
        }
        return;


    case 0x704:
    case 0x782:
        Halt(1);
        return;


    case 0x750:
        ICacheInvalidateAll();
        //Halt(255);
        return;
    case 0x751:
        ICacheInvalidateByAddr(val);
        //Halt(255);
        return;
    case 0x752:
        //Log(LogLevel::Warn, "CP15: ICACHE INVALIDATE WEIRD. %08X\n", val);
        //Halt(255);
        return;


    case 0x761:
        //printf("inval data cache %08X\n", val);
        return;
    case 0x762:
        //printf("inval data cache SI\n");
        return;

    case 0x7A1:
        //printf("flush data cache %08X\n", val);
        return;
    case 0x7A2:
        //printf("flush data cache SI\n");
        return;


    case 0xF00:
        //printf("cache debug index register %08X\n", val);
        return;

    case 0xF10:
        //printf("cache debug instruction tag %08X\n", val);
        return;

    case 0xF20:
        //printf("cache debug data tag %08X\n", val);
        return;

    case 0xF30:
        //printf("cache debug instruction cache %08X\n", val);
        return;

    case 0xF40:
        //printf("cache debug data cache %08X\n", val);
        return;

    }

    if ((id & 0xF00) == 0xF00) // test/debug shit?
        return;

    if ((id & 0xF00) != 0x700)
        printf("unknown CP15 write op %03X %08X\n", id, val);
}

u32 ARMv5::CP15Read(u32 id)
{
    //printf("CP15 read op %03X %08X\n", id, NDS::ARM9->R[15]);

    switch (id)
    {
    case 0x000: // CPU ID
    case 0x003:
    case 0x004:
    case 0x005:
    case 0x006:
    case 0x007:
        return 0x41069265;

    case 0x001: // cache type
        return 0x1D152152;

    case 0x002: // TCM size
        return 0;


    case 0x100: // control reg
        return CP15Control;
    }

    if ((id & 0xF00) == 0xF00) // test/debug shit?
        return 0;

    printf("unknown CP15 read op %03X\n", id);
    return 0;
}


// TCM are handled here.
// TODO: later on, handle PU, and maybe caches

u32 ARMv5::CodeRead32(u32 addr, bool branch)
{
    /*CodeCycles = RegionCodeCycles;
    if (CodeCycles == 0xFF) // cached memory. hax
    {
        if (branch || !(addr & 0x1F))
            CodeCycles = kCodeCacheTiming;//ICacheLookup(addr);
        else
            CodeCycles = 1;

        //return *(u32*)&CurICacheLine[addr & 0x1C];
    }*/
    CodeCycles = 1;

    //if (CodeMem.Mem) return *(u32*)&CodeMem.Mem[addr & CodeMem.Mask];

    return WUP::ARM9Read32(addr);
}


void ARMv5::DataRead8(u32 addr, u32* val)
{
    DataRegion = addr;

    *val = WUP::ARM9Read8(addr);
    DataCycles = 1;//MemTimings[addr >> 12][1];
}

void ARMv5::DataRead16(u32 addr, u32* val)
{
    DataRegion = addr;

    addr &= ~1;

    *val = WUP::ARM9Read16(addr);
    DataCycles = 1;//MemTimings[addr >> 12][1];
}

void ARMv5::DataRead32(u32 addr, u32* val)
{
    DataRegion = addr;

    addr &= ~3;

    *val = WUP::ARM9Read32(addr);
    DataCycles = 1;//MemTimings[addr >> 12][2];
}

void ARMv5::DataRead32S(u32 addr, u32* val)
{
    addr &= ~3;

    *val = WUP::ARM9Read32(addr);
    DataCycles += 1;//MemTimings[addr >> 12][3];
}

void ARMv5::DataWrite8(u32 addr, u8 val)
{
    DataRegion = addr;

    WUP::ARM9Write8(addr, val);
    DataCycles = 1;//MemTimings[addr >> 12][1];
}

void ARMv5::DataWrite16(u32 addr, u16 val)
{
    DataRegion = addr;

    addr &= ~1;

    WUP::ARM9Write16(addr, val);
    DataCycles = 1;//MemTimings[addr >> 12][1];
}

void ARMv5::DataWrite32(u32 addr, u32 val)
{
    DataRegion = addr;

    addr &= ~3;

    WUP::ARM9Write32(addr, val);
    DataCycles = 1;//MemTimings[addr >> 12][2];
}

void ARMv5::DataWrite32S(u32 addr, u32 val)
{
    addr &= ~3;

    WUP::ARM9Write32(addr, val);
    DataCycles = 1;//MemTimings[addr >> 12][3];
}

void ARMv5::GetCodeMemRegion(u32 addr, WUP::MemRegion* region)
{
    WUP::ARM9GetMemRegion(addr, false, &CodeMem);
}

