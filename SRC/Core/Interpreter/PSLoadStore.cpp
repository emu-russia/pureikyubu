// Paired Single Load and Store Instructions
// used for fast type casting and matrix transfers.
#include "../pch.h"
#include "interpreter.h"

namespace Gekko
{
    #define GEKKO_PSW   (op & 0x8000)
    #define GEKKO_PSI   ((op >> 12) & 7)

    #define LD_SCALE(n) ((Gekko->regs.spr[(int)SPR::GQRs + n] >> 24) & 0x3f)
    #define LD_TYPE(n)  (GEKKO_QUANT_TYPE)((Gekko->regs.spr[(int)SPR::GQRs + n] >> 16) & 7)
    #define ST_SCALE(n) ((Gekko->regs.spr[(int)SPR::GQRs + n] >>  8) & 0x3f)
    #define ST_TYPE(n)  (GEKKO_QUANT_TYPE)((Gekko->regs.spr[(int)SPR::GQRs + n]      ) & 7)

    #define FPRU(n) (Gekko->regs.fpr[n].uval)
    #define FPRD(n) (Gekko->regs.fpr[n].dbl)
    #define PS0(n)  (Gekko->regs.fpr[n].dbl)
    #define PS1(n)  (Gekko->regs.ps1[n].dbl)

    // INT -> float (F = I * 2 ** -S)
    float Interpreter::dequantize(uint32_t data, GEKKO_QUANT_TYPE type, uint8_t scale)
    {
        float flt;

        switch (type)
        {
            case GEKKO_QUANT_TYPE::U8: flt = (float)(uint8_t)data; break;
            case GEKKO_QUANT_TYPE::U16: flt = (float)(uint16_t)data; break;
            case GEKKO_QUANT_TYPE::S8: 
                if (data & 0x80) data |= 0xffffff00;
                flt = (float)(int8_t)data; break;
            case GEKKO_QUANT_TYPE::S16: 
                if (data & 0x8000) data |= 0xffff0000;
                flt = (float)(int16_t)data; break;
            case GEKKO_QUANT_TYPE::SINGLE_FLOAT:
            default: flt = *((float*)&data); break;
        }

        return flt * Gekko->interp->ldScale[scale];
    }

    // float -> INT (I = ROUND(F * 2 ** S))
    uint32_t Interpreter::quantize(float data, GEKKO_QUANT_TYPE type, uint8_t scale)
    {
        uint32_t uval;

        data *= Gekko->interp->stScale[scale];

        switch (type)
        {
            case GEKKO_QUANT_TYPE::U8:
                if (data < 0) data = 0;
                if (data > 255) data = 255;
                uval = (uint8_t)(uint32_t)data; break;
            case GEKKO_QUANT_TYPE::U16:
                if (data < 0) data = 0;
                if (data > 65535) data = 65535;
                uval = (uint16_t)(uint32_t)data; break;
            case GEKKO_QUANT_TYPE::S8:
                if (data < -128) data = -128;
                if (data > 127) data = 127;
                uval = (int8_t)(uint8_t)(int32_t)(uint32_t)data; break;
            case GEKKO_QUANT_TYPE::S16:
                if (data < -32768) data = -32768;
                if (data > 32767) data = 32767;
                uval = (int16_t)(uint16_t)(int32_t)(uint32_t)data; break;
            case GEKKO_QUANT_TYPE::SINGLE_FLOAT:
            default: *((float*)&uval) = data; break;
        }

        return uval;
    }

    // ---------------------------------------------------------------------------
    // loads

