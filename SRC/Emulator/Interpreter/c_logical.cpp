// Logical Instructions
#include "dolphin.h"

#define OP(name) void __fastcall c_##name##(uint32_t op)

#define COMPUTE_CR0(r)                                                                \
{                                                                                     \
    (CR = (CR & 0xfffffff)                   |                                        \
    ((XER & (1 << 31)) ? (0x10000000) : (0)) |                                        \
    (((int32_t)(r) < 0) ? (0x80000000) : (((int32_t)(r) > 0) ? (0x40000000) : (0x20000000))));\
}

// ra = rs & UIMM, CR0
OP(ANDID)
{
    u32 res = RRS & UIMM;
    RRA = res;
    COMPUTE_CR0(res);
}

// ra = rs & (UIMM || 0x0000), CR0
OP(ANDISD)
{
    u32 res = RRS & (op << 16);
    RRA = res;
    COMPUTE_CR0(res);
}

// ra = rs | (0x0000 || UIMM)
OP(ORI)
{
    RRA = RRS | UIMM;
}

// ra = rs | (UIMM || 0x0000)
OP(ORIS)
{
    RRA = RRS | (op << 16);
}

// ra = rs ^ (0x0000 || UIMM)
OP(XORI)
{
    RRA = RRS ^ UIMM;
}

// ra = rs ^ (UIMM || 0x0000)
OP(XORIS)
{
    RRA = RRS ^ (op << 16);
}

// ra = rs & rb
OP(AND)
{
    RRA = RRS & RRB;
}

// ra = rs & rb, CR0
OP(ANDD)
{
    u32 res = RRS & RRB;
    RRA = res;
    COMPUTE_CR0(res);
}

// ra = rs | rb
OP(OR)
{
    RRA = RRS | RRB;
}

// ra = rs | rb, CR0
OP(ORD)
{
    u32 res = RRS | RRB;
    RRA = res;
    COMPUTE_CR0(res);
}

// ra = rs ^ rb
OP(XOR)
{
    RRA = RRS ^ RRB;
}

// ra = rs ^ rb, CR0
OP(XORD)
{
    u32 res = RRS ^ RRB;
    RRA = res;
    COMPUTE_CR0(res);
}

// ra = ~(rs & rb)
OP(NAND)
{
    RRA = ~(RRS & RRB);
}

// ra = ~(rs & rb), CR0
OP(NANDD)
{
    u32 res = ~(RRS & RRB);
    RRA = res;
    COMPUTE_CR0(res);
}

// ra = ~(rs | rb)
OP(NOR)
{
    RRA = ~(RRS | RRB);
}

// ra = ~(rs | rb), CR0
OP(NORD)
{
    u32 res = ~(RRS | RRB);
    RRA = res;
    COMPUTE_CR0(res);
}

// ra = rs EQV rb
OP(EQV)
{
    RRA = ~(RRS ^ RRB);
}

// ra = rs EQV rb, CR0
OP(EQVD)
{
    u32 res = ~(RRS ^ RRB);
    RRA = res;
    COMPUTE_CR0(res);
}

// ra = rs & ~rb
OP(ANDC)
{
    RRA = RRS & (~RRB);
}

// ra = rs & ~rb, CR0
OP(ANDCD)
{
    u32 res = RRS & (~RRB);
    RRA = res;
    COMPUTE_CR0(res);
}

// ra = rs | ~rb
OP(ORC)
{
    RRA = RRS | (~RRB);
}

// ra = rs | ~rb, CR0
OP(ORCD)
{
    u32 res = RRS | (~RRB);
    RRA = res;
    COMPUTE_CR0(res);
}

// sign = rs[24]
// ra[24-31] = rs[24-31]
// ra[0-23] = (24)sign
OP(EXTSB)
{
    RRA = (u32)(s32)(s8)(u8)RRS;
}

// sign = rs[24]
// ra[24-31] = rs[24-31]
// ra[0-23] = (24)sign
// CR0
OP(EXTSBD)
{
    u32 res = (u32)(s32)(s8)(u8)RRS;
    RRA = res;
    COMPUTE_CR0(res);
}

// sign = rs[16]
// ra[16-31] = rs[16-31]
// ra[0-15] = (16)sign
OP(EXTSH)
{
    RRA = (u32)(s32)(s16)(u16)RRS;
}

// sign = rs[16]
// ra[16-31] = rs[16-31]
// ra[0-15] = (16)sign
// CR0
OP(EXTSHD)
{
    u32 res = (u32)(s32)(s16)(u16)RRS;
    RRA = res;
    COMPUTE_CR0(res);
}

// n = 0
// while n < 32
//      if rs[n] = 1 then leave
//      n = n + 1
// ra = n
OP(CNTLZW)
{
    u32 n, m, rs = RRS;
    for(n=0, m=1<<31; n<32; n++, m>>=1)
    {
        if(rs & m) break;
    }

    RRA = n;
}

// n = 0
// while n < 32
//      if rs[n] = 1 then leave
//      n = n + 1
// ra = n
// CR0
OP(CNTLZWD)
{
    u32 n, m, rs = RRS;
    for(n=0, m=1<<31; n<32; n++, m>>=1)
    {
        if(rs & m) break;
    }

    RRA = n;
    COMPUTE_CR0(n);
}
