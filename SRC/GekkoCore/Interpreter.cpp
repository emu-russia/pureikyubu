// Gekko interpreter
#include "pch.h"

namespace Gekko
{
    // parse and execute single opcode
    void Interpreter::ExecuteOpcode()
    {
        int WIMG;
        uint32_t instr = 0, pa;

        // execute one instruction
        // (possible CPU_EXCEPTION_DSI, ISI, ALIGN, PROGRAM, FPUNAVAIL, SYSCALL)
        pa = core->EffectiveToPhysical(core->regs.pc, MmuAccess::Execute, WIMG);
        if (pa == Gekko::BadAddress)
        {
            core->Exception(Exception::ISI);
        }
        else
        {
            MIReadWord(pa, &instr);
        }
        // ISI
        if (core->exception)
        {
            core->exception = false;
            return;
        }
        
        // Decode instruction using GekkoAnalyzer and dispatch

        AnalyzeInfo info;
        Analyzer::AnalyzeFast(core->regs.pc, instr, &info);
        Dispatch(info, instr);
        core->ops++;

        if (core->resetInstructionCounter)
        {
            core->resetInstructionCounter = false;
            core->ops = 0;
        }
        // DSI, ALIGN, PROGRAM, FPUNA, SC
        if (core->exception)
        {
            core->exception = false;
            return;
        }

        core->Tick();

        core->exception = false;
    }

    // For testing
    void Interpreter::ExecuteOpcodeDirect(uint32_t pc, uint32_t instr)
    {
        core->regs.pc = pc;
        AnalyzeInfo info;
        Analyzer::AnalyzeFast(core->regs.pc, instr, &info);
        Dispatch(info, instr);
    }

    bool Interpreter::ExecuteInterpeterFallback()
    {
        int WIMG;
        uint32_t instr = 0, pa;

        // execute one instruction
        // (possible CPU_EXCEPTION_DSI, ISI, ALIGN, PROGRAM, FPUNAVAIL, SYSCALL)
        pa = core->EffectiveToPhysical(core->regs.pc, MmuAccess::Execute, WIMG);
        if (pa == Gekko::BadAddress)
        {
            core->Exception(Exception::ISI);
        }
        else
        {
            MIReadWord(pa, &instr);
        }
        if (core->exception) goto JumpPC;  // ISI
        
        // Decode instruction using GekkoAnalyzer and dispatch

        AnalyzeInfo info;
        Analyzer::AnalyzeFast(core->regs.pc, instr, &info);
        Dispatch(info, instr);
        core->ops++;

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

    void Interpreter::Dispatch(AnalyzeInfo& info, uint32_t instr)
    {
        switch (info.instr)
        {
            case Instruction::cmpi: cmpi(info); break;
            case Instruction::cmp: cmp(info); break;
            case Instruction::cmpli: cmpli(info); break;
            case Instruction::cmpl: cmpl(info); break;

            // At the time of developing a new interpreter, a fallback to the old implementation will be made in this place.

            default:
                c_1[instr >> 26](instr);
                break;
        }
    }

    uint32_t Interpreter::GetRotMask(int mb, int me)
    {
        return rotmask[mb][me];
    }

}
