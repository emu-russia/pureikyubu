// System Instructions
#include "dolphin.h"

#include "Recompiler/X86.h"

#define OP(name) void __fastcall a_##name##(u32 op, u32 pc)

// ---------------------------------------------------------------------------
// system registers

// spr = rs
OP(MTSPR)
{
    MOV_EAX_DD(&RRS);
    MOV_DD_EAX(&SPR[(RB << 5) | RA]);
}

// rd = spr
OP(MFSPR)
{
    MOV_EAX_DD(&SPR[(RB << 5) | RA]);
    MOV_DD_EAX(&RRD);
}

// ---------------------------------------------------------------------------
// various context synchronizing

OP(EIEIO)
{
}

OP(SYNC)
{
}

OP(ISYNC)
{
}

OP(TLBSYNC)
{
}

OP(TLBIE)
{
}
