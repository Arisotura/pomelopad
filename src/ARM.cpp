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
#include <algorithm>
#include "WUP.h"
#include "ARM.h"
#include "ARMInterpreter.h"
#include "Platform.h"

using Platform::Log;
using Platform::LogLevel;


u32 ARM::ConditionTable[16] =
{
    0xF0F0, // EQ
    0x0F0F, // NE
    0xCCCC, // CS
    0x3333, // CC
    0xFF00, // MI
    0x00FF, // PL
    0xAAAA, // VS
    0x5555, // VC
    0x0C0C, // HI
    0xF3F3, // LS
    0xAA55, // GE
    0x55AA, // LT
    0x0A05, // GT
    0xF5FA, // LE
    0xFFFF, // AL
    0x0000  // NE
};


ARM::ARM(u32 num)
{
    // well uh
    Num = num;
}

ARM::~ARM()
{
    // dorp
}

ARMv5::ARMv5() : ARM(0)
{
}

ARMv5::~ARMv5()
{
}

void ARM::Reset()
{
    Cycles = 0;
    Halted = 0;

    IRQ = 0;

    for (int i = 0; i < 16; i++)
        R[i] = 0;

    CPSR = 0x000000D3;

    for (int i = 0; i < 7; i++)
        R_FIQ[i] = 0;
    for (int i = 0; i < 2; i++)
    {
        R_SVC[i] = 0;
        R_ABT[i] = 0;
        R_IRQ[i] = 0;
        R_UND[i] = 0;
    }

    R_FIQ[7] = 0x00000010;
    R_SVC[2] = 0x00000010;
    R_ABT[2] = 0x00000010;
    R_IRQ[2] = 0x00000010;
    R_UND[2] = 0x00000010;

    ExceptionBase = 0x00000000;// 0xFFFF0000;

    CodeMem.Mem = NULL;

    // zorp
    JumpTo(ExceptionBase);
}

void ARMv5::Reset()
{
    ARM::Reset();
}

void ARM::SoftReset()
{
    u32 oldcpsr = CPSR;
    CPSR &= ~0xFF;
    CPSR |= 0xD3;
    UpdateMode(oldcpsr, CPSR);

    JumpTo(ExceptionBase);
}


void ARMv5::JumpTo(u32 addr, bool restorecpsr)
{
    if (restorecpsr)
    {
        RestoreCPSR();

        if (CPSR & 0x20)    addr |= 0x1;
        else                addr &= ~0x1;
    }

    //u32 oldregion = R[15] >> 24;
    //u32 newregion = addr >> 24;

    RegionCodeCycles = 1;//MemTimings[addr >> 12][0];

    if (addr & 0x1)
    {
        addr &= ~0x1;
        R[15] = addr+2;

        //if (newregion != oldregion) SetupCodeMem(addr);

        // two-opcodes-at-once fetch
        // doesn't matter if we put garbage in the MSbs there
        if (addr & 0x2)
        {
            NextInstr[0] = CodeRead32(addr-2, true) >> 16;
            Cycles += CodeCycles;
            NextInstr[1] = CodeRead32(addr+2, false);
            Cycles += CodeCycles;
        }
        else
        {
            NextInstr[0] = CodeRead32(addr, true);
            NextInstr[1] = NextInstr[0] >> 16;
            Cycles += CodeCycles;
        }

        CPSR |= 0x20;
    }
    else
    {
        addr &= ~0x3;
        R[15] = addr+4;

        //if (newregion != oldregion) SetupCodeMem(addr);

        NextInstr[0] = CodeRead32(addr, true);
        Cycles += CodeCycles;
        NextInstr[1] = CodeRead32(addr+4, false);
        Cycles += CodeCycles;

        CPSR &= ~0x20;
    }
}

void ARM::RestoreCPSR()
{
    u32 oldcpsr = CPSR;

    switch (CPSR & 0x1F)
    {
    case 0x11:
        CPSR = R_FIQ[7];
        break;

    case 0x12:
        CPSR = R_IRQ[2];
        break;

    case 0x13:
        CPSR = R_SVC[2];
        break;

    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
        CPSR = R_ABT[2];
        break;

    case 0x18:
    case 0x19:
    case 0x1A:
    case 0x1B:
        CPSR = R_UND[2];
        break;

    default:
        Log(LogLevel::Warn, "!! attempt to restore CPSR under bad mode %02X, %08X\n", CPSR&0x1F, R[15]);
        break;
    }

    CPSR |= 0x00000010;

    UpdateMode(oldcpsr, CPSR);
}

void ARM::UpdateMode(u32 oldmode, u32 newmode, bool phony)
{
    if ((oldmode & 0x1F) == (newmode & 0x1F)) return;

    switch (oldmode & 0x1F)
    {
    case 0x11:
        std::swap(R[8], R_FIQ[0]);
        std::swap(R[9], R_FIQ[1]);
        std::swap(R[10], R_FIQ[2]);
        std::swap(R[11], R_FIQ[3]);
        std::swap(R[12], R_FIQ[4]);
        std::swap(R[13], R_FIQ[5]);
        std::swap(R[14], R_FIQ[6]);
        break;

    case 0x12:
        std::swap(R[13], R_IRQ[0]);
        std::swap(R[14], R_IRQ[1]);
        break;

    case 0x13:
        std::swap(R[13], R_SVC[0]);
        std::swap(R[14], R_SVC[1]);
        break;

    case 0x17:
        std::swap(R[13], R_ABT[0]);
        std::swap(R[14], R_ABT[1]);
        break;

    case 0x1B:
        std::swap(R[13], R_UND[0]);
        std::swap(R[14], R_UND[1]);
        break;
    }

    switch (newmode & 0x1F)
    {
    case 0x11:
        std::swap(R[8], R_FIQ[0]);
        std::swap(R[9], R_FIQ[1]);
        std::swap(R[10], R_FIQ[2]);
        std::swap(R[11], R_FIQ[3]);
        std::swap(R[12], R_FIQ[4]);
        std::swap(R[13], R_FIQ[5]);
        std::swap(R[14], R_FIQ[6]);
        break;

    case 0x12:
        std::swap(R[13], R_IRQ[0]);
        std::swap(R[14], R_IRQ[1]);
        break;

    case 0x13:
        std::swap(R[13], R_SVC[0]);
        std::swap(R[14], R_SVC[1]);
        break;

    case 0x17:
        std::swap(R[13], R_ABT[0]);
        std::swap(R[14], R_ABT[1]);
        break;

    case 0x1B:
        std::swap(R[13], R_UND[0]);
        std::swap(R[14], R_UND[1]);
        break;
    }
}

