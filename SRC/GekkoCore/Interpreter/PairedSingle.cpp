// Paired Single Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

using namespace Debug;

namespace Gekko
{
    OP(PS_ADD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            PS0(RD) = PS0(RA) + PS0(RB);
            PS1(RD) = PS1(RA) + PS1(RB);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_ADDD)
    {
        Halt("PS_ADDD\n");
    }

    OP(PS_SUB)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            PS0(RD) = PS0(RA) - PS0(RB);
            PS1(RD) = PS1(RA) - PS1(RB);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_SUBD)
    {
        Halt("PS_SUBD\n");
    }

    OP(PS_MUL)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            PS0(RD) = PS0(RA) * PS0(RC);
            PS1(RD) = PS1(RA) * PS1(RC);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MULD)
    {
        Halt("PS_MULD\n");
    }

    OP(PS_DIV)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            PS0(RD) = PS0(RA) / PS0(RB);
            PS1(RD) = PS1(RA) / PS1(RB);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_DIVD)
    {
        Halt("PS_DIVD\n");
    }

    OP(PS_RES)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            PS0(RD) = 1.0f / PS0(RB);
            PS1(RD) = 1.0f / PS1(RB);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_RESD)
    {
        Halt("PS_RESD\n");
    }

    OP(PS_RSQRTE)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            PS0(RD) = 1.0f / sqrt(PS0(RB));
            PS1(RD) = 1.0f / sqrt(PS1(RB));
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_RSQRTED)
    {
        Halt("PS_RSQRTED\n");
    }

    OP(PS_SEL)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            PS0(RD) = (PS0(RA) >= 0.0) ? (PS0(RC)) : (PS0(RB));
            PS1(RD) = (PS1(RA) >= 0.0) ? (PS1(RC)) : (PS1(RB));
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_SELD)
    {
        Halt("PS_SELD\n");
    }

    OP(PS_MULS0)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double m0 = PS0(RA) * PS0(RC);
            double m1 = PS1(RA) * PS0(RC);
            PS0(RD) = m0;
            PS1(RD) = m1;
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MULS0D)
    {
        Halt("PS_MULS0D\n");
    }

    OP(PS_MULS1)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double m0 = PS0(RA) * PS1(RC);
            double m1 = PS1(RA) * PS1(RC);
            PS0(RD) = m0;
            PS1(RD) = m1;
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MULS1D)
    {
        Halt("PS_MULS1D\n");
    }

    OP(PS_SUM0)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double s0 = PS0(RA) + PS1(RB);
            double s1 = PS1(RC);
            PS0(RD) = s0;
            PS1(RD) = s1;
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_SUM0D)
    {
        Halt("PS_SUM0D\n");
    }

    OP(PS_SUM1)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double s0 = PS0(RC);
            double s1 = PS0(RA) + PS1(RB);
            PS0(RD) = s0;
            PS1(RD) = s1;
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_SUM1D)
    {
        Halt("PS_SUM1D\n");
    }

    OP(PS_MADD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double a = PS0(RA);
            double b = PS0(RB);
            double c = PS0(RC);
            PS0(RD) = (a * c) + b;
            a = PS1(RA);
            b = PS1(RB);
            c = PS1(RC);
            PS1(RD) = (a * c) + b;
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MADDD)
    {
        Halt("PS_MADDD\n");
    }

    OP(PS_MSUB)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double a = PS0(RA);
            double b = PS0(RB);
            double c = PS0(RC);
            PS0(RD) = (a * c) - b;
            a = PS1(RA);
            b = PS1(RB);
            c = PS1(RC);
            PS1(RD) = (a * c) - b;
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MSUBD)
    {
        Halt("PS_MSUBD\n");
    }

    OP(PS_NMADD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double a = PS0(RA);
            double b = PS0(RB);
            double c = PS0(RC);
            PS0(RD) = -((a * c) + b);
            a = PS1(RA);
            b = PS1(RB);
            c = PS1(RC);
            PS1(RD) = -((a * c) + b);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_NMADDD)
    {
        Halt("PS_NMADDD\n");
    }

    OP(PS_NMSUB)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double a = PS0(RA);
            double b = PS0(RB);
            double c = PS0(RC);
            PS0(RD) = -((a * c) - b);
            a = PS1(RA);
            b = PS1(RB);
            c = PS1(RC);
            PS1(RD) = -((a * c) - b);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_NMSUBD)
    {
        Halt("PS_NMSUBD\n");
    }

    OP(PS_MADDS0)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double s0 = (PS0(RA) * PS0(RC)) + PS0(RB);
            double s1 = (PS1(RA) * PS0(RC)) + PS1(RB);
            PS0(RD) = s0;
            PS1(RD) = s1;
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MADDS0D)
    {
        Halt("PS_MADDS0D\n");
    }

    OP(PS_MADDS1)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double s0 = (PS0(RA) * PS1(RC)) + PS0(RB);
            double s1 = (PS1(RA) * PS1(RC)) + PS1(RB);
            PS0(RD) = s0;
            PS1(RD) = s1;
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MADDS1D)
    {
        Halt("PS_MADDS1D\n");
    }

    OP(PS_CMPU0)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            int n = CRFD;
            double a = PS0(RA), b = PS0(RB);
            uint64_t da, db;
            uint32_t c;

            da = *(uint64_t*)&a;
            db = *(uint64_t*)&b;

            if (IS_NAN(da) || IS_NAN(db)) c = 1;
            else if (a < b) c = 8;
            else if (a > b) c = 4;
            else c = 2;

            SET_CRF(n, c);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_CMPU1)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            int n = CRFD;
            double a = PS1(RA), b = PS1(RB);
            uint64_t da, db;
            uint32_t c;

            da = *(uint64_t*)&a;
            db = *(uint64_t*)&b;

            if (IS_NAN(da) || IS_NAN(db)) c = 1;
            else if (a < b) c = 8;
            else if (a > b) c = 4;
            else c = 2;

            SET_CRF(n, c);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_CMPO0)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            int n = CRFD;
            double a = PS0(RA), b = PS0(RB);
            uint64_t da, db;
            uint32_t c;

            da = *(uint64_t*)&a;
            db = *(uint64_t*)&b;

            if (IS_NAN(da) || IS_NAN(db)) c = 1;
            else if (a < b) c = 8;
            else if (a > b) c = 4;
            else c = 2;

            SET_CRF(n, c);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_CMPO1)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            int n = CRFD;
            double a = PS1(RA), b = PS1(RB);
            uint64_t da, db;
            uint32_t c;

            da = *(uint64_t*)&a;
            db = *(uint64_t*)&b;

            if (IS_NAN(da) || IS_NAN(db)) c = 1;
            else if (a < b) c = 8;
            else if (a > b) c = 4;
            else c = 2;

            SET_CRF(n, c);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MR)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double p0 = PS0(RB), p1 = PS1(RB);
            PS0(RD) = p0, PS1(RD) = p1;
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MRD)
    {
        Halt("PS_MRD\n");
    }

    OP(PS_NEG)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            PS0(RD) = -PS0(RB);
            PS1(RD) = -PS1(RB);
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_NEGD)
    {
        Halt("PS_NEGD\n");
    }

    OP(PS_ABS)
    {
        Halt("PS_ABS\n");
    }

    OP(PS_ABSD)
    {
        Halt("PS_ABSD\n");
    }

    OP(PS_NABS)
    {
        Halt("PS_NABS\n");
    }

    OP(PS_NABSD)
    {
        Halt("PS_NABSD\n");
    }

    OP(PS_MERGE00)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double a = PS0(RA);
            double b = PS0(RB);
            PS0(RD) = a;
            PS1(RD) = b;
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MERGE00D)
    {
        Halt("PS_MERGE00D\n");
    }

    OP(PS_MERGE01)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double a = PS0(RA);
            double b = PS1(RB);
            PS0(RD) = a;
            PS1(RD) = b;
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MERGE01D)
    {
        Halt("PS_MERGE01D\n");
    }

    OP(PS_MERGE10)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double a = PS1(RA);
            double b = PS0(RB);
            PS0(RD) = a;
            PS1(RD) = b;
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MERGE10D)
    {
        Halt("PS_MERGE10D\n");
    }

    OP(PS_MERGE11)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double a = PS1(RA);
            double b = PS1(RB);
            PS0(RD) = a;
            PS1(RD) = b;
            Gekko->regs.pc += 4;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MERGE11D)
    {
        Halt("PS_MERGE11D\n");
    }

}
