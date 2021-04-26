// Intel instruction code generator.

#include "pch.h"

namespace IntelCore
{

#pragma region "Private"

	void IntelAssembler::Invalid()
	{
		throw "Invalid instruction in specified mode";
	}

	void IntelAssembler::OneByte(AnalyzeInfo& info, uint8_t n)
	{
		if (info.instrSize >= InstrMaxSize)
		{
			throw "Code stream overflow";
		}

		info.instrBytes[info.instrSize] = n;
		info.instrSize++;
	}

	void IntelAssembler::TwoByte(AnalyzeInfo& info, uint8_t b1, uint8_t b2)
	{
		OneByte(info, b1);
		OneByte(info, b2);
	}

	void IntelAssembler::TriByte(AnalyzeInfo& info, uint8_t b1, uint8_t b2, uint8_t b3)
	{
		OneByte(info, b1);
		OneByte(info, b2);
		OneByte(info, b3);
	}

	void IntelAssembler::AddUshort(AnalyzeInfo& info, uint16_t n)
	{
		OneByte(info, n & 0xff);
		OneByte(info, (n >> 8) & 0xff);
	}

	void IntelAssembler::AddUlong(AnalyzeInfo& info, uint32_t n)
	{
		OneByte(info, n & 0xff);
		OneByte(info, (n >> 8) & 0xff);
		OneByte(info, (n >> 16) & 0xff);
		OneByte(info, (n >> 24) & 0xff);
	}

	void IntelAssembler::OneByteImm8(AnalyzeInfo& info, uint8_t n)
	{
		if (info.numParams != 1)
		{
			throw "Invalid number of parameters for instruction with imm8.";
		}

		if (info.params[0] != Param::imm8)
		{
			throw "Invalid parameter type for instruction with imm8.";
		}

		OneByte(info, n);
		OneByte(info, info.Imm.uimm8);
	}

	void IntelAssembler::AddImmParam(AnalyzeInfo& info, uint8_t n)
	{
		if (info.numParams >= ParamsMax)
		{
			throw "Parameter overflow";
		}

		info.params[info.numParams] = Param::imm8;
		info.Imm.uimm8 = n;
		info.numParams++;
	}

	void IntelAssembler::AddPrefix(AnalyzeInfo& info, Prefix pre)
	{
		if (info.numPrefixes >= PrefixMaxSize)
		{
			throw "Prefix overflow";
		}

		info.prefixes[info.numPrefixes] = pre;
		info.numPrefixes++;
	}

	void IntelAssembler::AddPrefixByte(AnalyzeInfo& info, uint8_t pre)
	{
		if (info.prefixSize >= PrefixMaxSize)
		{
			throw "PrefixBytes overflow";
		}

		info.prefixBytes[info.prefixSize] = pre;
		info.prefixSize++;
	}
	
	bool IntelAssembler::IsSpecial(Param p)
	{
		switch (p)
		{
			case Param::ImmStart:
			case Param::ImmEnd:
			case Param::RegStart:
			case Param::RegEnd:
			case Param::MemStart:
			case Param::Mem16Start:
			case Param::Mem16End:
			case Param::Mem32Start:
			case Param::Mem32End:
			case Param::Mem64Start:
			case Param::Mem64End:
			case Param::SibStart:
			case Param::MemSib32Start:
			case Param::MemSib32Scale1Start:
			case Param::MemSib32Scale1End:
			case Param::MemSib32Scale2Start:
			case Param::MemSib32Scale2End:
			case Param::MemSib32Scale4Start:
			case Param::MemSib32Scale4End:
			case Param::MemSib32Scale8Start:
			case Param::MemSib32Scale8End:
			case Param::MemSib32Disp8Start:
			case Param::MemSib32Scale1Disp8Start:
			case Param::MemSib32Scale1Disp8End:
			case Param::MemSib32Scale2Disp8Start:
			case Param::MemSib32Scale2Disp8End:
			case Param::MemSib32Scale4Disp8Start:
			case Param::MemSib32Scale4Disp8End:
			case Param::MemSib32Scale8Disp8Start:
			case Param::MemSib32Scale8Disp8End:
			case Param::MemSib32Disp8End:
			case Param::MemSib32Disp32Start:
			case Param::MemSib32Scale1Disp32Start:
			case Param::MemSib32Scale1Disp32End:
			case Param::MemSib32Scale2Disp32Start:
			case Param::MemSib32Scale2Disp32End:
			case Param::MemSib32Scale4Disp32Start:
			case Param::MemSib32Scale4Disp32End:
			case Param::MemSib32Scale8Disp32Start:
			case Param::MemSib32Scale8Disp32End:
			case Param::MemSib32Disp32End:
			case Param::MemSib32End:
			case Param::MemSib64Start:
			case Param::MemSib64Scale1Start:
			case Param::MemSib64Scale1End:
			case Param::MemSib64Scale2Start:
			case Param::MemSib64Scale2End:
			case Param::MemSib64Scale4Start:
			case Param::MemSib64Scale4End:
			case Param::MemSib64Scale8Start:
			case Param::MemSib64Scale8End:
			case Param::MemSib64Disp8Start:
			case Param::MemSib64Scale1Disp8Start:
			case Param::MemSib64Scale1Disp8End:
			case Param::MemSib64Scale2Disp8Start:
			case Param::MemSib64Scale2Disp8End:
			case Param::MemSib64Scale4Disp8Start:
			case Param::MemSib64Scale4Disp8End:
			case Param::MemSib64Scale8Disp8Start:
			case Param::MemSib64Scale8Disp8End:
			case Param::MemSib64Disp8End:
			case Param::MemSib64Disp32Start:
			case Param::MemSib64Scale1Disp32Start:
			case Param::MemSib64Scale1Disp32End:
			case Param::MemSib64Scale2Disp32Start:
			case Param::MemSib64Scale2Disp32End:
			case Param::MemSib64Scale4Disp32Start:
			case Param::MemSib64Scale4Disp32End:
			case Param::MemSib64Scale8Disp32Start:
			case Param::MemSib64Scale8Disp32End:
			case Param::MemSib64Disp32End:
			case Param::MemSib64End:
			case Param::SibEnd:
			case Param::MemEnd:
				return true;
		}

		return false;
	}

	bool IntelAssembler::IsImm(Param p)
	{
		return Param::ImmStart <= p && p <= Param::ImmEnd;
	}

	bool IntelAssembler::IsReg(Param p)
	{
		return Param::RegStart <= p && p <= Param::RegEnd;
	}

	bool IntelAssembler::IsReg8(Param p)
	{
		switch (p)
		{
			case Param::al: case Param::cl: case Param::dl: case Param::bl: case Param::ah: case Param::ch: case Param::dh: case Param::bh:
			case Param::spl: case Param::bpl: case Param::sil: case Param::dil:
			case Param::r8b: case Param::r9b: case Param::r10b: case Param::r11b: case Param::r12b: case Param::r13b: case Param::r14b: case Param::r15b:
				return true;
		}

		return false;
	}

	bool IntelAssembler::IsReg16(Param p)
	{
		switch (p)
		{
			case Param::ax: case Param::cx: case Param::dx: case Param::bx: case Param::sp: case Param::bp: case Param::si: case Param::di:
			case Param::r8w: case Param::r9w: case Param::r10w: case Param::r11w: case Param::r12w: case Param::r13w: case Param::r14w: case Param::r15w:
				return true;
		}

		return false;
	}

	bool IntelAssembler::IsReg32(Param p)
	{
		switch (p)
		{
			case Param::eax: case Param::ecx: case Param::edx: case Param::ebx: case Param::esp: case Param::ebp: case Param::esi: case Param::edi:
			case Param::r8d: case Param::r9d: case Param::r10d: case Param::r11d: case Param::r12d: case Param::r13d: case Param::r14d: case Param::r15d:
				return true;
		}

		return false;
	}

	bool IntelAssembler::IsReg64(Param p)
	{
		switch (p)
		{
			case Param::rax: case Param::rcx: case Param::rdx: case Param::rbx: case Param::rsp: case Param::rbp: case Param::rsi: case Param::rdi:
			case Param::r8: case Param::r9: case Param::r10: case Param::r11: case Param::r12: case Param::r13: case Param::r14: case Param::r15:
				return true;
		}

		return false;
	}

	bool IntelAssembler::IsMem(Param p)
	{
		return Param::MemStart <= p && p <= Param::MemEnd;
	}

	bool IntelAssembler::IsSib(Param p)
	{
		return Param::SibStart <= p && p <= Param::SibEnd;
	}

	bool IntelAssembler::IsMemDisp8(Param p)
	{
		if (Param::MemSib32Disp8Start <= p && p <= Param::MemSib32Disp8End) return true;
		if (Param::MemSib64Disp8Start <= p && p <= Param::MemSib64Disp8End) return true;

		switch (p)
		{
			case Param::m_bx_si_disp8: case Param::m_bx_di_disp8: case Param::m_bp_si_disp8: case Param::m_bp_di_disp8: case Param::m_si_disp8: case Param::m_di_disp8: case Param::m_bp_disp8: case Param::m_bx_disp8:
			case Param::m_eax_disp8: case Param::m_ecx_disp8: case Param::m_edx_disp8: case Param::m_ebx_disp8: case Param::m_ebp_disp8: case Param::m_esi_disp8: case Param::m_edi_disp8:
			case Param::m_rax_disp8: case Param::m_rcx_disp8: case Param::m_rdx_disp8: case Param::m_rbx_disp8: case Param::m_rbp_disp8: case Param::m_rsi_disp8: case Param::m_rdi_disp8:

				return true;
		}

		return false;
	}

