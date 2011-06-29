// Integer Compare Instructions
#include "dolphin.h"

#include "Recompiler/X86.h"

#define OP(name) void __fastcall a_##name##(u32 op, u32 pc)

// signed a <> b compare helper
/* ECX = c */ static void scmp(/* a = EAX, b = EDX */)
{
    CMP_EAX_EDX();
    JUMP_START(0);
    JGE(0 /* _GE */);
    MOV_ECX_IMM(8);
    JUMP_START(1);
    JMP(0 /* _DONE */);
//_GE:
    JUMP_END(0);
    JUMP_START(0);
    JE(0 /* _EQ */);
    MOV_ECX_IMM(4);
    JUMP_START(2);
    JMP(0 /* _DONE */);
//_EQ:
    JUMP_END(0);
    MOV_ECX_IMM(2);
//_DONE:
    JUMP_END(1);
    JUMP_END(2);
    MOV_EAX_DD(&XER);
    SHR_EAX_IMM(31);
    OR_ECX_EAX();
}

/*/
    compiled asm code :
        cmp     eax, edx        ; compare a <> b
        jge     _GE             ; if a < b, SIGNED
        mov     ecx, 8          ; c = 100
        jmp     _DONE
_GE:    je      _EQ             ; if a > b
        mov     ecx, 4          ; c = 010
        jmp     _DONE
_EQ:    mov     ecx, 2          ; c = 001

_DONE:  mov     eax, XER
        shr     eax, 31
        or      ECX, eax        ; <-- ECX = c || XER[SO]
/*/

// unsigned a <> b compare helper
/* ECX = c */ static void ucmp(/* a = EAX, b = EDX */)
{
    CMP_EAX_EDX();
    JUMP_START(0);
    JAE(0 /* _GE */);
    MOV_ECX_IMM(8);
    JUMP_START(1);
    JMP(0 /* _DONE */);
//_GE:
    JUMP_END(0);
    JUMP_START(0);
    JE(0 /* _EQ */);
    MOV_ECX_IMM(4);
    JUMP_START(2);
    JMP(0 /* _DONE */);
//_EQ:
    JUMP_END(0);
    MOV_ECX_IMM(2);
//_DONE:
    JUMP_END(1);
    JUMP_END(2);
    MOV_EAX_DD(&XER);
    SHR_EAX_IMM(31);
    OR_ECX_EAX();
}

/*/
    compiled asm code :
        cmp     eax, edx        ; compare a <> b
        jae     _GE             ; if a < b, UNSIGNED
        mov     ecx, 8          ; c = 100
        jmp     _DONE
_GE:    je      _EQ             ; if a > b
        mov     ecx, 4          ; c = 010
        jmp     _DONE
_EQ:    mov     ecx, 2          ; c = 001

_DONE:  mov     eax, XER
        shr     eax, 31
        or      ECX, eax        ; <-- ECX = c || XER[SO]
/*/

// set CR n field
static void set_cr(int n /* 0...7 */ /* c = ECX */)
{
    u32 mask = 0xf0000000 >> (4 * n);
    AND_DD_IMM(&CR, ~mask);
    SHL_ECX_IMM(4 * (7 - n));
    OR_DD_ECX(&CR);
}

// a = ra (signed)
// b = SIMM
// if a < b
//      then c = 0b100
//      else if a > b
//          then c = 0b010
//          else c = 0b001
// CR[4*crf..4*crf+3] = c || XER[SO]
OP(CMPI)
{
    // EAX = a, EDX = b
    MOV_EAX_DD(&RRA);
    MOV_EDX_IMM(SIMM);

    // signed compare
    /* ECX = */ scmp(/* a = EAX, b = EDX */);

    // set CR n=crfD field
    set_cr(CRFD /* c = ECX */);
}

// a = ra (signed)
// b = rb (signed)
// if a < b
//      then c = 0b100
//      else if a > b
//          then c = 0b010
//          else c = 0b001
// CR[4*crf..4*crf+3] = c || XER[SO]
OP(CMP)
{
    // EAX = a, EDX = b
    MOV_EAX_DD(&RRA);
    MOV_EDX_DD(&RRB);

    // signed compare
    /* ECX = */ scmp(/* a = EAX, b = EDX */);

    // set CR n=crfD field
    set_cr(CRFD /* c = ECX */);
}

// a = ra (unsigned)
// b = 0x0000 || UIMM
// if a < b
//      then c = 0b100
//      else if a > b
//          then c = 0b010
//          else c = 0b001
// CR[4*crf..4*crf+3] = c || XER[SO]
OP(CMPLI)
{
    // EAX = a, EDX = b
    MOV_EAX_DD(&RRA);
    MOV_EDX_IMM(UIMM);

    // unsigned compare
    /* ECX = */ ucmp(/* a = EAX, b = EDX */);

    // set CR n=crfD field
    set_cr(CRFD /* c = ECX */);
}

// a = ra (unsigned)
// b = rb (unsigned)
// if a < b
//      then c = 0b100
//      else if a > b
//          then c = 0b010
//          else c = 0b001
// CR[4*crf..4*crf+3] = c || XER[SO]
OP(CMPL)
{
    // EAX = a, EDX = b
    MOV_EAX_DD(&RRA);
    MOV_EDX_DD(&RRB);

    // unsigned compare
    /* ECX = */ ucmp(/* a = EAX, b = EDX */);

    // set CR n=crfD field
    set_cr(CRFD /* c = ECX */);
}
