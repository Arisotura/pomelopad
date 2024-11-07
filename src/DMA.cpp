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
#include "DMA.h"
#include "SPI.h"
#include "Platform.h"

using Platform::Log;
using Platform::LogLevel;

namespace DMA
{

struct sSPDMA
{
    u32 Start;
    u32 Cnt;
    u32 Unk08, Unk0C;
    u32 Length;
    u32 MemAddr;
    u32 IRQ;

    void Reset()
    {
        Start = 0;
        Cnt = 0;
        Unk08 = 0;
        Unk0C = 0;
        Length = 0;
        MemAddr = 0;
    }

    /*void StartTransfer()
    {
        // TODO make it not instant!!

        bool (*fnstart)(bool);
        void (*fnfinish)();
        u8 (*fnread)();
        void (*fnwrite)(u8);
        char* devname;

        u32 device = (Cnt >> 1) & 0x7;
        switch (device)
        {
        case 2: // SPI
            fnstart = SPI::DMAStart;
            fnfinish = SPI::DMAFinish;
            fnread = SPI::DMARead;
            fnwrite = SPI::DMAWrite;
            devname = "SPI";
            break;

        default:
            printf("SPDMA: unknown device %d\n", device);
            return;
        }

        if (Cnt & (1<<0))
        {
            // write
            printf("SPDMA: [%08X]->[%s] len=%05X\n", MemAddr, devname, Length);

            if (!fnstart(true)) return;
            for (;;)
            {
                fnwrite(WUP::MainRAM[MemAddr & 0x3FFFFF]);
                MemAddr++;
                if (Length == 0) break;
                Length--;
            }
            fnfinish();
        }
        else
        {
            // read
            printf("SPDMA: [%s]->[%08X] len=%05X\n", devname, MemAddr, Length);

            if (!fnstart(false)) return;
            for (;;)
            {
                WUP::MainRAM[MemAddr & 0x3FFFFF] = fnread();
                MemAddr++;
                if (Length == 0) break;
                Length--;
            }
            fnfinish();
        }

        Length = 0xFFFFF;
        Start &= ~(1<<0);
    }*/

    void StartTransfer()
    {
        u32 device = (Cnt >> 1) & 0x7;
        bool write = !!(Cnt & (1<<0));
        switch (device)
        {
        case 2: // SPI
            SPI::StartDMA(write);
            break;

        default:
            printf("SPDMA: unknown device %d\n", device);
            return;
        }
    }

    void DoTransfer(u32 maxlength)
    {
        void (*fnwrite)(u8);
        u8 (*fnread)();
        u32 device = (Cnt >> 1) & 0x7;
        switch (device)
        {
        case 2: // SPI
            fnwrite = SPI::WriteData;
            fnread = SPI::ReadData;
            break;

        default:
            return;
        }

        if (Cnt & (1<<0))
        {
            // write
            for (u32 i = 0; i < maxlength; i++)
            {
                fnwrite(WUP::MainRAM[MemAddr]);
                MemAddr = (MemAddr + 1) & 0x3FFFFF;
                Length = (Length - 1) & 0xFFFFF;
                if (Length == 0xFFFFF) break;
            }
        }
        else
        {
            // read
            for (u32 i = 0; i < maxlength; i++)
            {
                WUP::MainRAM[MemAddr] = fnread();
                //printf("DMA: read, addr=%08X, val=%02X, length=%08X\n", MemAddr, WUP::MainRAM[MemAddr & 0x3FFFFF], Length);
                MemAddr = (MemAddr + 1) & 0x3FFFFF;
                Length = (Length - 1) & 0xFFFFF;
                if (Length == 0xFFFFF) break;
            }
        }

        if (Length == 0xFFFFF)
        {
            Start &= ~(1<<0);
            WUP::SetIRQ(IRQ);
        }
    }

    void WriteStart(u32 val)
    {
        u32 oldstart = Start;
        Start = val;

        if ((Start & (1<<0)) && (!(oldstart & (1<<0))))
            StartTransfer();
    }

    u32 Read(u32 addr)
    {
        switch (addr & 0x1F)
        {
        case 0x00: return Start;
        case 0x04: return Cnt;
        case 0x08: return Unk08;
        case 0x0C: return Unk0C;
        case 0x10: return Length;
        case 0x14: return MemAddr;
        }

        return 0;
    }

    void Write(u32 addr, u32 val)
    {
        switch (addr & 0x1F)
        {
        case 0x00: WriteStart(val); return;
        case 0x04: Cnt = val; return;
        case 0x08: Unk08 = val; return;
        case 0x0C: Unk0C = val; return;
        case 0x10: Length = val & 0xFFFFF; return;
        case 0x14: MemAddr = val & 0x3FFFFF; return;
        }
    }
};

struct sGPDMA
{
    u32 Start;
    u32 Cnt;
    u32 ChunkSize;
    u32 SrcStride;
    u32 DstStride;
    u32 Length;
    u32 SrcAddr;
    u32 DstAddr;
    u16 Fill1, Fill2;
    u32 IRQ;

    void Reset()
    {
        Start = 0;
        Cnt = 0;
        ChunkSize = 0;
        SrcStride = 0;
        DstStride = 0;
        Length = 0;
        SrcAddr = 0;
        DstAddr = 0;
        Fill1 = 0;
        Fill2 = 0;
    }

