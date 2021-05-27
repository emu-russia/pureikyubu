// x87 (FPU) instructions assembler.

#include "pch.h"

namespace IntelCore
{

	bool IntelAssembler::IsFpuReg(Param p)
	{
		return Param::X87Start < p && p < Param::X87End;
	}

	void IntelAssembler::GetFpuReg(Param p, size_t& reg)
	{
		switch (p)
		{
			case Param::st0: reg = 0; break;
			case Param::st1: reg = 1; break;
			case Param::st2: reg = 2; break;
			case Param::st3: reg = 3; break;
			case Param::st4: reg = 4; break;
			case Param::st5: reg = 5; break;
			case Param::st6: reg = 6; break;
			case Param::st7: reg = 7; break;

			default:
				throw "Invalid parameter";
		}
	}

	bool IntelAssembler::IsFpuInstr(Instruction instr)
	{
		return Instruction::FpuInstrStart < instr && instr < Instruction::FpuInstrEnd;
	}

	/// <summary>
	/// A special version of the ModRM handler for FPU instructions.
	/// </summary>
	void IntelAssembler::HandleModRmFpu(AnalyzeInfo& info, size_t bits, uint8_t opcode, uint8_t opcodeReg, uint8_t extendedOpcode)
	{
		size_t mod = 0, reg = 0, rm = 0;
		size_t scale = 0, index = 0, base = 0;

		// Extract and check required information from parameters 

		if (IsReg(info.params[0]))
		{
			throw "Invalid parameter";
		}

		if (IsMem64(info.params[0]) && bits != 64)
		{
			throw "Invalid parameter";
		}

		reg = opcodeReg;
		GetMod(info.params[0], mod);
		GetRm(info.params[0], rm);

		bool sibRequired = IsSib(info.params[0]);

		if (sibRequired)
		{
			GetSS(info.params[0], scale);
			GetIndex(info.params[0], index);
			GetBase(info.params[0], base);
		}

		// Compile the resulting instruction, mode prefixes and possible displacement

		switch (bits)
		{
			case 16:
				if (IsMem32(info.params[0]) || info.ptrHint == PtrHint::M28Ptr || info.ptrHint == PtrHint::M108Ptr)
				{
					AddPrefixByte(info, 0x67);
				}
				else if (IsMem64(info.params[0]))
				{
					Invalid();
				}
				break;

			case 32:
				if (IsMem16(info.params[0]) || info.ptrHint == PtrHint::M14Ptr || info.ptrHint == PtrHint::M94Ptr)
				{
					AddPrefixByte(info, 0x67);
				}
				else if (IsMem64(info.params[0]))
				{
					Invalid();
				}
				break;

			case 64:
				if (IsMem32(info.params[0]))
				{
					AddPrefixByte(info, 0x67);
				}
				else if (info.params[0] == Param::m_eip_disp32)
				{
					AddPrefixByte(info, 0x67);
				}
				else if (IsMem16(info.params[0]))
				{
					Invalid();
				}
				break;
		}

		bool rexRequired = rm >= 8 || index >= 8 || base >= 8;

		bool fxSaveRstor64 = info.instr == Instruction::fxsave64 || info.instr == Instruction::fxrstor64;

		if (rexRequired && (bits != 64 || !fxSaveRstor64))
		{
			Invalid();
		}

		if (rexRequired)
		{
			int REX_W = 1;
			int REX_R = 0;
			int REX_X = sibRequired ? ((index >= 8) ? 1 : 0) : 0;
			int REX_B = sibRequired ? ((base >= 8) ? 1 : 0) : ((rm >= 8) ? 1 : 0);
			OneByte(info, 0x40 | (REX_W << 3) | (REX_R << 2) | (REX_X << 1) | REX_B);
		}

		if (extendedOpcode)
		{
			OneByte(info, extendedOpcode);
		}

		OneByte(info, opcode);

		uint8_t modRmByte = ((mod & 3) << 6) | ((reg & 7) << 3) | (rm & 7);
		OneByte(info, modRmByte);

		if (sibRequired)
		{
			uint8_t sibByte = ((scale & 3) << 6) | ((index & 7) << 3) | (base & 7);
			OneByte(info, sibByte);
		}

		if (IsMemDisp8(info.params[0])) OneByte(info, info.Disp.disp8);
		else if (IsMemDisp16(info.params[0])) AddUshort(info, info.Disp.disp16);
		else if (IsMemDisp32(info.params[0])) AddUlong(info, info.Disp.disp32);
	}

