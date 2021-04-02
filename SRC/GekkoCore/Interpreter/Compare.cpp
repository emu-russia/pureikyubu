// Integer Compare Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

namespace Gekko
{

    // if a < b
    //      then c = 0b100
    //      else if a > b
    //          then c = 0b010
    //          else c = 0b001
    // CR[4*crf..4*crf+3] = c || XER[SO]
    template <typename T1, typename T2>
    inline void Interpreter::CmpCommon(AnalyzeInfo& info, T1 a, T2 b)
    {
        int crfd = info.paramBits[0];
        if (a < b) SET_CR_LT(crfd); else RESET_CR_LT(crfd);
        if (a > b) SET_CR_GT(crfd); else RESET_CR_GT(crfd);
        if (a == b) SET_CR_EQ(crfd); else RESET_CR_EQ(crfd);
        if (IS_XER_SO) SET_CR_SO(crfd); else RESET_CR_SO(crfd);
        core->regs.pc += 4;
    }

    // a = ra (signed)
    // b = SIMM
    void Interpreter::cmpi(AnalyzeInfo& info)
    {
        if (core->opcodeStatsEnabled)
        {
            core->opcodeStats[(size_t)Gekko::Instruction::cmpi]++;
        }

        int32_t a = core->regs.gpr[info.paramBits[1]];
        int32_t b = (int32_t)info.Imm.Signed;
        CmpCommon<int32_t, int32_t>(info, a, b);
    }

    // a = ra (signed)
    // b = rb (signed)
    void Interpreter::cmp(AnalyzeInfo& info)
    {
        if (core->opcodeStatsEnabled)
        {
            core->opcodeStats[(size_t)Gekko::Instruction::cmp]++;
        }

        int32_t a = core->regs.gpr[info.paramBits[1]];
        int32_t b = core->regs.gpr[info.paramBits[2]];
        CmpCommon<int32_t, int32_t>(info, a, b);
    }

    // a = ra (unsigned)
    // b = 0x0000 || UIMM
    void Interpreter::cmpli(AnalyzeInfo& info)
    {
        if (core->opcodeStatsEnabled)
        {
            core->opcodeStats[(size_t)Gekko::Instruction::cmpli]++;
        }

        uint32_t a = core->regs.gpr[info.paramBits[1]];
        uint32_t b = info.Imm.Unsigned;
        CmpCommon<uint32_t, uint32_t>(info, a, b);
    }

    // a = ra (unsigned)
    // b = rb (unsigned)
    void Interpreter::cmpl(AnalyzeInfo& info)
    {
        if (core->opcodeStatsEnabled)
        {
            core->opcodeStats[(size_t)Gekko::Instruction::cmpl]++;
        }

        uint32_t a = core->regs.gpr[info.paramBits[1]];
        uint32_t b = core->regs.gpr[info.paramBits[2]];
        CmpCommon<uint32_t, uint32_t>(info, a, b);
    }

}