	bool IntelAssembler::IsMemDisp16(Param p)
	{
		switch (p)
		{
			case Param::m_disp16:
			case Param::m_bx_si_disp16: case Param::m_bx_di_disp16: case Param::m_bp_si_disp16: case Param::m_bp_di_disp16: case Param::m_si_disp16: case Param::m_di_disp16: case Param::m_bp_disp16: case Param::m_bx_disp16:
				return true;
		}

		return false;
	}

	bool IntelAssembler::IsMemDisp32(Param p)
	{
		if (Param::MemSib32Disp32Start <= p && p <= Param::MemSib32Disp32End) return true;
		if (Param::MemSib64Disp32Start <= p && p <= Param::MemSib64Disp32End) return true;

		switch (p)
		{
			case Param::m_disp32: case Param::m_eax_disp32: case Param::m_ecx_disp32: case Param::m_edx_disp32: case Param::m_ebx_disp32: case Param::m_ebp_disp32: case Param::m_esi_disp32: case Param::m_edi_disp32:
			case Param::m_rip_disp32: case Param::m_eip_disp32:
			case Param::m_rax_disp32: case Param::m_rcx_disp32: case Param::m_rdx_disp32: case Param::m_rbx_disp32: case Param::m_rbp_disp32: case Param::m_rsi_disp32: case Param::m_rdi_disp32:

			case Param::sib_eax_disp32: case Param::sib_ecx_disp32: case Param::sib_edx_disp32: case Param::sib_ebx_disp32: case Param::sib_none_disp32: case Param::sib_ebp_disp32: case Param::sib_esi_disp32: case Param::sib_edi_disp32: case Param::sib_r8d_disp32: case Param::sib_r9d_disp32: case Param::sib_r10d_disp32: case Param::sib_r11d_disp32: case Param::sib_r12d_disp32: case Param::sib_r13d_disp32: case Param::sib_r14d_disp32: case Param::sib_r15d_disp32:
			case Param::sib_eax_2_disp32: case Param::sib_ecx_2_disp32: case Param::sib_edx_2_disp32: case Param::sib_ebx_2_disp32: case Param::sib_none_2_disp32: case Param::sib_ebp_2_disp32: case Param::sib_esi_2_disp32: case Param::sib_edi_2_disp32: case Param::sib_r8d_2_disp32: case Param::sib_r9d_2_disp32: case Param::sib_r10d_2_disp32: case Param::sib_r11d_2_disp32: case Param::sib_r12d_2_disp32: case Param::sib_r13d_2_disp32: case Param::sib_r14d_2_disp32: case Param::sib_r15d_2_disp32:
			case Param::sib_eax_4_disp32: case Param::sib_ecx_4_disp32: case Param::sib_edx_4_disp32: case Param::sib_ebx_4_disp32: case Param::sib_none_4_disp32: case Param::sib_ebp_4_disp32: case Param::sib_esi_4_disp32: case Param::sib_edi_4_disp32: case Param::sib_r8d_4_disp32: case Param::sib_r9d_4_disp32: case Param::sib_r10d_4_disp32: case Param::sib_r11d_4_disp32: case Param::sib_r12d_4_disp32: case Param::sib_r13d_4_disp32: case Param::sib_r14d_4_disp32: case Param::sib_r15d_4_disp32:
			case Param::sib_eax_8_disp32: case Param::sib_ecx_8_disp32: case Param::sib_edx_8_disp32: case Param::sib_ebx_8_disp32: case Param::sib_none_8_disp32: case Param::sib_ebp_8_disp32: case Param::sib_esi_8_disp32: case Param::sib_edi_8_disp32: case Param::sib_r8d_8_disp32: case Param::sib_r9d_8_disp32: case Param::sib_r10d_8_disp32: case Param::sib_r11d_8_disp32: case Param::sib_r12d_8_disp32: case Param::sib_r13d_8_disp32: case Param::sib_r14d_8_disp32: case Param::sib_r15d_8_disp32:

			case Param::sib_rax_disp32: case Param::sib_rcx_disp32: case Param::sib_rdx_disp32: case Param::sib_rbx_disp32: case Param::sib_none_disp32_64: case Param::sib_rbp_disp32: case Param::sib_rsi_disp32: case Param::sib_rdi_disp32: case Param::sib_r8_disp32: case Param::sib_r9_disp32: case Param::sib_r10_disp32: case Param::sib_r11_disp32: case Param::sib_r12_disp32: case Param::sib_r13_disp32: case Param::sib_r14_disp32: case Param::sib_r15_disp32:
			case Param::sib_rax_2_disp32: case Param::sib_rcx_2_disp32: case Param::sib_rdx_2_disp32: case Param::sib_rbx_2_disp32: case Param::sib_none_2_disp32_64: case Param::sib_rbp_2_disp32: case Param::sib_rsi_2_disp32: case Param::sib_rdi_2_disp32: case Param::sib_r8_2_disp32: case Param::sib_r9_2_disp32: case Param::sib_r10_2_disp32: case Param::sib_r11_2_disp32: case Param::sib_r12_2_disp32: case Param::sib_r13_2_disp32: case Param::sib_r14_2_disp32: case Param::sib_r15_2_disp32:
			case Param::sib_rax_4_disp32: case Param::sib_rcx_4_disp32: case Param::sib_rdx_4_disp32: case Param::sib_rbx_4_disp32: case Param::sib_none_4_disp32_64: case Param::sib_rbp_4_disp32: case Param::sib_rsi_4_disp32: case Param::sib_rdi_4_disp32: case Param::sib_r8_4_disp32: case Param::sib_r9_4_disp32: case Param::sib_r10_4_disp32: case Param::sib_r11_4_disp32: case Param::sib_r12_4_disp32: case Param::sib_r13_4_disp32: case Param::sib_r14_4_disp32: case Param::sib_r15_4_disp32:
			case Param::sib_rax_8_disp32: case Param::sib_rcx_8_disp32: case Param::sib_rdx_8_disp32: case Param::sib_rbx_8_disp32: case Param::sib_none_8_disp32_64: case Param::sib_rbp_8_disp32: case Param::sib_rsi_8_disp32: case Param::sib_rdi_8_disp32: case Param::sib_r8_8_disp32: case Param::sib_r9_8_disp32: case Param::sib_r10_8_disp32: case Param::sib_r11_8_disp32: case Param::sib_r12_8_disp32: case Param::sib_r13_8_disp32: case Param::sib_r14_8_disp32: case Param::sib_r15_8_disp32:

				return true;
		}

		return false;
	}

	bool IntelAssembler::IsMemDisp(Param p)
	{
		return IsMemDisp8(p) || IsMemDisp16(p) || IsMemDisp32(p);
	}

	/// <summary>
	/// Used to compile a prefix list into raw form (bytes).
	/// </summary>
	void IntelAssembler::AssemblePrefixes(AnalyzeInfo& info)
	{
		for (size_t i = 0; i < info.numPrefixes; i++)
		{
			uint8_t prefixByte = 0;

			switch (info.prefixes[i])
			{
				case Prefix::AddressSize: prefixByte = 0x67; break;
				case Prefix::Lock: prefixByte = 0xf0; break;
				case Prefix::OperandSize: prefixByte = 0x66; break;
				case Prefix::SegCs: prefixByte = 0x2e; break;
				case Prefix::SegDs: prefixByte = 0x3e; break;
				case Prefix::SegEs: prefixByte = 0x26; break;
				case Prefix::SegFs: prefixByte = 0x64; break;
				case Prefix::SegGs: prefixByte = 0x65; break;
				case Prefix::SegSs: prefixByte = 0x36; break;
				case Prefix::Rep: prefixByte = 0xf3; break;
				case Prefix::Repne: prefixByte = 0xf2; break;

				default:
					throw "Invalid prefix";
			}

			AddPrefixByte(info, prefixByte);
		}
	}

	void IntelAssembler::GetReg(Param p, size_t& reg)
	{
		switch (p)
		{
			case Param::al: case Param::ax: case Param::eax: case Param::rax: reg = 0; break;
			case Param::cl: case Param::cx: case Param::ecx: case Param::rcx: reg = 1; break;
			case Param::dl: case Param::dx: case Param::edx: case Param::rdx: reg = 2; break;
			case Param::bl: case Param::bx: case Param::ebx: case Param::rbx: reg = 3; break;
			case Param::ah: case Param::sp: case Param::esp: case Param::rsp: reg = 4; break;
			case Param::ch: case Param::bp: case Param::ebp: case Param::rbp: reg = 5; break;
			case Param::dh: case Param::si: case Param::esi: case Param::rsi: reg = 6; break;
			case Param::bh: case Param::di: case Param::edi: case Param::rdi: reg = 7; break;

			case Param::spl: reg = 4; break;
			case Param::bpl: reg = 5; break;
			case Param::sil: reg = 6; break;
			case Param::dil: reg = 7; break;

			case Param::r8b: case Param::r8w: case Param::r8d: case Param::r8: reg = 8; break;
			case Param::r9b: case Param::r9w: case Param::r9d: case Param::r9: reg = 9; break;
			case Param::r10b: case Param::r10w: case Param::r10d: case Param::r10: reg = 10; break;
			case Param::r11b: case Param::r11w: case Param::r11d: case Param::r11: reg = 11; break;
			case Param::r12b: case Param::r12w: case Param::r12d: case Param::r12: reg = 12; break;
			case Param::r13b: case Param::r13w: case Param::r13d: case Param::r13: reg = 13; break;
			case Param::r14b: case Param::r14w: case Param::r14d: case Param::r14: reg = 14; break;
			case Param::r15b: case Param::r15w: case Param::r15d: case Param::r15: reg = 15; break;

			default:
				throw "Invalid parameter";
		}
	}