    void StartTransfer()
    {
        if (Cnt & 0xFFFFF803)
            printf("GPDMA: unusual cnt %08X\n", Cnt);

        if (Cnt & (1<<10))
        {
            // simple fill
            printf("GPDMA: simple fill TODO\n");
        }
        else if (Cnt & (1<<7))
        {
            // masked fill
            printf("GPDMA: masked fill TODO\n");
        }
        else
        {
            // copy
            u32 srcmode = (Cnt >> 2) & 0xF;
            if (srcmode != 0x3 && srcmode != 0xC)
                printf("GPDMA: unusual srcmode %X\n", srcmode);

            u32 chunk = ChunkSize;
            if (chunk == 0) chunk = Length + 1; // checkme

            int srcinc, dstinc;
            if (srcmode == 0x3)
                srcinc = -1;
            else if (srcmode == 0xC)
                srcinc = 1;
            else
                return;

            dstinc = 1;

            for (;;)
            {
                u32 nextsrc = SrcAddr + (SrcStride * srcinc);
                u32 nextdst = DstAddr + (DstStride * dstinc);

                for (u32 i = 0; i < chunk; i++)
                {
                    WUP::MainRAM[DstAddr] = WUP::MainRAM[SrcAddr];
                    SrcAddr = (SrcAddr + srcinc) & 0x3FFFFF;
                    DstAddr = (DstAddr + dstinc) & 0x3FFFFF;
                    Length = (Length - 1) & 0xFFFFFF;
                    if (Length == 0xFFFFFF) break;
                }

                if (Length == 0xFFFFFF)
                {
                    // CHECKME: are addresses updated correctly in this situation?
                    Start &= ~(1<<0);
                    WUP::SetIRQ(IRQ);
                    return;
                }

                SrcAddr = nextsrc & 0x3FFFFF;
                DstAddr = nextdst & 0x3FFFFF;
            }
        }
    }

    void WriteStart(u32 val)
    {
        u32 oldstart = Start;
        Start = val;

        if ((Start & (1<<0)) && (!(oldstart & (1<<0))))
            StartTransfer();
    }

    u32 Read(u32 addr)
    {
        switch (addr & 0x3F)
        {
            case 0x00: return Start;
            case 0x04: return Cnt;
            case 0x08: return ChunkSize;
            case 0x0C: return SrcStride;
            case 0x10: return DstStride;
            case 0x14: return Length;
            case 0x18: return SrcAddr;
            case 0x1C: return DstAddr;
            case 0x20: return Fill1;
            case 0x24: return Fill2;
        }

        return 0;
    }

    void Write(u32 addr, u32 val)
    {
        // TODO: what are the masks for the various registers?
        switch (addr & 0x3F)
        {
            case 0x00: WriteStart(val); return;
            case 0x04: Cnt = val; return;
            case 0x08: ChunkSize = val; return;
            case 0x0C: SrcStride = val; return;
            case 0x10: DstStride = val; return;
            case 0x14: Length = val & 0xFFFFFF; return;
            case 0x18: SrcAddr = val & 0x3FFFFF; return;
            case 0x1C: DstAddr = val & 0x3FFFFF; return;
            case 0x20: Fill1 = val & 0xFFFF; return;
            case 0x24: Fill2 = val & 0xFFFF; return;
        }
    }
};

u32 Cnt;
sSPDMA SPDMA[2];
sGPDMA GPDMA[3];


bool Init()
{
    SPDMA[0].IRQ = WUP::IRQ_SPDMA0;
    SPDMA[1].IRQ = WUP::IRQ_SPDMA1;
    GPDMA[0].IRQ = WUP::IRQ_GPDMA0;
    GPDMA[1].IRQ = WUP::IRQ_GPDMA1;
    GPDMA[2].IRQ = WUP::IRQ_GPDMA2;
    return true;
}

void DeInit()
{
}

void Reset()
{
    Cnt = 0;

    SPDMA[0].Reset();
    SPDMA[1].Reset();
    GPDMA[0].Reset();
    GPDMA[1].Reset();
    GPDMA[2].Reset();
}


void CheckSPDMA(u32 device, bool write, u32 maxlength)
{
    if (!maxlength) return;

    u32 cnt_check = ((device & 0x7) << 1) | (write & 0x1);
    for (int i = 0; i < 2; i++)
    {
        sSPDMA* dma = &SPDMA[i];
        if (!(dma->Start & (1<<0))) continue;
        if (dma->Cnt != cnt_check) continue;

        dma->DoTransfer(maxlength);
        return;
    }
}


u32 Read(u32 addr)
{
    switch (addr & 0xFFFFFFE0)
    {
    case 0xF0004040: return SPDMA[0].Read(addr);
    case 0xF0004060: return SPDMA[1].Read(addr);

    case 0xF0004100:
    case 0xF0004120: return GPDMA[0].Read(addr);
    case 0xF0004140:
    case 0xF0004160: return GPDMA[1].Read(addr);
    case 0xF0004180:
    case 0xF00041A0: return GPDMA[2].Read(addr);
    }

    if (addr == 0xF0004000)
        return Cnt;

    printf("unknown DMA read %08X\n", addr);
    return 0;
}

void Write(u32 addr, u32 val)
{
    switch (addr & 0xFFFFFFE0)
    {
    case 0xF0004040: SPDMA[0].Write(addr, val); return;
    case 0xF0004060: SPDMA[1].Write(addr, val); return;

    case 0xF0004100:
    case 0xF0004120: GPDMA[0].Write(addr, val); return;
    case 0xF0004140:
    case 0xF0004160: GPDMA[1].Write(addr, val); return;
    case 0xF0004180:
    case 0xF00041A0: GPDMA[2].Write(addr, val); return;
    }

    if (addr == 0xF0004000)
        Cnt = val;
    else
        printf("unknown DMA write %08X %08X\n", addr, val);
}

}
