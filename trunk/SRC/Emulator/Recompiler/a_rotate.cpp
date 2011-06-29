// Integer Rotate Instructions
#include "dolphin.h"

#include "Recompiler/X86.h"

#define OP(name) void __fastcall a_##name##(u32 op, u32 pc)

/*/
#define COMPUTE_CR0(r)                                                                \
{                                                                                     \
    (CR = (CR & 0xfffffff)                   |                                        \
    ((XER & (1 << 31)) ? (0x10000000) : (0)) |                                        \
    (((s32)(r) < 0) ? (0x80000000) : (((s32)(r) > 0) ? (0x40000000) : (0x20000000))));\
}
/*/

static void calc_cr0(/* ECX = result */)
{
    CMP_ECX_IMM(0);
    JUMP_START(0);
    JGE(0 /* _GE */);
    MOV_EAX_IMM(0x80000000);
    JUMP_START(1);
    JMP(0 /* _DONE */);
//_GE:
    JUMP_END(0);
    JUMP_START(0);
    JE(0 /* _EQ */);
    MOV_EAX_IMM(0x40000000);
    JUMP_START(2);
    JMP(0 /* _DONE */);
//_EQ:
    JUMP_END(0);
    MOV_EAX_IMM(0x20000000);
//_DONE:
    JUMP_END(1);
    JUMP_END(2);
    MOV_ECX_DD(&XER);
    AND_ECX_IMM(0x80000000);
    SHR_ECX_IMM(3);
    OR_EAX_ECX();
    OR_DD_EAX(&CR);
}

/*/
    compiled asm code :

        cmp     ecx, 0
        jge     _GE
        mov     eax, 0x80000000
        jmp     _DONE
_GE:    je      _EQ
        mov     eax, 0x40000000
        jmp     _DONE
_EQ:    mov     eax, 0x20000000
_DONE:  mov     ecx, XER
        and     ecx, 0x80000000
        shr     ecx, 3
        or      eax, ecx
        or      CR, eax
/*/

// n = SH
// r = ROTL(rs, n)
// m = MASK(MB, ME)
// ra = r & m
// CR0 (if .)
OP(RLWINM)
{
    u32 mask = cpu.rotmask[MB][ME];
    MOV_ECX_DD(&RRS);
    ROL_ECX_IMM(SH);
    AND_ECX_IMM(mask);
    MOV_DD_ECX(&RRA);
    if(op & 1) calc_cr0();
}

// n = rb[27-31]
// r = ROTL(rs, n)
// m = MASK(MB, ME)
// ra = r & m
OP(RLWNM)
{
}

// n = SH
// r = ROTL(rs, n)
// m = MASK(mb, me)
// ra = (r & m) | (ra & ~m)
// CR0 (if .)
OP(RLWIMI)
{
}
