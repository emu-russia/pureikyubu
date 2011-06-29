// Branch Instructions
#include "dolphin.h"

#define OP(name) void __fastcall c_##name##(u32 op)

// PC = PC + EXTS(LI || 0b00)
OP(B)
{
    u32 target = op & 0x03fffffc;
    if(target & 0x02000000) target |= 0xfc000000;
    PC = PC + target;
}

// PC = EXTS(LI || 0b00)
OP(BA)
{
    u32 target = op & 0x03fffffc;
    if(target & 0x02000000) target |= 0xfc000000;
    PC = target;
}

// LR = PC + 4, PC = PC + EXTS(LI || 0b00)
OP(BL)
{
    u32 target = op & 0x03fffffc;
    if(target & 0x02000000) target |= 0xfc000000;
    LR = PC + 4;
    PC = PC + target;
}

// LR = PC + 4, PC = EXTS(LI || 0b00)
OP(BLA)
{
    u32 target = op & 0x03fffffc;
    if(target & 0x02000000) target |= 0xfc000000;
    LR = PC + 4;
    PC = target;
}

static void (__fastcall *bx[4])(u32 op) = { c_B, c_BL, c_BA, c_BLA };

OP(BX)
{
    bx[op & 3](op);
    cpu.branch = TRUE;
}

// ---------------------------------------------------------------------------

// calculation of conditional branch
static BOOL bc(u32 op)
{
    BOOL ctr_ok, cond_ok;
    s32 bo = RD, bi = BI;

    if(BO(2) == 0)
    {
        CTR--;

        if(BO(3)) ctr_ok = (CTR == 0);
        else ctr_ok = (CTR != 0);
    }
    else ctr_ok = TRUE;

    if(BO(0) == 0)
    {
        if(BO(1)) cond_ok = ((CR << bi) & 0x80000000) != 0;
        else cond_ok = ((CR << bi) & 0x80000000) == 0;
    }
    else cond_ok = TRUE;

    return (ctr_ok & cond_ok);
}

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
    if(bc(op))
    {
        if(op & 1) LR = PC + 4; // LK

        u32 target = op & 0xfffc;
        if(target & 0x8000) target |= 0xffff0000;
        if(op & 2) PC = target; // AA
        else PC += target;
        cpu.branch = TRUE;
    }
}

// if ~BO2 then CTR = CTR - 1
// ctr_ok  = BO2 | ((CTR != 0) ^ BO3)
// cond_ok = BO0 | (CR[BI] EQV BO1)
// if ctr_ok & cond_ok then
//      PC = LR[0-29] || 0b00
OP(BCLR)
{
    if(bc(op))
    {
        PC = LR & ~3;
        cpu.branch = TRUE;
    }
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
    if(bc(op))
    {
        u32 lr = PC + 4;
        PC = LR & ~3;
        LR = lr;
        cpu.branch = TRUE;
    }
}

// ---------------------------------------------------------------------------

// calculation of conditional to count register branch
static BOOL bctr(u32 op)
{
    BOOL cond_ok;
    s32 bo = RD, bi = BI;

    if(BO(0) == 0)
    {
        if(BO(1)) cond_ok = ((CR << bi) & 0x80000000) != 0;
        else cond_ok = ((CR << bi) & 0x80000000) == 0;
    }
    else cond_ok = TRUE;

    return cond_ok;
}

// cond_ok = BO0 | (CR[BI] EQV BO1)
// if cond_ok
//      then
//              PC = CTR || 0b00
OP(BCCTR)
{
    if(bctr(op))
    {
        PC = CTR & ~3;
        cpu.branch = TRUE;
    }
}

// cond_ok = BO0 | (CR[BI] EQV BO1)
// if cond_ok
//      then
//              LR = PC + 4
//              PC = CTR || 0b00
OP(BCCTRL)
{
    if(bctr(op))
    {
        LR = PC + 4;
        PC = CTR & ~3;
        cpu.branch = TRUE;
    }
}
