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
#include <inttypes.h>
#include "WUP.h"
#include "ARM.h"
#include "DMA.h"
#include "SPI.h"
#include "Flash.h"
#include "UIC.h"
#include "UART.h"
#include "I2C.h"
#include "AudioAmp.h"
#include "Camera.h"
#include "LCD.h"
#include "Video.h"
#include "SDIO.h"
#include "Wifi.h"
#include "Platform.h"

using Platform::Log;
using Platform::LogLevel;


namespace WUP
{

u8 ARM9MemTimings[0x40000][8];
u32 ARM9Regions[0x40000];

ARMv5* ARM9;

u32 NumFrames;
u32 NumLagFrames;
bool LagFrameFlag;
u64 LastSysClockCycles;
u64 FrameStartTimestamp;

const s32 kMaxIterationCycles = 64;
const s32 kIterationCycleMargin = 8;


// no need to worry about those overflowing, they can keep going for atleast 4350 years
u64 ARM9Timestamp, ARM9Target;
u64 SysTimestamp;

SchedEvent SchedList[Event_MAX];
u32 SchedListMask;

u8 MainRAM[0x400000];

u32 SoftResetReg;


u8 IRQEnable[0x28];
u64 IRQMask;
u32 CurrentIRQ;
u32 IRQPriority;
u32 LastIRQPriority;


u64 TimerTimestamp;

// 0 = timer 0/1, 1 = count-up
u32 TimerPrescaler[2];
u32 TimerCounter[2];

u32 CountUpVal;

u32 TimerCnt[2];
u32 TimerTarget[2];
u32 TimerVal[2];
u32 TimerSubCounter[2];

bool Running;


bool Init()
{
    ARM9 = new ARMv5();

    if (!DMA::Init()) return false;

    if (!Flash::Init()) return false;
    if (!UIC::Init()) return false;
    if (!SPI::Init()) return false;

    if (!UART::Init()) return false;

    if (!AudioAmp::Init()) return false;
    if (!Camera::Init()) return false;
    if (!LCD::Init()) return false;
    if (!I2C::Init()) return false;

    if (!Video::Init()) return false;

    if (!SDIO::Init()) return false;
    if (!Wifi::Init()) return false;

    return true;
}

void DeInit()
{
    Wifi::DeInit();
    SDIO::DeInit();

    Video::DeInit();

    AudioAmp::DeInit();
    Camera::DeInit();
    LCD::DeInit();
    I2C::DeInit();

    UART::DeInit();

    SPI::DeInit();
    Flash::DeInit();
    UIC::DeInit();

    DMA::DeInit();

    delete ARM9;
}


void SetARM9RegionTimings(u32 addrstart, u32 addrend, u32 region, int buswidth, int nonseq, int seq)
{
    addrstart >>= 2;
    addrend   >>= 2;

    int N16, S16, N32, S32, cpuN;
    N16 = nonseq;
    S16 = seq;
    if (buswidth == 16)
    {
        N32 = N16 + S16;
        S32 = S16 + S16;
    }
    else
    {
        N32 = N16;
        S32 = S16;
    }

    // nonseq accesses on the CPU get a 3-cycle penalty for all regions except main RAM
    cpuN = (region == Mem9_MainRAM) ? 0 : 3;

    for (u32 i = addrstart; i < addrend; i++)
    {
        // CPU timings
        ARM9MemTimings[i][0] = N16 + cpuN;
        ARM9MemTimings[i][1] = S16;
        ARM9MemTimings[i][2] = N32 + cpuN;
        ARM9MemTimings[i][3] = S32;

        // DMA timings
        ARM9MemTimings[i][4] = N16;
        ARM9MemTimings[i][5] = S16;
        ARM9MemTimings[i][6] = N32;
        ARM9MemTimings[i][7] = S32;

        ARM9Regions[i] = region;
    }

    ARM9->UpdateRegionTimings(addrstart<<2, addrend<<2);
}

void InitTimings()
{
    // TODO!

    SetARM9RegionTimings(0x00000, 0x100000, 0, 32, 1, 1); // void

    //
}

void Reset()
{
    LastSysClockCycles = 0;

    // has to be called before InitTimings
    // otherwise some PU settings are completely
    // unitialised on the first run
    ARM9->CP15Reset();

    ARM9Timestamp = 0; ARM9Target = 0;
    SysTimestamp = 0;

    InitTimings();

    memset(MainRAM, 0, 0x400000);
    SoftResetReg = 1;

    ARM9->Reset();

    memset(IRQEnable, 0, sizeof(IRQEnable));
    IRQMask = 0;
    CurrentIRQ = IRQ_None;
    IRQPriority = 0;

    TimerTimestamp = 0;
    TimerPrescaler[0] = 0;
    TimerPrescaler[1] = 0;
    TimerCounter[0] = 0;
    TimerCounter[1] = 0;
    CountUpVal = 0;
    TimerCnt[0] = 0;
    TimerTarget[0] = 0;
    TimerVal[0] = 0;
    TimerCounter[0] = 0;
    TimerCnt[1] = 0;
    TimerTarget[1] = 0;
    TimerVal[1] = 0;
    TimerCounter[1] = 0;

    memset(SchedList, 0, sizeof(SchedList));
    SchedListMask = 0;

    DMA::Reset();

    Flash::Reset();
    UIC::Reset();
    SPI::Reset();

    UART::Reset();

    AudioAmp::Reset();
    Camera::Reset();
    LCD::Reset();
    I2C::Reset();

    Video::Reset();

    SDIO::Reset();
    Wifi::Reset();
}

void Start()
{
    Running = true;
}

bool LoadFirmware(const char* filename)
{
    Reset();

    if (!Flash::LoadFirmware(filename))
        return false;

    Flash::SetupBootloader();
    return true;
}


u64 NextTarget()
{
    u64 minEvent = UINT64_MAX;

    u32 mask = SchedListMask;
    for (int i = 0; i < Event_MAX; i++)
    {
        if (!mask) break;
        if (mask & 0x1)
        {
            if (SchedList[i].Timestamp < minEvent)
                minEvent = SchedList[i].Timestamp;
        }

        mask >>= 1;
    }

    u64 max = SysTimestamp + kMaxIterationCycles;

    if (minEvent < max + kIterationCycleMargin)
        return minEvent;

    return max;
}

void RunSystem(u64 timestamp)
{
    SysTimestamp = timestamp;

    u32 mask = SchedListMask;
    for (int i = 0; i < Event_MAX; i++)
    {
        if (!mask) break;
        if (mask & 0x1)
        {
            if (SchedList[i].Timestamp <= SysTimestamp)
            {
                SchedListMask &= ~(1<<i);
                SchedList[i].Func(SchedList[i].Param);
            }
        }

        mask >>= 1;
    }
}


u32 RunFrame()
{
    FrameStartTimestamp = SysTimestamp;

    // 16MHz = ~279620 cycles per frame
    // THIS IS VERY ROUGH AND LAME
    // also TODO add changeable clocks and shit
    // PLL settings applied by second stage bootloader change the clock fo 108MHz
    const u32 framecnt = 279620;
    u64 frametarget = SysTimestamp + framecnt;

    LagFrameFlag = true;
    bool runFrame = Running;// && !(CPUStop & 0x40000000);
    if (runFrame)
    {
        //GPU::StartFrame();

        while (Running)// && GPU::TotalScanlines==0)
        {
            u64 target = NextTarget();
            ARM9Target = target;

            ARM9->Execute();

            RunTimers();

            target = ARM9Timestamp;

            RunSystem(target);
            if (SysTimestamp >= frametarget) break;
        }

        //SPU::TransferOutput();

        // TODO: this should be done on VBlank
        SetIRQ(0x16);
        Video::RenderFrame();
    }

    // In the context of TASes, frame count is traditionally the primary measure of emulated time,
    // so it needs to be tracked even if NDS is powered off.
    NumFrames++;
    if (LagFrameFlag)
        NumLagFrames++;

    return 1;
}

void Reschedule(u64 target)
{
    {
        if (target < ARM9Target)
            ARM9Target = target;
    }
}

void ScheduleEvent(u32 id, bool periodic, s32 delay, void (*func)(u32), u32 param)
{
    if (SchedListMask & (1<<id))
    {
        Log(LogLevel::Debug, "!! EVENT %d ALREADY SCHEDULED\n", id);
        return;
    }

    SchedEvent* evt = &SchedList[id];

    if (periodic)
        evt->Timestamp += delay;
    else
    {
        evt->Timestamp = ARM9Timestamp + delay;
    }

    evt->Func = func;
    evt->Param = param;

    SchedListMask |= (1<<id);

    Reschedule(evt->Timestamp);
}

void ScheduleEvent(u32 id, u64 timestamp, void (*func)(u32), u32 param)
{
    if (SchedListMask & (1<<id))
    {
        Log(LogLevel::Debug, "!! EVENT %d ALREADY SCHEDULED\n", id);
        return;
    }

    SchedEvent* evt = &SchedList[id];

    evt->Timestamp = timestamp;
    evt->Func = func;
    evt->Param = param;

    SchedListMask |= (1<<id);

    Reschedule(evt->Timestamp);
}

void CancelEvent(u32 id)
{
    SchedListMask &= ~(1<<id);
}


u32* GetFramebuffer()
{
    return Video::GetFramebuffer();
}


void SoftReset()
{
    // TODO: research the implications of the soft reset event
    // not sure if it's supposed to disable IRQs this way

    for (int i = 0; i < 0x28; i++)
        IRQEnable[i] |= (1<<6);

    TimerCnt[0] = 0;
    TimerCnt[1] = 0;

    ARM9->SoftReset();
    printf("soft reset\n");
    for (int i = 0; i < 0x80; i+=4)
    {
        printf("%08X ", *(u32*)&MainRAM[i]);
        if ((i&0xC)==0xC) printf("\n");
    }
}


/*void TouchScreen(u16 x, u16 y)
{
    if (ConsoleType == 1)
    {
        DSi_SPI_TSC::SetTouchCoords(x, y);
    }
    else
    {
        SPI_TSC::SetTouchCoords(x, y);
        KeyInput &= ~(1 << (16+6));
    }
}

void ReleaseScreen()
{
    if (ConsoleType == 1)
    {
        DSi_SPI_TSC::SetTouchCoords(0x000, 0xFFF);
    }
    else
    {
        SPI_TSC::SetTouchCoords(0x000, 0xFFF);
        KeyInput |= (1 << (16+6));
    }
}


void SetKeyMask(u32 mask)
{
    u32 key_lo = mask & 0x3FF;
    u32 key_hi = (mask >> 10) & 0x3;

    KeyInput &= 0xFFFCFC00;
    KeyInput |= key_lo | (key_hi << 16);
}

void CamInputFrame(int cam, u32* data, int width, int height, bool rgb)
{
    // TODO: support things like the GBA-slot camera addon
    // whenever these are emulated

    if (ConsoleType == 1)
    {
        switch (cam)
        {
        case 0: return DSi_CamModule::Camera0->InputFrame(data, width, height, rgb);
        case 1: return DSi_CamModule::Camera1->InputFrame(data, width, height, rgb);
        }
    }
}

void MicInputFrame(s16* data, int samples)
{
    return SPI_TSC::MicInputFrame(data, samples);
}
*/


void Halt()
{
    //Log(LogLevel::Info, "Halt()\n");
    Running = false;
}


void UpdateIRQ()
{
    if (CurrentIRQ != IRQ_None)
        return;

    for (u32 prio = 0; prio < IRQPriority; prio++)
    {
        for (u32 irq = 0; irq < 0x28; irq++)
        {
            u8 enable = IRQEnable[irq];
            if (enable & (1<<6))
                continue;
            if ((enable & 0xF) != prio)
                continue;
            if (!(IRQMask & (1ULL << irq)))
                continue;

            CurrentIRQ = irq;
            LastIRQPriority = IRQPriority;
            ARM9->IRQ = 1;
            return;
        }
    }

    CurrentIRQ = IRQ_None;
    ARM9->IRQ = 0;
}

void SetIRQ(u32 irq)
{
    // TODO: what does the IRQ trigger type do? (F0001420)
    // TODO: what happens if the enable/prio register is changed after the IRQ triggered?

    u8 enable = IRQEnable[irq];
    if (enable & (1<<6))
        return;

    IRQMask |= (1ULL << irq);
    UpdateIRQ();
}

void AcknowledgeIRQ(u32 prio)
{
    IRQPriority = prio;
    if (CurrentIRQ != IRQ_None)
    {
        IRQMask &= ~(1ULL << CurrentIRQ);
        CurrentIRQ = IRQ_None;
        ARM9->IRQ = 0;
        UpdateIRQ();
    }
}

bool HaltInterrupted()
{
    return (CurrentIRQ != IRQ_None);
}


u32 GetPC()
{
    return ARM9->R[15];
}

u64 GetSysClockCycles(int num)
{
    u64 ret;

    if (num == 0 || num == 2)
    {
        ret = ARM9Timestamp;

        if (num == 2) ret -= FrameStartTimestamp;
    }
    else if (num == 1)
    {
        ret = LastSysClockCycles;
        LastSysClockCycles = 0;

        LastSysClockCycles = ARM9Timestamp;
    }

    return ret;
}



void TickTimer(int timer)
{
    if (!(TimerCnt[timer] & (1<<1)))
        return;

    u32 prescaler = 2 << ((TimerCnt[timer] >> 4) & 0x7);
    TimerSubCounter[timer]++;
    if (TimerSubCounter[timer] >= prescaler)
    {
        TimerSubCounter[timer] = 0;

        TimerVal[timer]++;
        if (TimerVal[timer] > TimerTarget[timer])
        {
            TimerVal[timer] = 0;
            SetIRQ(IRQ_Timer0 + timer);
        }
    }
}

void RunTimers()
{
    u32 cycles = (u32)(ARM9Timestamp - TimerTimestamp);
    TimerTimestamp = ARM9Timestamp;

    if (TimerPrescaler[0] > 0)
    {
        TimerCounter[0] += cycles;
        while (TimerCounter[0] > TimerPrescaler[0])
        {
            TimerCounter[0] -= TimerPrescaler[0];
            TickTimer(0);
            TickTimer(1);
        }
    }
    else
    {
        for (u32 i = 0; i < cycles; i++)
        {
            TickTimer(0);
            TickTimer(1);
        }
    }

    if (TimerPrescaler[1] > 0)
    {
        TimerCounter[1] += cycles;
        while (TimerCounter[1] > TimerPrescaler[1])
        {
            TimerCounter[1] -= TimerPrescaler[1];
            CountUpVal++;
        }
    }
    else
    {
        CountUpVal += cycles;
    }
}




void debug(u32 param)
{
    //
}



u8 ARM9Read8(u32 addr)
{
    if (addr < 0x40000000)
    {
        return *(u8*)&MainRAM[addr & 0x3FFFFF];
    }
    if (addr >= 0xE0010000 && addr < 0xE0020000)
    {
        return SDIO::Read8(addr);
    }
    if (addr >= 0xF0000000)
    {
        return ARM9IORead8(addr);
    }

    printf("unknown read8 %08X @ %08X\n", addr, ARM9->R[15]);
    return 0;
}

u16 ARM9Read16(u32 addr)
{
    addr &= ~0x1;

    if (addr < 0x40000000)
    {
        return *(u16*)&MainRAM[addr & 0x3FFFFF];
    }
    if (addr >= 0xE0010000 && addr < 0xE0020000)
    {
        return SDIO::Read16(addr);
    }
    if (addr >= 0xF0000000)
    {
        return ARM9IORead16(addr);
    }

    printf("unknown read16 %08X @ %08X\n", addr, ARM9->R[15]);
    return 0;
}

u32 ARM9Read32(u32 addr)
{
    addr &= ~0x3;

    if (addr < 0x40000000)
    {
        return *(u32*)&MainRAM[addr & 0x3FFFFF];
    }
    if (addr >= 0xE0010000 && addr < 0xE0020000)
    {
        return SDIO::Read32(addr);
    }
    if (addr >= 0xF0000000)
    {
        return ARM9IORead32(addr);
    }

    printf("unknown read32 %08X @ %08X\n", addr, ARM9->R[15]);
    return 0;
}

void ARM9Write8(u32 addr, u8 val)
{
    if (addr < 0x40000000)
    {
        *(u8*)&MainRAM[addr & 0x3FFFFF] = val;
        return;
    }
    if (addr >= 0xE0010000 && addr < 0xE0020000)
    {
        SDIO::Write8(addr, val);
        return;
    }
    if (addr >= 0xF0000000)
    {
        return ARM9IOWrite8(addr, val);
    }

    printf("unknown write8 %08X %02X @ %08X\n", addr, val, ARM9->R[15]);
}

void ARM9Write16(u32 addr, u16 val)
{
    addr &= ~0x1;

    if (addr < 0x40000000)
    {
        *(u16*)&MainRAM[addr & 0x3FFFFF] = val;
        return;
    }
    if (addr >= 0xE0010000 && addr < 0xE0020000)
    {
        SDIO::Write16(addr, val);
        return;
    }
    if (addr >= 0xF0000000)
    {
        return ARM9IOWrite16(addr, val);
    }

    printf("unknown write16 %08X %04X @ %08X\n", addr, val, ARM9->R[15]);
}

void ARM9Write32(u32 addr, u32 val)
{
    addr &= ~0x3;

    if (addr < 0x40000000)
    {
        *(u32*)&MainRAM[addr & 0x3FFFFF] = val;
        return;
    }
    if (addr >= 0xE0010000 && addr < 0xE0020000)
    {
        SDIO::Write32(addr, val);
        return;
    }
    if (addr >= 0xF0000000)
    {
        return ARM9IOWrite32(addr, val);
    }

    printf("unknown write32 %08X %08X @ %08X\n", addr, val, ARM9->R[15]);
}

bool ARM9GetMemRegion(u32 addr, bool write, MemRegion* region)
{
    if (addr < 0x40000000)
    {
        region->Mem = MainRAM;
        region->Mask = 0x3FFFFF;
        return true;
    }

    region->Mem = NULL;
    return false;
}



// NOTE on I/O
// * 8/16-bit writes act as 32-bit writes, with the data duplicated across the entire 32-bit range

u8 ARM9IORead8(u32 addr)
{
    u32 val = ARM9IORead32(addr & ~0x3);
    return (val >> (8 * (addr&0x3))) & 0xFF;
}

u16 ARM9IORead16(u32 addr)
{
    u32 val = ARM9IORead32(addr & ~0x3);
    return (val >> (16 * (addr&0x1))) & 0xFFFF;
}

u32 ARM9IORead32(u32 addr)
{
    if ((addr >= 0xF0001208) && (addr < 0xF00012A8))
    {
        addr = (addr - 0xF0001208) >> 2;
        return IRQEnable[addr];
    }

    switch (addr & 0xFFFFFF00)
    {
    case 0xF0004000:
    case 0xF0004100: return DMA::Read(addr);
    case 0xF0004400: return SPI::Read(addr);
    case 0xF0004C00: return UART::Read(addr);
    case 0xF0005800:
    case 0xF0005C00:
    case 0xF0006000:
    case 0xF0006400:
    case 0xF0006800: return I2C::Read(addr);
    case 0xF0009400:
    case 0xF0009500: return Video::Read(addr);
    }

    switch (addr)
    {
    case 0xF0000000: return 0x00041040; // hardware ID

    case 0xF0000400: return TimerPrescaler[0];
    case 0xF0000404: return TimerPrescaler[1];
    case 0xF0000408: return CountUpVal;
    case 0xF0000410: return TimerCnt[0];
    case 0xF0000414: return TimerVal[0];
    case 0xF0000418: return TimerTarget[0];
    case 0xF0000420: return TimerCnt[1];
    case 0xF0000424: return TimerVal[1];
    case 0xF0000428: return TimerTarget[1];

    case 0xF00013F0:
        if (CurrentIRQ != IRQ_None) IRQPriority = 0;
        return CurrentIRQ;
    case 0xF00013F8:
    case 0xF00019F8: return IRQPriority;
    case 0xF00013FC:
    case 0xF00019FC: return LastIRQPriority;

    case 0xF00050EC:
    case 0xF00050F0:
    case 0xF00050F4:
    case 0xF00050F8:
    case 0xF00050FC: return SPI::Read(addr);
    }

    printf("unknown IO read32 %08X @ %08X\n", addr, ARM9->R[15]);
    return 0;
}

void ARM9IOWrite8(u32 addr, u8 val)
{
    return ARM9IOWrite32(addr & ~0x3, val * 0x01010101);
}

void ARM9IOWrite16(u32 addr, u16 val)
{
    return ARM9IOWrite32(addr & ~0x3, val * 0x00010001);
}

void ARM9IOWrite32(u32 addr, u32 val)
{
    if ((addr >= 0xF0001208) && (addr < 0xF00012A8))
    {
        addr = (addr - 0xF0001208) >> 2;
        IRQEnable[addr] = val & 0xFF;
        if (addr!=4) printf("IRQEnable[%02X] = %02X\n", addr, val&0xFF);
        return;
    }

    switch (addr & 0xFFFFFF00)
    {
    case 0xF0004000:
    case 0xF0004100: DMA::Write(addr, val); return;
    case 0xF0004400: SPI::Write(addr, val); return;
    case 0xF0004C00: UART::Write(addr, val); return;
    case 0xF0005800:
    case 0xF0005C00:
    case 0xF0006000:
    case 0xF0006400:
    case 0xF0006800: I2C::Write(addr, val); return;
    case 0xF0009400:
    case 0xF0009500: Video::Write(addr, val); return;
    }

    switch (addr)
    {
    case 0xF0000004:
        // soft reset register -- this is a big fat guess
        if (val && (!SoftResetReg))
            SoftReset();
        SoftResetReg = val;
        return;

    case 0xF0000400:
        TimerPrescaler[0] = val & 0xFF;
        TimerCounter[0] = 0; // checkme
        return;
    case 0xF0000404:
        TimerPrescaler[1] = val & 0xFF;
        TimerCounter[1] = 0; // checkme
        return;

    case 0xF0000408:
        CountUpVal = val;
        return;

    case 0xF0000410:
        TimerCnt[0] = val;
        if (!(val & (1<<1)))
            TimerVal[0] = 0;
        else
            TimerSubCounter[0] = 0;
        return;
    case 0xF0000414:
        TimerVal[0] = val;
        return;
    case 0xF0000418:
        TimerTarget[0] = val;
        return;

    case 0xF0000420:
        TimerCnt[1] = val;
        if (!(val & (1<<1)))
            TimerVal[1] = 0;
        else
            TimerSubCounter[1] = 0;
        return;
    case 0xF0000424:
        TimerVal[1] = val;
        return;
    case 0xF0000428:
        TimerTarget[1] = val;
        return;

    case 0xF00013F8:
        AcknowledgeIRQ(val & 0xF);
        return;

    case 0xF00050EC:
    case 0xF00050F0:
    case 0xF00050F4:
    case 0xF00050F8:
    case 0xF00050FC:
        SPI::Write(addr, val);
        return;
    }

    printf("unknown IO write32 %08X %08X @ %08X  %08X %08X\n", addr, val, ARM9->R[15], ARM9->CPSR, ARM9->CurInstr);
}

}
