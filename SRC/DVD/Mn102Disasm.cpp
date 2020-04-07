// MN102 disassembler.

#include "pch.h"

namespace DVD
{
	std::string MnDisasm::HexToStr(uint8_t value)
	{
		char buf[0x10] = { 0, };
		sprintf_s(buf, sizeof(buf) - 1, "%02X", value);
		return std::string(buf);
	}

	std::string MnDisasm::HexToStr(uint16_t value)
	{
		char buf[0x10] = { 0, };
		sprintf_s(buf, sizeof(buf) - 1, "%04X", value);
		return std::string(buf);
	}

	std::string MnDisasm::HexToStr(uint32_t value)
	{
		char buf[0x10] = { 0, };
		sprintf_s(buf, sizeof(buf) - 1, "%06X", value & 0xffffff);
		return std::string(buf);
	}

	std::string MnDisasm::CcToText(MnCond cc)
	{
		std::string text;

		switch (cc)
		{
			case MnCond::LT: text = "lt"; break;
			case MnCond::GT: text = "gt"; break;
			case MnCond::GE: text = "ge"; break;
			case MnCond::LE: text = "le"; break;
			case MnCond::CS: text = "cs"; break;
			case MnCond::HI: text = "hi"; break;
			case MnCond::CC: text = "cc"; break;
			case MnCond::LS: text = "ls"; break;
			case MnCond::EQ: text = "eq"; break;
			case MnCond::NE: text = "ne"; break;
			case MnCond::RA: text = "ra"; break;
			case MnCond::VC: text = "vc"; break;
			case MnCond::VS: text = "vs"; break;
			case MnCond::NC: text = "nc"; break;
			case MnCond::NS: text = "ns"; break;

			default:
				text = "??";
				break;
		}

		return text;
	}

	std::string MnDisasm::InstrToText(MnInstrInfo* info)
	{
		std::string text;

		// We are waiting for the c++ committee to be able to add intrinsics like type.ToString () :P
		// In the meantime, we train patience and discipline in ourselves.
		switch (info->instr)
		{
			case MnInstruction::MOV: text = "mov"; break;
			case MnInstruction::MOVX: text = "movx"; break;
			case MnInstruction::MOVB: text = "movb"; break;
			case MnInstruction::MOVBU: text = "movbu"; break;
			case MnInstruction::EXT: text = "ext"; break;
			case MnInstruction::EXTX: text = "extx"; break;
			case MnInstruction::EXTXU: text = "extxu"; break;
			case MnInstruction::EXTXB: text = "extxb"; break;
			case MnInstruction::EXTXBU: text = "extxbu"; break;
			case MnInstruction::ADD: text = "add"; break;
			case MnInstruction::ADDC: text = "addc"; break;
			case MnInstruction::ADDNF: text = "addnf"; break;
			case MnInstruction::SUB: text = "sub"; break;
			case MnInstruction::SUBC: text = "subc"; break;
			case MnInstruction::MUL: text = "mul"; break;
			case MnInstruction::MULU: text = "mulu"; break;
			case MnInstruction::DIVU: text = "divu"; break;
			case MnInstruction::CMP: text = "cmp"; break;
			case MnInstruction::AND: text = "and"; break;
			case MnInstruction::OR: text = "or"; break;
			case MnInstruction::XOR: text = "xor"; break;
			case MnInstruction::NOT: text = "not"; break;
			case MnInstruction::ASR: text = "asr"; break;
			case MnInstruction::LSR: text = "lsr"; break;
			case MnInstruction::ROR: text = "ror"; break;
			case MnInstruction::ROL: text = "rol"; break;
			case MnInstruction::BTST: text = "btst"; break;
			case MnInstruction::BSET: text = "bset"; break;
			case MnInstruction::BCLR: text = "bclr"; break;
			case MnInstruction::Bcc:
				text = "b";
				text += CcToText(info->cc);
				break;
			case MnInstruction::BccX:
				text = "b";
				text += CcToText(info->cc);
				text += "x";
				break;
			case MnInstruction::JMP: text = "jmp"; break;
			case MnInstruction::JSR: text = "jsr"; break;
			case MnInstruction::NOP: text = "nop"; break;
			case MnInstruction::RTS: text = "rts"; break;
			case MnInstruction::RTI: text = "rti"; break;

			default:
				text = "???";
				break;
		}

		return text;
	}

