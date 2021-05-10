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

	void IntelAssembler::AddQword(AnalyzeInfo& info, uint64_t n)
	{
		OneByte(info, n & 0xff);
		OneByte(info, (n >> 8) & 0xff);
		OneByte(info, (n >> 16) & 0xff);
		OneByte(info, (n >> 24) & 0xff);
		OneByte(info, (n >> 32) & 0xff);
		OneByte(info, (n >> 40) & 0xff);
		OneByte(info, (n >> 48) & 0xff);
		OneByte(info, (n >> 56) & 0xff);
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

		// Check that there is no such prefix yet

		for (size_t i = 0; i < info.prefixSize; i++)
		{
			if (info.prefixBytes[i] == pre)
			{
				return;
			}
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
			case Param::RelStart:
			case Param::RelEnd:
			case Param::FarPtrStart:
			case Param::FarPtrEnd:
			case Param::MoffsStart:
			case Param::MoffsEnd:
			case Param::RegStart:
			case Param::RegEnd:
			case Param::SregStart:
			case Param::SregEnd:
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
		return Param::ImmStart < p && p < Param::ImmEnd;
	}

	bool IntelAssembler::IsSImm(Param p)
	{
		return (p == Param::simm8_as16 || p == Param::simm8_as32 || p == Param::simm8_as64);
	}

	bool IntelAssembler::IsRel(Param p)
	{
		return Param::RelStart < p && p < Param::RelEnd;
	}

	bool IntelAssembler::IsFarPtr(Param p)
	{
		return Param::FarPtrStart < p && p < Param::FarPtrEnd;
	}

	bool IntelAssembler::IsMoffs(Param p)
	{
		return Param::MoffsStart < p&& p < Param::MoffsEnd;
	}

	bool IntelAssembler::IsReg(Param p)
	{
		return Param::RegStart < p && p < Param::RegEnd;
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

	bool IntelAssembler::IsSreg(Param p)
	{
		return Param::SregStart < p && p < Param::SregEnd;
	}

	bool IntelAssembler::IsMem(Param p)
	{
		return Param::MemStart < p && p < Param::MemEnd;
	}

	bool IntelAssembler::IsMem16(Param p)
	{
		return Param::Mem16Start < p && p < Param::Mem16End;
	}

	bool IntelAssembler::IsMem32(Param p)
	{
		return (Param::Mem32Start < p && p < Param::Mem32End) || (Param::MemSib32Start < p && p < Param::MemSib32End);
	}

	bool IntelAssembler::IsMem64(Param p)
	{
		return (Param::Mem64Start < p && p < Param::Mem64End) || (Param::MemSib64Start < p && p < Param::MemSib64End);
	}

	bool IntelAssembler::IsSib(Param p)
	{
		return Param::SibStart < p && p < Param::SibEnd;
	}

	bool IntelAssembler::IsMemDisp8(Param p)
	{
		if (Param::MemSib32Disp8Start < p && p < Param::MemSib32Disp8End) return true;
		if (Param::MemSib64Disp8Start < p && p < Param::MemSib64Disp8End) return true;

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
		if (Param::MemSib32Disp32Start < p && p < Param::MemSib32Disp32End) return true;
		if (Param::MemSib64Disp32Start < p && p < Param::MemSib64Disp32End) return true;

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

	void IntelAssembler::GetSreg(Param p, size_t& sreg)
	{
		switch (p)
		{
			case Param::es: sreg = 0; break;
			case Param::cs: sreg = 1; break;
			case Param::ss: sreg = 2; break;
			case Param::ds: sreg = 3; break;
			case Param::fs: sreg = 4; break;
			case Param::gs: sreg = 5; break;

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
		// Try all formats one by one 

		if ((feature.forms & InstrForm::Form_O) && bits != 64)
		{
			if (info.numParams != 1)
			{
				throw "Invalid parameters";
			}

			if (IsReg16(info.params[0]) || IsReg32(info.params[0]))
			{
				if (IsReg16(info.params[0]) && bits == 32)
				{
					AddPrefixByte(info, 0x66);
				}

				if (IsReg32(info.params[0]) && bits == 16)
				{
					AddPrefixByte(info, 0x66);
				}

				size_t reg;
				GetReg(info.params[0], reg);

				if (reg >= 8)
				{
					Invalid();
				}

				OneByte(info, feature.Form_O_Opcode | (uint8_t)reg);
				return;
			}
		}

		if ((feature.forms & InstrForm::Form_O) && bits == 64)
		{
			if (info.numParams != 1)
			{
				throw "Invalid parameters";
			}

			if (IsReg64(info.params[0]))
			{
				size_t reg;
				GetReg(info.params[0], reg);

				if (reg >= 8)
				{
					Invalid();
				}

				OneByte(info, feature.Form_O_Opcode | (uint8_t)reg);
				return;
			}
		}

		if ((feature.forms & InstrForm::Form_OI) && IsReg(info.params[0]) && IsImm(info.params[1]))
		{
			if (info.numParams != 2)
			{
				throw "Invalid parameters";
			}

			if (IsReg8(info.params[0]) && info.params[1] != Param::imm8)
			{
				throw "Invalid parameters";
			}

			if (IsReg16(info.params[0]) && info.params[1] != Param::imm16)
			{
				throw "Invalid parameters";
			}

			if (IsReg32(info.params[0]) && info.params[1] != Param::imm32)
			{
				throw "Invalid parameters";
			}

			if (IsReg64(info.params[0]) && info.params[1] != Param::imm64)
			{
				throw "Invalid parameters";
			}

			size_t reg;
			GetReg(info.params[0], reg);

			if (IsReg32(info.params[0]) && bits == 16)
			{
				AddPrefixByte(info, 0x66);
			}

			if (IsReg16(info.params[0]) && bits != 16)
			{
				AddPrefixByte(info, 0x66);
			}

			bool freakingRegs = info.params[0] == Param::spl || info.params[0] == Param::bpl || info.params[0] == Param::sil || info.params[0] == Param::dil;
			bool rexRequired = reg >= 8 || freakingRegs || info.params[1] == Param::imm64;

			if (rexRequired && bits != 64)
			{
				Invalid();
			}

			if (rexRequired)
			{
				int REX_W = IsReg64(info.params[0]) ? 1 : 0;
				int REX_R = 0;
				int REX_X = 0;
				int REX_B = reg >= 8 ? 1 : 0;
				OneByte(info, 0x40 | (REX_W << 3) | (REX_R << 2) | (REX_X << 1) | REX_B);
			}

			OneByte(info, (IsReg8(info.params[0]) ? feature.Form_OI_Opcode8 : feature.Form_OI_Opcode16_64) | (reg & 7));

			if (info.params[1] == Param::imm8) OneByte(info, info.Imm.uimm8);
			else if (info.params[1] == Param::imm16) AddUshort(info, info.Imm.uimm16);
			else if (info.params[1] == Param::imm32) AddUlong(info, info.Imm.uimm32);
			else if (info.params[1] == Param::imm64) AddQword(info, info.Imm.uimm64);

			return;
		}

		if (feature.forms & InstrForm::Form_I)
		{
			if (!(info.numParams == 1 || info.numParams == 2))
			{
				throw "Invalid parameters";
			}

			if ((IsReg(info.params[0]) && IsImm(info.params[1])) && info.numParams == 2)
			{
				switch (info.params[0])
				{
					case Param::al:
						if (info.params[1] == Param::imm8)
						{
							OneByte(info, feature.Form_I_Opcode8);
							OneByte(info, info.Imm.uimm8);
							return;
						}
						break;
					case Param::ax:
						if (info.params[1] == Param::imm16)
						{
							if (bits != 16)
							{
								AddPrefixByte(info, 0x66);
							}
							OneByte(info, feature.Form_I_Opcode16_64);
							AddUshort(info, info.Imm.uimm16);
							return;
						}
						break;
					case Param::eax:
						if (info.params[1] == Param::imm32)
						{
							if (bits == 16)
							{
								AddPrefixByte(info, 0x66);
							}
							OneByte(info, feature.Form_I_Opcode16_64);
							AddUlong(info, info.Imm.uimm32);
							return;
						}
						break;
					case Param::rax:
						if (info.params[1] == Param::imm32)
						{
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
						}
						break;
					default:
						break;
				}
			}
			else if (IsImm(info.params[0]) && info.numParams == 1)
			{
				if (info.params[0] == Param::imm8)
				{
					OneByte(info, feature.Form_I_Opcode8);
					OneByte(info, info.Imm.uimm8);
					return;
				}
				else if (info.params[0] == Param::imm16)
				{
					if (bits != 16)
					{
						AddPrefixByte(info, 0x66);
					}
					OneByte(info, feature.Form_I_Opcode16_64);
					AddUshort(info, info.Imm.uimm16);
					return;
				}
				else if (info.params[0] == Param::imm32)
				{
					if (bits == 16)
					{
						AddPrefixByte(info, 0x66);
					}
					OneByte(info, feature.Form_I_Opcode16_64);
					AddUlong(info, info.Imm.uimm32);
					return;
				}
			}
		}
	
		if (feature.forms & InstrForm::Form_Rel8)
		{
			if (info.numParams != 1)
			{
				throw "Invalid parameters";
			}

			if (info.params[0] == Param::rel8)
			{
				OneByte(info, feature.Form_Rel_Opcode8);
				OneByte(info, info.Disp.disp8);
				return;
			}
		}

		if (feature.forms & InstrForm::Form_Rel16)
		{
			if (info.numParams != 1)
			{
				throw "Invalid parameters";
			}

			if (info.params[0] == Param::rel16)
			{
				switch (bits)
				{
					case 16:
						break;
					case 32:
						AddPrefixByte(info, 0x66);
						break;
					case 64:
						Invalid();
						break;
				}
				OneByte(info, feature.Form_Rel_Opcode16_32);
				AddUshort(info, info.Disp.disp16);
				return;
			}
		}

		if (feature.forms & InstrForm::Form_Rel32)
		{
			if (info.numParams != 1)
			{
				throw "Invalid parameters";
			}

			if (info.params[0] == Param::rel32)
			{
				switch (bits)
				{
					case 16:
						AddPrefixByte(info, 0x66);
						break;
					case 32:
					case 64:
						break;
				}
				OneByte(info, feature.Form_Rel_Opcode16_32);
				AddUlong(info, info.Disp.disp32);
				return;
			}
		}

		if (feature.forms & InstrForm::Form_Far16)
		{
			if (info.numParams != 1)
			{
				throw "Invalid parameters";
			}

			if (info.params[0] == Param::farptr16)
			{
				switch (bits)
				{
					case 16:
						break;
					case 32:
						AddPrefixByte(info, 0x66);
						break;
					case 64:
						Invalid();
						break;
				}
				OneByte(info, feature.Form_FarPtr_Opcode);
				AddUshort(info, info.Disp.disp16);
				AddUshort(info, info.Imm.uimm16);
				return;
			}
		}

		if (feature.forms & InstrForm::Form_Far32)
		{
			if (info.numParams != 1)
			{
				throw "Invalid parameters";
			}

			if (info.params[0] == Param::farptr32)
			{
				switch (bits)
				{
					case 16:
						AddPrefixByte(info, 0x66);
						break;
					case 32:
						break;
					case 64:
						Invalid();
						break;
				}
				OneByte(info, feature.Form_FarPtr_Opcode);
				AddUlong(info, info.Disp.disp32);
				AddUshort(info, info.Imm.uimm16);
				return;
			}
		}

		if ((feature.forms & InstrForm::Form_FD) && IsMoffs(info.params[1]))
		{
			if (info.numParams != 2)
			{
				throw "Invalid parameters";
			}

			HandleMoffs(info, bits, 0, 1, feature.Form_FD_Opcode8, feature.Form_FD_Opcode16_64);
			return;
		}

		if ((feature.forms & InstrForm::Form_TD) && IsMoffs(info.params[0]))
		{
			if (info.numParams != 2)
			{
				throw "Invalid parameters";
			}

			HandleMoffs(info, bits, 1, 0, feature.Form_TD_Opcode8, feature.Form_TD_Opcode16_64);
			return;
		}

		if (feature.forms & InstrForm::Form_MI)
		{
			if (info.numParams != 2)
			{
				throw "Invalid parameters";
			}

			if ((IsReg(info.params[0]) || IsMem(info.params[0])) && IsImm(info.params[1]))
			{
				HandleModRmImm(info, bits, feature.Form_MI_Opcode8, feature.Form_MI_Opcode16_64, feature.Form_MI_Opcode_SImm8, feature.Form_RegOpcode, feature.Extended_Opcode);
				return;
			}
		}

		if (feature.forms & InstrForm::Form_MI8)
		{
			if (info.numParams != 2)
			{
				throw "Invalid parameters";
			}

			if ((IsReg(info.params[0]) || IsMem(info.params[0])) && info.params[1] == Param::imm8)
			{
				HandleModRmImm(info, bits, feature.Form_MI_Opcode8, feature.Form_MI_Opcode16_64, 0, feature.Form_RegOpcode, feature.Extended_Opcode);
				return;
			}
		}

		if (feature.forms & InstrForm::Form_M)
		{
			// There is no need to check the number of parameters because some instructions can support both M, RM and RMI forms (e.g. IMUL).

			if ((IsReg(info.params[0]) || IsMem(info.params[0])) && info.numParams == 1)
			{
				HandleModRm(info, bits, feature.Form_M_Opcode8, feature.Form_M_Opcode16_64, feature.Form_RegOpcode, feature.Extended_Opcode);
				return;
			}
		}

		if (feature.forms & InstrForm::Form_M_Strict)
		{
			if (info.numParams != 1)
			{
				throw "Invalid parameters";
			}

			if (IsMem(info.params[0]))
			{
				HandleModRm(info, bits, feature.Form_M_Opcode8, feature.Form_M_Opcode16_64, feature.Form_RegOpcode, feature.Extended_Opcode);
				return;
			}
		}

		if (feature.forms & InstrForm::Form_MR)
		{
			if (info.numParams != 2)
			{
				throw "Invalid parameters";
			}

			if ((IsReg(info.params[0]) || IsMem(info.params[0])) && IsReg(info.params[1]) )
			{
				HandleModRegRm(info, bits, 1, 0, feature.Form_MR_Opcode8, feature.Form_MR_Opcode16_64, feature.Extended_Opcode, feature.Extended_Opcode2);
				return;
			}
		}

		if (feature.forms & InstrForm::Form_MR16)
		{
			if (info.numParams != 2)
			{
				throw "Invalid parameters";
			}

			if ((IsReg16(info.params[0]) || IsMem(info.params[0])) && IsReg16(info.params[1]))
			{
				HandleModRegRm(info, bits, 1, 0, feature.Form_MR_Opcode, feature.Form_MR_Opcode, feature.Extended_Opcode);
				return;
			}
		}

		if (feature.forms & InstrForm::Form_RM)
		{
			// There is no need to check the number of parameters because some instructions can support both M, RM and RMI forms (e.g. IMUL).

			if ((IsReg(info.params[0]) && IsMem(info.params[1])) && info.numParams == 2)
			{
				HandleModRegRm(info, bits, 0, 1, 
					feature.Form_RM_Opcode8, feature.Form_RM_Opcode16_64,
					feature.Extended_Opcode ? feature.Extended_Opcode : feature.Extended_Opcode_RMOnly,
					feature.Extended_Opcode2 );
				return;
			}
		}

		if (feature.forms & InstrForm::Form_RM16)
		{
			if (info.numParams != 2)
			{
				throw "Invalid parameters";
			}

			if (IsReg16(info.params[0]) && IsMem(info.params[1]))
			{
				HandleModRegRm(info, bits, 0, 1, feature.Form_RM_Opcode, feature.Form_RM_Opcode, feature.Extended_Opcode);
				return;
			}
		}

		if (feature.forms & InstrForm::Form_RM32)
		{
			if (info.numParams != 2)
			{
				throw "Invalid parameters";
			}

			if (IsReg32(info.params[0]) && IsMem(info.params[1]))
			{
				HandleModRegRm(info, bits, 0, 1, feature.Form_RM_Opcode, feature.Form_RM_Opcode, feature.Extended_Opcode);
				return;
			}
		}

		if (feature.forms & InstrForm::Form_RMI)
		{
			if (info.numParams != 3)
			{
				throw "Invalid parameters";
			}

			if (IsReg(info.params[0]) && IsMem(info.params[1]) && IsImm(info.params[2]))
			{
				HandleModRegRmImm(info, bits, 0, 1, feature.Form_RMI_Opcode8, feature.Form_RMI_Opcode16_64, feature.Extended_Opcode);
				return;
			}
		}

		if ((feature.forms & InstrForm::Form_MSr) && IsSreg(info.params[1]))
		{
			if (info.numParams != 2)
			{
				throw "Invalid parameters";
			}

			if (IsReg(info.params[0]) || IsMem(info.params[0]))
			{
				HandleModSregRm(info, bits, 1, 0, feature.Form_MSr_Opcode);
				return;
			}
		}

		if ((feature.forms & InstrForm::Form_SrM) && IsSreg(info.params[0]))
		{
			if (info.numParams != 2)
			{
				throw "Invalid parameters";
			}

			if (IsReg(info.params[1]) || IsMem(info.params[1]))
			{
				HandleModSregRm(info, bits, 0, 1, feature.Form_SrM_Opcode);
				return;
			}
		}

		if (feature.forms & InstrForm::Form_RMX)
		{
			if ((IsReg(info.params[0]) && (IsMem(info.params[1]) || IsReg(info.params[1]))) && info.numParams == 2)
			{
				HandleModRegRmx(info, bits, feature.Form_RM_Opcode8, feature.Form_RM_Opcode16_64, feature.Extended_Opcode);
				return;
			}
		}

		if (feature.forms & InstrForm::Form_RotSh)
		{
			if ((IsMem(info.params[0]) || IsReg(info.params[0])) && info.numParams == 2)
			{
				HandleModRmRotSh(info, bits, feature);
				return;
			}
		}

		if (feature.forms & InstrForm::Form_Shd)
		{
			if (info.numParams != 3)
			{
				throw "Invalid parameters";
			}

			if ( (IsMem(info.params[0]) || IsReg(info.params[0])) && IsReg(info.params[1]) && (info.params[2] == Param::imm8 || info.params[2] == Param::cl))
			{
				if (IsReg8(info.params[0]) || IsReg8(info.params[1]))
				{
					throw "Invalid parameters";
				}

				HandleModRegRmImm(info, bits, 1, 0, feature.Form_MRI_Opcode, feature.Form_MRC_Opcode, feature.Extended_Opcode);
				return;
			}
		}

		throw "Invalid instruction form";
	}

	// Various options for parameter processing using ModRM/SIB/REX mechanisms.
	// Someday I will combine them into one big method, but then only no one will be able to understand it.

	/// <summary>
	/// Parameter processing version when "M" form of instructions is used (i.e. one parameter, reg field is used for additional opcode).
	/// This form of instruction requires an additional "PtrHint" (byte ptr, word ptr, etc.), since guessing the size of the parameter is not possible by other means.
	/// </summary>
	/// <param name="info">Instruction Information</param>
	/// <param name="bits">Current processor mode (8, 16 or 32)</param>
	/// <param name="opcode8">Base opcode for 8-bit instruction form</param>
	/// <param name="opcode16_64">Base opcode for 16/32/64-bit instruction form</param>
	/// <param name="opcodeReg">Extended opcode, which is used instead of the reg field in the ModRM byte.</param>
	/// <param name="extendedOpcode">Escape opcode, for example 0x0F. If it is 0x00, it is not used.</param>
	void IntelAssembler::HandleModRm(AnalyzeInfo& info, size_t bits, uint8_t opcode8, uint8_t opcode16_64, uint8_t opcodeReg, uint8_t extendedOpcode)
	{
		size_t mod = 0, reg = 0, rm = 0;
		size_t scale = 0, index = 0, base = 0;

		// Extract and check required information from parameters 

		if (IsReg8(info.params[0]) && (info.instr == Instruction::call || info.instr == Instruction::jmp))
		{
			// Special processing for call and jmp

			throw "Invalid parameter";
		}

		if ((IsReg64(info.params[0]) || IsMem64(info.params[0])) && bits != 64)
		{
			throw "Invalid parameter";
		}

		switch (info.params[0])
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

		if ((IsReg32(info.params[0]) || info.ptrHint == PtrHint::DwordPtr) && bits == 16)
		{
			AddPrefixByte(info, 0x66);
		}

		if ((IsReg16(info.params[0]) || info.ptrHint == PtrHint::WordPtr) && bits != 16)
		{
			AddPrefixByte(info, 0x66);
		}

		switch (bits)
		{
			case 16:
				if (IsMem32(info.params[0]))
				{
					AddPrefixByte(info, 0x67);
				}
				else if (IsMem64(info.params[0]))
				{
					Invalid();
				}
				break;

			case 32:
				if (IsMem16(info.params[0]))
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

		bool freakingRegs = info.params[0] == Param::spl || info.params[0] == Param::bpl || info.params[0] == Param::sil || info.params[0] == Param::dil;
		bool rexRequired = reg >= 8 || rm >= 8 || index >= 8 || base >= 8 || IsReg64(info.params[0]) || info.ptrHint == PtrHint::QwordPtr || freakingRegs;

		if (rexRequired && bits != 64)
		{
			Invalid();
		}

		if (rexRequired)
		{
			int REX_W = (IsReg64(info.params[0]) || info.ptrHint == PtrHint::QwordPtr) ? 1 : 0;
			int REX_R = reg >= 8 ? 1 : 0;
			int REX_X = sibRequired ? ((index >= 8) ? 1 : 0) : 0;
			int REX_B = sibRequired ? ((base >= 8) ? 1 : 0) : ((rm >= 8) ? 1 : 0);
			OneByte(info, 0x40 | (REX_W << 3) | (REX_R << 2) | (REX_X << 1) | REX_B);
		}

		if (extendedOpcode)
		{
			OneByte(info, extendedOpcode);
		}

		uint8_t mainOpcode = (IsReg8(info.params[0]) || info.ptrHint == PtrHint::BytePtr) ? opcode8 : opcode16_64;
		if (mainOpcode == UnusedOpcode)
		{
			Invalid();
		}

		OneByte(info, mainOpcode);

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

	/// <summary>
	/// Version for processing "MR" and "RM" forms.
	/// The "MR" version is also used to process parameters of the `reg, reg` type (i.e. the first `rm` parameter is actually a register).
	/// </summary>
	void IntelAssembler::HandleModRegRm(AnalyzeInfo& info, size_t bits, size_t regParam, size_t rmParam, uint8_t opcode8, uint8_t opcode16_64, uint8_t extendedOpcode, uint8_t extendedOpcode2)
	{
		size_t mod = 0, reg = 0, rm = 0;
		size_t scale = 0, index = 0, base = 0;

		// Extract and check required information from parameters 

		if (IsReg64(info.params[regParam]) && bits != 64)
		{
			throw "Invalid parameter";
		}

		if ((IsReg64(info.params[rmParam]) || IsMem64(info.params[rmParam])) && bits != 64)
		{
			throw "Invalid parameter";
		}

		if ((IsReg8(info.params[regParam]) && IsReg16(info.params[rmParam])) ||
			(IsReg8(info.params[regParam]) && IsReg32(info.params[rmParam])) ||
			(IsReg8(info.params[regParam]) && IsReg64(info.params[rmParam])))
		{
			throw "Invalid parameter";
		}

		if ((IsReg16(info.params[regParam]) && IsReg8(info.params[rmParam])) || 
			(IsReg16(info.params[regParam]) && IsReg32(info.params[rmParam])) || 
			(IsReg16(info.params[regParam]) && IsReg64(info.params[rmParam])) )
		{
			throw "Invalid parameter";
		}

		if ((IsReg32(info.params[regParam]) && IsReg8(info.params[rmParam])) ||
			(IsReg32(info.params[regParam]) && IsReg16(info.params[rmParam])) ||
			(IsReg32(info.params[regParam]) && IsReg64(info.params[rmParam])))
		{
			throw "Invalid parameter";
		}

		if ((IsReg64(info.params[regParam]) && IsReg8(info.params[rmParam])) ||
			(IsReg64(info.params[regParam]) && IsReg16(info.params[rmParam])) ||
			(IsReg64(info.params[regParam]) && IsReg32(info.params[rmParam])))
		{
			throw "Invalid parameter";
		}

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

		switch (info.params[rmParam])
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

		switch (bits)
		{
			case 16:
				if (IsMem32(info.params[rmParam]))
				{
					AddPrefixByte(info, 0x67);
				}
				else if (IsMem64(info.params[rmParam]))
				{
					Invalid();
				}
				break;

			case 32:
				if (IsMem16(info.params[rmParam]))
				{
					AddPrefixByte(info, 0x67);
				}
				else if (IsMem64(info.params[rmParam]))
				{
					Invalid();
				}
				break;

			case 64:
				if (IsMem32(info.params[rmParam]))
				{
					AddPrefixByte(info, 0x67);
				}
				else if (info.params[rmParam] == Param::m_eip_disp32)
				{
					AddPrefixByte(info, 0x67);
				}
				else if (IsMem16(info.params[rmParam]))
				{
					Invalid();
				}
				break;
		}

		bool freakingRegs = info.params[regParam] == Param::spl || info.params[regParam] == Param::bpl || info.params[regParam] == Param::sil || info.params[regParam] == Param::dil;
		bool rexRequired = reg >= 8 || rm >= 8 || index >= 8 || base >= 8 || IsReg64(info.params[regParam]) || freakingRegs;

		if (rexRequired && bits != 64)
		{
			Invalid();
		}

		if (rexRequired)
		{
			int REX_W = (IsReg64(info.params[regParam]) || IsReg64(info.params[rmParam])) ? 1 : 0;
			int REX_R = reg >= 8 ? 1 : 0;
			int REX_X = sibRequired ? ((index >= 8) ? 1 : 0) : 0;
			int REX_B = sibRequired ? ((base >= 8) ? 1 : 0) : ((rm >= 8) ? 1 : 0);
			OneByte(info, 0x40 | (REX_W << 3) | (REX_R << 2) | (REX_X << 1) | REX_B);
		}

		if (extendedOpcode)
		{
			OneByte(info, extendedOpcode);
		}

		if (extendedOpcode2)
		{
			OneByte(info, extendedOpcode2);
		}

		uint8_t mainOpcode = IsReg8(info.params[regParam]) ? opcode8 : opcode16_64;
		if (mainOpcode == UnusedOpcode)
		{
			Invalid();
		}

		OneByte(info, mainOpcode);

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

	/// <summary>
	/// Version for processing "MI" type instruction forms. The reg field in the ModRM byte contains an additional opcode.
	/// </summary>
	void IntelAssembler::HandleModRmImm(AnalyzeInfo& info, size_t bits, uint8_t opcode8, uint8_t opcode16_64, uint8_t opcodeSimm8, uint8_t opcodeReg, uint8_t extendedOpcode)
	{
		size_t mod = 0, reg = 0, rm = 0;
		size_t scale = 0, index = 0, base = 0;

		// Extract and check required information from parameters 

		if (IsReg64(info.params[0]) && bits != 64)
		{
			throw "Invalid parameter";
		}

		switch (info.params[0])
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

		if ((IsReg32(info.params[0]) || info.params[1] == Param::imm32 || info.params[1] == Param::simm8_as32 || info.ptrHint == PtrHint::DwordPtr) && bits == 16)
		{
			AddPrefixByte(info, 0x66);
		}

		if ((IsReg16(info.params[0]) || info.params[1] == Param::imm16 || info.params[1] == Param::simm8_as16 || info.ptrHint == PtrHint::WordPtr) && bits != 16)
		{
			AddPrefixByte(info, 0x66);
		}

		switch (bits)
		{
			case 16:
				if (IsMem32(info.params[0]))
				{
					AddPrefixByte(info, 0x67);
				}
				else if (IsMem64(info.params[0]))
				{
					Invalid();
				}
				break;

			case 32:
				if (IsMem16(info.params[0]))
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

		bool freakingRegs = info.params[0] == Param::spl || info.params[0] == Param::bpl || info.params[0] == Param::sil || info.params[0] == Param::dil;
		bool rexRequired = reg >= 8 || rm >= 8 || index >= 8 || base >= 8 || IsReg64(info.params[0]) || info.ptrHint == PtrHint::QwordPtr || freakingRegs;

		if (rexRequired && bits != 64)
		{
			Invalid();
		}

		if (rexRequired)
		{
			int REX_W = (IsReg64(info.params[0]) || info.ptrHint == PtrHint::QwordPtr) ? 1 : 0;
			int REX_R = reg >= 8 ? 1 : 0;
			int REX_X = sibRequired ? ((index >= 8) ? 1 : 0) : 0;
			int REX_B = sibRequired ? ((base >= 8) ? 1 : 0) : ((rm >= 8) ? 1 : 0);
			OneByte(info, 0x40 | (REX_W << 3) | (REX_R << 2) | (REX_X << 1) | REX_B);
		}

		if (extendedOpcode)
		{
			OneByte(info, extendedOpcode);
		}

		uint8_t mainOpcode = IsSImm(info.params[1]) ? opcodeSimm8 : ((info.params[1] == Param::imm8) ? opcode8 : opcode16_64);
		if (mainOpcode == UnusedOpcode)
		{
			Invalid();
		}

		OneByte(info, mainOpcode);

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

		if (info.params[1] == Param::imm8) OneByte(info, info.Imm.uimm8);
		else if (IsSImm(info.params[1])) OneByte(info, info.Imm.simm8);
		else if (info.params[1] == Param::imm16) AddUshort(info, info.Imm.uimm16);
		else if (info.params[1] == Param::imm32) AddUlong(info, info.Imm.uimm32);
	}

	/// <summary>
	/// Did you think you were out of options? I thought.
	/// This is the "RMI" or "MRI" version of instruction form processing, with immediate as the third parameter.
	/// </summary>
	void IntelAssembler::HandleModRegRmImm(AnalyzeInfo& info, size_t bits, size_t regParam, size_t rmParam, uint8_t opcode8, uint8_t opcode16_64, uint8_t extendedOpcode)
	{
		size_t mod = 0, reg = 0, rm = 0;
		size_t scale = 0, index = 0, base = 0;

		// Extract and check required information from parameters 

		if (IsReg64(info.params[regParam]) && bits != 64)
		{
			throw "Invalid parameter";
		}

		if ((IsReg64(info.params[rmParam]) || IsMem64(info.params[rmParam])) && bits != 64)
		{
			throw "Invalid parameter";
		}

		if ((IsReg8(info.params[regParam]) && IsReg16(info.params[rmParam])) ||
			(IsReg8(info.params[regParam]) && IsReg32(info.params[rmParam])) ||
			(IsReg8(info.params[regParam]) && IsReg64(info.params[rmParam])))
		{
			throw "Invalid parameter";
		}

		if ((IsReg16(info.params[regParam]) && IsReg8(info.params[rmParam])) || 
			(IsReg16(info.params[regParam]) && IsReg32(info.params[rmParam])) || 
			(IsReg16(info.params[regParam]) && IsReg64(info.params[rmParam])) )
		{
			throw "Invalid parameter";
		}

		if ((IsReg32(info.params[regParam]) && IsReg8(info.params[rmParam])) ||
			(IsReg32(info.params[regParam]) && IsReg16(info.params[rmParam])) ||
			(IsReg32(info.params[regParam]) && IsReg64(info.params[rmParam])))
		{
			throw "Invalid parameter";
		}

		if ((IsReg64(info.params[regParam]) && IsReg8(info.params[rmParam])) ||
			(IsReg64(info.params[regParam]) && IsReg16(info.params[rmParam])) ||
			(IsReg64(info.params[regParam]) && IsReg32(info.params[rmParam])))
		{
			throw "Invalid parameter";
		}

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

		switch (info.params[rmParam])
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

		switch (bits)
		{
			case 16:
				if (IsMem32(info.params[rmParam]))
				{
					AddPrefixByte(info, 0x67);
				}
				else if (IsMem64(info.params[rmParam]))
				{
					Invalid();
				}
				break;

			case 32:
				if (IsMem16(info.params[rmParam]))
				{
					AddPrefixByte(info, 0x67);
				}
				else if (IsMem64(info.params[rmParam]))
				{
					Invalid();
				}
				break;

			case 64:
				if (IsMem32(info.params[rmParam]))
				{
					AddPrefixByte(info, 0x67);
				}
				else if (info.params[rmParam] == Param::m_eip_disp32)
				{
					AddPrefixByte(info, 0x67);
				}
				else if (IsMem16(info.params[rmParam]))
				{
					Invalid();
				}
				break;
		}

		bool freakingRegs = info.params[regParam] == Param::spl || info.params[regParam] == Param::bpl || info.params[regParam] == Param::sil || info.params[regParam] == Param::dil;
		bool rexRequired = reg >= 8 || rm >= 8 || index >= 8 || base >= 8 || IsReg64(info.params[regParam]) || freakingRegs;

		if (rexRequired && bits != 64)
		{
			Invalid();
		}

		if (rexRequired)
		{
			int REX_W = (IsReg64(info.params[regParam]) || IsReg64(info.params[rmParam])) ? 1 : 0;
			int REX_R = reg >= 8 ? 1 : 0;
			int REX_X = sibRequired ? ((index >= 8) ? 1 : 0) : 0;
			int REX_B = sibRequired ? ((base >= 8) ? 1 : 0) : ((rm >= 8) ? 1 : 0);
			OneByte(info, 0x40 | (REX_W << 3) | (REX_R << 2) | (REX_X << 1) | REX_B);
		}

		if (extendedOpcode)
		{
			OneByte(info, extendedOpcode);
		}

		uint8_t mainOpcode = (info.params[2] == Param::imm8) ? opcode8 : opcode16_64;
		if (mainOpcode == UnusedOpcode)
		{
			Invalid();
		}

		OneByte(info, mainOpcode);

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

		if (info.params[2] == Param::imm8) OneByte(info, info.Imm.uimm8);
		else if (info.params[2] == Param::imm16) AddUshort(info, info.Imm.uimm16);
		else if (info.params[2] == Param::imm32) AddUlong(info, info.Imm.uimm32);
	}

	void IntelAssembler::HandleMoffs(AnalyzeInfo& info, size_t bits, size_t regParam, size_t moffsParam, uint8_t opcode8, uint8_t opcode16_64)
	{
		switch (info.params[regParam])
		{
			case Param::al:
			case Param::ax:
			case Param::eax:
			case Param::rax:
				switch (info.params[moffsParam])
				{
					case Param::moffs16:
						if (bits != 16)
						{
							AddPrefixByte(info, 0x67);
						}
						OneByte(info, IsReg8(info.params[regParam]) ? opcode8 : opcode16_64);
						AddUshort(info, info.Disp.disp16);
						break;

					case Param::moffs32:
						if (bits == 16)
						{
							AddPrefixByte(info, 0x67);
						}
						OneByte(info, IsReg8(info.params[regParam]) ? opcode8 : opcode16_64);
						AddUlong(info, info.Disp.disp32);
						break;

					case Param::moffs64:
						if (bits == 64 && (info.params[regParam] == Param::al || info.params[regParam] == Param::rax))
						{
							OneByte(info, 0x48);
							OneByte(info, IsReg8(info.params[regParam]) ? opcode8 : opcode16_64);
							AddQword(info, info.Disp.disp64);
						}
						else
						{
							Invalid();
						}
						break;
				}
				break;

			default:
				Invalid();
				break;
		}
	}

	/// <summary>
	/// Unfortunately another clone of the ModRM-like addressing handler was needed, but for the case where `reg` is actually `Sreg`.
	/// Mixing reg and Sreg in the base handler is not a good idea, because it gets even more confusing.
	/// </summary>
	void IntelAssembler::HandleModSregRm(AnalyzeInfo& info, size_t bits, size_t sregParam, size_t rmParam, uint8_t opcode)
	{
		size_t mod = 0, reg = 0, rm = 0;
		size_t scale = 0, index = 0, base = 0;

		// Extract and check required information from parameters 

		GetSreg(info.params[sregParam], reg);
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

		if ((IsReg32(info.params[rmParam]) || info.ptrHint == PtrHint::DwordPtr) && bits == 16)
		{
			AddPrefixByte(info, 0x66);
		}

		if ((IsReg16(info.params[rmParam]) || info.ptrHint == PtrHint::WordPtr) && bits != 16)
		{
			AddPrefixByte(info, 0x66);
		}

		switch (bits)
		{
			case 16:
				if (IsMem32(info.params[rmParam]))
				{
					AddPrefixByte(info, 0x67);
				}
				else if (IsMem64(info.params[rmParam]))
				{
					Invalid();
				}
				break;

			case 32:
				if (IsMem16(info.params[rmParam]))
				{
					AddPrefixByte(info, 0x67);
				}
				else if (IsMem64(info.params[rmParam]))
				{
					Invalid();
				}
				break;

			case 64:
				if (IsMem32(info.params[rmParam]))
				{
					AddPrefixByte(info, 0x67);
				}
				else if (info.params[rmParam] == Param::m_eip_disp32)
				{
					AddPrefixByte(info, 0x67);
				}
				else if (IsMem16(info.params[rmParam]))
				{
					Invalid();
				}
				break;
		}

		bool rexRequired = reg >= 8 || rm >= 8 || index >= 8 || base >= 8 || IsReg64(info.params[rmParam]) || info.ptrHint == PtrHint::QwordPtr;

		if (rexRequired && bits != 64)
		{
			Invalid();
		}

		if (rexRequired)
		{
			int REX_W = (IsReg64(info.params[rmParam]) || info.ptrHint == PtrHint::QwordPtr) ? 1 : 0;
			int REX_R = reg >= 8 ? 1 : 0;
			int REX_X = sibRequired ? ((index >= 8) ? 1 : 0) : 0;
			int REX_B = sibRequired ? ((base >= 8) ? 1 : 0) : ((rm >= 8) ? 1 : 0);
			OneByte(info, 0x40 | (REX_W << 3) | (REX_R << 2) | (REX_X << 1) | REX_B);
		}

		OneByte(info, opcode);

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

	/// <summary>
	/// This version of the RM form can accept register (R) instead of "M" and also does not check for operand size matching (the opcode-associated size is taken as the second operand).
	/// </summary>
	void IntelAssembler::HandleModRegRmx(AnalyzeInfo& info, size_t bits, uint8_t opcode8, uint8_t opcode16_64, uint8_t extendedOpcode)
	{
		size_t mod = 0, reg = 0, rm = 0;
		size_t scale = 0, index = 0, base = 0;

		// Extract and check required information from parameters 

		if (IsReg64(info.params[0]) && bits != 64)
		{
			throw "Invalid parameter";
		}

		if ((IsReg64(info.params[1]) || IsMem64(info.params[1])) && bits != 64)
		{
			throw "Invalid parameter";
		}

		switch (info.params[1])
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

		GetReg(info.params[0], reg);
		GetMod(info.params[1], mod);
		GetRm(info.params[1], rm);

		bool sibRequired = IsSib(info.params[1]);

		if (sibRequired)
		{
			GetSS(info.params[1], scale);
			GetIndex(info.params[1], index);
			GetBase(info.params[1], base);
		}

		// Compile the resulting instruction, mode prefixes and possible displacement

		if (IsReg32(info.params[0]) && bits == 16)
		{
			AddPrefixByte(info, 0x66);
		}

		if (IsReg16(info.params[0]) && bits != 16)
		{
			AddPrefixByte(info, 0x66);
		}

		switch (bits)
		{
			case 16:
				if (IsMem32(info.params[1]))
				{
					AddPrefixByte(info, 0x67);
				}
				else if (IsMem64(info.params[1]))
				{
					Invalid();
				}
				break;

			case 32:
				if (IsMem16(info.params[1]))
				{
					AddPrefixByte(info, 0x67);
				}
				else if (IsMem64(info.params[1]))
				{
					Invalid();
				}
				break;

			case 64:
				if (IsMem32(info.params[1]))
				{
					AddPrefixByte(info, 0x67);
				}
				else if (info.params[1] == Param::m_eip_disp32)
				{
					AddPrefixByte(info, 0x67);
				}
				else if (IsMem16(info.params[1]))
				{
					Invalid();
				}
				break;
		}

		bool freakingRegs = info.params[1] == Param::spl || info.params[1] == Param::bpl || info.params[1] == Param::sil || info.params[1] == Param::dil;
		bool rexRequired = reg >= 8 || rm >= 8 || index >= 8 || base >= 8 || IsReg64(info.params[0]) || freakingRegs;

		if (rexRequired && bits != 64)
		{
			Invalid();
		}

		if (rexRequired)
		{
			int REX_W = IsReg64(info.params[0]) ? 1 : 0;
			int REX_R = reg >= 8 ? 1 : 0;
			int REX_X = sibRequired ? ((index >= 8) ? 1 : 0) : 0;
			int REX_B = sibRequired ? ((base >= 8) ? 1 : 0) : ((rm >= 8) ? 1 : 0);
			OneByte(info, 0x40 | (REX_W << 3) | (REX_R << 2) | (REX_X << 1) | REX_B);
		}

		if (extendedOpcode)
		{
			OneByte(info, extendedOpcode);
		}

		// The opcode is selected by the second operand.

		uint8_t mainOpcode = UnusedOpcode;
		
		if (IsReg8(info.params[1]))
		{
			mainOpcode = opcode8;
		}
		else if (IsReg16(info.params[1]) || IsReg32(info.params[1]))
		{
			mainOpcode = opcode16_64;
		}
		else if (IsMem(info.params[1]) && info.ptrHint == PtrHint::BytePtr)
		{
			mainOpcode = opcode8;
		}
		else
		{
			mainOpcode = opcode16_64;
		}

		if (mainOpcode == UnusedOpcode)
		{
			Invalid();
		}

		OneByte(info, mainOpcode);

		uint8_t modRmByte = ((mod & 3) << 6) | ((reg & 7) << 3) | (rm & 7);
		OneByte(info, modRmByte);

		if (sibRequired)
		{
			uint8_t sibByte = ((scale & 3) << 6) | ((index & 7) << 3) | (base & 7);
			OneByte(info, sibByte);
		}

		if (IsMemDisp8(info.params[1])) OneByte(info, info.Disp.disp8);
		else if (IsMemDisp16(info.params[1])) AddUshort(info, info.Disp.disp16);
		else if (IsMemDisp32(info.params[1])) AddUlong(info, info.Disp.disp32);
	}

	/// <summary>
	/// Different ModRM options for Rotate/Shift instructions.
	/// </summary>
	void IntelAssembler::HandleModRmRotSh(AnalyzeInfo& info, size_t bits, InstrFeatures& feature)
	{
		size_t mod = 0, reg = 0, rm = 0;
		size_t scale = 0, index = 0, base = 0;

		// Extract and check required information from parameters 

		if ((IsReg64(info.params[0]) || IsMem64(info.params[0])) && bits != 64)
		{
			throw "Invalid parameter";
		}

		switch (info.params[0])
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

		reg = feature.Form_RegOpcode;
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

		if ((IsReg32(info.params[0]) || info.ptrHint == PtrHint::DwordPtr) && bits == 16)
		{
			AddPrefixByte(info, 0x66);
		}

		if ((IsReg16(info.params[0]) || info.ptrHint == PtrHint::WordPtr) && bits != 16)
		{
			AddPrefixByte(info, 0x66);
		}

		switch (bits)
		{
			case 16:
				if (IsMem32(info.params[0]))
				{
					AddPrefixByte(info, 0x67);
				}
				else if (IsMem64(info.params[0]))
				{
					Invalid();
				}
				break;

			case 32:
				if (IsMem16(info.params[0]))
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

		bool freakingRegs = info.params[0] == Param::spl || info.params[0] == Param::bpl || info.params[0] == Param::sil || info.params[0] == Param::dil;
		bool rexRequired = reg >= 8 || rm >= 8 || index >= 8 || base >= 8 || IsReg64(info.params[0]) || info.ptrHint == PtrHint::QwordPtr || freakingRegs;

		if (rexRequired && bits != 64)
		{
			Invalid();
		}

		if (rexRequired)
		{
			int REX_W = (IsReg64(info.params[0]) || info.ptrHint == PtrHint::QwordPtr) ? 1 : 0;
			int REX_R = reg >= 8 ? 1 : 0;
			int REX_X = sibRequired ? ((index >= 8) ? 1 : 0) : 0;
			int REX_B = sibRequired ? ((base >= 8) ? 1 : 0) : ((rm >= 8) ? 1 : 0);
			OneByte(info, 0x40 | (REX_W << 3) | (REX_R << 2) | (REX_X << 1) | REX_B);
		}

		// Select the opcode depending on the second parameter (1, CL or imm8).

		bool immRequired = false;
		bool byteOperand = (IsReg8(info.params[0]) || info.ptrHint == PtrHint::BytePtr);
		uint8_t mainOpcode = UnusedOpcode;

		switch (info.params[1])
		{
			case Param::imm8:
				if (info.Imm.uimm8 == 1)
				{
					mainOpcode = byteOperand ? feature.Form_M1_Opcode8 : feature.Form_M1_Opcode16_64;
				}
				else
				{
					mainOpcode = byteOperand ? feature.Form_MI_Opcode8 : feature.Form_MI_Opcode16_64;
					immRequired = true;
				}
				break;

			case Param::cl:
				mainOpcode = byteOperand ? feature.Form_MC_Opcode8 : feature.Form_MC_Opcode16_64;
				break;

			default:
				throw "Invalid parameter";
				break;
		}

		if (mainOpcode == UnusedOpcode)
		{
			Invalid();
		}

		OneByte(info, mainOpcode);

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

		if (immRequired)
		{
			if (info.params[1] == Param::imm8)
			{
				OneByte(info, info.Imm.uimm8);
			}
			else
			{
				throw "Invalid immediate parameter";
			}
		}
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
				feature.Form_MI_Opcode_SImm8 = 0x83;
				feature.Form_MR_Opcode8 = 0x10;
				feature.Form_MR_Opcode16_64 = 0x11;
				feature.Form_RM_Opcode8 = 0x12;
				feature.Form_RM_Opcode16_64 = 0x13;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::add:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_I | InstrForm::Form_MI | InstrForm::Form_MR | InstrForm::Form_RM;
				feature.Form_RegOpcode = 0;
				feature.Form_I_Opcode8 = 0x4;
				feature.Form_I_Opcode16_64 = 0x5;
				feature.Form_MI_Opcode8 = 0x80;
				feature.Form_MI_Opcode16_64 = 0x81;
				feature.Form_MI_Opcode_SImm8 = 0x83;
				feature.Form_MR_Opcode8 = 0x0;
				feature.Form_MR_Opcode16_64 = 0x1;
				feature.Form_RM_Opcode8 = 0x2;
				feature.Form_RM_Opcode16_64 = 0x3;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::_and:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_I | InstrForm::Form_MI | InstrForm::Form_MR | InstrForm::Form_RM;
				feature.Form_RegOpcode = 4;
				feature.Form_I_Opcode8 = 0x24;
				feature.Form_I_Opcode16_64 = 0x25;
				feature.Form_MI_Opcode8 = 0x80;
				feature.Form_MI_Opcode16_64 = 0x81;
				feature.Form_MI_Opcode_SImm8 = 0x83;
				feature.Form_MR_Opcode8 = 0x20;
				feature.Form_MR_Opcode16_64 = 0x21;
				feature.Form_RM_Opcode8 = 0x22;
				feature.Form_RM_Opcode16_64 = 0x23;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::arpl:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_MR16;
				feature.Form_MR_Opcode = 0x63;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::bound:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RM16 | InstrForm::Form_RM32;
				feature.Form_RM_Opcode = 0x62;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::bsf:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xBC;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::bsr:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xBD;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::bt:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_MR | InstrForm::Form_MI8;
				feature.Extended_Opcode = 0x0F;
				feature.Form_MR_Opcode8 = UnusedOpcode;
				feature.Form_MR_Opcode16_64 = 0xA3;
				feature.Form_MI_Opcode8 = 0xBA;
				feature.Form_MI_Opcode16_64 = UnusedOpcode;
				feature.Form_RegOpcode = 4;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::btc:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_MR | InstrForm::Form_MI8;
				feature.Extended_Opcode = 0x0F;
				feature.Form_MR_Opcode8 = UnusedOpcode;
				feature.Form_MR_Opcode16_64 = 0xBB;
				feature.Form_MI_Opcode8 = 0xBA;
				feature.Form_MI_Opcode16_64 = UnusedOpcode;
				feature.Form_RegOpcode = 7;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::btr:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_MR | InstrForm::Form_MI8;
				feature.Extended_Opcode = 0x0F;
				feature.Form_MR_Opcode8 = UnusedOpcode;
				feature.Form_MR_Opcode16_64 = 0xB3;
				feature.Form_MI_Opcode8 = 0xBA;
				feature.Form_MI_Opcode16_64 = UnusedOpcode;
				feature.Form_RegOpcode = 6;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::bts:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_MR | InstrForm::Form_MI8;
				feature.Extended_Opcode = 0x0F;
				feature.Form_MR_Opcode8 = UnusedOpcode;
				feature.Form_MR_Opcode16_64 = 0xAB;
				feature.Form_MI_Opcode8 = 0xBA;
				feature.Form_MI_Opcode16_64 = UnusedOpcode;
				feature.Form_RegOpcode = 5;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::call:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_Rel16 | InstrForm::Form_Rel32 | InstrForm::Form_M;
				feature.Form_Rel_Opcode8 = UnusedOpcode;
				feature.Form_Rel_Opcode16_32 = 0xE8;
				feature.Form_RegOpcode = 2;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0xFF;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::callfar:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_Far16 | InstrForm::Form_Far32 | InstrForm::Form_M;
				feature.Form_FarPtr_Opcode = 0x9A;
				feature.Form_RegOpcode = 3;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0xFF;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::cmp:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_I | InstrForm::Form_MI | InstrForm::Form_MR | InstrForm::Form_RM;
				feature.Form_RegOpcode = 7;
				feature.Form_I_Opcode8 = 0x3C;
				feature.Form_I_Opcode16_64 = 0x3D;
				feature.Form_MI_Opcode8 = 0x80;
				feature.Form_MI_Opcode16_64 = 0x81;
				feature.Form_MI_Opcode_SImm8 = 0x83;
				feature.Form_MR_Opcode8 = 0x38;
				feature.Form_MR_Opcode16_64 = 0x39;
				feature.Form_RM_Opcode8 = 0x3A;
				feature.Form_RM_Opcode16_64 = 0x3B;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::cmpxchg:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_MR;
				feature.Extended_Opcode = 0x0F;
				feature.Form_MR_Opcode8 = 0xB0;
				feature.Form_MR_Opcode16_64 = 0xB1;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::dec:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M | InstrForm::Form_O;
				feature.Form_RegOpcode = 1;
				feature.Form_M_Opcode8 = 0xFE;
				feature.Form_M_Opcode16_64 = 0xFF;
				feature.Form_O_Opcode = 0x48;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::div:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 6;
				feature.Form_M_Opcode8 = 0xF6;
				feature.Form_M_Opcode16_64 = 0xF7;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::idiv:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 7;
				feature.Form_M_Opcode8 = 0xF6;
				feature.Form_M_Opcode16_64 = 0xF7;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::imul:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M | InstrForm::Form_RM | InstrForm::Form_RMI;
				feature.Form_RegOpcode = 5;
				feature.Extended_Opcode_RMOnly = 0x0F;
				feature.Form_M_Opcode8 = 0xF6;
				feature.Form_M_Opcode16_64 = 0xF7;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xAF;
				feature.Form_RMI_Opcode8 = 0x6B;
				feature.Form_RMI_Opcode16_64 = 0x69;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::inc:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M | InstrForm::Form_O;
				feature.Form_RegOpcode = 0;
				feature.Form_M_Opcode8 = 0xFE;
				feature.Form_M_Opcode16_64 = 0xFF;
				feature.Form_O_Opcode = 0x40;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::invlpg:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M_Strict;
				feature.Form_RegOpcode = 7;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x01;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::invpcid:
			{
				InstrFeatures feature = { 0 };

				// Special processing for INVPCID (only r32 and r64 are allowed)

				if (IsReg8(info.params[0]) || IsReg16(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Extended_Opcode2 = 0x38;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0x82;

				AddPrefixByte(info, 0x66);
				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::jmp:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_Rel8 | InstrForm::Form_Rel16 | InstrForm::Form_Rel32 | InstrForm::Form_M;
				feature.Form_Rel_Opcode8 = 0xEB;
				feature.Form_Rel_Opcode16_32 = 0xE9;
				feature.Form_RegOpcode = 4;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0xFF;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::jmpfar:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_Far16 | InstrForm::Form_Far32 | InstrForm::Form_M;
				feature.Form_FarPtr_Opcode = 0xEA;
				feature.Form_RegOpcode = 5;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0xFF;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::lar:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0x02;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::lds:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xC5;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::lea:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0x8D;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::les:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xC4;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::lfs:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xB4;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::lgdt:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 2;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x01;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::lgs:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xB5;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::lidt:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 3;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x01;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::lldt:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 2;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x00;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::lmsw:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 6;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x01;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::lsl:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0x03;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::lss:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xB2;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::ltr:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 3;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x00;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::mov:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_MR | InstrForm::Form_RM | InstrForm::Form_MSr | InstrForm::Form_SrM | InstrForm::Form_FD | InstrForm::Form_TD | InstrForm::Form_OI | InstrForm::Form_MI;
				feature.Form_RegOpcode = 0;
				feature.Form_MR_Opcode8 = 0x88;
				feature.Form_MR_Opcode16_64 = 0x89;
				feature.Form_MSr_Opcode = 0x8C;
				feature.Form_RM_Opcode8 = 0x8A;
				feature.Form_RM_Opcode16_64 = 0x8B;
				feature.Form_SrM_Opcode = 0x8E;
				feature.Form_FD_Opcode8 = 0xA0;
				feature.Form_FD_Opcode16_64 = 0xA1;
				feature.Form_TD_Opcode8 = 0xA2;
				feature.Form_TD_Opcode16_64 = 0xA3;
				feature.Form_OI_Opcode8 = 0xB0;
				feature.Form_OI_Opcode16_64 = 0xB8;
				feature.Form_MI_Opcode8 = 0xC6;
				feature.Form_MI_Opcode16_64 = 0xC7;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::movbe:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]) || IsReg8(info.params[1]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM | InstrForm::Form_MR;
				feature.Extended_Opcode = 0x0F;
				feature.Extended_Opcode2 = 0x38;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xF0;
				feature.Form_MR_Opcode8 = UnusedOpcode;
				feature.Form_MR_Opcode16_64 = 0xF1;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::movsx:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RMX;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = 0xBE;
				feature.Form_RM_Opcode16_64 = 0xBF;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::movsxd:
			{
				Invalid();
				break;
			}

			case Instruction::movzx:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RMX;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = 0xB6;
				feature.Form_RM_Opcode16_64 = 0xB7;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::mul:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 4;
				feature.Form_M_Opcode8 = 0xF6;
				feature.Form_M_Opcode16_64 = 0xF7;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::_not:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 2;
				feature.Form_M_Opcode8 = 0xF6;
				feature.Form_M_Opcode16_64 = 0xF7;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::_or:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_I | InstrForm::Form_MI | InstrForm::Form_MR | InstrForm::Form_RM;
				feature.Form_RegOpcode = 1;
				feature.Form_I_Opcode8 = 0x0C;
				feature.Form_I_Opcode16_64 = 0x0D;
				feature.Form_MI_Opcode8 = 0x80;
				feature.Form_MI_Opcode16_64 = 0x81;
				feature.Form_MI_Opcode_SImm8 = 0x83;
				feature.Form_MR_Opcode8 = 0x08;
				feature.Form_MR_Opcode16_64 = 0x09;
				feature.Form_RM_Opcode8 = 0x0A;
				feature.Form_RM_Opcode16_64 = 0x0B;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::pop:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M | InstrForm::Form_O;
				feature.Form_RegOpcode = 0;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x8F;
				feature.Form_O_Opcode = 0x58;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::push:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M | InstrForm::Form_O | InstrForm::Form_I;
				feature.Form_RegOpcode = 6;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0xFF;
				feature.Form_O_Opcode = 0x50;
				feature.Form_I_Opcode8 = 0x6A;
				feature.Form_I_Opcode16_64 = 0x68;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::rcl:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RotSh;
				feature.Form_RegOpcode = 2;
				feature.Form_M1_Opcode8 = 0xD0;
				feature.Form_M1_Opcode16_64 = 0xD1;
				feature.Form_MC_Opcode8 = 0xD2;
				feature.Form_MC_Opcode16_64 = 0xD3;
				feature.Form_MI_Opcode8 = 0xC0;
				feature.Form_MI_Opcode16_64 = 0xC1;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::rcr:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RotSh;
				feature.Form_RegOpcode = 3;
				feature.Form_M1_Opcode8 = 0xD0;
				feature.Form_M1_Opcode16_64 = 0xD1;
				feature.Form_MC_Opcode8 = 0xD2;
				feature.Form_MC_Opcode16_64 = 0xD3;
				feature.Form_MI_Opcode8 = 0xC0;
				feature.Form_MI_Opcode16_64 = 0xC1;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::rol:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RotSh;
				feature.Form_RegOpcode = 0;
				feature.Form_M1_Opcode8 = 0xD0;
				feature.Form_M1_Opcode16_64 = 0xD1;
				feature.Form_MC_Opcode8 = 0xD2;
				feature.Form_MC_Opcode16_64 = 0xD3;
				feature.Form_MI_Opcode8 = 0xC0;
				feature.Form_MI_Opcode16_64 = 0xC1;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::ror:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RotSh;
				feature.Form_RegOpcode = 1;
				feature.Form_M1_Opcode8 = 0xD0;
				feature.Form_M1_Opcode16_64 = 0xD1;
				feature.Form_MC_Opcode8 = 0xD2;
				feature.Form_MC_Opcode16_64 = 0xD3;
				feature.Form_MI_Opcode8 = 0xC0;
				feature.Form_MI_Opcode16_64 = 0xC1;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::sal:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RotSh;
				feature.Form_RegOpcode = 4;
				feature.Form_M1_Opcode8 = 0xD0;
				feature.Form_M1_Opcode16_64 = 0xD1;
				feature.Form_MC_Opcode8 = 0xD2;
				feature.Form_MC_Opcode16_64 = 0xD3;
				feature.Form_MI_Opcode8 = 0xC0;
				feature.Form_MI_Opcode16_64 = 0xC1;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::sar:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RotSh;
				feature.Form_RegOpcode = 7;
				feature.Form_M1_Opcode8 = 0xD0;
				feature.Form_M1_Opcode16_64 = 0xD1;
				feature.Form_MC_Opcode8 = 0xD2;
				feature.Form_MC_Opcode16_64 = 0xD3;
				feature.Form_MI_Opcode8 = 0xC0;
				feature.Form_MI_Opcode16_64 = 0xC1;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::sbb:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_I | InstrForm::Form_MI | InstrForm::Form_MR | InstrForm::Form_RM;
				feature.Form_RegOpcode = 3;
				feature.Form_I_Opcode8 = 0x1C;
				feature.Form_I_Opcode16_64 = 0x1D;
				feature.Form_MI_Opcode8 = 0x80;
				feature.Form_MI_Opcode16_64 = 0x81;
				feature.Form_MI_Opcode_SImm8 = 0x83;
				feature.Form_MR_Opcode8 = 0x18;
				feature.Form_MR_Opcode16_64 = 0x19;
				feature.Form_RM_Opcode8 = 0x1A;
				feature.Form_RM_Opcode16_64 = 0x1B;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::seto:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x90;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::setno:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x91;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::setb:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x92;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::setae:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x93;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::sete:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x94;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::setne:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x95;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::setbe:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x96;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::seta:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x97;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::sets:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x98;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::setns:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x99;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::setp:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x9A;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::setnp:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x9B;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::setl:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x9C;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::setge:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x9D;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::setle:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x9E;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::setg:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x9F;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::sgdt:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 0;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x01;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::shl:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RotSh;
				feature.Form_RegOpcode = 4;
				feature.Form_M1_Opcode8 = 0xD0;
				feature.Form_M1_Opcode16_64 = 0xD1;
				feature.Form_MC_Opcode8 = 0xD2;
				feature.Form_MC_Opcode16_64 = 0xD3;
				feature.Form_MI_Opcode8 = 0xC0;
				feature.Form_MI_Opcode16_64 = 0xC1;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::shld:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_Shd;
				feature.Extended_Opcode = 0x0F;
				feature.Form_MRI_Opcode = 0xA4;
				feature.Form_MRC_Opcode = 0xA5;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::shr:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RotSh;
				feature.Form_RegOpcode = 5;
				feature.Form_M1_Opcode8 = 0xD0;
				feature.Form_M1_Opcode16_64 = 0xD1;
				feature.Form_MC_Opcode8 = 0xD2;
				feature.Form_MC_Opcode16_64 = 0xD3;
				feature.Form_MI_Opcode8 = 0xC0;
				feature.Form_MI_Opcode16_64 = 0xC1;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::shrd:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_Shd;
				feature.Extended_Opcode = 0x0F;
				feature.Form_MRI_Opcode = 0xAC;
				feature.Form_MRC_Opcode = 0xAD;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::sidt:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 1;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x01;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::sldt:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 0;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x00;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::smsw:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 4;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x01;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::str:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 1;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x00;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::sub:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_I | InstrForm::Form_MI | InstrForm::Form_MR | InstrForm::Form_RM;
				feature.Form_RegOpcode = 5;
				feature.Form_I_Opcode8 = 0x2C;
				feature.Form_I_Opcode16_64 = 0x2D;
				feature.Form_MI_Opcode8 = 0x80;
				feature.Form_MI_Opcode16_64 = 0x81;
				feature.Form_MI_Opcode_SImm8 = 0x83;
				feature.Form_MR_Opcode8 = 0x28;
				feature.Form_MR_Opcode16_64 = 0x29;
				feature.Form_RM_Opcode8 = 0x2A;
				feature.Form_RM_Opcode16_64 = 0x2B;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::test:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_I | InstrForm::Form_MI | InstrForm::Form_MR;
				feature.Form_RegOpcode = 0;
				feature.Form_I_Opcode8 = 0xA8;
				feature.Form_I_Opcode16_64 = 0xA9;
				feature.Form_MI_Opcode8 = 0xF6;
				feature.Form_MI_Opcode16_64 = 0xF7;
				feature.Form_MI_Opcode_SImm8 = UnusedOpcode;
				feature.Form_MR_Opcode8 = 0x84;
				feature.Form_MR_Opcode16_64 = 0x85;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::ud0:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]) || IsReg64(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xFF;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::ud1:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]) || IsReg64(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xB9;

				ProcessGpInstr(info, 16, feature);
				break;
			}


			case Instruction::verr:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 4;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x00;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::verw:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 5;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x00;

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
			case Instruction::nop:
			{
				if (info.numParams == 0)
				{
					OneByte(info, 0x90);
				}
				else
				{
					InstrFeatures feature = { 0 };

					feature.forms = InstrForm::Form_M;
					feature.Form_RegOpcode = 0;
					feature.Extended_Opcode = 0x0F;
					feature.Form_M_Opcode8 = UnusedOpcode;
					feature.Form_M_Opcode16_64 = 0x1F;

					ProcessGpInstr(info, 16, feature);
				}
				break;
			}
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
				feature.Form_MI_Opcode_SImm8 = 0x83;
				feature.Form_MR_Opcode8 = 0x10;
				feature.Form_MR_Opcode16_64 = 0x11;
				feature.Form_RM_Opcode8 = 0x12;
				feature.Form_RM_Opcode16_64 = 0x13;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::add:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_I | InstrForm::Form_MI | InstrForm::Form_MR | InstrForm::Form_RM;
				feature.Form_RegOpcode = 0;
				feature.Form_I_Opcode8 = 0x4;
				feature.Form_I_Opcode16_64 = 0x5;
				feature.Form_MI_Opcode8 = 0x80;
				feature.Form_MI_Opcode16_64 = 0x81;
				feature.Form_MI_Opcode_SImm8 = 0x83;
				feature.Form_MR_Opcode8 = 0x0;
				feature.Form_MR_Opcode16_64 = 0x1;
				feature.Form_RM_Opcode8 = 0x2;
				feature.Form_RM_Opcode16_64 = 0x3;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::_and:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_I | InstrForm::Form_MI | InstrForm::Form_MR | InstrForm::Form_RM;
				feature.Form_RegOpcode = 4;
				feature.Form_I_Opcode8 = 0x24;
				feature.Form_I_Opcode16_64 = 0x25;
				feature.Form_MI_Opcode8 = 0x80;
				feature.Form_MI_Opcode16_64 = 0x81;
				feature.Form_MI_Opcode_SImm8 = 0x83;
				feature.Form_MR_Opcode8 = 0x20;
				feature.Form_MR_Opcode16_64 = 0x21;
				feature.Form_RM_Opcode8 = 0x22;
				feature.Form_RM_Opcode16_64 = 0x23;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::arpl:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_MR16;
				feature.Form_MR_Opcode = 0x63;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::bound:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RM16 | InstrForm::Form_RM32;
				feature.Form_RM_Opcode = 0x62;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::bsf:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xBC;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::bsr:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xBD;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::bt:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_MR | InstrForm::Form_MI8;
				feature.Extended_Opcode = 0x0F;
				feature.Form_MR_Opcode8 = UnusedOpcode;
				feature.Form_MR_Opcode16_64 = 0xA3;
				feature.Form_MI_Opcode8 = 0xBA;
				feature.Form_MI_Opcode16_64 = UnusedOpcode;
				feature.Form_RegOpcode = 4;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::btc:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_MR | InstrForm::Form_MI8;
				feature.Extended_Opcode = 0x0F;
				feature.Form_MR_Opcode8 = UnusedOpcode;
				feature.Form_MR_Opcode16_64 = 0xBB;
				feature.Form_MI_Opcode8 = 0xBA;
				feature.Form_MI_Opcode16_64 = UnusedOpcode;
				feature.Form_RegOpcode = 7;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::btr:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_MR | InstrForm::Form_MI8;
				feature.Extended_Opcode = 0x0F;
				feature.Form_MR_Opcode8 = UnusedOpcode;
				feature.Form_MR_Opcode16_64 = 0xB3;
				feature.Form_MI_Opcode8 = 0xBA;
				feature.Form_MI_Opcode16_64 = UnusedOpcode;
				feature.Form_RegOpcode = 6;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::bts:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_MR | InstrForm::Form_MI8;
				feature.Extended_Opcode = 0x0F;
				feature.Form_MR_Opcode8 = UnusedOpcode;
				feature.Form_MR_Opcode16_64 = 0xAB;
				feature.Form_MI_Opcode8 = 0xBA;
				feature.Form_MI_Opcode16_64 = UnusedOpcode;
				feature.Form_RegOpcode = 5;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::call:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_Rel16 | InstrForm::Form_Rel32 | InstrForm::Form_M;
				feature.Form_Rel_Opcode8 = UnusedOpcode;
				feature.Form_Rel_Opcode16_32 = 0xE8;
				feature.Form_RegOpcode = 2;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0xFF;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::callfar:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_Far16 | InstrForm::Form_Far32 | InstrForm::Form_M;
				feature.Form_FarPtr_Opcode = 0x9A;
				feature.Form_RegOpcode = 3;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0xFF;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::cmp:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_I | InstrForm::Form_MI | InstrForm::Form_MR | InstrForm::Form_RM;
				feature.Form_RegOpcode = 7;
				feature.Form_I_Opcode8 = 0x3C;
				feature.Form_I_Opcode16_64 = 0x3D;
				feature.Form_MI_Opcode8 = 0x80;
				feature.Form_MI_Opcode16_64 = 0x81;
				feature.Form_MI_Opcode_SImm8 = 0x83;
				feature.Form_MR_Opcode8 = 0x38;
				feature.Form_MR_Opcode16_64 = 0x39;
				feature.Form_RM_Opcode8 = 0x3A;
				feature.Form_RM_Opcode16_64 = 0x3B;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::cmpxchg:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_MR;
				feature.Extended_Opcode = 0x0F;
				feature.Form_MR_Opcode8 = 0xB0;
				feature.Form_MR_Opcode16_64 = 0xB1;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::dec:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M | InstrForm::Form_O;
				feature.Form_RegOpcode = 1;
				feature.Form_M_Opcode8 = 0xFE;
				feature.Form_M_Opcode16_64 = 0xFF;
				feature.Form_O_Opcode = 0x48;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::div:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 6;
				feature.Form_M_Opcode8 = 0xF6;
				feature.Form_M_Opcode16_64 = 0xF7;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::idiv:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 7;
				feature.Form_M_Opcode8 = 0xF6;
				feature.Form_M_Opcode16_64 = 0xF7;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::imul:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M | InstrForm::Form_RM | InstrForm::Form_RMI;
				feature.Form_RegOpcode = 5;
				feature.Extended_Opcode_RMOnly = 0x0F;
				feature.Form_M_Opcode8 = 0xF6;
				feature.Form_M_Opcode16_64 = 0xF7;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xAF;
				feature.Form_RMI_Opcode8 = 0x6B;
				feature.Form_RMI_Opcode16_64 = 0x69;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::inc:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M | InstrForm::Form_O;
				feature.Form_RegOpcode = 0;
				feature.Form_M_Opcode8 = 0xFE;
				feature.Form_M_Opcode16_64 = 0xFF;
				feature.Form_O_Opcode = 0x40;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::invlpg:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M_Strict;
				feature.Form_RegOpcode = 7;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x01;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::invpcid:
			{
				InstrFeatures feature = { 0 };

				// Special processing for INVPCID (only r32 and r64 are allowed)

				if (IsReg8(info.params[0]) || IsReg16(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Extended_Opcode2 = 0x38;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0x82;

				AddPrefixByte(info, 0x66);
				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::jmp:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_Rel8 | InstrForm::Form_Rel16 | InstrForm::Form_Rel32 | InstrForm::Form_M;
				feature.Form_Rel_Opcode8 = 0xEB;
				feature.Form_Rel_Opcode16_32 = 0xE9;
				feature.Form_RegOpcode = 4;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0xFF;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::jmpfar:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_Far16 | InstrForm::Form_Far32 | InstrForm::Form_M;
				feature.Form_FarPtr_Opcode = 0xEA;
				feature.Form_RegOpcode = 5;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0xFF;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::lar:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0x02;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::lds:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xC5;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::lea:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0x8D;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::les:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xC4;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::lfs:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xB4;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::lgdt:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 2;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x01;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::lgs:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xB5;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::lidt:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 3;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x01;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::lldt:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 2;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x00;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::lmsw:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 6;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x01;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::lsl:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0x03;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::lss:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xB2;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::ltr:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 3;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x00;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::mov:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_MR | InstrForm::Form_RM | InstrForm::Form_MSr | InstrForm::Form_SrM | InstrForm::Form_FD | InstrForm::Form_TD | InstrForm::Form_OI | InstrForm::Form_MI;
				feature.Form_RegOpcode = 0;
				feature.Form_MR_Opcode8 = 0x88;
				feature.Form_MR_Opcode16_64 = 0x89;
				feature.Form_MSr_Opcode = 0x8C;
				feature.Form_RM_Opcode8 = 0x8A;
				feature.Form_RM_Opcode16_64 = 0x8B;
				feature.Form_SrM_Opcode = 0x8E;
				feature.Form_FD_Opcode8 = 0xA0;
				feature.Form_FD_Opcode16_64 = 0xA1;
				feature.Form_TD_Opcode8 = 0xA2;
				feature.Form_TD_Opcode16_64 = 0xA3;
				feature.Form_OI_Opcode8 = 0xB0;
				feature.Form_OI_Opcode16_64 = 0xB8;
				feature.Form_MI_Opcode8 = 0xC6;
				feature.Form_MI_Opcode16_64 = 0xC7;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::movbe:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]) || IsReg8(info.params[1]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM | InstrForm::Form_MR;
				feature.Extended_Opcode = 0x0F;
				feature.Extended_Opcode2 = 0x38;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xF0;
				feature.Form_MR_Opcode8 = UnusedOpcode;
				feature.Form_MR_Opcode16_64 = 0xF1;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::movsx:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RMX;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = 0xBE;
				feature.Form_RM_Opcode16_64 = 0xBF;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::movsxd:
			{
				Invalid();
				break;
			}

			case Instruction::movzx:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RMX;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = 0xB6;
				feature.Form_RM_Opcode16_64 = 0xB7;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::mul:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 4;
				feature.Form_M_Opcode8 = 0xF6;
				feature.Form_M_Opcode16_64 = 0xF7;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::_not:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 2;
				feature.Form_M_Opcode8 = 0xF6;
				feature.Form_M_Opcode16_64 = 0xF7;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::_or:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_I | InstrForm::Form_MI | InstrForm::Form_MR | InstrForm::Form_RM;
				feature.Form_RegOpcode = 1;
				feature.Form_I_Opcode8 = 0x0C;
				feature.Form_I_Opcode16_64 = 0x0D;
				feature.Form_MI_Opcode8 = 0x80;
				feature.Form_MI_Opcode16_64 = 0x81;
				feature.Form_MI_Opcode_SImm8 = 0x83;
				feature.Form_MR_Opcode8 = 0x08;
				feature.Form_MR_Opcode16_64 = 0x09;
				feature.Form_RM_Opcode8 = 0x0A;
				feature.Form_RM_Opcode16_64 = 0x0B;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::pop:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M | InstrForm::Form_O;
				feature.Form_RegOpcode = 0;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x8F;
				feature.Form_O_Opcode = 0x58;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::push:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M | InstrForm::Form_O | InstrForm::Form_I;
				feature.Form_RegOpcode = 6;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0xFF;
				feature.Form_O_Opcode = 0x50;
				feature.Form_I_Opcode8 = 0x6A;
				feature.Form_I_Opcode16_64 = 0x68;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::rcl:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RotSh;
				feature.Form_RegOpcode = 2;
				feature.Form_M1_Opcode8 = 0xD0;
				feature.Form_M1_Opcode16_64 = 0xD1;
				feature.Form_MC_Opcode8 = 0xD2;
				feature.Form_MC_Opcode16_64 = 0xD3;
				feature.Form_MI_Opcode8 = 0xC0;
				feature.Form_MI_Opcode16_64 = 0xC1;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::rcr:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RotSh;
				feature.Form_RegOpcode = 3;
				feature.Form_M1_Opcode8 = 0xD0;
				feature.Form_M1_Opcode16_64 = 0xD1;
				feature.Form_MC_Opcode8 = 0xD2;
				feature.Form_MC_Opcode16_64 = 0xD3;
				feature.Form_MI_Opcode8 = 0xC0;
				feature.Form_MI_Opcode16_64 = 0xC1;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::rol:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RotSh;
				feature.Form_RegOpcode = 0;
				feature.Form_M1_Opcode8 = 0xD0;
				feature.Form_M1_Opcode16_64 = 0xD1;
				feature.Form_MC_Opcode8 = 0xD2;
				feature.Form_MC_Opcode16_64 = 0xD3;
				feature.Form_MI_Opcode8 = 0xC0;
				feature.Form_MI_Opcode16_64 = 0xC1;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::ror:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RotSh;
				feature.Form_RegOpcode = 1;
				feature.Form_M1_Opcode8 = 0xD0;
				feature.Form_M1_Opcode16_64 = 0xD1;
				feature.Form_MC_Opcode8 = 0xD2;
				feature.Form_MC_Opcode16_64 = 0xD3;
				feature.Form_MI_Opcode8 = 0xC0;
				feature.Form_MI_Opcode16_64 = 0xC1;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::sal:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RotSh;
				feature.Form_RegOpcode = 4;
				feature.Form_M1_Opcode8 = 0xD0;
				feature.Form_M1_Opcode16_64 = 0xD1;
				feature.Form_MC_Opcode8 = 0xD2;
				feature.Form_MC_Opcode16_64 = 0xD3;
				feature.Form_MI_Opcode8 = 0xC0;
				feature.Form_MI_Opcode16_64 = 0xC1;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::sar:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RotSh;
				feature.Form_RegOpcode = 7;
				feature.Form_M1_Opcode8 = 0xD0;
				feature.Form_M1_Opcode16_64 = 0xD1;
				feature.Form_MC_Opcode8 = 0xD2;
				feature.Form_MC_Opcode16_64 = 0xD3;
				feature.Form_MI_Opcode8 = 0xC0;
				feature.Form_MI_Opcode16_64 = 0xC1;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::sbb:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_I | InstrForm::Form_MI | InstrForm::Form_MR | InstrForm::Form_RM;
				feature.Form_RegOpcode = 3;
				feature.Form_I_Opcode8 = 0x1C;
				feature.Form_I_Opcode16_64 = 0x1D;
				feature.Form_MI_Opcode8 = 0x80;
				feature.Form_MI_Opcode16_64 = 0x81;
				feature.Form_MI_Opcode_SImm8 = 0x83;
				feature.Form_MR_Opcode8 = 0x18;
				feature.Form_MR_Opcode16_64 = 0x19;
				feature.Form_RM_Opcode8 = 0x1A;
				feature.Form_RM_Opcode16_64 = 0x1B;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::seto:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x90;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::setno:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x91;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::setb:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x92;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::setae:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x93;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 16, feature);
				break;
			}

			case Instruction::sete:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x94;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::setne:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x95;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::setbe:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x96;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::seta:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x97;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::sets:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x98;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::setns:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x99;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::setp:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x9A;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::setnp:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x9B;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::setl:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x9C;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::setge:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x9D;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::setle:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x9E;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::setg:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x9F;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::sgdt:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 0;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x01;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::shl:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RotSh;
				feature.Form_RegOpcode = 4;
				feature.Form_M1_Opcode8 = 0xD0;
				feature.Form_M1_Opcode16_64 = 0xD1;
				feature.Form_MC_Opcode8 = 0xD2;
				feature.Form_MC_Opcode16_64 = 0xD3;
				feature.Form_MI_Opcode8 = 0xC0;
				feature.Form_MI_Opcode16_64 = 0xC1;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::shld:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_Shd;
				feature.Extended_Opcode = 0x0F;
				feature.Form_MRI_Opcode = 0xA4;
				feature.Form_MRC_Opcode = 0xA5;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::shr:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RotSh;
				feature.Form_RegOpcode = 5;
				feature.Form_M1_Opcode8 = 0xD0;
				feature.Form_M1_Opcode16_64 = 0xD1;
				feature.Form_MC_Opcode8 = 0xD2;
				feature.Form_MC_Opcode16_64 = 0xD3;
				feature.Form_MI_Opcode8 = 0xC0;
				feature.Form_MI_Opcode16_64 = 0xC1;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::shrd:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_Shd;
				feature.Extended_Opcode = 0x0F;
				feature.Form_MRI_Opcode = 0xAC;
				feature.Form_MRC_Opcode = 0xAD;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::sidt:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 1;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x01;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::sldt:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 0;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x00;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::smsw:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 4;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x01;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::str:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 1;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x00;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::sub:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_I | InstrForm::Form_MI | InstrForm::Form_MR | InstrForm::Form_RM;
				feature.Form_RegOpcode = 5;
				feature.Form_I_Opcode8 = 0x2C;
				feature.Form_I_Opcode16_64 = 0x2D;
				feature.Form_MI_Opcode8 = 0x80;
				feature.Form_MI_Opcode16_64 = 0x81;
				feature.Form_MI_Opcode_SImm8 = 0x83;
				feature.Form_MR_Opcode8 = 0x28;
				feature.Form_MR_Opcode16_64 = 0x29;
				feature.Form_RM_Opcode8 = 0x2A;
				feature.Form_RM_Opcode16_64 = 0x2B;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::test:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_I | InstrForm::Form_MI | InstrForm::Form_MR;
				feature.Form_RegOpcode = 0;
				feature.Form_I_Opcode8 = 0xA8;
				feature.Form_I_Opcode16_64 = 0xA9;
				feature.Form_MI_Opcode8 = 0xF6;
				feature.Form_MI_Opcode16_64 = 0xF7;
				feature.Form_MI_Opcode_SImm8 = UnusedOpcode;
				feature.Form_MR_Opcode8 = 0x84;
				feature.Form_MR_Opcode16_64 = 0x85;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::ud0:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]) || IsReg64(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xFF;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::ud1:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]) || IsReg64(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xB9;

				ProcessGpInstr(info, 32, feature);
				break;
			}


			case Instruction::verr:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 4;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x00;

				ProcessGpInstr(info, 32, feature);
				break;
			}

			case Instruction::verw:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 5;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x00;

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
			case Instruction::nop:
			{
				if (info.numParams == 0)
				{
					OneByte(info, 0x90);
				}
				else
				{
					InstrFeatures feature = { 0 };

					feature.forms = InstrForm::Form_M;
					feature.Form_RegOpcode = 0;
					feature.Extended_Opcode = 0x0F;
					feature.Form_M_Opcode8 = UnusedOpcode;
					feature.Form_M_Opcode16_64 = 0x1F;

					ProcessGpInstr(info, 32, feature);
				}
				break;
			}
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
				feature.Form_MI_Opcode_SImm8 = 0x83;
				feature.Form_MR_Opcode8 = 0x10;
				feature.Form_MR_Opcode16_64 = 0x11;
				feature.Form_RM_Opcode8 = 0x12;
				feature.Form_RM_Opcode16_64 = 0x13;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::add:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_I | InstrForm::Form_MI | InstrForm::Form_MR | InstrForm::Form_RM;
				feature.Form_RegOpcode = 0;
				feature.Form_I_Opcode8 = 0x4;
				feature.Form_I_Opcode16_64 = 0x5;
				feature.Form_MI_Opcode8 = 0x80;
				feature.Form_MI_Opcode16_64 = 0x81;
				feature.Form_MI_Opcode_SImm8 = 0x83;
				feature.Form_MR_Opcode8 = 0x0;
				feature.Form_MR_Opcode16_64 = 0x1;
				feature.Form_RM_Opcode8 = 0x2;
				feature.Form_RM_Opcode16_64 = 0x3;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::_and:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_I | InstrForm::Form_MI | InstrForm::Form_MR | InstrForm::Form_RM;
				feature.Form_RegOpcode = 4;
				feature.Form_I_Opcode8 = 0x24;
				feature.Form_I_Opcode16_64 = 0x25;
				feature.Form_MI_Opcode8 = 0x80;
				feature.Form_MI_Opcode16_64 = 0x81;
				feature.Form_MI_Opcode_SImm8 = 0x83;
				feature.Form_MR_Opcode8 = 0x20;
				feature.Form_MR_Opcode16_64 = 0x21;
				feature.Form_RM_Opcode8 = 0x22;
				feature.Form_RM_Opcode16_64 = 0x23;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::arpl:
			{
				Invalid();
				break;
			}

			case Instruction::bound:
			{
				Invalid();
				break;
			}

			case Instruction::bsf:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xBC;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::bsr:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xBD;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::bt:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_MR | InstrForm::Form_MI8;
				feature.Extended_Opcode = 0x0F;
				feature.Form_MR_Opcode8 = UnusedOpcode;
				feature.Form_MR_Opcode16_64 = 0xA3;
				feature.Form_MI_Opcode8 = 0xBA;
				feature.Form_MI_Opcode16_64 = UnusedOpcode;
				feature.Form_RegOpcode = 4;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::btc:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_MR | InstrForm::Form_MI8;
				feature.Extended_Opcode = 0x0F;
				feature.Form_MR_Opcode8 = UnusedOpcode;
				feature.Form_MR_Opcode16_64 = 0xBB;
				feature.Form_MI_Opcode8 = 0xBA;
				feature.Form_MI_Opcode16_64 = UnusedOpcode;
				feature.Form_RegOpcode = 7;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::btr:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_MR | InstrForm::Form_MI8;
				feature.Extended_Opcode = 0x0F;
				feature.Form_MR_Opcode8 = UnusedOpcode;
				feature.Form_MR_Opcode16_64 = 0xB3;
				feature.Form_MI_Opcode8 = 0xBA;
				feature.Form_MI_Opcode16_64 = UnusedOpcode;
				feature.Form_RegOpcode = 6;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::bts:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_MR | InstrForm::Form_MI8;
				feature.Extended_Opcode = 0x0F;
				feature.Form_MR_Opcode8 = UnusedOpcode;
				feature.Form_MR_Opcode16_64 = 0xAB;
				feature.Form_MI_Opcode8 = 0xBA;
				feature.Form_MI_Opcode16_64 = UnusedOpcode;
				feature.Form_RegOpcode = 5;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::call:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_Rel16 | InstrForm::Form_Rel32 | InstrForm::Form_M;
				feature.Form_Rel_Opcode8 = UnusedOpcode;
				feature.Form_Rel_Opcode16_32 = 0xE8;
				feature.Form_RegOpcode = 2;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0xFF;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::callfar:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_Far16 | InstrForm::Form_Far32 | InstrForm::Form_M;
				feature.Form_FarPtr_Opcode = 0x9A;
				feature.Form_RegOpcode = 3;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0xFF;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::cmp:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_I | InstrForm::Form_MI | InstrForm::Form_MR | InstrForm::Form_RM;
				feature.Form_RegOpcode = 7;
				feature.Form_I_Opcode8 = 0x3C;
				feature.Form_I_Opcode16_64 = 0x3D;
				feature.Form_MI_Opcode8 = 0x80;
				feature.Form_MI_Opcode16_64 = 0x81;
				feature.Form_MI_Opcode_SImm8 = 0x83;
				feature.Form_MR_Opcode8 = 0x38;
				feature.Form_MR_Opcode16_64 = 0x39;
				feature.Form_RM_Opcode8 = 0x3A;
				feature.Form_RM_Opcode16_64 = 0x3B;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::cmpxchg:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_MR;
				feature.Extended_Opcode = 0x0F;
				feature.Form_MR_Opcode8 = 0xB0;
				feature.Form_MR_Opcode16_64 = 0xB1;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::dec:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M | InstrForm::Form_O;
				feature.Form_RegOpcode = 1;
				feature.Form_M_Opcode8 = 0xFE;
				feature.Form_M_Opcode16_64 = 0xFF;
				feature.Form_O_Opcode = 0x48;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::div:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 6;
				feature.Form_M_Opcode8 = 0xF6;
				feature.Form_M_Opcode16_64 = 0xF7;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::idiv:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 7;
				feature.Form_M_Opcode8 = 0xF6;
				feature.Form_M_Opcode16_64 = 0xF7;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::imul:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M | InstrForm::Form_RM | InstrForm::Form_RMI;
				feature.Form_RegOpcode = 5;
				feature.Extended_Opcode_RMOnly = 0x0F;
				feature.Form_M_Opcode8 = 0xF6;
				feature.Form_M_Opcode16_64 = 0xF7;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xAF;
				feature.Form_RMI_Opcode8 = 0x6B;
				feature.Form_RMI_Opcode16_64 = 0x69;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::inc:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M | InstrForm::Form_O;
				feature.Form_RegOpcode = 0;
				feature.Form_M_Opcode8 = 0xFE;
				feature.Form_M_Opcode16_64 = 0xFF;
				feature.Form_O_Opcode = 0x40;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::invlpg:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M_Strict;
				feature.Form_RegOpcode = 7;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x01;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::invpcid:
			{
				InstrFeatures feature = { 0 };

				// Special processing for INVPCID (only r32 and r64 are allowed)

				if (IsReg8(info.params[0]) || IsReg16(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Extended_Opcode2 = 0x38;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0x82;

				AddPrefixByte(info, 0x66);
				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::jmp:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_Rel8 | InstrForm::Form_Rel16 | InstrForm::Form_Rel32 | InstrForm::Form_M;
				feature.Form_Rel_Opcode8 = 0xEB;
				feature.Form_Rel_Opcode16_32 = 0xE9;
				feature.Form_RegOpcode = 4;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0xFF;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::jmpfar:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_Far16 | InstrForm::Form_Far32 | InstrForm::Form_M;
				feature.Form_FarPtr_Opcode = 0xEA;
				feature.Form_RegOpcode = 5;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0xFF;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::lar:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0x02;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::lds:
			{
				Invalid();
				break;
			}

			case Instruction::lea:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0x8D;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::les:
			{
				Invalid();
				break;
			}

			case Instruction::lfs:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xB4;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::lgdt:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 2;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x01;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::lgs:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xB5;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::lidt:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 3;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x01;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::lldt:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 2;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x00;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::lmsw:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 6;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x01;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::lsl:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0x03;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::lss:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xB2;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::ltr:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 3;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x00;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::mov:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_MR | InstrForm::Form_RM | InstrForm::Form_MSr | InstrForm::Form_SrM | InstrForm::Form_FD | InstrForm::Form_TD | InstrForm::Form_OI | InstrForm::Form_MI;
				feature.Form_RegOpcode = 0;
				feature.Form_MR_Opcode8 = 0x88;
				feature.Form_MR_Opcode16_64 = 0x89;
				feature.Form_MSr_Opcode = 0x8C;
				feature.Form_RM_Opcode8 = 0x8A;
				feature.Form_RM_Opcode16_64 = 0x8B;
				feature.Form_SrM_Opcode = 0x8E;
				feature.Form_FD_Opcode8 = 0xA0;
				feature.Form_FD_Opcode16_64 = 0xA1;
				feature.Form_TD_Opcode8 = 0xA2;
				feature.Form_TD_Opcode16_64 = 0xA3;
				feature.Form_OI_Opcode8 = 0xB0;
				feature.Form_OI_Opcode16_64 = 0xB8;
				feature.Form_MI_Opcode8 = 0xC6;
				feature.Form_MI_Opcode16_64 = 0xC7;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::movbe:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]) || IsReg8(info.params[1]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM | InstrForm::Form_MR;
				feature.Extended_Opcode = 0x0F;
				feature.Extended_Opcode2 = 0x38;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xF0;
				feature.Form_MR_Opcode8 = UnusedOpcode;
				feature.Form_MR_Opcode16_64 = 0xF1;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::movsx:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RMX;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = 0xBE;
				feature.Form_RM_Opcode16_64 = 0xBF;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::movsxd:
			{
				InstrFeatures feature = { 0 };

				if (!IsReg64(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RMX;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0x63;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::movzx:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RMX;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = 0xB6;
				feature.Form_RM_Opcode16_64 = 0xB7;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::mul:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 4;
				feature.Form_M_Opcode8 = 0xF6;
				feature.Form_M_Opcode16_64 = 0xF7;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::_not:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 2;
				feature.Form_M_Opcode8 = 0xF6;
				feature.Form_M_Opcode16_64 = 0xF7;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::_or:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_I | InstrForm::Form_MI | InstrForm::Form_MR | InstrForm::Form_RM;
				feature.Form_RegOpcode = 1;
				feature.Form_I_Opcode8 = 0x0C;
				feature.Form_I_Opcode16_64 = 0x0D;
				feature.Form_MI_Opcode8 = 0x80;
				feature.Form_MI_Opcode16_64 = 0x81;
				feature.Form_MI_Opcode_SImm8 = 0x83;
				feature.Form_MR_Opcode8 = 0x08;
				feature.Form_MR_Opcode16_64 = 0x09;
				feature.Form_RM_Opcode8 = 0x0A;
				feature.Form_RM_Opcode16_64 = 0x0B;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::pop:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M | InstrForm::Form_O;
				feature.Form_RegOpcode = 0;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x8F;
				feature.Form_O_Opcode = 0x58;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::push:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M | InstrForm::Form_O | InstrForm::Form_I;
				feature.Form_RegOpcode = 6;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0xFF;
				feature.Form_O_Opcode = 0x50;
				feature.Form_I_Opcode8 = 0x6A;
				feature.Form_I_Opcode16_64 = 0x68;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::rcl:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RotSh;
				feature.Form_RegOpcode = 2;
				feature.Form_M1_Opcode8 = 0xD0;
				feature.Form_M1_Opcode16_64 = 0xD1;
				feature.Form_MC_Opcode8 = 0xD2;
				feature.Form_MC_Opcode16_64 = 0xD3;
				feature.Form_MI_Opcode8 = 0xC0;
				feature.Form_MI_Opcode16_64 = 0xC1;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::rcr:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RotSh;
				feature.Form_RegOpcode = 3;
				feature.Form_M1_Opcode8 = 0xD0;
				feature.Form_M1_Opcode16_64 = 0xD1;
				feature.Form_MC_Opcode8 = 0xD2;
				feature.Form_MC_Opcode16_64 = 0xD3;
				feature.Form_MI_Opcode8 = 0xC0;
				feature.Form_MI_Opcode16_64 = 0xC1;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::rol:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RotSh;
				feature.Form_RegOpcode = 0;
				feature.Form_M1_Opcode8 = 0xD0;
				feature.Form_M1_Opcode16_64 = 0xD1;
				feature.Form_MC_Opcode8 = 0xD2;
				feature.Form_MC_Opcode16_64 = 0xD3;
				feature.Form_MI_Opcode8 = 0xC0;
				feature.Form_MI_Opcode16_64 = 0xC1;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::ror:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RotSh;
				feature.Form_RegOpcode = 1;
				feature.Form_M1_Opcode8 = 0xD0;
				feature.Form_M1_Opcode16_64 = 0xD1;
				feature.Form_MC_Opcode8 = 0xD2;
				feature.Form_MC_Opcode16_64 = 0xD3;
				feature.Form_MI_Opcode8 = 0xC0;
				feature.Form_MI_Opcode16_64 = 0xC1;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::sal:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RotSh;
				feature.Form_RegOpcode = 4;
				feature.Form_M1_Opcode8 = 0xD0;
				feature.Form_M1_Opcode16_64 = 0xD1;
				feature.Form_MC_Opcode8 = 0xD2;
				feature.Form_MC_Opcode16_64 = 0xD3;
				feature.Form_MI_Opcode8 = 0xC0;
				feature.Form_MI_Opcode16_64 = 0xC1;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::sar:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RotSh;
				feature.Form_RegOpcode = 7;
				feature.Form_M1_Opcode8 = 0xD0;
				feature.Form_M1_Opcode16_64 = 0xD1;
				feature.Form_MC_Opcode8 = 0xD2;
				feature.Form_MC_Opcode16_64 = 0xD3;
				feature.Form_MI_Opcode8 = 0xC0;
				feature.Form_MI_Opcode16_64 = 0xC1;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::sbb:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_I | InstrForm::Form_MI | InstrForm::Form_MR | InstrForm::Form_RM;
				feature.Form_RegOpcode = 3;
				feature.Form_I_Opcode8 = 0x1C;
				feature.Form_I_Opcode16_64 = 0x1D;
				feature.Form_MI_Opcode8 = 0x80;
				feature.Form_MI_Opcode16_64 = 0x81;
				feature.Form_MI_Opcode_SImm8 = 0x83;
				feature.Form_MR_Opcode8 = 0x18;
				feature.Form_MR_Opcode16_64 = 0x19;
				feature.Form_RM_Opcode8 = 0x1A;
				feature.Form_RM_Opcode16_64 = 0x1B;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::seto:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x90;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::setno:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x91;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::setb:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x92;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::setae:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x93;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::sete:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x94;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::setne:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x95;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::setbe:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x96;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::seta:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x97;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::sets:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x98;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::setns:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x99;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::setp:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x9A;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::setnp:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x9B;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::setl:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x9C;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::setge:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x9D;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::setle:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x9E;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::setg:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = 0x9F;
				feature.Form_M_Opcode16_64 = UnusedOpcode;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::sgdt:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 0;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x01;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::shl:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RotSh;
				feature.Form_RegOpcode = 4;
				feature.Form_M1_Opcode8 = 0xD0;
				feature.Form_M1_Opcode16_64 = 0xD1;
				feature.Form_MC_Opcode8 = 0xD2;
				feature.Form_MC_Opcode16_64 = 0xD3;
				feature.Form_MI_Opcode8 = 0xC0;
				feature.Form_MI_Opcode16_64 = 0xC1;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::shld:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_Shd;
				feature.Extended_Opcode = 0x0F;
				feature.Form_MRI_Opcode = 0xA4;
				feature.Form_MRC_Opcode = 0xA5;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::shr:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_RotSh;
				feature.Form_RegOpcode = 5;
				feature.Form_M1_Opcode8 = 0xD0;
				feature.Form_M1_Opcode16_64 = 0xD1;
				feature.Form_MC_Opcode8 = 0xD2;
				feature.Form_MC_Opcode16_64 = 0xD3;
				feature.Form_MI_Opcode8 = 0xC0;
				feature.Form_MI_Opcode16_64 = 0xC1;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::shrd:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_Shd;
				feature.Extended_Opcode = 0x0F;
				feature.Form_MRI_Opcode = 0xAC;
				feature.Form_MRC_Opcode = 0xAD;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::sidt:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 1;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x01;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::sldt:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 0;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x00;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::smsw:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 4;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x01;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::str:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 1;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x00;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::sub:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_I | InstrForm::Form_MI | InstrForm::Form_MR | InstrForm::Form_RM;
				feature.Form_RegOpcode = 5;
				feature.Form_I_Opcode8 = 0x2C;
				feature.Form_I_Opcode16_64 = 0x2D;
				feature.Form_MI_Opcode8 = 0x80;
				feature.Form_MI_Opcode16_64 = 0x81;
				feature.Form_MI_Opcode_SImm8 = 0x83;
				feature.Form_MR_Opcode8 = 0x28;
				feature.Form_MR_Opcode16_64 = 0x29;
				feature.Form_RM_Opcode8 = 0x2A;
				feature.Form_RM_Opcode16_64 = 0x2B;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::test:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_I | InstrForm::Form_MI | InstrForm::Form_MR;
				feature.Form_RegOpcode = 0;
				feature.Form_I_Opcode8 = 0xA8;
				feature.Form_I_Opcode16_64 = 0xA9;
				feature.Form_MI_Opcode8 = 0xF6;
				feature.Form_MI_Opcode16_64 = 0xF7;
				feature.Form_MI_Opcode_SImm8 = UnusedOpcode;
				feature.Form_MR_Opcode8 = 0x84;
				feature.Form_MR_Opcode16_64 = 0x85;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::ud0:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]) || IsReg64(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xFF;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::ud1:
			{
				InstrFeatures feature = { 0 };

				if (IsReg8(info.params[0]) || IsReg64(info.params[0]))
				{
					throw "Invalid parameter";
				}

				feature.forms = InstrForm::Form_RM;
				feature.Extended_Opcode = 0x0F;
				feature.Form_RM_Opcode8 = UnusedOpcode;
				feature.Form_RM_Opcode16_64 = 0xB9;

				ProcessGpInstr(info, 64, feature);
				break;
			}


			case Instruction::verr:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 4;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x00;

				ProcessGpInstr(info, 64, feature);
				break;
			}

			case Instruction::verw:
			{
				InstrFeatures feature = { 0 };

				feature.forms = InstrForm::Form_M;
				feature.Form_RegOpcode = 5;
				feature.Extended_Opcode = 0x0F;
				feature.Form_M_Opcode8 = UnusedOpcode;
				feature.Form_M_Opcode16_64 = 0x00;

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
			case Instruction::nop:
			{
				if (info.numParams == 0)
				{
					OneByte(info, 0x90);
				}
				else
				{
					InstrFeatures feature = { 0 };

					feature.forms = InstrForm::Form_M;
					feature.Form_RegOpcode = 0;
					feature.Extended_Opcode = 0x0F;
					feature.Form_M_Opcode8 = UnusedOpcode;
					feature.Form_M_Opcode16_64 = 0x1F;

					ProcessGpInstr(info, 64, feature);
				}
				break;
			}
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

	/// \cond DO_NOT_DOCUMENT

	// Instructions using ModRM / with an immediate operand

	template <> AnalyzeInfo IntelAssembler::adc<16>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::adc;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::adc<32>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::adc;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::adc<64>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::adc;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::add<16>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::add;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::add<32>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::add;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::add<64>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::add;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::_and<16>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::_and;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::_and<32>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::_and;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::_and<64>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::_and;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::arpl<16>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::arpl;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::arpl<32>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::arpl;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::arpl<64>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::arpl;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::bound<16>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::bound;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::bound<32>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::bound;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::bound<64>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::bound;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::bsf<16>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::bsf;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::bsf<32>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::bsf;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::bsf<64>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::bsf;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::bsr<16>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::bsr;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::bsr<32>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::bsr;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::bsr<64>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::bsr;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::bt<16>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::bt;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::bt<32>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::bt;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::bt<64>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::bt;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::btc<16>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::btc;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::btc<32>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::btc;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::btc<64>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::btc;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::btr<16>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::btr;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::btr<32>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::btr;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::btr<64>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::btr;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::bts<16>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::bts;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::bts<32>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::bts;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::bts<64>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::bts;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::call<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::call;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p) || IsRel(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::call<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::call;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p) || IsRel(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::call<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::call;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p) || IsRel(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::callf<16>(Param p, uint16_t seg, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::callfar;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsFarPtr(p)) info.Imm.uimm16 = seg;
		if (IsMemDisp(p) || IsFarPtr(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::callf<32>(Param p, uint16_t seg, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::callfar;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsFarPtr(p)) info.Imm.uimm16 = seg;
		if (IsMemDisp(p) || IsFarPtr(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::callf<64>(Param p, uint16_t seg, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::callfar;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsFarPtr(p)) info.Imm.uimm16 = seg;
		if (IsMemDisp(p) || IsFarPtr(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cmp<16>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cmp;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cmp<32>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cmp;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cmp<64>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cmp;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cmpxchg<16>(Param to, Param from, uint64_t disp, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cmpxchg;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cmpxchg<32>(Param to, Param from, uint64_t disp, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cmpxchg;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::cmpxchg<64>(Param to, Param from, uint64_t disp, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::cmpxchg;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::dec<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::dec;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::dec<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::dec;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::dec<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::dec;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::div<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::div;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::div<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::div;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::div<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::div;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::idiv<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::idiv;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::idiv<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::idiv;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::idiv<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::idiv;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::imul<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::imul;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::imul<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::imul;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::imul<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::imul;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::imul<16>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::imul;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::imul<32>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::imul;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::imul<64>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::imul;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::imul<16>(Param to, Param from, Param i, uint64_t disp, int32_t imm, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::imul;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		info.params[info.numParams++] = i;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		info.Imm.simm32 = imm;
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::imul<32>(Param to, Param from, Param i, uint64_t disp, int32_t imm, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::imul;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		info.params[info.numParams++] = i;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		info.Imm.simm32 = imm;
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::imul<64>(Param to, Param from, Param i, uint64_t disp, int32_t imm, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::imul;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		info.params[info.numParams++] = i;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		info.Imm.simm32 = imm;
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::inc<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::inc;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::inc<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::inc;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::inc<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::inc;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::invlpg<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::invlpg;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::invlpg<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::invlpg;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::invlpg<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::invlpg;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::invpcid<16>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::invpcid;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::invpcid<32>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::invpcid;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::invpcid<64>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::invpcid;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::jmp<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::jmp;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p) || IsRel(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::jmp<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::jmp;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p) || IsRel(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::jmp<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::jmp;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p) || IsRel(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::jmpf<16>(Param p, uint16_t seg, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::jmpfar;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsFarPtr(p)) info.Imm.uimm16 = seg;
		if (IsMemDisp(p) || IsFarPtr(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::jmpf<32>(Param p, uint16_t seg, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::jmpfar;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsFarPtr(p)) info.Imm.uimm16 = seg;
		if (IsMemDisp(p) || IsFarPtr(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::jmpf<64>(Param p, uint16_t seg, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::jmpfar;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsFarPtr(p)) info.Imm.uimm16 = seg;
		if (IsMemDisp(p) || IsFarPtr(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lar<16>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lar;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lar<32>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lar;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lar<64>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lar;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lds<16>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lds;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lds<32>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lds;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lds<64>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lds;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lea<16>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lea;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lea<32>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lea;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lea<64>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lea;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::les<16>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::les;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::les<32>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::les;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::les<64>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::les;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lfs<16>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lfs;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lfs<32>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lfs;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lfs<64>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lfs;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lgdt<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lgdt;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lgdt<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lgdt;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lgdt<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lgdt;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lgs<16>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lgs;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lgs<32>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lgs;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lgs<64>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lgs;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lidt<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lidt;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lidt<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lidt;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lidt<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lidt;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lldt<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lldt;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lldt<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lldt;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lldt<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lldt;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lmsw<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lmsw;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lmsw<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lmsw;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lmsw<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lmsw;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lsl<16>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lsl;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lsl<32>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lsl;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lsl<64>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lsl;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lss<16>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lss;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lss<32>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lss;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::lss<64>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::lss;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ltr<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ltr;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ltr<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ltr;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ltr<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ltr;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::mov<16>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::mov;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to) || IsMemDisp(from) || IsMoffs(to) || IsMoffs(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::mov<32>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::mov;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to) || IsMemDisp(from) || IsMoffs(to) || IsMoffs(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::mov<64>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::mov;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to) || IsMemDisp(from) || IsMoffs(to) || IsMoffs(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::movbe<16>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::movbe;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::movbe<32>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::movbe;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::movbe<64>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::movbe;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::movsx<16>(Param to, Param from, uint64_t disp, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::movsx;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::movsx<32>(Param to, Param from, uint64_t disp, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::movsx;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::movsx<64>(Param to, Param from, uint64_t disp, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::movsx;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::movsxd<16>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::movsxd;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::movsxd<32>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::movsxd;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::movsxd<64>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::movsxd;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::movzx<16>(Param to, Param from, uint64_t disp, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::movzx;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::movzx<32>(Param to, Param from, uint64_t disp, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::movzx;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::movzx<64>(Param to, Param from, uint64_t disp, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::movzx;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::mul<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::mul;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::mul<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::mul;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::mul<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::mul;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::nop<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::nop;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::nop<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::nop;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::nop<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::nop;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::_not<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::_not;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::_not<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::_not;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::_not<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::_not;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::_or<16>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::_or;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::_or<32>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::_or;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::_or<64>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::_or;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::pop<16>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::pop;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::pop<32>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::pop;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::pop<64>(Param p, PtrHint ptrHint, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::pop;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::push<16>(Param p, PtrHint ptrHint, uint64_t disp, int32_t imm, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::push;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		info.Imm.simm32 = imm;
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::push<32>(Param p, PtrHint ptrHint, uint64_t disp, int32_t imm, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::push;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		info.Imm.simm32 = imm;
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::push<64>(Param p, PtrHint ptrHint, uint64_t disp, int32_t imm, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::push;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		info.Imm.simm32 = imm;
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::rcl<16>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::rcl;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::rcl<32>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::rcl;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::rcl<64>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::rcl;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::rcr<16>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::rcr;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::rcr<32>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::rcr;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::rcr<64>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::rcr;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::rol<16>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::rol;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::rol<32>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::rol;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::rol<64>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::rol;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ror<16>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::ror;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ror<32>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::ror;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ror<64>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::ror;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sal<16>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::sal;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sal<32>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::sal;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sal<64>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::sal;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sar<16>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::sar;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sar<32>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::sar;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sar<64>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::sar;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sbb<16>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sbb;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sbb<32>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sbb;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sbb<64>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sbb;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::seta<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::seta;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::seta<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::seta;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::seta<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::seta;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setae<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setae;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setae<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setae;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setae<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setae;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setb<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setb;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setb<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setb;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setb<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setb;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setbe<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setbe;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setbe<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setbe;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setbe<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setbe;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setc<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setc;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setc<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setc;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setc<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setc;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sete<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::sete;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sete<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::sete;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sete<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::sete;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setg<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setg;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setg<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setg;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setg<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setg;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setge<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setge;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setge<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setge;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setge<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setge;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setl<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setl;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setl<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setl;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setl<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setl;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setle<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setle;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setle<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setle;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setle<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setle;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setna<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setna;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setna<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setna;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setna<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setna;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnae<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnae;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnae<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnae;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnae<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnae;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnb<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnb;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnb<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnb;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnb<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnb;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnbe<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnbe;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnbe<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnbe;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnbe<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnbe;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnc<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnc;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnc<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnc;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnc<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnc;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setne<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setne;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setne<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setne;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setne<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setne;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setng<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setng;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setng<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setng;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setng<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setng;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnge<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnge;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnge<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnge;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnge<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnge;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnl<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnl;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnl<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnl;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnl<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnl;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnle<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnle;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnle<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnle;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnle<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnle;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setno<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setno;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setno<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setno;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setno<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setno;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnp<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnp;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnp<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnp;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnp<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnp;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setns<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setns;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setns<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setns;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setns<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setns;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnz<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnz;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnz<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnz;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setnz<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setnz;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::seto<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::seto;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::seto<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::seto;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::seto<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::seto;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setp<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setp;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setp<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setp;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setp<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setp;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setpe<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setpe;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setpe<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setpe;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setpe<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setpe;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setpo<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setpo;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setpo<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setpo;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setpo<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setpo;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sets<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::sets;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sets<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::sets;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sets<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::sets;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setz<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setz;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setz<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setz;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::setz<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = PtrHint::BytePtr;
		info.instr = Instruction::setz;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sgdt<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sgdt;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sgdt<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sgdt;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sgdt<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sgdt;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::shl<16>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::shl;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::shl<32>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::shl;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::shl<64>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::shl;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::shld<16>(Param to, Param from, Param c, uint64_t disp, int32_t imm, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::shld;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		info.params[info.numParams++] = c;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		info.Imm.simm32 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::shld<32>(Param to, Param from, Param c, uint64_t disp, int32_t imm, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::shld;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		info.params[info.numParams++] = c;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		info.Imm.simm32 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::shld<64>(Param to, Param from, Param c, uint64_t disp, int32_t imm, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::shld;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		info.params[info.numParams++] = c;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		info.Imm.simm32 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::shr<16>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::shr;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::shr<32>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::shr;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::shr<64>(Param to, Param from, uint64_t disp, int64_t imm, PtrHint ptrHint, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.ptrHint = ptrHint;
		info.instr = Instruction::shr;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsImm(from)) info.Imm.simm64 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::shrd<16>(Param to, Param from, Param c, uint64_t disp, int32_t imm, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::shrd;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		info.params[info.numParams++] = c;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		info.Imm.simm32 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::shrd<32>(Param to, Param from, Param c, uint64_t disp, int32_t imm, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::shrd;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		info.params[info.numParams++] = c;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		info.Imm.simm32 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::shrd<64>(Param to, Param from, Param c, uint64_t disp, int32_t imm, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::shrd;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		info.params[info.numParams++] = c;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		info.Imm.simm32 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sidt<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sidt;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sidt<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sidt;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sidt<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sidt;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sldt<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sldt;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sldt<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sldt;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sldt<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sldt;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::smsw<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::smsw;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::smsw<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::smsw;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::smsw<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::smsw;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::str<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::str;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::str<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::str;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::str<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::str;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sub<16>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sub;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sub<32>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sub;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::sub<64>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::sub;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to) || IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::test<16>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::test;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::test<32>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::test;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::test<64>(Param to, Param from, uint64_t disp, int32_t imm, Prefix sr, Prefix lock)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::test;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (lock != Prefix::NoPrefix) AddPrefix(info, lock);
		if (IsImm(from)) info.Imm.simm32 = imm;
		if (IsMemDisp(to)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ud0<16>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ud0;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ud0<32>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ud0;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ud0<64>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ud0;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ud1<16>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ud1;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ud1<32>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ud1;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::ud1<64>(Param to, Param from, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::ud1;
		info.params[info.numParams++] = to;
		info.params[info.numParams++] = from;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(from)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}


	template <> AnalyzeInfo IntelAssembler::verr<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::verr;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::verr<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::verr;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::verr<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::verr;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble64(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::verw<16>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::verw;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::verw<32>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::verw;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::verw<64>(Param p, uint64_t disp, Prefix sr)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::verw;
		info.params[info.numParams++] = p;
		if (sr != Prefix::NoPrefix) AddPrefix(info, sr);
		if (IsMemDisp(p)) info.Disp.disp64 = disp;
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

	template <> AnalyzeInfo IntelAssembler::nop<16>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::nop;
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::nop<32>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::nop;
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo IntelAssembler::nop<64>(Prefix pre)
	{
		AnalyzeInfo info = { 0 };
		info.instr = Instruction::nop;
		if (pre != Prefix::NoPrefix) AddPrefix(info, pre);
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

	/// \endcond

#pragma endregion "Quick helpers"

}
