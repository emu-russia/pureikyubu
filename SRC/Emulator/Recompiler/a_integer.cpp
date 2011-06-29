// Integer Instructions
#include "dolphin.h"

#include "Recompiler/X86.h"

#define OP(name) void __fastcall a_##name##(u32 op, u32 pc)

// rd = (ra | 0) + SIMM
OP(ADDI)
{
    if(RA)
    {
        MOV_EAX_DD(&RRA);
        ADD_EAX_IMM(SIMM);
        MOV_DD_EAX(&RRD);
    }
    else MOV_DD_IMM(&RRD, SIMM);
}

// rd = (ra | 0) + (SIMM || 0x0000)
OP(ADDIS)
{
    if(RA)
    {
        MOV_ECX_DD(&RRA);
        ADD_ECX_IMM(SIMM << 16);
        MOV_DD_ECX(&RRD);
    }
    else MOV_DD_IMM(&RRD, SIMM << 16);
}
