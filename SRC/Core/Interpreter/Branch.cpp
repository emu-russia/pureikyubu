// Branch Instructions
#include "pch.h"
#include "InterpreterPrivate.h"

namespace Gekko
{

    void Interpreter::BranchCheck()
    {
        if (Gekko->intFlag && (Gekko->regs.msr & MSR_EE))
        {
            Gekko->Exception(Gekko::Exception::INTERRUPT);
            Gekko->exception = false;
            return;
        }

        // modify CPU counters (possible CPU_EXCEPTION_DECREMENTER)
        Gekko->Tick();
        if (Gekko->decreq && (Gekko->regs.msr & MSR_EE))
        {
            Gekko->decreq = false;
            Gekko->Exception(Gekko::Exception::DECREMENTER);
        }
    }

    // PC = PC + EXTS(LI || 0b00)
    OP(B)
    {
        uint32_t target = op & 0x03fffffc;
        if (target & 0x02000000) target |= 0xfc000000;
        Gekko->regs.pc = Gekko->regs.pc + target;
    }

    // PC = EXTS(LI || 0b00)
    OP(BA)
    {
        uint32_t target = op & 0x03fffffc;
        if (target & 0x02000000) target |= 0xfc000000;
        Gekko->regs.pc = target;
    }

    // LR = PC + 4, PC = PC + EXTS(LI || 0b00)
    OP(BL)
    {
        uint32_t target = op & 0x03fffffc;
        if (target & 0x02000000) target |= 0xfc000000;
        Gekko->regs.spr[(int)SPR::LR] = Gekko->regs.pc + 4;
        Gekko->regs.pc = Gekko->regs.pc + target;
    }

    // LR = PC + 4, PC = EXTS(LI || 0b00)
    OP(BLA)
    {
        uint32_t target = op & 0x03fffffc;
        if (target & 0x02000000) target |= 0xfc000000;
        Gekko->regs.spr[(int)SPR::LR] = Gekko->regs.pc + 4;
        Gekko->regs.pc = target;
    }

    OP(BX)
    {
        bx[op & 3](op);
        BranchCheck();
    }

    // ---------------------------------------------------------------------------

    // calculation of conditional branch
    static bool bc(uint32_t op)
    {
        bool ctr_ok, cond_ok;
        int bo = RD, bi = BI;

        if (BO(2) == 0)
        {
            Gekko->regs.spr[(int)SPR::CTR]--;

            if (BO(3)) ctr_ok = (Gekko->regs.spr[(int)Gekko::SPR::CTR] == 0);
            else ctr_ok = (Gekko->regs.spr[(int)Gekko::SPR::CTR] != 0);
        }
        else ctr_ok = true;

        if (BO(0) == 0)
        {
            if (BO(1)) cond_ok = ((Gekko->regs.cr << bi) & 0x80000000) != 0;
            else cond_ok = ((Gekko->regs.cr << bi) & 0x80000000) == 0;
        }
        else cond_ok = true;

        return (ctr_ok & cond_ok);
    }

    // if ~BO2 then CTR = CTR - 1
    // ctr_ok  = BO2 | ((CTR != 0) ^ BO3)
    // cond_ok = BO0 | (CR[BI] EQV BO1)
    // if ctr_ok & cond_ok then
    //      if LK = 1
    //          LR = PC + 4
    //      if AA = 1
    //          then PC = EXTS(BD || 0b00)
    //          else PC = PC + EXTS(BD || 0b00)
    OP(BCX)
    {
        if (bc(op))
        {
            if (op & 1) Gekko->regs.spr[(int)SPR::LR] = Gekko->regs.pc + 4; // LK

            uint32_t target = op & 0xfffc;
            if (target & 0x8000) target |= 0xffff0000;
            if (op & 2) Gekko->regs.pc = target; // AA
            else Gekko->regs.pc += target;
            BranchCheck();
        }
        else
        {
            Gekko->regs.pc += 4;
        }
    }

    // if ~BO2 then CTR = CTR - 1
    // ctr_ok  = BO2 | ((CTR != 0) ^ BO3)
    // cond_ok = BO0 | (CR[BI] EQV BO1)
    // if ctr_ok & cond_ok then
    //      PC = LR[0-29] || 0b00
    OP(BCLR)
    {
        if (bc(op))
        {
            Gekko->regs.pc = Gekko->regs.spr[(int)SPR::LR] & ~3;
            BranchCheck();
        }
        else
        {
            Gekko->regs.pc += 4;
        }
    }

    // if ~BO2 then CTR = CTR - 1
    // ctr_ok  = BO2 | ((CTR != 0) ^ BO3)
    // cond_ok = BO0 | (CR[BI] EQV BO1)
    // if ctr_ok & cond_ok then
    //      NLR = PC + 4
    //      PC = LR[0-29] || 0b00
    //      LR = NLR
    OP(BCLRL)
    {
        if (bc(op))
        {
            uint32_t lr = Gekko->regs.pc + 4;
            Gekko->regs.pc = Gekko->regs.spr[(int)SPR::LR] & ~3;
            Gekko->regs.spr[(int)SPR::LR] = lr;
            BranchCheck();
        }
        else
        {
            Gekko->regs.pc += 4;
        }
    }

    // ---------------------------------------------------------------------------

    // calculation of conditional to count register branch
    static bool bctr(uint32_t op)
    {
        bool cond_ok;
        int bo = RD, bi = BI;

        if (BO(0) == 0)
        {
            if (BO(1)) cond_ok = ((Gekko->regs.cr << bi) & 0x80000000) != 0;
            else cond_ok = ((Gekko->regs.cr << bi) & 0x80000000) == 0;
        }
        else cond_ok = true;

        return cond_ok;
    }

    // cond_ok = BO0 | (CR[BI] EQV BO1)
    // if cond_ok
    //      then
    //              PC = CTR || 0b00
    OP(BCCTR)
    {
        if (bctr(op))
        {
            Gekko->regs.pc = Gekko->regs.spr[(int)SPR::CTR] & ~3;
            BranchCheck();
        }
        else
        {
            Gekko->regs.pc += 4;
        }
    }

    // cond_ok = BO0 | (CR[BI] EQV BO1)
    // if cond_ok
    //      then
    //              LR = PC + 4
    //              PC = CTR || 0b00
    OP(BCCTRL)
    {
        if (bctr(op))
        {
            Gekko->regs.spr[(int)SPR::LR] = Gekko->regs.pc + 4;
            Gekko->regs.pc = Gekko->regs.spr[(int)SPR::CTR] & ~3;
            BranchCheck();
        }
        else
        {
            Gekko->regs.pc += 4;
        }
    }
}
