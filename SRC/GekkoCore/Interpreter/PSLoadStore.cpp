// Paired Single Load and Store Instructions
// used for fast type casting and matrix transfers.
#include "../pch.h"
#include "InterpreterPrivate.h"

namespace Gekko
{
	#define LD_SCALE(n) ((core->regs.spr[SPR::GQRs + n] >> 24) & 0x3f)
	#define LD_TYPE(n)  (GEKKO_QUANT_TYPE)((core->regs.spr[SPR::GQRs + n] >> 16) & 7)
	#define ST_SCALE(n) ((core->regs.spr[SPR::GQRs + n] >>  8) & 0x3f)
	#define ST_TYPE(n)  (GEKKO_QUANT_TYPE)((core->regs.spr[SPR::GQRs + n]      ) & 7)

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

		return flt * core->interp->ldScale[scale];
	}

	// float -> INT (I = ROUND(F * 2 ** S))
	uint32_t Interpreter::quantize(float data, GEKKO_QUANT_TYPE type, uint8_t scale)
	{
		uint32_t uval;

		data *= core->interp->stScale[scale];

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

	void Interpreter::psq_lx(DecoderInfo& info)
	{
		if ((core->regs.spr[SPR::HID2] & HID2_PSE) == 0 ||
			(core->regs.spr[SPR::HID2] & HID2_LSQE) == 0)
		{
			core->PrCause = PrivilegedCause::IllegalInstruction;
			core->Exception(Exception::PROGRAM);
			return;
		}

		if (core->regs.msr & MSR_FP)
		{
			int i = info.paramBits[4];
			uint32_t EA = core->regs.gpr[info.paramBits[2]], data0, data1;
			int32_t d = info.paramBits[0];
			uint8_t scale = (uint8_t)LD_SCALE(i);
			GEKKO_QUANT_TYPE type = LD_TYPE(i);

			if (info.paramBits[1]) EA += core->regs.gpr[info.paramBits[1]];

			if (info.paramBits[3] /* W */)
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->ReadByte(EA, &data0);
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->ReadHalf(EA, &data0);
				else core->ReadWord(EA, &data0);

				if (core->exception) return;

				PS0(d) = (double)dequantize(data0, type, scale);
				PS1(d) = 1.0f;
			}
			else
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->ReadByte(EA, &data0);
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->ReadHalf(EA, &data0);
				else core->ReadWord(EA, &data0);

				if (core->exception) return;

				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->ReadByte(EA + 1, &data1);
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->ReadHalf(EA + 2, &data1);
				else core->ReadWord(EA + 4, &data1);

				if (core->exception) return;

				PS0(d) = (double)dequantize(data0, type, scale);
				PS1(d) = (double)dequantize(data1, type, scale);
			}

			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::psq_stx(DecoderInfo& info)
	{
		if ((core->regs.spr[SPR::HID2] & HID2_PSE) == 0 ||
			(core->regs.spr[SPR::HID2] & HID2_LSQE) == 0)
		{
			core->PrCause = PrivilegedCause::IllegalInstruction;
			core->Exception(Exception::PROGRAM);
			return;
		}

		if (core->regs.msr & MSR_FP)
		{
			int i = info.paramBits[4];
			uint32_t EA = core->regs.gpr[info.paramBits[2]];
			int32_t d = info.paramBits[0];
			uint8_t scale = (uint8_t)ST_SCALE(i);
			GEKKO_QUANT_TYPE type = ST_TYPE(i);

			if (info.paramBits[1]) EA += core->regs.gpr[info.paramBits[1]];

			if (info.paramBits[3] /* W */)
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->WriteByte(EA, quantize((float)PS0(d), type, scale));
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->WriteHalf(EA, quantize((float)PS0(d), type, scale));
				else core->WriteWord(EA, quantize((float)PS0(d), type, scale));
			}
			else
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->WriteByte(EA, quantize((float)PS0(d), type, scale));
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->WriteHalf(EA, quantize((float)PS0(d), type, scale));
				else core->WriteWord(EA, quantize((float)PS0(d), type, scale));

				if (core->exception) return;

				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->WriteByte(EA + 1, quantize((float)PS1(d), type, scale));
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->WriteHalf(EA + 2, quantize((float)PS1(d), type, scale));
				else core->WriteWord(EA + 4, quantize((float)PS1(d), type, scale));
			}

			if (core->exception) return;

			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::psq_lux(DecoderInfo& info)
	{
		if ((core->regs.spr[SPR::HID2] & HID2_PSE) == 0 ||
			(core->regs.spr[SPR::HID2] & HID2_LSQE) == 0 ||
			info.paramBits[1] == 0)
		{
			core->PrCause = PrivilegedCause::IllegalInstruction;
			core->Exception(Exception::PROGRAM);
			return;
		}

		if (core->regs.msr & MSR_FP)
		{
			int i = info.paramBits[4];
			uint32_t EA = core->regs.gpr[info.paramBits[2]], data0, data1;
			int32_t d = info.paramBits[0];
			uint8_t scale = (uint8_t)LD_SCALE(i);
			GEKKO_QUANT_TYPE type = LD_TYPE(i);

			EA += core->regs.gpr[info.paramBits[1]];

			if (info.paramBits[3] /* W */)
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->ReadByte(EA, &data0);
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->ReadHalf(EA, &data0);
				else core->ReadWord(EA, &data0);

				if (core->exception) return;

				PS0(d) = (double)dequantize(data0, type, scale);
				PS1(d) = 1.0f;
			}
			else
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->ReadByte(EA, &data0);
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->ReadHalf(EA, &data0);
				else core->ReadWord(EA, &data0);

				if (core->exception) return;

				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->ReadByte(EA + 1, &data1);
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->ReadHalf(EA + 2, &data1);
				else core->ReadWord(EA + 4, &data1);

				if (core->exception) return;

				PS0(d) = (double)dequantize(data0, type, scale);
				PS1(d) = (double)dequantize(data1, type, scale);
			}

			core->regs.gpr[info.paramBits[1]] = EA;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::psq_stux(DecoderInfo& info)
	{
		if ((core->regs.spr[SPR::HID2] & HID2_PSE) == 0 ||
			(core->regs.spr[SPR::HID2] & HID2_LSQE) == 0 ||
			info.paramBits[1] == 0)
		{
			core->PrCause = PrivilegedCause::IllegalInstruction;
			core->Exception(Exception::PROGRAM);
			return;
		}

		if (core->regs.msr & MSR_FP)
		{
			int i = info.paramBits[4];
			uint32_t EA = core->regs.gpr[info.paramBits[2]];
			int32_t d = info.paramBits[0];
			uint8_t scale = (uint8_t)ST_SCALE(i);
			GEKKO_QUANT_TYPE type = ST_TYPE(i);

			EA += core->regs.gpr[info.paramBits[1]];

			if (info.paramBits[3] /* W */)
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->WriteByte(EA, quantize((float)PS0(d), type, scale));
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->WriteHalf(EA, quantize((float)PS0(d), type, scale));
				else core->WriteWord(EA, quantize((float)PS0(d), type, scale));
			}
			else
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->WriteByte(EA, quantize((float)PS0(d), type, scale));
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->WriteHalf(EA, quantize((float)PS0(d), type, scale));
				else core->WriteWord(EA, quantize((float)PS0(d), type, scale));

				if (core->exception) return;

				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->WriteByte(EA + 1, quantize((float)PS1(d), type, scale));
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->WriteHalf(EA + 2, quantize((float)PS1(d), type, scale));
				else core->WriteWord(EA + 4, quantize((float)PS1(d), type, scale));
			}

			if (core->exception) return;

			core->regs.gpr[info.paramBits[1]] = EA;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::psq_l(DecoderInfo& info)
	{
		if ((core->regs.spr[SPR::HID2] & HID2_PSE) == 0 ||
			(core->regs.spr[SPR::HID2] & HID2_LSQE) == 0)
		{
			core->PrCause = PrivilegedCause::IllegalInstruction;
			core->Exception(Exception::PROGRAM);
			return;
		}

		if (core->regs.msr & MSR_FP)
		{
			uint32_t EA = info.Imm.Signed & 0xfff, data0, data1;
			int32_t d = info.paramBits[0];
			uint8_t scale = (uint8_t)LD_SCALE(info.paramBits[3]);
			GEKKO_QUANT_TYPE type = LD_TYPE(info.paramBits[3]);

			if (EA & 0x800) EA |= 0xfffff000;
			if (info.paramBits[1]) EA += core->regs.gpr[info.paramBits[1]];

			if (info.paramBits[2])
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->ReadByte(EA, &data0);
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->ReadHalf(EA, &data0);
				else core->ReadWord(EA, &data0);

				if (core->exception) return;

				PS0(d) = (double)dequantize(data0, type, scale);
				PS1(d) = 1.0f;
			}
			else
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->ReadByte(EA, &data0);
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->ReadHalf(EA, &data0);
				else core->ReadWord(EA, &data0);

				if (core->exception) return;

				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->ReadByte(EA + 1, &data1);
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->ReadHalf(EA + 2, &data1);
				else core->ReadWord(EA + 4, &data1);

				if (core->exception) return;

				PS0(d) = (double)dequantize(data0, type, scale);
				PS1(d) = (double)dequantize(data1, type, scale);
			}

			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::psq_lu(DecoderInfo& info)
	{
		if ((core->regs.spr[SPR::HID2] & HID2_PSE) == 0 ||
			(core->regs.spr[SPR::HID2] & HID2_LSQE) == 0 || 
			info.paramBits[1] == 0)
		{
			core->PrCause = PrivilegedCause::IllegalInstruction;
			core->Exception(Exception::PROGRAM);
			return;
		}

		if (core->regs.msr & MSR_FP)
		{
			uint32_t EA = info.Imm.Signed & 0xfff, data0, data1;
			int32_t d = info.paramBits[0];
			uint8_t scale = (uint8_t)LD_SCALE(info.paramBits[3]);
			GEKKO_QUANT_TYPE type = LD_TYPE(info.paramBits[3]);

			if (EA & 0x800) EA |= 0xfffff000;
			EA += core->regs.gpr[info.paramBits[1]];

			if (info.paramBits[2])
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->ReadByte(EA, &data0);
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->ReadHalf(EA, &data0);
				else core->ReadWord(EA, &data0);

				if (core->exception) return;

				PS0(d) = (double)dequantize(data0, type, scale);
				PS1(d) = 1.0f;
			}
			else
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->ReadByte(EA, &data0);
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->ReadHalf(EA, &data0);
				else core->ReadWord(EA, &data0);

				if (core->exception) return;

				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->ReadByte(EA + 1, &data1);
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->ReadHalf(EA + 2, &data1);
				else core->ReadWord(EA + 4, &data1);

				if (core->exception) return;

				PS0(d) = (double)dequantize(data0, type, scale);
				PS1(d) = (double)dequantize(data1, type, scale);
			}

			core->regs.gpr[info.paramBits[1]] = EA;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::psq_st(DecoderInfo& info)
	{
		if ((core->regs.spr[SPR::HID2] & HID2_PSE) == 0 ||
			(core->regs.spr[SPR::HID2] & HID2_LSQE) == 0)
		{
			core->PrCause = PrivilegedCause::IllegalInstruction;
			core->Exception(Exception::PROGRAM);
			return;
		}

		if (core->regs.msr & MSR_FP)
		{
			uint32_t EA = info.Imm.Signed & 0xfff;
			int32_t d = info.paramBits[0];
			uint8_t scale = (uint8_t)ST_SCALE(info.paramBits[3]);
			GEKKO_QUANT_TYPE type = ST_TYPE(info.paramBits[3]);

			if (EA & 0x800) EA |= 0xfffff000;
			if (info.paramBits[1]) EA += core->regs.gpr[info.paramBits[1]];

			if (info.paramBits[2])
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->WriteByte(EA, quantize((float)PS0(d), type, scale));
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->WriteHalf(EA, quantize((float)PS0(d), type, scale));
				else core->WriteWord(EA, quantize((float)PS0(d), type, scale));
			}
			else
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->WriteByte(EA, quantize((float)PS0(d), type, scale));
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->WriteHalf(EA, quantize((float)PS0(d), type, scale));
				else core->WriteWord(EA, quantize((float)PS0(d), type, scale));

				if (core->exception) return;

				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->WriteByte(EA + 1, quantize((float)PS1(d), type, scale));
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->WriteHalf(EA + 2, quantize((float)PS1(d), type, scale));
				else core->WriteWord(EA + 4, quantize((float)PS1(d), type, scale));
			}

			if (core->exception) return;

			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

	void Interpreter::psq_stu(DecoderInfo& info)
	{
		if ((core->regs.spr[SPR::HID2] & HID2_PSE) == 0 ||
			(core->regs.spr[SPR::HID2] & HID2_LSQE) == 0 ||
			info.paramBits[1] == 0)
		{
			core->PrCause = PrivilegedCause::IllegalInstruction;
			core->Exception(Exception::PROGRAM);
			return;
		}

		if (core->regs.msr & MSR_FP)
		{
			uint32_t EA = info.Imm.Signed & 0xfff;
			int32_t d = info.paramBits[0];
			uint8_t scale = (uint8_t)ST_SCALE(info.paramBits[3]);
			GEKKO_QUANT_TYPE type = ST_TYPE(info.paramBits[3]);

			if (EA & 0x800) EA |= 0xfffff000;
			EA += core->regs.gpr[info.paramBits[1]];

			if (info.paramBits[2])
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->WriteByte(EA, quantize((float)PS0(d), type, scale));
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->WriteHalf(EA, quantize((float)PS0(d), type, scale));
				else core->WriteWord(EA, quantize((float)PS0(d), type, scale));
			}
			else
			{
				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->WriteByte(EA, quantize((float)PS0(d), type, scale));
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->WriteHalf(EA, quantize((float)PS0(d), type, scale));
				else core->WriteWord(EA, quantize((float)PS0(d), type, scale));

				if (core->exception) return;

				if ((type == GEKKO_QUANT_TYPE::U8) || (type == GEKKO_QUANT_TYPE::S8)) core->WriteByte(EA + 1, quantize((float)PS1(d), type, scale));
				else if ((type == GEKKO_QUANT_TYPE::U16) || (type == GEKKO_QUANT_TYPE::S16)) core->WriteHalf(EA + 2, quantize((float)PS1(d), type, scale));
				else core->WriteWord(EA + 4, quantize((float)PS1(d), type, scale));
			}

			if (core->exception) return;

			core->regs.gpr[info.paramBits[1]] = EA;
			core->regs.pc += 4;
		}
		else core->Exception(Gekko::Exception::FPUNAVAIL);
	}

}
