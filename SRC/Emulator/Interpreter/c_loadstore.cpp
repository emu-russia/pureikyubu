// Integer Load and Store Instructions
#include "dolphin.h"

#define OP(name) void __fastcall c_##name##(u32 op)

#define SET_CR0_LT      (CR |=  (1 << 31))
#define SET_CR0_GT      (CR |=  (1 << 30))
#define SET_CR0_EQ      (CR |=  (1 << 29))
#define SET_CR0_SO      (CR |=  (1 << 28))
#define RESET_CR0_LT    (CR &= ~(1 << 31))
#define RESET_CR0_GT    (CR &= ~(1 << 30))
#define RESET_CR0_EQ    (CR &= ~(1 << 29))
#define RESET_CR0_SO    (CR &= ~(1 << 28))

#define IS_XER_SO       (XER & (1 << 31))

// ---------------------------------------------------------------------------
// loads

// ea = (ra | 0) + SIMM
// rd = 0x000000 || MEM(ea, 1)
OP(LBZ)
{
    if(RA) CPUReadByte(RRA + SIMM, &RRD);
    else CPUReadByte(SIMM, &RRD);
}

// ea = (ra | 0) + rb
// rd = 0x000000 || MEM(ea, 1)
OP(LBZX)
{
    if(RA) CPUReadByte(RRA + RRB, &RRD);
    else CPUReadByte(RRB, &RRD);
}

// ea = ra + SIMM
// rd = 0x000000 || MEM(ea, 1)
// ra = ea
OP(LBZU)
{
    u32 ea = RRA + SIMM;
    CPUReadByte(ea, &RRD);
    RRA = ea;
}

// ea = ra + rb
// rd = 0x000000 || MEM(ea, 1)
// ra = ea
OP(LBZUX)
{
    u32 ea = RRA + RRB;
    CPUReadByte(ea, &RRD);
    RRA = ea;
}

// ea = (ra | 0) + SIMM
// rd = 0x0000 || MEM(ea, 2)
OP(LHZ)
{
    if(RA) CPUReadHalf(RRA + SIMM, &RRD);
    else CPUReadHalf(SIMM, &RRD);
}

// ea = (ra | 0) + rb
// rd = 0x0000 || MEM(ea, 2)
OP(LHZX)
{
    if(RA) CPUReadHalf(RRA + RRB, &RRD);
    else CPUReadHalf(RRB, &RRD);
}

// ea = ra + SIMM
// rd = 0x0000 || MEM(ea, 2)
// ra = ea
OP(LHZU)
{
    u32 ea = RRA + SIMM;
    CPUReadHalf(ea, &RRD);
    RRA = ea;
}

// ea = ra + rb
// rd = 0x0000 || MEM(ea, 2)
// ra = ea
OP(LHZUX)
{
    u32 ea = RRA + RRB;
    CPUReadHalf(ea, &RRD);
    RRA = ea;
}

// ea = (ra | 0) + SIMM
// rd = (signed)MEM(ea, 2)
OP(LHA)
{
    if(RA) CPUReadHalfS(RRA + SIMM, &RRD);
    else CPUReadHalfS(SIMM, &RRD);
}

// ea = (ra | 0) + rb
// rd = (signed)MEM(ea, 2)
OP(LHAX)
{
    if(RA) CPUReadHalfS(RRA + RRB, &RRD);
    else CPUReadHalfS(RRB, &RRD);
}

// ea = ra + SIMM
// rd = (signed)MEM(ea, 2)
// ra = ea
OP(LHAU)
{
    u32 ea = RRA + SIMM;
    CPUReadHalfS(ea, &RRD);
    RRA = ea;
}

// ea = ra + rb
// rd = (signed)MEM(ea, 2)
// ra = ea
OP(LHAUX)
{
    u32 ea = RRA + RRB;
    CPUReadHalfS(ea, &RRD);
    RRA = ea;
}

// ea = (ra | 0) + SIMM
// rd = MEM(ea, 4)
OP(LWZ)
{
    if(RA) CPUReadWord(RRA + SIMM, &RRD);
    else CPUReadWord(SIMM, &RRD);
}

// ea = (ra | 0) + rb
// rd = MEM(ea, 4)
OP(LWZX)
{
    if(RA) CPUReadWord(RRA + RRB, &RRD);
    else CPUReadWord(RRB, &RRD);
}

// ea = ra + SIMM
// rd = MEM(ea, 4)
// ra = ea
OP(LWZU)
{
    u32 ea = RRA + SIMM;
    CPUReadWord(ea, &RRD);
    RRA = ea;
}

// ea = ra + rb
// rd = MEM(ea, 4)
// ra = ea
OP(LWZUX)
{
    u32 ea = RRA + RRB;
    CPUReadWord(ea, &RRD);
    RRA = ea;
}

// ---------------------------------------------------------------------------
// stores

// ea = (ra | 0) + SIMM
// MEM(ea, 1) = rs[24-31]
OP(STB)
{
    if(RA) CPUWriteByte(RRA + SIMM, RRS);
    else CPUWriteByte(SIMM, RRS);
}

// ea = (ra | 0) + rb
// MEM(ea, 1) = rs[24-31]
OP(STBX)
{
    if(RA) CPUWriteByte(RRA + RRB, RRS);
    else CPUWriteByte(RRB, RRS);
}

// ea = ra + SIMM
// MEM(ea, 1) = rs[24-31]
// ra = ea
OP(STBU)
{
    u32 ea = RRA + SIMM;
    CPUWriteByte(ea, RRS);
    RRA = ea;
}

