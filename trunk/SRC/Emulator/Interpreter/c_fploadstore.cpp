// Floating-Point Load and Store Instructions
#include "dolphin.h"

#define OP(name) void __fastcall c_##name##(u32 op)

// ---------------------------------------------------------------------------
// loads

// ea = (ra | 0) + SIMM
// if HID2[PSE] = 0
//      then fd = DOUBLE(MEM(ea, 4))
//      else fd(ps0) = SINGLE(MEM(ea, 4))
//           fd(ps1) = SINGLE(MEM(ea, 4))
OP(LFS)
{
    if(MSR & MSR_FP)
    {
        f32 res;

        if(RA) CPUReadWord(RRA + SIMM, (u32 *)&res);
        else CPUReadWord(SIMM, (u32 *)&res);

        if(PSE) PS0(RD) = PS1(RD) = (f64)res;
        else FPRD(RD) = (f64)res;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

// ea = (ra | 0) + rb
// if HID2[PSE] = 0
//      then fd = DOUBLE(MEM(ea, 4))
//      else fd(ps0) = SINGLE(MEM(ea, 4))
//           fd(ps1) = SINGLE(MEM(ea, 4))
OP(LFSX)
{
    if(MSR & MSR_FP)
    {
        f32 res;

        if(RA) CPUReadWord(RRA + RRB, (u32 *)&res);
        else CPUReadWord(RRB, (u32 *)&res);

        if(PSE) PS0(RD) = PS1(RD) = (f64)res;
        else FPRD(RD) = (f64)res;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

// ea = ra + SIMM
// if HID2[PSE] = 0
//      then fd = DOUBLE(MEM(ea, 4))
//      else fd(ps0) = SINGLE(MEM(ea, 4))
//           fd(ps1) = SINGLE(MEM(ea, 4))
// ra = ea
OP(LFSU)
{
    if(MSR & MSR_FP)
    {
        u32 ea = RRA + SIMM;
        f32 res;

        CPUReadWord(ea, (u32 *)&res);

        if(PSE) PS0(RD) = PS1(RD) = (f64)res;
        else FPRD(RD) = (f64)res;

        RRA = ea;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

// ea = ra + rb
// if HID2[PSE] = 0
//      then fd = DOUBLE(MEM(ea, 4))
//      else fd(ps0) = SINGLE(MEM(ea, 4))
//           fd(ps1) = SINGLE(MEM(ea, 4))
// ra = ea
OP(LFSUX)
{
    if(MSR & MSR_FP)
    {
        u32 ea = RRA + RRB;
        f32 res;

        CPUReadWord(ea, (u32 *)&res);

        if(PSE) PS0(RD) = PS1(RD) = (f64)res;
        else FPRD(RD) = (f64)res;

        RRA = ea;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

// ea = (ra | 0) + SIMM
// fd = MEM(ea, 8)
OP(LFD)
{
    if(MSR & MSR_FP)
    {
        if(RA) CPUReadDouble(RRA + SIMM, &FPRU(RD));
        else CPUReadDouble(SIMM, &FPRU(RD));
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

// ea = (ra | 0) + rb
// fd = MEM(ea, 8)
OP(LFDX)
{
    if(MSR & MSR_FP)
    {
        if(RA) CPUReadDouble(RRA + RRB, &FPRU(RD));
        else CPUReadDouble(RRB, &FPRU(RD));
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

// ea = ra + SIMM
// fd = MEM(ea, 8)
// ra = ea
OP(LFDU)
{
    if(MSR & MSR_FP)
    {
        u32 ea = RRA + SIMM;
        CPUReadDouble(ea, &FPRU(RD));
        RRA = ea;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

// ea = ra + rb
// fd = MEM(ea, 8)
// ra = ea
OP(LFDUX)
{
    if(MSR & MSR_FP)
    {
        u32 ea = RRA + RRB;
        CPUReadDouble(ea, &FPRU(RD));
        RRA = ea;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

// ---------------------------------------------------------------------------
// stores

// ea = (ra | 0) + SIMM
// MEM(ea, 4) = SINGLE(fs)
OP(STFS)
{
    if(MSR & MSR_FP)
    {
        f32 data;
        
        if(PSE) data = (f32)PS0(RS);
        else data = (f32)FPRD(RS);

        if(RA) CPUWriteWord(RRA + SIMM, *(u32 *)&data);
        else CPUWriteWord(SIMM, *(u32 *)&data);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

// ea = (ra | 0) + rb
// MEM(ea, 4) = SINGLE(fs)
OP(STFSX)
{
    if(MSR & MSR_FP)
    {
        f32 num;
        u32 *data = (u32 *)&num;
        
        if(PSE) num = (f32)PS0(RS);
        else num = (f32)FPRD(RS);

        if(RA) CPUWriteWord(RRA + RRB, *data);
        else CPUWriteWord(RRB, *data);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

// ea = ra + SIMM
// MEM(ea, 4) = SINGLE(fs)
// ra = ea
OP(STFSU)
{
    if(MSR & MSR_FP)
    {
        f32 data;
        u32 ea = RRA + SIMM;
        
        if(PSE) data = (f32)PS0(RS);
        else data = (f32)FPRD(RS);

        CPUWriteWord(ea, *(u32 *)&data);
        RRA = ea;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

// ea = ra + rb
// MEM(ea, 4) = SINGLE(fs)
// ra = ea
OP(STFSUX)
{
    if(MSR & MSR_FP)
    {
        f32 num;
        u32 *data = (u32 *)&num;
        u32 ea = RRA + RRB;
        
        if(PSE) num = (f32)PS0(RS);
        else num = (f32)FPRD(RS);

        CPUWriteWord(ea, *data);
        RRA = ea;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

// ea = (ra | 0) + SIMM
// MEM(ea, 8) = fs
OP(STFD)
{
    if(MSR & MSR_FP)
    {
        if(RA) CPUWriteDouble(RRA + SIMM, &FPRU(RS));
        else CPUWriteDouble(SIMM, &FPRU(RS));
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

// ea = (ra | 0) + rb
// MEM(ea, 8) = fs
OP(STFDX)
{
    if(MSR & MSR_FP)
    {
        if(RA) CPUWriteDouble(RRA + RRB, &FPRU(RS));
        else CPUWriteDouble(RRB, &FPRU(RS));
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

// ea = ra + SIMM
// MEM(ea, 8) = fs
// ra = ea
OP(STFDU)
{
    if(MSR & MSR_FP)
    {
        u32 ea = RRA + SIMM;
        CPUWriteDouble(ea, &FPRU(RS));
        RRA = ea;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

// ea = ra + rb
// MEM(ea, 8) = fs
// ra = ea
OP(STFDUX)
{
    if(MSR & MSR_FP)
    {
        u32 ea = RRA + RRB;
        CPUWriteDouble(ea, &FPRU(RS));
        RRA = ea;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

// ---------------------------------------------------------------------------
// special

// ea = (ra | 0) + rb
// MEM(ea, 4) = fs[32-63]
OP(STFIWX)
{
    if(MSR & MSR_FP)
    {
        u32 val = (u32)(FPRU(RS) & 0x00000000ffffffff);
        if(RA) CPUWriteWord(RRA + RRB, val);
        else CPUWriteWord(RRB, val);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}
