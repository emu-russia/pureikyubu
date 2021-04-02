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

            case Instruction::crand: crand(info); break;
            case Instruction::crandc: crandc(info); break;
            case Instruction::creqv: creqv(info); break;
            case Instruction::crnand: crnand(info); break;
            case Instruction::crnor: crnor(info); break;
            case Instruction::cror: cror(info); break;
            case Instruction::crorc: crorc(info); break;
            case Instruction::crxor: crxor(info); break;
            case Instruction::mcrf: mcrf(info); break;

            case Instruction::rlwimi: rlwimi(info); break;
            case Instruction::rlwimi_d: rlwimi_d(info); break;
            case Instruction::rlwinm: rlwinm(info); break;
            case Instruction::rlwinm_d: rlwinm_d(info); break;
            case Instruction::rlwnm: rlwnm(info); break;
            case Instruction::rlwnm_d: rlwnm_d(info); break;

            case Instruction::slw: slw(info); break;
            case Instruction::slw_d: slw_d(info); break;
            case Instruction::sraw: sraw(info); break;
            case Instruction::sraw_d: sraw_d(info); break;
            case Instruction::srawi: srawi(info); break;
            case Instruction::srawi_d: srawi_d(info); break;
            case Instruction::srw: srw(info); break;
            case Instruction::srw_d: srw_d(info); break;

            // At the time of developing a new interpreter, a fallback to the old implementation will be made in this place.

            default:
                c_1[instr >> 26](instr);
                break;
        }

        // TODO: Move here opcode usage statistics when all handlers have been redone. 

        //if (Gekko->opcodeStatsEnabled)
        //{
        //    Gekko->opcodeStats[(size_t)Gekko::Instruction::sraw_d]++;
        //}

    }

    uint32_t Interpreter::GetRotMask(int mb, int me)
    {
        return rotmask[mb][me];
    }

}
