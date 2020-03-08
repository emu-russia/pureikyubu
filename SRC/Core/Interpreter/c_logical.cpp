// Logical Instructions
#include "../pch.h"
#include "interpreter.h"

// ra = rs & UIMM, CR0
OP(ANDID)
{
    uint32_t res = RRS & UIMM;
    RRA = res;
    COMPUTE_CR0(res);
}

// ra = rs & (UIMM || 0x0000), CR0
OP(ANDISD)
{
    uint32_t res = RRS & (op << 16);
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
    uint32_t res = RRS & RRB;
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
    uint32_t res = RRS | RRB;
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
    uint32_t res = RRS ^ RRB;
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
    uint32_t res = ~(RRS & RRB);
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
    uint32_t res = ~(RRS | RRB);
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
    uint32_t res = ~(RRS ^ RRB);
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
    uint32_t res = RRS & (~RRB);
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
    uint32_t res = RRS | (~RRB);
    RRA = res;
    COMPUTE_CR0(res);
}

// sign = rs[24]
// ra[24-31] = rs[24-31]
// ra[0-23] = (24)sign
OP(EXTSB)
{
    RRA = (uint32_t)(int32_t)(int8_t)(uint8_t)RRS;
}

// sign = rs[24]
// ra[24-31] = rs[24-31]
// ra[0-23] = (24)sign
// CR0
OP(EXTSBD)
{
    uint32_t res = (uint32_t)(int32_t)(int8_t)(uint8_t)RRS;
    RRA = res;
    COMPUTE_CR0(res);
}

// sign = rs[16]
// ra[16-31] = rs[16-31]
// ra[0-15] = (16)sign
OP(EXTSH)
{
    RRA = (uint32_t)(int32_t)(int16_t)(uint16_t)RRS;
}

// sign = rs[16]
// ra[16-31] = rs[16-31]
// ra[0-15] = (16)sign
// CR0
OP(EXTSHD)
{
    uint32_t res = (uint32_t)(int32_t)(int16_t)(uint16_t)RRS;
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
    uint32_t n, m, rs = RRS;
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
    uint32_t n, m, rs = RRS;
    for(n=0, m=1<<31; n<32; n++, m>>=1)
    {
        if(rs & m) break;
    }

    RRA = n;
    COMPUTE_CR0(n);
}
