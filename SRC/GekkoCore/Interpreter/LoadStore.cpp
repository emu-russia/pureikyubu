// Integer Load and Store Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

namespace Gekko
{

    // ---------------------------------------------------------------------------
    // loads

    // ea = (ra | 0) + SIMM
    // rd = 0x000000 || MEM(ea, 1)
    OP(LBZ)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::lbz]++;
        }

        if (RA) Gekko->ReadByte(RRA + SIMM, &RRD);
        else Gekko->ReadByte(SIMM, &RRD);
        if (Gekko->exception) return;
        Gekko->regs.pc += 4;
    }

    // ea = (ra | 0) + rb
    // rd = 0x000000 || MEM(ea, 1)
    OP(LBZX)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::lbzx]++;
        }

        if (RA) Gekko->ReadByte(RRA + RRB, &RRD);
        else Gekko->ReadByte(RRB, &RRD);
        if (Gekko->exception) return;
        Gekko->regs.pc += 4;
    }

    // ea = ra + SIMM
    // rd = 0x000000 || MEM(ea, 1)
    // ra = ea
    OP(LBZU)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::lbzu]++;
        }

        uint32_t ea = RRA + SIMM;
        Gekko->ReadByte(ea, &RRD);
        if (Gekko->exception) return;
        RRA = ea;
        Gekko->regs.pc += 4;
    }

    // ea = ra + rb
    // rd = 0x000000 || MEM(ea, 1)
    // ra = ea
    OP(LBZUX)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::lbzux]++;
        }

        uint32_t ea = RRA + RRB;
        Gekko->ReadByte(ea, &RRD);
        if (Gekko->exception) return;
        RRA = ea;
        Gekko->regs.pc += 4;
    }

    // ea = (ra | 0) + SIMM
    // rd = 0x0000 || MEM(ea, 2)
    OP(LHZ)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::lhz]++;
        }

        if (RA) Gekko->ReadHalf(RRA + SIMM, &RRD);
        else Gekko->ReadHalf(SIMM, &RRD);
        if (Gekko->exception) return;
        Gekko->regs.pc += 4;
    }

    // ea = (ra | 0) + rb
    // rd = 0x0000 || MEM(ea, 2)
    OP(LHZX)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::lhzx]++;
        }

        if (RA) Gekko->ReadHalf(RRA + RRB, &RRD);
        else Gekko->ReadHalf(RRB, &RRD);
        if (Gekko->exception) return;
        Gekko->regs.pc += 4;
    }

    // ea = ra + SIMM
    // rd = 0x0000 || MEM(ea, 2)
    // ra = ea
    OP(LHZU)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::lhzu]++;
        }

        uint32_t ea = RRA + SIMM;
        Gekko->ReadHalf(ea, &RRD);
        if (Gekko->exception) return;
        RRA = ea;
        Gekko->regs.pc += 4;
    }

    // ea = ra + rb
    // rd = 0x0000 || MEM(ea, 2)
    // ra = ea
    OP(LHZUX)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::lhzux]++;
        }

        uint32_t ea = RRA + RRB;
        Gekko->ReadHalf(ea, &RRD);
        if (Gekko->exception) return;
        RRA = ea;
        Gekko->regs.pc += 4;
    }

    // ea = (ra | 0) + SIMM
    // rd = (signed)MEM(ea, 2)
    OP(LHA)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::lha]++;
        }

        if (RA) Gekko->ReadHalfS(RRA + SIMM, &RRD);
        else Gekko->ReadHalfS(SIMM, &RRD);
        if (Gekko->exception) return;
        Gekko->regs.pc += 4;
    }

    // ea = (ra | 0) + rb
    // rd = (signed)MEM(ea, 2)
    OP(LHAX)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::lhax]++;
        }

        if (RA) Gekko->ReadHalfS(RRA + RRB, &RRD);
        else Gekko->ReadHalfS(RRB, &RRD);
        if (Gekko->exception) return;
        Gekko->regs.pc += 4;
    }

    // ea = ra + SIMM
    // rd = (signed)MEM(ea, 2)
    // ra = ea
    OP(LHAU)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::lhau]++;
        }

        uint32_t ea = RRA + SIMM;
        Gekko->ReadHalfS(ea, &RRD);
        if (Gekko->exception) return;
        RRA = ea;
        Gekko->regs.pc += 4;
    }

    // ea = ra + rb
    // rd = (signed)MEM(ea, 2)
    // ra = ea
    OP(LHAUX)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::lhaux]++;
        }

        uint32_t ea = RRA + RRB;
        Gekko->ReadHalfS(ea, &RRD);
        if (Gekko->exception) return;
        RRA = ea;
        Gekko->regs.pc += 4;
    }

    // ea = (ra | 0) + SIMM
    // rd = MEM(ea, 4)
    OP(LWZ)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::lwz]++;
        }

        if (RA) Gekko->ReadWord(RRA + SIMM, &RRD);
        else Gekko->ReadWord(SIMM, &RRD);
        if (Gekko->exception) return;
        Gekko->regs.pc += 4;
    }

    // ea = (ra | 0) + rb
    // rd = MEM(ea, 4)
    OP(LWZX)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::lwzx]++;
        }

        if (RA) Gekko->ReadWord(RRA + RRB, &RRD);
        else Gekko->ReadWord(RRB, &RRD);
        if (Gekko->exception) return;
        Gekko->regs.pc += 4;
    }

    // ea = ra + SIMM
    // rd = MEM(ea, 4)
    // ra = ea
    OP(LWZU)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::lwzu]++;
        }

        uint32_t ea = RRA + SIMM;
        Gekko->ReadWord(ea, &RRD);
        if (Gekko->exception) return;
        RRA = ea;
        Gekko->regs.pc += 4;
    }

    // ea = ra + rb
    // rd = MEM(ea, 4)
    // ra = ea
    OP(LWZUX)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::lwzux]++;
        }

        uint32_t ea = RRA + RRB;
        Gekko->ReadWord(ea, &RRD);
        if (Gekko->exception) return;
        RRA = ea;
        Gekko->regs.pc += 4;
    }

    // ---------------------------------------------------------------------------
    // stores

    // ea = (ra | 0) + SIMM
    // MEM(ea, 1) = rs[24-31]
    OP(STB)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::stb]++;
        }

        if (RA) Gekko->WriteByte(RRA + SIMM, RRS);
        else Gekko->WriteByte(SIMM, RRS);
        if (Gekko->exception) return;
        Gekko->regs.pc += 4;
    }

    // ea = (ra | 0) + rb
    // MEM(ea, 1) = rs[24-31]
    OP(STBX)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::stbx]++;
        }

        if (RA) Gekko->WriteByte(RRA + RRB, RRS);
        else Gekko->WriteByte(RRB, RRS);
        if (Gekko->exception) return;
        Gekko->regs.pc += 4;
    }

    // ea = ra + SIMM
    // MEM(ea, 1) = rs[24-31]
    // ra = ea
    OP(STBU)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::stbu]++;
        }

        uint32_t ea = RRA + SIMM;
        Gekko->WriteByte(ea, RRS);
        if (Gekko->exception) return;
        RRA = ea;
        Gekko->regs.pc += 4;
    }

    // ea = ra + rb
    // MEM(ea, 1) = rs[24-31]
    // ra = ea
    OP(STBUX)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::stbux]++;
        }

        uint32_t ea = RRA + RRB;
        Gekko->WriteByte(ea, RRS);
        if (Gekko->exception) return;
        RRA = ea;
        Gekko->regs.pc += 4;
    }

    // ea = (ra | 0) + SIMM
    // MEM(ea, 2) = rs[16-31]
    OP(STH)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::sth]++;
        }

        if (RA) Gekko->WriteHalf(RRA + SIMM, RRS);
        else Gekko->WriteHalf(SIMM, RRS);
        if (Gekko->exception) return;
        Gekko->regs.pc += 4;
    }

    // ea = (ra | 0) + rb
    // MEM(ea, 2) = rs[16-31]
    OP(STHX)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::sthx]++;
        }

        if (RA) Gekko->WriteHalf(RRA + RRB, RRS);
        else Gekko->WriteHalf(RRB, RRS);
        if (Gekko->exception) return;
        Gekko->regs.pc += 4;
    }

    // ea = ra + SIMM
    // MEM(ea, 2) = rs[16-31]
    // ra = ea
    OP(STHU)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::sthu]++;
        }

        uint32_t ea = RRA + SIMM;
        Gekko->WriteHalf(ea, RRS);
        if (Gekko->exception) return;
        RRA = ea;
        Gekko->regs.pc += 4;
    }

    // ea = ra + rb
    // MEM(ea, 2) = rs[16-31]
    // ra = ea
    OP(STHUX)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::sthux]++;
        }

        uint32_t ea = RRA + RRB;
        Gekko->WriteHalf(ea, RRS);
        if (Gekko->exception) return;
        RRA = ea;
        Gekko->regs.pc += 4;
    }

    // ea = (ra | 0) + SIMM
    // MEM(ea, 4) = rs
    OP(STW)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::stw]++;
        }

        if (RA) Gekko->WriteWord(RRA + SIMM, RRS);
        else Gekko->WriteWord(SIMM, RRS);
        if (Gekko->exception) return;
        Gekko->regs.pc += 4;
    }

    // ea = (ra | 0) + rb
    // MEM(ea, 4) = rs
    OP(STWX)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::stwx]++;
        }

        if (RA) Gekko->WriteWord(RRA + RRB, RRS);
        else Gekko->WriteWord(RRB, RRS);
        if (Gekko->exception) return;
        Gekko->regs.pc += 4;
    }

    // ea = ra + SIMM
    // MEM(ea, 4) = rs
    // ra = ea
    OP(STWU)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::stwu]++;
        }

        uint32_t ea = RRA + SIMM;
        Gekko->WriteWord(ea, RRS);
        if (Gekko->exception) return;
        RRA = ea;
        Gekko->regs.pc += 4;
    }

    // ea = ra + rb
    // MEM(ea, 4) = rs
    // ra = ea
    OP(STWUX)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::stwux]++;
        }

        uint32_t ea = RRA + RRB;
        Gekko->WriteWord(ea, RRS);
        if (Gekko->exception) return;
        RRA = ea;
        Gekko->regs.pc += 4;
    }

    // ---------------------------------------------------------------------------
    // special

    // ea = (ra | 0) + rb
    // rd = 0x0000 || MEM(ea+1, 1) || MEM(EA, 1)
    OP(LHBRX)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::lhbrx]++;
        }

        uint32_t val;
        if (RA) Gekko->ReadHalf(RRA + RRB, &val);
        else Gekko->ReadHalf(RRB, &val);
        if (Gekko->exception) return;
        RRD = _BYTESWAP_UINT16((uint16_t)val);
        Gekko->regs.pc += 4;
    }

    // ea = (ra | 0) + rb
    // rd = MEM(ea+3, 1) || MEM(ea+2, 1) || MEM(ea+1, 1) || MEM(ea, 1)
    OP(LWBRX)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::lwbrx]++;
        }

        uint32_t val;
        if (RA) Gekko->ReadWord(RRA + RRB, &val);
        else Gekko->ReadWord(RRB, &val);
        if (Gekko->exception) return;
        RRD = _BYTESWAP_UINT32(val);
        Gekko->regs.pc += 4;
    }

    // ea = (ra | 0) + rb
    // MEM(ea, 2) = rs[24-31] || rs[16-23]
    OP(STHBRX)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::sthbrx]++;
        }

        if (RA) Gekko->WriteHalf(RRA + RRB, _BYTESWAP_UINT16((uint16_t)RRS));
        else Gekko->WriteHalf(RRB, _BYTESWAP_UINT16((uint16_t)RRS));
        if (Gekko->exception) return;
        Gekko->regs.pc += 4;
    }

    // ea = (ra | 0) + rb
    // MEM(ea, 4) = rs[24-31] || rs[16-23] || rs[8-15] || rs[0-7]
    OP(STWBRX)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::stwbrx]++;
        }

        if (RA) Gekko->WriteWord(RRA + RRB, _BYTESWAP_UINT32(RRS));
        else Gekko->WriteWord(RRB, _BYTESWAP_UINT32(RRS));
        if (Gekko->exception) return;
        Gekko->regs.pc += 4;
    }

    // ea = (ra | 0) + SIMM
    // r = rd
    // while r <= 31
    //      GPR(r) = MEM(ea, 4)
    //      r = r + 1
    //      ea = ea + 4
    OP(LMW)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::lmw]++;
        }

        uint32_t ea;
        if (RA) ea = RRA + SIMM;
        else ea = SIMM;

        for (int r = RD; r < 32; r++, ea += 4)
        {
            Gekko->ReadWord(ea, &Gekko->regs.gpr[r]);
            if (Gekko->exception) return;
        }
        Gekko->regs.pc += 4;
    }

    // ea = (ra | 0) + SIMM
    // r = rs
    // while r <= 31
    //      MEM(ea, 4) = GPR(r)
    //      r = r + 1
    //      ea = ea + 4
    OP(STMW)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::stmw]++;
        }

        uint32_t ea;
        if (RA) ea = RRA + SIMM;
        else ea = SIMM;

        for (int r = RS; r < 32; r++, ea += 4)
        {
            Gekko->WriteWord(ea, Gekko->regs.gpr[r]);
            if (Gekko->exception) return;
        }
        Gekko->regs.pc += 4;
    }

    // ea = (ra | 0)
    // n = NB ? NB : 32
    // r = rd - 1
    // i = 0
    // while n > 0
    //      if i = 0 then
    //          r = (r + 1) % 32
    //          GPR(r) = 0
    //      GPR(r)[i...i+7] = MEM(ea, 1)
    //      i = i + 8
    //      if i = 32 then i = 0
    //      ea = ea + 1
    //      n = n -1
    OP(LSWI)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::lswi]++;
        }

        int32_t rd = RD, n = (RB) ? (RB) : 32, i = 4;
        uint32_t ea = (RA) ? (RRA) : 0;
        uint32_t r = 0, val;

        while (n > 0)
        {
            if (i == 0)
            {
                i = 4;
                Gekko->regs.gpr[rd] = r;
                rd++;
                rd %= 32;
                r = 0;
            }
            Gekko->ReadByte(ea, &val);
            if (Gekko->exception) return;
            r <<= 8;
            r |= (uint8_t)val;
            ea++;
            i--;
            n--;
        }

        while (i)
        {
            r <<= 8;
            i--;
        }
        Gekko->regs.gpr[rd] = r;
        Gekko->regs.pc += 4;
    }

    // ea = (ra | 0)
    // n = NB ? NB : 32
    // r = rs - 1
    // i = 0
    // while n > 0
    //      if i = 0 then r = (r + 1) % 32
    //      MEM(ea, 1) = GPR(r)[i...i+7]
    //      i = i + 8
    //      if i = 32 then i = 0;
    //      ea = ea + 1
    //      n = n -1
    OP(STSWI)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::stswi]++;
        }

        int32_t rs = RS, n = (RB) ? (RB) : 32, i = 0;
        uint32_t ea = (RA) ? (RRA) : 0;
        uint32_t r = 0;

        while (n > 0)
        {
            if (i == 0)
            {
                r = Gekko->regs.gpr[rs];
                rs++;
                rs %= 32;
                i = 4;
            }
            Gekko->WriteByte(ea, r >> 24);
            if (Gekko->exception) return;
            r <<= 8;
            ea++;
            i--;
            n--;
        }
        Gekko->regs.pc += 4;
    }

    // ea = (ra | 0) + rb
    // RESERVE = 1
    // RESERVE_ADDR = physical(ea)
    // rd = MEM(ea, 4)
    OP(LWARX)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::lwarx]++;
        }

        int WIMG;
        uint32_t ea = RRB;
        if (RA) ea += RRA;
        Gekko->interp->RESERVE = true;
        Gekko->interp->RESERVE_ADDR = Gekko->EffectiveToPhysical(ea, Gekko::MmuAccess::Read, WIMG);
        Gekko->ReadWord(ea, &RRD);
        if (Gekko->exception) return;
        Gekko->regs.pc += 4;
    }

    // ea = (ra | 0) + rb
    // if RESERVE
    //      then
    //          MEM(ea, 4) = rs
    //          CR0 = 0b00 || 0b1 || XER[SO]
    //          RESERVE = 0
    //      else
    //          CR0 = 0b00 || 0b0 || XER[SO]
    OP(STWCXD)
    {
        if (Gekko->opcodeStatsEnabled)
        {
            Gekko->opcodeStats[(size_t)Gekko::Instruction::stwcx_d]++;
        }

        uint32_t ea = RRB;
        if (RA) ea += RRA;

        Gekko->regs.cr &= 0x0fffffff;

        if (Gekko->interp->RESERVE)
        {
            Gekko->WriteWord(ea, RRS);
            if (Gekko->exception) return;
            SET_CR0_EQ;
            Gekko->interp->RESERVE = false;
        }

        if (IS_XER_SO) SET_CR0_SO;
        Gekko->regs.pc += 4;
    }

}