// ea = ra + rb
// MEM(ea, 1) = rs[24-31]
// ra = ea
OP(STBUX)
{
    u32 ea = RRA + RRB;
    CPUWriteByte(ea, RRS);
    RRA = ea;
}

// ea = (ra | 0) + SIMM
// MEM(ea, 2) = rs[16-31]
OP(STH)
{
    if(RA) CPUWriteHalf(RRA + SIMM, RRS);
    else CPUWriteHalf(SIMM, RRS);
}

// ea = (ra | 0) + rb
// MEM(ea, 2) = rs[16-31]
OP(STHX)
{
    if(RA) CPUWriteHalf(RRA + RRB, RRS);
    else CPUWriteHalf(RRB, RRS);
}

// ea = ra + SIMM
// MEM(ea, 2) = rs[16-31]
// ra = ea
OP(STHU)
{
    u32 ea = RRA + SIMM;
    CPUWriteHalf(ea, RRS);
    RRA = ea;
}

// ea = ra + rb
// MEM(ea, 2) = rs[16-31]
// ra = ea
OP(STHUX)
{
    u32 ea = RRA + RRB;
    CPUWriteHalf(ea, RRS);
    RRA = ea;
}

// ea = (ra | 0) + SIMM
// MEM(ea, 4) = rs
OP(STW)
{
    if(RA) CPUWriteWord(RRA + SIMM, RRS);
    else CPUWriteWord(SIMM, RRS);
}

// ea = (ra | 0) + rb
// MEM(ea, 4) = rs
OP(STWX)
{
    if(RA) CPUWriteWord(RRA + RRB, RRS);
    else CPUWriteWord(RRB, RRS);
}

// ea = ra + SIMM
// MEM(ea, 4) = rs
// ra = ea
OP(STWU)
{
    u32 ea = RRA + SIMM;
    CPUWriteWord(ea, RRS);
    RRA = ea;
}

// ea = ra + rb
// MEM(ea, 4) = rs
// ra = ea
OP(STWUX)
{
    u32 ea = RRA + RRB;
    CPUWriteWord(ea, RRS);
    RRA = ea;
}

// ---------------------------------------------------------------------------
// special

// ea = (ra | 0) + rb
// rd = 0x0000 || MEM(ea+1, 1) || MEM(EA, 1)
OP(LHBRX)
{
    u32 val;
    if(RA) CPUReadHalf(RRA + RRB, &val);
    else CPUReadHalf(RRB, &val);
    RRD = MEMSwapHalf((u16)val);
}

// ea = (ra | 0) + rb
// rd = MEM(ea+3, 1) || MEM(ea+2, 1) || MEM(ea+1, 1) || MEM(ea, 1)
OP(LWBRX)
{
    u32 val;
    if(RA) CPUReadWord(RRA + RRB, &val);
    else CPUReadWord(RRB, &val);
    RRD = MEMSwap(val);
}

// ea = (ra | 0) + rb
// MEM(ea, 2) = rs[24-31] || rs[16-23]
OP(STHBRX)
{
    if(RA) CPUWriteHalf(RRA + RRB, MEMSwapHalf((u16)RRS));
    else CPUWriteHalf(RRB, MEMSwapHalf((u16)RRS));
}

// ea = (ra | 0) + rb
// MEM(ea, 4) = rs[24-31] || rs[16-23] || rs[8-15] || rs[0-7]
OP(STWBRX)
{
    if(RA) CPUWriteWord(RRA + RRB, MEMSwap(RRS));
    else CPUWriteWord(RRB, MEMSwap(RRS));
}

// ea = (ra | 0) + SIMM
// r = rd
// while r <= 31
//      GPR(r) = MEM(ea, 4)
//      r = r + 1
//      ea = ea + 4
OP(LMW)
{
    u32 ea;
    if(RA) ea = RRA + SIMM;
    else ea = SIMM;

    for(s32 r=RD; r<32; r++, ea+=4)
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
    u32 ea;
    if(RA) ea = RRA + SIMM;
    else ea = SIMM;

    for(s32 r=RS; r<32; r++, ea+=4)
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
    s32 rd = RD, n = (RB) ? (RB) : 32, i = 4;
    u32 ea = (RA) ? (RRA) : 0;
    u32 r = 0, val;

    while(n > 0)
    {
        if(i == 0)
        {
            i = 4;
            GPR[rd] = r;
            rd++;
            rd %= 32;
            r = 0;
        }
        CPUReadByte(ea, &val);
        r <<= 8;
        r |= (u8)val;
        ea++;
        i--;
        n--;
    }

    while(i)
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
    s32 rs = RS, n = (RB) ? (RB) : 32, i = 0;
    u32 ea = (RA) ? (RRA) : 0;
    u32 r = 0;
    
    while(n > 0)
    {
        if(i == 0)
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
    u32 ea = RRB;
    if(RA) ea += RRA;
    cpu.RESERVE = TRUE;
    cpu.RESERVE_ADDR = MEMEffectiveToPhysical(ea, 0);
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
    u32 ea = RRB;
    if(RA) ea += RRA;

    CR &= 0x0fffffff;
   
    if(cpu.RESERVE)
    {
        CPUWriteWord(ea, RRS);
        SET_CR0_EQ;    
        cpu.RESERVE = FALSE;
    }

    if(IS_XER_SO) SET_CR0_SO;
}
