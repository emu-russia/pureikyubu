// Condition Register Logical Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

namespace Gekko
{

    // CR[crbd] = CR[crba] & CR[crbb]
    OP(CRAND)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::crand]++;
        }

        uint32_t crbd = CRBD, crba = CRBA, crbb = CRBB;

        uint32_t a = (Gekko->regs.cr >> (31 - crba)) & 1;
        uint32_t b = (Gekko->regs.cr >> (31 - crbb)) & 1;
        uint32_t d = (a & b) << (31 - crbd);     // <- crop is here
        uint32_t m = ~(1 << (31 - crbd));
        Gekko->regs.cr = (Gekko->regs.cr & m) | d;
        Gekko->regs.pc += 4;
    }

    // CR[crbd] = CR[crba] | CR[crbb]
    OP(CROR)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::cror]++;
        }

        uint32_t crbd = CRBD, crba = CRBA, crbb = CRBB;

        uint32_t a = (Gekko->regs.cr >> (31 - crba)) & 1;
        uint32_t b = (Gekko->regs.cr >> (31 - crbb)) & 1;
        uint32_t d = (a | b) << (31 - crbd);     // <- crop is here
        uint32_t m = ~(1 << (31 - crbd));
        Gekko->regs.cr = (Gekko->regs.cr & m) | d;
        Gekko->regs.pc += 4;
    }

    // CR[crbd] = CR[crba] ^ CR[crbb]
    OP(CRXOR)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::crxor]++;
        }

        uint32_t crbd = CRBD, crba = CRBA, crbb = CRBB;

        uint32_t a = (Gekko->regs.cr >> (31 - crba)) & 1;
        uint32_t b = (Gekko->regs.cr >> (31 - crbb)) & 1;
        uint32_t d = (a ^ b) << (31 - crbd);     // <- crop is here
        uint32_t m = ~(1 << (31 - crbd));
        Gekko->regs.cr = (Gekko->regs.cr & m) | d;
        Gekko->regs.pc += 4;
    }

    // CR[crbd] = !(CR[crba] & CR[crbb])
    OP(CRNAND)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::crnand]++;
        }

        uint32_t crbd = CRBD, crba = CRBA, crbb = CRBB;

        uint32_t a = (Gekko->regs.cr >> (31 - crba)) & 1;
        uint32_t b = (Gekko->regs.cr >> (31 - crbb)) & 1;
        uint32_t d = (!(a & b)) << (31 - crbd);     // <- crop is here
        uint32_t m = ~(1 << (31 - crbd));
        Gekko->regs.cr = (Gekko->regs.cr & m) | d;
        Gekko->regs.pc += 4;
    }

    // CR[crbd] = !(CR[crba] | CR[crbb])
    OP(CRNOR)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::crnor]++;
        }

        uint32_t crbd = CRBD, crba = CRBA, crbb = CRBB;

        uint32_t a = (Gekko->regs.cr >> (31 - crba)) & 1;
        uint32_t b = (Gekko->regs.cr >> (31 - crbb)) & 1;
        uint32_t d = (!(a | b)) << (31 - crbd);     // <- crop is here
        uint32_t m = ~(1 << (31 - crbd));
        Gekko->regs.cr = (Gekko->regs.cr & m) | d;
        Gekko->regs.pc += 4;
    }

    // CR[crbd] = CR[crba] EQV CR[crbb]
    OP(CREQV)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::creqv]++;
        }

        uint32_t crbd = CRBD, crba = CRBA, crbb = CRBB;

        uint32_t a = (Gekko->regs.cr >> (31 - crba)) & 1;
        uint32_t b = (Gekko->regs.cr >> (31 - crbb)) & 1;
        uint32_t d = (!(a ^ b)) << (31 - crbd);     // <- crop is here
        uint32_t m = ~(1 << (31 - crbd));
        Gekko->regs.cr = (Gekko->regs.cr & m) | d;
        Gekko->regs.pc += 4;
    }

    // CR[crbd] = CR[crba] & ~CR[crbb]
    OP(CRANDC)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::crandc]++;
        }

        uint32_t crbd = CRBD, crba = CRBA, crbb = CRBB;

        uint32_t a = (Gekko->regs.cr >> (31 - crba)) & 1;
        uint32_t b = (Gekko->regs.cr >> (31 - crbb)) & 1;
        uint32_t d = (a & (~b)) << (31 - crbd);     // <- crop is here
        uint32_t m = ~(1 << (31 - crbd));
        Gekko->regs.cr = (Gekko->regs.cr & m) | d;
        Gekko->regs.pc += 4;
    }

    // CR[crbd] = CR[crba] | ~CR[crbb]
    OP(CRORC)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::crorc]++;
        }

        uint32_t crbd = CRBD, crba = CRBA, crbb = CRBB;

        uint32_t a = (Gekko->regs.cr >> (31 - crba)) & 1;
        uint32_t b = (Gekko->regs.cr >> (31 - crbb)) & 1;
        uint32_t d = (a | (~b)) << (31 - crbd);     // <- crop is here
        uint32_t m = ~(1 << (31 - crbd));
        Gekko->regs.cr = (Gekko->regs.cr & m) | d;
        Gekko->regs.pc += 4;
    }

    // CR[4*crfd .. 4*crfd + 3] = CR[4*crfs .. 4*crfs + 3]
    OP(MCRF)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::mcrf]++;
        }

        int32_t crfd = 4 * (7 - CRFD), crfs = 4 * (7 - CRFS);
        Gekko->regs.cr = (Gekko->regs.cr & (~(0xf << crfd))) | (((Gekko->regs.cr >> crfs) & 0xf) << crfd);
        Gekko->regs.pc += 4;
    }

}
