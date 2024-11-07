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

#ifndef ARM_H
#define ARM_H

#include <algorithm>

#include "types.h"
#include "WUP.h"

inline u32 ROR(u32 x, u32 n)
{
    return (x >> (n&0x1F)) | (x << ((32-n)&0x1F));
}

enum
{
    RWFlags_Nonseq = (1<<5),
    RWFlags_ForceUser = (1<<21),
};

// TODO: merge ARMv5 into ARM?

class ARM
{
public:
    ARM(u32 num);
    virtual ~ARM(); // destroy shit

    virtual void Reset();
    void SoftReset();

    virtual void FillPipeline() = 0;

    virtual void JumpTo(u32 addr, bool restorecpsr = false) = 0;
    void RestoreCPSR();

    void Halt(u32 halt)
    {
        if (halt==2 && Halted==1) return;
        Halted = halt;
    }

    virtual void Execute() = 0;

    bool CheckCondition(u32 code)
    {
        if (code == 0xE) return true;
        if (ConditionTable[code] & (1 << (CPSR>>28))) return true;
        return false;
    }

    void SetC(bool c)
    {
        if (c) CPSR |= 0x20000000;
        else CPSR &= ~0x20000000;
    }

    void SetNZ(bool n, bool z)
    {
        CPSR &= ~0xC0000000;
        if (n) CPSR |= 0x80000000;
        if (z) CPSR |= 0x40000000;
    }

    void SetNZCV(bool n, bool z, bool c, bool v)
    {
        CPSR &= ~0xF0000000;
        if (n) CPSR |= 0x80000000;
        if (z) CPSR |= 0x40000000;
        if (c) CPSR |= 0x20000000;
        if (v) CPSR |= 0x10000000;
    }

    void UpdateMode(u32 oldmode, u32 newmode, bool phony = false);

    void TriggerIRQ();

    void SetupCodeMem(u32 addr);


    virtual void DataRead8(u32 addr, u32* val) = 0;
    virtual void DataRead16(u32 addr, u32* val) = 0;
    virtual void DataRead32(u32 addr, u32* val) = 0;
    virtual void DataRead32S(u32 addr, u32* val) = 0;
    virtual void DataWrite8(u32 addr, u8 val) = 0;
    virtual void DataWrite16(u32 addr, u16 val) = 0;
    virtual void DataWrite32(u32 addr, u32 val) = 0;
    virtual void DataWrite32S(u32 addr, u32 val) = 0;

    virtual void AddCycles_C() = 0;
    virtual void AddCycles_CI(s32 numI) = 0;
    virtual void AddCycles_CDI() = 0;
    virtual void AddCycles_CD() = 0;


    u32 Num;

    s32 Cycles;
    union
    {
        struct
        {
            u8 Halted;
            u8 IRQ; // nonzero to trigger IRQ
            u8 IdleLoop;
        };
        u32 StopExecution;
    };

    u32 CodeRegion;
    s32 CodeCycles;

    u32 DataRegion;
    s32 DataCycles;

    u32 R[16]; // heh
    u32 CPSR;
    u32 R_FIQ[8]; // holding SPSR too
    u32 R_SVC[3];
    u32 R_ABT[3];
    u32 R_IRQ[3];
    u32 R_UND[3];
    u32 CurInstr;
    u32 NextInstr[2];

    u32 ExceptionBase;

    WUP::MemRegion CodeMem;

    static u32 ConditionTable[16];
};

class ARMv5 : public ARM
{
public:
    ARMv5();
    ~ARMv5();

    void Reset();

    void UpdateRegionTimings(u32 addrstart, u32 addrend);

    void FillPipeline();

    void JumpTo(u32 addr, bool restorecpsr = false);

    void PrefetchAbort();
    void DataAbort();

    void Execute();
#ifdef JIT_ENABLED
    void ExecuteJIT();
#endif

    // all code accesses are forced nonseq 32bit
    u32 CodeRead32(u32 addr, bool branch);

    void DataRead8(u32 addr, u32* val);
    void DataRead16(u32 addr, u32* val);
    void DataRead32(u32 addr, u32* val);
    void DataRead32S(u32 addr, u32* val);
    void DataWrite8(u32 addr, u8 val);
    void DataWrite16(u32 addr, u16 val);
    void DataWrite32(u32 addr, u32 val);
    void DataWrite32S(u32 addr, u32 val);

    void AddCycles_C()
    {
        // code only. always nonseq 32-bit for ARM9.
        s32 numC = (R[15] & 0x2) ? 0 : CodeCycles;
        Cycles += numC;
    }

    void AddCycles_CI(s32 numI)
    {
        // code+internal
        s32 numC = (R[15] & 0x2) ? 0 : CodeCycles;
        Cycles += numC + numI;
    }

    void AddCycles_CDI()
    {
        // LDR/LDM cycles. ARM9 seems to skip the internal cycle there.
        // TODO: ITCM data fetches shouldn't be parallelized, they say
        s32 numC = (R[15] & 0x2) ? 0 : CodeCycles;
        s32 numD = DataCycles;

        //if (DataRegion != CodeRegion)
            Cycles += std::max(numC + numD - 6, std::max(numC, numD));
        //else
        //    Cycles += numC + numD;
    }

    void AddCycles_CD()
    {
        // TODO: ITCM data fetches shouldn't be parallelized, they say
        s32 numC = (R[15] & 0x2) ? 0 : CodeCycles;
        s32 numD = DataCycles;

        //if (DataRegion != CodeRegion)
            Cycles += std::max(numC + numD - 6, std::max(numC, numD));
        //else
        //    Cycles += numC + numD;
    }

    void GetCodeMemRegion(u32 addr, WUP::MemRegion* region);

    void CP15Reset();

    u32 RandomLineIndex();

    void ICacheLookup(u32 addr);
    void ICacheInvalidateByAddr(u32 addr);
    void ICacheInvalidateAll();

    void CP15Write(u32 id, u32 val);
    u32 CP15Read(u32 id);

    u32 CP15Control;

    u32 RNGSeed;

    s32 RegionCodeCycles;

    u8 ICache[0x2000];
    u32 ICacheTags[64*4];
    u8 ICacheCount[64];

    // code/16N/32N/32S
    u8 MemTimings[0x100000][4];

    u8* CurICacheLine;
};

namespace ARMInterpreter
{

void A_UNK(ARM* cpu);
void T_UNK(ARM* cpu);

}

namespace WUP
{

extern ARMv5* ARM9;

}

#endif // ARM_H