	void IntelAssembler::GetMod(Param p, size_t& mod)
	{
		if (IsReg(p))
		{
			mod = 3;
			return;
		}

		if ((Param::MemSib32Scale1Start <= p && p <= Param::MemSib32Scale1End) ||
			(Param::MemSib64Scale1Start <= p && p <= Param::MemSib64Scale1End) ||
			(Param::MemSib32Scale2Start <= p && p <= Param::MemSib32Scale2End) ||
			(Param::MemSib64Scale2Start <= p && p <= Param::MemSib64Scale2End) ||
			(Param::MemSib32Scale4Start <= p && p <= Param::MemSib32Scale4End) ||
			(Param::MemSib64Scale4Start <= p && p <= Param::MemSib64Scale4End) ||
			(Param::MemSib32Scale8Start <= p && p <= Param::MemSib32Scale8End) ||
			(Param::MemSib64Scale8Start <= p && p <= Param::MemSib64Scale8End) )
		{
			mod = 0;
			return;
		}
		else if ((Param::MemSib32Scale1Disp8Start <= p && p <= Param::MemSib32Scale1Disp8End) ||
			(Param::MemSib64Scale1Disp8Start <= p && p <= Param::MemSib64Scale1Disp8End) ||
			(Param::MemSib32Scale2Disp8Start <= p && p <= Param::MemSib32Scale2Disp8End) ||
			(Param::MemSib64Scale2Disp8Start <= p && p <= Param::MemSib64Scale2Disp8End) ||
			(Param::MemSib32Scale4Disp8Start <= p && p <= Param::MemSib32Scale4Disp8End) ||
			(Param::MemSib64Scale4Disp8Start <= p && p <= Param::MemSib64Scale4Disp8End) ||
			(Param::MemSib32Scale8Disp8Start <= p && p <= Param::MemSib32Scale8Disp8End) ||
			(Param::MemSib64Scale8Disp8Start <= p && p <= Param::MemSib64Scale8Disp8End))
		{
			mod = 1;
			return;
		}
		else if ((Param::MemSib32Scale1Disp32Start <= p && p <= Param::MemSib32Scale1Disp32End) ||
			(Param::MemSib64Scale1Disp32Start <= p && p <= Param::MemSib64Scale1Disp32End) ||
			(Param::MemSib32Scale2Disp32Start <= p && p <= Param::MemSib32Scale2Disp32End) ||
			(Param::MemSib64Scale2Disp32Start <= p && p <= Param::MemSib64Scale2Disp32End) ||
			(Param::MemSib32Scale4Disp32Start <= p && p <= Param::MemSib32Scale4Disp32End) ||
			(Param::MemSib64Scale4Disp32Start <= p && p <= Param::MemSib64Scale4Disp32End) ||
			(Param::MemSib32Scale8Disp32Start <= p && p <= Param::MemSib32Scale8Disp32End) ||
			(Param::MemSib64Scale8Disp32Start <= p && p <= Param::MemSib64Scale8Disp32End))
		{
			mod = 2;
			return;
		}

		switch (p)
		{
			case Param::m_bx_si: case Param::m_bx_di: case Param::m_bp_si: case Param::m_bp_di: case Param::m_si: case Param::m_di: case Param::m_disp16: case Param::m_bx:
			case Param::m_eax: case Param::m_ecx: case Param::m_edx: case Param::m_ebx: case Param::m_disp32: case Param::m_esi: case Param::m_edi:
			case Param::m_rax: case Param::m_rcx: case Param::m_rdx: case Param::m_rbx: case Param::m_rip_disp32: case Param::m_eip_disp32: case Param::m_rsi: case Param::m_rdi:
				mod = 0;
				break;

			case Param::m_bx_si_disp8: case Param::m_bx_di_disp8: case Param::m_bp_si_disp8: case Param::m_bp_di_disp8: case Param::m_si_disp8: case Param::m_di_disp8: case Param::m_bp_disp8: case Param::m_bx_disp8:
			case Param::m_eax_disp8: case Param::m_ecx_disp8: case Param::m_edx_disp8: case Param::m_ebx_disp8: case Param::m_ebp_disp8: case Param::m_esi_disp8: case Param::m_edi_disp8:
			case Param::m_rax_disp8: case Param::m_rcx_disp8: case Param::m_rdx_disp8: case Param::m_rbx_disp8: case Param::m_rbp_disp8: case Param::m_rsi_disp8: case Param::m_rdi_disp8:
				mod = 1;
				break;

			case Param::m_bx_si_disp16: case Param::m_bx_di_disp16: case Param::m_bp_si_disp16: case Param::m_bp_di_disp16: case Param::m_si_disp16: case Param::m_di_disp16: case Param::m_bp_disp16: case Param::m_bx_disp16:
			case Param::m_eax_disp32: case Param::m_ecx_disp32: case Param::m_edx_disp32: case Param::m_ebx_disp32: case Param::m_ebp_disp32: case Param::m_esi_disp32: case Param::m_edi_disp32:
			case Param::m_rax_disp32: case Param::m_rcx_disp32: case Param::m_rdx_disp32: case Param::m_rbx_disp32: case Param::m_rbp_disp32: case Param::m_rsi_disp32: case Param::m_rdi_disp32:
				mod = 2;
				break;

			default:
				throw "Invalid parameter";
		}
	}

	void IntelAssembler::GetRm(Param p, size_t& rm)
	{
		if (IsReg(p))
		{
			GetReg(p, rm);
			return;
		}
		else if (IsSib(p))
		{
			rm = 4;
			return;
		}

		switch (p)
		{
			case Param::m_bx_si: case Param::m_bx_si_disp8: case Param::m_bx_si_disp16: case Param::m_eax: case Param::m_eax_disp8: case Param::m_eax_disp32: case Param::m_rax: case Param::m_rax_disp8: case Param::m_rax_disp32:
				rm = 0;
				break;
				
			case Param::m_bx_di: case Param::m_bx_di_disp8: case Param::m_bx_di_disp16: case Param::m_ecx: case Param::m_ecx_disp8: case Param::m_ecx_disp32: case Param::m_rcx: case Param::m_rcx_disp8: case Param::m_rcx_disp32:
				rm = 1;
				break;

			case Param::m_bp_si: case Param::m_bp_si_disp8: case Param::m_bp_si_disp16: case Param::m_edx: case Param::m_edx_disp8: case Param::m_edx_disp32: case Param::m_rdx: case Param::m_rdx_disp8: case Param::m_rdx_disp32:
				rm = 2;
				break;

			case Param::m_bp_di: case Param::m_bp_di_disp8: case Param::m_bp_di_disp16: case Param::m_ebx: case Param::m_ebx_disp8: case Param::m_ebx_disp32: case Param::m_rbx: case Param::m_rbx_disp8: case Param::m_rbx_disp32:
				rm = 3;
				break;

			case Param::m_si: case Param::m_si_disp8: case Param::m_si_disp16:
				rm = 4;
				break;

			case Param::m_di: case Param::m_di_disp8: case Param::m_di_disp16: case Param::m_disp32: case Param::m_ebp_disp8: case Param::m_ebp_disp32: case Param::m_rip_disp32: case Param::m_eip_disp32: case Param::m_rbp_disp8: case Param::m_rbp_disp32:
				rm = 5;
				break;

			case Param::m_disp16: case Param::m_bp_disp8: case Param::m_bp_disp16: case Param::m_esi: case Param::m_esi_disp8: case Param::m_esi_disp32: case Param::m_rsi: case Param::m_rsi_disp8: case Param::m_rsi_disp32:
				rm = 6;
				break;

			case Param::m_bx: case Param::m_bx_disp8: case Param::m_bx_disp16: case Param::m_edi: case Param::m_edi_disp8: case Param::m_edi_disp32: case Param::m_rdi: case Param::m_rdi_disp8: case Param::m_rdi_disp32:
				rm = 7;
				break;

			default:
				throw "Invalid parameter";
		}
	}

	void IntelAssembler::GetSS(Param p, size_t& scale)
	{
		if ( (Param::MemSib32Scale1Start <= p && p <= Param::MemSib32Scale1End) || 
			(Param::MemSib32Scale1Disp8Start <= p && p <= Param::MemSib32Scale1Disp8End) ||
			(Param::MemSib32Scale1Disp32Start <= p && p <= Param::MemSib32Scale1Disp32End) ||
			(Param::MemSib64Scale1Start <= p && p <= Param::MemSib64Scale1End) ||
			(Param::MemSib64Scale1Disp8Start <= p && p <= Param::MemSib64Scale1Disp8End) ||
			(Param::MemSib64Scale1Disp32Start <= p && p <= Param::MemSib64Scale1Disp32End) )
		{
			scale = 0;
		}
		else if ((Param::MemSib32Scale2Start <= p && p <= Param::MemSib32Scale2End) ||
			(Param::MemSib32Scale2Disp8Start <= p && p <= Param::MemSib32Scale2Disp8End) ||
			(Param::MemSib32Scale2Disp32Start <= p && p <= Param::MemSib32Scale2Disp32End) ||
			(Param::MemSib64Scale2Start <= p && p <= Param::MemSib64Scale2End) ||
			(Param::MemSib64Scale2Disp8Start <= p && p <= Param::MemSib64Scale2Disp8End) ||
			(Param::MemSib64Scale2Disp32Start <= p && p <= Param::MemSib64Scale2Disp32End))
		{
			scale = 1;
		}
		else if ((Param::MemSib32Scale4Start <= p && p <= Param::MemSib32Scale4End) ||
			(Param::MemSib32Scale4Disp8Start <= p && p <= Param::MemSib32Scale4Disp8End) ||
			(Param::MemSib32Scale4Disp32Start <= p && p <= Param::MemSib32Scale4Disp32End) ||
			(Param::MemSib64Scale4Start <= p && p <= Param::MemSib64Scale4End) ||
			(Param::MemSib64Scale4Disp8Start <= p && p <= Param::MemSib64Scale4Disp8End) ||
			(Param::MemSib64Scale4Disp32Start <= p && p <= Param::MemSib64Scale4Disp32End))
		{
			scale = 2;
		}
		else if ((Param::MemSib32Scale8Start <= p && p <= Param::MemSib32Scale8End) ||
			(Param::MemSib32Scale8Disp8Start <= p && p <= Param::MemSib32Scale8Disp8End) ||
			(Param::MemSib32Scale8Disp32Start <= p && p <= Param::MemSib32Scale8Disp32End) ||
			(Param::MemSib64Scale8Start <= p && p <= Param::MemSib64Scale8End) ||
			(Param::MemSib64Scale8Disp8Start <= p && p <= Param::MemSib64Scale8Disp8End) ||
			(Param::MemSib64Scale8Disp32Start <= p && p <= Param::MemSib64Scale8Disp32End))
		{
			scale = 3;
		}
		else
		{
			throw "Invalid parameter";
		}
	}

