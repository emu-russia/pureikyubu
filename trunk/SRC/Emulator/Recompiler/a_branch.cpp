// Integer Load and Store Instructions
#include "dolphin.h"

#include "Recompiler/X86.h"

#define OP(name) void __fastcall a_##name##(u32 op, u32 pc)

// if LK then LR = PC + 4
// if AA then PC = EXTS(LI || 0b00)
//       else PC = PC + EXTS(LI || 0b00)
OP(BX)
{
    u32 npc;
    u32 target = op & 0x03fffffc;
    if(target & 0x02000000) target |= 0xfc000000;

    switch(op & 3)
    {
        case 0:                 // b
        {
            npc = pc += target;
            pc = MEMEffectiveToPhysical(pc);

            MOV_ECX_IMM(npc);
            JMP_DD(&cpu.groups[pc >> 2]);
            break;
        }
        case 1:                 // bl
        {
            u32 lr = pc + 4;
            npc = pc += target;
            pc = MEMEffectiveToPhysical(pc);

            MOV_DD_IMM(&LR, lr & ~3);
            MOV_ECX_IMM(npc);
            JMP_DD(&cpu.groups[pc >> 2]);
            break;
        }
        case 2:                 // ba
        {
            pc = MEMEffectiveToPhysical(npc = target);

            MOV_ECX_IMM(npc);
            JMP_DD(&cpu.groups[pc >> 2]);
            break;
        }
        case 3:                 // bla
        {
            u32 lr = pc + 4;
            pc = MEMEffectiveToPhysical(npc = target);

            MOV_DD_IMM(&LR, lr & ~3);
            MOV_ECX_IMM(npc);
            JMP_DD(&cpu.groups[pc >> 2]);
            break;
        }
    }
}

// ---------------------------------------------------------------------------

// continue execution
static void no_branch(u32 pc)
{
    u32 phys = MEMEffectiveToPhysical(pc+4);
    MOV_ECX_IMM(pc+4);
    JMP_DD(&cpu.groups[phys >> 2]);
}

/* EAX = ctr_ok && cond_ok */ static void bc(u32 op)
{
    // ctr_ok = EAX, cond_ok = ECX
    int bo = RD, bi = BI;

    // EAX = counter
    if(BO(2) == 0)
    {
        XOR_EAX_EAX();
        SUB_DD_IMM(&CTR, 1);
        if(BO(3)) {SETE_AL();}
        else {SETNE_AL();}
    }
    else MOV_EAX_IMM(1);

    // ECX = condition
    if(BO(0) == 0)
    {
        XOR_ECX_ECX();
        MOV_EDX_DD(&CR);
        BT_EDX_IMM(31-bi);
        if(BO(1)) {SETB_CL();}
        else {SETNB_CL();}
    }
    else MOV_ECX_IMM(1);

    // return (ctr_ok & cond_ok)
    AND_EAX_ECX();
}

/*/
    compiled asm code :
   
        // counter check (EAX = ctr_ok)
        xor     eax, eax        ; ctr_ok = FALSE
        sub     CTR, 1          ; decrement CTR and set flags at same time
BO(3)=1 sete    al              ; if (CTR == 0) then ctr_ok = TRUE
BO(3)=0 setne   al              ; if (CTR != 0) then ctr_ok = TRUE

        // condition check (ECX = cond_ok)
        xor     ecx, ecx        ; cond_ok = FALSE
        mov     edx, CR
        bt      edx, 31-BI      ; check CR[BI] bit (result in carry flag)
BO(1)=1 setc    cl              ; if CR[BI] != 0 then cond_ok = TRUE
BO(1)=0 setnc   cl              ; if CR[BI] == 0 then cond_ok = TRUE
/*/

// if ~BO2 then CTR = CTR - 1
// ctr_ok  = BO2 | ((CTR != 0) ^ BO3)
// cond_ok = BO0 | (CR[BI] EQV BO1)
// if ctr_ok & cond_ok then
//      if LK = 1
//          LR = PC + 4
//      if AA = 1
//          then PC = EXTS(BD || 0b00)
//          else PC = PC + EXTS(BD || 0b00)
OP(BCX)
{
    // EAX = ctr_ok & cond_ok
    bc(op);

    TEST_EAX_EAX();
    JUMP_START(0);
    JE(0);
        if(op & 1) MOV_DD_IMM(&LR, (pc + 4) & ~3);
        u32 target = op & 0xfffc, npc;
        if(target & 0x8000) target |= 0xffff0000;
        if(op & 2) npc = target;
        else npc = pc + target;
        u32 phys = MEMEffectiveToPhysical(npc);
        MOV_ECX_IMM(npc);
        JMP_DD(&cpu.groups[phys >> 2]);
    JUMP_END(0);
    no_branch(pc);
}

