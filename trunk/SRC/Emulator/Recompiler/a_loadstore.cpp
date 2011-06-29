// Integer Load and Store Instructions
#include "dolphin.h"

#include "Recompiler/X86.h"

#define OP(name) void __fastcall a_##name##(u32 op, u32 pc)

// ea = (ra | 0) + SIMM
// rd = 0x000000 || MEM(ea, 1)
OP(LBZ)
{
    if(RA)
    {
        MOV_ECX_DD(&RRA);
        ADD_ECX_IMM(SIMM);
        MOV_EDX_IMM(&RRD);
    }
    else
    {
        MOV_ECX_IMM(SIMM);
        MOV_EDX_IMM(&RRD);
    }
    CALL_DD(&CPUReadByte);
}

// ea = (ra | 0) + rb
// rd = 0x000000 || MEM(ea, 1)
OP(LBZX)
{
    if(RA)
    {
        MOV_ECX_DD(&RRA);
        ADD_ECX_DD(&RRB);
        MOV_EDX_IMM(&RRD);
    }
    else
    {
        MOV_ECX_DD(&RRB);
        MOV_EDX_IMM(&RRD);
    }
    CALL_DD(&CPUReadByte);
}

// ea = ra + SIMM
// rd = 0x000000 || MEM(ea, 1)
// ra = ea
OP(LBZU)
{
    MOV_ECX_DD(&RRA);
    ADD_ECX_IMM(SIMM);
    MOV_EDX_DD(&RRD);
    PUSH_ECX();
    CALL_DD(&CPUReadByte);
    POP_ECX();
    MOV_DD_ECX(&RRA);
}

// ea = ra + rb
// rd = 0x000000 || MEM(ea, 1)
// ra = ea
OP(LBZUX)
{
    MOV_ECX_DD(&RRA);
    ADD_ECX_DD(&RRB);
    MOV_EDX_DD(&RRD);
    PUSH_ECX();
    CALL_DD(&CPUReadByte);
    POP_ECX();
    MOV_DD_ECX(&RRA);
}

// ea = (ra | 0) + SIMM
// rd = 0x0000 || MEM(ea, 2)
OP(LHZ)
{
    if(RA)
    {
        MOV_ECX_DD(&RRA);
        ADD_ECX_IMM(SIMM);
        MOV_EDX_IMM(&RRD);
    }
    else
    {
        MOV_ECX_IMM(SIMM);
        MOV_EDX_IMM(&RRD);
    }
    CALL_DD(&CPUReadHalf);
}

// ea = (ra | 0) + rb
// rd = 0x0000 || MEM(ea, 2)
OP(LHZX)
{
    if(RA)
    {
        MOV_ECX_DD(&RRA);
        ADD_ECX_DD(&RRB);
        MOV_EDX_IMM(&RRD);
    }
    else
    {
        MOV_ECX_DD(&RRB);
        MOV_EDX_IMM(&RRD);
    }
    CALL_DD(&CPUReadHalf);
}

// ea = ra + SIMM
// rd = 0x0000 || MEM(ea, 2)
// ra = ea
OP(LHZU)
{
    MOV_ECX_DD(&RRA);
    ADD_ECX_IMM(SIMM);
    MOV_EDX_DD(&RRD);
    PUSH_ECX();
    CALL_DD(&CPUReadHalf);
    POP_ECX();
    MOV_DD_ECX(&RRA);
}

// ea = ra + rb
// rd = 0x0000 || MEM(ea, 2)
// ra = ea
OP(LHZUX)
{
    MOV_ECX_DD(&RRA);
    ADD_ECX_DD(&RRB);
    MOV_EDX_DD(&RRD);
    PUSH_ECX();
    CALL_DD(&CPUReadHalf);
    POP_ECX();
    MOV_DD_ECX(&RRA);
}

// ea = (ra | 0) + SIMM
// rd = MEM(ea, 4)
OP(LWZ)
{
    if(RA)
    {
        MOV_ECX_DD(&RRA);
        ADD_ECX_IMM(SIMM);
        MOV_EDX_IMM(&RRD);
    }
    else
    {
        MOV_ECX_IMM(SIMM);
        MOV_EDX_IMM(&RRD);
    }
    CALL_DD(&CPUReadWord);
}

// ea = (ra | 0) + rb
// rd = MEM(ea, 4)
OP(LWZX)
{
    if(RA)
    {
        MOV_ECX_DD(&RRA);
        ADD_ECX_DD(&RRB);
        MOV_EDX_IMM(&RRD);
    }
    else
    {
        MOV_ECX_DD(&RRB);
        MOV_EDX_IMM(&RRD);
    }
    CALL_DD(&CPUReadWord);
}

// ea = ra + SIMM
// rd = MEM(ea, 4)
// ra = ea
OP(LWZU)
{
    MOV_ECX_DD(&RRA);
    ADD_ECX_IMM(SIMM);
    MOV_EDX_DD(&RRD);
    PUSH_ECX();
    CALL_DD(&CPUReadWord);
    POP_ECX();
    MOV_DD_ECX(&RRA);
}

// ea = ra + rb
// rd = MEM(ea, 4)
// ra = ea
OP(LWZUX)
{
    MOV_ECX_DD(&RRA);
    ADD_ECX_DD(&RRB);
    MOV_EDX_DD(&RRD);
    PUSH_ECX();
    CALL_DD(&CPUReadWord);
    POP_ECX();
    MOV_DD_ECX(&RRA);
}

