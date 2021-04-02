// Integer Compare Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

namespace Gekko
{

    OP(CMPI)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::cmpi]++;
        }

        int32_t a = RRA, b = SIMM;
        int crfd = CRFD;
        if (a < b) SET_CR_LT(crfd); else RESET_CR_LT(crfd);
        if (a > b) SET_CR_GT(crfd); else RESET_CR_GT(crfd);
        if (a == b) SET_CR_EQ(crfd); else RESET_CR_EQ(crfd);
        if (IS_XER_SO) SET_CR_SO(crfd); else RESET_CR_SO(crfd);
        Gekko->regs.pc += 4;
    }

    OP(CMP)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::cmp]++;
        }

        int32_t a = RRA, b = RRB;
        int crfd = CRFD;
        if (a < b) SET_CR_LT(crfd); else RESET_CR_LT(crfd);
        if (a > b) SET_CR_GT(crfd); else RESET_CR_GT(crfd);
        if (a == b) SET_CR_EQ(crfd); else RESET_CR_EQ(crfd);
        if (IS_XER_SO) SET_CR_SO(crfd); else RESET_CR_SO(crfd);
        Gekko->regs.pc += 4;
    }

    OP(CMPLI)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::cmpli]++;
        }

        uint32_t a = RRA, b = UIMM;
        int crfd = CRFD;
        if (a < b) SET_CR_LT(crfd); else RESET_CR_LT(crfd);
        if (a > b) SET_CR_GT(crfd); else RESET_CR_GT(crfd);
        if (a == b) SET_CR_EQ(crfd); else RESET_CR_EQ(crfd);
        if (IS_XER_SO) SET_CR_SO(crfd); else RESET_CR_SO(crfd);
        Gekko->regs.pc += 4;
    }

    OP(CMPL)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::cmpl]++;
        }

        uint32_t a = RRA;
        uint32_t b = RRB;
        int crfd = CRFD;
        if (a < b) SET_CR_LT(crfd); else RESET_CR_LT(crfd);
        if (a > b) SET_CR_GT(crfd); else RESET_CR_GT(crfd);
        if (a == b) SET_CR_EQ(crfd); else RESET_CR_EQ(crfd);
        if (IS_XER_SO) SET_CR_SO(crfd); else RESET_CR_SO(crfd);
        Gekko->regs.pc += 4;
    }

    // a = ra (signed)
    // b = SIMM
    // if a < b
    //      then c = 0b100
    //      else if a > b
    //          then c = 0b010
    //          else c = 0b001
    // CR[4*crf..4*crf+3] = c || XER[SO]
    void Interpreter::cmpi(AnalyzeInfo& info)
    {

    }

    // a = ra (signed)
    // b = rb (signed)
    // if a < b
    //      then c = 0b100
    //      else if a > b
    //          then c = 0b010
    //          else c = 0b001
    // CR[4*crf..4*crf+3] = c || XER[SO]
    void Interpreter::cmp(AnalyzeInfo& info)
    {

    }

    // a = ra (unsigned)
    // b = 0x0000 || UIMM
    // if a < b
    //      then c = 0b100
    //      else if a > b
    //          then c = 0b010
    //          else c = 0b001
    // CR[4*crf..4*crf+3] = c || XER[SO]
    void Interpreter::cmpli(AnalyzeInfo& info)
    {

    }

    // a = ra (unsigned)
    // b = rb (unsigned)
    // if a < b
    //      then c = 0b100
    //      else if a > b
    //          then c = 0b010
    //          else c = 0b001
    // CR[4*crf..4*crf+3] = c || XER[SO]
    void Interpreter::cmpl(AnalyzeInfo& info)
    {
        if (core->opcodeStatsEnabled)
        {
            core->opcodeStats[(size_t)Gekko::Instruction::cmpl]++;
        }

        uint32_t a = core->regs.gpr[info.paramBits[1]];
        uint32_t b = core->regs.gpr[info.paramBits[2]];
        int crfd = info.paramBits[0];
        if (a < b) SET_CR_LT(crfd); else RESET_CR_LT(crfd);
        if (a > b) SET_CR_GT(crfd); else RESET_CR_GT(crfd);
        if (a == b) SET_CR_EQ(crfd); else RESET_CR_EQ(crfd);
        if (IS_XER_SO) SET_CR_SO(crfd); else RESET_CR_SO(crfd);
        core->regs.pc += 4;
    }

}