	void IntelAssembler::GetIndex(Param p, size_t& index)
	{
		if (Param::MemSib32Scale1Start < p && p < Param::MemSib32Scale1End)
		{
			index = ((size_t)p - ((size_t)Param::MemSib32Scale1Start + 1)) / 16;
		}
		else if (Param::MemSib32Scale2Start < p && p < Param::MemSib32Scale2End)
		{
			index = ((size_t)p - ((size_t)Param::MemSib32Scale2Start + 1)) / 16;
		}
		else if (Param::MemSib32Scale4Start < p && p < Param::MemSib32Scale4End)
		{
			index = ((size_t)p - ((size_t)Param::MemSib32Scale4Start + 1)) / 16;
		}
		else if (Param::MemSib32Scale8Start < p && p < Param::MemSib32Scale8End)
		{
			index = ((size_t)p - ((size_t)Param::MemSib32Scale8Start + 1)) / 16;
		}
		else if (Param::MemSib32Scale1Disp8Start < p && p < Param::MemSib32Scale1Disp8End)
		{
			index = ((size_t)p - ((size_t)Param::MemSib32Scale1Disp8Start + 1)) / 16;
		}
		else if (Param::MemSib32Scale2Disp8Start < p && p < Param::MemSib32Scale2Disp8End)
		{
			index = ((size_t)p - ((size_t)Param::MemSib32Scale2Disp8Start + 1)) / 16;
		}
		else if (Param::MemSib32Scale4Disp8Start < p && p < Param::MemSib32Scale4Disp8End)
		{
			index = ((size_t)p - ((size_t)Param::MemSib32Scale4Disp8Start + 1)) / 16;
		}
		else if (Param::MemSib32Scale8Disp8Start < p && p < Param::MemSib32Scale8Disp8End)
		{
			index = ((size_t)p - ((size_t)Param::MemSib32Scale8Disp8Start + 1)) / 16;
		}
		else if (Param::MemSib32Scale1Disp32Start < p && p < Param::MemSib32Scale1Disp32End)
		{
			index = ((size_t)p - ((size_t)Param::MemSib32Scale1Disp32Start + 1)) / 16;
		}
		else if (Param::MemSib32Scale2Disp32Start < p && p < Param::MemSib32Scale2Disp32End)
		{
			index = ((size_t)p - ((size_t)Param::MemSib32Scale2Disp32Start + 1)) / 16;
		}
		else if (Param::MemSib32Scale4Disp32Start < p && p < Param::MemSib32Scale4Disp32End)
		{
			index = ((size_t)p - ((size_t)Param::MemSib32Scale4Disp32Start + 1)) / 16;
		}
		else if (Param::MemSib32Scale8Disp32Start < p && p < Param::MemSib32Scale8Disp32End)
		{
			index = ((size_t)p - ((size_t)Param::MemSib32Scale8Disp32Start + 1)) / 16;
		}

		else if (Param::MemSib64Scale1Start < p && p < Param::MemSib64Scale1End)
		{
			index = ((size_t)p - ((size_t)Param::MemSib64Scale1Start + 1)) / 16;
		}
		else if (Param::MemSib64Scale2Start < p && p < Param::MemSib64Scale2End)
		{
			index = ((size_t)p - ((size_t)Param::MemSib64Scale2Start + 1)) / 16;
		}
		else if (Param::MemSib64Scale4Start < p && p < Param::MemSib64Scale4End)
		{
			index = ((size_t)p - ((size_t)Param::MemSib64Scale4Start + 1)) / 16;
		}
		else if (Param::MemSib64Scale8Start < p && p < Param::MemSib64Scale8End)
		{
			index = ((size_t)p - ((size_t)Param::MemSib64Scale8Start + 1)) / 16;
		}
		else if (Param::MemSib64Scale1Disp8Start < p && p < Param::MemSib64Scale1Disp8End)
		{
			index = ((size_t)p - ((size_t)Param::MemSib64Scale1Disp8Start + 1)) / 16;
		}
		else if (Param::MemSib64Scale2Disp8Start < p && p < Param::MemSib64Scale2Disp8End)
		{
			index = ((size_t)p - ((size_t)Param::MemSib64Scale2Disp8Start + 1)) / 16;
		}
		else if (Param::MemSib64Scale4Disp8Start < p && p < Param::MemSib64Scale4Disp8End)
		{
			index = ((size_t)p - ((size_t)Param::MemSib64Scale4Disp8Start + 1)) / 16;
		}
		else if (Param::MemSib64Scale8Disp8Start < p && p < Param::MemSib64Scale8Disp8End)
		{
			index = ((size_t)p - ((size_t)Param::MemSib64Scale8Disp8Start + 1)) / 16;
		}
		else if (Param::MemSib64Scale1Disp32Start < p && p < Param::MemSib64Scale1Disp32End)
		{
			index = ((size_t)p - ((size_t)Param::MemSib64Scale1Disp32Start + 1)) / 16;
		}
		else if (Param::MemSib64Scale2Disp32Start < p && p < Param::MemSib64Scale2Disp32End)
		{
			index = ((size_t)p - ((size_t)Param::MemSib64Scale2Disp32Start + 1)) / 16;
		}
		else if (Param::MemSib64Scale4Disp32Start < p && p < Param::MemSib64Scale4Disp32End)
		{
			index = ((size_t)p - ((size_t)Param::MemSib64Scale4Disp32Start + 1)) / 16;
		}
		else if (Param::MemSib64Scale8Disp32Start < p && p < Param::MemSib64Scale8Disp32End)
		{
			index = ((size_t)p - ((size_t)Param::MemSib64Scale8Disp32Start + 1)) / 16;
		}

		else
		{
			throw "Invalid parameters";
		}
	}

