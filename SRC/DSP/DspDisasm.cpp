// DSP disassembler
#include "pch.h"

namespace DSP
{

	std::string DspDisasm::ParameterToString(DspParameter index, AnalyzeInfo& info)
	{
		std::string text;

		switch (index)
		{
			// Registers

			case DspParameter::r0: text = "r0"; break;
			case DspParameter::r1: text = "r1"; break;
			case DspParameter::r2: text = "r2"; break;
			case DspParameter::r3: text = "r3"; break;
			case DspParameter::m0: text = "m0"; break;
			case DspParameter::m1: text = "m1"; break;
			case DspParameter::m2: text = "m2"; break;
			case DspParameter::m3: text = "m3"; break;
			case DspParameter::l0: text = "l0"; break;
			case DspParameter::l1: text = "l1"; break;
			case DspParameter::l2: text = "l2"; break;
			case DspParameter::l3: text = "l3"; break;
			case DspParameter::pcs: text = "pcs"; break;
			case DspParameter::pss: text = "pss"; break;
			case DspParameter::eas: text = "eas"; break;
			case DspParameter::lcs: text = "lcs"; break;
			case DspParameter::a2: text = "a2"; break;
			case DspParameter::b2: text = "b2"; break;
			case DspParameter::dpp: text = "dpp"; break;
			case DspParameter::psr: text = "psr"; break;
			case DspParameter::ps0: text = "ps0"; break;
			case DspParameter::ps1: text = "ps1"; break;
			case DspParameter::ps2: text = "ps2"; break;
			case DspParameter::pc1: text = "pc1"; break;
			case DspParameter::x0: text = "x0"; break;
			case DspParameter::y0: text = "y0"; break;
			case DspParameter::x1: text = "x1"; break;
			case DspParameter::y1: text = "y1"; break;
			case DspParameter::a0: text = "a0"; break;
			case DspParameter::b0: text = "b0"; break;
			case DspParameter::a1: text = "a1"; break;
			case DspParameter::b1: text = "b1"; break;

			case DspParameter::a: text = "a"; break;
			case DspParameter::b: text = "b"; break;

			case DspParameter::x: text = "x"; break;
			case DspParameter::y: text = "y"; break;
			
			case DspParameter::prod: text = "p"; break;

			// PSR bits

			case DspParameter::psr_c: text = "c"; break;
			case DspParameter::psr_v: text = "v"; break;
			case DspParameter::psr_z: text = "z"; break;
			case DspParameter::psr_n: text = "n"; break;
			case DspParameter::psr_e: text = "e"; break;
			case DspParameter::psr_u: text = "u"; break;
			case DspParameter::psr_tb: text = "tb"; break;
			case DspParameter::psr_sv: text = "sv"; break;
			case DspParameter::psr_te0: text = "te0"; break;
			case DspParameter::psr_te1: text = "te1"; break;
			case DspParameter::psr_te2: text = "te2"; break;
			case DspParameter::psr_te3: text = "te3"; break;
			case DspParameter::psr_et: text = "et"; break;
			case DspParameter::psr_im: text = "im"; break;
			case DspParameter::psr_xl: text = "xl"; break;
			case DspParameter::psr_dp: text = "dp"; break;

			// Modifier

			case DspParameter::mod_none: text = "0"; break;
			case DspParameter::mod_dec: text = "-1"; break;
			case DspParameter::mod_inc: text = "+1"; break;
			case DspParameter::mod_minus_m: text = "-m"; break;
			case DspParameter::mod_plus_m0: text = "+m0"; break;
			case DspParameter::mod_plus_m1: text = "+m1"; break;
			case DspParameter::mod_plus_m2: text = "+m2"; break;
			case DspParameter::mod_plus_m3: text = "+m3"; break;
			case DspParameter::mod_plus_m: text = "+m"; break;

			// Immediates

			case DspParameter::Byte:
				text = "#0x" + ToHexString(info.ImmOperand.Byte);
				break;
			case DspParameter::Byte2:
				text = "#0x" + ToHexString(info.ImmOperand2.Byte);
				break;
			case DspParameter::SignedByte:
				text = std::to_string((int)(int16_t)info.ImmOperand.SignedByte);
				break;
			case DspParameter::SignedByte2:
				text = std::to_string((int)(int16_t)info.ImmOperand2.SignedByte);
				break;
			case DspParameter::SignedShort:
				text = std::to_string((int)info.ImmOperand.SignedShort);
				break;
			case DspParameter::SignedShort2:
				text = std::to_string((int)info.ImmOperand2.SignedShort);
				break;
			case DspParameter::UnsignedShort:
				text = "#0x" + ToHexString((uint16_t)info.ImmOperand.UnsignedShort);
				break;
			case DspParameter::UnsignedShort2:
				text = "#0x" + ToHexString((uint16_t)info.ImmOperand2.UnsignedShort);
				break;
			case DspParameter::Address:
				if (IsHardwareReg(info.ImmOperand.Address))
				{
					text = HardwareRegName(info.ImmOperand.Address);
				}
				else
				{
					text = "$0x" + ToHexString((uint16_t)info.ImmOperand.Address);
				}
				break;
			case DspParameter::Address2:
				text = "$0x" + ToHexString((uint16_t)info.ImmOperand2.Address);
				break;
		}

		return text;
	}

