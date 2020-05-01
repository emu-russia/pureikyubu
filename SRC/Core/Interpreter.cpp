// Gekko interpreter
#include "pch.h"

namespace Gekko
{
    // parse and execute single opcode
    void Interpreter::ExecuteOpcode()
    {
        uint32_t op, pa;

        // execute one instruction
        // (possible CPU_EXCEPTION_DSI, ISI, ALIGN, PROGRAM, FPUNAVAIL, SYSCALL)
        pa = core->EffectiveToPhysical(Gekko->regs.pc, MmuAccess::Execute);
        if (pa == Gekko::BadAddress)
        {
            Exception(Exception::ISI);
        }
        else
        {
            MIReadWord(pa, &op);
        }
        if (exception) goto JumpPC;  // ISI
        c_1[op >> 26](op); core->ops++;
        if (exception) goto JumpPC;  // DSI, ALIGN, PROGRAM, FPUNA, SC

        if (branch && core->intFlag && (core->regs.msr & MSR_EE))
        {
            Exception(Gekko::Exception::INTERRUPT);
            goto JumpPC;
        }

        // modify CPU counters (possible CPU_EXCEPTION_DECREMENTER)
        core->Tick();
        if (branch && core->decreq && (core->regs.msr & MSR_EE))
        {
            core->decreq = false;
            Exception(Gekko::Exception::DECREMENTER);
            if (exception) goto JumpPC;
        }

        // branch or exception ?
        if (branch)
        {
        JumpPC:
            exception = false;
            branch = false;

        }
        else core->regs.pc += 4;
    }

    // For testing
    void Interpreter::ExecuteOpcodeDirect(uint32_t pc, uint32_t instr)
    {
        Gekko->regs.pc = pc;
        c_1[instr >> 26](instr);
    }

    // interpreter exception
    void Interpreter::Exception(Gekko::Exception code)
    {
        //DBReport2(DbgChannel::CPU, "Gekko Exception: #%04X\n", (uint16_t)code);

        if (exception)
        {
            DBHalt("CPU Double Fault!\n");
        }

        // save regs
      
        core->regs.spr[(int)Gekko::SPR::SRR0] = core->regs.pc;
        core->regs.spr[(int)Gekko::SPR::SRR1] = core->regs.msr;

        // Special processing for MMU
        if (code == Exception::ISI)
        {
            Gekko->regs.spr[(int)Gekko::SPR::SRR1] &= 0x0fff'ffff;

            switch (core->MmuLastResult)
            {
                case MmuResult::PageFault:
                    Gekko->regs.spr[(int)Gekko::SPR::SRR1] |= 0x4000'0000;
                    break;

                case MmuResult::ProtectedFetch:
                    Gekko->regs.spr[(int)Gekko::SPR::SRR1] |= 0x0800'0000;
                    break;

                case MmuResult::NoExecute:
                    Gekko->regs.spr[(int)Gekko::SPR::SRR1] |= 0x1000'0000;
                    break;
            }
        }
        else if (code == Exception::DSI)
        {
            Gekko->regs.spr[(int)Gekko::SPR::DSISR] = 0;

            switch (core->MmuLastResult)
            {
                case MmuResult::PageFault:
                    Gekko->regs.spr[(int)Gekko::SPR::DSISR] |= 0x4000'0000;
                    break;

                case MmuResult::ProtectedRead:
                    Gekko->regs.spr[(int)Gekko::SPR::DSISR] |= 0x0800'0000;
                    break;
                case MmuResult::ProtectedWrite:
                    Gekko->regs.spr[(int)Gekko::SPR::DSISR] |= 0x0A00'0000;
                    break;
            }
        }
        
        // Special processing for Program
        if (code == Exception::PROGRAM)
        {
            Gekko->regs.spr[(int)Gekko::SPR::SRR1] &= 0x0000'ffff;

            switch (core->PrCause)
            {
                case PrivilegedCause::FpuEnabled:
                    Gekko->regs.spr[(int)Gekko::SPR::SRR1] |= 0x0010'0000;
                    break;
                case PrivilegedCause::IllegalInstruction:
                    Gekko->regs.spr[(int)Gekko::SPR::SRR1] |= 0x0008'0000;
                    break;
                case PrivilegedCause::Privileged:
                    Gekko->regs.spr[(int)Gekko::SPR::SRR1] |= 0x0004'0000;
                    break;
                case PrivilegedCause::Trap:
                    Gekko->regs.spr[(int)Gekko::SPR::SRR1] |= 0x0002'0000;
                    break;
            }
        }

        // disable address translation
        core->regs.msr &= ~(MSR_IR | MSR_DR);

        core->regs.msr &= ~MSR_RI;

        core->regs.msr &= ~MSR_EE;

        // change PC and set exception flag
        core->regs.pc = (uint32_t)code;
        exception = true;
    }

    bool Interpreter::ExecuteInterpeterFallback()
    {
        uint32_t op, pa;

        // execute one instruction
        // (possible CPU_EXCEPTION_DSI, ISI, ALIGN, PROGRAM, FPUNAVAIL, SYSCALL)
        pa = core->EffectiveToPhysical(core->regs.pc, MmuAccess::Execute);
        if (pa == Gekko::BadAddress)
        {
            Exception(Exception::ISI);
        }
        else
        {
            MIReadWord(pa, &op);
        }
        if (exception) goto JumpPC;  // ISI
        c_1[op >> 26](op); core->ops++;
        if (exception) goto JumpPC;  // DSI, ALIGN, PROGRAM, FPUNA, SC

        core->Tick();

        if (exception)
        {
        JumpPC:
            branch = false;
            exception = false;
            return true;
        }
        
        if (branch)
        {
            branch = false;
        }
        else
        {
            core->regs.pc += 4;
        }
        return false;
    }

}
