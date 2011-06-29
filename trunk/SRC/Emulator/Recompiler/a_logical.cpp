// Logical Instructions
#include "dolphin.h"

#include "Recompiler/X86.h"

#define OP(name) void __fastcall a_##name##(u32 op, u32 pc)

// ra = rs | (0x0000 || UIMM)
OP(ORI)
{
    MOV_ECX_DD(&RRS);
    OR_ECX_IMM(UIMM);
    MOV_DD_ECX(&RRA);
}

// ra = rs | (UIMM || 0x0000)
OP(ORIS)
{
    MOV_ECX_DD(&RRS);
    OR_ECX_IMM(op << 16);
    MOV_DD_ECX(&RRA);
}
