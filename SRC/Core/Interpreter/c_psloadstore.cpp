// Paired Single Load and Store Instructions
// used for fast type casting and matrix transfers.
#include "../pch.h"
#include "interpreter.h"

namespace Gekko
{
    #define GEKKO_PSW   (op & 0x8000)
    #define GEKKO_PSI   ((op >> 12) & 7)

    #define LD_SCALE(n) ((GQR[n] >> 24) & 0x3f)
    #define LD_TYPE(n)  ((GQR[n] >> 16) & 7)
    #define ST_SCALE(n) ((GQR[n] >>  8) & 0x3f)
    #define ST_TYPE(n)  ((GQR[n]      ) & 7)

    /*/ ---------------------------------------------------------------------------

       Quantization data types :
       -------------------------

       0 single-precision floating-point (no conversion)
       1-3 reserved
       4 unsigned 8 bit integer
       5 unsigned 16 bit integer
       6 signed 8 bit integer
       7 signed 16 bit integer

    --------------------------------------------------------------------------- /*/

    // INT -> float (F = I * 2 ** -S)
    static float dequantize(uint32_t data, int type, uint8_t scale)
    {
        float flt;

        switch (type)
        {
            case 4: flt = (float)(uint8_t)data; break;     // U8
            case 5: flt = (float)(uint16_t)data; break;    // U16
            case 6: flt = (float)(int8_t)data; break;     // S8
            case 7: flt = (float)(int16_t)data; break;    // S16
            case 0:
            default: flt = *((float*)&data); break;
        }

        return flt * cpu.ldScale[scale];
    }

    // float -> INT (I = ROUND(F * 2 ** S))
    static uint32_t quantize(float data, int type, uint8_t scale)
    {
        uint32_t uval;

        data *= cpu.stScale[scale];

        switch (type)
        {
        case 4:                                 // U8
            if (data < 0) data = 0;
            if (data > 255) data = 255;
            uval = (uint8_t)(uint32_t)data; break;
        case 5:                                 // U16
            if (data < 0) data = 0;
            if (data > 65535) data = 65535;
            uval = (uint16_t)(uint32_t)data; break;
        case 6:                                 // S8
            if (data < -128) data = -128;
            if (data > 127) data = 127;
            uval = (int8_t)(uint8_t)(int32_t)(uint32_t)data; break;
        case 7:                                 // S16
            if (data < -32768) data = -32768;
            if (data > 32767) data = 32767;
            uval = (int16_t)(uint16_t)(int32_t)(uint32_t)data; break;
        case 0:
        default: *((float*)&uval) = data; break;
        }

        return uval;
    }

    // ---------------------------------------------------------------------------
    // loads

    OP(PSQ_L)
    {
        if (MSR & MSR_FP)
        {
            uint32_t EA = op & 0xfff, data0, data1;
            int32_t d = RD, type = LD_TYPE(GEKKO_PSI);
            uint8_t scale = (uint8_t)LD_SCALE(GEKKO_PSI);

            if (EA & 0x800) EA |= 0xfffff000;
            if (RA) EA += RRA;

            if (GEKKO_PSW)
            {
                if ((type == 4) || (type == 6)) CPUReadByte(EA, &data0);
                else if ((type == 5) || (type == 7)) CPUReadHalf(EA, &data0);
                else CPUReadWord(EA, &data0);
                if (type == 6) if (data0 & 0x80) data0 |= 0xffffff00;
                if (type == 7) if (data0 & 0x8000) data0 |= 0xffff0000;

                PS0(d) = (double)dequantize(data0, type, scale);
                PS1(d) = 1.0f;
            }
            else
            {
                if ((type == 4) || (type == 6)) CPUReadByte(EA, &data0);
                else if ((type == 5) || (type == 7)) CPUReadHalf(EA, &data0);
                else CPUReadWord(EA, &data0);
                if (type == 6) if (data0 & 0x80) data0 |= 0xffffff00;
                if (type == 7) if (data0 & 0x8000) data0 |= 0xffff0000;

                if ((type == 4) || (type == 6)) CPUReadByte(EA + 1, &data1);
                else if ((type == 5) || (type == 7)) CPUReadHalf(EA + 2, &data1);
                else CPUReadWord(EA + 4, &data1);
                if (type == 6) if (data0 & 0x80) data1 |= 0xffffff00;
                if (type == 7) if (data0 & 0x8000) data1 |= 0xffff0000;

                PS0(d) = (double)dequantize(data0, type, scale);
                PS1(d) = (double)dequantize(data1, type, scale);
            }
        }
        else Gekko->Exception(CPU_EXCEPTION_FPUNAVAIL);
    }

