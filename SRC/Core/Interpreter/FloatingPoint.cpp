// Floating-Point Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

namespace Gekko
{

    // ---------------------------------------------------------------------------
    // arithmetic

    OP(FADD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = FPRD(RA) + FPRD(RB);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FADDD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = FPRD(RA) + FPRD(RB);
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FADDS)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = (float)(FPRD(RA) + FPRD(RB));
            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) PS1(RD) = PS0(RD);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FADDSD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = (float)(FPRD(RA) + FPRD(RB));
            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) PS1(RD) = PS0(RD);
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FSUB)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = FPRD(RA) - FPRD(RB);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FSUBD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = FPRD(RA) - FPRD(RB);
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FSUBS)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = (float)(FPRD(RA) - FPRD(RB));
            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) PS1(RD) = PS0(RD);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FSUBSD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = (float)(FPRD(RA) - FPRD(RB));
            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) PS1(RD) = PS0(RD);
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FMUL)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = FPRD(RA) * FPRD(RC);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FMULD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = FPRD(RA) * FPRD(RC);
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FMULS)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = (float)(FPRD(RA) * FPRD(RC));
            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) PS1(RD) = PS0(RD);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FMULSD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = (float)(FPRD(RA) * FPRD(RC));
            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) PS1(RD) = PS0(RD);
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FDIV)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = FPRD(RA) / FPRD(RB);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FDIVD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = FPRD(RA) / FPRD(RB);
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FDIVS)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = (float)(FPRD(RA) / FPRD(RB));
            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) PS1(RD) = PS0(RD);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FDIVSD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = (float)(FPRD(RA) / FPRD(RB));
            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) PS1(RD) = PS0(RD);
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FRES)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = 1.0 / FPRD(RB);
            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) PS1(RD) = PS0(RD);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FRESD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = 1.0 / FPRD(RB);
            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) PS1(RD) = PS0(RD);
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FRSQRTE)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = 1.0 / sqrt(FPRD(RB));
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FRSQRTED)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = 1.0 / sqrt(FPRD(RB));
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FSEL)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = (FPRD(RA) >= 0.0) ? (FPRD(RC)) : (FPRD(RB));
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FSELD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = (FPRD(RA) >= 0.0) ? (FPRD(RC)) : (FPRD(RB));
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FMADD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = FPRD(RA) * FPRD(RC) + FPRD(RB);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FMADDD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = FPRD(RA) * FPRD(RC) + FPRD(RB);
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FMADDS)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = (float)(FPRD(RA) * FPRD(RC) + FPRD(RB));
            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) PS1(RD) = PS0(RD);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FMADDSD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = (float)(FPRD(RA) * FPRD(RC) + FPRD(RB));
            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) PS1(RD) = PS0(RD);
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FMSUB)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = FPRD(RA) * FPRD(RC) - FPRD(RB);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FMSUBD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = FPRD(RA) * FPRD(RC) - FPRD(RB);
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FMSUBS)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = (float)(FPRD(RA) * FPRD(RC) - FPRD(RB));
            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) PS1(RD) = PS0(RD);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FMSUBSD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = (float)(FPRD(RA) * FPRD(RC) - FPRD(RB));
            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) PS1(RD) = PS0(RD);
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FNMADD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = -(FPRD(RA) * FPRD(RC) + FPRD(RB));
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FNMADDD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = -(FPRD(RA) * FPRD(RC) + FPRD(RB));
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FNMADDS)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = (float)(-(FPRD(RA) * FPRD(RC) + FPRD(RB)));
            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) PS1(RD) = PS0(RD);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FNMADDSD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = (float)(-(FPRD(RA) * FPRD(RC) + FPRD(RB)));
            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) PS1(RD) = PS0(RD);
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FNMSUB)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = -(FPRD(RA) * FPRD(RC) - FPRD(RB));
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FNMSUBD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = -(FPRD(RA) * FPRD(RC) - FPRD(RB));
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FNMSUBS)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = (float)(-(FPRD(RA) * FPRD(RC) - FPRD(RB)));
            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) PS1(RD) = PS0(RD);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FNMSUBSD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRD(RD) = (float)(-(FPRD(RA) * FPRD(RC) - FPRD(RB)));
            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) PS1(RD) = PS0(RD);
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FRSP)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE)
            {
                PS0(RD) = (float)FPRD(RB);
                PS1(RD) = PS0(RD);
            }
            else FPRD(RD) = (float)FPRD(RB);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FRSPD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE)
            {
                PS0(RD) = (float)FPRD(RB);
                PS1(RD) = PS0(RD);
            }
            else FPRD(RD) = (float)FPRD(RB);
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FCTIW)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRU(RD) = (uint64_t)(uint32_t)(int32_t)FPRD(RB);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FCTIWD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRU(RD) = (uint64_t)(uint32_t)(int32_t)FPRD(RB);
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FCTIWZ)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRU(RD) = (uint64_t)(uint32_t)(int32_t)FPRD(RB);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FCTIWZD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRU(RD) = (uint64_t)(uint32_t)(int32_t)FPRD(RB);
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FNEG)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRU(RD) = FPRU(RB) ^ 0x8000000000000000;
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FNEGD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRU(RD) = FPRU(RB) ^ 0x8000000000000000;
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FABS)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRU(RD) = FPRU(RB) & ~0x8000000000000000;
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FABSD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRU(RD) = FPRU(RB) & ~0x8000000000000000;
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FNABS)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRU(RD) = FPRU(RB) | 0x8000000000000000;
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FNABSD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRU(RD) = FPRU(RB) | 0x8000000000000000;
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    // ---------------------------------------------------------------------------
    // compare

    OP(FCMPU)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            int32_t n = CRFD;
            double a = FPRD(RA), b = FPRD(RB);
            uint64_t da = FPRU(RA), db = FPRU(RB);
            uint32_t c;

            if (IS_NAN(da) || IS_NAN(db)) c = 1;
            else if (a < b) c = 8;
            else if (a > b) c = 4;
            else c = 2;

            Gekko->regs.fpscr = (Gekko->regs.fpscr & 0xffff0fff) | (c << 12);
            Gekko->regs.cr = (Gekko->regs.cr & (~(0xf << ((7 - n) * 4)))) | (c << ((7 - n) * 4));
            if (IS_SNAN(da) || IS_SNAN(db))
            {
                Gekko->regs.fpscr = Gekko->regs.fpscr | 0x01000000;
            }
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FCMPO)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            int32_t n = CRFD;
            double a = FPRD(RA), b = FPRD(RB);
            uint64_t da = FPRU(RA), db = FPRU(RB);
            uint32_t c;

            if (IS_NAN(da) || IS_NAN(db)) c = 1;
            else if (a < b) c = 8;
            else if (a > b) c = 4;
            else c = 2;

            Gekko->regs.fpscr = (Gekko->regs.fpscr & 0xffff0fff) | (c << 12);
            Gekko->regs.cr = (Gekko->regs.cr & (~(0xf << ((7 - n) * 4)))) | (c << ((7 - n) * 4));
            if (IS_SNAN(da) || IS_SNAN(db))
            {
                Gekko->regs.fpscr = Gekko->regs.fpscr | 0x01000000;
            }
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    // ---------------------------------------------------------------------------
    // move

    // fd[32-63] = FPSCR
    OP(MFFS)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRU(RD) = (uint64_t)Gekko->regs.fpscr;
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    // fd[32-63] = FPSCR, CR1
    OP(MFFSD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRU(RD) = (uint64_t)Gekko->regs.fpscr;
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    // CR[crfD] = FPSCR[crfS]
    OP(MCRFS)
    {
        uint32_t fp = (Gekko->regs.fpscr >> (28 - RA)) & 0xf;
        Gekko->regs.cr &= ~(0xf0000000 >> RD);
        Gekko->regs.cr |= fp << (28 - RD);
        Gekko->regs.pc += 4;
    }

    // mask = (4)FM[0] || (4)FM[1] || ... || (4)FM[7]
    // FPSCR = (fb & mask) | (FPSCR & ~mask)
    OP(MTFSF)
    {
        uint32_t m = 0, fm = FM;

        for (int i = 7; i >= 0; i--)
        {
            if ((fm >> i) & 1)
            {
                m |= 0xf;
            }
            m <<= 4;
        }

        Gekko->regs.fpscr = ((uint32_t)FPRU(RB) & m) | (Gekko->regs.fpscr & ~m);
        Gekko->regs.pc += 4;
    }

    // mask = (4)FM[0] || (4)FM[1] || ... || (4)FM[7]
    // FPSCR = (fb & mask) | (FPSCR & ~mask)
    OP(MTFSFD)
    {
        uint32_t m = 0, fm = FM;

        for (int i = 7; i >= 0; i--)
        {
            if ((fm >> i) & 1)
            {
                m |= 0xf;
            }
            m <<= 4;
        }

        Gekko->regs.fpscr = ((uint32_t)FPRU(RB) & m) | (Gekko->regs.fpscr & ~m);
        COMPUTE_CR1();
        Gekko->regs.pc += 4;
    }

    // FPSCR(crbD) = 0 (clear bit)
    OP(MTFSB0)
    {
        uint32_t m = 1 << (31 - CRBD);
        Gekko->regs.fpscr &= ~m;
        Gekko->regs.pc += 4;
    }

    // FPSCR(crbD) = 0 (clear bit), CR1
    OP(MTFSB0D)
    {
        uint32_t m = 1 << (31 - CRBD);
        Gekko->regs.fpscr &= ~m;
        COMPUTE_CR1();
        Gekko->regs.pc += 4;
    }

    // FPSCR(crbD) = 1 (set bit)
    OP(MTFSB1)
    {
        uint32_t m = 1 << (31 - CRBD);
        Gekko->regs.fpscr = (Gekko->regs.fpscr & ~m) | m;
        Gekko->regs.pc += 4;
    }

    // FPSCR(crbD) = 1 (set bit), CR1
    OP(MTFSB1D)
    {
        uint32_t m = 1 << (31 - CRBD);
        Gekko->regs.fpscr = (Gekko->regs.fpscr & ~m) | m;
        COMPUTE_CR1();
        Gekko->regs.pc += 4;
    }

    // fd = fb
    OP(FMR)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRU(RD) = FPRU(RB);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(FMRD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            FPRU(RD) = FPRU(RB);
            COMPUTE_CR1();
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

}
