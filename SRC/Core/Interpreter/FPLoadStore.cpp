// Floating-Point Load and Store Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

namespace Gekko
{
    // ---------------------------------------------------------------------------
    // loads

    // ea = (ra | 0) + SIMM
    // if HID2[PSE] = 0
    //      then fd = DOUBLE(MEM(ea, 4))
    //      else fd(ps0) = SINGLE(MEM(ea, 4))
    //           fd(ps1) = SINGLE(MEM(ea, 4))
    OP(LFS)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            float res;

            if (RA) Gekko->ReadWord(RRA + SIMM, (uint32_t*)&res);
            else Gekko->ReadWord(SIMM, (uint32_t*)&res);

            if (Gekko->interp->exception) return;

            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) PS0(RD) = PS1(RD) = (double)res;
            else FPRD(RD) = (double)res;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    // ea = (ra | 0) + rb
    // if HID2[PSE] = 0
    //      then fd = DOUBLE(MEM(ea, 4))
    //      else fd(ps0) = SINGLE(MEM(ea, 4))
    //           fd(ps1) = SINGLE(MEM(ea, 4))
    OP(LFSX)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            float res;

            if (RA) Gekko->ReadWord(RRA + RRB, (uint32_t*)&res);
            else Gekko->ReadWord(RRB, (uint32_t*)&res);

            if (Gekko->interp->exception) return;

            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) PS0(RD) = PS1(RD) = (double)res;
            else FPRD(RD) = (double)res;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    // ea = ra + SIMM
    // if HID2[PSE] = 0
    //      then fd = DOUBLE(MEM(ea, 4))
    //      else fd(ps0) = SINGLE(MEM(ea, 4))
    //           fd(ps1) = SINGLE(MEM(ea, 4))
    // ra = ea
    OP(LFSU)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            uint32_t ea = RRA + SIMM;
            float res;

            Gekko->ReadWord(ea, (uint32_t*)&res);

            if (Gekko->interp->exception) return;

            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) PS0(RD) = PS1(RD) = (double)res;
            else FPRD(RD) = (double)res;

            RRA = ea;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    // ea = ra + rb
    // if HID2[PSE] = 0
    //      then fd = DOUBLE(MEM(ea, 4))
    //      else fd(ps0) = SINGLE(MEM(ea, 4))
    //           fd(ps1) = SINGLE(MEM(ea, 4))
    // ra = ea
    OP(LFSUX)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            uint32_t ea = RRA + RRB;
            float res;

            Gekko->ReadWord(ea, (uint32_t*)&res);

            if (Gekko->interp->exception) return;

            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) PS0(RD) = PS1(RD) = (double)res;
            else FPRD(RD) = (double)res;

            RRA = ea;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    // ea = (ra | 0) + SIMM
    // fd = MEM(ea, 8)
    OP(LFD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            if (RA) Gekko->ReadDouble(RRA + SIMM, &FPRU(RD));
            else Gekko->ReadDouble(SIMM, &FPRU(RD));
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    // ea = (ra | 0) + rb
    // fd = MEM(ea, 8)
    OP(LFDX)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            if (RA) Gekko->ReadDouble(RRA + RRB, &FPRU(RD));
            else Gekko->ReadDouble(RRB, &FPRU(RD));
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    // ea = ra + SIMM
    // fd = MEM(ea, 8)
    // ra = ea
    OP(LFDU)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            uint32_t ea = RRA + SIMM;
            Gekko->ReadDouble(ea, &FPRU(RD));
            if (Gekko->interp->exception) return;
            RRA = ea;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    // ea = ra + rb
    // fd = MEM(ea, 8)
    // ra = ea
    OP(LFDUX)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            uint32_t ea = RRA + RRB;
            Gekko->ReadDouble(ea, &FPRU(RD));
            if (Gekko->interp->exception) return;
            RRA = ea;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    // ---------------------------------------------------------------------------
    // stores

    // ea = (ra | 0) + SIMM
    // MEM(ea, 4) = SINGLE(fs)
    OP(STFS)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            float data;

            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) data = (float)PS0(RS);
            else data = (float)FPRD(RS);

            if (RA) Gekko->WriteWord(RRA + SIMM, *(uint32_t*)&data);
            else Gekko->WriteWord(SIMM, *(uint32_t*)&data);
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    // ea = (ra | 0) + rb
    // MEM(ea, 4) = SINGLE(fs)
    OP(STFSX)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            float num;
            uint32_t* data = (uint32_t*)&num;

            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) num = (float)PS0(RS);
            else num = (float)FPRD(RS);

            if (RA) Gekko->WriteWord(RRA + RRB, *data);
            else Gekko->WriteWord(RRB, *data);
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    // ea = ra + SIMM
    // MEM(ea, 4) = SINGLE(fs)
    // ra = ea
    OP(STFSU)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            float data;
            uint32_t ea = RRA + SIMM;

            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) data = (float)PS0(RS);
            else data = (float)FPRD(RS);

            Gekko->WriteWord(ea, *(uint32_t*)&data);
            if (Gekko->interp->exception) return;
            RRA = ea;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    // ea = ra + rb
    // MEM(ea, 4) = SINGLE(fs)
    // ra = ea
    OP(STFSUX)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            float num;
            uint32_t* data = (uint32_t*)&num;
            uint32_t ea = RRA + RRB;

            if (Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) num = (float)PS0(RS);
            else num = (float)FPRD(RS);

            Gekko->WriteWord(ea, *data);
            if (Gekko->interp->exception) return;
            RRA = ea;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    // ea = (ra | 0) + SIMM
    // MEM(ea, 8) = fs
    OP(STFD)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            if (RA) Gekko->WriteDouble(RRA + SIMM, &FPRU(RS));
            else Gekko->WriteDouble(SIMM, &FPRU(RS));
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    // ea = (ra | 0) + rb
    // MEM(ea, 8) = fs
    OP(STFDX)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            if (RA) Gekko->WriteDouble(RRA + RRB, &FPRU(RS));
            else Gekko->WriteDouble(RRB, &FPRU(RS));
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    // ea = ra + SIMM
    // MEM(ea, 8) = fs
    // ra = ea
    OP(STFDU)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            uint32_t ea = RRA + SIMM;
            Gekko->WriteDouble(ea, &FPRU(RS));
            if (Gekko->interp->exception) return;
            RRA = ea;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    // ea = ra + rb
    // MEM(ea, 8) = fs
    // ra = ea
    OP(STFDUX)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            uint32_t ea = RRA + RRB;
            Gekko->WriteDouble(ea, &FPRU(RS));
            if (Gekko->interp->exception) return;
            RRA = ea;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    // ---------------------------------------------------------------------------
    // special

    // ea = (ra | 0) + rb
    // MEM(ea, 4) = fs[32-63]
    OP(STFIWX)
    {
        if (Gekko->regs.msr & MSR_FP)
        {
            uint32_t val = (uint32_t)(FPRU(RS) & 0x00000000ffffffff);
            if (RA) Gekko->WriteWord(RRA + RRB, val);
            else Gekko->WriteWord(RRB, val);
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

}