    OP(PSQ_LX)
    {
        if (MSR & MSR_FP)
        {
            int i = (op >> 7) & 7;
            uint32_t EA = RRB, data0, data1;
            int32_t d = RD, type = LD_TYPE(i);
            uint8_t scale = (uint8_t)LD_SCALE(i);

            if (RA) EA += RRA;

            if (op & 0x400 /* W */)
            {
                if ((type == 4) || (type == 6)) CPUReadByte(EA, &data0);
                else if ((type == 5) || (type == 7)) CPUReadHalf(EA, &data0);
                else CPUReadWord(EA, &data0);
                if (type == 6) if (data0 & 0x80) data0 |= 0xffffff00;
                if (type == 7) if (data0 & 0x8000) data0 |= 0xffff0000;

                PS0(d) = (double)dequantize(data0, type, scale);
                PS1(d) = 1.0f;
            }
            else
            {
                if ((type == 4) || (type == 6)) CPUReadByte(EA, &data0);
                else if ((type == 5) || (type == 7)) CPUReadHalf(EA, &data0);
                else CPUReadWord(EA, &data0);
                if (type == 6) if (data0 & 0x80) data0 |= 0xffffff00;
                if (type == 7) if (data0 & 0x8000) data0 |= 0xffff0000;

                if ((type == 4) || (type == 6)) CPUReadByte(EA + 1, &data1);
                else if ((type == 5) || (type == 7)) CPUReadHalf(EA + 2, &data1);
                else CPUReadWord(EA + 4, &data1);
                if (type == 6) if (data0 & 0x80) data1 |= 0xffffff00;
                if (type == 7) if (data0 & 0x8000) data1 |= 0xffff0000;

                PS0(d) = (double)dequantize(data0, type, scale);
                PS1(d) = (double)dequantize(data1, type, scale);
            }
        }
        else Gekko->Exception(CPU_EXCEPTION_FPUNAVAIL);
    }

    OP(PSQ_LU)
    {
        if (MSR & MSR_FP)
        {
            uint32_t EA = op & 0xfff, data0, data1;
            int32_t d = RD, type = LD_TYPE(GEKKO_PSI);
            uint8_t scale = (uint8_t)LD_SCALE(GEKKO_PSI);

            if (EA & 0x800) EA |= 0xfffff000;
            if (RA) EA += RRA;

            if (GEKKO_PSW)
            {
                if ((type == 4) || (type == 6)) CPUReadByte(EA, &data0);
                else if ((type == 5) || (type == 7)) CPUReadHalf(EA, &data0);
                else CPUReadWord(EA, &data0);
                if (type == 6) if (data0 & 0x80) data0 |= 0xffffff00;
                if (type == 7) if (data0 & 0x8000) data0 |= 0xffff0000;

                PS0(d) = (double)dequantize(data0, type, scale);
                PS1(d) = 1.0f;
            }
            else
            {
                if ((type == 4) || (type == 6)) CPUReadByte(EA, &data0);
                else if ((type == 5) || (type == 7)) CPUReadHalf(EA, &data0);
                else CPUReadWord(EA, &data0);
                if (type == 6) if (data0 & 0x80) data0 |= 0xffffff00;
                if (type == 7) if (data0 & 0x8000) data0 |= 0xffff0000;

                if ((type == 4) || (type == 6)) CPUReadByte(EA + 1, &data1);
                else if ((type == 5) || (type == 7)) CPUReadHalf(EA + 2, &data1);
                else CPUReadWord(EA + 4, &data1);
                if (type == 6) if (data0 & 0x80) data1 |= 0xffffff00;
                if (type == 7) if (data0 & 0x8000) data1 |= 0xffff0000;

                PS0(d) = (double)dequantize(data0, type, scale);
                PS1(d) = (double)dequantize(data1, type, scale);
            }

            RRA = EA;
        }
        else Gekko->Exception(CPU_EXCEPTION_FPUNAVAIL);
    }