	void IntelAssembler::ProcessFpuInstr(AnalyzeInfo& info, size_t bits, FpuInstrFeatures& feature)
	{
		if ((feature.forms & FpuForm_ToST0) != 0)
		{
			if (info.numParams != 2)
			{
				throw "Invalid parameters";
			}

			if (info.params[0] != Param::st0 || !IsFpuReg(info.params[1]))
			{
				throw "Invalid parameters";
			}

			size_t reg;
			GetFpuReg(info.params[0], reg);
			OneByte(info, feature.FpuForm_ToST0_Opcode1);
			OneByte(info, feature.FpuForm_ToST0_Opcode2 | (reg & 7));
			return;
		}

		if ((feature.forms & FpuForm_FromST0) != 0)
		{
			if (info.numParams != 2)
			{
				throw "Invalid parameters";
			}

			if (!IsFpuReg(info.params[0]) || info.params[1] != Param::st0)
			{
				throw "Invalid parameters";
			}

			size_t reg;
			GetFpuReg(info.params[1], reg);
			OneByte(info, feature.FpuForm_FromST0_Opcode1);
			OneByte(info, feature.FpuForm_FromST0_Opcode2 | (reg & 7));
			return;
		}

		if ((feature.forms & FpuForm_STn) != 0)
		{
			if (info.numParams != 1)
			{
				throw "Invalid parameters";
			}

			if (IsFpuReg(info.params[0]))
			{
				size_t reg;
				GetFpuReg(info.params[0], reg);
				OneByte(info, feature.FpuForm_STn_Opcode1);
				OneByte(info, feature.FpuForm_STn_Opcode2 | (reg & 7));
				return;
			}
		}

		// In the case of FPU instructions, PtrHint selects the opcode used. Changing the operand size makes no sense here.

		if ((feature.forms & FpuForm_M32FP) != 0)
		{
			if (info.ptrHint == PtrHint::DwordPtr)
			{
				HandleModRmFpu(info, bits, feature.FpuForm_M32FP_Opcode, feature.FpuForm_M32FP_RegOpcode, 0);
				return;
			}
		}

		if ((feature.forms & FpuForm_M64FP) != 0)
		{
			if (info.ptrHint == PtrHint::QwordPtr)
			{
				HandleModRmFpu(info, bits, feature.FpuForm_M64FP_Opcode, feature.FpuForm_M64FP_RegOpcode, 0);
				return;
			}
		}

		if ((feature.forms & FpuForm_M80FP) != 0)
		{
			if (info.ptrHint == PtrHint::XwordPtr)
			{
				HandleModRmFpu(info, bits, feature.FpuForm_M80FP_Opcode, feature.FpuForm_M80FP_RegOpcode, 0);
				return;
			}
		}

		if ((feature.forms & FpuForm_M80BCD) != 0)
		{
			HandleModRmFpu(info, bits, feature.FpuForm_M80BCD_Opcode, feature.FpuForm_M80BCD_RegOpcode, 0);
			return;
		}

		if ((feature.forms & FpuForm_M16INT) != 0)
		{
			if (info.ptrHint == PtrHint::WordPtr)
			{
				HandleModRmFpu(info, bits, feature.FpuForm_M16INT_Opcode, feature.FpuForm_M16INT_RegOpcode, 0);
				return;
			}
		}

		if ((feature.forms & FpuForm_M32INT) != 0)
		{
			if (info.ptrHint == PtrHint::DwordPtr)
			{
				HandleModRmFpu(info, bits, feature.FpuForm_M32INT_Opcode, feature.FpuForm_M32INT_RegOpcode, 0);
				return;
			}
		}

		if ((feature.forms & FpuForm_M64INT) != 0)
		{
			if (info.ptrHint == PtrHint::QwordPtr)
			{
				HandleModRmFpu(info, bits, feature.FpuForm_M64INT_Opcode, feature.FpuForm_M64INT_RegOpcode, 0);
				return;
			}
		}

		if ((feature.forms & FpuForm_M2Byte) != 0)
		{
			HandleModRmFpu(info, bits, feature.FpuForm_M2Byte_Opcode, feature.FpuForm_M2Byte_RegOpcode, 0);
			return;
		}

		if ((feature.forms & FpuForm_M14_28Byte) != 0)
		{
			if (info.ptrHint == PtrHint::M14Ptr || info.ptrHint == PtrHint::M28Ptr)
			{
				HandleModRmFpu(info, bits, feature.FpuForm_M14_28Byte_Opcode, feature.FpuForm_M14_28Byte_RegOpcode, 0);
				return;
			}
		}

		if ((feature.forms & FpuForm_M94_108Byte) != 0)
		{
			if (info.ptrHint == PtrHint::M94Ptr || info.ptrHint == PtrHint::M108Ptr)
			{
				HandleModRmFpu(info, bits, feature.FpuForm_M94_108Byte_Opcode, feature.FpuForm_M94_108Byte_RegOpcode, 0);
				return;
			}
		}

		if ((feature.forms & FpuForm_M512Byte) != 0)
		{
			HandleModRmFpu(info, bits, feature.FpuForm_M512Byte_Opcode, feature.FpuForm_M512Byte_RegOpcode, feature.FpuForm_M512Byte_ExtOpcode);
			return;
		}

		throw "Invalid instruction form";
	}

