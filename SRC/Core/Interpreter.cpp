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
        pa = GCEffectiveToPhysical(core->regs.pc, true);
        MIReadWord(pa, &op);
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
        pa = GCEffectiveToPhysical(core->regs.pc, true);
        MIReadWord(pa, &op);
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
