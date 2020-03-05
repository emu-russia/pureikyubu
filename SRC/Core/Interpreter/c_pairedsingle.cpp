// Paired Single Instructions
#include "dolphin.h"

#define OP(name) void __fastcall c_##name##(uint32_t op)

#define IS_NAN(n)       (((n) & 0x7ff0000000000000) == 0x7ff0000000000000 && ((n) & 0x000fffffffffffff) != 0)
#define IS_SNAN(n)      (((n) & 0x7ff0000000000000) == 0x7ff0000000000000 && ((n) & 0x000fffffffffffff) != 0 && ((n) & 0x0008000000000000) == 0)
#define SET_CRF(n, c)   (CR = (CR & (~(0xf0000000 >> (4 * n)))) | (c << (4 * (7 - n))))

OP(PS_ADD)
{
    if(MSR & MSR_FP)
    {
        PS0(RD) = PS0(RA) + PS0(RB);
        PS1(RD) = PS1(RA) + PS1(RB);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_ADDD)
{
}

OP(PS_SUB)
{
    if(MSR & MSR_FP)
    {
        PS0(RD) = PS0(RA) - PS0(RB);
        PS1(RD) = PS1(RA) - PS1(RB);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_SUBD)
{
}

OP(PS_MUL)
{
    if(MSR & MSR_FP)
    {
        PS0(RD) = PS0(RA) * PS0(RC);
        PS1(RD) = PS1(RA) * PS1(RC);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_MULD)
{
}

OP(PS_DIV)
{
    if(MSR & MSR_FP)
    {
        PS0(RD) = PS0(RA) / PS0(RB);
        PS1(RD) = PS1(RA) / PS1(RB);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_DIVD)
{
}

OP(PS_RES)
{
    if(MSR & MSR_FP)
    {
        PS0(RD) = 1.0f / PS0(RB);
        PS1(RD) = 1.0f / PS1(RB);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_RESD)
{
}

OP(PS_RSQRTE)
{
    if(MSR & MSR_FP)
    {
        PS0(RD) = 1.0f / sqrt(PS0(RB));
        PS1(RD) = 1.0f / sqrt(PS1(RB));
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_RSQRTED)
{
}

OP(PS_SEL)
{
    if(MSR & MSR_FP)
    {
        PS0(RD) = (PS0(RA) >= 0.0) ? (PS0(RC)) : (PS0(RB));
        PS1(RD) = (PS1(RA) >= 0.0) ? (PS1(RC)) : (PS1(RB));
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_SELD)
{
}

OP(PS_MULS0)
{
    if(MSR & MSR_FP)
    {
        double m0 = PS0(RA) * PS0(RC);
        double m1 = PS1(RA) * PS0(RC);
        PS0(RD) = m0;
        PS1(RD) = m1;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_MULS0D)
{
}

OP(PS_MULS1)
{
    if(MSR & MSR_FP)
    {
        double m0 = PS0(RA) * PS1(RC);
        double m1 = PS1(RA) * PS1(RC);
        PS0(RD) = m0;
        PS1(RD) = m1;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_MULS1D)
{
}

OP(PS_SUM0)
{
    if(MSR & MSR_FP)
    {
        double s0 = PS0(RA) + PS1(RB);
        double s1 = PS1(RC);
        PS0(RD) = s0;
        PS1(RD) = s1;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_SUM0D)
{
}

OP(PS_SUM1)
{
    if(MSR & MSR_FP)
    {
        double s0 = PS0(RC);
        double s1 = PS0(RA) + PS1(RB);
        PS0(RD) = s0;
        PS1(RD) = s1;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_SUM1D)
{
}

OP(PS_MADD)
{
    if(MSR & MSR_FP)
    {
        double a = PS0(RA);
        double b = PS0(RB);
        double c = PS0(RC);
        PS0(RD) = (a * c) + b;
        a = PS1(RA);
        b = PS1(RB);
        c = PS1(RC);
        PS1(RD) = (a * c) + b;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_MADDD)
{
}

OP(PS_MSUB)
{
    if(MSR & MSR_FP)
    {
        double a = PS0(RA);
        double b = PS0(RB);
        double c = PS0(RC);
        PS0(RD) = (a * c) - b;
        a = PS1(RA);
        b = PS1(RB);
        c = PS1(RC);
        PS1(RD) = (a * c) - b;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_MSUBD)
{
}

OP(PS_NMADD)
{
    if(MSR & MSR_FP)
    {
        double a = PS0(RA);
        double b = PS0(RB);
        double c = PS0(RC);
        PS0(RD) = -((a * c) + b);
        a = PS1(RA);
        b = PS1(RB);
        c = PS1(RC);
        PS1(RD) = -((a * c) + b);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_NMADDD)
{
}

OP(PS_NMSUB)
{
    if(MSR & MSR_FP)
    {
        double a = PS0(RA);
        double b = PS0(RB);
        double c = PS0(RC);
        PS0(RD) = -((a * c) - b);
        a = PS1(RA);
        b = PS1(RB);
        c = PS1(RC);
        PS1(RD) = -((a * c) - b);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_NMSUBD)
{
}

OP(PS_MADDS0)
{
    if(MSR & MSR_FP)
    {
        double s0 = (PS0(RA) * PS0(RC)) + PS0(RB);
        double s1 = (PS1(RA) * PS0(RC)) + PS1(RB);
        PS0(RD) = s0;
        PS1(RD) = s1;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_MADDS0D)
{
}

OP(PS_MADDS1)
{
    if(MSR & MSR_FP)
    {
        double s0 = (PS0(RA) * PS1(RC)) + PS0(RB);
        double s1 = (PS1(RA) * PS1(RC)) + PS1(RB);
        PS0(RD) = s0;
        PS1(RD) = s1;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_MADDS1D)
{
}

OP(PS_CMPU0)
{
    if(MSR & MSR_FP)
    {
        int n = CRFD;
        double a = PS0(RA), b = PS0(RB);
        uint64_t da, db;
        uint32_t c;

        __asm   fld     qword ptr a
        __asm   fstp    qword ptr da
        __asm   fld     qword ptr b
        __asm   fstp    qword ptr db

        if(IS_NAN(da) || IS_NAN(db)) c = 1;
        else if(a < b) c = 8;
        else if(a > b) c = 4;
        else c = 2;

        SET_CRF(n, c);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_CMPU1)
{
    if(MSR & MSR_FP)
    {
        int n = CRFD;
        double a = PS1(RA), b = PS1(RB);
        uint64_t da, db;
        uint32_t c;

        __asm   fld     qword ptr a
        __asm   fstp    qword ptr da
        __asm   fld     qword ptr b
        __asm   fstp    qword ptr db

        if(IS_NAN(da) || IS_NAN(db)) c = 1;
        else if(a < b) c = 8;
        else if(a > b) c = 4;
        else c = 2;

        SET_CRF(n, c);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_CMPO0)
{
    if(MSR & MSR_FP)
    {
        int n = CRFD;
        double a = PS0(RA), b = PS0(RB);
        uint64_t da, db;
        uint32_t c;

        __asm   fld     qword ptr a
        __asm   fstp    qword ptr da
        __asm   fld     qword ptr b
        __asm   fstp    qword ptr db

        if(IS_NAN(da) || IS_NAN(db)) c = 1;
        else if(a < b) c = 8;
        else if(a > b) c = 4;
        else c = 2;

        SET_CRF(n, c);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_CMPO1)
{
    if(MSR & MSR_FP)
    {
        int n = CRFD;
        double a = PS1(RA), b = PS1(RB);
        uint64_t da, db;
        uint32_t c;

        __asm   fld     qword ptr a
        __asm   fstp    qword ptr da
        __asm   fld     qword ptr b
        __asm   fstp    qword ptr db

        if(IS_NAN(da) || IS_NAN(db)) c = 1;
        else if(a < b) c = 8;
        else if(a > b) c = 4;
        else c = 2;

        SET_CRF(n, c);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_MR)
{
    if(MSR & MSR_FP)
    {
        double p0 = PS0(RB), p1 = PS1(RB);
        PS0(RD) = p0, PS1(RD) = p1;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_MRD)
{
}

OP(PS_NEG)
{
    if(MSR & MSR_FP)
    {
        PS0(RD) = -PS0(RB);
        PS1(RD) = -PS1(RB);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_NEGD)
{
}

OP(PS_ABS)
{
}

OP(PS_ABSD)
{
}

OP(PS_NABS)
{
}

OP(PS_NABSD)
{
}

OP(PS_MERGE00)
{
    if(MSR & MSR_FP)
    {
        double a = PS0(RA);
        double b = PS0(RB);
        PS0(RD) = a;
        PS1(RD) = b;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_MERGE00D)
{
}

OP(PS_MERGE01)
{
    if(MSR & MSR_FP)
    {
        double a = PS0(RA);
        double b = PS1(RB);
        PS0(RD) = a;
        PS1(RD) = b;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_MERGE01D)
{
}

OP(PS_MERGE10)
{
    if(MSR & MSR_FP)
    {
        double a = PS1(RA);
        double b = PS0(RB);
        PS0(RD) = a;
        PS1(RD) = b;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_MERGE10D)
{
}

OP(PS_MERGE11)
{
    if(MSR & MSR_FP)
    {
        double a = PS1(RA);
        double b = PS1(RB);
        PS0(RD) = a;
        PS1(RD) = b;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PS_MERGE11D)
{
}
