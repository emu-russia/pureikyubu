// Floating-Point Instructions
#include "dolphin.h"

#define OP(name) void __fastcall c_##name##(uint32_t op)

#define COMPUTE_CR1()                                                                 \
{                                                                                     \
    CR = (CR & 0xf0ffffff) | ((FPSCR & 0xf0000000) >> 4);                             \
}

#define IS_NAN(n)       (((n) & 0x7ff0000000000000) == 0x7ff0000000000000 && ((n) & 0x000fffffffffffff) != 0)
#define IS_SNAN(n)      (((n) & 0x7ff0000000000000) == 0x7ff0000000000000 && ((n) & 0x000fffffffffffff) != 0 && ((n) & 0x0008000000000000) == 0)

// ---------------------------------------------------------------------------
// arithmetic

OP(FADD)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = FPRD(RA) + FPRD(RB);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FADDD)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = FPRD(RA) + FPRD(RB);
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FADDS)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = (float)(FPRD(RA) + FPRD(RB));
        if(PSE) PS1(RD) = PS0(RD);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FADDSD)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = (float)(FPRD(RA) + FPRD(RB));
        if(PSE) PS1(RD) = PS0(RD);
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FSUB)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = FPRD(RA) - FPRD(RB);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FSUBD)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = FPRD(RA) - FPRD(RB);
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FSUBS)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = (float)(FPRD(RA) - FPRD(RB));
        if(PSE) PS1(RD) = PS0(RD);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FSUBSD)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = (float)(FPRD(RA) - FPRD(RB));
        if(PSE) PS1(RD) = PS0(RD);
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FMUL)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = FPRD(RA) * FPRD(RC);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FMULD)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = FPRD(RA) * FPRD(RC);
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FMULS)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = (float)(FPRD(RA) * FPRD(RC));
        if(PSE) PS1(RD) = PS0(RD);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FMULSD)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = (float)(FPRD(RA) * FPRD(RC));
        if(PSE) PS1(RD) = PS0(RD);
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FDIV)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = FPRD(RA) / FPRD(RB);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FDIVD)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = FPRD(RA) / FPRD(RB);
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FDIVS)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = (float)(FPRD(RA) / FPRD(RB));
        if(PSE) PS1(RD) = PS0(RD);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FDIVSD)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = (float)(FPRD(RA) / FPRD(RB));
        if(PSE) PS1(RD) = PS0(RD);
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FRES)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = 1.0 / FPRD(RB);
        if(PSE) PS1(RD) = PS0(RD);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FRESD)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = 1.0 / FPRD(RB);
        if(PSE) PS1(RD) = PS0(RD);
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FRSQRTE)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = 1.0 / sqrt(FPRD(RB));
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FRSQRTED)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = 1.0 / sqrt(FPRD(RB));
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FSEL)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = (FPRD(RA) >= 0.0) ? (FPRD(RC)) : (FPRD(RB));
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FSELD)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = (FPRD(RA) >= 0.0) ? (FPRD(RC)) : (FPRD(RB));
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FMADD)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = FPRD(RA) * FPRD(RC) + FPRD(RB);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FMADDD)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = FPRD(RA) * FPRD(RC) + FPRD(RB);
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FMADDS)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = (float)(FPRD(RA) * FPRD(RC) + FPRD(RB));
        if(PSE) PS1(RD) = PS0(RD);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FMADDSD)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = (float)(FPRD(RA) * FPRD(RC) + FPRD(RB));
        if(PSE) PS1(RD) = PS0(RD);
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FMSUB)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = FPRD(RA) * FPRD(RC) - FPRD(RB);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FMSUBD)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = FPRD(RA) * FPRD(RC) - FPRD(RB);
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FMSUBS)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = (float)(FPRD(RA) * FPRD(RC) - FPRD(RB));
        if(PSE) PS1(RD) = PS0(RD);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FMSUBSD)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = (float)(FPRD(RA) * FPRD(RC) - FPRD(RB));
        if(PSE) PS1(RD) = PS0(RD);
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FNMADD)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = -(FPRD(RA) * FPRD(RC) + FPRD(RB));
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FNMADDD)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = -(FPRD(RA) * FPRD(RC) + FPRD(RB));
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FNMADDS)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = (float)(-(FPRD(RA) * FPRD(RC) + FPRD(RB)));
        if(PSE) PS1(RD) = PS0(RD);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FNMADDSD)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = (float)(-(FPRD(RA) * FPRD(RC) + FPRD(RB)));
        if(PSE) PS1(RD) = PS0(RD);
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FNMSUB)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = -(FPRD(RA) * FPRD(RC) - FPRD(RB));
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FNMSUBD)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = -(FPRD(RA) * FPRD(RC) - FPRD(RB));
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FNMSUBS)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = (float)(-(FPRD(RA) * FPRD(RC) - FPRD(RB)));
        if(PSE) PS1(RD) = PS0(RD);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FNMSUBSD)
{
    if(MSR & MSR_FP)
    {
        FPRD(RD) = (float)(-(FPRD(RA) * FPRD(RC) - FPRD(RB)));
        if(PSE) PS1(RD) = PS0(RD);
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FRSP)
{
    if(MSR & MSR_FP)
    {
        if(PSE)
        {
            PS0(RD) = (float)FPRD(RB);
            PS1(RD) = PS0(RD);
        }
        else FPRD(RD) = (float)FPRD(RB);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FRSPD)
{
    if(MSR & MSR_FP)
    {
        if(PSE)
        {
            PS0(RD) = (float)FPRD(RB);
            PS1(RD) = PS0(RD);
        }
        else FPRD(RD) = (float)FPRD(RB);
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FCTIW)
{
    if(MSR & MSR_FP)
    {
        FPRU(RD) = (uint64_t)(uint32_t)(int32_t)FPRD(RB);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FCTIWD)
{
    if(MSR & MSR_FP)
    {
        FPRU(RD) = (uint64_t)(uint32_t)(int32_t)FPRD(RB);
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FCTIWZ)
{
    if(MSR & MSR_FP)
    {
        FPRU(RD) = (uint64_t)(uint32_t)(int32_t)FPRD(RB);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FCTIWZD)
{
    if(MSR & MSR_FP)
    {
        FPRU(RD) = (uint64_t)(uint32_t)(int32_t)FPRD(RB);
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FNEG)
{
    if(MSR & MSR_FP)
    {
        FPRU(RD) = FPRU(RB) ^ 0x8000000000000000;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FNEGD)
{
    if(MSR & MSR_FP)
    {
        FPRU(RD) = FPRU(RB) ^ 0x8000000000000000;
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FABS)
{
    if(MSR & MSR_FP)
    {
        FPRU(RD) = FPRU(RB) & ~0x8000000000000000;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FABSD)
{
    if(MSR & MSR_FP)
    {
        FPRU(RD) = FPRU(RB) & ~0x8000000000000000;
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FNABS)
{
    if(MSR & MSR_FP)
    {
        FPRU(RD) = FPRU(RB) | 0x8000000000000000;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FNABSD)
{
    if(MSR & MSR_FP)
    {
        FPRU(RD) = FPRU(RB) | 0x8000000000000000;
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

// ---------------------------------------------------------------------------
// compare

OP(FCMPU)
{
    if(MSR & MSR_FP)
    {
        int32_t n = CRFD;
        double a = FPRD(RA), b = FPRD(RB);
        uint64_t da = FPRU(RA), db = FPRU(RB);
        uint32_t c;

        if(IS_NAN(da) || IS_NAN(db)) c = 1;
        else if(a < b) c = 8;
        else if(a > b) c = 4;
        else c = 2;
        
        FPSCR = (FPSCR & 0xffff0fff) | (c << 12);
        CR = (CR & (~(0xf << ((7 - n) * 4)))) | (c << ((7 - n) * 4));
        if(IS_SNAN(da) || IS_SNAN(db))
        {
            FPSCR = FPSCR | 0x01000000;
        }
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FCMPO)
{
    if(MSR & MSR_FP)
    {
        int32_t n = CRFD;
        double a = FPRD(RA), b = FPRD(RB);
        uint64_t da = FPRU(RA), db = FPRU(RB);
        uint32_t c;

        if(IS_NAN(da) || IS_NAN(db)) c = 1;
        else if(a < b) c = 8;
        else if(a > b) c = 4;
        else c = 2;
        
        FPSCR = (FPSCR & 0xffff0fff) | (c << 12);
        CR = (CR & (~(0xf << ((7 - n) * 4)))) | (c << ((7 - n) * 4));
        if(IS_SNAN(da) || IS_SNAN(db))
        {
            FPSCR = FPSCR | 0x01000000;
        }
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

// ---------------------------------------------------------------------------
// move

// fd[32-63] = FPSCR
OP(MFFS)
{
    if(MSR & MSR_FP)
    {
        FPRU(RD) = (uint64_t)FPSCR;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

// fd[32-63] = FPSCR, CR1
OP(MFFSD)
{
    if(MSR & MSR_FP)
    {
        FPRU(RD) = (uint64_t)FPSCR;
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

// CR[crfD] = FPSCR[crfS]
OP(MCRFS)
{
    uint32_t fp = (FPSCR >> (28 - RA)) & 0xf;
    CR &= ~(0xf0000000 >> RD);
    CR |= fp << (28 - RD);
}

// mask = (4)FM[0] || (4)FM[1] || ... || (4)FM[7]
// FPSCR = (fb & mask) | (FPSCR & ~mask)
OP(MTFSF)
{
    uint32_t m = 0, fm = FM;

    for(int i=7; i>=0; i--)
    {
        if((fm >> i) & 1)
        {
            m |= 0xf;
        }
        m <<= 4;
    }

    FPSCR = ((uint32_t)FPRU(RB) & m) | (FPSCR & ~m);
}

// mask = (4)FM[0] || (4)FM[1] || ... || (4)FM[7]
// FPSCR = (fb & mask) | (FPSCR & ~mask)
OP(MTFSFD)
{
    uint32_t m = 0, fm = FM;

    for(int i=7; i>=0; i--)
    {
        if((fm >> i) & 1)
        {
            m |= 0xf;
        }
        m <<= 4;
    }

    FPSCR = ((uint32_t)FPRU(RB) & m) | (FPSCR & ~m);
    COMPUTE_CR1();
}

// FPSCR(crbD) = 0 (clear bit)
OP(MTFSB0)
{
    uint32_t m = 1 << (31 - CRBD);
    FPSCR &= ~m;
}

// FPSCR(crbD) = 0 (clear bit), CR1
OP(MTFSB0D)
{
    uint32_t m = 1 << (31 - CRBD);
    FPSCR &= ~m;
    COMPUTE_CR1();
}

// FPSCR(crbD) = 1 (set bit)
OP(MTFSB1)
{
    uint32_t m = 1 << (31 - CRBD);
    FPSCR = (FPSCR & ~m) | m;
}

// FPSCR(crbD) = 1 (set bit), CR1
OP(MTFSB1D)
{
    uint32_t m = 1 << (31 - CRBD);
    FPSCR = (FPSCR & ~m) | m;
    COMPUTE_CR1();
}

// fd = fb
OP(FMR)
{
    if(MSR & MSR_FP)
    {
        FPRU(RD) = FPRU(RB);
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(FMRD)
{
    if(MSR & MSR_FP)
    {
        FPRU(RD) = FPRU(RB);
        COMPUTE_CR1();
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}