    OP(PSQ_L)
    {
        if ((Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) == 0 ||
            (Gekko->regs.spr[(int)SPR::HID2] & HID2_LSQE) == 0)
        {
            Gekko->PrCause = PrivilegedCause::IllegalInstruction;
            Gekko->Exception(Exception::PROGRAM);
            return;
        }

        if (Gekko->regs.msr & MSR_FP)
        {
            uint32_t EA = op & 0xfff, data0, data1;
            int32_t d = RD;
            uint8_t scale = (uint8_t)LD_SCALE(GEKKO_PSI);
            GEKKO_QUANT_TYPE type = LD_TYPE(GEKKO_PSI);

            if (EA & 0x800) EA |= 0xfffff000;
            if (RA) EA += RRA;

            if (GEKKO_PSW)
            {
                if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) Gekko->ReadByte(EA, &data0);
                else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) Gekko->ReadHalf(EA, &data0);
                else Gekko->ReadWord(EA, &data0);

                if (Gekko->interp->exception) return;

                PS0(d) = (double)dequantize(data0, type, scale);
                PS1(d) = 1.0f;
            }
            else
            {
                if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) Gekko->ReadByte(EA, &data0);
                else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) Gekko->ReadHalf(EA, &data0);
                else Gekko->ReadWord(EA, &data0);

                if (Gekko->interp->exception) return;

                if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) Gekko->ReadByte(EA + 1, &data1);
                else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) Gekko->ReadHalf(EA + 2, &data1);
                else Gekko->ReadWord(EA + 4, &data1);

                if (Gekko->interp->exception) return;

                PS0(d) = (double)dequantize(data0, type, scale);
                PS1(d) = (double)dequantize(data1, type, scale);
            }
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PSQ_LU)
    {
        if ((Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) == 0 ||
            (Gekko->regs.spr[(int)SPR::HID2] & HID2_LSQE) == 0 || 
            RA == 0)
        {
            Gekko->PrCause = PrivilegedCause::IllegalInstruction;
            Gekko->Exception(Exception::PROGRAM);
            return;
        }

        if (Gekko->regs.msr & MSR_FP)
        {
            uint32_t EA = op & 0xfff, data0, data1;
            int32_t d = RD;
            uint8_t scale = (uint8_t)LD_SCALE(GEKKO_PSI);
            GEKKO_QUANT_TYPE type = LD_TYPE(GEKKO_PSI);

            if (EA & 0x800) EA |= 0xfffff000;
            EA += RRA;

            if (GEKKO_PSW)
            {
                if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) Gekko->ReadByte(EA, &data0);
                else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) Gekko->ReadHalf(EA, &data0);
                else Gekko->ReadWord(EA, &data0);

                if (Gekko->interp->exception) return;

                PS0(d) = (double)dequantize(data0, type, scale);
                PS1(d) = 1.0f;
            }
            else
            {
                if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) Gekko->ReadByte(EA, &data0);
                else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) Gekko->ReadHalf(EA, &data0);
                else Gekko->ReadWord(EA, &data0);

                if (Gekko->interp->exception) return;

                if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) Gekko->ReadByte(EA + 1, &data1);
                else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) Gekko->ReadHalf(EA + 2, &data1);
                else Gekko->ReadWord(EA + 4, &data1);

                if (Gekko->interp->exception) return;

                PS0(d) = (double)dequantize(data0, type, scale);
                PS1(d) = (double)dequantize(data1, type, scale);
            }

            RRA = EA;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PSQ_LUX)
    {
        if ((Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) == 0 ||
            (Gekko->regs.spr[(int)SPR::HID2] & HID2_LSQE) == 0 || 
            RA == 0)
        {
            Gekko->PrCause = PrivilegedCause::IllegalInstruction;
            Gekko->Exception(Exception::PROGRAM);
            return;
        }

        if (Gekko->regs.msr & MSR_FP)
        {
            int i = (op >> 7) & 7;
            uint32_t EA = RRB, data0, data1;
            int32_t d = RD;
            uint8_t scale = (uint8_t)LD_SCALE(i);
            GEKKO_QUANT_TYPE type = LD_TYPE(i);

            EA += RRA;

            if (op & 0x400 /* W */)
            {
                if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) Gekko->ReadByte(EA, &data0);
                else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) Gekko->ReadHalf(EA, &data0);
                else Gekko->ReadWord(EA, &data0);

                if (Gekko->interp->exception) return;

                PS0(d) = (double)dequantize(data0, type, scale);
                PS1(d) = 1.0f;
            }
            else
            {
                if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) Gekko->ReadByte(EA, &data0);
                else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) Gekko->ReadHalf(EA, &data0);
                else Gekko->ReadWord(EA, &data0);

                if (Gekko->interp->exception) return;

                if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) Gekko->ReadByte(EA + 1, &data1);
                else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) Gekko->ReadHalf(EA + 2, &data1);
                else Gekko->ReadWord(EA + 4, &data1);

                if (Gekko->interp->exception) return;

                PS0(d) = (double)dequantize(data0, type, scale);
                PS1(d) = (double)dequantize(data1, type, scale);
            }

            RRA = EA;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PSQ_LX)
    {
        if ((Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) == 0 ||
            (Gekko->regs.spr[(int)SPR::HID2] & HID2_LSQE) == 0)
        {
            Gekko->PrCause = PrivilegedCause::IllegalInstruction;
            Gekko->Exception(Exception::PROGRAM);
            return;
        }

        if (Gekko->regs.msr & MSR_FP)
        {
            int i = (op >> 7) & 7;
            uint32_t EA = RRB, data0, data1;
            int32_t d = RD;
            uint8_t scale = (uint8_t)LD_SCALE(i);
            GEKKO_QUANT_TYPE type = LD_TYPE(i);

            if (RA) EA += RRA;

            if (op & 0x400 /* W */)
            {
                if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) Gekko->ReadByte(EA, &data0);
                else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) Gekko->ReadHalf(EA, &data0);
                else Gekko->ReadWord(EA, &data0);

                if (Gekko->interp->exception) return;

                PS0(d) = (double)dequantize(data0, type, scale);
                PS1(d) = 1.0f;
            }
            else
            {
                if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) Gekko->ReadByte(EA, &data0);
                else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) Gekko->ReadHalf(EA, &data0);
                else Gekko->ReadWord(EA, &data0);

                if (Gekko->interp->exception) return;

                if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) Gekko->ReadByte(EA + 1, &data1);
                else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) Gekko->ReadHalf(EA + 2, &data1);
                else Gekko->ReadWord(EA + 4, &data1);

                if (Gekko->interp->exception) return;

                PS0(d) = (double)dequantize(data0, type, scale);
                PS1(d) = (double)dequantize(data1, type, scale);
            }
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    // ---------------------------------------------------------------------------
    // stores

    OP(PSQ_ST)
    {
        if ((Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) == 0 ||
            (Gekko->regs.spr[(int)SPR::HID2] & HID2_LSQE) == 0)
        {
            Gekko->PrCause = PrivilegedCause::IllegalInstruction;
            Gekko->Exception(Exception::PROGRAM);
            return;
        }

        if (Gekko->regs.msr & MSR_FP)
        {
            uint32_t EA = op & 0xfff;
            int32_t d = RS;
            uint8_t scale = (uint8_t)ST_SCALE(GEKKO_PSI);
            GEKKO_QUANT_TYPE type = ST_TYPE(GEKKO_PSI);

            if (EA & 0x800) EA |= 0xfffff000;
            if (RA) EA += RRA;

            if (GEKKO_PSW)
            {
                if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) Gekko->WriteByte(EA, quantize((float)PS0(d), type, scale));
                else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) Gekko->WriteHalf(EA, quantize((float)PS0(d), type, scale));
                else Gekko->WriteWord(EA, quantize((float)PS0(d), type, scale));
            }
            else
            {
                if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) Gekko->WriteByte(EA, quantize((float)PS0(d), type, scale));
                else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) Gekko->WriteHalf(EA, quantize((float)PS0(d), type, scale));
                else Gekko->WriteWord(EA, quantize((float)PS0(d), type, scale));

                if (Gekko->interp->exception) return;

                if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) Gekko->WriteByte(EA + 1, quantize((float)PS1(d), type, scale));
                else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) Gekko->WriteHalf(EA + 2, quantize((float)PS1(d), type, scale));
                else Gekko->WriteWord(EA + 4, quantize((float)PS1(d), type, scale));
            }
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PSQ_STU)
    {
        if ((Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) == 0 ||
            (Gekko->regs.spr[(int)SPR::HID2] & HID2_LSQE) == 0 ||
             RA == 0)
        {
            Gekko->PrCause = PrivilegedCause::IllegalInstruction;
            Gekko->Exception(Exception::PROGRAM);
            return;
        }

        if (Gekko->regs.msr & MSR_FP)
        {
            uint32_t EA = op & 0xfff;
            int32_t d = RS;
            uint8_t scale = (uint8_t)ST_SCALE(GEKKO_PSI);
            GEKKO_QUANT_TYPE type = ST_TYPE(GEKKO_PSI);

            if (EA & 0x800) EA |= 0xfffff000;
            EA += RRA;

            if (GEKKO_PSW)
            {
                if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) Gekko->WriteByte(EA, quantize((float)PS0(d), type, scale));
                else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) Gekko->WriteHalf(EA, quantize((float)PS0(d), type, scale));
                else Gekko->WriteWord(EA, quantize((float)PS0(d), type, scale));
            }
            else
            {
                if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) Gekko->WriteByte(EA, quantize((float)PS0(d), type, scale));
                else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) Gekko->WriteHalf(EA, quantize((float)PS0(d), type, scale));
                else Gekko->WriteWord(EA, quantize((float)PS0(d), type, scale));

                if (Gekko->interp->exception) return;

                if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) Gekko->WriteByte(EA + 1, quantize((float)PS1(d), type, scale));
                else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) Gekko->WriteHalf(EA + 2, quantize((float)PS1(d), type, scale));
                else Gekko->WriteWord(EA + 4, quantize((float)PS1(d), type, scale));
            }

            if (Gekko->interp->exception) return;

            RRA = EA;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PSQ_STUX)
    {
        if ((Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) == 0 ||
            (Gekko->regs.spr[(int)SPR::HID2] & HID2_LSQE) == 0 ||
            RA == 0)
        {
            Gekko->PrCause = PrivilegedCause::IllegalInstruction;
            Gekko->Exception(Exception::PROGRAM);
            return;
        }

        if (Gekko->regs.msr & MSR_FP)
        {
            int i = (op >> 7) & 7;
            uint32_t EA = RRB;
            int32_t d = RS;
            uint8_t scale = (uint8_t)ST_SCALE(i);
            GEKKO_QUANT_TYPE type = ST_TYPE(i);

            EA += RRA;

            if (op & 0x400)
            {
                if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) Gekko->WriteByte(EA, quantize((float)PS0(d), type, scale));
                else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) Gekko->WriteHalf(EA, quantize((float)PS0(d), type, scale));
                else Gekko->WriteWord(EA, quantize((float)PS0(d), type, scale));
            }
            else
            {
                if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) Gekko->WriteByte(EA, quantize((float)PS0(d), type, scale));
                else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) Gekko->WriteHalf(EA, quantize((float)PS0(d), type, scale));
                else Gekko->WriteWord(EA, quantize((float)PS0(d), type, scale));

                if (Gekko->interp->exception) return;

                if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) Gekko->WriteByte(EA + 1, quantize((float)PS1(d), type, scale));
                else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) Gekko->WriteHalf(EA + 2, quantize((float)PS1(d), type, scale));
                else Gekko->WriteWord(EA + 4, quantize((float)PS1(d), type, scale));
            }

            if (Gekko->interp->exception) return;

            RRA = EA;
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

    OP(PSQ_STX)
    {
        if ((Gekko->regs.spr[(int)SPR::HID2] & HID2_PSE) == 0 ||
            (Gekko->regs.spr[(int)SPR::HID2] & HID2_LSQE) == 0)
        {
            Gekko->PrCause = PrivilegedCause::IllegalInstruction;
            Gekko->Exception(Exception::PROGRAM);
            return;
        }

        if (Gekko->regs.msr & MSR_FP)
        {
            int i = (op >> 7) & 7;
            uint32_t EA = RRB;
            int32_t d = RS;
            uint8_t scale = (uint8_t)ST_SCALE(i);
            GEKKO_QUANT_TYPE type = ST_TYPE(i);

            if (RA) EA += RRA;

            if (op & 0x400)
            {
                if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) Gekko->WriteByte(EA, quantize((float)PS0(d), type, scale));
                else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) Gekko->WriteHalf(EA, quantize((float)PS0(d), type, scale));
                else Gekko->WriteWord(EA, quantize((float)PS0(d), type, scale));
            }
            else
            {
                if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) Gekko->WriteByte(EA, quantize((float)PS0(d), type, scale));
                else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) Gekko->WriteHalf(EA, quantize((float)PS0(d), type, scale));
                else Gekko->WriteWord(EA, quantize((float)PS0(d), type, scale));

                if (Gekko->interp->exception) return;

                if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) Gekko->WriteByte(EA + 1, quantize((float)PS1(d), type, scale));
                else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) Gekko->WriteHalf(EA + 2, quantize((float)PS1(d), type, scale));
                else Gekko->WriteWord(EA + 4, quantize((float)PS1(d), type, scale));
            }
        }
        else Gekko->Exception(Gekko::Exception::FPUNAVAIL);
    }

}
