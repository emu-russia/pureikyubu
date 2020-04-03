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
        pa = GCEffectiveToPhysical(PC, true);
        MIReadWord(pa, &op);
        if (cpu.exception) goto JumpPC;  // ISI
        c_1[op >> 26](op); cpu.ops++;
        if (cpu.exception) goto JumpPC;  // DSI, ALIGN, PROGRAM, FPUNA, SC

        if (cpu.branch && core->intFlag && (MSR & MSR_EE))
        {
            Exception(CPU_EXCEPTION_INTERRUPT);
            goto JumpPC;
        }

        // modify CPU counters (possible CPU_EXCEPTION_DECREMENTER)
        core->Tick();
        if (cpu.branch && cpu.decreq && (MSR & MSR_EE))
        {
            cpu.decreq = 0;
            Exception(CPU_EXCEPTION_DECREMENTER);
            if (cpu.exception) goto JumpPC;
        }

        // branch or exception ?
        if (cpu.branch)
        {
        JumpPC:
            cpu.exception = false;
            cpu.branch = false;

        }
        else PC += 4;
    }

    // interpreter exception
    void Interpreter::Exception(uint32_t code)
    {
        if (cpu.exception)
        {
            DBHalt("CPU Double Fault!\n");
        }

        // save regs
        SRR0 = PC;
        SRR1 = MSR;

        // disable address translation
        MSR &= ~(MSR_IR | MSR_DR);

        // Gekko exceptions are always recoverable
        MSR |= MSR_RI;

        MSR &= ~MSR_EE;

        // change PC and set exception flag
        PC = code;
        cpu.exception = true;
    }

}