	void IntelAssembler::FpuAssemble(size_t bits, AnalyzeInfo& info)
	{
		switch (info.instr)
		{
			case Instruction::fld:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M32FP | FpuForm_M64FP | FpuForm_M80FP | FpuForm_STn;

				feature.FpuForm_M32FP_Opcode = 0xD9;
				feature.FpuForm_M32FP_RegOpcode = 0;
				feature.FpuForm_M64FP_Opcode = 0xDD;
				feature.FpuForm_M64FP_RegOpcode = 0;
				feature.FpuForm_M80FP_Opcode = 0xDB;
				feature.FpuForm_M80FP_RegOpcode = 5;
				feature.FpuForm_STn_Opcode1 = 0xD9;
				feature.FpuForm_STn_Opcode2 = 0xC0;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fst:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M32FP | FpuForm_M64FP | FpuForm_STn;

				feature.FpuForm_M32FP_Opcode = 0xD9;
				feature.FpuForm_M32FP_RegOpcode = 2;
				feature.FpuForm_M64FP_Opcode = 0xDD;
				feature.FpuForm_M64FP_RegOpcode = 2;
				feature.FpuForm_STn_Opcode1 = 0xDD;
				feature.FpuForm_STn_Opcode2 = 0xD0;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fstp:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M32FP | FpuForm_M64FP | FpuForm_M80FP | FpuForm_STn;

				feature.FpuForm_M32FP_Opcode = 0xD9;
				feature.FpuForm_M32FP_RegOpcode = 3;
				feature.FpuForm_M64FP_Opcode = 0xDD;
				feature.FpuForm_M64FP_RegOpcode = 3;
				feature.FpuForm_M80FP_Opcode = 0xDB;
				feature.FpuForm_M80FP_RegOpcode = 7;
				feature.FpuForm_STn_Opcode1 = 0xDD;
				feature.FpuForm_STn_Opcode2 = 0xD8;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fild:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M16INT | FpuForm_M32INT | FpuForm_M64INT;

				feature.FpuForm_M16INT_Opcode = 0xDF;
				feature.FpuForm_M16INT_RegOpcode = 0;
				feature.FpuForm_M32INT_Opcode = 0xDB;
				feature.FpuForm_M32INT_RegOpcode = 0;
				feature.FpuForm_M64INT_Opcode = 0xDF;
				feature.FpuForm_M64INT_RegOpcode = 5;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fist:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M16INT | FpuForm_M32INT;

				feature.FpuForm_M16INT_Opcode = 0xDF;
				feature.FpuForm_M16INT_RegOpcode = 2;
				feature.FpuForm_M32INT_Opcode = 0xDB;
				feature.FpuForm_M32INT_RegOpcode = 2;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fistp:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M16INT | FpuForm_M32INT | FpuForm_M64INT;

				feature.FpuForm_M16INT_Opcode = 0xDF;
				feature.FpuForm_M16INT_RegOpcode = 3;
				feature.FpuForm_M32INT_Opcode = 0xDB;
				feature.FpuForm_M32INT_RegOpcode = 3;
				feature.FpuForm_M64INT_Opcode = 0xDF;
				feature.FpuForm_M64INT_RegOpcode = 7;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fbld:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M80BCD;

				feature.FpuForm_M80BCD_Opcode = 0xDF;
				feature.FpuForm_M80BCD_RegOpcode = 4;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fbstp:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M80BCD;

				feature.FpuForm_M80BCD_Opcode = 0xDF;
				feature.FpuForm_M80BCD_RegOpcode = 6;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fxch:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_STn;

				feature.FpuForm_STn_Opcode1 = 0xD9;
				feature.FpuForm_STn_Opcode2 = 0xC8;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fcmovb:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_ToST0;

				feature.FpuForm_ToST0_Opcode1 = 0xDA;
				feature.FpuForm_ToST0_Opcode2 = 0xC0;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fcmove:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_ToST0;

				feature.FpuForm_ToST0_Opcode1 = 0xDA;
				feature.FpuForm_ToST0_Opcode2 = 0xC8;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fcmovbe:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_ToST0;

				feature.FpuForm_ToST0_Opcode1 = 0xDA;
				feature.FpuForm_ToST0_Opcode2 = 0xD0;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fcmovu:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_ToST0;

				feature.FpuForm_ToST0_Opcode1 = 0xDA;
				feature.FpuForm_ToST0_Opcode2 = 0xD8;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fcmovnb:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_ToST0;

				feature.FpuForm_ToST0_Opcode1 = 0xDB;
				feature.FpuForm_ToST0_Opcode2 = 0xC0;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fcmovne:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_ToST0;

				feature.FpuForm_ToST0_Opcode1 = 0xDB;
				feature.FpuForm_ToST0_Opcode2 = 0xC8;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fcmovnbe:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_ToST0;

				feature.FpuForm_ToST0_Opcode1 = 0xDB;
				feature.FpuForm_ToST0_Opcode2 = 0xD0;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fcmovnu:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_ToST0;

				feature.FpuForm_ToST0_Opcode1 = 0xDB;
				feature.FpuForm_ToST0_Opcode2 = 0xD8;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fadd:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M32FP | FpuForm_M64FP | FpuForm_ToST0 | FpuForm_FromST0;

				feature.FpuForm_M32FP_Opcode = 0xD8;
				feature.FpuForm_M32FP_RegOpcode = 0;
				feature.FpuForm_M64FP_Opcode = 0xDC;
				feature.FpuForm_M64FP_RegOpcode = 0;
				feature.FpuForm_ToST0_Opcode1 = 0xD8;
				feature.FpuForm_ToST0_Opcode2 = 0xC0;
				feature.FpuForm_FromST0_Opcode1 = 0xDC;
				feature.FpuForm_FromST0_Opcode2 = 0xC0;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::faddp:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_FromST0;

				feature.FpuForm_FromST0_Opcode1 = 0xDE;
				feature.FpuForm_FromST0_Opcode2 = 0xC0;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fiadd:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M16INT | FpuForm_M32INT;

				feature.FpuForm_M16INT_Opcode = 0xDE;
				feature.FpuForm_M16INT_RegOpcode = 0;
				feature.FpuForm_M32INT_Opcode = 0xDA;
				feature.FpuForm_M32INT_RegOpcode = 0;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fsub:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M32FP | FpuForm_M64FP | FpuForm_ToST0 | FpuForm_FromST0;

				feature.FpuForm_M32FP_Opcode = 0xD8;
				feature.FpuForm_M32FP_RegOpcode = 4;
				feature.FpuForm_M64FP_Opcode = 0xDC;
				feature.FpuForm_M64FP_RegOpcode = 4;
				feature.FpuForm_ToST0_Opcode1 = 0xD8;
				feature.FpuForm_ToST0_Opcode2 = 0xE0;
				feature.FpuForm_FromST0_Opcode1 = 0xDC;
				feature.FpuForm_FromST0_Opcode2 = 0xE8;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fsubp:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_FromST0;

				feature.FpuForm_FromST0_Opcode1 = 0xDE;
				feature.FpuForm_FromST0_Opcode2 = 0xE8;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fisub:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M16INT | FpuForm_M32INT;

				feature.FpuForm_M16INT_Opcode = 0xDE;
				feature.FpuForm_M16INT_RegOpcode = 4;
				feature.FpuForm_M32INT_Opcode = 0xDA;
				feature.FpuForm_M32INT_RegOpcode = 4;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fsubr:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M32FP | FpuForm_M64FP | FpuForm_ToST0 | FpuForm_FromST0;

				feature.FpuForm_M32FP_Opcode = 0xD8;
				feature.FpuForm_M32FP_RegOpcode = 5;
				feature.FpuForm_M64FP_Opcode = 0xDC;
				feature.FpuForm_M64FP_RegOpcode = 5;
				feature.FpuForm_ToST0_Opcode1 = 0xD8;
				feature.FpuForm_ToST0_Opcode2 = 0xE8;
				feature.FpuForm_FromST0_Opcode1 = 0xDC;
				feature.FpuForm_FromST0_Opcode2 = 0xE0;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fsubrp:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_FromST0;

				feature.FpuForm_FromST0_Opcode1 = 0xDE;
				feature.FpuForm_FromST0_Opcode2 = 0xE0;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fisubr:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M16INT | FpuForm_M32INT;

				feature.FpuForm_M16INT_Opcode = 0xDE;
				feature.FpuForm_M16INT_RegOpcode = 5;
				feature.FpuForm_M32INT_Opcode = 0xDA;
				feature.FpuForm_M32INT_RegOpcode = 5;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fmul:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M32FP | FpuForm_M64FP | FpuForm_ToST0 | FpuForm_FromST0;

				feature.FpuForm_M32FP_Opcode = 0xD8;
				feature.FpuForm_M32FP_RegOpcode = 1;
				feature.FpuForm_M64FP_Opcode = 0xDC;
				feature.FpuForm_M64FP_RegOpcode = 1;
				feature.FpuForm_ToST0_Opcode1 = 0xD8;
				feature.FpuForm_ToST0_Opcode2 = 0xC8;
				feature.FpuForm_FromST0_Opcode1 = 0xDC;
				feature.FpuForm_FromST0_Opcode2 = 0xC8;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fmulp:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_FromST0;

				feature.FpuForm_FromST0_Opcode1 = 0xDE;
				feature.FpuForm_FromST0_Opcode2 = 0xC8;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fimul:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M16INT | FpuForm_M32INT;

				feature.FpuForm_M16INT_Opcode = 0xDE;
				feature.FpuForm_M16INT_RegOpcode = 1;
				feature.FpuForm_M32INT_Opcode = 0xDA;
				feature.FpuForm_M32INT_RegOpcode = 1;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fdiv:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M32FP | FpuForm_M64FP | FpuForm_ToST0 | FpuForm_FromST0;

				feature.FpuForm_M32FP_Opcode = 0xD8;
				feature.FpuForm_M32FP_RegOpcode = 6;
				feature.FpuForm_M64FP_Opcode = 0xDC;
				feature.FpuForm_M64FP_RegOpcode = 6;
				feature.FpuForm_ToST0_Opcode1 = 0xD8;
				feature.FpuForm_ToST0_Opcode2 = 0xF0;
				feature.FpuForm_FromST0_Opcode1 = 0xDC;
				feature.FpuForm_FromST0_Opcode2 = 0xF8;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fdivp:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_FromST0;

				feature.FpuForm_FromST0_Opcode1 = 0xDE;
				feature.FpuForm_FromST0_Opcode2 = 0xF8;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fidiv:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M16INT | FpuForm_M32INT;

				feature.FpuForm_M16INT_Opcode = 0xDE;
				feature.FpuForm_M16INT_RegOpcode = 6;
				feature.FpuForm_M32INT_Opcode = 0xDA;
				feature.FpuForm_M32INT_RegOpcode = 6;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fdivr:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M32FP | FpuForm_M64FP | FpuForm_ToST0 | FpuForm_FromST0;

				feature.FpuForm_M32FP_Opcode = 0xD8;
				feature.FpuForm_M32FP_RegOpcode = 7;
				feature.FpuForm_M64FP_Opcode = 0xDC;
				feature.FpuForm_M64FP_RegOpcode = 7;
				feature.FpuForm_ToST0_Opcode1 = 0xD8;
				feature.FpuForm_ToST0_Opcode2 = 0xF8;
				feature.FpuForm_FromST0_Opcode1 = 0xDC;
				feature.FpuForm_FromST0_Opcode2 = 0xF0;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fdivrp:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_FromST0;

				feature.FpuForm_FromST0_Opcode1 = 0xDE;
				feature.FpuForm_FromST0_Opcode2 = 0xF0;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fidivr:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M16INT | FpuForm_M32INT;

				feature.FpuForm_M16INT_Opcode = 0xDE;
				feature.FpuForm_M16INT_RegOpcode = 7;
				feature.FpuForm_M32INT_Opcode = 0xDA;
				feature.FpuForm_M32INT_RegOpcode = 7;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fprem:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xF8);
				break;
			}

