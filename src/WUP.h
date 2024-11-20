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

#ifndef WUP_H
#define WUP_H

#include <string>

#include "types.h"

namespace WUP
{

enum
{
    Event_LCD = 0,

    Event_SPITransfer,
    Event_UART,
    Event_WifiResponse,

    Event_MAX
};

struct SchedEvent
{
    void (*Func)(u32 param);
    u64 Timestamp;
    u32 Param;
};

enum
{
    IRQ_Timer0 = 0x00,
    IRQ_Timer1,
    IRQ_SDIO,

    IRQ_SPI = 0x06,
    IRQ_SPIUnk,
    IRQ_SPDMA0,
    IRQ_SPDMA1,
    IRQ_GPDMA2 = 0x0C,
    IRQ_GPDMA0,
    IRQ_GPDMA1,

    IRQ_I2C = 0x0F,

    IRQ_None = 0x80
};

struct Timer
{
    u16 Reload;
    u16 Cnt;
    u32 Counter;
    u32 CycleShift;
};

enum
{
    Mem9_MainRAM    = 0x00000001,
};

struct MemRegion
{
    u8* Mem;
    u32 Mask;
};


extern u8 ARM9MemTimings[0x40000][8];
extern u32 ARM9Regions[0x40000];

extern u32 NumFrames;
extern u32 NumLagFrames;
extern bool LagFrameFlag;

extern u64 ARM9Timestamp, ARM9Target;

extern u8 MainRAM[0x400000];
extern u32 MainRAMMask;


bool Init();
void DeInit();
void Reset();
void Start();

void SetARM9RegionTimings(u32 addrstart, u32 addrend, u32 region, int buswidth, int nonseq, int seq);

bool LoadFirmware(const char* filename);

u32 RunFrame();
u32* GetFramebuffer();

void SetKeyMask(u32 mask);
void SetTouchCoords(bool touching, int x, int y);
/*void TouchScreen(u16 x, u16 y);
void ReleaseScreen();

void SetKeyMask(u32 mask);

bool IsLidClosed();
void SetLidClosed(bool closed);

void CamInputFrame(int cam, u32* data, int width, int height, bool rgb);
void MicInputFrame(s16* data, int samples);*/

void ScheduleEvent(u32 id, bool periodic, s32 delay, void (*func)(u32), u32 param);
void ScheduleEvent(u32 id, u64 timestamp, void (*func)(u32), u32 param);
void CancelEvent(u32 id);

void debug(u32 p);

void Halt();

void UpdateIRQ();
void SetIRQ(u32 irq);
bool HaltInterrupted();

u32 GetPC();
u64 GetSysClockCycles(int num);

void RunTimers();

u8 ARM9Read8(u32 addr);
u16 ARM9Read16(u32 addr);
u32 ARM9Read32(u32 addr);
void ARM9Write8(u32 addr, u8 val);
void ARM9Write16(u32 addr, u16 val);
void ARM9Write32(u32 addr, u32 val);

bool ARM9GetMemRegion(u32 addr, bool write, MemRegion* region);

u8 ARM9IORead8(u32 addr);
u16 ARM9IORead16(u32 addr);
u32 ARM9IORead32(u32 addr);
void ARM9IOWrite8(u32 addr, u8 val);
void ARM9IOWrite16(u32 addr, u16 val);
void ARM9IOWrite32(u32 addr, u32 val);

}

#endif // WUP_H
