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
#include <string>
#include <unordered_map>
#include "WUP.h"
#include "SDIO.h"
#include "Wifi.h"
#include "FIFO.h"
#include "Platform.h"

using Platform::Log;
using Platform::LogLevel;

namespace Wifi
{

const u8 EnumROM[] =
{
    0x01, 0x00, 0xF8, 0x4B, 0x11, 0x42, 0x00, 0x23, 0x03, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x18, // 0x00-0x0F
    0xC5, 0x00, 0x10, 0x18, 0x01, 0x12, 0xF8, 0x4B, 0x11, 0x42, 0x00, 0x16, 0x03, 0x01, 0x00, 0x00, // 0x10-0x1F
    0x05, 0x10, 0x00, 0x18, 0xC5, 0x10, 0x10, 0x18, 0x01, 0x29, 0xF8, 0x4B, 0x11, 0x42, 0x00, 0x03, // 0x20-0x2F
    0x03, 0x02, 0x00, 0x00, 0x05, 0x20, 0x00, 0x18, 0xC5, 0x20, 0x10, 0x18, 0x01, 0x2A, 0xF8, 0x4B, // 0x30-0x3F
    0x11, 0x42, 0x00, 0x02, 0x03, 0x03, 0x00, 0x00, 0x05, 0x30, 0x00, 0x18, 0xC5, 0x30, 0x10, 0x18, // 0x40-0x4F
    0x01, 0x0E, 0xF8, 0x4B, 0x01, 0x04, 0x08, 0x06, 0x05, 0x40, 0x00, 0x18, 0x35, 0x01, 0x00, 0x00, // 0x50-0x5F
    0x00, 0x80, 0x04, 0x00, 0x35, 0x01, 0x00, 0x10, 0x00, 0x80, 0x04, 0x00, 0x35, 0x01, 0x00, 0x1E, // 0x60-0x6F
    0x00, 0x00, 0x02, 0x00, 0x85, 0x41, 0x10, 0x18, 0x01, 0x1A, 0xF8, 0x4B, 0x11, 0x42, 0x00, 0x08, // 0x70-0x7F
    0x03, 0x04, 0x00, 0x00, 0x05, 0x50, 0x00, 0x18, 0xC5, 0x50, 0x10, 0x18, 0x01, 0x35, 0xB1, 0x43, // 0x80-0x8F
    0x01, 0x02, 0x08, 0x00, 0x75, 0x00, 0x00, 0x18, 0x00, 0x00, 0x01, 0x00, 0x85, 0x60, 0x10, 0x18, // 0x90-0x9F
    0x01, 0x35, 0xB1, 0x43, 0x01, 0x02, 0x08, 0x00, 0x75, 0x00, 0x10, 0x18, 0x00, 0x00, 0x01, 0x00, // 0xA0-0xAF
    0x85, 0x70, 0x10, 0x18, 0x01, 0x67, 0xB3, 0x43, 0x01, 0x02, 0x00, 0x00, 0x05, 0x80, 0x10, 0x18, // 0xB0-0xBF
    0x01, 0x66, 0xB3, 0x43, 0x01, 0x02, 0x00, 0x00, 0x05, 0x90, 0x10, 0x18, 0x01, 0x01, 0xB3, 0x43, // 0xC0-0xCF
    0x01, 0x02, 0x00, 0x11, 0x05, 0x60, 0x00, 0x18, 0x01, 0xFF, 0xBF, 0x43, 0x01, 0x02, 0x08, 0x00, // 0xD0-0xDF
    0x35, 0x70, 0x00, 0x18, 0x00, 0x90, 0x0F, 0x00, 0x35, 0xB0, 0x10, 0x18, 0x00, 0x50, 0xEF, 0x05, // 0xE0-0xEF
    0x35, 0x40, 0x01, 0x1E, 0x00, 0xC0, 0xFE, 0x01, 0x85, 0xA0, 0x10, 0x18, 0x0F, 0x00, 0x00, 0x00, // 0xF0-0xFF
};

u8 FuncEnable;
u8 FuncReady;

u8 TransferFunc;
u32 TransferAddr;
int TransferIncr;
u32 TransferLen;

u16 F1BlockSize;
u16 F2BlockSize;

u8 ClockCnt;

u32 F1BaseAddr;
//u32 F1LastAddr;
u32 F1Temp;

struct sCore
{
    u32 IOCtrl;
    u32 ResetCtrl;
};

// WIFI CORES
// 0 = backplane
// 1 = 802.11 MAC
// 2 = SDIO device
// 3 = ARM Cortex-M3
// 4 = RAM
// 5 = USB2.0 device
sCore Cores[6];

u8 RAM[0x48000];

u32 IRQStatus;
u32 IRQEnable;

FIFO<u8, 0x8000> RXMailbox; // incoming F2 data
FIFO<u8, 0x8000> TXMailbox; // outgoing F2 data

u8 Scratch[0x8000];

// ioctl vars
const u8 MACAddr[6] = {0x40, 0xF4, 0x01, 0x23, 0x45, 0x67};
u32 RoamOff;
u32 SgiTx;
u32 SgiRx;
u32 AmpduTxfailEvent;
u32 AcRemapMode;
u8 EventMsgs[12];
u32 Ampdu;
u32 Country;
u32 Lifetime;

u32 RadioDisable;
u32 Srl, Lrl;

u32 BcnTimeout;
u32 SupWpa;

struct sVarEntry
{
    u8* Data;
    u32 Length;
};

sVarEntry _varEntry(u8* data, u32 len)
{
    sVarEntry ret;
    ret.Data = data;
    ret.Length = len;
    return ret;
}

std::unordered_map<std::string, sVarEntry> VarMap;

// WIFI RESPONSES
//
// event:
// 00000000: 0C 00 F3 FF 00 01 00 0C 00 04 00 00 00 00 00 00
/*
        00000000: 2A 00 D5 FF 01 00 00 0C 00 06 00 00 06 01 00 00
        00000010: 0E 00 00 00 00 00 01 00 00 00 00 00 40 F4 07 EA
        00000020: 66 19 68 65 72 61 64 64 72 00 00 00 00 00 00 00
        00000030: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
         */


bool Init()
{
    VarMap["cur_etheraddr"] = _varEntry((u8*)MACAddr, 6);
    VarMap["roam_off"] = _varEntry((u8*)&RoamOff, 4);
    VarMap["sgi_tx"] = _varEntry((u8*)&SgiTx, 4);
    VarMap["sgi_rx"] = _varEntry((u8*)&SgiRx, 4);
    VarMap["ampdu_txfail_event"] = _varEntry((u8*)&AmpduTxfailEvent, 4);
    VarMap["ac_remap_mode"] = _varEntry((u8*)&AcRemapMode, 4);
    VarMap["event_msgs"] = _varEntry(EventMsgs, 12);
    VarMap["ampdu"] = _varEntry((u8*)&Ampdu, 4);
    VarMap["country"] = _varEntry((u8*)&Country, 4);
    VarMap["lifetime"] = _varEntry((u8*)&Lifetime, 4);

    VarMap["bcn_timeout"] = _varEntry((u8*)&BcnTimeout, 4);
    VarMap["sup_wpa"] = _varEntry((u8*)&SupWpa, 4);

    return true;
}

void DeInit()
{
}

void Reset()
{
    FuncEnable = 0;
    FuncReady = 0;

    TransferFunc = 0;
    TransferAddr = 0;
    TransferIncr = 0;
    TransferLen = 0;

    F1BlockSize = 0;
    F2BlockSize = 0;

    ClockCnt = 0;

    F1BaseAddr = 0;
    //F1LastAddr = 0xFFFFFFFF;
    F1Temp = 0;

    memset(Cores, 0, sizeof(Cores));
    for (int i = 0; i < 6; i++)
    {
        Cores[i].IOCtrl = 0;
        Cores[i].ResetCtrl = 0x1;
    }

    // enable backplane and SDIOD cores
    Cores[0].IOCtrl = 0x1;
    Cores[0].ResetCtrl = 0;
    Cores[2].IOCtrl = 0x1;
    Cores[2].ResetCtrl = 0;

    memset(RAM, 0, 0x48000);

    IRQStatus = 0;
    IRQEnable = 0;

    RXMailbox.Clear();
    TXMailbox.Clear();

    memset(Scratch, 0, sizeof(Scratch));

    memset(EventMsgs, 0, sizeof(EventMsgs));
    EventMsgs[6] = 0x40;

    RoamOff = 1;
    SgiTx = 0;
    SgiRx = 0;
    AmpduTxfailEvent = 0;
    AcRemapMode = 0;
    Ampdu = 1;
    Country = 0x4C41;
    Lifetime = 0;

    RadioDisable = 1;
    Srl = 7;
    Lrl = 0;

    // TODO
    BcnTimeout = 0;
    SupWpa = 0;
}


void SetIRQ(int irq)
{
    IRQStatus |= (1<<irq);
    if (IRQStatus & IRQEnable)
        SDIO::SetIRQ(SDIO::IRQ_CardInterrupt);
}


u8 MB_Read8()
{
    return RXMailbox.Read();
}

u16 MB_Read16()
{
    u16 ret = RXMailbox.Read();
    ret |= (RXMailbox.Read() << 8);
    return ret;
}

u32 MB_Read32()
{
    u32 ret = RXMailbox.Read();
    ret |= (RXMailbox.Read() << 8);
    ret |= (RXMailbox.Read() << 16);
    ret |= (RXMailbox.Read() << 24);
    return ret;
}

void MB_Write8(u8 val)
{
    TXMailbox.Write(val);
}

void MB_Write16(u16 val)
{
    TXMailbox.Write(val & 0xFF);
    TXMailbox.Write((val >> 8) & 0xFF);
}

void MB_Write32(u32 val)
{
    TXMailbox.Write(val & 0xFF);
    TXMailbox.Write((val >> 8) & 0xFF);
    TXMailbox.Write((val >> 16) & 0xFF);
    TXMailbox.Write(val >> 24);
}

u16 MB_PeekSize()
{
    u16 ret = RXMailbox.Peek(0);
    ret |= (RXMailbox.Peek(1) << 8);
    return ret;
}

u32 MB_AlignedSize(u32 msgsize)
{
    u32 roundsize = 0;
    while (roundsize < msgsize)
        roundsize += F2BlockSize;
    return roundsize;
}

void MB_Pad(u32 msgsize)
{
    //u32 roundsize = MB_AlignedSize(msgsize);
    //while (TXMailbox.Level() < roundsize)
    while (TXMailbox.Level() < msgsize)
        TXMailbox.Write(0);
}

void MB_SignalCB(u32 param)
{
    SetIRQ(IRQ_TXFrame);
}

void MB_Signal()
{
    WUP::ScheduleEvent(WUP::Event_WifiResponse, false, 1024, MB_SignalCB, 0);
}


void MakeIoctlRespHeader(u32 ioctl, u32 datalen, u8 seqno, u32 reqid, u32 status = 0)
{
    u16 totallen = datalen + 0x1C;

    TXMailbox.Clear();
    MB_Write16(totallen);
    MB_Write16(~totallen);

    MB_Write8(seqno); // checkme
    MB_Write8(0); // channel (ctrl)
    MB_Write8(0);
    MB_Write8(0x0C); // data offset
    MB_Write8(0);
    MB_Write8(seqno+2); // credit
    MB_Write8(0);
    MB_Write8(0);

    MB_Write32(ioctl);
    MB_Write32(datalen);
    MB_Write32(reqid);
    MB_Write32(status);
}

void IoctlGetVar(u8* data, u32 datalen, u8 seqno, u32 reqid)
{
    char* var = (char*)data;
    printf("WIFI: GetVar %s\n", var);
    if (VarMap.count(var))
    {
        auto& entry = VarMap[var];

        MakeIoctlRespHeader(262, datalen, seqno, reqid);

        for (u32 i = 0; i < entry.Length; i++)
            MB_Write8(entry.Data[i]);
        MB_Pad(datalen + 0x1C);

        MB_Signal();
        return;
    }

    printf("IoctlGetVar: unknown var name %s\n", var);
    exit(-1);
}

void IoctlSetVar(u8* data, u32 datalen, u8 seqno, u32 reqid)
{
    char* var = (char*)data;
    u8* val = data + strlen(var) + 1;
    printf("WIFI: SetVar %s\n", var);

    if (VarMap.count(var))
    {
        auto& entry = VarMap[var];

        // TODO: are all vars writable?
        for (u32 i = 0; i < entry.Length; i++)
            entry.Data[i] = val[i];

        MakeIoctlRespHeader(263, datalen, seqno, reqid);

        //for (u32 i = 0; i < entry.Length; i++)
        //    MB_Write8(entry.Data[i]);
        // checkme
        MB_Pad(datalen + 0x1C);

        MB_Signal();
        return;
    }

    printf("IoctlSetVar: unknown var name %s\n", var);
    exit(-1);
}

void HandleIoctl(u8 seqno, u16 opc, u8* data, u32 datalen, u32 reqid)
{
    printf("WIFI: IOCTL %d  len=%d\n", opc, datalen);
    switch (opc)
    {
    case 2: // up
        MakeIoctlRespHeader(opc, 4, seqno, reqid);
        MB_Write32(1);
        // TODO
        MB_Signal();
        return;

    case 3: // down
        MakeIoctlRespHeader(opc, 4, seqno, reqid);
        MB_Write32(1);
        // TODO
        MB_Signal();
        return;

    case 20: // set infra
        MakeIoctlRespHeader(opc, 4, seqno, reqid);
        MB_Write32(1);
        // TODO
        MB_Signal();
        return;

    case 22: // set auth
        MakeIoctlRespHeader(opc, 4, seqno, reqid);
        MB_Write32(1);
        // TODO
        MB_Signal();
        return;

    case 26: // set SSID
        // TODO join network!!
        {
            char ssid[33];
            u32 ssidlen = *(u32*)&data[0];
            memset(ssid, 0, 33);
            memcpy(ssid, &data[4], ssidlen);
            printf(" - JOIN NETWORK %s\n", ssid);
        }
        MakeIoctlRespHeader(opc, 4, seqno, reqid);
        MB_Write32(1);
        MB_Signal();
        return;

    case 29: // get channel
        MakeIoctlRespHeader(opc, 12, seqno, reqid);
        // TODO make not hardcoded
        MB_Write32(0);
        MB_Write32(0);
        MB_Write32(0);
        MB_Signal();
        return;

    case 31: // get SRL
        MakeIoctlRespHeader(opc, datalen, seqno, reqid);
        MB_Write32(Srl);
        MB_Pad(datalen + 0x1C);
        MB_Signal();
        return;

    case 33: // get LRL
        MakeIoctlRespHeader(opc, datalen, seqno, reqid);
        MB_Write32(Lrl);
        MB_Pad(datalen + 0x1C);
        MB_Signal();
        return;

    case 37: // get radio disable
        MakeIoctlRespHeader(opc, datalen, seqno, reqid);
        MB_Write32(RadioDisable);
        MB_Pad(datalen + 0x1C);
        MB_Signal();
        return;

    case 38: // set radio disable
        MakeIoctlRespHeader(opc, 4, seqno, reqid);
        MB_Write32(1); // TODO
        MB_Signal();
        return;

    case 61: // get TX antenna
    case 63: // get antenna div?
        MakeIoctlRespHeader(opc, 4, seqno, reqid);
        MB_Write32(3);
        MB_Signal();
        return;

    case 134: // set wsec
        MakeIoctlRespHeader(opc, 4, seqno, reqid);
        MB_Write32(1);
        // TODO
        MB_Signal();
        return;

    case 142: // set band
        MakeIoctlRespHeader(opc, datalen, seqno, reqid);
        MB_Pad(datalen + 0x1C); // checkme
        MB_Signal();
        return;

    case 165: // set WPA auth
        MakeIoctlRespHeader(opc, 4, seqno, reqid);
        MB_Write32(1);
        // TODO
        MB_Signal();
        return;

    case 262: // get variable
        IoctlGetVar(data, datalen, seqno, reqid);
        return;

    case 263: // set variable
        IoctlSetVar(data, datalen, seqno, reqid);
        return;

    case 268: // set WPA password
        MakeIoctlRespHeader(opc, 4, seqno, reqid);
        MB_Write32(1);
        // TODO
        MB_Signal();
        return;
    }

    printf("HandleIoctl: unknown ioctl %d, len=%d\n", opc, datalen);
    for (int i = 0; i < datalen; i++)
    {
        if (!(i&0xF)) printf("%08X:  ", i);
        printf("%02X ", data[i]);
        if ((i&0xF)==0xF) printf("\n");
    }
    exit(-1);
}

void HandleMessage()
{
    u16 msgsize = MB_Read16();
    u16 notsize = MB_Read16();
    if ((u16)(~notsize) != msgsize)
    {
        printf("wifi: bad message header (%04X != %04X)\n", msgsize, (u16)(~notsize));
        return;
    }

    u8 seqno = MB_Read8();
    u8 chan = MB_Read8();
    MB_Read8();
    u8 dataoffset = MB_Read8();
    MB_Read32(); // zero

    if (dataoffset < 0xC)
    {
        printf("wifi: bad data offset %02X\n", dataoffset);
        return;
    }

    dataoffset -= 0xC;
    for (int i = 0; i < dataoffset; i++)
        MB_Read8();

    chan &= 0xF;
    if (chan == 0)
    {
        // control
        u32 opc = MB_Read32();
        u32 datalen = MB_Read32();
        u32 reqid = MB_Read32();
        u32 status = MB_Read32(); // should be 0

        if (datalen > sizeof(Scratch))
        {
            printf("wifi: ioctl data too long (%08X)\n", datalen);
            return;
        }

        for (u32 i = 0; i < datalen; i++)
            Scratch[i] = MB_Read8();

        HandleIoctl(seqno, opc, Scratch, datalen, reqid);
    }

    RXMailbox.Clear();
}


u32 F1_Read32(u32 addr)
{
    if (addr < 0x48000)
    {
        return *(u32*)&RAM[addr];
    }

    if (addr >= 0x18100000 && addr < 0x18106000)
    {
        u32 cid = (addr >> 12) & 0xF;
        sCore* core = &Cores[cid];
        switch (addr & 0xFFF)
        {
        case 0x408: return core->IOCtrl;
        case 0x800: return core->ResetCtrl;
        }
    }

    if (addr >= 0x18109000 && addr < 0x18109100)
    {
        // enumeration ROM
        return *(u32*)&EnumROM[addr - 0x18109000];
    }

    switch (addr)
    {
    case 0x18000000: return 0x16914319; // chip ID
    case 0x18000004: return 0x10480009; // chip capabilities
    case 0x180000FC: return 0x18109000; // enumeration ROM address
    case 0x18000604: return 0x19CC3607; // PMU capabilities

    case 0x18002020: return IRQStatus;
    case 0x18002024: return IRQEnable;

    case 0x18004000: return 0x00258033; // RAM size register
    }

    printf("wifi: unknown F1 read %08X\n", addr);
    return 0;
}

void F1_Write32(u32 addr, u32 val)
{
    if (addr < 0x48000)
    {
        *(u32*)&RAM[addr] = val;
        return;
    }

    if (addr >= 0x18100000 && addr < 0x18106000)
    {
        u32 cid = (addr >> 12) & 0xF;
        sCore* core = &Cores[cid];
        switch (addr & 0xFFF)
        {
        case 0x408:
            core->IOCtrl = val;
            return;

        case 0x800:
            core->ResetCtrl = val;
            return;
        }
    }

    switch (addr)
    {
    case 0x18002020:
        IRQStatus &= ~val;
        return;
    case 0x18002024:
        IRQEnable = val;
        return;
    }

    printf("wifi: unknown F1 write %08X %08X\n", addr, val);
}


u8 Read8(u32 func, u32 addr)
{
    if (func == 0)
    {
        switch (addr)
        {
        case 0x00002: return FuncEnable;
        case 0x00003: return FuncReady;
        case 0x00009: return 0x00;
        case 0x0000A: return 0x10;
        case 0x0000B: return 0x00;

        case 0x00109: return 0x03;
        case 0x0010A: return 0x10;
        case 0x0010B: return 0x00;
        case 0x00110: return F1BlockSize & 0xFF;
        case 0x00111: return F1BlockSize >> 8;

        case 0x00209: return 0x03;
        case 0x0020A: return 0x10;
        case 0x0020B: return 0x00;
        case 0x00210: return F2BlockSize & 0xFF;
        case 0x00211: return F2BlockSize >> 8;
        }
    }
    else if (func == 1)
    {
        if (addr >= 0x8000 && addr < 0x10000)
        {
            // 32-bit F1 memory window
            u32 f1addr = F1BaseAddr | (addr & 0x7FFC);
            //if (f1addr != F1LastAddr)
            if (!(addr & 0x3))
                F1Temp = F1_Read32(f1addr);

            //F1LastAddr = f1addr;
            return (F1Temp >> (8 * (addr&0x3))) & 0xFF;
        }

        switch (addr)
        {
        case 0x1000A: return (F1BaseAddr >> 8) & 0x80;
        case 0x1000B: return (F1BaseAddr >> 16) & 0xFF;
        case 0x1000C: return (F1BaseAddr >> 24) & 0xFF;

        case 0x1000E: return ClockCnt;
        }
    }
    else if (func == 2)
    {
        // FIFO
        if (TXMailbox.IsEmpty()) return 0;
        return TXMailbox.Read();
    }

    printf("unknown wifi read8 %d:%05X\n", func, addr);
    return 0;
}

void Write8(u32 func, u32 addr, u8 val)
{
    if (func == 0)
    {
        switch (addr)
        {
        case 0x00002:
            FuncEnable = val;
            FuncReady = val; // hax
            return;

        case 0x00110:
            F1BlockSize = (F1BlockSize & 0xFF00) | val;
            return;
        case 0x00111:
            F1BlockSize = (F1BlockSize & 0x00FF) | (val << 8);
            return;

        case 0x00210:
            F2BlockSize = (F2BlockSize & 0xFF00) | val;
            return;
        case 0x00211:
            F2BlockSize = (F2BlockSize & 0x00FF) | (val << 8);
            return;
        }
    }
    else if (func == 1)
    {
        if (addr >= 0x8000 && addr < 0x10000)
        {
            // 32-bit F1 memory window
            u32 f1addr = F1BaseAddr | (addr & 0x7FFC);
            if (!(addr & 0x3))
                F1Temp = 0;
            F1Temp &= ~(0xFF << (8 * (addr&0x3)));
            F1Temp |= (val << (8 * (addr&0x3)));
            if ((addr & 0x3) == 0x3)
                F1_Write32(f1addr, F1Temp);

            return;
        }

        switch (addr)
        {
        case 0x1000A:
            F1BaseAddr = (F1BaseAddr & 0xFFFF0000) | ((val & 0x80) << 8);
            return;
        case 0x1000B:
            F1BaseAddr = (F1BaseAddr & 0xFF008000) | (val << 16);
            return;
        case 0x1000C:
            F1BaseAddr = (F1BaseAddr & 0x00FF8000) | (val << 24);
            return;

        case 0x1000E:
            ClockCnt = val & 0x3F;
            if (ClockCnt & ((1<<0)|(1<<3))) // ALP (crystal)
                ClockCnt |= (1<<6);
            if (ClockCnt & ((1<<1)|(1<<4))) // HT (PLL)
                ClockCnt |= (1<<6)|(1<<7);
            return;
        }
    }
    else if (func == 2)
    {
        // FIFO
        RXMailbox.Write(val);

        if (RXMailbox.Level() < 4)
            return;

        u16 msgsize = MB_PeekSize();
        if (msgsize < 0x1C)
        {
            if (msgsize)
                printf("wifi: bad message size %04X\n", msgsize);
            RXMailbox.Clear();
            return;
        }

        //u16 roundsize = MB_AlignedSize(msgsize);

        if (RXMailbox.Level() < msgsize)
        //if (RXMailbox.Level() < roundsize)
            return;

        HandleMessage();
        return;
    }

    printf("unknown wifi write8 %d:%05X %02X\n", func, addr, val);
}


void Cmd52(u32 arg)
{
    u32 func = (arg >> 28) & 0x7;
    u32 addr = (arg >> 9) & 0x1FFFF;

    if (arg & (1<<31))
    {
        // write

        Write8(func, addr, arg & 0xFF);
        if (arg & (1<<27))
        {
            u8 val = Read8(func, addr);
            SDIO::SendResponse(0x1000 | val);
        }
        else
            SDIO::SendResponse(0x1000);
    }
    else
    {
        // read

        u8 val = Read8(func, addr);
        SDIO::SendResponse(0x1000 | val);
    }
}

void Cmd53(u32 arg)
{
    u32 func = (arg >> 28) & 0x7;
    u32 addr = (arg >> 9) & 0x1FFFF;
    u32 len = arg & 0x1FF;
    if (!len) len = 0x200;

    TransferFunc = func;
    TransferAddr = addr;

    if (arg & (1<<26))
        TransferIncr = 1;
    else
        TransferIncr = 0;

    if (arg & (1<<27))
    {
        // block mode
        if (func == 1)
            TransferLen = len * F1BlockSize;
        else if (func == 2)
            TransferLen = len * F2BlockSize;
        else
        {
            printf("Wifi: CMD53 block mode on unsupported function %d (arg=%08X)\n", func, arg);
            return;
        }
    }
    else
        TransferLen = len;

    if (arg & (1<<31))
    {
        // write

        if (TransferFunc == 2)
            RXMailbox.Clear(); // hack

        SDIO::StartTransfer(true);
        SDIO::SendResponse(0x1000);
    }
    else
    {
        // read
        SDIO::StartTransfer(false);
        SDIO::SendResponse(0x1000);
    }
}

void SendCommand(u8 cmd, u32 arg)
{
    switch (cmd)
    {
    case 5: // get OCR
        SDIO::SendResponse(0xA0FE0000);
        break;

    case 7: // select card
        SDIO::SendResponse(0x1E00);
        break;

    case 52: // byte read/write
        Cmd52(arg);
        break;

    case 53: // block read/write
        Cmd53(arg);
        break;

    default:
        printf("Wifi: unknown SD command %d %08X\n", cmd, arg);
        break;
    }
}

void ReadBlock(u8* data, u32 len)
{
    if (TransferLen == 0)
    {
        // TODO?
        return;
    }

    for (u32 i = 0; i < len; i++)
    {
        data[i] = Read8(TransferFunc, TransferAddr);
        TransferAddr += TransferIncr;
        TransferLen--;
        if (TransferLen == 0) break;
    }
}

void WriteBlock(u8* data, u32 len)
{
    if (TransferLen == 0)
    {
        // TODO?
        return;
    }

    for (u32 i = 0; i < len; i++)
    {
        Write8(TransferFunc, TransferAddr, data[i]);
        TransferAddr += TransferIncr;
        TransferLen--;
        if (TransferLen == 0) break;
    }
}

}
