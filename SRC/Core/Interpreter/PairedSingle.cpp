// Paired Single Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

namespace Gekko
{
    OP(PS_ADD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            PS0(RD) = PS0(RA) + PS0(RB);
            PS1(RD) = PS1(RA) + PS1(RB);
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_ADDD)
    {
        DBHalt("PS_ADDD\n");
    }

    OP(PS_SUB)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            PS0(RD) = PS0(RA) - PS0(RB);
            PS1(RD) = PS1(RA) - PS1(RB);
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_SUBD)
    {
        DBHalt("PS_SUBD\n");
    }

    OP(PS_MUL)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            PS0(RD) = PS0(RA) * PS0(RC);
            PS1(RD) = PS1(RA) * PS1(RC);
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MULD)
    {
        DBHalt("PS_MULD\n");
    }

    OP(PS_DIV)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            PS0(RD) = PS0(RA) / PS0(RB);
            PS1(RD) = PS1(RA) / PS1(RB);
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_DIVD)
    {
        DBHalt("PS_DIVD\n");
    }

    OP(PS_RES)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            PS0(RD) = 1.0f / PS0(RB);
            PS1(RD) = 1.0f / PS1(RB);
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_RESD)
    {
        DBHalt("PS_RESD\n");
    }

    OP(PS_RSQRTE)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            PS0(RD) = 1.0f / sqrt(PS0(RB));
            PS1(RD) = 1.0f / sqrt(PS1(RB));
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_RSQRTED)
    {
        DBHalt("PS_RSQRTED\n");
    }

    OP(PS_SEL)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            PS0(RD) = (PS0(RA) >= 0.0) ? (PS0(RC)) : (PS0(RB));
            PS1(RD) = (PS1(RA) >= 0.0) ? (PS1(RC)) : (PS1(RB));
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_SELD)
    {
        DBHalt("PS_SELD\n");
    }

    OP(PS_MULS0)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double m0 = PS0(RA) * PS0(RC);
            double m1 = PS1(RA) * PS0(RC);
            PS0(RD) = m0;
            PS1(RD) = m1;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MULS0D)
    {
        DBHalt("PS_MULS0D\n");
    }

    OP(PS_MULS1)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double m0 = PS0(RA) * PS1(RC);
            double m1 = PS1(RA) * PS1(RC);
            PS0(RD) = m0;
            PS1(RD) = m1;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MULS1D)
    {
        DBHalt("PS_MULS1D\n");
    }

    OP(PS_SUM0)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double s0 = PS0(RA) + PS1(RB);
            double s1 = PS1(RC);
            PS0(RD) = s0;
            PS1(RD) = s1;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_SUM0D)
    {
        DBHalt("PS_SUM0D\n");
    }

    OP(PS_SUM1)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double s0 = PS0(RC);
            double s1 = PS0(RA) + PS1(RB);
            PS0(RD) = s0;
            PS1(RD) = s1;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_SUM1D)
    {
        DBHalt("PS_SUM1D\n");
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
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MADDD)
    {
        DBHalt("PS_MADDD\n");
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
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MSUBD)
    {
        DBHalt("PS_MSUBD\n");
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
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_NMADDD)
    {
        DBHalt("PS_NMADDD\n");
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
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_NMSUBD)
    {
        DBHalt("PS_NMSUBD\n");
    }

    OP(PS_MADDS0)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double s0 = (PS0(RA) * PS0(RC)) + PS0(RB);
            double s1 = (PS1(RA) * PS0(RC)) + PS1(RB);
            PS0(RD) = s0;
            PS1(RD) = s1;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MADDS0D)
    {
        DBHalt("PS_MADDS0D\n");
    }

    OP(PS_MADDS1)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double s0 = (PS0(RA) * PS1(RC)) + PS0(RB);
            double s1 = (PS1(RA) * PS1(RC)) + PS1(RB);
            PS0(RD) = s0;
            PS1(RD) = s1;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MADDS1D)
    {
        DBHalt("PS_MADDS1D\n");
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
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MR)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double p0 = PS0(RB), p1 = PS1(RB);
            PS0(RD) = p0, PS1(RD) = p1;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MRD)
    {
        DBHalt("PS_MRD\n");
    }

    OP(PS_NEG)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            PS0(RD) = -PS0(RB);
            PS1(RD) = -PS1(RB);
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_NEGD)
    {
        DBHalt("PS_NEGD\n");
    }

    OP(PS_ABS)
    {
        DBHalt("PS_ABS\n");
    }

    OP(PS_ABSD)
    {
        DBHalt("PS_ABSD\n");
    }

    OP(PS_NABS)
    {
        DBHalt("PS_NABS\n");
    }

    OP(PS_NABSD)
    {
        DBHalt("PS_NABSD\n");
    }

    OP(PS_MERGE00)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double a = PS0(RA);
            double b = PS0(RB);
            PS0(RD) = a;
            PS1(RD) = b;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MERGE00D)
    {
        DBHalt("PS_MERGE00D\n");
    }

    OP(PS_MERGE01)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double a = PS0(RA);
            double b = PS1(RB);
            PS0(RD) = a;
            PS1(RD) = b;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MERGE01D)
    {
        DBHalt("PS_MERGE01D\n");
    }

    OP(PS_MERGE10)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double a = PS1(RA);
            double b = PS0(RB);
            PS0(RD) = a;
            PS1(RD) = b;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MERGE10D)
    {
        DBHalt("PS_MERGE10D\n");
    }

    OP(PS_MERGE11)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            double a = PS1(RA);
            double b = PS1(RB);
            PS0(RD) = a;
            PS1(RD) = b;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PS_MERGE11D)
    {
        DBHalt("PS_MERGE11D\n");
    }

}