	void IntelAssembler::GetBase(Param p, size_t& base)
	{
		if (Param::MemSib32Scale1Start < p && p < Param::MemSib32Scale1End)
		{
			base = ((size_t)p - ((size_t)Param::MemSib32Scale1Start + 1)) % 16;
		}
		else if (Param::MemSib32Scale2Start < p && p < Param::MemSib32Scale2End)
		{
			base = ((size_t)p - ((size_t)Param::MemSib32Scale2Start + 1)) % 16;
		}
		else if (Param::MemSib32Scale4Start < p && p < Param::MemSib32Scale4End)
		{
			base = ((size_t)p - ((size_t)Param::MemSib32Scale4Start + 1)) % 16;
		}
		else if (Param::MemSib32Scale8Start < p && p < Param::MemSib32Scale8End)
		{
			base = ((size_t)p - ((size_t)Param::MemSib32Scale8Start + 1)) % 16;
		}
		else if (Param::MemSib32Scale1Disp8Start < p && p < Param::MemSib32Scale1Disp8End)
		{
			base = ((size_t)p - ((size_t)Param::MemSib32Scale1Disp8Start + 1)) % 16;
		}
		else if (Param::MemSib32Scale2Disp8Start < p && p < Param::MemSib32Scale2Disp8End)
		{
			base = ((size_t)p - ((size_t)Param::MemSib32Scale2Disp8Start + 1)) % 16;
		}
		else if (Param::MemSib32Scale4Disp8Start < p && p < Param::MemSib32Scale4Disp8End)
		{
			base = ((size_t)p - ((size_t)Param::MemSib32Scale4Disp8Start + 1)) % 16;
		}
		else if (Param::MemSib32Scale8Disp8Start < p && p < Param::MemSib32Scale8Disp8End)
		{
			base = ((size_t)p - ((size_t)Param::MemSib32Scale8Disp8Start + 1)) % 16;
		}
		else if (Param::MemSib32Scale1Disp32Start < p && p < Param::MemSib32Scale1Disp32End)
		{
			base = ((size_t)p - ((size_t)Param::MemSib32Scale1Disp32Start + 1)) % 16;
		}
		else if (Param::MemSib32Scale2Disp32Start < p && p < Param::MemSib32Scale2Disp32End)
		{
			base = ((size_t)p - ((size_t)Param::MemSib32Scale2Disp32Start + 1)) % 16;
		}
		else if (Param::MemSib32Scale4Disp32Start < p && p < Param::MemSib32Scale4Disp32End)
		{
			base = ((size_t)p - ((size_t)Param::MemSib32Scale4Disp32Start + 1)) % 16;
		}
		else if (Param::MemSib32Scale8Disp32Start < p && p < Param::MemSib32Scale8Disp32End)
		{
			base = ((size_t)p - ((size_t)Param::MemSib32Scale8Disp32Start + 1)) % 16;
		}

		else if (Param::MemSib64Scale1Start < p && p < Param::MemSib64Scale1End)
		{
			base = ((size_t)p - ((size_t)Param::MemSib64Scale1Start + 1)) % 16;
		}
		else if (Param::MemSib64Scale2Start < p && p < Param::MemSib64Scale2End)
		{
			base = ((size_t)p - ((size_t)Param::MemSib64Scale2Start + 1)) % 16;
		}
		else if (Param::MemSib64Scale4Start < p && p < Param::MemSib64Scale4End)
		{
			base = ((size_t)p - ((size_t)Param::MemSib64Scale4Start + 1)) % 16;
		}
		else if (Param::MemSib64Scale8Start < p && p < Param::MemSib64Scale8End)
		{
			base = ((size_t)p - ((size_t)Param::MemSib64Scale8Start + 1)) % 16;
		}
		else if (Param::MemSib64Scale1Disp8Start < p && p < Param::MemSib64Scale1Disp8End)
		{
			base = ((size_t)p - ((size_t)Param::MemSib64Scale1Disp8Start + 1)) % 16;
		}
		else if (Param::MemSib64Scale2Disp8Start < p && p < Param::MemSib64Scale2Disp8End)
		{
			base = ((size_t)p - ((size_t)Param::MemSib64Scale2Disp8Start + 1)) % 16;
		}
		else if (Param::MemSib64Scale4Disp8Start < p && p < Param::MemSib64Scale4Disp8End)
		{
			base = ((size_t)p - ((size_t)Param::MemSib64Scale4Disp8Start + 1)) % 16;
		}
		else if (Param::MemSib64Scale8Disp8Start < p && p < Param::MemSib64Scale8Disp8End)
		{
			base = ((size_t)p - ((size_t)Param::MemSib64Scale8Disp8Start + 1)) % 16;
		}
		else if (Param::MemSib64Scale1Disp32Start < p && p < Param::MemSib64Scale1Disp32End)
		{
			base = ((size_t)p - ((size_t)Param::MemSib64Scale1Disp32Start + 1)) % 16;
		}
		else if (Param::MemSib64Scale2Disp32Start < p && p < Param::MemSib64Scale2Disp32End)
		{
			base = ((size_t)p - ((size_t)Param::MemSib64Scale2Disp32Start + 1)) % 16;
		}
		else if (Param::MemSib64Scale4Disp32Start < p && p < Param::MemSib64Scale4Disp32End)
		{
			base = ((size_t)p - ((size_t)Param::MemSib64Scale4Disp32Start + 1)) % 16;
		}
		else if (Param::MemSib64Scale8Disp32Start < p && p < Param::MemSib64Scale8Disp32End)
		{
			base = ((size_t)p - ((size_t)Param::MemSib64Scale8Disp32Start + 1)) % 16;
		}

		else
		{
			throw "Invalid parameters";
		}
	}

	/// <summary>
	/// It is engaged in the processing of the instruction format based on the specified processor operating mode (16, 32, 64)
	/// and the structure with feature data supported by the instruction. 
	/// </summary>
	/// <param name="info">Instruction information</param>
	/// <param name="bits">Processor operating mode (16, 32, 64)</param>
	/// <param name="feature">Instruction features</param>
	void IntelAssembler::ProcessGpInstr(AnalyzeInfo& info, size_t bits, InstrFeatures& feature)
	{
		size_t mod = 0, reg = 0, rm = 0;
		size_t scale = 0, index = 0, base = 0;
		bool sib = false;

		if (info.numParams != 2)
		{
			throw "Invalid parameters";
		}

		// Try all formats one by one 

		if (feature.forms & InstrForm::Form_I)
		{
			if (IsReg(info.params[0]) && IsImm(info.params[1]))
			{
				switch (info.params[0])
				{
					case Param::al:
						OneByte(info, feature.Form_I_Opcode8);
						OneByte(info, info.Imm.uimm8);
						return;
					case Param::ax:
						if (bits != 16)
						{
							AddPrefixByte(info, 0x66);
						}
						OneByte(info, feature.Form_I_Opcode16_64);
						AddUshort(info, info.Imm.uimm16);
						return;
					case Param::eax:
						if (bits == 16)
						{
							AddPrefixByte(info, 0x66);
						}
						OneByte(info, feature.Form_I_Opcode16_64);
						AddUlong(info, info.Imm.uimm32);
						return;
					case Param::rax:
						if (bits == 64)
						{
							OneByte(info, 0x48);
							OneByte(info, feature.Form_I_Opcode16_64);
							AddUlong(info, info.Imm.uimm32);
						}
						else
						{
							Invalid();
						}
						return;
					default:
						break;
				}
			}
		}
	
		if (feature.forms & InstrForm::Form_MI)
		{
			if ((IsReg(info.params[0]) || IsMem(info.params[0])) && IsImm(info.params[1]))
			{

			}
		}

		if (feature.forms & InstrForm::Form_MR)
		{
			if ((IsReg(info.params[0]) || IsMem(info.params[0])) && IsReg(info.params[1]))
			{
				HandleModRegRm(info, bits, 1, 0, feature.Form_MR_Opcode8, feature.Form_MR_Opcode16_64);
				return;
			}
		}

		if (feature.forms & InstrForm::Form_RM)
		{
			if (IsReg(info.params[0]) && IsMem(info.params[1]))
			{
				HandleModRegRm(info, bits, 0, 1, feature.Form_RM_Opcode8, feature.Form_RM_Opcode16_64);
				return;
			}
		}

		throw "Invalid instruction form";
	}

	void IntelAssembler::HandleModRegRm(AnalyzeInfo& info, size_t bits, size_t regParam, size_t rmParam, uint8_t opcode8, uint8_t opcode16_64)
	{
		size_t mod = 0, reg = 0, rm = 0;
		size_t scale = 0, index = 0, base = 0;

		// Extract and check required information from parameters 

		switch (info.params[regParam])
		{
			case Param::ah: case Param::ch: case Param::dh: case Param::bh:
				if (bits == 64)
				{
					Invalid();
				}
				break;

			case Param::spl: case Param::bpl: case Param::sil: case Param::dil:
				if (bits != 64)
				{
					Invalid();
				}
				break;
		}

		GetReg(info.params[regParam], reg);
		GetMod(info.params[rmParam], mod);
		GetRm(info.params[rmParam], rm);

		bool sibRequired = IsSib(info.params[rmParam]);

		if (sibRequired)
		{
			GetSS(info.params[rmParam], scale);
			GetIndex(info.params[rmParam], index);
			GetBase(info.params[rmParam], base);
		}

		// Compile the resulting instruction, mode prefixes and possible displacement

		if (IsReg32(info.params[regParam]) && bits == 16)
		{
			AddPrefixByte(info, 0x66);
		}

		if (IsReg16(info.params[regParam]) && bits != 16)
		{
			AddPrefixByte(info, 0x66);
		}

		// TODO: 0x67

		bool rexRequired = reg > 8 || rm > 8 || index > 8 || base > 8;

		if (rexRequired && bits != 64)
		{
			Invalid();
		}

		if (rexRequired)
		{
			int REX_R = reg > 8 ? 1 : 0;
			int REX_X = sibRequired ? ((index > 8) ? 1 : 0) : 0;
			int REX_B = sibRequired ? ((base > 8) ? 1 : 0) : ((rm > 8) ? 1 : 0);
			OneByte(info, 0x48 | (REX_R << 2) | (REX_X << 1) | REX_B);
		}

		OneByte(info, IsReg8(info.params[regParam]) ? opcode8 : opcode16_64 );

		uint8_t modRmByte = ((mod & 3) << 6) | ((reg & 7) << 3) | (rm & 7);
		OneByte(info, modRmByte);

		if (sibRequired)
		{
			uint8_t sibByte = ((scale & 3) << 6) | ((index & 7) << 3) | (base & 7);
			OneByte(info, sibByte);
		}

		if (IsMemDisp8(info.params[rmParam])) OneByte(info, info.Disp.disp8);
		else if (IsMemDisp16(info.params[rmParam])) AddUshort(info, info.Disp.disp16);
		else if (IsMemDisp32(info.params[rmParam])) AddUlong(info, info.Disp.disp32);
	}

#pragma endregion "Private"

#pragma region "Base methods"

