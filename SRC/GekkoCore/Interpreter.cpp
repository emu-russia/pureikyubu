// Gekko interpreter
#include "pch.h"

namespace Gekko
{
    // parse and execute single opcode
    void Interpreter::ExecuteOpcode()
    {
        int WIMG;
        uint32_t op, pa;

        // execute one instruction
        // (possible CPU_EXCEPTION_DSI, ISI, ALIGN, PROGRAM, FPUNAVAIL, SYSCALL)
        //pa = core->EffectiveToPhysical(Gekko->regs.pc, MmuAccess::Execute);
        pa = core->EffectiveToPhysical(Gekko->regs.pc, MmuAccess::Execute, WIMG);
        if (pa == Gekko::BadAddress)
        {
            Gekko->Exception(Exception::ISI);
        }
        else
        {
            MIReadWord(pa, &op);
        }
        // ISI
        if (core->exception)
        {
            core->exception = false;
            return;
        }
        c_1[op >> 26](op);
        core->ops++;
        // DSI, ALIGN, PROGRAM, FPUNA, SC
        if (core->exception)
        {
            core->exception = false;
            return;
        }

        Gekko->Tick();

        core->exception = false;
    }

    // For testing
    void Interpreter::ExecuteOpcodeDirect(uint32_t pc, uint32_t instr)
    {
        Gekko->regs.pc = pc;
        c_1[instr >> 26](instr);
    }

    bool Interpreter::ExecuteInterpeterFallback()
    {
        int WIMG;
        uint32_t op, pa;

        // execute one instruction
        // (possible CPU_EXCEPTION_DSI, ISI, ALIGN, PROGRAM, FPUNAVAIL, SYSCALL)
        //pa = core->EffectiveToPhysical(core->regs.pc, MmuAccess::Execute);
        pa = core->EffectiveToPhysical(Gekko->regs.pc, MmuAccess::Execute, WIMG);
        if (pa == Gekko::BadAddress)
        {
            Gekko->Exception(Exception::ISI);
        }
        else
        {
            MIReadWord(pa, &op);
        }
        if (core->exception) goto JumpPC;  // ISI
        c_1[op >> 26](op); core->ops++;
        if (core->exception) goto JumpPC;  // DSI, ALIGN, PROGRAM, FPUNA, SC

        core->Tick();

        if (core->exception)
        {
        JumpPC:
            core->exception = false;
            return true;
        }

        return false;
    }

    uint32_t Interpreter::GetRotMask(int mb, int me)
    {
        return rotmask[mb][me];
    }

}
