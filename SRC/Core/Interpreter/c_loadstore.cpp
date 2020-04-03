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
        if (RA) CPUReadByte(RRA + SIMM, &RRD);
        else CPUReadByte(SIMM, &RRD);
    }

    // ea = (ra | 0) + rb
    // rd = 0x000000 || MEM(ea, 1)
    OP(LBZX)
    {
        if (RA) CPUReadByte(RRA + RRB, &RRD);
        else CPUReadByte(RRB, &RRD);
    }

    // ea = ra + SIMM
    // rd = 0x000000 || MEM(ea, 1)
    // ra = ea
    OP(LBZU)
    {
        uint32_t ea = RRA + SIMM;
        CPUReadByte(ea, &RRD);
        RRA = ea;
    }

    // ea = ra + rb
    // rd = 0x000000 || MEM(ea, 1)
    // ra = ea
    OP(LBZUX)
    {
        uint32_t ea = RRA + RRB;
        CPUReadByte(ea, &RRD);
        RRA = ea;
    }

    // ea = (ra | 0) + SIMM
    // rd = 0x0000 || MEM(ea, 2)
    OP(LHZ)
    {
        if (RA) CPUReadHalf(RRA + SIMM, &RRD);
        else CPUReadHalf(SIMM, &RRD);
    }

    // ea = (ra | 0) + rb
    // rd = 0x0000 || MEM(ea, 2)
    OP(LHZX)
    {
        if (RA) CPUReadHalf(RRA + RRB, &RRD);
        else CPUReadHalf(RRB, &RRD);
    }

    // ea = ra + SIMM
    // rd = 0x0000 || MEM(ea, 2)
    // ra = ea
    OP(LHZU)
    {
        uint32_t ea = RRA + SIMM;
        CPUReadHalf(ea, &RRD);
        RRA = ea;
    }

    // ea = ra + rb
    // rd = 0x0000 || MEM(ea, 2)
    // ra = ea
    OP(LHZUX)
    {
        uint32_t ea = RRA + RRB;
        CPUReadHalf(ea, &RRD);
        RRA = ea;
    }

    // ea = (ra | 0) + SIMM
    // rd = (signed)MEM(ea, 2)
    OP(LHA)
    {
        if (RA) CPUReadHalfS(RRA + SIMM, &RRD);
        else CPUReadHalfS(SIMM, &RRD);
    }

    // ea = (ra | 0) + rb
    // rd = (signed)MEM(ea, 2)
    OP(LHAX)
    {
        if (RA) CPUReadHalfS(RRA + RRB, &RRD);
        else CPUReadHalfS(RRB, &RRD);
    }

    // ea = ra + SIMM
    // rd = (signed)MEM(ea, 2)
    // ra = ea
    OP(LHAU)
    {
        uint32_t ea = RRA + SIMM;
        CPUReadHalfS(ea, &RRD);
        RRA = ea;
    }

    // ea = ra + rb
    // rd = (signed)MEM(ea, 2)
    // ra = ea
    OP(LHAUX)
    {
        uint32_t ea = RRA + RRB;
        CPUReadHalfS(ea, &RRD);
        RRA = ea;
    }

    // ea = (ra | 0) + SIMM
    // rd = MEM(ea, 4)
    OP(LWZ)
    {
        if (RA) CPUReadWord(RRA + SIMM, &RRD);
        else CPUReadWord(SIMM, &RRD);
    }

    // ea = (ra | 0) + rb
    // rd = MEM(ea, 4)
    OP(LWZX)
    {
        if (RA) CPUReadWord(RRA + RRB, &RRD);
        else CPUReadWord(RRB, &RRD);
    }

    // ea = ra + SIMM
    // rd = MEM(ea, 4)
    // ra = ea
    OP(LWZU)
    {
        uint32_t ea = RRA + SIMM;
        CPUReadWord(ea, &RRD);
        RRA = ea;
    }

    // ea = ra + rb
    // rd = MEM(ea, 4)
    // ra = ea
    OP(LWZUX)
    {
        uint32_t ea = RRA + RRB;
        CPUReadWord(ea, &RRD);
        RRA = ea;
    }

    // ---------------------------------------------------------------------------
    // stores

    // ea = (ra | 0) + SIMM
    // MEM(ea, 1) = rs[24-31]
    OP(STB)
    {
        if (RA) CPUWriteByte(RRA + SIMM, RRS);
        else CPUWriteByte(SIMM, RRS);
    }

    // ea = (ra | 0) + rb
    // MEM(ea, 1) = rs[24-31]
    OP(STBX)
    {
        if (RA) CPUWriteByte(RRA + RRB, RRS);
        else CPUWriteByte(RRB, RRS);
    }

    // ea = ra + SIMM
    // MEM(ea, 1) = rs[24-31]
    // ra = ea
    OP(STBU)
    {
        uint32_t ea = RRA + SIMM;
        CPUWriteByte(ea, RRS);
        RRA = ea;
    }

    // ea = ra + rb
    // MEM(ea, 1) = rs[24-31]
    // ra = ea
    OP(STBUX)
    {
        uint32_t ea = RRA + RRB;
        CPUWriteByte(ea, RRS);
        RRA = ea;
    }

    // ea = (ra | 0) + SIMM
    // MEM(ea, 2) = rs[16-31]
    OP(STH)
    {
        if (RA) CPUWriteHalf(RRA + SIMM, RRS);
        else CPUWriteHalf(SIMM, RRS);
    }

    // ea = (ra | 0) + rb
    // MEM(ea, 2) = rs[16-31]
    OP(STHX)
    {
        if (RA) CPUWriteHalf(RRA + RRB, RRS);
        else CPUWriteHalf(RRB, RRS);
    }

    // ea = ra + SIMM
    // MEM(ea, 2) = rs[16-31]
    // ra = ea
    OP(STHU)
    {
        uint32_t ea = RRA + SIMM;
        CPUWriteHalf(ea, RRS);
        RRA = ea;
    }

    // ea = ra + rb
    // MEM(ea, 2) = rs[16-31]
    // ra = ea
    OP(STHUX)
    {
        uint32_t ea = RRA + RRB;
        CPUWriteHalf(ea, RRS);
        RRA = ea;
    }

    // ea = (ra | 0) + SIMM
    // MEM(ea, 4) = rs
    OP(STW)
    {
        if (RA) CPUWriteWord(RRA + SIMM, RRS);
        else CPUWriteWord(SIMM, RRS);
    }

    // ea = (ra | 0) + rb
    // MEM(ea, 4) = rs
    OP(STWX)
    {
        if (RA) CPUWriteWord(RRA + RRB, RRS);
        else CPUWriteWord(RRB, RRS);
    }

    // ea = ra + SIMM
    // MEM(ea, 4) = rs
    // ra = ea
    OP(STWU)
    {
        uint32_t ea = RRA + SIMM;
        CPUWriteWord(ea, RRS);
        RRA = ea;
    }

    // ea = ra + rb
    // MEM(ea, 4) = rs
    // ra = ea
    OP(STWUX)
    {
        uint32_t ea = RRA + RRB;
        CPUWriteWord(ea, RRS);
        RRA = ea;
    }

    // ---------------------------------------------------------------------------
    // special

    // ea = (ra | 0) + rb
    // rd = 0x0000 || MEM(ea+1, 1) || MEM(EA, 1)
    OP(LHBRX)
    {
        uint32_t val;
        if (RA) CPUReadHalf(RRA + RRB, &val);
        else CPUReadHalf(RRB, &val);
        RRD = _byteswap_ushort((uint16_t)val);
    }

    // ea = (ra | 0) + rb
    // rd = MEM(ea+3, 1) || MEM(ea+2, 1) || MEM(ea+1, 1) || MEM(ea, 1)
    OP(LWBRX)
    {
        uint32_t val;
        if (RA) CPUReadWord(RRA + RRB, &val);
        else CPUReadWord(RRB, &val);
        RRD = _byteswap_ulong(val);
    }

    // ea = (ra | 0) + rb
    // MEM(ea, 2) = rs[24-31] || rs[16-23]
    OP(STHBRX)
    {
        if (RA) CPUWriteHalf(RRA + RRB, _byteswap_ushort((uint16_t)RRS));
        else CPUWriteHalf(RRB, _byteswap_ushort((uint16_t)RRS));
    }

    // ea = (ra | 0) + rb
    // MEM(ea, 4) = rs[24-31] || rs[16-23] || rs[8-15] || rs[0-7]
    OP(STWBRX)
    {
        if (RA) CPUWriteWord(RRA + RRB, _byteswap_ulong(RRS));
        else CPUWriteWord(RRB, _byteswap_ulong(RRS));
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
            CPUReadWord(ea, &GPR[r]);
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
            CPUWriteWord(ea, GPR[r]);
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
                GPR[rd] = r;
                rd++;
                rd %= 32;
                r = 0;
            }
            CPUReadByte(ea, &val);
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
        GPR[rd] = r;
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
                r = GPR[rs];
                rs++;
                rs %= 32;
                i = 4;
            }
            CPUWriteByte(ea, r >> 24);
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
        cpu.RESERVE = true;
        cpu.RESERVE_ADDR = GCEffectiveToPhysical(ea, 0);
        CPUReadWord(ea, &RRD);
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

        PPC_CR &= 0x0fffffff;

        if (cpu.RESERVE)
        {
            CPUWriteWord(ea, RRS);
            SET_CR0_EQ;
            cpu.RESERVE = false;
        }

        if (IS_XER_SO) SET_CR0_SO;
    }

}