	std::string DspDisasm::InstrToString(DspRegularInstruction instr, ConditionCode cc)
	{
		std::string text;

		switch (instr)
		{
			case DspRegularInstruction::jmp: text = "jmp" + CondCodeToString(cc); break;
			case DspRegularInstruction::call: text = "call" + CondCodeToString(cc); break;
			case DspRegularInstruction::rets: text = "rets" + CondCodeToString(cc); break;
			case DspRegularInstruction::reti: text = "reti" + CondCodeToString(cc); break;
			case DspRegularInstruction::trap: text = "trap"; break;
			case DspRegularInstruction::wait: text = "wait"; break;
			case DspRegularInstruction::exec: text = "exec" + CondCodeToString(cc); break;
			case DspRegularInstruction::loop: text = "loop"; break;
			case DspRegularInstruction::rep: text = "rep"; break;
			case DspRegularInstruction::pld: text = "pld"; break;
			case DspRegularInstruction::nop: text = "nop"; break;
			case DspRegularInstruction::mr: text = "mr"; break;
			case DspRegularInstruction::adsi: text = "adsi"; break;
			case DspRegularInstruction::adli: text = "adli"; break;
			case DspRegularInstruction::cmpsi: text = "cmpsi"; break;
			case DspRegularInstruction::cmpli: text = "cmpli"; break;
			case DspRegularInstruction::lsfi: text = "lsfi"; break;
			case DspRegularInstruction::asfi: text = "asfi"; break;
			case DspRegularInstruction::xorli: text = "xorli"; break;
			case DspRegularInstruction::anli: text = "anli"; break;
			case DspRegularInstruction::orli: text = "orli"; break;
			case DspRegularInstruction::norm: text = "norm"; break;
			case DspRegularInstruction::div: text = "div"; break;
			case DspRegularInstruction::addc: text = "addc"; break;
			case DspRegularInstruction::subc: text = "subc"; break;
			case DspRegularInstruction::negc: text = "negc"; break;
			case DspRegularInstruction::max: text = "max"; break;
			case DspRegularInstruction::lsf: text = "lsf"; break;
			case DspRegularInstruction::asf: text = "asf"; break;
			case DspRegularInstruction::ld: text = "ld"; break;
			case DspRegularInstruction::st: text = "st"; break;
			case DspRegularInstruction::ldsa: text = "ldsa"; break;
			case DspRegularInstruction::stsa: text = "stsa"; break;
			case DspRegularInstruction::ldla: text = "ldla"; break;
			case DspRegularInstruction::stla: text = "stla"; break;
			case DspRegularInstruction::mv: text = "mv"; break;
			case DspRegularInstruction::mvsi: text = "mvsi"; break;
			case DspRegularInstruction::mvli: text = "mvli"; break;
			case DspRegularInstruction::stli: text = "stli"; break;
			case DspRegularInstruction::clr: text = "clr"; break;
			case DspRegularInstruction::set: text = "set"; break;
			case DspRegularInstruction::btstl: text = "btstl"; break;
			case DspRegularInstruction::btsth: text = "btsth"; break;
		}

		while (text.size() < 5)
		{
			text += " ";
		}

		return text;
	}