    OP(PSQ_LUX)
    {
        if (MSR & MSR_FP)
        {
            int i = (op >> 7) & 7;
            uint32_t EA = RRB, data0, data1;
            int32_t d = RD, type = LD_TYPE(i);
            uint8_t scale = (uint8_t)LD_SCALE(i);

            if (RA) EA += RRA;

            if (op & 0x400 /* W */)
            {
                if ((type == 4) || (type == 6)) CPUReadByte(EA, &data0);
                else if ((type == 5) || (type == 7)) CPUReadHalf(EA, &data0);
                else CPUReadWord(EA, &data0);
                if (type == 6) if (data0 & 0x80) data0 |= 0xffffff00;
                if (type == 7) if (data0 & 0x8000) data0 |= 0xffff0000;

                PS0(d) = (double)dequantize(data0, type, scale);
                PS1(d) = 1.0f;
            }
            else
            {
                if ((type == 4) || (type == 6)) CPUReadByte(EA, &data0);
                else if ((type == 5) || (type == 7)) CPUReadHalf(EA, &data0);
                else CPUReadWord(EA, &data0);
                if (type == 6) if (data0 & 0x80) data0 |= 0xffffff00;
                if (type == 7) if (data0 & 0x8000) data0 |= 0xffff0000;

                if ((type == 4) || (type == 6)) CPUReadByte(EA + 1, &data1);
                else if ((type == 5) || (type == 7)) CPUReadHalf(EA + 2, &data1);
                else CPUReadWord(EA + 4, &data1);
                if (type == 6) if (data0 & 0x80) data1 |= 0xffffff00;
                if (type == 7) if (data0 & 0x8000) data1 |= 0xffff0000;

                PS0(d) = (double)dequantize(data0, type, scale);
                PS1(d) = (double)dequantize(data1, type, scale);
            }

            RRA = EA;
        }
        else Gekko->Exception(CPU_EXCEPTION_FPUNAVAIL);
    }

    // ---------------------------------------------------------------------------
    // stores

    OP(PSQ_ST)
    {
        if (MSR & MSR_FP)
        {
            uint32_t EA = op & 0xfff;
            int32_t d = RS, type = ST_TYPE(GEKKO_PSI);
            uint8_t scale = (uint8_t)ST_SCALE(GEKKO_PSI);

            if (EA & 0x800) EA |= 0xfffff000;
            if (RA) EA += RRA;

            if (GEKKO_PSW)
            {
                if ((type == 4) || (type == 6)) CPUWriteByte(EA, quantize((float)PS0(d), type, scale));
                else if ((type == 5) || (type == 7)) CPUWriteHalf(EA, quantize((float)PS0(d), type, scale));
                else CPUWriteWord(EA, quantize((float)PS0(d), type, scale));
            }
            else
            {
                if ((type == 4) || (type == 6)) CPUWriteByte(EA, quantize((float)PS0(d), type, scale));
                else if ((type == 5) || (type == 7)) CPUWriteHalf(EA, quantize((float)PS0(d), type, scale));
                else CPUWriteWord(EA, quantize((float)PS0(d), type, scale));

                if ((type == 4) || (type == 6)) CPUWriteByte(EA + 1, quantize((float)PS1(d), type, scale));
                else if ((type == 5) || (type == 7)) CPUWriteHalf(EA + 2, quantize((float)PS1(d), type, scale));
                else CPUWriteWord(EA + 4, quantize((float)PS1(d), type, scale));
            }
        }
        else Gekko->Exception(CPU_EXCEPTION_FPUNAVAIL);
    }

