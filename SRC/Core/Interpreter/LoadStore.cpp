// Integer Load and Store Instructions
#include "../pch.h"
#include "interpreter.h"

namespace Gekko
{

    // ---------------------------------------------------------------------------
    // loads

    // ea = (ra | 0) + SIMM
    // rd = 0x000000 || MEM(ea, 1)
    OP(LBZ)
    {
        if (RA) Gekko->ReadByte(RRA + SIMM, &RRD);
        else Gekko->ReadByte(SIMM, &RRD);
    }

    // ea = (ra | 0) + rb
    // rd = 0x000000 || MEM(ea, 1)
    OP(LBZX)
    {
        if (RA) Gekko->ReadByte(RRA + RRB, &RRD);
        else Gekko->ReadByte(RRB, &RRD);
    }

    // ea = ra + SIMM
    // rd = 0x000000 || MEM(ea, 1)
    // ra = ea
    OP(LBZU)
    {
        uint32_t ea = RRA + SIMM;
        Gekko->ReadByte(ea, &RRD);
        if (Gekko->interp->exception) return;
        RRA = ea;
    }

    // ea = ra + rb
    // rd = 0x000000 || MEM(ea, 1)
    // ra = ea
    OP(LBZUX)
    {
        uint32_t ea = RRA + RRB;
        Gekko->ReadByte(ea, &RRD);
        if (Gekko->interp->exception) return;
        RRA = ea;
    }

    // ea = (ra | 0) + SIMM
    // rd = 0x0000 || MEM(ea, 2)
    OP(LHZ)
    {
        if (RA) Gekko->ReadHalf(RRA + SIMM, &RRD);
        else Gekko->ReadHalf(SIMM, &RRD);
    }

    // ea = (ra | 0) + rb
    // rd = 0x0000 || MEM(ea, 2)
    OP(LHZX)
    {
        if (RA) Gekko->ReadHalf(RRA + RRB, &RRD);
        else Gekko->ReadHalf(RRB, &RRD);
    }

    // ea = ra + SIMM
    // rd = 0x0000 || MEM(ea, 2)
    // ra = ea
    OP(LHZU)
    {
        uint32_t ea = RRA + SIMM;
        Gekko->ReadHalf(ea, &RRD);
        if (Gekko->interp->exception) return;
        RRA = ea;
    }

    // ea = ra + rb
    // rd = 0x0000 || MEM(ea, 2)
    // ra = ea
    OP(LHZUX)
    {
        uint32_t ea = RRA + RRB;
        Gekko->ReadHalf(ea, &RRD);
        if (Gekko->interp->exception) return;
        RRA = ea;
    }

    // ea = (ra | 0) + SIMM
    // rd = (signed)MEM(ea, 2)
    OP(LHA)
    {
        if (RA) Gekko->ReadHalfS(RRA + SIMM, &RRD);
        else Gekko->ReadHalfS(SIMM, &RRD);
    }

    // ea = (ra | 0) + rb
    // rd = (signed)MEM(ea, 2)
    OP(LHAX)
    {
        if (RA) Gekko->ReadHalfS(RRA + RRB, &RRD);
        else Gekko->ReadHalfS(RRB, &RRD);
    }

    // ea = ra + SIMM
    // rd = (signed)MEM(ea, 2)
    // ra = ea
    OP(LHAU)
    {
        uint32_t ea = RRA + SIMM;
        Gekko->ReadHalfS(ea, &RRD);
        if (Gekko->interp->exception) return;
        RRA = ea;
    }

    // ea = ra + rb
    // rd = (signed)MEM(ea, 2)
    // ra = ea
    OP(LHAUX)
    {
        uint32_t ea = RRA + RRB;
        Gekko->ReadHalfS(ea, &RRD);
        if (Gekko->interp->exception) return;
        RRA = ea;
    }

    // ea = (ra | 0) + SIMM
    // rd = MEM(ea, 4)
    OP(LWZ)
    {
        if (RA) Gekko->ReadWord(RRA + SIMM, &RRD);
        else Gekko->ReadWord(SIMM, &RRD);
    }

