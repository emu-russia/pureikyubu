// Integer Rotate Instructions
#include "dolphin.h"

#define OP(name) void __fastcall c_##name##(u32 op)

#define COMPUTE_CR0(r)                                                                \
{                                                                                     \
    (CR = (CR & 0xfffffff)                   |                                        \
    ((XER & (1 << 31)) ? (0x10000000) : (0)) |                                        \
    (((s32)(r) < 0) ? (0x80000000) : (((s32)(r) > 0) ? (0x40000000) : (0x20000000))));\
}

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
    u32 m = cpu.rotmask[MB][ME];
    u32 r = rotl(SH, RRS);
    u32 res = r & m;
    RRA = res;
    if(op & 1) COMPUTE_CR0(res);
}

// n = rb[27-31]
// r = ROTL(rs, n)
// m = MASK(MB, ME)
// ra = r & m
OP(RLWNM)
{
    u32 m = cpu.rotmask[MB][ME];
    u32 r = rotl(RRB & 0x1f, RRS);
    u32 res = r & m;
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
    u32 m = cpu.rotmask[MB][ME];
    u32 r = rotl(SH, RRS);
    u32 res = (r & m) | (RRA & ~m);
    RRA = res;
    if(op & 1) COMPUTE_CR0(res);
}