void ARM::TriggerIRQ()
{
    if (CPSR & 0x80)
        return;

    u32 oldcpsr = CPSR;
    CPSR &= ~0xFF;
    CPSR |= 0xD2;
    UpdateMode(oldcpsr, CPSR);

    R_IRQ[2] = oldcpsr;
    R[14] = R[15] + (oldcpsr & 0x20 ? 2 : 0);
    JumpTo(ExceptionBase + 0x18);
    IRQ = 0;
}

void ARMv5::PrefetchAbort()
{
    //Log(LogLevel::Warn, "ARM9: prefetch abort (%08X)\n", R[15]);

    u32 oldcpsr = CPSR;
    CPSR &= ~0xBF;
    CPSR |= 0x97;
    UpdateMode(oldcpsr, CPSR);

    // this shouldn't happen, but if it does, we're stuck in some nasty endless loop
    // so better take care of it
    /*if (!(PU_Map[ExceptionBase>>12] & 0x04))
    {
        //Log(LogLevel::Error, "!!!!! EXCEPTION REGION NOT EXECUTABLE. THIS IS VERY BAD!!\n");
        //NDS::Stop(Platform::StopReason::BadExceptionRegion);
        return;
    }*/

    R_ABT[2] = oldcpsr;
    R[14] = R[15] + (oldcpsr & 0x20 ? 2 : 0);
    JumpTo(ExceptionBase + 0x0C);
}

void ARMv5::DataAbort()
{
    //Log(LogLevel::Warn, "ARM9: data abort (%08X)\n", R[15]);

    u32 oldcpsr = CPSR;
    CPSR &= ~0xBF;
    CPSR |= 0x97;
    UpdateMode(oldcpsr, CPSR);

    R_ABT[2] = oldcpsr;
    R[14] = R[15] + (oldcpsr & 0x20 ? 4 : 0);
    JumpTo(ExceptionBase + 0x10);
}

void ARMv5::Execute()
{
    if (Halted)
    {
        if (Halted == 2)
        {
            Halted = 0;
        }
        //else if (WUP::HaltInterrupted())
        else if (IRQ)
        {
            Halted = 0;
            TriggerIRQ();
        }
        else
        {
            WUP::ARM9Timestamp = WUP::ARM9Target;
            return;
        }
    }

    while (WUP::ARM9Timestamp < WUP::ARM9Target)
    {
        if (CPSR & 0x20) // THUMB
        {
            // prefetch
            R[15] += 2;
            CurInstr = NextInstr[0];
            NextInstr[0] = NextInstr[1];
            if (R[15] & 0x2) { NextInstr[1] >>= 16; CodeCycles = 0; }
            else             NextInstr[1] = CodeRead32(R[15], false);

            // actually execute
            u32 icode = (CurInstr >> 6) & 0x3FF;
            ARMInterpreter::THUMBInstrTable[icode](this);
        }
        else
        {
            // prefetch
            R[15] += 4;
            CurInstr = NextInstr[0];
            NextInstr[0] = NextInstr[1];
            NextInstr[1] = CodeRead32(R[15], false);

            // actually execute
            if (CheckCondition(CurInstr >> 28))
            {
                u32 icode = ((CurInstr >> 4) & 0xF) | ((CurInstr >> 16) & 0xFF0);
                ARMInterpreter::ARMInstrTable[icode](this);
            }
            else if ((CurInstr & 0xFE000000) == 0xFA000000)
            {
                ARMInterpreter::A_BLX_IMM(this);
            }
            else
                AddCycles_C();
        }

        if (R[15]==0xB935E) printf("BAKA cmd=%02X\n", R[0]);

        // TODO optimize this shit!!!
        if (Halted)
        {
            if (Halted == 1 && WUP::ARM9Timestamp < WUP::ARM9Target)
            {
                WUP::ARM9Timestamp = WUP::ARM9Target;
            }
            break;
        }

        if (IRQ) TriggerIRQ();

        WUP::ARM9Timestamp += Cycles;
        WUP::RunTimers();
        Cycles = 0;
    }

    if (Halted == 2)
        Halted = 0;
}

void ARMv5::FillPipeline()
{
    //SetupCodeMem(R[15]);

    if (CPSR & 0x20)
    {
        if ((R[15] - 2) & 0x2)
        {
            NextInstr[0] = CodeRead32(R[15] - 4, false) >> 16;
            NextInstr[1] = CodeRead32(R[15], false);
        }
        else
        {
            NextInstr[0] = CodeRead32(R[15] - 2, false);
            NextInstr[1] = NextInstr[0] >> 16;
        }
    }
    else
    {
        NextInstr[0] = CodeRead32(R[15] - 4, false);
        NextInstr[1] = CodeRead32(R[15], false);
    }
}
