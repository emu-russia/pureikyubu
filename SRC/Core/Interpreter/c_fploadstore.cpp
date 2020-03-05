// Floating-Point Load and Store Instructions
#include "dolphin.h"
#include "interpreter.h"

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
        float res;

        if(RA) CPUReadWord(RRA + SIMM, (uint32_t*)&res);
        else CPUReadWord(SIMM, (uint32_t*)&res);

        if(PSE) PS0(RD) = PS1(RD) = (double)res;
        else FPRD(RD) = (double)res;
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
        float res;

        if(RA) CPUReadWord(RRA + RRB, (uint32_t*)&res);
        else CPUReadWord(RRB, (uint32_t*)&res);

        if(PSE) PS0(RD) = PS1(RD) = (double)res;
        else FPRD(RD) = (double)res;
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
        uint32_t ea = RRA + SIMM;
        float res;

        CPUReadWord(ea, (uint32_t*)&res);

        if(PSE) PS0(RD) = PS1(RD) = (double)res;
        else FPRD(RD) = (double)res;

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
        uint32_t ea = RRA + RRB;
        float res;

        CPUReadWord(ea, (uint32_t*)&res);

        if(PSE) PS0(RD) = PS1(RD) = (double)res;
        else FPRD(RD) = (double)res;

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
        uint32_t ea = RRA + SIMM;
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
        uint32_t ea = RRA + RRB;
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
        float data;
        
        if(PSE) data = (float)PS0(RS);
        else data = (float)FPRD(RS);

        if(RA) CPUWriteWord(RRA + SIMM, *(uint32_t*)&data);
        else CPUWriteWord(SIMM, *(uint32_t*)&data);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

// ea = (ra | 0) + rb
// MEM(ea, 4) = SINGLE(fs)
OP(STFSX)
{
    if(MSR & MSR_FP)
    {
        float num;
        uint32_t*data = (uint32_t*)&num;
        
        if(PSE) num = (float)PS0(RS);
        else num = (float)FPRD(RS);

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
        float data;
        uint32_t ea = RRA + SIMM;
        
        if(PSE) data = (float)PS0(RS);
        else data = (float)FPRD(RS);

        CPUWriteWord(ea, *(uint32_t*)&data);
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
        float num;
        uint32_t*data = (uint32_t*)&num;
        uint32_t ea = RRA + RRB;
        
        if(PSE) num = (float)PS0(RS);
        else num = (float)FPRD(RS);

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
        uint32_t ea = RRA + SIMM;
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
        uint32_t ea = RRA + RRB;
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
        uint32_t val = (uint32_t)(FPRU(RS) & 0x00000000ffffffff);
        if(RA) CPUWriteWord(RRA + RRB, val);
        else CPUWriteWord(RRB, val);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}