    // ea = (ra | 0) + rb
    // rd = MEM(ea, 4)
    OP(LWZX)
    {
        if (RA) Gekko->ReadWord(RRA + RRB, &RRD);
        else Gekko->ReadWord(RRB, &RRD);
    }

    // ea = ra + SIMM
    // rd = MEM(ea, 4)
    // ra = ea
    OP(LWZU)
    {
        uint32_t ea = RRA + SIMM;
        Gekko->ReadWord(ea, &RRD);
        if (Gekko->interp->exception) return;
        RRA = ea;
    }

    // ea = ra + rb
    // rd = MEM(ea, 4)
    // ra = ea
    OP(LWZUX)
    {
        uint32_t ea = RRA + RRB;
        Gekko->ReadWord(ea, &RRD);
        if (Gekko->interp->exception) return;
        RRA = ea;
    }

    // ---------------------------------------------------------------------------
    // stores

    // ea = (ra | 0) + SIMM
    // MEM(ea, 1) = rs[24-31]
    OP(STB)
    {
        if (RA) Gekko->WriteByte(RRA + SIMM, RRS);
        else Gekko->WriteByte(SIMM, RRS);
    }

    // ea = (ra | 0) + rb
    // MEM(ea, 1) = rs[24-31]
    OP(STBX)
    {
        if (RA) Gekko->WriteByte(RRA + RRB, RRS);
        else Gekko->WriteByte(RRB, RRS);
    }

    // ea = ra + SIMM
    // MEM(ea, 1) = rs[24-31]
    // ra = ea
    OP(STBU)
    {
        uint32_t ea = RRA + SIMM;
        Gekko->WriteByte(ea, RRS);
        if (Gekko->interp->exception) return;
        RRA = ea;
    }

    // ea = ra + rb
    // MEM(ea, 1) = rs[24-31]
    // ra = ea
    OP(STBUX)
    {
        uint32_t ea = RRA + RRB;
        Gekko->WriteByte(ea, RRS);
        if (Gekko->interp->exception) return;
        RRA = ea;
    }

    // ea = (ra | 0) + SIMM
    // MEM(ea, 2) = rs[16-31]
    OP(STH)
    {
        if (RA) Gekko->WriteHalf(RRA + SIMM, RRS);
        else Gekko->WriteHalf(SIMM, RRS);
    }

    // ea = (ra | 0) + rb
    // MEM(ea, 2) = rs[16-31]
    OP(STHX)
    {
        if (RA) Gekko->WriteHalf(RRA + RRB, RRS);
        else Gekko->WriteHalf(RRB, RRS);
    }

    // ea = ra + SIMM
    // MEM(ea, 2) = rs[16-31]
    // ra = ea
    OP(STHU)
    {
        uint32_t ea = RRA + SIMM;
        Gekko->WriteHalf(ea, RRS);
        if (Gekko->interp->exception) return;
        RRA = ea;
    }

    // ea = ra + rb
    // MEM(ea, 2) = rs[16-31]
    // ra = ea
    OP(STHUX)
    {
        uint32_t ea = RRA + RRB;
        Gekko->WriteHalf(ea, RRS);
        if (Gekko->interp->exception) return;
        RRA = ea;
    }

    // ea = (ra | 0) + SIMM
    // MEM(ea, 4) = rs
    OP(STW)
    {
        if (RA) Gekko->WriteWord(RRA + SIMM, RRS);
        else Gekko->WriteWord(SIMM, RRS);
    }

    // ea = (ra | 0) + rb
    // MEM(ea, 4) = rs
    OP(STWX)
    {
        if (RA) Gekko->WriteWord(RRA + RRB, RRS);
        else Gekko->WriteWord(RRB, RRS);
    }

    // ea = ra + SIMM
    // MEM(ea, 4) = rs
    // ra = ea
    OP(STWU)
    {
        uint32_t ea = RRA + SIMM;
        Gekko->WriteWord(ea, RRS);
        if (Gekko->interp->exception) return;
        RRA = ea;
    }

