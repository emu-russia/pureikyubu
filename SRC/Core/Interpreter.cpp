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
        pa = GCEffectiveToPhysical(Gekko->regs.pc, true);
        MIReadWord(pa, &op);
        if (exception) goto JumpPC;  // ISI
        c_1[op >> 26](op); Gekko->ops++;
        if (exception) goto JumpPC;  // DSI, ALIGN, PROGRAM, FPUNA, SC

        if (branch && core->intFlag && (Gekko->regs.msr & MSR_EE))
        {
            Exception(Gekko::Exception::INTERRUPT);
            goto JumpPC;
        }

        // modify CPU counters (possible CPU_EXCEPTION_DECREMENTER)
        core->Tick();
        if (branch && Gekko->decreq && (Gekko->regs.msr & MSR_EE))
        {
            Gekko->decreq = 0;
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
        else Gekko->regs.pc += 4;
    }

    // interpreter exception
    void Interpreter::Exception(Gekko::Exception code)
    {
        if (exception)
        {
            DBHalt("CPU Double Fault!\n");
        }

        // save regs
      
        Gekko->regs.spr[(int)Gekko::SPR::SRR0] = Gekko->regs.pc;
        Gekko->regs.spr[(int)Gekko::SPR::SRR1] = Gekko->regs.msr;

        // disable address translation
        Gekko->regs.msr &= ~(MSR_IR | MSR_DR);

        // Gekko exceptions are always recoverable
        Gekko->regs.msr |= MSR_RI;

        Gekko->regs.msr &= ~MSR_EE;

        // change PC and set exception flag
        Gekko->regs.pc = (uint32_t)code;
        exception = true;
    }

}
