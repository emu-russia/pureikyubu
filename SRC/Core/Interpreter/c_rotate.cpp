// Integer Rotate Instructions
#include "dolphin.h"
#include "interpreter.h"

// fast left bit-rotation
static __declspec(naked) unsigned long __fastcall rotl(long sa, unsigned long data)
{
    __asm   rol     edx, cl
    __asm   mov     eax, edx
    __asm   ret
}

// n = SH
// r = ROTL(rs, n)
// m = MASK(MB, ME)
// ra = r & m
// CR0 (if .)
OP(RLWINM)
{
    uint32_t m = cpu.rotmask[MB][ME];
    uint32_t r = rotl(SH, RRS);
    uint32_t res = r & m;
    RRA = res;
    if(op & 1) COMPUTE_CR0(res);
}

// n = rb[27-31]
// r = ROTL(rs, n)
// m = MASK(MB, ME)
// ra = r & m
OP(RLWNM)
{
    uint32_t m = cpu.rotmask[MB][ME];
    uint32_t r = rotl(RRB & 0x1f, RRS);
    uint32_t res = r & m;
    RRA = res;
    if(op & 1) COMPUTE_CR0(res);
}

// n = SH
// r = ROTL(rs, n)
// m = MASK(mb, me)
// ra = (r & m) | (ra & ~m)
// CR0 (if .)
OP(RLWIMI)
{
    uint32_t m = cpu.rotmask[MB][ME];
    uint32_t r = rotl(SH, RRS);
    uint32_t res = (r & m) | (RRA & ~m);
    RRA = res;
    if(op & 1) COMPUTE_CR0(res);
}