	std::string DspDisasm::ParrallelInstrToString(DspParallelInstruction instr)
	{
		std::string text;

		switch (instr)
		{
			case DspParallelInstruction::add: text = "add"; break;
			case DspParallelInstruction::addl: text = "addl"; break;
			case DspParallelInstruction::sub: text = "sub"; break;
			case DspParallelInstruction::amv: text = "amv"; break;
			case DspParallelInstruction::cmp: text = "cmp"; break;
			case DspParallelInstruction::inc: text = "inc"; break;
			case DspParallelInstruction::dec: text = "dec"; break;
			case DspParallelInstruction::abs: text = "abs"; break;
			case DspParallelInstruction::neg: text = "neg"; break;
			case DspParallelInstruction::clr: text = "clr"; break;
			case DspParallelInstruction::rnd: text = "rnd"; break;
			case DspParallelInstruction::rndp: text = "rndp"; break;
			case DspParallelInstruction::tst: text = "tst"; break;
			case DspParallelInstruction::lsl16: text = "lsl16"; break;
			case DspParallelInstruction::lsr16: text = "lsr16"; break;
			case DspParallelInstruction::asr16: text = "asr16"; break;
			case DspParallelInstruction::addp: text = "addp"; break;
			// Dont show nop2's
			case DspParallelInstruction::nop: break;
			case DspParallelInstruction::set: text = "set"; break;
			case DspParallelInstruction::mpy: text = "mpy"; break;
			case DspParallelInstruction::mac: text = "mac"; break;
			case DspParallelInstruction::macn: text = "macn"; break;
			case DspParallelInstruction::mvmpy: text = "mvmpy"; break;
			case DspParallelInstruction::rnmpy: text = "rnmpy"; break;
			case DspParallelInstruction::admpy: text = "admpy"; break;
			case DspParallelInstruction::_not: text = "not"; break;
			case DspParallelInstruction::_xor: text = "xor"; break;
			case DspParallelInstruction::_and: text = "and"; break;
			case DspParallelInstruction::_or: text = "or"; break;
			case DspParallelInstruction::lsf: text = "lsf"; break;
			case DspParallelInstruction::asf: text = "asf"; break;
		}

		while (text.size() < 5)
		{
			text += " ";
		}

		return text;
	}

	std::string DspDisasm::ParrallelMemInstrToString(DspParallelMemInstruction instr)
	{
		std::string text;

		switch (instr)
		{
			case DspParallelMemInstruction::nop: break;
			case DspParallelMemInstruction::ldd: text = "ldd"; break;
			case DspParallelMemInstruction::ls: text = "ls"; break;
			case DspParallelMemInstruction::ld: text = "ld"; break;
			case DspParallelMemInstruction::st: text = "st"; break;
			case DspParallelMemInstruction::mv: text = "mv"; break;
			case DspParallelMemInstruction::mr: text = "mr"; break;
		}

		while (text.size() < 5)
		{
			text += " ";
		}

		return text;
	}

	bool DspDisasm::IsHardwareReg(DspAddress address)
	{
		return address >= 0xFF00;
	}

	std::string DspDisasm::HardwareRegName(DspAddress address)
	{
		std::string text;

		switch ((DspHardwareRegs)address)
		{
			case DspHardwareRegs::CMBH: text = "CMBH"; break;
			case DspHardwareRegs::CMBL: text = "CMBL"; break;
			case DspHardwareRegs::DMBH: text = "DMBH"; break;
			case DspHardwareRegs::DMBL: text = "DMBL"; break;

			case DspHardwareRegs::DSMAH: text = "DSMAH"; break;
			case DspHardwareRegs::DSMAL: text = "DSMAL"; break;
			case DspHardwareRegs::DSPA: text = "DSPA"; break;
			case DspHardwareRegs::DSCR: text = "DSCR"; break;
			case DspHardwareRegs::DSBL: text = "DSBL"; break;

			case DspHardwareRegs::ACDAT2: text = "ACDAT2"; break;
			case DspHardwareRegs::ACSAH: text = "ACSAH"; break;
			case DspHardwareRegs::ACSAL: text = "ACSAL"; break;
			case DspHardwareRegs::ACEAH: text = "ACEAH"; break;
			case DspHardwareRegs::ACEAL: text = "ACEAL"; break;
			case DspHardwareRegs::ACCAH: text = "ACCAH"; break;
			case DspHardwareRegs::ACCAL: text = "ACCAL"; break;
			case DspHardwareRegs::ACDAT: text = "ACDAT"; break;

			case DspHardwareRegs::DIRQ: text = "DIRQ"; break;

			case DspHardwareRegs::ACFMT: text = "ACFMT"; break;
			case DspHardwareRegs::ACPDS: text = "ACPDS"; break;
			case DspHardwareRegs::ACYN1: text = "ACYN1"; break;
			case DspHardwareRegs::ACYN2: text = "ACYN2"; break;
			case DspHardwareRegs::ACGAN: text = "ACGAN"; break;

			case DspHardwareRegs::ADPCM_A00: text = "ADPCM_A00"; break;
			case DspHardwareRegs::ADPCM_A10: text = "ADPCM_A10"; break;
			case DspHardwareRegs::ADPCM_A20: text = "ADPCM_A20"; break;
			case DspHardwareRegs::ADPCM_A30: text = "ADPCM_A30"; break;
			case DspHardwareRegs::ADPCM_A40: text = "ADPCM_A40"; break;
			case DspHardwareRegs::ADPCM_A50: text = "ADPCM_A50"; break;
			case DspHardwareRegs::ADPCM_A60: text = "ADPCM_A60"; break;
			case DspHardwareRegs::ADPCM_A70: text = "ADPCM_A70"; break;
			case DspHardwareRegs::ADPCM_A01: text = "ADPCM_A01"; break;
			case DspHardwareRegs::ADPCM_A11: text = "ADPCM_A11"; break;
			case DspHardwareRegs::ADPCM_A21: text = "ADPCM_A21"; break;
			case DspHardwareRegs::ADPCM_A31: text = "ADPCM_A31"; break;
			case DspHardwareRegs::ADPCM_A41: text = "ADPCM_A41"; break;
			case DspHardwareRegs::ADPCM_A51: text = "ADPCM_A51"; break;
			case DspHardwareRegs::ADPCM_A61: text = "ADPCM_A61"; break;
			case DspHardwareRegs::ADPCM_A71: text = "ADPCM_A71"; break;

			default:
				text = "UnkHW_" + ToHexString((uint16_t)address);
		}

		return "$(" + text + ")";
	}