	void IntelAssembler::Assemble16(AnalyzeInfo& info)
	{
		info.prefixSize = 0;
		info.instrSize = 0;

		for (size_t i = 0; i < info.numParams; i++)
		{
			if (IsSpecial(info.params[i]))
			{
				throw "Invalid parameter";
			}
		}

		AssemblePrefixes(info);

		switch (info.instr)
		{
			// Instructions using ModRM / with an immediate operand

			case Instruction::adc:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_I | InstrForm::Form_MI | InstrForm::Form_MR | InstrForm::Form_RM;
				feature.Form_RegOpcode = 2;
				feature.Form_I_Opcode8 = 0x14;
				feature.Form_I_Opcode16_64 = 0x15;
				feature.Form_MI_Opcode8 = 0x80;
				feature.Form_MI_Opcode16_64 = 0x81;
				feature.Form_MI_Opcode_Imm8 = 0x83;
				feature.Form_MR_Opcode8 = 0x10;
				feature.Form_MR_Opcode16_64 = 0x11;
				feature.Form_RM_Opcode8 = 0x12;
				feature.Form_RM_Opcode16_64 = 0x13;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			// One or more byte instructions

			case Instruction::aaa: OneByte(info, 0x37); break;
			case Instruction::aad: OneByteImm8(info, 0xd5); break;
			case Instruction::aam: OneByteImm8(info, 0xd4); break;
			case Instruction::aas: OneByte(info, 0x3f); break;

			case Instruction::cbw: OneByte(info, 0x98); break;
			case Instruction::cwde: AddPrefixByte(info, 0x66); OneByte(info, 0x98); break;
			case Instruction::cdqe: Invalid(); break;
			case Instruction::cwd: OneByte(info, 0x99); break;
			case Instruction::cdq: AddPrefixByte(info, 0x66); OneByte(info, 0x99); break;
			case Instruction::cqo: Invalid(); break;

			case Instruction::clc: OneByte(info, 0xf8); break;
			case Instruction::cld: OneByte(info, 0xfc); break;
			case Instruction::cli: OneByte(info, 0xfa); break;
			case Instruction::clts: TwoByte(info, 0x0f, 0x06); break;
			case Instruction::cmc: OneByte(info, 0xf5); break;
			case Instruction::stc: OneByte(info, 0xf9); break;
			case Instruction::std: OneByte(info, 0xfd); break;
			case Instruction::sti: OneByte(info, 0xfb); break;

			case Instruction::cpuid: TwoByte(info, 0x0f, 0xa2); break;
			case Instruction::daa: OneByte(info, 0x27); break;
			case Instruction::das: OneByte(info, 0x2f); break;
			case Instruction::hlt: OneByte(info, 0xf4); break;
			case Instruction::int3: OneByte(info, 0xcc); break;
			case Instruction::into: OneByte(info, 0xce); break;
			case Instruction::int1: OneByte(info, 0xf1); break;
			case Instruction::invd: TwoByte(info, 0x0f, 0x08); break;
			case Instruction::iret: OneByte(info, 0xcf); break;
			case Instruction::iretd: AddPrefixByte(info, 0x66); OneByte(info, 0xcf); break;
			case Instruction::iretq: Invalid(); break;
			case Instruction::lahf: OneByte(info, 0x9f); break;
			case Instruction::sahf: OneByte(info, 0x9e); break;
			case Instruction::leave: OneByte(info, 0xc9); break;
			case Instruction::nop: OneByte(info, 0x90); break;
			case Instruction::rdmsr: TwoByte(info, 0x0f, 0x32); break;
			case Instruction::rdpmc: TwoByte(info, 0x0f, 0x33); break;
			case Instruction::rdtsc: TwoByte(info, 0x0f, 0x31); break;
			case Instruction::rdtscp: TriByte(info, 0x0f, 0x01, 0xf9); break;
			case Instruction::rsm: TwoByte(info, 0x0f, 0xaa); break;
			case Instruction::swapgs: Invalid(); break;
			case Instruction::syscall: Invalid(); break;
			case Instruction::sysret: Invalid(); break;
			case Instruction::sysretq: Invalid(); break;
			case Instruction::ud2: TwoByte(info, 0x0f, 0x0b); break;
			case Instruction::wait: OneByte(info, 0x9b); break;
			case Instruction::wbinvd: TwoByte(info, 0x0f, 0x09); break;
			case Instruction::wrmsr: TwoByte(info, 0x0f, 0x30); break;
			case Instruction::xlatb: OneByte(info, 0xd7); break;

			case Instruction::popa: OneByte(info, 0x61); break;
			case Instruction::popad: AddPrefixByte(info, 0x66); OneByte(info, 0x61); break;
			case Instruction::popf: OneByte(info, 0x9d); break;
			case Instruction::popfd: AddPrefixByte(info, 0x66); OneByte(info, 0x9d); break;
			case Instruction::popfq: Invalid(); break;
			case Instruction::pusha: OneByte(info, 0x60); break;
			case Instruction::pushad: AddPrefixByte(info, 0x66); OneByte(info, 0x60); break;
			case Instruction::pushf: OneByte(info, 0x9c); break;
			case Instruction::pushfd: AddPrefixByte(info, 0x66); OneByte(info, 0x9c); break;
			case Instruction::pushfq: Invalid(); break;

			case Instruction::cmpsb: OneByte(info, 0xa6); break;
			case Instruction::cmpsw: OneByte(info, 0xa7); break;
			case Instruction::cmpsd: AddPrefixByte(info, 0x66); OneByte(info, 0xa7); break;
			case Instruction::cmpsq: Invalid(); break;
			case Instruction::lodsb: OneByte(info, 0xac); break;
			case Instruction::lodsw: OneByte(info, 0xad); break;
			case Instruction::lodsd: AddPrefixByte(info, 0x66); OneByte(info, 0xad); break;
			case Instruction::lodsq: Invalid(); break;
			case Instruction::movsb: OneByte(info, 0xa4); break;
			case Instruction::movsw: OneByte(info, 0xa5); break;
			case Instruction::movsd: AddPrefixByte(info, 0x66); OneByte(info, 0xa5); break;
			case Instruction::movsq: Invalid(); break;
			case Instruction::scasb: OneByte(info, 0xae); break;
			case Instruction::scasw: OneByte(info, 0xaf); break;
			case Instruction::scasd: AddPrefixByte(info, 0x66); OneByte(info, 0xaf); break;
			case Instruction::scasq: Invalid(); break;
			case Instruction::stosb: OneByte(info, 0xaa); break;
			case Instruction::stosw: OneByte(info, 0xab); break;
			case Instruction::stosd: AddPrefixByte(info, 0x66); OneByte(info, 0xab); break;
			case Instruction::stosq: Invalid(); break;
			case Instruction::insb: OneByte(info, 0x6c); break;
			case Instruction::insw: OneByte(info, 0x6d); break;
			case Instruction::insd: AddPrefixByte(info, 0x66); OneByte(info, 0x6d); break;
			case Instruction::outsb: OneByte(info, 0x6e); break;
			case Instruction::outsw: OneByte(info, 0x6f); break;
			case Instruction::outsd: AddPrefixByte(info, 0x66); OneByte(info, 0x6f); break;
		}
	}

	void IntelAssembler::Assemble32(AnalyzeInfo& info)
	{
		info.prefixSize = 0;
		info.instrSize = 0;

		for (size_t i = 0; i < info.numParams; i++)
		{
			if (IsSpecial(info.params[i]))
			{
				throw "Invalid parameter";
			}
		}

		AssemblePrefixes(info);

		switch (info.instr)
		{
			// Instructions using ModRM / with an immediate operand

			case Instruction::adc:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_I | InstrForm::Form_MI | InstrForm::Form_MR | InstrForm::Form_RM;
				feature.Form_RegOpcode = 2;
				feature.Form_I_Opcode8 = 0x14;
				feature.Form_I_Opcode16_64 = 0x15;
				feature.Form_MI_Opcode8 = 0x80;
				feature.Form_MI_Opcode16_64 = 0x81;
				feature.Form_MI_Opcode_Imm8 = 0x83;
				feature.Form_MR_Opcode8 = 0x10;
				feature.Form_MR_Opcode16_64 = 0x11;
				feature.Form_RM_Opcode8 = 0x12;
				feature.Form_RM_Opcode16_64 = 0x13;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			// One or more byte instructions

			case Instruction::aaa: OneByte(info, 0x37); break;
			case Instruction::aad: OneByteImm8(info, 0xd5); break;
			case Instruction::aam: OneByteImm8(info, 0xd4); break;
			case Instruction::aas: OneByte(info, 0x3f); break;

			case Instruction::cbw: AddPrefixByte(info, 0x66); OneByte(info, 0x98); break;
			case Instruction::cwde: OneByte(info, 0x98); break;
			case Instruction::cdqe: Invalid(); break;
			case Instruction::cwd: AddPrefixByte(info, 0x66); OneByte(info, 0x99); break;
			case Instruction::cdq: OneByte(info, 0x99); break;
			case Instruction::cqo: Invalid(); break;

			case Instruction::clc: OneByte(info, 0xf8); break;
			case Instruction::cld: OneByte(info, 0xfc); break;
			case Instruction::cli: OneByte(info, 0xfa); break;
			case Instruction::clts: TwoByte(info, 0x0f, 0x06); break;
			case Instruction::cmc: OneByte(info, 0xf5); break;
			case Instruction::stc: OneByte(info, 0xf9); break;
			case Instruction::std: OneByte(info, 0xfd); break;
			case Instruction::sti: OneByte(info, 0xfb); break;

			case Instruction::cpuid: TwoByte(info, 0x0f, 0xa2); break;
			case Instruction::daa: OneByte(info, 0x27); break;
			case Instruction::das: OneByte(info, 0x2f); break;
			case Instruction::hlt: OneByte(info, 0xf4); break;
			case Instruction::int3: OneByte(info, 0xcc); break;
			case Instruction::into: OneByte(info, 0xce); break;
			case Instruction::int1: OneByte(info, 0xf1); break;
			case Instruction::invd: TwoByte(info, 0x0f, 0x08); break;
			case Instruction::iret: AddPrefixByte(info, 0x66); OneByte(info, 0xcf); break;
			case Instruction::iretd: OneByte(info, 0xcf); break;
			case Instruction::iretq: Invalid(); break;
			case Instruction::lahf: OneByte(info, 0x9f); break;
			case Instruction::sahf: OneByte(info, 0x9e); break;
			case Instruction::leave: OneByte(info, 0xc9); break;
			case Instruction::nop: OneByte(info, 0x90); break;
			case Instruction::rdmsr: TwoByte(info, 0x0f, 0x32); break;
			case Instruction::rdpmc: TwoByte(info, 0x0f, 0x33); break;
			case Instruction::rdtsc: TwoByte(info, 0x0f, 0x31); break;
			case Instruction::rdtscp: TriByte(info, 0x0f, 0x01, 0xf9); break;
			case Instruction::rsm: TwoByte(info, 0x0f, 0xaa); break;
			case Instruction::swapgs: Invalid(); break;
			case Instruction::syscall: Invalid(); break;
			case Instruction::sysret: Invalid(); break;
			case Instruction::sysretq: Invalid(); break;
			case Instruction::ud2: TwoByte(info, 0x0f, 0x0b); break;
			case Instruction::wait: OneByte(info, 0x9b); break;
			case Instruction::wbinvd: TwoByte(info, 0x0f, 0x09); break;
			case Instruction::wrmsr: TwoByte(info, 0x0f, 0x30); break;
			case Instruction::xlatb: OneByte(info, 0xd7); break;

			case Instruction::popa: AddPrefixByte(info, 0x66); OneByte(info, 0x61); break;
			case Instruction::popad: OneByte(info, 0x61); break;
			case Instruction::popf: AddPrefixByte(info, 0x66); OneByte(info, 0x9d); break;
			case Instruction::popfd: OneByte(info, 0x9d); break;
			case Instruction::popfq: Invalid(); break;
			case Instruction::pusha: AddPrefixByte(info, 0x66); OneByte(info, 0x60); break;
			case Instruction::pushad: OneByte(info, 0x60); break;
			case Instruction::pushf: AddPrefixByte(info, 0x66); OneByte(info, 0x9c); break;
			case Instruction::pushfd: OneByte(info, 0x9c); break;
			case Instruction::pushfq: Invalid(); break;

			case Instruction::cmpsb: OneByte(info, 0xa6); break;
			case Instruction::cmpsw: AddPrefixByte(info, 0x66); OneByte(info, 0xa7); break;
			case Instruction::cmpsd: OneByte(info, 0xa7); break;
			case Instruction::cmpsq: Invalid(); break;
			case Instruction::lodsb: OneByte(info, 0xac); break;
			case Instruction::lodsw: AddPrefixByte(info, 0x66); OneByte(info, 0xad); break;
			case Instruction::lodsd: OneByte(info, 0xad); break;
			case Instruction::lodsq: Invalid(); break;
			case Instruction::movsb: OneByte(info, 0xa4); break;
			case Instruction::movsw: AddPrefixByte(info, 0x66); OneByte(info, 0xa5); break;
			case Instruction::movsd: OneByte(info, 0xa5); break;
			case Instruction::movsq: Invalid(); break;
			case Instruction::scasb: OneByte(info, 0xae); break;
			case Instruction::scasw: AddPrefixByte(info, 0x66); OneByte(info, 0xaf); break;
			case Instruction::scasd: OneByte(info, 0xaf); break;
			case Instruction::scasq: Invalid(); break;
			case Instruction::stosb: OneByte(info, 0xaa); break;
			case Instruction::stosw: AddPrefixByte(info, 0x66); OneByte(info, 0xab); break;
			case Instruction::stosd: OneByte(info, 0xab); break;
			case Instruction::stosq: Invalid(); break;
			case Instruction::insb: OneByte(info, 0x6c); break;
			case Instruction::insw: AddPrefixByte(info, 0x66); OneByte(info, 0x6d); break;
			case Instruction::insd: OneByte(info, 0x6d); break;
			case Instruction::outsb: OneByte(info, 0x6e); break;
			case Instruction::outsw: AddPrefixByte(info, 0x66); OneByte(info, 0x6f); break;
			case Instruction::outsd: OneByte(info, 0x6f); break;
		}
	}

