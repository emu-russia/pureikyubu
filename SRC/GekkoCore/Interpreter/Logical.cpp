// Logical Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

namespace Gekko
{

    // ra = rs & UIMM, CR0
    OP(ANDID)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::andi_d]++;
        }

        uint32_t res = RRS & UIMM;
        RRA = res;
        COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

    // ra = rs & (UIMM || 0x0000), CR0
    OP(ANDISD)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::andis_d]++;
        }

        uint32_t res = RRS & (op << 16);
        RRA = res;
        COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

    // ra = rs | (0x0000 || UIMM)
    OP(ORI)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::ori]++;
        }

        RRA = RRS | UIMM;
        Gekko->regs.pc += 4;
    }

    // ra = rs | (UIMM || 0x0000)
    OP(ORIS)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::oris]++;
        }

        RRA = RRS | (op << 16);
        Gekko->regs.pc += 4;
    }

    // ra = rs ^ (0x0000 || UIMM)
    OP(XORI)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::xori]++;
        }

        RRA = RRS ^ UIMM;
        Gekko->regs.pc += 4;
    }

    // ra = rs ^ (UIMM || 0x0000)
    OP(XORIS)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::xoris]++;
        }

        RRA = RRS ^ (op << 16);
        Gekko->regs.pc += 4;
    }

    // ra = rs & rb
    OP(AND)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::_and]++;
        }

        RRA = RRS & RRB;
        Gekko->regs.pc += 4;
    }

    // ra = rs & rb, CR0
    OP(ANDD)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::and_d]++;
        }

        uint32_t res = RRS & RRB;
        RRA = res;
        COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

    // ra = rs | rb
    OP(OR)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::_or]++;
        }

        RRA = RRS | RRB;
        Gekko->regs.pc += 4;
    }

    // ra = rs | rb, CR0
    OP(ORD)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::or_d]++;
        }

        uint32_t res = RRS | RRB;
        RRA = res;
        COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

    // ra = rs ^ rb
    OP(XOR)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::_xor]++;
        }

        RRA = RRS ^ RRB;
        Gekko->regs.pc += 4;
    }

    // ra = rs ^ rb, CR0
    OP(XORD)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::xor_d]++;
        }

        uint32_t res = RRS ^ RRB;
        RRA = res;
        COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

    // ra = ~(rs & rb)
    OP(NAND)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::nand]++;
        }

        RRA = ~(RRS & RRB);
        Gekko->regs.pc += 4;
    }

    // ra = ~(rs & rb), CR0
    OP(NANDD)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::nand_d]++;
        }

        uint32_t res = ~(RRS & RRB);
        RRA = res;
        COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

    // ra = ~(rs | rb)
    OP(NOR)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::nor]++;
        }

        RRA = ~(RRS | RRB);
        Gekko->regs.pc += 4;
    }

    // ra = ~(rs | rb), CR0
    OP(NORD)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::nor_d]++;
        }

        uint32_t res = ~(RRS | RRB);
        RRA = res;
        COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

    // ra = rs EQV rb
    OP(EQV)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::eqv]++;
        }

        RRA = ~(RRS ^ RRB);
        Gekko->regs.pc += 4;
    }

    // ra = rs EQV rb, CR0
    OP(EQVD)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::eqv_d]++;
        }

        uint32_t res = ~(RRS ^ RRB);
        RRA = res;
        COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

    // ra = rs & ~rb
    OP(ANDC)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::andc]++;
        }

        RRA = RRS & (~RRB);
        Gekko->regs.pc += 4;
    }

    // ra = rs & ~rb, CR0
    OP(ANDCD)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::andc_d]++;
        }

        uint32_t res = RRS & (~RRB);
        RRA = res;
        COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

    // ra = rs | ~rb
    OP(ORC)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::orc]++;
        }

        RRA = RRS | (~RRB);
        Gekko->regs.pc += 4;
    }

    // ra = rs | ~rb, CR0
    OP(ORCD)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::orc_d]++;
        }

        uint32_t res = RRS | (~RRB);
        RRA = res;
        COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

    // sign = rs[24]
    // ra[24-31] = rs[24-31]
    // ra[0-23] = (24)sign
    OP(EXTSB)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::extsb]++;
        }

        RRA = (uint32_t)(int32_t)(int8_t)(uint8_t)RRS;
        Gekko->regs.pc += 4;
    }

    // sign = rs[24]
    // ra[24-31] = rs[24-31]
    // ra[0-23] = (24)sign
    // CR0
    OP(EXTSBD)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::extsb_d]++;
        }

        uint32_t res = (uint32_t)(int32_t)(int8_t)(uint8_t)RRS;
        RRA = res;
        COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

    // sign = rs[16]
    // ra[16-31] = rs[16-31]
    // ra[0-15] = (16)sign
    OP(EXTSH)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::extsh]++;
        }

        RRA = (uint32_t)(int32_t)(int16_t)(uint16_t)RRS;
        Gekko->regs.pc += 4;
    }

    // sign = rs[16]
    // ra[16-31] = rs[16-31]
    // ra[0-15] = (16)sign
    // CR0
    OP(EXTSHD)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::extsh_d]++;
        }

        uint32_t res = (uint32_t)(int32_t)(int16_t)(uint16_t)RRS;
        RRA = res;
        COMPUTE_CR0(res);
        Gekko->regs.pc += 4;
    }

    // n = 0
    // while n < 32
    //      if rs[n] = 1 then leave
    //      n = n + 1
    // ra = n
    OP(CNTLZW)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::cntlzw]++;
        }

        uint32_t n, m, rs = RRS;
        for (n = 0, m = 1 << 31; n < 32; n++, m >>= 1)
        {
            if (rs & m) break;
        }

        RRA = n;
        Gekko->regs.pc += 4;
    }

    // n = 0
    // while n < 32
    //      if rs[n] = 1 then leave
    //      n = n + 1
    // ra = n
    // CR0
    OP(CNTLZWD)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::cntlzw_d]++;
        }

        uint32_t n, m, rs = RRS;
        for (n = 0, m = 1 << 31; n < 32; n++, m >>= 1)
        {
            if (rs & m) break;
        }

        RRA = n;
        COMPUTE_CR0(n);
        Gekko->regs.pc += 4;
    }

}