			case Instruction::fprem1:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xF5);
				break;
			}

			case Instruction::fabs:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xE1);
				break;
			}

			case Instruction::fchs:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xE0);
				break;
			}

			case Instruction::frndint:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xFC);
				break;
			}

			case Instruction::fscale:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xFD);
				break;
			}

			case Instruction::fsqrt:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xFA);
				break;
			}

			case Instruction::fxtract:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xF4);
				break;
			}

			case Instruction::fcom:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M32FP | FpuForm_M64FP | FpuForm_STn;

				feature.FpuForm_M32FP_Opcode = 0xD8;
				feature.FpuForm_M32FP_RegOpcode = 2;
				feature.FpuForm_M64FP_Opcode = 0xDC;
				feature.FpuForm_M64FP_RegOpcode = 2;
				feature.FpuForm_STn_Opcode1 = 0xD8;
				feature.FpuForm_STn_Opcode2 = 0xD0;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fcomp:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M32FP | FpuForm_M64FP | FpuForm_STn;

				feature.FpuForm_M32FP_Opcode = 0xD8;
				feature.FpuForm_M32FP_RegOpcode = 3;
				feature.FpuForm_M64FP_Opcode = 0xDC;
				feature.FpuForm_M64FP_RegOpcode = 3;
				feature.FpuForm_STn_Opcode1 = 0xD8;
				feature.FpuForm_STn_Opcode2 = 0xD8;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fcompp:
			{
				OneByte(info, 0xDE);
				OneByte(info, 0xD9);
				break;
			}

			case Instruction::fucom:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_STn;

				feature.FpuForm_STn_Opcode1 = 0xDD;
				feature.FpuForm_STn_Opcode2 = 0xE0;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fucomp:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_STn;

				feature.FpuForm_STn_Opcode1 = 0xDD;
				feature.FpuForm_STn_Opcode2 = 0xE8;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fucompp:
			{
				OneByte(info, 0xDA);
				OneByte(info, 0xE9);
				break;
			}

			case Instruction::ficom:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M16INT | FpuForm_M32INT;

				feature.FpuForm_M16INT_Opcode = 0xDE;
				feature.FpuForm_M16INT_RegOpcode = 2;
				feature.FpuForm_M32INT_Opcode = 0xDA;
				feature.FpuForm_M32INT_RegOpcode = 2;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::ficomp:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M16INT | FpuForm_M32INT;

				feature.FpuForm_M16INT_Opcode = 0xDE;
				feature.FpuForm_M16INT_RegOpcode = 3;
				feature.FpuForm_M32INT_Opcode = 0xDA;
				feature.FpuForm_M32INT_RegOpcode = 3;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fcomi:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_ToST0;

				feature.FpuForm_ToST0_Opcode1 = 0xDB;
				feature.FpuForm_ToST0_Opcode2 = 0xF0;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fcomip:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_ToST0;

				feature.FpuForm_ToST0_Opcode1 = 0xDF;
				feature.FpuForm_ToST0_Opcode2 = 0xF0;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fucomi:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_ToST0;

				feature.FpuForm_ToST0_Opcode1 = 0xDB;
				feature.FpuForm_ToST0_Opcode2 = 0xE8;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fucomip:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_ToST0;

				feature.FpuForm_ToST0_Opcode1 = 0xDF;
				feature.FpuForm_ToST0_Opcode2 = 0xE8;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::ftst:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xE4);
				break;
			}

			case Instruction::fxam:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xE5);
				break;
			}

			case Instruction::fsin:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xFE);
				break;
			}

			case Instruction::fcos:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xFF);
				break;
			}

			case Instruction::fsincos:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xFB);
				break;
			}

			case Instruction::fptan:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xF2);
				break;
			}

			case Instruction::fpatan:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xF3);
				break;
			}

			case Instruction::f2xm1:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xF0);
				break;
			}

			case Instruction::fyl2x:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xF1);
				break;
			}

			case Instruction::fyl2xp1:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xF9);
				break;
			}

			case Instruction::fld1:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xE8);
				break;
			}

			case Instruction::fldl2t:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xE9);
				break;
			}

			case Instruction::fldl2e:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xEA);
				break;
			}

			case Instruction::fldpi:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xEB);
				break;
			}

			case Instruction::fldlg2:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xEC);
				break;
			}

			case Instruction::fldln2:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xED);
				break;
			}

			case Instruction::fldz:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xEE);
				break;
			}

			case Instruction::fincstp:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xF7);
				break;
			}

			case Instruction::fdecstp:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xF6);
				break;
			}

			case Instruction::ffree:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_STn;

				feature.FpuForm_STn_Opcode1 = 0xDD;
				feature.FpuForm_STn_Opcode2 = 0xC0;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::finit:
			{
				OneByte(info, 0x9B);
				OneByte(info, 0xDB);
				OneByte(info, 0xE3);
				break;
			}

			case Instruction::fninit:
			{
				OneByte(info, 0xDB);
				OneByte(info, 0xE3);
				break;
			}

			case Instruction::fclex:
			{
				OneByte(info, 0x9B);
				OneByte(info, 0xDB);
				OneByte(info, 0xE2);
				break;
			}

			case Instruction::fnclex:
			{
				OneByte(info, 0xDB);
				OneByte(info, 0xE2);
				break;
			}

			case Instruction::fstcw:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M2Byte;

				feature.FpuForm_M2Byte_Opcode = 0xD9;
				feature.FpuForm_M2Byte_RegOpcode = 7;

				OneByte(info, 0x9B);
				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fnstcw:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M2Byte;

				feature.FpuForm_M2Byte_Opcode = 0xD9;
				feature.FpuForm_M2Byte_RegOpcode = 7;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fldcw:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M2Byte;

				feature.FpuForm_M2Byte_Opcode = 0xD9;
				feature.FpuForm_M2Byte_RegOpcode = 5;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fstenv:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M14_28Byte;

				feature.FpuForm_M14_28Byte_Opcode = 0xD9;
				feature.FpuForm_M14_28Byte_RegOpcode = 6;

				OneByte(info, 0x9B);
				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fnstenv:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M14_28Byte;

				feature.FpuForm_M14_28Byte_Opcode = 0xD9;
				feature.FpuForm_M14_28Byte_RegOpcode = 6;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fldenv:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M14_28Byte;

				feature.FpuForm_M14_28Byte_Opcode = 0xD9;
				feature.FpuForm_M14_28Byte_RegOpcode = 4;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fsave:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M94_108Byte;

				feature.FpuForm_M94_108Byte_Opcode = 0xDD;
				feature.FpuForm_M94_108Byte_RegOpcode = 6;

				OneByte(info, 0x9B);
				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fnsave:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M94_108Byte;

				feature.FpuForm_M94_108Byte_Opcode = 0xDD;
				feature.FpuForm_M94_108Byte_RegOpcode = 6;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::frstor:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M94_108Byte;

				feature.FpuForm_M94_108Byte_Opcode = 0xDD;
				feature.FpuForm_M94_108Byte_RegOpcode = 4;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fstsw:
			{
				if (info.params[0] == Param::ax && info.numParams == 1)
				{
					OneByte(info, 0x9B);
					OneByte(info, 0xDF);
					OneByte(info, 0xE0);
				}
				else
				{
					FpuInstrFeatures feature = { 0 };

					feature.forms = FpuForm_M2Byte;

					feature.FpuForm_M2Byte_Opcode = 0xDD;
					feature.FpuForm_M2Byte_RegOpcode = 7;

					OneByte(info, 0x9B);
					ProcessFpuInstr(info, bits, feature);
				}
				break;
			}

			case Instruction::fnstsw:
			{
				if (info.params[0] == Param::ax && info.numParams == 1)
				{
					OneByte(info, 0xDF);
					OneByte(info, 0xE0);
				}
				else
				{
					FpuInstrFeatures feature = { 0 };

					feature.forms = FpuForm_M2Byte;

					feature.FpuForm_M2Byte_Opcode = 0xDD;
					feature.FpuForm_M2Byte_RegOpcode = 7;

					ProcessFpuInstr(info, bits, feature);
				}
				break;
			}

			case Instruction::fnop:
			{
				OneByte(info, 0xD9);
				OneByte(info, 0xD0);
				break;
			}

			case Instruction::fxsave:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M512Byte;

				feature.FpuForm_M512Byte_ExtOpcode = 0x0F;
				feature.FpuForm_M512Byte_Opcode = 0xAE;
				feature.FpuForm_M512Byte_RegOpcode = 0;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fxsave64:
			{
				if (bits == 64)
				{
					FpuInstrFeatures feature = { 0 };

					feature.forms = FpuForm_M512Byte;

					feature.FpuForm_M512Byte_ExtOpcode = 0x0F;
					feature.FpuForm_M512Byte_Opcode = 0xAE;
					feature.FpuForm_M512Byte_RegOpcode = 0;

					ProcessFpuInstr(info, bits, feature);
				}
				else
				{
					Invalid();
				}
				break;
			}

			case Instruction::fxrstor:
			{
				FpuInstrFeatures feature = { 0 };

				feature.forms = FpuForm_M512Byte;

				feature.FpuForm_M512Byte_ExtOpcode = 0x0F;
				feature.FpuForm_M512Byte_Opcode = 0xAE;
				feature.FpuForm_M512Byte_RegOpcode = 1;

				ProcessFpuInstr(info, bits, feature);
				break;
			}

			case Instruction::fxrstor64:
			{
				if (bits == 64)
				{
					FpuInstrFeatures feature = { 0 };

					feature.forms = FpuForm_M512Byte;

					feature.FpuForm_M512Byte_ExtOpcode = 0x0F;
					feature.FpuForm_M512Byte_Opcode = 0xAE;
					feature.FpuForm_M512Byte_RegOpcode = 1;

					ProcessFpuInstr(info, bits, feature);
				}
				else
				{
					Invalid();
				}
				break;
			}

			default:
				throw "Invalid instruction";
		}
	}

	template <> AnalyzeInfo IntelAssembler::fld<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fld;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fld<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fld;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fld<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fld;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fst<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fst;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fst<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fst;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fst<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fst;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fstp<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fstp;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fstp<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fstp;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fstp<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fstp;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fild<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fild;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fild<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fild;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fild<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fild;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fist<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fist;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fist<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fist;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fist<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fist;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fistp<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fistp;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fistp<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fistp;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fistp<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fistp;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fbld<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fbld;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fbld<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fbld;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fbld<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fbld;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fbstp<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fbstp;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fbstp<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fbstp;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fbstp<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fbstp;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fxch<16>(Param p)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fxch;
		info.params[info.numParams++] = p;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fxch<32>(Param p)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fxch;
		info.params[info.numParams++] = p;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fxch<64>(Param p)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fxch;
		info.params[info.numParams++] = p;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcmovb<16>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmovb;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcmovb<32>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmovb;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcmovb<64>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmovb;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcmove<16>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmove;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcmove<32>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmove;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcmove<64>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmove;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcmovbe<16>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmovbe;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcmovbe<32>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmovbe;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcmovbe<64>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmovbe;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcmovu<16>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmovu;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcmovu<32>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmovu;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcmovu<64>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmovu;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcmovnb<16>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmovnb;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcmovnb<32>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmovnb;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcmovnb<64>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmovnb;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcmovne<16>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmovne;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcmovne<32>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmovne;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcmovne<64>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmovne;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcmovnbe<16>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmovnbe;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcmovnbe<32>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmovnbe;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcmovnbe<64>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmovnbe;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcmovnu<16>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmovnu;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcmovnu<32>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmovnu;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcmovnu<64>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcmovnu;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fadd<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fadd;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fadd<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fadd;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fadd<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fadd;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fadd<16>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fadd;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fadd<32>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fadd;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fadd<64>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fadd;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::faddp<16>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::faddp;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::faddp<32>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::faddp;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::faddp<64>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::faddp;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fiadd<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fiadd;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fiadd<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fiadd;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fiadd<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fiadd;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsub<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fsub;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsub<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fsub;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsub<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fsub;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsub<16>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsub;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsub<32>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsub;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsub<64>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsub;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsubp<16>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsubp;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsubp<32>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsubp;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsubp<64>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsubp;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fisub<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fisub;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fisub<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fisub;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fisub<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fisub;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsubr<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fsubr;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsubr<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fsubr;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsubr<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fsubr;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsubr<16>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsubr;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsubr<32>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsubr;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsubr<64>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsubr;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsubrp<16>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsubrp;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsubrp<32>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsubrp;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsubrp<64>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsubrp;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fisubr<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fisubr;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fisubr<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fisubr;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fisubr<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fisubr;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fmul<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fmul;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fmul<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fmul;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fmul<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fmul;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fmul<16>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fmul;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fmul<32>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fmul;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fmul<64>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fmul;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fmulp<16>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fmulp;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fmulp<32>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fmulp;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fmulp<64>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fmulp;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fimul<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fimul;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fimul<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fimul;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fimul<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fimul;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fdiv<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fdiv;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fdiv<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fdiv;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fdiv<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fdiv;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fdiv<16>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fdiv;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fdiv<32>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fdiv;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fdiv<64>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fdiv;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fdivp<16>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fdivp;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fdivp<32>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fdivp;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fdivp<64>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fdivp;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fidiv<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fidiv;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fidiv<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fidiv;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fidiv<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fidiv;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fdivr<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fdivr;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fdivr<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fdivr;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fdivr<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fdivr;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fdivr<16>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fdivr;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fdivr<32>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fdivr;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fdivr<64>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fdivr;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fdivrp<16>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fdivrp;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fdivrp<32>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fdivrp;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fdivrp<64>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fdivrp;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fidivr<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fidivr;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fidivr<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fidivr;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fidivr<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fidivr;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fprem<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fprem;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fprem<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fprem;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fprem<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fprem;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fprem1<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fprem1;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fprem1<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fprem1;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fprem1<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fprem1;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fabs<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fabs;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fabs<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fabs;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fabs<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fabs;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fchs<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fchs;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fchs<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fchs;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fchs<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fchs;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::frndint<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::frndint;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::frndint<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::frndint;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::frndint<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::frndint;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fscale<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fscale;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fscale<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fscale;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fscale<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fscale;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsqrt<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsqrt;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsqrt<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsqrt;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsqrt<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsqrt;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fxtract<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fxtract;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fxtract<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fxtract;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fxtract<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fxtract;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcom<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fcom;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcom<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fcom;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcom<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fcom;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcomp<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fcomp;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcomp<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fcomp;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcomp<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fcomp;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcompp<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcompp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcompp<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcompp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcompp<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcompp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fucom<16>(Param p)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fucom;
		info.params[info.numParams++] = p;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fucom<32>(Param p)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fucom;
		info.params[info.numParams++] = p;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fucom<64>(Param p)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fucom;
		info.params[info.numParams++] = p;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fucomp<16>(Param p)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fucomp;
		info.params[info.numParams++] = p;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fucomp<32>(Param p)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fucomp;
		info.params[info.numParams++] = p;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fucomp<64>(Param p)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fucomp;
		info.params[info.numParams++] = p;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fucompp<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fucompp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fucompp<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fucompp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fucompp<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fucompp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ficom<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::ficom;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ficom<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::ficom;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ficom<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::ficom;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ficomp<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::ficomp;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ficomp<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::ficomp;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ficomp<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::ficomp;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcomi<16>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcomi;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcomi<32>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcomi;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcomi<64>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcomi;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcomip<16>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcomip;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcomip<32>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcomip;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcomip<64>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcomip;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fucomi<16>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fucomi;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fucomi<32>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fucomi;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fucomi<64>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fucomi;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fucomip<16>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fucomip;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fucomip<32>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fucomip;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fucomip<64>(Param to, Param from)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fucomip;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ftst<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ftst;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ftst<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ftst;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ftst<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ftst;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fxam<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fxam;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fxam<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fxam;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fxam<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fxam;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsin<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsin;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsin<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsin;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsin<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsin;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcos<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcos;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcos<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcos;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fcos<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fcos;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsincos<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsincos;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsincos<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsincos;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsincos<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fsincos;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fptan<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fptan;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fptan<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fptan;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fptan<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fptan;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fpatan<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fpatan;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fpatan<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fpatan;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fpatan<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fpatan;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::f2xm1<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::f2xm1;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::f2xm1<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::f2xm1;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::f2xm1<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::f2xm1;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fyl2x<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fyl2x;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fyl2x<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fyl2x;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fyl2x<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fyl2x;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fyl2xp1<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fyl2xp1;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fyl2xp1<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fyl2xp1;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fyl2xp1<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fyl2xp1;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fld1<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fld1;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fld1<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fld1;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fld1<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fld1;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fldl2t<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fldl2t;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fldl2t<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fldl2t;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fldl2t<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fldl2t;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fldl2e<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fldl2e;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fldl2e<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fldl2e;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fldl2e<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fldl2e;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fldpi<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fldpi;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fldpi<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fldpi;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fldpi<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fldpi;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fldlg2<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fldlg2;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fldlg2<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fldlg2;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fldlg2<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fldlg2;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fldln2<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fldln2;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fldln2<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fldln2;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fldln2<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fldln2;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fldz<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fldz;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fldz<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fldz;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fldz<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fldz;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fincstp<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fincstp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fincstp<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fincstp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fincstp<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fincstp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fdecstp<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fdecstp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fdecstp<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fdecstp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fdecstp<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fdecstp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ffree<16>(Param p)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ffree;
		info.params[info.numParams++] = p;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ffree<32>(Param p)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ffree;
		info.params[info.numParams++] = p;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ffree<64>(Param p)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ffree;
		info.params[info.numParams++] = p;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::finit<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::finit;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::finit<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::finit;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::finit<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::finit;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fninit<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fninit;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fninit<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fninit;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fninit<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fninit;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fclex<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fclex;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fclex<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fclex;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fclex<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fclex;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fnclex<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fnclex;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fnclex<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fnclex;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fnclex<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fnclex;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fstcw<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fstcw;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fstcw<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fstcw;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fstcw<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fstcw;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fnstcw<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fnstcw;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fnstcw<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fnstcw;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fnstcw<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fnstcw;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fldcw<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fldcw;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fldcw<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fldcw;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fldcw<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fldcw;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fstenv<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fstenv;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fstenv<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fstenv;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fstenv<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fstenv;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fnstenv<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fnstenv;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fnstenv<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fnstenv;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fnstenv<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fnstenv;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fldenv<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fldenv;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fldenv<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fldenv;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fldenv<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fldenv;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsave<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fsave;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsave<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fsave;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fsave<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fsave;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fnsave<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fnsave;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fnsave<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fnsave;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fnsave<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::fnsave;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::frstor<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::frstor;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::frstor<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::frstor;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::frstor<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::frstor;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fstsw<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fstsw;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fstsw<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fstsw;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fstsw<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fstsw;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fnstsw<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fnstsw;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fnstsw<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fnstsw;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fnstsw<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fnstsw;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fnop<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fnop;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fnop<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fnop;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fnop<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fnop;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fxsave<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fxsave;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fxsave<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fxsave;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fxsave<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fxsave;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fxsave64<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fxsave64;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fxsave64<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fxsave64;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fxsave64<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fxsave64;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fxrstor<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fxrstor;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fxrstor<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fxrstor;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fxrstor<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fxrstor;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fxrstor64<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fxrstor64;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fxrstor64<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fxrstor64;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fxrstor64<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::fxrstor64;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

}
