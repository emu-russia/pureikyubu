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
            switch (core->MmuLastResult)
            {
                case MmuResult::PageFault:
                    Gekko->regs.spr[(int)Gekko::SPR::SRR1] |= 0x4000'0000;
                    break;

                case MmuResult::Protected:
                    Gekko->regs.spr[(int)Gekko::SPR::SRR1] |= 0x0800'0000;
                    break;

                case MmuResult::NoExecute:
                    Gekko->regs.spr[(int)Gekko::SPR::SRR1] |= 0x1000'0000;
                    break;
            }
        }
        else if (code == Exception::DSI)
        {
            switch (core->MmuLastResult)
            {
                case MmuResult::PageFault:
                    Gekko->regs.spr[(int)Gekko::SPR::DSISR] |= 0x4000'0000;
                    break;

                case MmuResult::Protected:
                    Gekko->regs.spr[(int)Gekko::SPR::DSISR] |= 0x0800'0000;
                    break;
            }
        }

        // disable address translation
        core->regs.msr &= ~(MSR_IR | MSR_DR);

        // Gekko exceptions are always recoverable
        core->regs.msr |= MSR_RI;

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
