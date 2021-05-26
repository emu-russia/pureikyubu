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

	void IntelAssembler::ProcessFpuInstr(AnalyzeInfo& info, size_t bits, FpuInstrFeatures& feature)
	{

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

			default:
				throw "Invalid instruction";
		}
	}

}