	void IntelAssembler::Assemble64(AnalyzeInfo& info)
	{
		info.prefixSize = 0;
		info.instrSize = 0;

		for (size_t i = 0; i < info.numParams; i++)
		{
			if (IsSpecial(info.params[i]))
			{
				throw "Invalid parameter";
			}
		}

		AssemblePrefixes(info);

		switch (info.instr)
		{
			// Instructions using ModRM / with an immediate operand

			case Instruction::adc:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_I | InstrForm::Form_MI | InstrForm::Form_MR | InstrForm::Form_RM;
				feature.Form_RegOpcode = 2;
				feature.Form_I_Opcode8 = 0x14;
				feature.Form_I_Opcode16_64 = 0x15;
				feature.Form_MI_Opcode8 = 0x80;
				feature.Form_MI_Opcode16_64 = 0x81;
				feature.Form_MI_Opcode_Imm8 = 0x83;
				feature.Form_MR_Opcode8 = 0x10;
				feature.Form_MR_Opcode16_64 = 0x11;
				feature.Form_RM_Opcode8 = 0x12;
				feature.Form_RM_Opcode16_64 = 0x13;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			// One or more byte instructions

			case Instruction::aaa: Invalid(); break;
			case Instruction::aad: Invalid(); break;
			case Instruction::aam: Invalid(); break;
			case Instruction::aas: Invalid(); break;

			case Instruction::cbw: AddPrefixByte(info, 0x66); OneByte(info, 0x98); break;
			case Instruction::cwde: OneByte(info, 0x98); break;
			case Instruction::cdqe: TwoByte(info, 0x48, 0x98); break;
			case Instruction::cwd: AddPrefixByte(info, 0x66); OneByte(info, 0x99); break;
			case Instruction::cdq: OneByte(info, 0x99); break;
			case Instruction::cqo: TwoByte(info, 0x48, 0x99); break;

			case Instruction::clc: OneByte(info, 0xf8); break;
			case Instruction::cld: OneByte(info, 0xfc); break;
			case Instruction::cli: OneByte(info, 0xfa); break;
			case Instruction::clts: TwoByte(info, 0x0f, 0x06); break;
			case Instruction::cmc: OneByte(info, 0xf5); break;
			case Instruction::stc: OneByte(info, 0xf9); break;
			case Instruction::std: OneByte(info, 0xfd); break;
			case Instruction::sti: OneByte(info, 0xfb); break;

			case Instruction::cpuid: TwoByte(info, 0x0f, 0xa2); break;
			case Instruction::daa: Invalid(); break;
			case Instruction::das: Invalid(); break;
			case Instruction::hlt: OneByte(info, 0xf4); break;
			case Instruction::int3: OneByte(info, 0xcc); break;
			case Instruction::into: OneByte(info, 0xce); break;
			case Instruction::int1: OneByte(info, 0xf1); break;
			case Instruction::invd: TwoByte(info, 0x0f, 0x08); break;
			case Instruction::iret: AddPrefixByte(info, 0x66); OneByte(info, 0xcf); break;
			case Instruction::iretd: OneByte(info, 0xcf); break;
			case Instruction::iretq: TwoByte(info, 0x48, 0xcf); break;
			case Instruction::lahf: Invalid(); break;
			case Instruction::sahf: Invalid(); break;
			case Instruction::leave: OneByte(info, 0xc9); break;
			case Instruction::nop: OneByte(info, 0x90); break;
			case Instruction::rdmsr: TwoByte(info, 0x0f, 0x32); break;
			case Instruction::rdpmc: TwoByte(info, 0x0f, 0x33); break;
			case Instruction::rdtsc: TwoByte(info, 0x0f, 0x31); break;
			case Instruction::rdtscp: TriByte(info, 0x0f, 0x01, 0xf9); break;
			case Instruction::rsm: TwoByte(info, 0x0f, 0xaa); break;
			case Instruction::swapgs: TriByte(info, 0x0f, 0x01, 0xf8); break;
			case Instruction::syscall: TwoByte(info, 0x0f, 0x05); break;
			case Instruction::sysret: TwoByte(info, 0x0f, 0x07); break;
			case Instruction::sysretq: TriByte(info, 0x48, 0x0f, 0x07); break;
			case Instruction::ud2: TwoByte(info, 0x0f, 0x0b); break;
			case Instruction::wait: OneByte(info, 0x9b); break;
			case Instruction::wbinvd: TwoByte(info, 0x0f, 0x09); break;
			case Instruction::wrmsr: TwoByte(info, 0x0f, 0x30); break;
			case Instruction::xlatb: TwoByte(info, 0x48, 0xd7); break;

			case Instruction::popa: Invalid(); break;
			case Instruction::popad: Invalid(); break;
			case Instruction::popf: AddPrefixByte(info, 0x66); OneByte(info, 0x9d); break;
			case Instruction::popfd: Invalid(); break;
			case Instruction::popfq: OneByte(info, 0x9d); break;
			case Instruction::pusha: Invalid(); break;
			case Instruction::pushad: Invalid(); break;
			case Instruction::pushf: AddPrefixByte(info, 0x66); OneByte(info, 0x9c); break;
			case Instruction::pushfd: Invalid(); break;
			case Instruction::pushfq: OneByte(info, 0x9c); break;

			case Instruction::cmpsb: OneByte(info, 0xa6); break;
			case Instruction::cmpsw: AddPrefixByte(info, 0x66); OneByte(info, 0xa7); break;
			case Instruction::cmpsd: AddPrefixByte(info, 0x67); OneByte(info, 0xa7); break;
			case Instruction::cmpsq: TwoByte(info, 0x48, 0xa7); break;
			case Instruction::lodsb: OneByte(info, 0xac); break;
			case Instruction::lodsw: AddPrefixByte(info, 0x66); OneByte(info, 0xad); break;
			case Instruction::lodsd: AddPrefixByte(info, 0x67); OneByte(info, 0xad); break;
			case Instruction::lodsq: TwoByte(info, 0x48, 0xad); break;
			case Instruction::movsb: OneByte(info, 0xa4); break;
			case Instruction::movsw: AddPrefixByte(info, 0x66); OneByte(info, 0xa5); break;
			case Instruction::movsd: AddPrefixByte(info, 0x67); OneByte(info, 0xa5); break;
			case Instruction::movsq: TwoByte(info, 0x48, 0xa5); break;
			case Instruction::scasb: OneByte(info, 0xae); break;
			case Instruction::scasw: AddPrefixByte(info, 0x66); OneByte(info, 0xaf); break;
			case Instruction::scasd: AddPrefixByte(info, 0x67); OneByte(info, 0xaf); break;
			case Instruction::scasq: TwoByte(info, 0x48, 0xaf); break;
			case Instruction::stosb: OneByte(info, 0xaa); break;
			case Instruction::stosw: AddPrefixByte(info, 0x66); OneByte(info, 0xab); break;
			case Instruction::stosd: AddPrefixByte(info, 0x67); OneByte(info, 0xab); break;
			case Instruction::stosq: TwoByte(info, 0x48, 0xab); break;
			case Instruction::insb: OneByte(info, 0x6c); break;
			case Instruction::insw: AddPrefixByte(info, 0x66); OneByte(info, 0x6d); break;
			case Instruction::insd: AddPrefixByte(info, 0x67); OneByte(info, 0x6d); break;
			case Instruction::outsb: OneByte(info, 0x6e); break;
			case Instruction::outsw: AddPrefixByte(info, 0x66); OneByte(info, 0x6f); break;
			case Instruction::outsd: AddPrefixByte(info, 0x67); OneByte(info, 0x6f); break;
		}
	}

#pragma endregion "Base methods"

#pragma region "Quick helpers"

