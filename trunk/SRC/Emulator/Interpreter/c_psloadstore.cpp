// Paired Single Load and Store Instructions
// used for fast type casting and matrix transfers.
#include "dolphin.h"

#define OP(name) void __fastcall c_##name##(u32 op)

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

// INT -> F32 (F = I * 2 ** -S)
static f32 dequantize(u32 data, s32 type, u8 scale)
{
    f32 flt;

    switch(type)
    {
        case 4: flt = (f32)(u8)data; break;     // U8
        case 5: flt = (f32)(u16)data; break;    // U16
        case 6: flt = (f32)(s8)data; break;     // S8
        case 7: flt = (f32)(s16)data; break;    // S16
        case 0: 
        default: flt = *((f32 *)&data); break;
    }

    return flt * cpu.ldScale[scale];
}

// F32 -> INT (I = ROUND(F * 2 ** S))
static u32 quantize(f32 data, s32 type, u8 scale)
{
    u32 uval;

    data *= cpu.stScale[scale];

    switch(type)
    {
        case 4:                                 // U8
            if(data < 0) data = 0;
            if(data > 255) data = 255;
            uval = (u8)(u32)data; break;
        case 5:                                 // U16
            if(data < 0) data = 0;
            if(data > 65535) data = 65535;
            uval = (u16)(u32)data; break;
        case 6:                                 // S8
            if(data < -128) data = -128;
            if(data > 127) data = 127;
            uval = (s8)(u8)(s32)(u32)data; break;
        case 7:                                 // S16
            if(data < -32768) data = -32768;
            if(data > 32767) data = 32767;
            uval = (s16)(u16)(s32)(u32)data; break;
        case 0:
        default: *((float *)&uval) = data; break;
    }

    return uval;
}

// ---------------------------------------------------------------------------
// loads