	std::string MnDisasm::OperandToText(uint32_t pc, MnInstrInfo* info, int n)
	{
		std::string text;

		switch (info->op[n])
		{
			case MnOperand::D0: text = "d0"; break;
			case MnOperand::D1: text = "d1"; break;
			case MnOperand::D2: text = "d2"; break;
			case MnOperand::D3: text = "d3"; break;
			case MnOperand::A0: text = "a0"; break;
			case MnOperand::A1: text = "a1"; break;
			case MnOperand::A2: text = "a2"; break;
			case MnOperand::A3: text = "a3"; break;
				
			case MnOperand::Imm8: text = "0x" + HexToStr(info->imm.Uint8); break;
			case MnOperand::Imm16: text = "0x" + HexToStr(info->imm.Uint16); break;
			case MnOperand::Imm24: text = "0x" + HexToStr(info->imm.Uint24); break;

			case MnOperand::Ind_A0: text = "(a0)"; break;
			case MnOperand::Ind_A1: text = "(a1)"; break;
			case MnOperand::Ind_A2: text = "(a2)"; break;
			case MnOperand::Ind_A3: text = "(a3)"; break;

			case MnOperand::D8_A0: text = "(0x" + HexToStr(info->imm.Uint8) + ", a0)"; break;
			case MnOperand::D8_A1: text = "(0x" + HexToStr(info->imm.Uint8) + ", a1)"; break;
			case MnOperand::D8_A2: text = "(0x" + HexToStr(info->imm.Uint8) + ", a2)"; break;
			case MnOperand::D8_A3: text = "(0x" + HexToStr(info->imm.Uint8) + ", a3)"; break;

			case MnOperand::D16_A0: text = "(0x" + HexToStr(info->imm.Uint16) + ", a0)"; break;
			case MnOperand::D16_A1: text = "(0x" + HexToStr(info->imm.Uint16) + ", a1)"; break;
			case MnOperand::D16_A2: text = "(0x" + HexToStr(info->imm.Uint16) + ", a2)"; break;
			case MnOperand::D16_A3: text = "(0x" + HexToStr(info->imm.Uint16) + ", a3)"; break;

			case MnOperand::D24_A0: text = "(0x" + HexToStr(info->imm.Uint24) + ", a0)"; break;
			case MnOperand::D24_A1: text = "(0x" + HexToStr(info->imm.Uint24) + ", a1)"; break;
			case MnOperand::D24_A2: text = "(0x" + HexToStr(info->imm.Uint24) + ", a2)"; break;
			case MnOperand::D24_A3: text = "(0x" + HexToStr(info->imm.Uint24) + ", a3)"; break;

			case MnOperand::D8_PC: text = "(0x" + HexToStr(info->imm.Uint8) + ", " + HexToStr(pc) + ")"; break;
			case MnOperand::D16_PC: text = "(0x" + HexToStr(info->imm.Uint16) + ", " + HexToStr(pc) + ")"; break;
			case MnOperand::D24_PC: text = "(0x" + HexToStr(info->imm.Uint24) + ", " + HexToStr(pc) + ")"; break;

			case MnOperand::Abs16: text = "(0x" + HexToStr(info->imm.Uint16) + ")"; break;
			case MnOperand::Abs24: text = "(0x" + HexToStr(info->imm.Uint24) + ")"; break;

			case MnOperand::Ind_D0_A0: text = "(d0, a0)"; break;
			case MnOperand::Ind_D0_A1: text = "(d0, a1)"; break;
			case MnOperand::Ind_D0_A2: text = "(d0, a2)"; break;
			case MnOperand::Ind_D0_A3: text = "(d0, a3)"; break;
			case MnOperand::Ind_D1_A0: text = "(d1, a0)"; break;
			case MnOperand::Ind_D1_A1: text = "(d1, a1)"; break;
			case MnOperand::Ind_D1_A2: text = "(d1, a2)"; break;
			case MnOperand::Ind_D1_A3: text = "(d1, a3)"; break;
			case MnOperand::Ind_D2_A0: text = "(d2, a0)"; break;
			case MnOperand::Ind_D2_A1: text = "(d2, a1)"; break;
			case MnOperand::Ind_D2_A2: text = "(d2, a2)"; break;
			case MnOperand::Ind_D2_A3: text = "(d2, a3)"; break;
			case MnOperand::Ind_D3_A0: text = "(d3, a0)"; break;
			case MnOperand::Ind_D3_A1: text = "(d3, a1)"; break;
			case MnOperand::Ind_D3_A2: text = "(d3, a2)"; break;
			case MnOperand::Ind_D3_A3: text = "(d3, a3)"; break;

			default:
				text = "???";
				break;
		}

		return text;
	}

	std::string MnDisasm::Disasm(uint32_t pc, MnInstrInfo* info)
	{
		assert(info);
		std::string text = "";

		// Program counter
		text += HexToStr(pc);
		text += "\t";

		// Bytes
		for (int i = 0; i < sizeof(info->instrBytes); i++)
		{
			if (i < info->instrSize)
			{
				text += HexToStr(info->instrBytes[i]);
			}
			else
			{
				text += "   ";
			}
		}
		text += "\t";

		// Instruction name
		text += InstrToText(info);
		text += "\t";

		// Operands
		bool first = true;
		for (int i = 0; i < info->numOp; i++)
		{
			if (!first)
			{
				text += ", ";
				first = false;
			}
			text += OperandToText(pc, info, i);
		}

		return text;
	}
}