// ---------------------------------------------------------------------------

// ea = (ra | 0) + SIMM
// MEM(ea, 1) = rs[24-31]
OP(STB)
{
    if(RA)
    {
        MOV_ECX_DD(&RRA);
        ADD_ECX_IMM(SIMM);
        MOV_EDX_DD(&RRS);
    }
    else
    {
        MOV_ECX_IMM(SIMM);
        MOV_EDX_DD(&RRS);
    }
    CALL_DD(&CPUWriteByte);
}

// ea = (ra | 0) + rb
// MEM(ea, 1) = rs[24-31]
OP(STBX)
{
    if(RA)
    {
        MOV_ECX_DD(&RRA);
        ADD_ECX_DD(&RRB);
        MOV_EDX_DD(&RRS);
    }
    else
    {
        MOV_ECX_DD(&RRB);
        MOV_EDX_DD(&RRS);
    }
    CALL_DD(&CPUWriteByte);
}

// ea = ra + SIMM
// MEM(ea, 1) = rs[24-31]
// ra = ea
OP(STBU)
{
    MOV_ECX_DD(&RRA);
    ADD_ECX_IMM(SIMM);
    MOV_EDX_DD(&RRS);
    PUSH_ECX();
    CALL_DD(&CPUWriteByte);
    POP_ECX();
    MOV_DD_ECX(&RRA);
}

// ea = ra + rb
// MEM(ea, 1) = rs[24-31]
// ra = ea
OP(STBUX)
{
    MOV_ECX_DD(&RRA);
    ADD_ECX_DD(&RRB);
    MOV_EDX_DD(&RRS);
    PUSH_ECX();
    CALL_DD(&CPUWriteByte);
    POP_ECX();
    MOV_DD_ECX(&RRA);
}

// ea = (ra | 0) + SIMM
// MEM(ea, 2) = rs[16-31]
OP(STH)
{
    if(RA)
    {
        MOV_ECX_DD(&RRA);
        ADD_ECX_IMM(SIMM);
        MOV_EDX_DD(&RRS);
    }
    else
    {
        MOV_ECX_IMM(SIMM);
        MOV_EDX_DD(&RRS);
    }
    CALL_DD(&CPUWriteHalf);
}

// ea = (ra | 0) + rb
// MEM(ea, 2) = rs[16-31]
OP(STHX)
{
    if(RA)
    {
        MOV_ECX_DD(&RRA);
        ADD_ECX_DD(&RRB);
        MOV_EDX_DD(&RRS);
    }
    else
    {
        MOV_ECX_DD(&RRB);
        MOV_EDX_DD(&RRS);
    }
    CALL_DD(&CPUWriteHalf);
}

// ea = ra + SIMM
// MEM(ea, 2) = rs[16-31]
// ra = ea
OP(STHU)
{
    MOV_ECX_DD(&RRA);
    ADD_ECX_IMM(SIMM);
    MOV_EDX_DD(&RRS);
    PUSH_ECX();
    CALL_DD(&CPUWriteHalf);
    POP_ECX();
    MOV_DD_ECX(&RRA);
}

// ea = ra + rb
// MEM(ea, 2) = rs[16-31]
// ra = ea
OP(STHUX)
{
    MOV_ECX_DD(&RRA);
    ADD_ECX_DD(&RRB);
    MOV_EDX_DD(&RRS);
    PUSH_ECX();
    CALL_DD(&CPUWriteHalf);
    POP_ECX();
    MOV_DD_ECX(&RRA);
}

// ea = (ra | 0) + SIMM
// MEM(ea, 4) = rs
OP(STW)
{
    if(RA)
    {
        MOV_ECX_DD(&RRA);
        ADD_ECX_IMM(SIMM);
        MOV_EDX_DD(&RRS);
    }
    else
    {
        MOV_ECX_IMM(SIMM);
        MOV_EDX_DD(&RRS);
    }
    CALL_DD(&CPUWriteWord);
}

// ea = (ra | 0) + rb
// MEM(ea, 4) = rs
OP(STWX)
{
    if(RA)
    {
        MOV_ECX_DD(&RRA);
        ADD_ECX_DD(&RRB);
        MOV_EDX_DD(&RRS);
    }
    else
    {
        MOV_ECX_DD(&RRB);
        MOV_EDX_DD(&RRS);
    }
    CALL_DD(&CPUWriteWord);
}

// ea = ra + SIMM
// MEM(ea, 4) = rs
// ra = ea
OP(STWU)
{
    MOV_ECX_DD(&RRA);
    ADD_ECX_IMM(SIMM);
    MOV_EDX_DD(&RRS);
    PUSH_ECX();
    CALL_DD(&CPUWriteWord);
    POP_ECX();
    MOV_DD_ECX(&RRA);
}

// ea = ra + rb
// MEM(ea, 4) = rs
// ra = ea
OP(STWUX)
{
    MOV_ECX_DD(&RRA);
    ADD_ECX_DD(&RRB);
    MOV_EDX_DD(&RRS);
    PUSH_ECX();
    CALL_DD(&CPUWriteWord);
    POP_ECX();
    MOV_DD_ECX(&RRA);
}