OP(PSQ_L)
{
    if(MSR & MSR_FP)
    {
        u32 EA = op & 0xfff, data0, data1;
        s32 d = RD, type = LD_TYPE(PSI);
        u8 scale = (u8)LD_SCALE(PSI);

        if(EA & 0x800) EA |= 0xfffff000;
        if(RA) EA += RRA;

        if(PSW)
        {
            if((type == 4) || (type == 6)) CPUReadByte(EA, &data0);
            else if((type == 5)|| (type == 7)) CPUReadHalf(EA, &data0);
            else CPUReadWord(EA, &data0);
            if(type == 6) if(data0 & 0x80) data0 |= 0xffffff00;
            if(type == 7) if(data0 & 0x8000) data0 |= 0xffff0000;

            PS0(d) = (f64)dequantize(data0, type, scale);
            PS1(d) = 1.0f;
        }
        else
        {
            if((type == 4) || (type == 6)) CPUReadByte(EA, &data0);
            else if((type == 5) || (type == 7)) CPUReadHalf(EA, &data0);
            else CPUReadWord(EA, &data0);
            if(type == 6) if(data0 & 0x80) data0 |= 0xffffff00;
            if(type == 7) if(data0 & 0x8000) data0 |= 0xffff0000;

            if((type == 4) || (type == 6)) CPUReadByte(EA + 1, &data1);
            else if((type == 5) || (type == 7)) CPUReadHalf(EA + 2, &data1);
            else CPUReadWord(EA + 4, &data1);
            if(type == 6) if(data0 & 0x80) data1 |= 0xffffff00;
            if(type == 7) if(data0 & 0x8000) data1 |= 0xffff0000;

            PS0(d) = (f64)dequantize(data0, type, scale);
            PS1(d) = (f64)dequantize(data1, type, scale);
        }
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PSQ_LX)
{
    if(MSR & MSR_FP)
    {
        int i = (op >> 7) & 7;
        u32 EA = RRB, data0, data1;
        s32 d = RD, type = LD_TYPE(i);
        u8 scale = (u8)LD_SCALE(i);

        if(RA) EA += RRA;

        if(op & 0x400 /* W */)
        {
            if((type == 4) || (type == 6)) CPUReadByte(EA, &data0);
            else if((type == 5)|| (type == 7)) CPUReadHalf(EA, &data0);
            else CPUReadWord(EA, &data0);
            if(type == 6) if(data0 & 0x80) data0 |= 0xffffff00;
            if(type == 7) if(data0 & 0x8000) data0 |= 0xffff0000;

            PS0(d) = (f64)dequantize(data0, type, scale);
            PS1(d) = 1.0f;
        }
        else
        {
            if((type == 4) || (type == 6)) CPUReadByte(EA, &data0);
            else if((type == 5) || (type == 7)) CPUReadHalf(EA, &data0);
            else CPUReadWord(EA, &data0);
            if(type == 6) if(data0 & 0x80) data0 |= 0xffffff00;
            if(type == 7) if(data0 & 0x8000) data0 |= 0xffff0000;

            if((type == 4) || (type == 6)) CPUReadByte(EA + 1, &data1);
            else if((type == 5) || (type == 7)) CPUReadHalf(EA + 2, &data1);
            else CPUReadWord(EA + 4, &data1);
            if(type == 6) if(data0 & 0x80) data1 |= 0xffffff00;
            if(type == 7) if(data0 & 0x8000) data1 |= 0xffff0000;

            PS0(d) = (f64)dequantize(data0, type, scale);
            PS1(d) = (f64)dequantize(data1, type, scale);
        }
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PSQ_LU)
{
    if(MSR & MSR_FP)
    {
        u32 EA = op & 0xfff, data0, data1;
        s32 d = RD, type = LD_TYPE(PSI);
        u8 scale = (u8)LD_SCALE(PSI);

        if(EA & 0x800) EA |= 0xfffff000;
        if(RA) EA += RRA;

        if(PSW)
        {
            if((type == 4) || (type == 6)) CPUReadByte(EA, &data0);
            else if((type == 5) || (type == 7)) CPUReadHalf(EA, &data0);
            else CPUReadWord(EA, &data0);
            if(type == 6) if(data0 & 0x80) data0 |= 0xffffff00;
            if(type == 7) if(data0 & 0x8000) data0 |= 0xffff0000;

            PS0(d) = (f64)dequantize(data0, type, scale);
            PS1(d) = 1.0f;
        }
        else
        {
            if((type == 4) || (type == 6)) CPUReadByte(EA, &data0);
            else if((type == 5) || (type == 7)) CPUReadHalf(EA, &data0);
            else CPUReadWord(EA, &data0);
            if(type == 6) if(data0 & 0x80) data0 |= 0xffffff00;
            if(type == 7) if(data0 & 0x8000) data0 |= 0xffff0000;

            if((type == 4) || (type == 6)) CPUReadByte(EA + 1, &data1);
            else if((type == 5) || (type == 7)) CPUReadHalf(EA + 2, &data1);
            else CPUReadWord(EA + 4, &data1);
            if(type == 6) if(data0 & 0x80) data1 |= 0xffffff00;
            if(type == 7) if(data0 & 0x8000) data1 |= 0xffff0000;

            PS0(d) = (f64)dequantize(data0, type, scale);
            PS1(d) = (f64)dequantize(data1, type, scale);
        }

        RRA = EA;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PSQ_LUX)
{
    if(MSR & MSR_FP)
    {
        int i = (op >> 7) & 7;
        u32 EA = RRB, data0, data1;
        s32 d = RD, type = LD_TYPE(i);
        u8 scale = (u8)LD_SCALE(i);

        if(RA) EA += RRA;

        if(op & 0x400 /* W */)
        {
            if((type == 4) || (type == 6)) CPUReadByte(EA, &data0);
            else if((type == 5)|| (type == 7)) CPUReadHalf(EA, &data0);
            else CPUReadWord(EA, &data0);
            if(type == 6) if(data0 & 0x80) data0 |= 0xffffff00;
            if(type == 7) if(data0 & 0x8000) data0 |= 0xffff0000;

            PS0(d) = (f64)dequantize(data0, type, scale);
            PS1(d) = 1.0f;
        }
        else
        {
            if((type == 4) || (type == 6)) CPUReadByte(EA, &data0);
            else if((type == 5) || (type == 7)) CPUReadHalf(EA, &data0);
            else CPUReadWord(EA, &data0);
            if(type == 6) if(data0 & 0x80) data0 |= 0xffffff00;
            if(type == 7) if(data0 & 0x8000) data0 |= 0xffff0000;

            if((type == 4) || (type == 6)) CPUReadByte(EA + 1, &data1);
            else if((type == 5) || (type == 7)) CPUReadHalf(EA + 2, &data1);
            else CPUReadWord(EA + 4, &data1);
            if(type == 6) if(data0 & 0x80) data1 |= 0xffffff00;
            if(type == 7) if(data0 & 0x8000) data1 |= 0xffff0000;

            PS0(d) = (f64)dequantize(data0, type, scale);
            PS1(d) = (f64)dequantize(data1, type, scale);
        }

        RRA = EA;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

// ---------------------------------------------------------------------------
// stores

OP(PSQ_ST)
{
    if(MSR & MSR_FP)
    {
        u32 EA = op & 0xfff;
        s32 d = RS, type = ST_TYPE(PSI);
        u8 scale = (u8)ST_SCALE(PSI);

        if(EA & 0x800) EA |= 0xfffff000;
        if(RA) EA += RRA;

        if(PSW)
        {
            if((type == 4) || (type == 6)) CPUWriteByte(EA, quantize((f32)PS0(d), type, scale));
            else if((type == 5) || (type == 7)) CPUWriteHalf(EA, quantize((f32)PS0(d), type, scale));
            else CPUWriteWord(EA, quantize((f32)PS0(d), type, scale));
        }
        else
        {
            if((type == 4) || (type == 6)) CPUWriteByte(EA, quantize((f32)PS0(d), type, scale));
            else if((type == 5) || (type == 7)) CPUWriteHalf(EA, quantize((f32)PS0(d), type, scale));
            else CPUWriteWord(EA, quantize((f32)PS0(d), type, scale));

            if((type == 4) || (type == 6)) CPUWriteByte(EA + 1, quantize((f32)PS1(d), type, scale));
            else if((type == 5) || (type == 7)) CPUWriteHalf(EA + 2, quantize((f32)PS1(d), type, scale));
            else CPUWriteWord(EA + 4, quantize((f32)PS1(d), type, scale));
        }
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PSQ_STX)
{
    if(MSR & MSR_FP)
    {
        int i = (op >> 7) & 7;
        u32 EA = RRB;
        s32 d = RS, type = ST_TYPE(PSI);
        u8 scale = (u8)ST_SCALE(PSI);

        if(RA) EA += RRA;

        if(op & 0x400)
        {
            if((type == 4) || (type == 6)) CPUWriteByte(EA, quantize((f32)PS0(d), type, scale));
            else if((type == 5) || (type == 7)) CPUWriteHalf(EA, quantize((f32)PS0(d), type, scale));
            else CPUWriteWord(EA, quantize((f32)PS0(d), type, scale));
        }
        else
        {
            if((type == 4) || (type == 6)) CPUWriteByte(EA, quantize((f32)PS0(d), type, scale));
            else if((type == 5) || (type == 7)) CPUWriteHalf(EA, quantize((f32)PS0(d), type, scale));
            else CPUWriteWord(EA, quantize((f32)PS0(d), type, scale));

            if((type == 4) || (type == 6)) CPUWriteByte(EA + 1, quantize((f32)PS1(d), type, scale));
            else if((type == 5) || (type == 7)) CPUWriteHalf(EA + 2, quantize((f32)PS1(d), type, scale));
            else CPUWriteWord(EA + 4, quantize((f32)PS1(d), type, scale));
        }
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PSQ_STU)
{
    if(MSR & MSR_FP)
    {
        u32 EA = op & 0xfff;
        s32 d = RS, type = ST_TYPE(PSI);
        u8 scale = (u8)ST_SCALE(PSI);

        if(EA & 0x800) EA |= 0xfffff000;
        if(RA) EA += RRA;

        if(PSW)
        {
            if((type == 4) || (type == 6)) CPUWriteByte(EA, quantize((f32)PS0(d), type, scale));
            else if((type == 5) || (type == 7)) CPUWriteHalf(EA, quantize((f32)PS0(d), type, scale));
            else CPUWriteWord(EA, quantize((f32)PS0(d), type, scale));
        }
        else
        {
            if((type == 4) || (type == 6)) CPUWriteByte(EA, quantize((f32)PS0(d), type, scale));
            else if((type == 5) || (type == 7)) CPUWriteHalf(EA, quantize((f32)PS0(d), type, scale));
            else CPUWriteWord(EA, quantize((f32)PS0(d), type, scale));

            if((type == 4) || (type == 6)) CPUWriteByte(EA + 1, quantize((f32)PS1(d), type, scale));
            else if((type == 5) || (type == 7)) CPUWriteHalf(EA + 2, quantize((f32)PS1(d), type, scale));
            else CPUWriteWord(EA + 4, quantize((f32)PS1(d), type, scale));
        }

        RRA = EA;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}

OP(PSQ_STUX)
{
    if(MSR & MSR_FP)
    {
        int i = (op >> 7) & 7;
        u32 EA = RRB;
        s32 d = RS, type = ST_TYPE(PSI);
        u8 scale = (u8)ST_SCALE(PSI);

        if(RA) EA += RRA;

        if(op & 0x400)
        {
            if((type == 4) || (type == 6)) CPUWriteByte(EA, quantize((f32)PS0(d), type, scale));
            else if((type == 5) || (type == 7)) CPUWriteHalf(EA, quantize((f32)PS0(d), type, scale));
            else CPUWriteWord(EA, quantize((f32)PS0(d), type, scale));
        }
        else
        {
            if((type == 4) || (type == 6)) CPUWriteByte(EA, quantize((f32)PS0(d), type, scale));
            else if((type == 5) || (type == 7)) CPUWriteHalf(EA, quantize((f32)PS0(d), type, scale));
            else CPUWriteWord(EA, quantize((f32)PS0(d), type, scale));

            if((type == 4) || (type == 6)) CPUWriteByte(EA + 1, quantize((f32)PS1(d), type, scale));
            else if((type == 5) || (type == 7)) CPUWriteHalf(EA + 2, quantize((f32)PS1(d), type, scale));
            else CPUWriteWord(EA + 4, quantize((f32)PS1(d), type, scale));
        }

        RRA = EA;
    }
    else CPUException(CPU_EXCEPTION_FPUNAVAIL);
}