	std::string DspDisasm::CondCodeToString(ConditionCode cc)
	{
		std::string text = "";

		switch (cc)
		{
			case ConditionCode::ge: text = "ge"; break;
			case ConditionCode::lt: text = "lt"; break;
			case ConditionCode::gt: text = "gt"; break;
			case ConditionCode::le: text = "le"; break;
			case ConditionCode::nz: text = "nz"; break;
			case ConditionCode::z: text = "z"; break;
			case ConditionCode::nc: text = "nc"; break;
			case ConditionCode::c: text = "c"; break;
			case ConditionCode::ne: text = "ne"; break;
			case ConditionCode::e: text = "e"; break;
			case ConditionCode::nm: text = "nm"; break;
			case ConditionCode::m: text = "m"; break;
			case ConditionCode::nt: text = "nt"; break;
			case ConditionCode::t: text = "t"; break;
			case ConditionCode::v: text = "v"; break;
			case ConditionCode::always: text = ""; break;
		}

		return text;
	}

	std::string DspDisasm::Disasm(DspAddress startAddr, AnalyzeInfo& info)
	{
		bool firstParam;
		std::string text = "";

		// Address and code bytes

		text += ToHexString((uint16_t)startAddr);
		text += " ";

		for (size_t i = 0; i < DspCore::MaxInstructionSizeInBytes; i++)
		{
			if (i < info.sizeInBytes)
			{
				text += ToHexString(info.bytes[i]) + " ";
			}
			else
			{
				text += "   ";
			}
		}

		// Regular instruction

		if (!info.parallel)
		{
			if (info.instr != DspRegularInstruction::Unknown)
			{
				text += "\t" + DspDisasm::InstrToString(info.instr, info.cc) + "\t";
			}
			else
			{
				text += "\t??? ";
			}

			firstParam = true;
			for (size_t i = 0; i < info.numParameters; i++)
			{
				if (!firstParam)
				{
					text += ", ";
				}
				text += ParameterToString(info.params[i], info);
				firstParam = false;
			}
		}

		// Parallel instruction pair (same line)

		else
		{
			// Top

			if (info.parallelInstr != DspParallelInstruction::Unknown)
			{
				text += "\t" + DspDisasm::ParrallelInstrToString(info.parallelInstr) + "\t";
			}
			else
			{
				text += "\t??? ";
			}

			firstParam = true;
			for (size_t i = 0; i < info.numParameters; i++)
			{
				if (!firstParam)
				{
					text += ", ";
				}
				text += ParameterToString(info.params[i], info);
				firstParam = false;
			}

			// Bottom

			while (text.size() < 40)
			{
				text += " ";
			}

			if (info.parallelMemInstr != DspParallelMemInstruction::Unknown)
			{
				text += "\t" + ParrallelMemInstrToString(info.parallelMemInstr) + "\t";
			}
			else
			{
				text += "???";
			}

			firstParam = true;
			for (size_t i = 0; i < info.numParametersEx; i++)
			{
				if (!firstParam)
				{
					text += ", ";
				}
				text += ParameterToString(info.paramsEx[i], info);
				firstParam = false;
			}
		}

		return text;
	}

}