    // ea = ra + rb
    // MEM(ea, 4) = rs
    // ra = ea
    OP(STWUX)
    {
        uint32_t ea = RRA + RRB;
        Gekko->WriteWord(ea, RRS);
        if (Gekko->interp->exception) return;
        RRA = ea;
    }

    // ---------------------------------------------------------------------------
    // special

    // ea = (ra | 0) + rb
    // rd = 0x0000 || MEM(ea+1, 1) || MEM(EA, 1)
    OP(LHBRX)
    {
        uint32_t val;
        if (RA) Gekko->ReadHalf(RRA + RRB, &val);
        else Gekko->ReadHalf(RRB, &val);
        if (Gekko->interp->exception) return;
        RRD = _byteswap_ushort((uint16_t)val);
    }

    // ea = (ra | 0) + rb
    // rd = MEM(ea+3, 1) || MEM(ea+2, 1) || MEM(ea+1, 1) || MEM(ea, 1)
    OP(LWBRX)
    {
        uint32_t val;
        if (RA) Gekko->ReadWord(RRA + RRB, &val);
        else Gekko->ReadWord(RRB, &val);
        if (Gekko->interp->exception) return;
        RRD = _byteswap_ulong(val);
    }

    // ea = (ra | 0) + rb
    // MEM(ea, 2) = rs[24-31] || rs[16-23]
    OP(STHBRX)
    {
        if (RA) Gekko->WriteHalf(RRA + RRB, _byteswap_ushort((uint16_t)RRS));
        else Gekko->WriteHalf(RRB, _byteswap_ushort((uint16_t)RRS));
    }

    // ea = (ra | 0) + rb
    // MEM(ea, 4) = rs[24-31] || rs[16-23] || rs[8-15] || rs[0-7]
    OP(STWBRX)
    {
        if (RA) Gekko->WriteWord(RRA + RRB, _byteswap_ulong(RRS));
        else Gekko->WriteWord(RRB, _byteswap_ulong(RRS));
    }

    // ea = (ra | 0) + SIMM
    // r = rd
    // while r <= 31
    //      GPR(r) = MEM(ea, 4)
    //      r = r + 1
    //      ea = ea + 4
    OP(LMW)
    {
        uint32_t ea;
        if (RA) ea = RRA + SIMM;
        else ea = SIMM;

        for (int r = RD; r < 32; r++, ea += 4)
        {
            Gekko->ReadWord(ea, &Gekko->regs.gpr[r]);
            if (Gekko->interp->exception) break;
        }
    }

    // ea = (ra | 0) + SIMM
    // r = rs
    // while r <= 31
    //      MEM(ea, 4) = GPR(r)
    //      r = r + 1
    //      ea = ea + 4
    OP(STMW)
    {
        uint32_t ea;
        if (RA) ea = RRA + SIMM;
        else ea = SIMM;

        for (int r = RS; r < 32; r++, ea += 4)
        {
            Gekko->WriteWord(ea, Gekko->regs.gpr[r]);
            if (Gekko->interp->exception) break;
        }
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
            if (Gekko->interp->exception) return;
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
            if (Gekko->interp->exception) return;
            r <<= 8;
            ea++;
            i--;
            n--;
        }
    }

    // ea = (ra | 0) + rb
    // RESERVE = 1
    // RESERVE_ADDR = physical(ea)
    // rd = MEM(ea, 4)
    OP(LWARX)
    {
        uint32_t ea = RRB;
        if (RA) ea += RRA;
        Gekko->interp->RESERVE = true;
        Gekko->interp->RESERVE_ADDR = Gekko->EffectiveToPhysical(ea, Gekko::MmuAccess::Read);
        Gekko->ReadWord(ea, &RRD);
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
        uint32_t ea = RRB;
        if (RA) ea += RRA;

        Gekko->regs.cr &= 0x0fffffff;

        if (Gekko->interp->RESERVE)
        {
            Gekko->WriteWord(ea, RRS);
            if (Gekko->interp->exception) return;
            SET_CR0_EQ;
            Gekko->interp->RESERVE = false;
        }

        if (IS_XER_SO) SET_CR0_SO;
    }

}