    OP(PSQ_STX)
    {
        if (MSR & MSR_FP)
        {
            int i = (op >> 7) & 7;
            uint32_t EA = RRB;
            int32_t d = RS, type = ST_TYPE(GEKKO_PSI);
            uint8_t scale = (uint8_t)ST_SCALE(GEKKO_PSI);

            if (RA) EA += RRA;

            if (op & 0x400)
            {
                if ((type == 4) || (type == 6)) CPUWriteByte(EA, quantize((float)PS0(d), type, scale));
                else if ((type == 5) || (type == 7)) CPUWriteHalf(EA, quantize((float)PS0(d), type, scale));
                else CPUWriteWord(EA, quantize((float)PS0(d), type, scale));
            }
            else
            {
                if ((type == 4) || (type == 6)) CPUWriteByte(EA, quantize((float)PS0(d), type, scale));
                else if ((type == 5) || (type == 7)) CPUWriteHalf(EA, quantize((float)PS0(d), type, scale));
                else CPUWriteWord(EA, quantize((float)PS0(d), type, scale));

                if ((type == 4) || (type == 6)) CPUWriteByte(EA + 1, quantize((float)PS1(d), type, scale));
                else if ((type == 5) || (type == 7)) CPUWriteHalf(EA + 2, quantize((float)PS1(d), type, scale));
                else CPUWriteWord(EA + 4, quantize((float)PS1(d), type, scale));
            }
        }
        else Gekko->Exception(CPU_EXCEPTION_FPUNAVAIL);
    }

    OP(PSQ_STU)
    {
        if (MSR & MSR_FP)
        {
            uint32_t EA = op & 0xfff;
            int32_t d = RS, type = ST_TYPE(GEKKO_PSI);
            uint8_t scale = (uint8_t)ST_SCALE(GEKKO_PSI);

            if (EA & 0x800) EA |= 0xfffff000;
            if (RA) EA += RRA;

            if (GEKKO_PSW)
            {
                if ((type == 4) || (type == 6)) CPUWriteByte(EA, quantize((float)PS0(d), type, scale));
                else if ((type == 5) || (type == 7)) CPUWriteHalf(EA, quantize((float)PS0(d), type, scale));
                else CPUWriteWord(EA, quantize((float)PS0(d), type, scale));
            }
            else
            {
                if ((type == 4) || (type == 6)) CPUWriteByte(EA, quantize((float)PS0(d), type, scale));
                else if ((type == 5) || (type == 7)) CPUWriteHalf(EA, quantize((float)PS0(d), type, scale));
                else CPUWriteWord(EA, quantize((float)PS0(d), type, scale));

                if ((type == 4) || (type == 6)) CPUWriteByte(EA + 1, quantize((float)PS1(d), type, scale));
                else if ((type == 5) || (type == 7)) CPUWriteHalf(EA + 2, quantize((float)PS1(d), type, scale));
                else CPUWriteWord(EA + 4, quantize((float)PS1(d), type, scale));
            }

            RRA = EA;
        }
        else Gekko->Exception(CPU_EXCEPTION_FPUNAVAIL);
    }

    OP(PSQ_STUX)
    {
        if (MSR & MSR_FP)
        {
            int i = (op >> 7) & 7;
            uint32_t EA = RRB;
            int32_t d = RS, type = ST_TYPE(GEKKO_PSI);
            uint8_t scale = (uint8_t)ST_SCALE(GEKKO_PSI);

            if (RA) EA += RRA;

            if (op & 0x400)
            {
                if ((type == 4) || (type == 6)) CPUWriteByte(EA, quantize((float)PS0(d), type, scale));
                else if ((type == 5) || (type == 7)) CPUWriteHalf(EA, quantize((float)PS0(d), type, scale));
                else CPUWriteWord(EA, quantize((float)PS0(d), type, scale));
            }
            else
            {
                if ((type == 4) || (type == 6)) CPUWriteByte(EA, quantize((float)PS0(d), type, scale));
                else if ((type == 5) || (type == 7)) CPUWriteHalf(EA, quantize((float)PS0(d), type, scale));
                else CPUWriteWord(EA, quantize((float)PS0(d), type, scale));

                if ((type == 4) || (type == 6)) CPUWriteByte(EA + 1, quantize((float)PS1(d), type, scale));
                else if ((type == 5) || (type == 7)) CPUWriteHalf(EA + 2, quantize((float)PS1(d), type, scale));
                else CPUWriteWord(EA + 4, quantize((float)PS1(d), type, scale));
            }

            RRA = EA;
        }
        else Gekko->Exception(CPU_EXCEPTION_FPUNAVAIL);
    }

}