// if ~BO2 then CTR = CTR - 1
// ctr_ok  = BO2 | ((CTR != 0) ^ BO3)
// cond_ok = BO0 | (CR[BI] EQV BO1)
// if ctr_ok & cond_ok then
//      PC = LR[0-29] || 0b00
OP(BCLR)
{
    if(op == 0x4e800020 /* blr */)
    {
        MOV_ECX_DD(&LR);
        AND_ECX_IMM(~3);
        PUSH_ECX();
        MOV_EDX_IMM(1);
        CALL_DD(&MEMEffectiveToPhysical);
        ADD_EAX_IMM(cpu.groups);
        POP_ECX();
        RE(0xff); RE(0x20);     // JMP [EAX]
        return;
    }

    // EAX = ctr_ok & cond_ok
    bc(op);

    TEST_EAX_EAX();
    JUMP_START(0);
    JE(0);
        MOV_ECX_DD(&LR);
        AND_ECX_IMM(~3);
        PUSH_ECX();
        MOV_EDX_IMM(1);
        CALL_DD(&MEMEffectiveToPhysical);
        ADD_EAX_IMM(cpu.groups);
        POP_ECX();
        RE(0xff); RE(0x20);     // JMP [EAX]
    JUMP_END(0);
    no_branch(pc);
}

// if ~BO2 then CTR = CTR - 1
// ctr_ok  = BO2 | ((CTR != 0) ^ BO3)
// cond_ok = BO0 | (CR[BI] EQV BO1)
// if ctr_ok & cond_ok then
//      NLR = PC + 4
//      PC = LR[0-29] || 0b00
//      LR = NLR
OP(BCLRL)
{
    // EAX = ctr_ok & cond_ok
    bc(op);

    TEST_EAX_EAX();
    JUMP_START(0);
    JE(0);
        u32 nlr = pc + 4;
        MOV_ECX_DD(&LR);
        AND_ECX_IMM(~3);
        PUSH_ECX();
        MOV_DD_IMM(&LR, nlr);
        MOV_EDX_IMM(1);
        CALL_DD(&MEMEffectiveToPhysical);
        ADD_EAX_IMM(cpu.groups);
        POP_ECX();
        RE(0xff); RE(0x20);     // JMP [EAX]
    JUMP_END(0);
    no_branch(pc);
}

// ---------------------------------------------------------------------------

/* ECX = cond_ok */ static void __fastcall bctr(u32 op)
{
    // cond_ok = ECX
    int bo = RD, bi = BI;

    // ECX = condition
    if(BO(0) == 0)
    {
        XOR_ECX_ECX();
        MOV_EDX_DD(&CR);
        BT_EDX_IMM(31-bi);
        if(BO(1)) {SETB_CL();}
        else {SETNB_CL();}
    }
    else MOV_ECX_IMM(1);
}

// cond_ok = BO0 | (CR[BI] EQV BO1)
// if cond_ok
//      then
//              PC = CTR || 0b00
OP(BCCTR)
{
    // ECX = cond_ok
    bctr(op);

    TEST_ECX_ECX();
    JUMP_START(0);
    JE(0);
        MOV_ECX_DD(&CTR);
        AND_ECX_IMM(~3);
        PUSH_ECX();
        MOV_EDX_IMM(1);
        CALL_DD(&MEMEffectiveToPhysical);
        ADD_EAX_IMM(cpu.groups);
        POP_ECX();
        RE(0xff); RE(0x20);     // JMP [EAX]
    JUMP_END(0);
    no_branch(pc);
}

// cond_ok = BO0 | (CR[BI] EQV BO1)
// if cond_ok
//      then
//              LR = PC + 4
//              PC = CTR || 0b00
OP(BCCTRL)
{
    // ECX = cond_ok
    bctr(op);

    TEST_ECX_ECX();
    JUMP_START(0);
    JE(0);
        MOV_DD_IMM(&LR, pc + 4);
        MOV_ECX_DD(&CTR);
        AND_ECX_IMM(~3);
        PUSH_ECX();
        MOV_EDX_IMM(1);
        CALL_DD(&MEMEffectiveToPhysical);
        ADD_EAX_IMM(cpu.groups);
        POP_ECX();
        RE(0xff); RE(0x20);     // JMP [EAX]
    JUMP_END(0);
    no_branch(pc);
}