	// Instructions using ModRM / with an immediate operand

	template <> AnalyzeInfo IntelAssembler::adc<16>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::adc;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::adc<32>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::adc;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::adc<64>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::adc;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	// Simple encoding instructions

	// One or more byte instructions

	template <> AnalyzeInfo IntelAssembler::aaa<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::aaa;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::aaa<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::aaa;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::aaa<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::aaa;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::aad<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::aad;
		AddImmParam(info, 10);
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::aad<16>(uint8_t v)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::aad;
		AddImmParam(info, v);
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::aad<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::aad;
		AddImmParam(info, 10);
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::aad<32>(uint8_t v)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::aad;
		AddImmParam(info, v);
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::aad<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::aad;
		AddImmParam(info, 10);
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::aad<64>(uint8_t v)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::aad;
		AddImmParam(info, v);
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::aam<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::aam;
		AddImmParam(info, 10);
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::aam<16>(uint8_t v)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::aam;
		AddImmParam(info, v);
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::aam<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::aam;
		AddImmParam(info, 10);
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::aam<32>(uint8_t v)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::aam;
		AddImmParam(info, v);
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::aam<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::aam;
		AddImmParam(info, 10);
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::aam<64>(uint8_t v)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::aam;
		AddImmParam(info, v);
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::aas<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::aas;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::aas<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::aas;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::aas<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::aas;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cbw<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cbw;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cbw<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cbw;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cbw<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cbw;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cwde<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cwde;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cwde<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cwde;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cwde<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cwde;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cdqe<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cdqe;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cdqe<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cdqe;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cdqe<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cdqe;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cwd<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cwd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cwd<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cwd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cwd<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cwd;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cdq<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cdq;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cdq<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cdq;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cdq<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cdq;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cqo<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cqo;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cqo<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cqo;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cqo<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cqo;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::clc<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::clc;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::clc<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::clc;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::clc<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::clc;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cld<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cld;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cld<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cld;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cld<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cld;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cli<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cli;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cli<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cli;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cli<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cli;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::clts<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::clts;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::clts<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::clts;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::clts<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::clts;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cmc<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cmc;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cmc<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cmc;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cmc<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cmc;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::stc<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::stc;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::stc<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::stc;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::stc<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::stc;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::std<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::std;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::std<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::std;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::std<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::std;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sti<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sti;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sti<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sti;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sti<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sti;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cpuid<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cpuid;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cpuid<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cpuid;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cpuid<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cpuid;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::daa<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::daa;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::daa<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::daa;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::daa<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::daa;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::das<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::das;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::das<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::das;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::das<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::das;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::hlt<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::hlt;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::hlt<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::hlt;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::hlt<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::hlt;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::int3<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::int3;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::int3<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::int3;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::int3<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::int3;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::into<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::into;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::into<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::into;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::into<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::into;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::int1<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::int1;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::int1<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::int1;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::int1<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::int1;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::invd<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::invd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::invd<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::invd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::invd<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::invd;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::iret<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::iret;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::iret<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::iret;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::iret<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::iret;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::iretd<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::iretd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::iretd<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::iretd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::iretd<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::iretd;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::iretq<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::iretq;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::iretq<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::iretq;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::iretq<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::iretq;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lahf<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lahf;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lahf<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lahf;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lahf<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lahf;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sahf<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sahf;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sahf<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sahf;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sahf<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sahf;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::leave<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::leave;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::leave<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::leave;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::leave<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::leave;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::nop<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::nop;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::nop<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::nop;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::nop<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::nop;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::rdmsr<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::rdmsr;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::rdmsr<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::rdmsr;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::rdmsr<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::rdmsr;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::rdpmc<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::rdpmc;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::rdpmc<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::rdpmc;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::rdpmc<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::rdpmc;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::rdtsc<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::rdtsc;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::rdtsc<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::rdtsc;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::rdtsc<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::rdtsc;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::rdtscp<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::rdtscp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::rdtscp<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::rdtscp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::rdtscp<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::rdtscp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::rsm<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::rsm;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::rsm<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::rsm;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::rsm<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::rsm;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::swapgs<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::swapgs;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::swapgs<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::swapgs;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::swapgs<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::swapgs;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::syscall<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::syscall;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::syscall<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::syscall;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::syscall<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::syscall;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sysret<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sysret;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sysret<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sysret;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sysret<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sysret;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sysretq<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sysretq;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sysretq<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sysretq;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sysretq<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sysretq;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ud2<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ud2;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ud2<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ud2;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ud2<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ud2;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::wait<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::wait;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::wait<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::wait;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::wait<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::wait;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fwait<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::wait;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fwait<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::wait;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::fwait<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::wait;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::wbinvd<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::wbinvd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::wbinvd<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::wbinvd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::wbinvd<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::wbinvd;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::wrmsr<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::wrmsr;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::wrmsr<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::wrmsr;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::wrmsr<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::wrmsr;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::xlatb<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::xlatb;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::xlatb<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::xlatb;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::xlatb<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::xlatb;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::popa<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::popa;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::popa<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::popa;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::popa<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::popa;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::popad<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::popad;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::popad<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::popad;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::popad<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::popad;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::popf<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::popf;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::popf<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::popf;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::popf<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::popf;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::popfd<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::popfd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::popfd<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::popfd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::popfd<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::popfd;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::popfq<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::popfq;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::popfq<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::popfq;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::popfq<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::popfq;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::pusha<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::pusha;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::pusha<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::pusha;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::pusha<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::pusha;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::pushad<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::pushad;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::pushad<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::pushad;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::pushad<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::pushad;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::pushf<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::pushf;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::pushf<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::pushf;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::pushf<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::pushf;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::pushfd<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::pushfd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::pushfd<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::pushfd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::pushfd<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::pushfd;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::pushfq<16>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::pushfq;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::pushfq<32>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::pushfq;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::pushfq<64>()
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::pushfq;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cmpsb<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::cmpsb;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cmpsb<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::cmpsb;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cmpsb<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::cmpsb;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cmpsw<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::cmpsw;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cmpsw<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::cmpsw;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cmpsw<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::cmpsw;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cmpsd<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::cmpsd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cmpsd<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::cmpsd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cmpsd<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::cmpsd;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cmpsq<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::cmpsq;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cmpsq<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::cmpsq;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cmpsq<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::cmpsq;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lodsb<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::lodsb;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lodsb<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::lodsb;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lodsb<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::lodsb;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lodsw<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::lodsw;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lodsw<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::lodsw;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lodsw<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::lodsw;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lodsd<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::lodsd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lodsd<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::lodsd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lodsd<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::lodsd;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lodsq<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::lodsq;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lodsq<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::lodsq;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lodsq<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::lodsq;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::movsb<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::movsb;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::movsb<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::movsb;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::movsb<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::movsb;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::movsw<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::movsw;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::movsw<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::movsw;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::movsw<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::movsw;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::movsd<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::movsd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::movsd<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::movsd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::movsd<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::movsd;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::movsq<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::movsq;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::movsq<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::movsq;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::movsq<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::movsq;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::scasb<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::scasb;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::scasb<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::scasb;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::scasb<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::scasb;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::scasw<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::scasw;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::scasw<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::scasw;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::scasw<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::scasw;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::scasd<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::scasd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::scasd<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::scasd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::scasd<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::scasd;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::scasq<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::scasq;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::scasq<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::scasq;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::scasq<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::scasq;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::stosb<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::stosb;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::stosb<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::stosb;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::stosb<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::stosb;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::stosw<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::stosw;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::stosw<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::stosw;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::stosw<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::stosw;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::stosd<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::stosd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::stosd<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::stosd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::stosd<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::stosd;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::stosq<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::stosq;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::stosq<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::stosq;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::stosq<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::stosq;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::insb<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::insb;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::insb<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::insb;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::insb<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::insb;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::insw<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::insw;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::insw<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::insw;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::insw<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::insw;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::insd<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::insd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::insd<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::insd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::insd<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::insd;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::outsb<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::outsb;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::outsb<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::outsb;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::outsb<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::outsb;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::outsw<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::outsw;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::outsw<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::outsw;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::outsw<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::outsw;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::outsd<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::outsd;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::outsd<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::outsd;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::outsd<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		info.instr = Instruction::outsd;
		Assemble64(info);
		return info;
	}

#pragma endregion "Quick helpers"

}
