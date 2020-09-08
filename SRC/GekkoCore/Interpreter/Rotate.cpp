// Integer Rotate Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

namespace Gekko
{

    // n = SH
    // r = ROTL(rs, n)
    // m = MASK(MB, ME)
    // ra = r & m
    // CR0 (if .)
    OP(RLWINM)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::rlwinm]++;
        }

        uint32_t m = Gekko->interp->rotmask[MB][ME];
        uint32_t r = Rotl32(SH, RRS);
        uint32_t res = r & m;
        RRA = res;
        if (op & 1) COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

    // n = rb[27-31]
    // r = ROTL(rs, n)
    // m = MASK(MB, ME)
    // ra = r & m
    OP(RLWNM)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::rlwnm]++;
        }

        uint32_t m = Gekko->interp->rotmask[MB][ME];
        uint32_t r = Rotl32(RRB & 0x1f, RRS);
        uint32_t res = r & m;
        RRA = res;
        if (op & 1) COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

    // n = SH
    // r = ROTL(rs, n)
    // m = MASK(mb, me)
    // ra = (r & m) | (ra & ~m)
    // CR0 (if .)
    OP(RLWIMI)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::rlwimi]++;
        }

        uint32_t m = Gekko->interp->rotmask[MB][ME];
        uint32_t r = Rotl32(SH, RRS);
        uint32_t res = (r & m) | (RRA & ~m);
        RRA = res;
        if (op & 1) COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

}
