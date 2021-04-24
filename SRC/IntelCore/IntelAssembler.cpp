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

	bool IntelAssembler::IsImm(Param p)
	{
		return Param::ImmStart <= p && p <= Param::ImmEnd;
	}

	bool IntelAssembler::IsReg(Param p)
	{
		return Param::RegStart <= p && p <= Param::RegEnd;
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
		switch (p)
		{
			case Param::m_bx_si_disp8: case Param::m_bx_di_disp8: case Param::m_bp_si_disp8: case Param::m_bp_di_disp8: case Param::m_si_disp8: case Param::m_di_disp8: case Param::m_bp_disp8: case Param::m_bx_disp8:
			case Param::m_eax_disp8: case Param::m_ecx_disp8: case Param::m_edx_disp8: case Param::m_ebx_disp8: case Param::m_ebp_disp8: case Param::m_esi_disp8: case Param::m_edi_disp8:
			case Param::m_rax_disp8: case Param::m_rcx_disp8: case Param::m_rdx_disp8: case Param::m_rbx_disp8: case Param::m_rbp_disp8: case Param::m_rsi_disp8: case Param::m_rdi_disp8:

			case Param::sib_eax_eax_disp8: case Param::sib_eax_ecx_disp8: case Param::sib_eax_edx_disp8: case Param::sib_eax_ebx_disp8: case Param::sib_eax_esp_disp8: case Param::sib_eax_ebp_disp8: case Param::sib_eax_esi_disp8: case Param::sib_eax_edi_disp8: case Param::sib_eax_r8d_disp8: case Param::sib_eax_r9d_disp8: case Param::sib_eax_r10d_disp8: case Param::sib_eax_r11d_disp8: case Param::sib_eax_r12d_disp8: case Param::sib_eax_r13d_disp8: case Param::sib_eax_r14d_disp8: case Param::sib_eax_r15d_disp8:
			case Param::sib_ecx_eax_disp8: case Param::sib_ecx_ecx_disp8: case Param::sib_ecx_edx_disp8: case Param::sib_ecx_ebx_disp8: case Param::sib_ecx_esp_disp8: case Param::sib_ecx_ebp_disp8: case Param::sib_ecx_esi_disp8: case Param::sib_ecx_edi_disp8: case Param::sib_ecx_r8d_disp8: case Param::sib_ecx_r9d_disp8: case Param::sib_ecx_r10d_disp8: case Param::sib_ecx_r11d_disp8: case Param::sib_ecx_r12d_disp8: case Param::sib_ecx_r13d_disp8: case Param::sib_ecx_r14d_disp8: case Param::sib_ecx_r15d_disp8:
			case Param::sib_edx_eax_disp8: case Param::sib_edx_ecx_disp8: case Param::sib_edx_edx_disp8: case Param::sib_edx_ebx_disp8: case Param::sib_edx_esp_disp8: case Param::sib_edx_ebp_disp8: case Param::sib_edx_esi_disp8: case Param::sib_edx_edi_disp8: case Param::sib_edx_r8d_disp8: case Param::sib_edx_r9d_disp8: case Param::sib_edx_r10d_disp8: case Param::sib_edx_r11d_disp8: case Param::sib_edx_r12d_disp8: case Param::sib_edx_r13d_disp8: case Param::sib_edx_r14d_disp8: case Param::sib_edx_r15d_disp8:
			case Param::sib_ebx_eax_disp8: case Param::sib_ebx_ecx_disp8: case Param::sib_ebx_edx_disp8: case Param::sib_ebx_ebx_disp8: case Param::sib_ebx_esp_disp8: case Param::sib_ebx_ebp_disp8: case Param::sib_ebx_esi_disp8: case Param::sib_ebx_edi_disp8: case Param::sib_ebx_r8d_disp8: case Param::sib_ebx_r9d_disp8: case Param::sib_ebx_r10d_disp8: case Param::sib_ebx_r11d_disp8: case Param::sib_ebx_r12d_disp8: case Param::sib_ebx_r13d_disp8: case Param::sib_ebx_r14d_disp8: case Param::sib_ebx_r15d_disp8:
			case Param::sib_none_eax_disp8: case Param::sib_none_ecx_disp8: case Param::sib_none_edx_disp8: case Param::sib_none_ebx_disp8: case Param::sib_none_esp_disp8: case Param::sib_none_ebp_disp8: case Param::sib_none_esi_disp8: case Param::sib_none_edi_disp8: case Param::sib_none_r8d_disp8: case Param::sib_none_r9d_disp8: case Param::sib_none_r10d_disp8: case Param::sib_none_r11d_disp8: case Param::sib_none_r12d_disp8: case Param::sib_none_r13d_disp8: case Param::sib_none_r14d_disp8: case Param::sib_none_r15d_disp8:
			case Param::sib_ebp_eax_disp8: case Param::sib_ebp_ecx_disp8: case Param::sib_ebp_edx_disp8: case Param::sib_ebp_ebx_disp8: case Param::sib_ebp_esp_disp8: case Param::sib_ebp_ebp_disp8: case Param::sib_ebp_esi_disp8: case Param::sib_ebp_edi_disp8: case Param::sib_ebp_r8d_disp8: case Param::sib_ebp_r9d_disp8: case Param::sib_ebp_r10d_disp8: case Param::sib_ebp_r11d_disp8: case Param::sib_ebp_r12d_disp8: case Param::sib_ebp_r13d_disp8: case Param::sib_ebp_r14d_disp8: case Param::sib_ebp_r15d_disp8:
			case Param::sib_esi_eax_disp8: case Param::sib_esi_ecx_disp8: case Param::sib_esi_edx_disp8: case Param::sib_esi_ebx_disp8: case Param::sib_esi_esp_disp8: case Param::sib_esi_ebp_disp8: case Param::sib_esi_esi_disp8: case Param::sib_esi_edi_disp8: case Param::sib_esi_r8d_disp8: case Param::sib_esi_r9d_disp8: case Param::sib_esi_r10d_disp8: case Param::sib_esi_r11d_disp8: case Param::sib_esi_r12d_disp8: case Param::sib_esi_r13d_disp8: case Param::sib_esi_r14d_disp8: case Param::sib_esi_r15d_disp8:
			case Param::sib_edi_eax_disp8: case Param::sib_edi_ecx_disp8: case Param::sib_edi_edx_disp8: case Param::sib_edi_ebx_disp8: case Param::sib_edi_esp_disp8: case Param::sib_edi_ebp_disp8: case Param::sib_edi_esi_disp8: case Param::sib_edi_edi_disp8: case Param::sib_edi_r8d_disp8: case Param::sib_edi_r9d_disp8: case Param::sib_edi_r10d_disp8: case Param::sib_edi_r11d_disp8: case Param::sib_edi_r12d_disp8: case Param::sib_edi_r13d_disp8: case Param::sib_edi_r14d_disp8: case Param::sib_edi_r15d_disp8:
			case Param::sib_r8d_eax_disp8: case Param::sib_r8d_ecx_disp8: case Param::sib_r8d_edx_disp8: case Param::sib_r8d_ebx_disp8: case Param::sib_r8d_esp_disp8: case Param::sib_r8d_ebp_disp8: case Param::sib_r8d_esi_disp8: case Param::sib_r8d_edi_disp8: case Param::sib_r8d_r8d_disp8: case Param::sib_r8d_r9d_disp8: case Param::sib_r8d_r10d_disp8: case Param::sib_r8d_r11d_disp8: case Param::sib_r8d_r12d_disp8: case Param::sib_r8d_r13d_disp8: case Param::sib_r8d_r14d_disp8: case Param::sib_r8d_r15d_disp8:
			case Param::sib_r9d_eax_disp8: case Param::sib_r9d_ecx_disp8: case Param::sib_r9d_edx_disp8: case Param::sib_r9d_ebx_disp8: case Param::sib_r9d_esp_disp8: case Param::sib_r9d_ebp_disp8: case Param::sib_r9d_esi_disp8: case Param::sib_r9d_edi_disp8: case Param::sib_r9d_r8d_disp8: case Param::sib_r9d_r9d_disp8: case Param::sib_r9d_r10d_disp8: case Param::sib_r9d_r11d_disp8: case Param::sib_r9d_r12d_disp8: case Param::sib_r9d_r13d_disp8: case Param::sib_r9d_r14d_disp8: case Param::sib_r9d_r15d_disp8:
			case Param::sib_r10d_eax_disp8: case Param::sib_r10d_ecx_disp8: case Param::sib_r10d_edx_disp8: case Param::sib_r10d_ebx_disp8: case Param::sib_r10d_esp_disp8: case Param::sib_r10d_ebp_disp8: case Param::sib_r10d_esi_disp8: case Param::sib_r10d_edi_disp8: case Param::sib_r10d_r8d_disp8: case Param::sib_r10d_r9d_disp8: case Param::sib_r10d_r10d_disp8: case Param::sib_r10d_r11d_disp8: case Param::sib_r10d_r12d_disp8: case Param::sib_r10d_r13d_disp8: case Param::sib_r10d_r14d_disp8: case Param::sib_r10d_r15d_disp8:
			case Param::sib_r11d_eax_disp8: case Param::sib_r11d_ecx_disp8: case Param::sib_r11d_edx_disp8: case Param::sib_r11d_ebx_disp8: case Param::sib_r11d_esp_disp8: case Param::sib_r11d_ebp_disp8: case Param::sib_r11d_esi_disp8: case Param::sib_r11d_edi_disp8: case Param::sib_r11d_r8d_disp8: case Param::sib_r11d_r9d_disp8: case Param::sib_r11d_r10d_disp8: case Param::sib_r11d_r11d_disp8: case Param::sib_r11d_r12d_disp8: case Param::sib_r11d_r13d_disp8: case Param::sib_r11d_r14d_disp8: case Param::sib_r11d_r15d_disp8:
			case Param::sib_r12d_eax_disp8: case Param::sib_r12d_ecx_disp8: case Param::sib_r12d_edx_disp8: case Param::sib_r12d_ebx_disp8: case Param::sib_r12d_esp_disp8: case Param::sib_r12d_ebp_disp8: case Param::sib_r12d_esi_disp8: case Param::sib_r12d_edi_disp8: case Param::sib_r12d_r8d_disp8: case Param::sib_r12d_r9d_disp8: case Param::sib_r12d_r10d_disp8: case Param::sib_r12d_r11d_disp8: case Param::sib_r12d_r12d_disp8: case Param::sib_r12d_r13d_disp8: case Param::sib_r12d_r14d_disp8: case Param::sib_r12d_r15d_disp8:
			case Param::sib_r13d_eax_disp8: case Param::sib_r13d_ecx_disp8: case Param::sib_r13d_edx_disp8: case Param::sib_r13d_ebx_disp8: case Param::sib_r13d_esp_disp8: case Param::sib_r13d_ebp_disp8: case Param::sib_r13d_esi_disp8: case Param::sib_r13d_edi_disp8: case Param::sib_r13d_r8d_disp8: case Param::sib_r13d_r9d_disp8: case Param::sib_r13d_r10d_disp8: case Param::sib_r13d_r11d_disp8: case Param::sib_r13d_r12d_disp8: case Param::sib_r13d_r13d_disp8: case Param::sib_r13d_r14d_disp8: case Param::sib_r13d_r15d_disp8:
			case Param::sib_r14d_eax_disp8: case Param::sib_r14d_ecx_disp8: case Param::sib_r14d_edx_disp8: case Param::sib_r14d_ebx_disp8: case Param::sib_r14d_esp_disp8: case Param::sib_r14d_ebp_disp8: case Param::sib_r14d_esi_disp8: case Param::sib_r14d_edi_disp8: case Param::sib_r14d_r8d_disp8: case Param::sib_r14d_r9d_disp8: case Param::sib_r14d_r10d_disp8: case Param::sib_r14d_r11d_disp8: case Param::sib_r14d_r12d_disp8: case Param::sib_r14d_r13d_disp8: case Param::sib_r14d_r14d_disp8: case Param::sib_r14d_r15d_disp8:
			case Param::sib_r15d_eax_disp8: case Param::sib_r15d_ecx_disp8: case Param::sib_r15d_edx_disp8: case Param::sib_r15d_ebx_disp8: case Param::sib_r15d_esp_disp8: case Param::sib_r15d_ebp_disp8: case Param::sib_r15d_esi_disp8: case Param::sib_r15d_edi_disp8: case Param::sib_r15d_r8d_disp8: case Param::sib_r15d_r9d_disp8: case Param::sib_r15d_r10d_disp8: case Param::sib_r15d_r11d_disp8: case Param::sib_r15d_r12d_disp8: case Param::sib_r15d_r13d_disp8: case Param::sib_r15d_r14d_disp8: case Param::sib_r15d_r15d_disp8:
			case Param::sib_eax_2_eax_disp8: case Param::sib_eax_2_ecx_disp8: case Param::sib_eax_2_edx_disp8: case Param::sib_eax_2_ebx_disp8: case Param::sib_eax_2_esp_disp8: case Param::sib_eax_2_ebp_disp8: case Param::sib_eax_2_esi_disp8: case Param::sib_eax_2_edi_disp8: case Param::sib_eax_2_r8d_disp8: case Param::sib_eax_2_r9d_disp8: case Param::sib_eax_2_r10d_disp8: case Param::sib_eax_2_r11d_disp8: case Param::sib_eax_2_r12d_disp8: case Param::sib_eax_2_r13d_disp8: case Param::sib_eax_2_r14d_disp8: case Param::sib_eax_2_r15d_disp8:
			case Param::sib_ecx_2_eax_disp8: case Param::sib_ecx_2_ecx_disp8: case Param::sib_ecx_2_edx_disp8: case Param::sib_ecx_2_ebx_disp8: case Param::sib_ecx_2_esp_disp8: case Param::sib_ecx_2_ebp_disp8: case Param::sib_ecx_2_esi_disp8: case Param::sib_ecx_2_edi_disp8: case Param::sib_ecx_2_r8d_disp8: case Param::sib_ecx_2_r9d_disp8: case Param::sib_ecx_2_r10d_disp8: case Param::sib_ecx_2_r11d_disp8: case Param::sib_ecx_2_r12d_disp8: case Param::sib_ecx_2_r13d_disp8: case Param::sib_ecx_2_r14d_disp8: case Param::sib_ecx_2_r15d_disp8:
			case Param::sib_edx_2_eax_disp8: case Param::sib_edx_2_ecx_disp8: case Param::sib_edx_2_edx_disp8: case Param::sib_edx_2_ebx_disp8: case Param::sib_edx_2_esp_disp8: case Param::sib_edx_2_ebp_disp8: case Param::sib_edx_2_esi_disp8: case Param::sib_edx_2_edi_disp8: case Param::sib_edx_2_r8d_disp8: case Param::sib_edx_2_r9d_disp8: case Param::sib_edx_2_r10d_disp8: case Param::sib_edx_2_r11d_disp8: case Param::sib_edx_2_r12d_disp8: case Param::sib_edx_2_r13d_disp8: case Param::sib_edx_2_r14d_disp8: case Param::sib_edx_2_r15d_disp8:
			case Param::sib_ebx_2_eax_disp8: case Param::sib_ebx_2_ecx_disp8: case Param::sib_ebx_2_edx_disp8: case Param::sib_ebx_2_ebx_disp8: case Param::sib_ebx_2_esp_disp8: case Param::sib_ebx_2_ebp_disp8: case Param::sib_ebx_2_esi_disp8: case Param::sib_ebx_2_edi_disp8: case Param::sib_ebx_2_r8d_disp8: case Param::sib_ebx_2_r9d_disp8: case Param::sib_ebx_2_r10d_disp8: case Param::sib_ebx_2_r11d_disp8: case Param::sib_ebx_2_r12d_disp8: case Param::sib_ebx_2_r13d_disp8: case Param::sib_ebx_2_r14d_disp8: case Param::sib_ebx_2_r15d_disp8:
			case Param::sib_ebp_2_eax_disp8: case Param::sib_ebp_2_ecx_disp8: case Param::sib_ebp_2_edx_disp8: case Param::sib_ebp_2_ebx_disp8: case Param::sib_ebp_2_esp_disp8: case Param::sib_ebp_2_ebp_disp8: case Param::sib_ebp_2_esi_disp8: case Param::sib_ebp_2_edi_disp8: case Param::sib_ebp_2_r8d_disp8: case Param::sib_ebp_2_r9d_disp8: case Param::sib_ebp_2_r10d_disp8: case Param::sib_ebp_2_r11d_disp8: case Param::sib_ebp_2_r12d_disp8: case Param::sib_ebp_2_r13d_disp8: case Param::sib_ebp_2_r14d_disp8: case Param::sib_ebp_2_r15d_disp8:
			case Param::sib_esi_2_eax_disp8: case Param::sib_esi_2_ecx_disp8: case Param::sib_esi_2_edx_disp8: case Param::sib_esi_2_ebx_disp8: case Param::sib_esi_2_esp_disp8: case Param::sib_esi_2_ebp_disp8: case Param::sib_esi_2_esi_disp8: case Param::sib_esi_2_edi_disp8: case Param::sib_esi_2_r8d_disp8: case Param::sib_esi_2_r9d_disp8: case Param::sib_esi_2_r10d_disp8: case Param::sib_esi_2_r11d_disp8: case Param::sib_esi_2_r12d_disp8: case Param::sib_esi_2_r13d_disp8: case Param::sib_esi_2_r14d_disp8: case Param::sib_esi_2_r15d_disp8:
			case Param::sib_edi_2_eax_disp8: case Param::sib_edi_2_ecx_disp8: case Param::sib_edi_2_edx_disp8: case Param::sib_edi_2_ebx_disp8: case Param::sib_edi_2_esp_disp8: case Param::sib_edi_2_ebp_disp8: case Param::sib_edi_2_esi_disp8: case Param::sib_edi_2_edi_disp8: case Param::sib_edi_2_r8d_disp8: case Param::sib_edi_2_r9d_disp8: case Param::sib_edi_2_r10d_disp8: case Param::sib_edi_2_r11d_disp8: case Param::sib_edi_2_r12d_disp8: case Param::sib_edi_2_r13d_disp8: case Param::sib_edi_2_r14d_disp8: case Param::sib_edi_2_r15d_disp8:
			case Param::sib_r8d_2_eax_disp8: case Param::sib_r8d_2_ecx_disp8: case Param::sib_r8d_2_edx_disp8: case Param::sib_r8d_2_ebx_disp8: case Param::sib_r8d_2_esp_disp8: case Param::sib_r8d_2_ebp_disp8: case Param::sib_r8d_2_esi_disp8: case Param::sib_r8d_2_edi_disp8: case Param::sib_r8d_2_r8d_disp8: case Param::sib_r8d_2_r9d_disp8: case Param::sib_r8d_2_r10d_disp8: case Param::sib_r8d_2_r11d_disp8: case Param::sib_r8d_2_r12d_disp8: case Param::sib_r8d_2_r13d_disp8: case Param::sib_r8d_2_r14d_disp8: case Param::sib_r8d_2_r15d_disp8:
			case Param::sib_r9d_2_eax_disp8: case Param::sib_r9d_2_ecx_disp8: case Param::sib_r9d_2_edx_disp8: case Param::sib_r9d_2_ebx_disp8: case Param::sib_r9d_2_esp_disp8: case Param::sib_r9d_2_ebp_disp8: case Param::sib_r9d_2_esi_disp8: case Param::sib_r9d_2_edi_disp8: case Param::sib_r9d_2_r8d_disp8: case Param::sib_r9d_2_r9d_disp8: case Param::sib_r9d_2_r10d_disp8: case Param::sib_r9d_2_r11d_disp8: case Param::sib_r9d_2_r12d_disp8: case Param::sib_r9d_2_r13d_disp8: case Param::sib_r9d_2_r14d_disp8: case Param::sib_r9d_2_r15d_disp8:
			case Param::sib_r10d_2_eax_disp8: case Param::sib_r10d_2_ecx_disp8: case Param::sib_r10d_2_edx_disp8: case Param::sib_r10d_2_ebx_disp8: case Param::sib_r10d_2_esp_disp8: case Param::sib_r10d_2_ebp_disp8: case Param::sib_r10d_2_esi_disp8: case Param::sib_r10d_2_edi_disp8: case Param::sib_r10d_2_r8d_disp8: case Param::sib_r10d_2_r9d_disp8: case Param::sib_r10d_2_r10d_disp8: case Param::sib_r10d_2_r11d_disp8: case Param::sib_r10d_2_r12d_disp8: case Param::sib_r10d_2_r13d_disp8: case Param::sib_r10d_2_r14d_disp8: case Param::sib_r10d_2_r15d_disp8:
			case Param::sib_r11d_2_eax_disp8: case Param::sib_r11d_2_ecx_disp8: case Param::sib_r11d_2_edx_disp8: case Param::sib_r11d_2_ebx_disp8: case Param::sib_r11d_2_esp_disp8: case Param::sib_r11d_2_ebp_disp8: case Param::sib_r11d_2_esi_disp8: case Param::sib_r11d_2_edi_disp8: case Param::sib_r11d_2_r8d_disp8: case Param::sib_r11d_2_r9d_disp8: case Param::sib_r11d_2_r10d_disp8: case Param::sib_r11d_2_r11d_disp8: case Param::sib_r11d_2_r12d_disp8: case Param::sib_r11d_2_r13d_disp8: case Param::sib_r11d_2_r14d_disp8: case Param::sib_r11d_2_r15d_disp8:
			case Param::sib_r12d_2_eax_disp8: case Param::sib_r12d_2_ecx_disp8: case Param::sib_r12d_2_edx_disp8: case Param::sib_r12d_2_ebx_disp8: case Param::sib_r12d_2_esp_disp8: case Param::sib_r12d_2_ebp_disp8: case Param::sib_r12d_2_esi_disp8: case Param::sib_r12d_2_edi_disp8: case Param::sib_r12d_2_r8d_disp8: case Param::sib_r12d_2_r9d_disp8: case Param::sib_r12d_2_r10d_disp8: case Param::sib_r12d_2_r11d_disp8: case Param::sib_r12d_2_r12d_disp8: case Param::sib_r12d_2_r13d_disp8: case Param::sib_r12d_2_r14d_disp8: case Param::sib_r12d_2_r15d_disp8:
			case Param::sib_r13d_2_eax_disp8: case Param::sib_r13d_2_ecx_disp8: case Param::sib_r13d_2_edx_disp8: case Param::sib_r13d_2_ebx_disp8: case Param::sib_r13d_2_esp_disp8: case Param::sib_r13d_2_ebp_disp8: case Param::sib_r13d_2_esi_disp8: case Param::sib_r13d_2_edi_disp8: case Param::sib_r13d_2_r8d_disp8: case Param::sib_r13d_2_r9d_disp8: case Param::sib_r13d_2_r10d_disp8: case Param::sib_r13d_2_r11d_disp8: case Param::sib_r13d_2_r12d_disp8: case Param::sib_r13d_2_r13d_disp8: case Param::sib_r13d_2_r14d_disp8: case Param::sib_r13d_2_r15d_disp8:
			case Param::sib_r14d_2_eax_disp8: case Param::sib_r14d_2_ecx_disp8: case Param::sib_r14d_2_edx_disp8: case Param::sib_r14d_2_ebx_disp8: case Param::sib_r14d_2_esp_disp8: case Param::sib_r14d_2_ebp_disp8: case Param::sib_r14d_2_esi_disp8: case Param::sib_r14d_2_edi_disp8: case Param::sib_r14d_2_r8d_disp8: case Param::sib_r14d_2_r9d_disp8: case Param::sib_r14d_2_r10d_disp8: case Param::sib_r14d_2_r11d_disp8: case Param::sib_r14d_2_r12d_disp8: case Param::sib_r14d_2_r13d_disp8: case Param::sib_r14d_2_r14d_disp8: case Param::sib_r14d_2_r15d_disp8:
			case Param::sib_r15d_2_eax_disp8: case Param::sib_r15d_2_ecx_disp8: case Param::sib_r15d_2_edx_disp8: case Param::sib_r15d_2_ebx_disp8: case Param::sib_r15d_2_esp_disp8: case Param::sib_r15d_2_ebp_disp8: case Param::sib_r15d_2_esi_disp8: case Param::sib_r15d_2_edi_disp8: case Param::sib_r15d_2_r8d_disp8: case Param::sib_r15d_2_r9d_disp8: case Param::sib_r15d_2_r10d_disp8: case Param::sib_r15d_2_r11d_disp8: case Param::sib_r15d_2_r12d_disp8: case Param::sib_r15d_2_r13d_disp8: case Param::sib_r15d_2_r14d_disp8: case Param::sib_r15d_2_r15d_disp8:
			case Param::sib_eax_4_eax_disp8: case Param::sib_eax_4_ecx_disp8: case Param::sib_eax_4_edx_disp8: case Param::sib_eax_4_ebx_disp8: case Param::sib_eax_4_esp_disp8: case Param::sib_eax_4_ebp_disp8: case Param::sib_eax_4_esi_disp8: case Param::sib_eax_4_edi_disp8: case Param::sib_eax_4_r8d_disp8: case Param::sib_eax_4_r9d_disp8: case Param::sib_eax_4_r10d_disp8: case Param::sib_eax_4_r11d_disp8: case Param::sib_eax_4_r12d_disp8: case Param::sib_eax_4_r13d_disp8: case Param::sib_eax_4_r14d_disp8: case Param::sib_eax_4_r15d_disp8:
			case Param::sib_ecx_4_eax_disp8: case Param::sib_ecx_4_ecx_disp8: case Param::sib_ecx_4_edx_disp8: case Param::sib_ecx_4_ebx_disp8: case Param::sib_ecx_4_esp_disp8: case Param::sib_ecx_4_ebp_disp8: case Param::sib_ecx_4_esi_disp8: case Param::sib_ecx_4_edi_disp8: case Param::sib_ecx_4_r8d_disp8: case Param::sib_ecx_4_r9d_disp8: case Param::sib_ecx_4_r10d_disp8: case Param::sib_ecx_4_r11d_disp8: case Param::sib_ecx_4_r12d_disp8: case Param::sib_ecx_4_r13d_disp8: case Param::sib_ecx_4_r14d_disp8: case Param::sib_ecx_4_r15d_disp8:
			case Param::sib_edx_4_eax_disp8: case Param::sib_edx_4_ecx_disp8: case Param::sib_edx_4_edx_disp8: case Param::sib_edx_4_ebx_disp8: case Param::sib_edx_4_esp_disp8: case Param::sib_edx_4_ebp_disp8: case Param::sib_edx_4_esi_disp8: case Param::sib_edx_4_edi_disp8: case Param::sib_edx_4_r8d_disp8: case Param::sib_edx_4_r9d_disp8: case Param::sib_edx_4_r10d_disp8: case Param::sib_edx_4_r11d_disp8: case Param::sib_edx_4_r12d_disp8: case Param::sib_edx_4_r13d_disp8: case Param::sib_edx_4_r14d_disp8: case Param::sib_edx_4_r15d_disp8:
			case Param::sib_ebx_4_eax_disp8: case Param::sib_ebx_4_ecx_disp8: case Param::sib_ebx_4_edx_disp8: case Param::sib_ebx_4_ebx_disp8: case Param::sib_ebx_4_esp_disp8: case Param::sib_ebx_4_ebp_disp8: case Param::sib_ebx_4_esi_disp8: case Param::sib_ebx_4_edi_disp8: case Param::sib_ebx_4_r8d_disp8: case Param::sib_ebx_4_r9d_disp8: case Param::sib_ebx_4_r10d_disp8: case Param::sib_ebx_4_r11d_disp8: case Param::sib_ebx_4_r12d_disp8: case Param::sib_ebx_4_r13d_disp8: case Param::sib_ebx_4_r14d_disp8: case Param::sib_ebx_4_r15d_disp8:
			case Param::sib_ebp_4_eax_disp8: case Param::sib_ebp_4_ecx_disp8: case Param::sib_ebp_4_edx_disp8: case Param::sib_ebp_4_ebx_disp8: case Param::sib_ebp_4_esp_disp8: case Param::sib_ebp_4_ebp_disp8: case Param::sib_ebp_4_esi_disp8: case Param::sib_ebp_4_edi_disp8: case Param::sib_ebp_4_r8d_disp8: case Param::sib_ebp_4_r9d_disp8: case Param::sib_ebp_4_r10d_disp8: case Param::sib_ebp_4_r11d_disp8: case Param::sib_ebp_4_r12d_disp8: case Param::sib_ebp_4_r13d_disp8: case Param::sib_ebp_4_r14d_disp8: case Param::sib_ebp_4_r15d_disp8:
			case Param::sib_esi_4_eax_disp8: case Param::sib_esi_4_ecx_disp8: case Param::sib_esi_4_edx_disp8: case Param::sib_esi_4_ebx_disp8: case Param::sib_esi_4_esp_disp8: case Param::sib_esi_4_ebp_disp8: case Param::sib_esi_4_esi_disp8: case Param::sib_esi_4_edi_disp8: case Param::sib_esi_4_r8d_disp8: case Param::sib_esi_4_r9d_disp8: case Param::sib_esi_4_r10d_disp8: case Param::sib_esi_4_r11d_disp8: case Param::sib_esi_4_r12d_disp8: case Param::sib_esi_4_r13d_disp8: case Param::sib_esi_4_r14d_disp8: case Param::sib_esi_4_r15d_disp8:
			case Param::sib_edi_4_eax_disp8: case Param::sib_edi_4_ecx_disp8: case Param::sib_edi_4_edx_disp8: case Param::sib_edi_4_ebx_disp8: case Param::sib_edi_4_esp_disp8: case Param::sib_edi_4_ebp_disp8: case Param::sib_edi_4_esi_disp8: case Param::sib_edi_4_edi_disp8: case Param::sib_edi_4_r8d_disp8: case Param::sib_edi_4_r9d_disp8: case Param::sib_edi_4_r10d_disp8: case Param::sib_edi_4_r11d_disp8: case Param::sib_edi_4_r12d_disp8: case Param::sib_edi_4_r13d_disp8: case Param::sib_edi_4_r14d_disp8: case Param::sib_edi_4_r15d_disp8:
			case Param::sib_r8d_4_eax_disp8: case Param::sib_r8d_4_ecx_disp8: case Param::sib_r8d_4_edx_disp8: case Param::sib_r8d_4_ebx_disp8: case Param::sib_r8d_4_esp_disp8: case Param::sib_r8d_4_ebp_disp8: case Param::sib_r8d_4_esi_disp8: case Param::sib_r8d_4_edi_disp8: case Param::sib_r8d_4_r8d_disp8: case Param::sib_r8d_4_r9d_disp8: case Param::sib_r8d_4_r10d_disp8: case Param::sib_r8d_4_r11d_disp8: case Param::sib_r8d_4_r12d_disp8: case Param::sib_r8d_4_r13d_disp8: case Param::sib_r8d_4_r14d_disp8: case Param::sib_r8d_4_r15d_disp8:
			case Param::sib_r9d_4_eax_disp8: case Param::sib_r9d_4_ecx_disp8: case Param::sib_r9d_4_edx_disp8: case Param::sib_r9d_4_ebx_disp8: case Param::sib_r9d_4_esp_disp8: case Param::sib_r9d_4_ebp_disp8: case Param::sib_r9d_4_esi_disp8: case Param::sib_r9d_4_edi_disp8: case Param::sib_r9d_4_r8d_disp8: case Param::sib_r9d_4_r9d_disp8: case Param::sib_r9d_4_r10d_disp8: case Param::sib_r9d_4_r11d_disp8: case Param::sib_r9d_4_r12d_disp8: case Param::sib_r9d_4_r13d_disp8: case Param::sib_r9d_4_r14d_disp8: case Param::sib_r9d_4_r15d_disp8:
			case Param::sib_r10d_4_eax_disp8: case Param::sib_r10d_4_ecx_disp8: case Param::sib_r10d_4_edx_disp8: case Param::sib_r10d_4_ebx_disp8: case Param::sib_r10d_4_esp_disp8: case Param::sib_r10d_4_ebp_disp8: case Param::sib_r10d_4_esi_disp8: case Param::sib_r10d_4_edi_disp8: case Param::sib_r10d_4_r8d_disp8: case Param::sib_r10d_4_r9d_disp8: case Param::sib_r10d_4_r10d_disp8: case Param::sib_r10d_4_r11d_disp8: case Param::sib_r10d_4_r12d_disp8: case Param::sib_r10d_4_r13d_disp8: case Param::sib_r10d_4_r14d_disp8: case Param::sib_r10d_4_r15d_disp8:
			case Param::sib_r11d_4_eax_disp8: case Param::sib_r11d_4_ecx_disp8: case Param::sib_r11d_4_edx_disp8: case Param::sib_r11d_4_ebx_disp8: case Param::sib_r11d_4_esp_disp8: case Param::sib_r11d_4_ebp_disp8: case Param::sib_r11d_4_esi_disp8: case Param::sib_r11d_4_edi_disp8: case Param::sib_r11d_4_r8d_disp8: case Param::sib_r11d_4_r9d_disp8: case Param::sib_r11d_4_r10d_disp8: case Param::sib_r11d_4_r11d_disp8: case Param::sib_r11d_4_r12d_disp8: case Param::sib_r11d_4_r13d_disp8: case Param::sib_r11d_4_r14d_disp8: case Param::sib_r11d_4_r15d_disp8:
			case Param::sib_r12d_4_eax_disp8: case Param::sib_r12d_4_ecx_disp8: case Param::sib_r12d_4_edx_disp8: case Param::sib_r12d_4_ebx_disp8: case Param::sib_r12d_4_esp_disp8: case Param::sib_r12d_4_ebp_disp8: case Param::sib_r12d_4_esi_disp8: case Param::sib_r12d_4_edi_disp8: case Param::sib_r12d_4_r8d_disp8: case Param::sib_r12d_4_r9d_disp8: case Param::sib_r12d_4_r10d_disp8: case Param::sib_r12d_4_r11d_disp8: case Param::sib_r12d_4_r12d_disp8: case Param::sib_r12d_4_r13d_disp8: case Param::sib_r12d_4_r14d_disp8: case Param::sib_r12d_4_r15d_disp8:
			case Param::sib_r13d_4_eax_disp8: case Param::sib_r13d_4_ecx_disp8: case Param::sib_r13d_4_edx_disp8: case Param::sib_r13d_4_ebx_disp8: case Param::sib_r13d_4_esp_disp8: case Param::sib_r13d_4_ebp_disp8: case Param::sib_r13d_4_esi_disp8: case Param::sib_r13d_4_edi_disp8: case Param::sib_r13d_4_r8d_disp8: case Param::sib_r13d_4_r9d_disp8: case Param::sib_r13d_4_r10d_disp8: case Param::sib_r13d_4_r11d_disp8: case Param::sib_r13d_4_r12d_disp8: case Param::sib_r13d_4_r13d_disp8: case Param::sib_r13d_4_r14d_disp8: case Param::sib_r13d_4_r15d_disp8:
			case Param::sib_r14d_4_eax_disp8: case Param::sib_r14d_4_ecx_disp8: case Param::sib_r14d_4_edx_disp8: case Param::sib_r14d_4_ebx_disp8: case Param::sib_r14d_4_esp_disp8: case Param::sib_r14d_4_ebp_disp8: case Param::sib_r14d_4_esi_disp8: case Param::sib_r14d_4_edi_disp8: case Param::sib_r14d_4_r8d_disp8: case Param::sib_r14d_4_r9d_disp8: case Param::sib_r14d_4_r10d_disp8: case Param::sib_r14d_4_r11d_disp8: case Param::sib_r14d_4_r12d_disp8: case Param::sib_r14d_4_r13d_disp8: case Param::sib_r14d_4_r14d_disp8: case Param::sib_r14d_4_r15d_disp8:
			case Param::sib_r15d_4_eax_disp8: case Param::sib_r15d_4_ecx_disp8: case Param::sib_r15d_4_edx_disp8: case Param::sib_r15d_4_ebx_disp8: case Param::sib_r15d_4_esp_disp8: case Param::sib_r15d_4_ebp_disp8: case Param::sib_r15d_4_esi_disp8: case Param::sib_r15d_4_edi_disp8: case Param::sib_r15d_4_r8d_disp8: case Param::sib_r15d_4_r9d_disp8: case Param::sib_r15d_4_r10d_disp8: case Param::sib_r15d_4_r11d_disp8: case Param::sib_r15d_4_r12d_disp8: case Param::sib_r15d_4_r13d_disp8: case Param::sib_r15d_4_r14d_disp8: case Param::sib_r15d_4_r15d_disp8:
			case Param::sib_eax_8_eax_disp8: case Param::sib_eax_8_ecx_disp8: case Param::sib_eax_8_edx_disp8: case Param::sib_eax_8_ebx_disp8: case Param::sib_eax_8_esp_disp8: case Param::sib_eax_8_ebp_disp8: case Param::sib_eax_8_esi_disp8: case Param::sib_eax_8_edi_disp8: case Param::sib_eax_8_r8d_disp8: case Param::sib_eax_8_r9d_disp8: case Param::sib_eax_8_r10d_disp8: case Param::sib_eax_8_r11d_disp8: case Param::sib_eax_8_r12d_disp8: case Param::sib_eax_8_r13d_disp8: case Param::sib_eax_8_r14d_disp8: case Param::sib_eax_8_r15d_disp8:
			case Param::sib_ecx_8_eax_disp8: case Param::sib_ecx_8_ecx_disp8: case Param::sib_ecx_8_edx_disp8: case Param::sib_ecx_8_ebx_disp8: case Param::sib_ecx_8_esp_disp8: case Param::sib_ecx_8_ebp_disp8: case Param::sib_ecx_8_esi_disp8: case Param::sib_ecx_8_edi_disp8: case Param::sib_ecx_8_r8d_disp8: case Param::sib_ecx_8_r9d_disp8: case Param::sib_ecx_8_r10d_disp8: case Param::sib_ecx_8_r11d_disp8: case Param::sib_ecx_8_r12d_disp8: case Param::sib_ecx_8_r13d_disp8: case Param::sib_ecx_8_r14d_disp8: case Param::sib_ecx_8_r15d_disp8:
			case Param::sib_edx_8_eax_disp8: case Param::sib_edx_8_ecx_disp8: case Param::sib_edx_8_edx_disp8: case Param::sib_edx_8_ebx_disp8: case Param::sib_edx_8_esp_disp8: case Param::sib_edx_8_ebp_disp8: case Param::sib_edx_8_esi_disp8: case Param::sib_edx_8_edi_disp8: case Param::sib_edx_8_r8d_disp8: case Param::sib_edx_8_r9d_disp8: case Param::sib_edx_8_r10d_disp8: case Param::sib_edx_8_r11d_disp8: case Param::sib_edx_8_r12d_disp8: case Param::sib_edx_8_r13d_disp8: case Param::sib_edx_8_r14d_disp8: case Param::sib_edx_8_r15d_disp8:
			case Param::sib_ebx_8_eax_disp8: case Param::sib_ebx_8_ecx_disp8: case Param::sib_ebx_8_edx_disp8: case Param::sib_ebx_8_ebx_disp8: case Param::sib_ebx_8_esp_disp8: case Param::sib_ebx_8_ebp_disp8: case Param::sib_ebx_8_esi_disp8: case Param::sib_ebx_8_edi_disp8: case Param::sib_ebx_8_r8d_disp8: case Param::sib_ebx_8_r9d_disp8: case Param::sib_ebx_8_r10d_disp8: case Param::sib_ebx_8_r11d_disp8: case Param::sib_ebx_8_r12d_disp8: case Param::sib_ebx_8_r13d_disp8: case Param::sib_ebx_8_r14d_disp8: case Param::sib_ebx_8_r15d_disp8:
			case Param::sib_ebp_8_eax_disp8: case Param::sib_ebp_8_ecx_disp8: case Param::sib_ebp_8_edx_disp8: case Param::sib_ebp_8_ebx_disp8: case Param::sib_ebp_8_esp_disp8: case Param::sib_ebp_8_ebp_disp8: case Param::sib_ebp_8_esi_disp8: case Param::sib_ebp_8_edi_disp8: case Param::sib_ebp_8_r8d_disp8: case Param::sib_ebp_8_r9d_disp8: case Param::sib_ebp_8_r10d_disp8: case Param::sib_ebp_8_r11d_disp8: case Param::sib_ebp_8_r12d_disp8: case Param::sib_ebp_8_r13d_disp8: case Param::sib_ebp_8_r14d_disp8: case Param::sib_ebp_8_r15d_disp8:
			case Param::sib_esi_8_eax_disp8: case Param::sib_esi_8_ecx_disp8: case Param::sib_esi_8_edx_disp8: case Param::sib_esi_8_ebx_disp8: case Param::sib_esi_8_esp_disp8: case Param::sib_esi_8_ebp_disp8: case Param::sib_esi_8_esi_disp8: case Param::sib_esi_8_edi_disp8: case Param::sib_esi_8_r8d_disp8: case Param::sib_esi_8_r9d_disp8: case Param::sib_esi_8_r10d_disp8: case Param::sib_esi_8_r11d_disp8: case Param::sib_esi_8_r12d_disp8: case Param::sib_esi_8_r13d_disp8: case Param::sib_esi_8_r14d_disp8: case Param::sib_esi_8_r15d_disp8:
			case Param::sib_edi_8_eax_disp8: case Param::sib_edi_8_ecx_disp8: case Param::sib_edi_8_edx_disp8: case Param::sib_edi_8_ebx_disp8: case Param::sib_edi_8_esp_disp8: case Param::sib_edi_8_ebp_disp8: case Param::sib_edi_8_esi_disp8: case Param::sib_edi_8_edi_disp8: case Param::sib_edi_8_r8d_disp8: case Param::sib_edi_8_r9d_disp8: case Param::sib_edi_8_r10d_disp8: case Param::sib_edi_8_r11d_disp8: case Param::sib_edi_8_r12d_disp8: case Param::sib_edi_8_r13d_disp8: case Param::sib_edi_8_r14d_disp8: case Param::sib_edi_8_r15d_disp8:
			case Param::sib_r8d_8_eax_disp8: case Param::sib_r8d_8_ecx_disp8: case Param::sib_r8d_8_edx_disp8: case Param::sib_r8d_8_ebx_disp8: case Param::sib_r8d_8_esp_disp8: case Param::sib_r8d_8_ebp_disp8: case Param::sib_r8d_8_esi_disp8: case Param::sib_r8d_8_edi_disp8: case Param::sib_r8d_8_r8d_disp8: case Param::sib_r8d_8_r9d_disp8: case Param::sib_r8d_8_r10d_disp8: case Param::sib_r8d_8_r11d_disp8: case Param::sib_r8d_8_r12d_disp8: case Param::sib_r8d_8_r13d_disp8: case Param::sib_r8d_8_r14d_disp8: case Param::sib_r8d_8_r15d_disp8:
			case Param::sib_r9d_8_eax_disp8: case Param::sib_r9d_8_ecx_disp8: case Param::sib_r9d_8_edx_disp8: case Param::sib_r9d_8_ebx_disp8: case Param::sib_r9d_8_esp_disp8: case Param::sib_r9d_8_ebp_disp8: case Param::sib_r9d_8_esi_disp8: case Param::sib_r9d_8_edi_disp8: case Param::sib_r9d_8_r8d_disp8: case Param::sib_r9d_8_r9d_disp8: case Param::sib_r9d_8_r10d_disp8: case Param::sib_r9d_8_r11d_disp8: case Param::sib_r9d_8_r12d_disp8: case Param::sib_r9d_8_r13d_disp8: case Param::sib_r9d_8_r14d_disp8: case Param::sib_r9d_8_r15d_disp8:
			case Param::sib_r10d_8_eax_disp8: case Param::sib_r10d_8_ecx_disp8: case Param::sib_r10d_8_edx_disp8: case Param::sib_r10d_8_ebx_disp8: case Param::sib_r10d_8_esp_disp8: case Param::sib_r10d_8_ebp_disp8: case Param::sib_r10d_8_esi_disp8: case Param::sib_r10d_8_edi_disp8: case Param::sib_r10d_8_r8d_disp8: case Param::sib_r10d_8_r9d_disp8: case Param::sib_r10d_8_r10d_disp8: case Param::sib_r10d_8_r11d_disp8: case Param::sib_r10d_8_r12d_disp8: case Param::sib_r10d_8_r13d_disp8: case Param::sib_r10d_8_r14d_disp8: case Param::sib_r10d_8_r15d_disp8:
			case Param::sib_r11d_8_eax_disp8: case Param::sib_r11d_8_ecx_disp8: case Param::sib_r11d_8_edx_disp8: case Param::sib_r11d_8_ebx_disp8: case Param::sib_r11d_8_esp_disp8: case Param::sib_r11d_8_ebp_disp8: case Param::sib_r11d_8_esi_disp8: case Param::sib_r11d_8_edi_disp8: case Param::sib_r11d_8_r8d_disp8: case Param::sib_r11d_8_r9d_disp8: case Param::sib_r11d_8_r10d_disp8: case Param::sib_r11d_8_r11d_disp8: case Param::sib_r11d_8_r12d_disp8: case Param::sib_r11d_8_r13d_disp8: case Param::sib_r11d_8_r14d_disp8: case Param::sib_r11d_8_r15d_disp8:
			case Param::sib_r12d_8_eax_disp8: case Param::sib_r12d_8_ecx_disp8: case Param::sib_r12d_8_edx_disp8: case Param::sib_r12d_8_ebx_disp8: case Param::sib_r12d_8_esp_disp8: case Param::sib_r12d_8_ebp_disp8: case Param::sib_r12d_8_esi_disp8: case Param::sib_r12d_8_edi_disp8: case Param::sib_r12d_8_r8d_disp8: case Param::sib_r12d_8_r9d_disp8: case Param::sib_r12d_8_r10d_disp8: case Param::sib_r12d_8_r11d_disp8: case Param::sib_r12d_8_r12d_disp8: case Param::sib_r12d_8_r13d_disp8: case Param::sib_r12d_8_r14d_disp8: case Param::sib_r12d_8_r15d_disp8:
			case Param::sib_r13d_8_eax_disp8: case Param::sib_r13d_8_ecx_disp8: case Param::sib_r13d_8_edx_disp8: case Param::sib_r13d_8_ebx_disp8: case Param::sib_r13d_8_esp_disp8: case Param::sib_r13d_8_ebp_disp8: case Param::sib_r13d_8_esi_disp8: case Param::sib_r13d_8_edi_disp8: case Param::sib_r13d_8_r8d_disp8: case Param::sib_r13d_8_r9d_disp8: case Param::sib_r13d_8_r10d_disp8: case Param::sib_r13d_8_r11d_disp8: case Param::sib_r13d_8_r12d_disp8: case Param::sib_r13d_8_r13d_disp8: case Param::sib_r13d_8_r14d_disp8: case Param::sib_r13d_8_r15d_disp8:
			case Param::sib_r14d_8_eax_disp8: case Param::sib_r14d_8_ecx_disp8: case Param::sib_r14d_8_edx_disp8: case Param::sib_r14d_8_ebx_disp8: case Param::sib_r14d_8_esp_disp8: case Param::sib_r14d_8_ebp_disp8: case Param::sib_r14d_8_esi_disp8: case Param::sib_r14d_8_edi_disp8: case Param::sib_r14d_8_r8d_disp8: case Param::sib_r14d_8_r9d_disp8: case Param::sib_r14d_8_r10d_disp8: case Param::sib_r14d_8_r11d_disp8: case Param::sib_r14d_8_r12d_disp8: case Param::sib_r14d_8_r13d_disp8: case Param::sib_r14d_8_r14d_disp8: case Param::sib_r14d_8_r15d_disp8:
			case Param::sib_r15d_8_eax_disp8: case Param::sib_r15d_8_ecx_disp8: case Param::sib_r15d_8_edx_disp8: case Param::sib_r15d_8_ebx_disp8: case Param::sib_r15d_8_esp_disp8: case Param::sib_r15d_8_ebp_disp8: case Param::sib_r15d_8_esi_disp8: case Param::sib_r15d_8_edi_disp8: case Param::sib_r15d_8_r8d_disp8: case Param::sib_r15d_8_r9d_disp8: case Param::sib_r15d_8_r10d_disp8: case Param::sib_r15d_8_r11d_disp8: case Param::sib_r15d_8_r12d_disp8: case Param::sib_r15d_8_r13d_disp8: case Param::sib_r15d_8_r14d_disp8: case Param::sib_r15d_8_r15d_disp8:

			case Param::sib_rax_rax_disp8: case Param::sib_rax_rcx_disp8: case Param::sib_rax_rdx_disp8: case Param::sib_rax_rbx_disp8: case Param::sib_rax_rsp_disp8: case Param::sib_rax_rbp_disp8: case Param::sib_rax_rsi_disp8: case Param::sib_rax_rdi_disp8: case Param::sib_rax_r8_disp8: case Param::sib_rax_r9_disp8: case Param::sib_rax_r10_disp8: case Param::sib_rax_r11_disp8: case Param::sib_rax_r12_disp8: case Param::sib_rax_r13_disp8: case Param::sib_rax_r14_disp8: case Param::sib_rax_r15_disp8:
			case Param::sib_rcx_rax_disp8: case Param::sib_rcx_rcx_disp8: case Param::sib_rcx_rdx_disp8: case Param::sib_rcx_rbx_disp8: case Param::sib_rcx_rsp_disp8: case Param::sib_rcx_rbp_disp8: case Param::sib_rcx_rsi_disp8: case Param::sib_rcx_rdi_disp8: case Param::sib_rcx_r8_disp8: case Param::sib_rcx_r9_disp8: case Param::sib_rcx_r10_disp8: case Param::sib_rcx_r11_disp8: case Param::sib_rcx_r12_disp8: case Param::sib_rcx_r13_disp8: case Param::sib_rcx_r14_disp8: case Param::sib_rcx_r15_disp8:
			case Param::sib_rdx_rax_disp8: case Param::sib_rdx_rcx_disp8: case Param::sib_rdx_rdx_disp8: case Param::sib_rdx_rbx_disp8: case Param::sib_rdx_rsp_disp8: case Param::sib_rdx_rbp_disp8: case Param::sib_rdx_rsi_disp8: case Param::sib_rdx_rdi_disp8: case Param::sib_rdx_r8_disp8: case Param::sib_rdx_r9_disp8: case Param::sib_rdx_r10_disp8: case Param::sib_rdx_r11_disp8: case Param::sib_rdx_r12_disp8: case Param::sib_rdx_r13_disp8: case Param::sib_rdx_r14_disp8: case Param::sib_rdx_r15_disp8:
			case Param::sib_rbx_rax_disp8: case Param::sib_rbx_rcx_disp8: case Param::sib_rbx_rdx_disp8: case Param::sib_rbx_rbx_disp8: case Param::sib_rbx_rsp_disp8: case Param::sib_rbx_rbp_disp8: case Param::sib_rbx_rsi_disp8: case Param::sib_rbx_rdi_disp8: case Param::sib_rbx_r8_disp8: case Param::sib_rbx_r9_disp8: case Param::sib_rbx_r10_disp8: case Param::sib_rbx_r11_disp8: case Param::sib_rbx_r12_disp8: case Param::sib_rbx_r13_disp8: case Param::sib_rbx_r14_disp8: case Param::sib_rbx_r15_disp8:
			case Param::sib_rbp_rax_disp8: case Param::sib_rbp_rcx_disp8: case Param::sib_rbp_rdx_disp8: case Param::sib_rbp_rbx_disp8: case Param::sib_rbp_rsp_disp8: case Param::sib_rbp_rbp_disp8: case Param::sib_rbp_rsi_disp8: case Param::sib_rbp_rdi_disp8: case Param::sib_rbp_r8_disp8: case Param::sib_rbp_r9_disp8: case Param::sib_rbp_r10_disp8: case Param::sib_rbp_r11_disp8: case Param::sib_rbp_r12_disp8: case Param::sib_rbp_r13_disp8: case Param::sib_rbp_r14_disp8: case Param::sib_rbp_r15_disp8:
			case Param::sib_rsi_rax_disp8: case Param::sib_rsi_rcx_disp8: case Param::sib_rsi_rdx_disp8: case Param::sib_rsi_rbx_disp8: case Param::sib_rsi_rsp_disp8: case Param::sib_rsi_rbp_disp8: case Param::sib_rsi_rsi_disp8: case Param::sib_rsi_rdi_disp8: case Param::sib_rsi_r8_disp8: case Param::sib_rsi_r9_disp8: case Param::sib_rsi_r10_disp8: case Param::sib_rsi_r11_disp8: case Param::sib_rsi_r12_disp8: case Param::sib_rsi_r13_disp8: case Param::sib_rsi_r14_disp8: case Param::sib_rsi_r15_disp8:
			case Param::sib_rdi_rax_disp8: case Param::sib_rdi_rcx_disp8: case Param::sib_rdi_rdx_disp8: case Param::sib_rdi_rbx_disp8: case Param::sib_rdi_rsp_disp8: case Param::sib_rdi_rbp_disp8: case Param::sib_rdi_rsi_disp8: case Param::sib_rdi_rdi_disp8: case Param::sib_rdi_r8_disp8: case Param::sib_rdi_r9_disp8: case Param::sib_rdi_r10_disp8: case Param::sib_rdi_r11_disp8: case Param::sib_rdi_r12_disp8: case Param::sib_rdi_r13_disp8: case Param::sib_rdi_r14_disp8: case Param::sib_rdi_r15_disp8:
			case Param::sib_r8_rax_disp8: case Param::sib_r8_rcx_disp8: case Param::sib_r8_rdx_disp8: case Param::sib_r8_rbx_disp8: case Param::sib_r8_rsp_disp8: case Param::sib_r8_rbp_disp8: case Param::sib_r8_rsi_disp8: case Param::sib_r8_rdi_disp8: case Param::sib_r8_r8_disp8: case Param::sib_r8_r9_disp8: case Param::sib_r8_r10_disp8: case Param::sib_r8_r11_disp8: case Param::sib_r8_r12_disp8: case Param::sib_r8_r13_disp8: case Param::sib_r8_r14_disp8: case Param::sib_r8_r15_disp8:
			case Param::sib_r9_rax_disp8: case Param::sib_r9_rcx_disp8: case Param::sib_r9_rdx_disp8: case Param::sib_r9_rbx_disp8: case Param::sib_r9_rsp_disp8: case Param::sib_r9_rbp_disp8: case Param::sib_r9_rsi_disp8: case Param::sib_r9_rdi_disp8: case Param::sib_r9_r8_disp8: case Param::sib_r9_r9_disp8: case Param::sib_r9_r10_disp8: case Param::sib_r9_r11_disp8: case Param::sib_r9_r12_disp8: case Param::sib_r9_r13_disp8: case Param::sib_r9_r14_disp8: case Param::sib_r9_r15_disp8:
			case Param::sib_r10_rax_disp8: case Param::sib_r10_rcx_disp8: case Param::sib_r10_rdx_disp8: case Param::sib_r10_rbx_disp8: case Param::sib_r10_rsp_disp8: case Param::sib_r10_rbp_disp8: case Param::sib_r10_rsi_disp8: case Param::sib_r10_rdi_disp8: case Param::sib_r10_r8_disp8: case Param::sib_r10_r9_disp8: case Param::sib_r10_r10_disp8: case Param::sib_r10_r11_disp8: case Param::sib_r10_r12_disp8: case Param::sib_r10_r13_disp8: case Param::sib_r10_r14_disp8: case Param::sib_r10_r15_disp8:
			case Param::sib_r11_rax_disp8: case Param::sib_r11_rcx_disp8: case Param::sib_r11_rdx_disp8: case Param::sib_r11_rbx_disp8: case Param::sib_r11_rsp_disp8: case Param::sib_r11_rbp_disp8: case Param::sib_r11_rsi_disp8: case Param::sib_r11_rdi_disp8: case Param::sib_r11_r8_disp8: case Param::sib_r11_r9_disp8: case Param::sib_r11_r10_disp8: case Param::sib_r11_r11_disp8: case Param::sib_r11_r12_disp8: case Param::sib_r11_r13_disp8: case Param::sib_r11_r14_disp8: case Param::sib_r11_r15_disp8:
			case Param::sib_r12_rax_disp8: case Param::sib_r12_rcx_disp8: case Param::sib_r12_rdx_disp8: case Param::sib_r12_rbx_disp8: case Param::sib_r12_rsp_disp8: case Param::sib_r12_rbp_disp8: case Param::sib_r12_rsi_disp8: case Param::sib_r12_rdi_disp8: case Param::sib_r12_r8_disp8: case Param::sib_r12_r9_disp8: case Param::sib_r12_r10_disp8: case Param::sib_r12_r11_disp8: case Param::sib_r12_r12_disp8: case Param::sib_r12_r13_disp8: case Param::sib_r12_r14_disp8: case Param::sib_r12_r15_disp8:
			case Param::sib_r13_rax_disp8: case Param::sib_r13_rcx_disp8: case Param::sib_r13_rdx_disp8: case Param::sib_r13_rbx_disp8: case Param::sib_r13_rsp_disp8: case Param::sib_r13_rbp_disp8: case Param::sib_r13_rsi_disp8: case Param::sib_r13_rdi_disp8: case Param::sib_r13_r8_disp8: case Param::sib_r13_r9_disp8: case Param::sib_r13_r10_disp8: case Param::sib_r13_r11_disp8: case Param::sib_r13_r12_disp8: case Param::sib_r13_r13_disp8: case Param::sib_r13_r14_disp8: case Param::sib_r13_r15_disp8:
			case Param::sib_r14_rax_disp8: case Param::sib_r14_rcx_disp8: case Param::sib_r14_rdx_disp8: case Param::sib_r14_rbx_disp8: case Param::sib_r14_rsp_disp8: case Param::sib_r14_rbp_disp8: case Param::sib_r14_rsi_disp8: case Param::sib_r14_rdi_disp8: case Param::sib_r14_r8_disp8: case Param::sib_r14_r9_disp8: case Param::sib_r14_r10_disp8: case Param::sib_r14_r11_disp8: case Param::sib_r14_r12_disp8: case Param::sib_r14_r13_disp8: case Param::sib_r14_r14_disp8: case Param::sib_r14_r15_disp8:
			case Param::sib_r15_rax_disp8: case Param::sib_r15_rcx_disp8: case Param::sib_r15_rdx_disp8: case Param::sib_r15_rbx_disp8: case Param::sib_r15_rsp_disp8: case Param::sib_r15_rbp_disp8: case Param::sib_r15_rsi_disp8: case Param::sib_r15_rdi_disp8: case Param::sib_r15_r8_disp8: case Param::sib_r15_r9_disp8: case Param::sib_r15_r10_disp8: case Param::sib_r15_r11_disp8: case Param::sib_r15_r12_disp8: case Param::sib_r15_r13_disp8: case Param::sib_r15_r14_disp8: case Param::sib_r15_r15_disp8:
			case Param::sib_rax_2_rax_disp8: case Param::sib_rax_2_rcx_disp8: case Param::sib_rax_2_rdx_disp8: case Param::sib_rax_2_rbx_disp8: case Param::sib_rax_2_rsp_disp8: case Param::sib_rax_2_rbp_disp8: case Param::sib_rax_2_rsi_disp8: case Param::sib_rax_2_rdi_disp8: case Param::sib_rax_2_r8_disp8: case Param::sib_rax_2_r9_disp8: case Param::sib_rax_2_r10_disp8: case Param::sib_rax_2_r11_disp8: case Param::sib_rax_2_r12_disp8: case Param::sib_rax_2_r13_disp8: case Param::sib_rax_2_r14_disp8: case Param::sib_rax_2_r15_disp8:
			case Param::sib_rcx_2_rax_disp8: case Param::sib_rcx_2_rcx_disp8: case Param::sib_rcx_2_rdx_disp8: case Param::sib_rcx_2_rbx_disp8: case Param::sib_rcx_2_rsp_disp8: case Param::sib_rcx_2_rbp_disp8: case Param::sib_rcx_2_rsi_disp8: case Param::sib_rcx_2_rdi_disp8: case Param::sib_rcx_2_r8_disp8: case Param::sib_rcx_2_r9_disp8: case Param::sib_rcx_2_r10_disp8: case Param::sib_rcx_2_r11_disp8: case Param::sib_rcx_2_r12_disp8: case Param::sib_rcx_2_r13_disp8: case Param::sib_rcx_2_r14_disp8: case Param::sib_rcx_2_r15_disp8:
			case Param::sib_rdx_2_rax_disp8: case Param::sib_rdx_2_rcx_disp8: case Param::sib_rdx_2_rdx_disp8: case Param::sib_rdx_2_rbx_disp8: case Param::sib_rdx_2_rsp_disp8: case Param::sib_rdx_2_rbp_disp8: case Param::sib_rdx_2_rsi_disp8: case Param::sib_rdx_2_rdi_disp8: case Param::sib_rdx_2_r8_disp8: case Param::sib_rdx_2_r9_disp8: case Param::sib_rdx_2_r10_disp8: case Param::sib_rdx_2_r11_disp8: case Param::sib_rdx_2_r12_disp8: case Param::sib_rdx_2_r13_disp8: case Param::sib_rdx_2_r14_disp8: case Param::sib_rdx_2_r15_disp8:
			case Param::sib_rbx_2_rax_disp8: case Param::sib_rbx_2_rcx_disp8: case Param::sib_rbx_2_rdx_disp8: case Param::sib_rbx_2_rbx_disp8: case Param::sib_rbx_2_rsp_disp8: case Param::sib_rbx_2_rbp_disp8: case Param::sib_rbx_2_rsi_disp8: case Param::sib_rbx_2_rdi_disp8: case Param::sib_rbx_2_r8_disp8: case Param::sib_rbx_2_r9_disp8: case Param::sib_rbx_2_r10_disp8: case Param::sib_rbx_2_r11_disp8: case Param::sib_rbx_2_r12_disp8: case Param::sib_rbx_2_r13_disp8: case Param::sib_rbx_2_r14_disp8: case Param::sib_rbx_2_r15_disp8:
			case Param::sib_rbp_2_rax_disp8: case Param::sib_rbp_2_rcx_disp8: case Param::sib_rbp_2_rdx_disp8: case Param::sib_rbp_2_rbx_disp8: case Param::sib_rbp_2_rsp_disp8: case Param::sib_rbp_2_rbp_disp8: case Param::sib_rbp_2_rsi_disp8: case Param::sib_rbp_2_rdi_disp8: case Param::sib_rbp_2_r8_disp8: case Param::sib_rbp_2_r9_disp8: case Param::sib_rbp_2_r10_disp8: case Param::sib_rbp_2_r11_disp8: case Param::sib_rbp_2_r12_disp8: case Param::sib_rbp_2_r13_disp8: case Param::sib_rbp_2_r14_disp8: case Param::sib_rbp_2_r15_disp8:
			case Param::sib_rsi_2_rax_disp8: case Param::sib_rsi_2_rcx_disp8: case Param::sib_rsi_2_rdx_disp8: case Param::sib_rsi_2_rbx_disp8: case Param::sib_rsi_2_rsp_disp8: case Param::sib_rsi_2_rbp_disp8: case Param::sib_rsi_2_rsi_disp8: case Param::sib_rsi_2_rdi_disp8: case Param::sib_rsi_2_r8_disp8: case Param::sib_rsi_2_r9_disp8: case Param::sib_rsi_2_r10_disp8: case Param::sib_rsi_2_r11_disp8: case Param::sib_rsi_2_r12_disp8: case Param::sib_rsi_2_r13_disp8: case Param::sib_rsi_2_r14_disp8: case Param::sib_rsi_2_r15_disp8:
			case Param::sib_rdi_2_rax_disp8: case Param::sib_rdi_2_rcx_disp8: case Param::sib_rdi_2_rdx_disp8: case Param::sib_rdi_2_rbx_disp8: case Param::sib_rdi_2_rsp_disp8: case Param::sib_rdi_2_rbp_disp8: case Param::sib_rdi_2_rsi_disp8: case Param::sib_rdi_2_rdi_disp8: case Param::sib_rdi_2_r8_disp8: case Param::sib_rdi_2_r9_disp8: case Param::sib_rdi_2_r10_disp8: case Param::sib_rdi_2_r11_disp8: case Param::sib_rdi_2_r12_disp8: case Param::sib_rdi_2_r13_disp8: case Param::sib_rdi_2_r14_disp8: case Param::sib_rdi_2_r15_disp8:
			case Param::sib_r8_2_rax_disp8: case Param::sib_r8_2_rcx_disp8: case Param::sib_r8_2_rdx_disp8: case Param::sib_r8_2_rbx_disp8: case Param::sib_r8_2_rsp_disp8: case Param::sib_r8_2_rbp_disp8: case Param::sib_r8_2_rsi_disp8:  case Param::sib_r8_2_rdi_disp8:  case Param::sib_r8_2_r8_disp8:  case Param::sib_r8_2_r9_disp8:  case Param::sib_r8_2_r10_disp8:  case Param::sib_r8_2_r11_disp8:  case Param::sib_r8_2_r12_disp8:  case Param::sib_r8_2_r13_disp8:  case Param::sib_r8_2_r14_disp8:  case Param::sib_r8_2_r15_disp8:
			case Param::sib_r9_2_rax_disp8: case Param::sib_r9_2_rcx_disp8: case Param::sib_r9_2_rdx_disp8: case Param::sib_r9_2_rbx_disp8: case Param::sib_r9_2_rsp_disp8: case Param::sib_r9_2_rbp_disp8: case Param::sib_r9_2_rsi_disp8:  case Param::sib_r9_2_rdi_disp8:  case Param::sib_r9_2_r8_disp8:  case Param::sib_r9_2_r9_disp8:  case Param::sib_r9_2_r10_disp8:  case Param::sib_r9_2_r11_disp8:  case Param::sib_r9_2_r12_disp8:  case Param::sib_r9_2_r13_disp8:  case Param::sib_r9_2_r14_disp8:  case Param::sib_r9_2_r15_disp8:
			case Param::sib_r10_2_rax_disp8: case Param::sib_r10_2_rcx_disp8: case Param::sib_r10_2_rdx_disp8: case Param::sib_r10_2_rbx_disp8: case Param::sib_r10_2_rsp_disp8: case Param::sib_r10_2_rbp_disp8: case Param::sib_r10_2_rsi_disp8: case Param::sib_r10_2_rdi_disp8: case Param::sib_r10_2_r8_disp8: case Param::sib_r10_2_r9_disp8: case Param::sib_r10_2_r10_disp8: case Param::sib_r10_2_r11_disp8: case Param::sib_r10_2_r12_disp8: case Param::sib_r10_2_r13_disp8: case Param::sib_r10_2_r14_disp8: case Param::sib_r10_2_r15_disp8:
			case Param::sib_r11_2_rax_disp8: case Param::sib_r11_2_rcx_disp8: case Param::sib_r11_2_rdx_disp8: case Param::sib_r11_2_rbx_disp8: case Param::sib_r11_2_rsp_disp8: case Param::sib_r11_2_rbp_disp8: case Param::sib_r11_2_rsi_disp8: case Param::sib_r11_2_rdi_disp8: case Param::sib_r11_2_r8_disp8: case Param::sib_r11_2_r9_disp8: case Param::sib_r11_2_r10_disp8: case Param::sib_r11_2_r11_disp8: case Param::sib_r11_2_r12_disp8: case Param::sib_r11_2_r13_disp8: case Param::sib_r11_2_r14_disp8: case Param::sib_r11_2_r15_disp8:
			case Param::sib_r12_2_rax_disp8: case Param::sib_r12_2_rcx_disp8: case Param::sib_r12_2_rdx_disp8: case Param::sib_r12_2_rbx_disp8: case Param::sib_r12_2_rsp_disp8: case Param::sib_r12_2_rbp_disp8: case Param::sib_r12_2_rsi_disp8: case Param::sib_r12_2_rdi_disp8: case Param::sib_r12_2_r8_disp8: case Param::sib_r12_2_r9_disp8: case Param::sib_r12_2_r10_disp8: case Param::sib_r12_2_r11_disp8: case Param::sib_r12_2_r12_disp8: case Param::sib_r12_2_r13_disp8: case Param::sib_r12_2_r14_disp8: case Param::sib_r12_2_r15_disp8:
			case Param::sib_r13_2_rax_disp8: case Param::sib_r13_2_rcx_disp8: case Param::sib_r13_2_rdx_disp8: case Param::sib_r13_2_rbx_disp8: case Param::sib_r13_2_rsp_disp8: case Param::sib_r13_2_rbp_disp8: case Param::sib_r13_2_rsi_disp8: case Param::sib_r13_2_rdi_disp8: case Param::sib_r13_2_r8_disp8: case Param::sib_r13_2_r9_disp8: case Param::sib_r13_2_r10_disp8: case Param::sib_r13_2_r11_disp8: case Param::sib_r13_2_r12_disp8: case Param::sib_r13_2_r13_disp8: case Param::sib_r13_2_r14_disp8: case Param::sib_r13_2_r15_disp8:
			case Param::sib_r14_2_rax_disp8: case Param::sib_r14_2_rcx_disp8: case Param::sib_r14_2_rdx_disp8: case Param::sib_r14_2_rbx_disp8: case Param::sib_r14_2_rsp_disp8: case Param::sib_r14_2_rbp_disp8: case Param::sib_r14_2_rsi_disp8: case Param::sib_r14_2_rdi_disp8: case Param::sib_r14_2_r8_disp8: case Param::sib_r14_2_r9_disp8: case Param::sib_r14_2_r10_disp8: case Param::sib_r14_2_r11_disp8: case Param::sib_r14_2_r12_disp8: case Param::sib_r14_2_r13_disp8: case Param::sib_r14_2_r14_disp8: case Param::sib_r14_2_r15_disp8:
			case Param::sib_r15_2_rax_disp8: case Param::sib_r15_2_rcx_disp8: case Param::sib_r15_2_rdx_disp8: case Param::sib_r15_2_rbx_disp8: case Param::sib_r15_2_rsp_disp8: case Param::sib_r15_2_rbp_disp8: case Param::sib_r15_2_rsi_disp8: case Param::sib_r15_2_rdi_disp8: case Param::sib_r15_2_r8_disp8: case Param::sib_r15_2_r9_disp8: case Param::sib_r15_2_r10_disp8: case Param::sib_r15_2_r11_disp8: case Param::sib_r15_2_r12_disp8: case Param::sib_r15_2_r13_disp8: case Param::sib_r15_2_r14_disp8: case Param::sib_r15_2_r15_disp8:
			case Param::sib_rax_4_rax_disp8: case Param::sib_rax_4_rcx_disp8: case Param::sib_rax_4_rdx_disp8: case Param::sib_rax_4_rbx_disp8: case Param::sib_rax_4_rsp_disp8: case Param::sib_rax_4_rbp_disp8: case Param::sib_rax_4_rsi_disp8: case Param::sib_rax_4_rdi_disp8: case Param::sib_rax_4_r8_disp8: case Param::sib_rax_4_r9_disp8: case Param::sib_rax_4_r10_disp8: case Param::sib_rax_4_r11_disp8: case Param::sib_rax_4_r12_disp8: case Param::sib_rax_4_r13_disp8: case Param::sib_rax_4_r14_disp8: case Param::sib_rax_4_r15_disp8:
			case Param::sib_rcx_4_rax_disp8: case Param::sib_rcx_4_rcx_disp8: case Param::sib_rcx_4_rdx_disp8: case Param::sib_rcx_4_rbx_disp8: case Param::sib_rcx_4_rsp_disp8: case Param::sib_rcx_4_rbp_disp8: case Param::sib_rcx_4_rsi_disp8: case Param::sib_rcx_4_rdi_disp8: case Param::sib_rcx_4_r8_disp8: case Param::sib_rcx_4_r9_disp8: case Param::sib_rcx_4_r10_disp8: case Param::sib_rcx_4_r11_disp8: case Param::sib_rcx_4_r12_disp8: case Param::sib_rcx_4_r13_disp8: case Param::sib_rcx_4_r14_disp8: case Param::sib_rcx_4_r15_disp8:
			case Param::sib_rdx_4_rax_disp8: case Param::sib_rdx_4_rcx_disp8: case Param::sib_rdx_4_rdx_disp8: case Param::sib_rdx_4_rbx_disp8: case Param::sib_rdx_4_rsp_disp8: case Param::sib_rdx_4_rbp_disp8: case Param::sib_rdx_4_rsi_disp8: case Param::sib_rdx_4_rdi_disp8: case Param::sib_rdx_4_r8_disp8: case Param::sib_rdx_4_r9_disp8: case Param::sib_rdx_4_r10_disp8: case Param::sib_rdx_4_r11_disp8: case Param::sib_rdx_4_r12_disp8: case Param::sib_rdx_4_r13_disp8: case Param::sib_rdx_4_r14_disp8: case Param::sib_rdx_4_r15_disp8:
			case Param::sib_rbx_4_rax_disp8: case Param::sib_rbx_4_rcx_disp8: case Param::sib_rbx_4_rdx_disp8: case Param::sib_rbx_4_rbx_disp8: case Param::sib_rbx_4_rsp_disp8: case Param::sib_rbx_4_rbp_disp8: case Param::sib_rbx_4_rsi_disp8: case Param::sib_rbx_4_rdi_disp8: case Param::sib_rbx_4_r8_disp8: case Param::sib_rbx_4_r9_disp8: case Param::sib_rbx_4_r10_disp8: case Param::sib_rbx_4_r11_disp8: case Param::sib_rbx_4_r12_disp8: case Param::sib_rbx_4_r13_disp8: case Param::sib_rbx_4_r14_disp8: case Param::sib_rbx_4_r15_disp8:
			case Param::sib_rbp_4_rax_disp8: case Param::sib_rbp_4_rcx_disp8: case Param::sib_rbp_4_rdx_disp8: case Param::sib_rbp_4_rbx_disp8: case Param::sib_rbp_4_rsp_disp8: case Param::sib_rbp_4_rbp_disp8: case Param::sib_rbp_4_rsi_disp8: case Param::sib_rbp_4_rdi_disp8: case Param::sib_rbp_4_r8_disp8: case Param::sib_rbp_4_r9_disp8: case Param::sib_rbp_4_r10_disp8: case Param::sib_rbp_4_r11_disp8: case Param::sib_rbp_4_r12_disp8: case Param::sib_rbp_4_r13_disp8: case Param::sib_rbp_4_r14_disp8: case Param::sib_rbp_4_r15_disp8:
			case Param::sib_rsi_4_rax_disp8: case Param::sib_rsi_4_rcx_disp8: case Param::sib_rsi_4_rdx_disp8: case Param::sib_rsi_4_rbx_disp8: case Param::sib_rsi_4_rsp_disp8: case Param::sib_rsi_4_rbp_disp8: case Param::sib_rsi_4_rsi_disp8: case Param::sib_rsi_4_rdi_disp8: case Param::sib_rsi_4_r8_disp8: case Param::sib_rsi_4_r9_disp8: case Param::sib_rsi_4_r10_disp8: case Param::sib_rsi_4_r11_disp8: case Param::sib_rsi_4_r12_disp8: case Param::sib_rsi_4_r13_disp8: case Param::sib_rsi_4_r14_disp8: case Param::sib_rsi_4_r15_disp8:
			case Param::sib_rdi_4_rax_disp8: case Param::sib_rdi_4_rcx_disp8: case Param::sib_rdi_4_rdx_disp8: case Param::sib_rdi_4_rbx_disp8: case Param::sib_rdi_4_rsp_disp8: case Param::sib_rdi_4_rbp_disp8: case Param::sib_rdi_4_rsi_disp8: case Param::sib_rdi_4_rdi_disp8: case Param::sib_rdi_4_r8_disp8: case Param::sib_rdi_4_r9_disp8: case Param::sib_rdi_4_r10_disp8: case Param::sib_rdi_4_r11_disp8: case Param::sib_rdi_4_r12_disp8: case Param::sib_rdi_4_r13_disp8: case Param::sib_rdi_4_r14_disp8: case Param::sib_rdi_4_r15_disp8:
			case Param::sib_r8_4_rax_disp8: case Param::sib_r8_4_rcx_disp8: case Param::sib_r8_4_rdx_disp8: case Param::sib_r8_4_rbx_disp8: case Param::sib_r8_4_rsp_disp8: case Param::sib_r8_4_rbp_disp8: case Param::sib_r8_4_rsi_disp8:  case Param::sib_r8_4_rdi_disp8:  case Param::sib_r8_4_r8_disp8:  case Param::sib_r8_4_r9_disp8:  case Param::sib_r8_4_r10_disp8:  case Param::sib_r8_4_r11_disp8:  case Param::sib_r8_4_r12_disp8:  case Param::sib_r8_4_r13_disp8:  case Param::sib_r8_4_r14_disp8:  case Param::sib_r8_4_r15_disp8:
			case Param::sib_r9_4_rax_disp8: case Param::sib_r9_4_rcx_disp8: case Param::sib_r9_4_rdx_disp8: case Param::sib_r9_4_rbx_disp8: case Param::sib_r9_4_rsp_disp8: case Param::sib_r9_4_rbp_disp8: case Param::sib_r9_4_rsi_disp8:  case Param::sib_r9_4_rdi_disp8:  case Param::sib_r9_4_r8_disp8:  case Param::sib_r9_4_r9_disp8:  case Param::sib_r9_4_r10_disp8:  case Param::sib_r9_4_r11_disp8:  case Param::sib_r9_4_r12_disp8:  case Param::sib_r9_4_r13_disp8:  case Param::sib_r9_4_r14_disp8:  case Param::sib_r9_4_r15_disp8:
			case Param::sib_r10_4_rax_disp8: case Param::sib_r10_4_rcx_disp8: case Param::sib_r10_4_rdx_disp8: case Param::sib_r10_4_rbx_disp8: case Param::sib_r10_4_rsp_disp8: case Param::sib_r10_4_rbp_disp8: case Param::sib_r10_4_rsi_disp8: case Param::sib_r10_4_rdi_disp8: case Param::sib_r10_4_r8_disp8: case Param::sib_r10_4_r9_disp8: case Param::sib_r10_4_r10_disp8: case Param::sib_r10_4_r11_disp8: case Param::sib_r10_4_r12_disp8: case Param::sib_r10_4_r13_disp8: case Param::sib_r10_4_r14_disp8: case Param::sib_r10_4_r15_disp8:
			case Param::sib_r11_4_rax_disp8: case Param::sib_r11_4_rcx_disp8: case Param::sib_r11_4_rdx_disp8: case Param::sib_r11_4_rbx_disp8: case Param::sib_r11_4_rsp_disp8: case Param::sib_r11_4_rbp_disp8: case Param::sib_r11_4_rsi_disp8: case Param::sib_r11_4_rdi_disp8: case Param::sib_r11_4_r8_disp8: case Param::sib_r11_4_r9_disp8: case Param::sib_r11_4_r10_disp8: case Param::sib_r11_4_r11_disp8: case Param::sib_r11_4_r12_disp8: case Param::sib_r11_4_r13_disp8: case Param::sib_r11_4_r14_disp8: case Param::sib_r11_4_r15_disp8:
			case Param::sib_r12_4_rax_disp8: case Param::sib_r12_4_rcx_disp8: case Param::sib_r12_4_rdx_disp8: case Param::sib_r12_4_rbx_disp8: case Param::sib_r12_4_rsp_disp8: case Param::sib_r12_4_rbp_disp8: case Param::sib_r12_4_rsi_disp8: case Param::sib_r12_4_rdi_disp8: case Param::sib_r12_4_r8_disp8: case Param::sib_r12_4_r9_disp8: case Param::sib_r12_4_r10_disp8: case Param::sib_r12_4_r11_disp8: case Param::sib_r12_4_r12_disp8: case Param::sib_r12_4_r13_disp8: case Param::sib_r12_4_r14_disp8: case Param::sib_r12_4_r15_disp8:
			case Param::sib_r13_4_rax_disp8: case Param::sib_r13_4_rcx_disp8: case Param::sib_r13_4_rdx_disp8: case Param::sib_r13_4_rbx_disp8: case Param::sib_r13_4_rsp_disp8: case Param::sib_r13_4_rbp_disp8: case Param::sib_r13_4_rsi_disp8: case Param::sib_r13_4_rdi_disp8: case Param::sib_r13_4_r8_disp8: case Param::sib_r13_4_r9_disp8: case Param::sib_r13_4_r10_disp8: case Param::sib_r13_4_r11_disp8: case Param::sib_r13_4_r12_disp8: case Param::sib_r13_4_r13_disp8: case Param::sib_r13_4_r14_disp8: case Param::sib_r13_4_r15_disp8:
			case Param::sib_r14_4_rax_disp8: case Param::sib_r14_4_rcx_disp8: case Param::sib_r14_4_rdx_disp8: case Param::sib_r14_4_rbx_disp8: case Param::sib_r14_4_rsp_disp8: case Param::sib_r14_4_rbp_disp8: case Param::sib_r14_4_rsi_disp8: case Param::sib_r14_4_rdi_disp8: case Param::sib_r14_4_r8_disp8: case Param::sib_r14_4_r9_disp8: case Param::sib_r14_4_r10_disp8: case Param::sib_r14_4_r11_disp8: case Param::sib_r14_4_r12_disp8: case Param::sib_r14_4_r13_disp8: case Param::sib_r14_4_r14_disp8: case Param::sib_r14_4_r15_disp8:
			case Param::sib_r15_4_rax_disp8: case Param::sib_r15_4_rcx_disp8: case Param::sib_r15_4_rdx_disp8: case Param::sib_r15_4_rbx_disp8: case Param::sib_r15_4_rsp_disp8: case Param::sib_r15_4_rbp_disp8: case Param::sib_r15_4_rsi_disp8: case Param::sib_r15_4_rdi_disp8: case Param::sib_r15_4_r8_disp8: case Param::sib_r15_4_r9_disp8: case Param::sib_r15_4_r10_disp8: case Param::sib_r15_4_r11_disp8: case Param::sib_r15_4_r12_disp8: case Param::sib_r15_4_r13_disp8: case Param::sib_r15_4_r14_disp8: case Param::sib_r15_4_r15_disp8:
			case Param::sib_rax_8_rax_disp8: case Param::sib_rax_8_rcx_disp8: case Param::sib_rax_8_rdx_disp8: case Param::sib_rax_8_rbx_disp8: case Param::sib_rax_8_rsp_disp8: case Param::sib_rax_8_rbp_disp8: case Param::sib_rax_8_rsi_disp8: case Param::sib_rax_8_rdi_disp8: case Param::sib_rax_8_r8_disp8: case Param::sib_rax_8_r9_disp8: case Param::sib_rax_8_r10_disp8: case Param::sib_rax_8_r11_disp8: case Param::sib_rax_8_r12_disp8: case Param::sib_rax_8_r13_disp8: case Param::sib_rax_8_r14_disp8: case Param::sib_rax_8_r15_disp8:
			case Param::sib_rcx_8_rax_disp8: case Param::sib_rcx_8_rcx_disp8: case Param::sib_rcx_8_rdx_disp8: case Param::sib_rcx_8_rbx_disp8: case Param::sib_rcx_8_rsp_disp8: case Param::sib_rcx_8_rbp_disp8: case Param::sib_rcx_8_rsi_disp8: case Param::sib_rcx_8_rdi_disp8: case Param::sib_rcx_8_r8_disp8: case Param::sib_rcx_8_r9_disp8: case Param::sib_rcx_8_r10_disp8: case Param::sib_rcx_8_r11_disp8: case Param::sib_rcx_8_r12_disp8: case Param::sib_rcx_8_r13_disp8: case Param::sib_rcx_8_r14_disp8: case Param::sib_rcx_8_r15_disp8:
			case Param::sib_rdx_8_rax_disp8: case Param::sib_rdx_8_rcx_disp8: case Param::sib_rdx_8_rdx_disp8: case Param::sib_rdx_8_rbx_disp8: case Param::sib_rdx_8_rsp_disp8: case Param::sib_rdx_8_rbp_disp8: case Param::sib_rdx_8_rsi_disp8: case Param::sib_rdx_8_rdi_disp8: case Param::sib_rdx_8_r8_disp8: case Param::sib_rdx_8_r9_disp8: case Param::sib_rdx_8_r10_disp8: case Param::sib_rdx_8_r11_disp8: case Param::sib_rdx_8_r12_disp8: case Param::sib_rdx_8_r13_disp8: case Param::sib_rdx_8_r14_disp8: case Param::sib_rdx_8_r15_disp8:
			case Param::sib_rbx_8_rax_disp8: case Param::sib_rbx_8_rcx_disp8: case Param::sib_rbx_8_rdx_disp8: case Param::sib_rbx_8_rbx_disp8: case Param::sib_rbx_8_rsp_disp8: case Param::sib_rbx_8_rbp_disp8: case Param::sib_rbx_8_rsi_disp8: case Param::sib_rbx_8_rdi_disp8: case Param::sib_rbx_8_r8_disp8: case Param::sib_rbx_8_r9_disp8: case Param::sib_rbx_8_r10_disp8: case Param::sib_rbx_8_r11_disp8: case Param::sib_rbx_8_r12_disp8: case Param::sib_rbx_8_r13_disp8: case Param::sib_rbx_8_r14_disp8: case Param::sib_rbx_8_r15_disp8:
			case Param::sib_rbp_8_rax_disp8: case Param::sib_rbp_8_rcx_disp8: case Param::sib_rbp_8_rdx_disp8: case Param::sib_rbp_8_rbx_disp8: case Param::sib_rbp_8_rsp_disp8: case Param::sib_rbp_8_rbp_disp8: case Param::sib_rbp_8_rsi_disp8: case Param::sib_rbp_8_rdi_disp8: case Param::sib_rbp_8_r8_disp8: case Param::sib_rbp_8_r9_disp8: case Param::sib_rbp_8_r10_disp8: case Param::sib_rbp_8_r11_disp8: case Param::sib_rbp_8_r12_disp8: case Param::sib_rbp_8_r13_disp8: case Param::sib_rbp_8_r14_disp8: case Param::sib_rbp_8_r15_disp8:
			case Param::sib_rsi_8_rax_disp8: case Param::sib_rsi_8_rcx_disp8: case Param::sib_rsi_8_rdx_disp8: case Param::sib_rsi_8_rbx_disp8: case Param::sib_rsi_8_rsp_disp8: case Param::sib_rsi_8_rbp_disp8: case Param::sib_rsi_8_rsi_disp8: case Param::sib_rsi_8_rdi_disp8: case Param::sib_rsi_8_r8_disp8: case Param::sib_rsi_8_r9_disp8: case Param::sib_rsi_8_r10_disp8: case Param::sib_rsi_8_r11_disp8: case Param::sib_rsi_8_r12_disp8: case Param::sib_rsi_8_r13_disp8: case Param::sib_rsi_8_r14_disp8: case Param::sib_rsi_8_r15_disp8:
			case Param::sib_rdi_8_rax_disp8: case Param::sib_rdi_8_rcx_disp8: case Param::sib_rdi_8_rdx_disp8: case Param::sib_rdi_8_rbx_disp8: case Param::sib_rdi_8_rsp_disp8: case Param::sib_rdi_8_rbp_disp8: case Param::sib_rdi_8_rsi_disp8: case Param::sib_rdi_8_rdi_disp8: case Param::sib_rdi_8_r8_disp8: case Param::sib_rdi_8_r9_disp8: case Param::sib_rdi_8_r10_disp8: case Param::sib_rdi_8_r11_disp8: case Param::sib_rdi_8_r12_disp8: case Param::sib_rdi_8_r13_disp8: case Param::sib_rdi_8_r14_disp8: case Param::sib_rdi_8_r15_disp8:
			case Param::sib_r8_8_rax_disp8: case Param::sib_r8_8_rcx_disp8: case Param::sib_r8_8_rdx_disp8: case Param::sib_r8_8_rbx_disp8: case Param::sib_r8_8_rsp_disp8: case Param::sib_r8_8_rbp_disp8: case Param::sib_r8_8_rsi_disp8:  case Param::sib_r8_8_rdi_disp8:  case Param::sib_r8_8_r8_disp8:  case Param::sib_r8_8_r9_disp8:  case Param::sib_r8_8_r10_disp8:  case Param::sib_r8_8_r11_disp8:  case Param::sib_r8_8_r12_disp8:  case Param::sib_r8_8_r13_disp8:  case Param::sib_r8_8_r14_disp8:  case Param::sib_r8_8_r15_disp8:
			case Param::sib_r9_8_rax_disp8: case Param::sib_r9_8_rcx_disp8: case Param::sib_r9_8_rdx_disp8: case Param::sib_r9_8_rbx_disp8: case Param::sib_r9_8_rsp_disp8: case Param::sib_r9_8_rbp_disp8: case Param::sib_r9_8_rsi_disp8:  case Param::sib_r9_8_rdi_disp8:  case Param::sib_r9_8_r8_disp8:  case Param::sib_r9_8_r9_disp8:  case Param::sib_r9_8_r10_disp8:  case Param::sib_r9_8_r11_disp8:  case Param::sib_r9_8_r12_disp8:  case Param::sib_r9_8_r13_disp8:  case Param::sib_r9_8_r14_disp8:  case Param::sib_r9_8_r15_disp8:
			case Param::sib_r10_8_rax_disp8: case Param::sib_r10_8_rcx_disp8: case Param::sib_r10_8_rdx_disp8: case Param::sib_r10_8_rbx_disp8: case Param::sib_r10_8_rsp_disp8: case Param::sib_r10_8_rbp_disp8: case Param::sib_r10_8_rsi_disp8: case Param::sib_r10_8_rdi_disp8: case Param::sib_r10_8_r8_disp8: case Param::sib_r10_8_r9_disp8: case Param::sib_r10_8_r10_disp8: case Param::sib_r10_8_r11_disp8: case Param::sib_r10_8_r12_disp8: case Param::sib_r10_8_r13_disp8: case Param::sib_r10_8_r14_disp8: case Param::sib_r10_8_r15_disp8:
			case Param::sib_r11_8_rax_disp8: case Param::sib_r11_8_rcx_disp8: case Param::sib_r11_8_rdx_disp8: case Param::sib_r11_8_rbx_disp8: case Param::sib_r11_8_rsp_disp8: case Param::sib_r11_8_rbp_disp8: case Param::sib_r11_8_rsi_disp8: case Param::sib_r11_8_rdi_disp8: case Param::sib_r11_8_r8_disp8: case Param::sib_r11_8_r9_disp8: case Param::sib_r11_8_r10_disp8: case Param::sib_r11_8_r11_disp8: case Param::sib_r11_8_r12_disp8: case Param::sib_r11_8_r13_disp8: case Param::sib_r11_8_r14_disp8: case Param::sib_r11_8_r15_disp8:
			case Param::sib_r12_8_rax_disp8: case Param::sib_r12_8_rcx_disp8: case Param::sib_r12_8_rdx_disp8: case Param::sib_r12_8_rbx_disp8: case Param::sib_r12_8_rsp_disp8: case Param::sib_r12_8_rbp_disp8: case Param::sib_r12_8_rsi_disp8: case Param::sib_r12_8_rdi_disp8: case Param::sib_r12_8_r8_disp8: case Param::sib_r12_8_r9_disp8: case Param::sib_r12_8_r10_disp8: case Param::sib_r12_8_r11_disp8: case Param::sib_r12_8_r12_disp8: case Param::sib_r12_8_r13_disp8: case Param::sib_r12_8_r14_disp8: case Param::sib_r12_8_r15_disp8:
			case Param::sib_r13_8_rax_disp8: case Param::sib_r13_8_rcx_disp8: case Param::sib_r13_8_rdx_disp8: case Param::sib_r13_8_rbx_disp8: case Param::sib_r13_8_rsp_disp8: case Param::sib_r13_8_rbp_disp8: case Param::sib_r13_8_rsi_disp8: case Param::sib_r13_8_rdi_disp8: case Param::sib_r13_8_r8_disp8: case Param::sib_r13_8_r9_disp8: case Param::sib_r13_8_r10_disp8: case Param::sib_r13_8_r11_disp8: case Param::sib_r13_8_r12_disp8: case Param::sib_r13_8_r13_disp8: case Param::sib_r13_8_r14_disp8: case Param::sib_r13_8_r15_disp8:
			case Param::sib_r14_8_rax_disp8: case Param::sib_r14_8_rcx_disp8: case Param::sib_r14_8_rdx_disp8: case Param::sib_r14_8_rbx_disp8: case Param::sib_r14_8_rsp_disp8: case Param::sib_r14_8_rbp_disp8: case Param::sib_r14_8_rsi_disp8: case Param::sib_r14_8_rdi_disp8: case Param::sib_r14_8_r8_disp8: case Param::sib_r14_8_r9_disp8: case Param::sib_r14_8_r10_disp8: case Param::sib_r14_8_r11_disp8: case Param::sib_r14_8_r12_disp8: case Param::sib_r14_8_r13_disp8: case Param::sib_r14_8_r14_disp8: case Param::sib_r14_8_r15_disp8:
			case Param::sib_r15_8_rax_disp8: case Param::sib_r15_8_rcx_disp8: case Param::sib_r15_8_rdx_disp8: case Param::sib_r15_8_rbx_disp8: case Param::sib_r15_8_rsp_disp8: case Param::sib_r15_8_rbp_disp8: case Param::sib_r15_8_rsi_disp8: case Param::sib_r15_8_rdi_disp8: case Param::sib_r15_8_r8_disp8: case Param::sib_r15_8_r9_disp8: case Param::sib_r15_8_r10_disp8: case Param::sib_r15_8_r11_disp8: case Param::sib_r15_8_r12_disp8: case Param::sib_r15_8_r13_disp8: case Param::sib_r15_8_r14_disp8: case Param::sib_r15_8_r15_disp8:
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
		switch (p)
		{
			case Param::m_disp32: case Param::m_eax_disp32: case Param::m_ecx_disp32: case Param::m_edx_disp32: case Param::m_ebx_disp32: case Param::m_ebp_disp32: case Param::m_esi_disp32: case Param::m_edi_disp32:
			case Param::m_rip_disp32: case Param::m_eip_disp32:
			case Param::m_rax_disp32: case Param::m_rcx_disp32: case Param::m_rdx_disp32: case Param::m_rbx_disp32: case Param::m_rbp_disp32: case Param::m_rsi_disp32: case Param::m_rdi_disp32:

			case Param::sib_eax_disp32: case Param::sib_ecx_disp32: case Param::sib_edx_disp32: case Param::sib_ebx_disp32: case Param::sib_none_disp32: case Param::sib_ebp_disp32: case Param::sib_esi_disp32: case Param::sib_edi_disp32: case Param::sib_r8d_disp32: case Param::sib_r9d_disp32: case Param::sib_r10d_disp32: case Param::sib_r11d_disp32: case Param::sib_r12d_disp32: case Param::sib_r13d_disp32: case Param::sib_r14d_disp32: case Param::sib_r15d_disp32:
			case Param::sib_eax_2_disp32: case Param::sib_ecx_2_disp32: case Param::sib_edx_2_disp32: case Param::sib_ebx_2_disp32: case Param::sib_ebp_2_disp32: case Param::sib_esi_2_disp32: case Param::sib_edi_2_disp32: case Param::sib_r8d_2_disp32: case Param::sib_r9d_2_disp32: case Param::sib_r10d_2_disp32: case Param::sib_r11d_2_disp32: case Param::sib_r12d_2_disp32: case Param::sib_r13d_2_disp32: case Param::sib_r14d_2_disp32: case Param::sib_r15d_2_disp32:
			case Param::sib_eax_4_disp32: case Param::sib_ecx_4_disp32: case Param::sib_edx_4_disp32: case Param::sib_ebx_4_disp32: case Param::sib_ebp_4_disp32: case Param::sib_esi_4_disp32: case Param::sib_edi_4_disp32: case Param::sib_r8d_4_disp32: case Param::sib_r9d_4_disp32: case Param::sib_r10d_4_disp32: case Param::sib_r11d_4_disp32: case Param::sib_r12d_4_disp32: case Param::sib_r13d_4_disp32: case Param::sib_r14d_4_disp32: case Param::sib_r15d_4_disp32:
			case Param::sib_eax_8_disp32: case Param::sib_ecx_8_disp32: case Param::sib_edx_8_disp32: case Param::sib_ebx_8_disp32: case Param::sib_ebp_8_disp32: case Param::sib_esi_8_disp32: case Param::sib_edi_8_disp32: case Param::sib_r8d_8_disp32: case Param::sib_r9d_8_disp32: case Param::sib_r10d_8_disp32: case Param::sib_r11d_8_disp32: case Param::sib_r12d_8_disp32: case Param::sib_r13d_8_disp32: case Param::sib_r14d_8_disp32: case Param::sib_r15d_8_disp32:
			case Param::sib_eax_eax_disp32: case Param::sib_eax_ecx_disp32: case Param::sib_eax_edx_disp32: case Param::sib_eax_ebx_disp32: case Param::sib_eax_esp_disp32: case Param::sib_eax_ebp_disp32: case Param::sib_eax_esi_disp32: case Param::sib_eax_edi_disp32: case Param::sib_eax_r8d_disp32: case Param::sib_eax_r9d_disp32: case Param::sib_eax_r10d_disp32: case Param::sib_eax_r11d_disp32: case Param::sib_eax_r12d_disp32: case Param::sib_eax_r13d_disp32: case Param::sib_eax_r14d_disp32: case Param::sib_eax_r15d_disp32:
			case Param::sib_ecx_eax_disp32: case Param::sib_ecx_ecx_disp32: case Param::sib_ecx_edx_disp32: case Param::sib_ecx_ebx_disp32: case Param::sib_ecx_esp_disp32: case Param::sib_ecx_ebp_disp32: case Param::sib_ecx_esi_disp32: case Param::sib_ecx_edi_disp32: case Param::sib_ecx_r8d_disp32: case Param::sib_ecx_r9d_disp32: case Param::sib_ecx_r10d_disp32: case Param::sib_ecx_r11d_disp32: case Param::sib_ecx_r12d_disp32: case Param::sib_ecx_r13d_disp32: case Param::sib_ecx_r14d_disp32: case Param::sib_ecx_r15d_disp32:
			case Param::sib_edx_eax_disp32: case Param::sib_edx_ecx_disp32: case Param::sib_edx_edx_disp32: case Param::sib_edx_ebx_disp32: case Param::sib_edx_esp_disp32: case Param::sib_edx_ebp_disp32: case Param::sib_edx_esi_disp32: case Param::sib_edx_edi_disp32: case Param::sib_edx_r8d_disp32: case Param::sib_edx_r9d_disp32: case Param::sib_edx_r10d_disp32: case Param::sib_edx_r11d_disp32: case Param::sib_edx_r12d_disp32: case Param::sib_edx_r13d_disp32: case Param::sib_edx_r14d_disp32: case Param::sib_edx_r15d_disp32:
			case Param::sib_ebx_eax_disp32: case Param::sib_ebx_ecx_disp32: case Param::sib_ebx_edx_disp32: case Param::sib_ebx_ebx_disp32: case Param::sib_ebx_esp_disp32: case Param::sib_ebx_ebp_disp32: case Param::sib_ebx_esi_disp32: case Param::sib_ebx_edi_disp32: case Param::sib_ebx_r8d_disp32: case Param::sib_ebx_r9d_disp32: case Param::sib_ebx_r10d_disp32: case Param::sib_ebx_r11d_disp32: case Param::sib_ebx_r12d_disp32: case Param::sib_ebx_r13d_disp32: case Param::sib_ebx_r14d_disp32: case Param::sib_ebx_r15d_disp32:
			case Param::sib_none_eax_disp32: case Param::sib_none_ecx_disp32: case Param::sib_none_edx_disp32: case Param::sib_none_ebx_disp32: case Param::sib_none_esp_disp32: case Param::sib_none_ebp_disp32: case Param::sib_none_esi_disp32: case Param::sib_none_edi_disp32: case Param::sib_none_r8d_disp32: case Param::sib_none_r9d_disp32: case Param::sib_none_r10d_disp32: case Param::sib_none_r11d_disp32: case Param::sib_none_r12d_disp32: case Param::sib_none_r13d_disp32: case Param::sib_none_r14d_disp32: case Param::sib_none_r15d_disp32:
			case Param::sib_ebp_eax_disp32: case Param::sib_ebp_ecx_disp32: case Param::sib_ebp_edx_disp32: case Param::sib_ebp_ebx_disp32: case Param::sib_ebp_esp_disp32: case Param::sib_ebp_ebp_disp32: case Param::sib_ebp_esi_disp32: case Param::sib_ebp_edi_disp32: case Param::sib_ebp_r8d_disp32: case Param::sib_ebp_r9d_disp32: case Param::sib_ebp_r10d_disp32: case Param::sib_ebp_r11d_disp32: case Param::sib_ebp_r12d_disp32: case Param::sib_ebp_r13d_disp32: case Param::sib_ebp_r14d_disp32: case Param::sib_ebp_r15d_disp32:
			case Param::sib_esi_eax_disp32: case Param::sib_esi_ecx_disp32: case Param::sib_esi_edx_disp32: case Param::sib_esi_ebx_disp32: case Param::sib_esi_esp_disp32: case Param::sib_esi_ebp_disp32: case Param::sib_esi_esi_disp32: case Param::sib_esi_edi_disp32: case Param::sib_esi_r8d_disp32: case Param::sib_esi_r9d_disp32: case Param::sib_esi_r10d_disp32: case Param::sib_esi_r11d_disp32: case Param::sib_esi_r12d_disp32: case Param::sib_esi_r13d_disp32: case Param::sib_esi_r14d_disp32: case Param::sib_esi_r15d_disp32:
			case Param::sib_edi_eax_disp32: case Param::sib_edi_ecx_disp32: case Param::sib_edi_edx_disp32: case Param::sib_edi_ebx_disp32: case Param::sib_edi_esp_disp32: case Param::sib_edi_ebp_disp32: case Param::sib_edi_esi_disp32: case Param::sib_edi_edi_disp32: case Param::sib_edi_r8d_disp32: case Param::sib_edi_r9d_disp32: case Param::sib_edi_r10d_disp32: case Param::sib_edi_r11d_disp32: case Param::sib_edi_r12d_disp32: case Param::sib_edi_r13d_disp32: case Param::sib_edi_r14d_disp32: case Param::sib_edi_r15d_disp32:
			case Param::sib_r8d_eax_disp32: case Param::sib_r8d_ecx_disp32: case Param::sib_r8d_edx_disp32: case Param::sib_r8d_ebx_disp32: case Param::sib_r8d_esp_disp32: case Param::sib_r8d_ebp_disp32: case Param::sib_r8d_esi_disp32: case Param::sib_r8d_edi_disp32: case Param::sib_r8d_r8d_disp32: case Param::sib_r8d_r9d_disp32: case Param::sib_r8d_r10d_disp32: case Param::sib_r8d_r11d_disp32: case Param::sib_r8d_r12d_disp32: case Param::sib_r8d_r13d_disp32: case Param::sib_r8d_r14d_disp32: case Param::sib_r8d_r15d_disp32:
			case Param::sib_r9d_eax_disp32: case Param::sib_r9d_ecx_disp32: case Param::sib_r9d_edx_disp32: case Param::sib_r9d_ebx_disp32: case Param::sib_r9d_esp_disp32: case Param::sib_r9d_ebp_disp32: case Param::sib_r9d_esi_disp32: case Param::sib_r9d_edi_disp32: case Param::sib_r9d_r8d_disp32: case Param::sib_r9d_r9d_disp32: case Param::sib_r9d_r10d_disp32: case Param::sib_r9d_r11d_disp32: case Param::sib_r9d_r12d_disp32: case Param::sib_r9d_r13d_disp32: case Param::sib_r9d_r14d_disp32: case Param::sib_r9d_r15d_disp32:
			case Param::sib_r10d_eax_disp32: case Param::sib_r10d_ecx_disp32: case Param::sib_r10d_edx_disp32: case Param::sib_r10d_ebx_disp32: case Param::sib_r10d_esp_disp32: case Param::sib_r10d_ebp_disp32: case Param::sib_r10d_esi_disp32: case Param::sib_r10d_edi_disp32: case Param::sib_r10d_r8d_disp32: case Param::sib_r10d_r9d_disp32: case Param::sib_r10d_r10d_disp32: case Param::sib_r10d_r11d_disp32: case Param::sib_r10d_r12d_disp32: case Param::sib_r10d_r13d_disp32: case Param::sib_r10d_r14d_disp32: case Param::sib_r10d_r15d_disp32:
			case Param::sib_r11d_eax_disp32: case Param::sib_r11d_ecx_disp32: case Param::sib_r11d_edx_disp32: case Param::sib_r11d_ebx_disp32: case Param::sib_r11d_esp_disp32: case Param::sib_r11d_ebp_disp32: case Param::sib_r11d_esi_disp32: case Param::sib_r11d_edi_disp32: case Param::sib_r11d_r8d_disp32: case Param::sib_r11d_r9d_disp32: case Param::sib_r11d_r10d_disp32: case Param::sib_r11d_r11d_disp32: case Param::sib_r11d_r12d_disp32: case Param::sib_r11d_r13d_disp32: case Param::sib_r11d_r14d_disp32: case Param::sib_r11d_r15d_disp32:
			case Param::sib_r12d_eax_disp32: case Param::sib_r12d_ecx_disp32: case Param::sib_r12d_edx_disp32: case Param::sib_r12d_ebx_disp32: case Param::sib_r12d_esp_disp32: case Param::sib_r12d_ebp_disp32: case Param::sib_r12d_esi_disp32: case Param::sib_r12d_edi_disp32: case Param::sib_r12d_r8d_disp32: case Param::sib_r12d_r9d_disp32: case Param::sib_r12d_r10d_disp32: case Param::sib_r12d_r11d_disp32: case Param::sib_r12d_r12d_disp32: case Param::sib_r12d_r13d_disp32: case Param::sib_r12d_r14d_disp32: case Param::sib_r12d_r15d_disp32:
			case Param::sib_r13d_eax_disp32: case Param::sib_r13d_ecx_disp32: case Param::sib_r13d_edx_disp32: case Param::sib_r13d_ebx_disp32: case Param::sib_r13d_esp_disp32: case Param::sib_r13d_ebp_disp32: case Param::sib_r13d_esi_disp32: case Param::sib_r13d_edi_disp32: case Param::sib_r13d_r8d_disp32: case Param::sib_r13d_r9d_disp32: case Param::sib_r13d_r10d_disp32: case Param::sib_r13d_r11d_disp32: case Param::sib_r13d_r12d_disp32: case Param::sib_r13d_r13d_disp32: case Param::sib_r13d_r14d_disp32: case Param::sib_r13d_r15d_disp32:
			case Param::sib_r14d_eax_disp32: case Param::sib_r14d_ecx_disp32: case Param::sib_r14d_edx_disp32: case Param::sib_r14d_ebx_disp32: case Param::sib_r14d_esp_disp32: case Param::sib_r14d_ebp_disp32: case Param::sib_r14d_esi_disp32: case Param::sib_r14d_edi_disp32: case Param::sib_r14d_r8d_disp32: case Param::sib_r14d_r9d_disp32: case Param::sib_r14d_r10d_disp32: case Param::sib_r14d_r11d_disp32: case Param::sib_r14d_r12d_disp32: case Param::sib_r14d_r13d_disp32: case Param::sib_r14d_r14d_disp32: case Param::sib_r14d_r15d_disp32:
			case Param::sib_r15d_eax_disp32: case Param::sib_r15d_ecx_disp32: case Param::sib_r15d_edx_disp32: case Param::sib_r15d_ebx_disp32: case Param::sib_r15d_esp_disp32: case Param::sib_r15d_ebp_disp32: case Param::sib_r15d_esi_disp32: case Param::sib_r15d_edi_disp32: case Param::sib_r15d_r8d_disp32: case Param::sib_r15d_r9d_disp32: case Param::sib_r15d_r10d_disp32: case Param::sib_r15d_r11d_disp32: case Param::sib_r15d_r12d_disp32: case Param::sib_r15d_r13d_disp32: case Param::sib_r15d_r14d_disp32: case Param::sib_r15d_r15d_disp32:
			case Param::sib_eax_2_eax_disp32: case Param::sib_eax_2_ecx_disp32: case Param::sib_eax_2_edx_disp32: case Param::sib_eax_2_ebx_disp32: case Param::sib_eax_2_esp_disp32: case Param::sib_eax_2_ebp_disp32: case Param::sib_eax_2_esi_disp32: case Param::sib_eax_2_edi_disp32: case Param::sib_eax_2_r8d_disp32: case Param::sib_eax_2_r9d_disp32: case Param::sib_eax_2_r10d_disp32: case Param::sib_eax_2_r11d_disp32: case Param::sib_eax_2_r12d_disp32: case Param::sib_eax_2_r13d_disp32: case Param::sib_eax_2_r14d_disp32: case Param::sib_eax_2_r15d_disp32:
			case Param::sib_ecx_2_eax_disp32: case Param::sib_ecx_2_ecx_disp32: case Param::sib_ecx_2_edx_disp32: case Param::sib_ecx_2_ebx_disp32: case Param::sib_ecx_2_esp_disp32: case Param::sib_ecx_2_ebp_disp32: case Param::sib_ecx_2_esi_disp32: case Param::sib_ecx_2_edi_disp32: case Param::sib_ecx_2_r8d_disp32: case Param::sib_ecx_2_r9d_disp32: case Param::sib_ecx_2_r10d_disp32: case Param::sib_ecx_2_r11d_disp32: case Param::sib_ecx_2_r12d_disp32: case Param::sib_ecx_2_r13d_disp32: case Param::sib_ecx_2_r14d_disp32: case Param::sib_ecx_2_r15d_disp32:
			case Param::sib_edx_2_eax_disp32: case Param::sib_edx_2_ecx_disp32: case Param::sib_edx_2_edx_disp32: case Param::sib_edx_2_ebx_disp32: case Param::sib_edx_2_esp_disp32: case Param::sib_edx_2_ebp_disp32: case Param::sib_edx_2_esi_disp32: case Param::sib_edx_2_edi_disp32: case Param::sib_edx_2_r8d_disp32: case Param::sib_edx_2_r9d_disp32: case Param::sib_edx_2_r10d_disp32: case Param::sib_edx_2_r11d_disp32: case Param::sib_edx_2_r12d_disp32: case Param::sib_edx_2_r13d_disp32: case Param::sib_edx_2_r14d_disp32: case Param::sib_edx_2_r15d_disp32:
			case Param::sib_ebx_2_eax_disp32: case Param::sib_ebx_2_ecx_disp32: case Param::sib_ebx_2_edx_disp32: case Param::sib_ebx_2_ebx_disp32: case Param::sib_ebx_2_esp_disp32: case Param::sib_ebx_2_ebp_disp32: case Param::sib_ebx_2_esi_disp32: case Param::sib_ebx_2_edi_disp32: case Param::sib_ebx_2_r8d_disp32: case Param::sib_ebx_2_r9d_disp32: case Param::sib_ebx_2_r10d_disp32: case Param::sib_ebx_2_r11d_disp32: case Param::sib_ebx_2_r12d_disp32: case Param::sib_ebx_2_r13d_disp32: case Param::sib_ebx_2_r14d_disp32: case Param::sib_ebx_2_r15d_disp32:
			case Param::sib_ebp_2_eax_disp32: case Param::sib_ebp_2_ecx_disp32: case Param::sib_ebp_2_edx_disp32: case Param::sib_ebp_2_ebx_disp32: case Param::sib_ebp_2_esp_disp32: case Param::sib_ebp_2_ebp_disp32: case Param::sib_ebp_2_esi_disp32: case Param::sib_ebp_2_edi_disp32: case Param::sib_ebp_2_r8d_disp32: case Param::sib_ebp_2_r9d_disp32: case Param::sib_ebp_2_r10d_disp32: case Param::sib_ebp_2_r11d_disp32: case Param::sib_ebp_2_r12d_disp32: case Param::sib_ebp_2_r13d_disp32: case Param::sib_ebp_2_r14d_disp32: case Param::sib_ebp_2_r15d_disp32:
			case Param::sib_esi_2_eax_disp32: case Param::sib_esi_2_ecx_disp32: case Param::sib_esi_2_edx_disp32: case Param::sib_esi_2_ebx_disp32: case Param::sib_esi_2_esp_disp32: case Param::sib_esi_2_ebp_disp32: case Param::sib_esi_2_esi_disp32: case Param::sib_esi_2_edi_disp32: case Param::sib_esi_2_r8d_disp32: case Param::sib_esi_2_r9d_disp32: case Param::sib_esi_2_r10d_disp32: case Param::sib_esi_2_r11d_disp32: case Param::sib_esi_2_r12d_disp32: case Param::sib_esi_2_r13d_disp32: case Param::sib_esi_2_r14d_disp32: case Param::sib_esi_2_r15d_disp32:
			case Param::sib_edi_2_eax_disp32: case Param::sib_edi_2_ecx_disp32: case Param::sib_edi_2_edx_disp32: case Param::sib_edi_2_ebx_disp32: case Param::sib_edi_2_esp_disp32: case Param::sib_edi_2_ebp_disp32: case Param::sib_edi_2_esi_disp32: case Param::sib_edi_2_edi_disp32: case Param::sib_edi_2_r8d_disp32: case Param::sib_edi_2_r9d_disp32: case Param::sib_edi_2_r10d_disp32: case Param::sib_edi_2_r11d_disp32: case Param::sib_edi_2_r12d_disp32: case Param::sib_edi_2_r13d_disp32: case Param::sib_edi_2_r14d_disp32: case Param::sib_edi_2_r15d_disp32:
			case Param::sib_r8d_2_eax_disp32: case Param::sib_r8d_2_ecx_disp32: case Param::sib_r8d_2_edx_disp32: case Param::sib_r8d_2_ebx_disp32: case Param::sib_r8d_2_esp_disp32: case Param::sib_r8d_2_ebp_disp32: case Param::sib_r8d_2_esi_disp32: case Param::sib_r8d_2_edi_disp32: case Param::sib_r8d_2_r8d_disp32: case Param::sib_r8d_2_r9d_disp32: case Param::sib_r8d_2_r10d_disp32: case Param::sib_r8d_2_r11d_disp32: case Param::sib_r8d_2_r12d_disp32: case Param::sib_r8d_2_r13d_disp32: case Param::sib_r8d_2_r14d_disp32: case Param::sib_r8d_2_r15d_disp32:
			case Param::sib_r9d_2_eax_disp32: case Param::sib_r9d_2_ecx_disp32: case Param::sib_r9d_2_edx_disp32: case Param::sib_r9d_2_ebx_disp32: case Param::sib_r9d_2_esp_disp32: case Param::sib_r9d_2_ebp_disp32: case Param::sib_r9d_2_esi_disp32: case Param::sib_r9d_2_edi_disp32: case Param::sib_r9d_2_r8d_disp32: case Param::sib_r9d_2_r9d_disp32: case Param::sib_r9d_2_r10d_disp32: case Param::sib_r9d_2_r11d_disp32: case Param::sib_r9d_2_r12d_disp32: case Param::sib_r9d_2_r13d_disp32: case Param::sib_r9d_2_r14d_disp32: case Param::sib_r9d_2_r15d_disp32:
			case Param::sib_r10d_2_eax_disp32: case Param::sib_r10d_2_ecx_disp32: case Param::sib_r10d_2_edx_disp32: case Param::sib_r10d_2_ebx_disp32: case Param::sib_r10d_2_esp_disp32: case Param::sib_r10d_2_ebp_disp32: case Param::sib_r10d_2_esi_disp32: case Param::sib_r10d_2_edi_disp32: case Param::sib_r10d_2_r8d_disp32: case Param::sib_r10d_2_r9d_disp32: case Param::sib_r10d_2_r10d_disp32: case Param::sib_r10d_2_r11d_disp32: case Param::sib_r10d_2_r12d_disp32: case Param::sib_r10d_2_r13d_disp32: case Param::sib_r10d_2_r14d_disp32: case Param::sib_r10d_2_r15d_disp32:
			case Param::sib_r11d_2_eax_disp32: case Param::sib_r11d_2_ecx_disp32: case Param::sib_r11d_2_edx_disp32: case Param::sib_r11d_2_ebx_disp32: case Param::sib_r11d_2_esp_disp32: case Param::sib_r11d_2_ebp_disp32: case Param::sib_r11d_2_esi_disp32: case Param::sib_r11d_2_edi_disp32: case Param::sib_r11d_2_r8d_disp32: case Param::sib_r11d_2_r9d_disp32: case Param::sib_r11d_2_r10d_disp32: case Param::sib_r11d_2_r11d_disp32: case Param::sib_r11d_2_r12d_disp32: case Param::sib_r11d_2_r13d_disp32: case Param::sib_r11d_2_r14d_disp32: case Param::sib_r11d_2_r15d_disp32:
			case Param::sib_r12d_2_eax_disp32: case Param::sib_r12d_2_ecx_disp32: case Param::sib_r12d_2_edx_disp32: case Param::sib_r12d_2_ebx_disp32: case Param::sib_r12d_2_esp_disp32: case Param::sib_r12d_2_ebp_disp32: case Param::sib_r12d_2_esi_disp32: case Param::sib_r12d_2_edi_disp32: case Param::sib_r12d_2_r8d_disp32: case Param::sib_r12d_2_r9d_disp32: case Param::sib_r12d_2_r10d_disp32: case Param::sib_r12d_2_r11d_disp32: case Param::sib_r12d_2_r12d_disp32: case Param::sib_r12d_2_r13d_disp32: case Param::sib_r12d_2_r14d_disp32: case Param::sib_r12d_2_r15d_disp32:
			case Param::sib_r13d_2_eax_disp32: case Param::sib_r13d_2_ecx_disp32: case Param::sib_r13d_2_edx_disp32: case Param::sib_r13d_2_ebx_disp32: case Param::sib_r13d_2_esp_disp32: case Param::sib_r13d_2_ebp_disp32: case Param::sib_r13d_2_esi_disp32: case Param::sib_r13d_2_edi_disp32: case Param::sib_r13d_2_r8d_disp32: case Param::sib_r13d_2_r9d_disp32: case Param::sib_r13d_2_r10d_disp32: case Param::sib_r13d_2_r11d_disp32: case Param::sib_r13d_2_r12d_disp32: case Param::sib_r13d_2_r13d_disp32: case Param::sib_r13d_2_r14d_disp32: case Param::sib_r13d_2_r15d_disp32:
			case Param::sib_r14d_2_eax_disp32: case Param::sib_r14d_2_ecx_disp32: case Param::sib_r14d_2_edx_disp32: case Param::sib_r14d_2_ebx_disp32: case Param::sib_r14d_2_esp_disp32: case Param::sib_r14d_2_ebp_disp32: case Param::sib_r14d_2_esi_disp32: case Param::sib_r14d_2_edi_disp32: case Param::sib_r14d_2_r8d_disp32: case Param::sib_r14d_2_r9d_disp32: case Param::sib_r14d_2_r10d_disp32: case Param::sib_r14d_2_r11d_disp32: case Param::sib_r14d_2_r12d_disp32: case Param::sib_r14d_2_r13d_disp32: case Param::sib_r14d_2_r14d_disp32: case Param::sib_r14d_2_r15d_disp32:
			case Param::sib_r15d_2_eax_disp32: case Param::sib_r15d_2_ecx_disp32: case Param::sib_r15d_2_edx_disp32: case Param::sib_r15d_2_ebx_disp32: case Param::sib_r15d_2_esp_disp32: case Param::sib_r15d_2_ebp_disp32: case Param::sib_r15d_2_esi_disp32: case Param::sib_r15d_2_edi_disp32: case Param::sib_r15d_2_r8d_disp32: case Param::sib_r15d_2_r9d_disp32: case Param::sib_r15d_2_r10d_disp32: case Param::sib_r15d_2_r11d_disp32: case Param::sib_r15d_2_r12d_disp32: case Param::sib_r15d_2_r13d_disp32: case Param::sib_r15d_2_r14d_disp32: case Param::sib_r15d_2_r15d_disp32:
			case Param::sib_eax_4_eax_disp32: case Param::sib_eax_4_ecx_disp32: case Param::sib_eax_4_edx_disp32: case Param::sib_eax_4_ebx_disp32: case Param::sib_eax_4_esp_disp32: case Param::sib_eax_4_ebp_disp32: case Param::sib_eax_4_esi_disp32: case Param::sib_eax_4_edi_disp32: case Param::sib_eax_4_r8d_disp32: case Param::sib_eax_4_r9d_disp32: case Param::sib_eax_4_r10d_disp32: case Param::sib_eax_4_r11d_disp32: case Param::sib_eax_4_r12d_disp32: case Param::sib_eax_4_r13d_disp32: case Param::sib_eax_4_r14d_disp32: case Param::sib_eax_4_r15d_disp32:
			case Param::sib_ecx_4_eax_disp32: case Param::sib_ecx_4_ecx_disp32: case Param::sib_ecx_4_edx_disp32: case Param::sib_ecx_4_ebx_disp32: case Param::sib_ecx_4_esp_disp32: case Param::sib_ecx_4_ebp_disp32: case Param::sib_ecx_4_esi_disp32: case Param::sib_ecx_4_edi_disp32: case Param::sib_ecx_4_r8d_disp32: case Param::sib_ecx_4_r9d_disp32: case Param::sib_ecx_4_r10d_disp32: case Param::sib_ecx_4_r11d_disp32: case Param::sib_ecx_4_r12d_disp32: case Param::sib_ecx_4_r13d_disp32: case Param::sib_ecx_4_r14d_disp32: case Param::sib_ecx_4_r15d_disp32:
			case Param::sib_edx_4_eax_disp32: case Param::sib_edx_4_ecx_disp32: case Param::sib_edx_4_edx_disp32: case Param::sib_edx_4_ebx_disp32: case Param::sib_edx_4_esp_disp32: case Param::sib_edx_4_ebp_disp32: case Param::sib_edx_4_esi_disp32: case Param::sib_edx_4_edi_disp32: case Param::sib_edx_4_r8d_disp32: case Param::sib_edx_4_r9d_disp32: case Param::sib_edx_4_r10d_disp32: case Param::sib_edx_4_r11d_disp32: case Param::sib_edx_4_r12d_disp32: case Param::sib_edx_4_r13d_disp32: case Param::sib_edx_4_r14d_disp32: case Param::sib_edx_4_r15d_disp32:
			case Param::sib_ebx_4_eax_disp32: case Param::sib_ebx_4_ecx_disp32: case Param::sib_ebx_4_edx_disp32: case Param::sib_ebx_4_ebx_disp32: case Param::sib_ebx_4_esp_disp32: case Param::sib_ebx_4_ebp_disp32: case Param::sib_ebx_4_esi_disp32: case Param::sib_ebx_4_edi_disp32: case Param::sib_ebx_4_r8d_disp32: case Param::sib_ebx_4_r9d_disp32: case Param::sib_ebx_4_r10d_disp32: case Param::sib_ebx_4_r11d_disp32: case Param::sib_ebx_4_r12d_disp32: case Param::sib_ebx_4_r13d_disp32: case Param::sib_ebx_4_r14d_disp32: case Param::sib_ebx_4_r15d_disp32:
			case Param::sib_ebp_4_eax_disp32: case Param::sib_ebp_4_ecx_disp32: case Param::sib_ebp_4_edx_disp32: case Param::sib_ebp_4_ebx_disp32: case Param::sib_ebp_4_esp_disp32: case Param::sib_ebp_4_ebp_disp32: case Param::sib_ebp_4_esi_disp32: case Param::sib_ebp_4_edi_disp32: case Param::sib_ebp_4_r8d_disp32: case Param::sib_ebp_4_r9d_disp32: case Param::sib_ebp_4_r10d_disp32: case Param::sib_ebp_4_r11d_disp32: case Param::sib_ebp_4_r12d_disp32: case Param::sib_ebp_4_r13d_disp32: case Param::sib_ebp_4_r14d_disp32: case Param::sib_ebp_4_r15d_disp32:
			case Param::sib_esi_4_eax_disp32: case Param::sib_esi_4_ecx_disp32: case Param::sib_esi_4_edx_disp32: case Param::sib_esi_4_ebx_disp32: case Param::sib_esi_4_esp_disp32: case Param::sib_esi_4_ebp_disp32: case Param::sib_esi_4_esi_disp32: case Param::sib_esi_4_edi_disp32: case Param::sib_esi_4_r8d_disp32: case Param::sib_esi_4_r9d_disp32: case Param::sib_esi_4_r10d_disp32: case Param::sib_esi_4_r11d_disp32: case Param::sib_esi_4_r12d_disp32: case Param::sib_esi_4_r13d_disp32: case Param::sib_esi_4_r14d_disp32: case Param::sib_esi_4_r15d_disp32:
			case Param::sib_edi_4_eax_disp32: case Param::sib_edi_4_ecx_disp32: case Param::sib_edi_4_edx_disp32: case Param::sib_edi_4_ebx_disp32: case Param::sib_edi_4_esp_disp32: case Param::sib_edi_4_ebp_disp32: case Param::sib_edi_4_esi_disp32: case Param::sib_edi_4_edi_disp32: case Param::sib_edi_4_r8d_disp32: case Param::sib_edi_4_r9d_disp32: case Param::sib_edi_4_r10d_disp32: case Param::sib_edi_4_r11d_disp32: case Param::sib_edi_4_r12d_disp32: case Param::sib_edi_4_r13d_disp32: case Param::sib_edi_4_r14d_disp32: case Param::sib_edi_4_r15d_disp32:
			case Param::sib_r8d_4_eax_disp32: case Param::sib_r8d_4_ecx_disp32: case Param::sib_r8d_4_edx_disp32: case Param::sib_r8d_4_ebx_disp32: case Param::sib_r8d_4_esp_disp32: case Param::sib_r8d_4_ebp_disp32: case Param::sib_r8d_4_esi_disp32: case Param::sib_r8d_4_edi_disp32: case Param::sib_r8d_4_r8d_disp32: case Param::sib_r8d_4_r9d_disp32: case Param::sib_r8d_4_r10d_disp32: case Param::sib_r8d_4_r11d_disp32: case Param::sib_r8d_4_r12d_disp32: case Param::sib_r8d_4_r13d_disp32: case Param::sib_r8d_4_r14d_disp32: case Param::sib_r8d_4_r15d_disp32:
			case Param::sib_r9d_4_eax_disp32: case Param::sib_r9d_4_ecx_disp32: case Param::sib_r9d_4_edx_disp32: case Param::sib_r9d_4_ebx_disp32: case Param::sib_r9d_4_esp_disp32: case Param::sib_r9d_4_ebp_disp32: case Param::sib_r9d_4_esi_disp32: case Param::sib_r9d_4_edi_disp32: case Param::sib_r9d_4_r8d_disp32: case Param::sib_r9d_4_r9d_disp32: case Param::sib_r9d_4_r10d_disp32: case Param::sib_r9d_4_r11d_disp32: case Param::sib_r9d_4_r12d_disp32: case Param::sib_r9d_4_r13d_disp32: case Param::sib_r9d_4_r14d_disp32: case Param::sib_r9d_4_r15d_disp32:
			case Param::sib_r10d_4_eax_disp32: case Param::sib_r10d_4_ecx_disp32: case Param::sib_r10d_4_edx_disp32: case Param::sib_r10d_4_ebx_disp32: case Param::sib_r10d_4_esp_disp32: case Param::sib_r10d_4_ebp_disp32: case Param::sib_r10d_4_esi_disp32: case Param::sib_r10d_4_edi_disp32: case Param::sib_r10d_4_r8d_disp32: case Param::sib_r10d_4_r9d_disp32: case Param::sib_r10d_4_r10d_disp32: case Param::sib_r10d_4_r11d_disp32: case Param::sib_r10d_4_r12d_disp32: case Param::sib_r10d_4_r13d_disp32: case Param::sib_r10d_4_r14d_disp32: case Param::sib_r10d_4_r15d_disp32:
			case Param::sib_r11d_4_eax_disp32: case Param::sib_r11d_4_ecx_disp32: case Param::sib_r11d_4_edx_disp32: case Param::sib_r11d_4_ebx_disp32: case Param::sib_r11d_4_esp_disp32: case Param::sib_r11d_4_ebp_disp32: case Param::sib_r11d_4_esi_disp32: case Param::sib_r11d_4_edi_disp32: case Param::sib_r11d_4_r8d_disp32: case Param::sib_r11d_4_r9d_disp32: case Param::sib_r11d_4_r10d_disp32: case Param::sib_r11d_4_r11d_disp32: case Param::sib_r11d_4_r12d_disp32: case Param::sib_r11d_4_r13d_disp32: case Param::sib_r11d_4_r14d_disp32: case Param::sib_r11d_4_r15d_disp32:
			case Param::sib_r12d_4_eax_disp32: case Param::sib_r12d_4_ecx_disp32: case Param::sib_r12d_4_edx_disp32: case Param::sib_r12d_4_ebx_disp32: case Param::sib_r12d_4_esp_disp32: case Param::sib_r12d_4_ebp_disp32: case Param::sib_r12d_4_esi_disp32: case Param::sib_r12d_4_edi_disp32: case Param::sib_r12d_4_r8d_disp32: case Param::sib_r12d_4_r9d_disp32: case Param::sib_r12d_4_r10d_disp32: case Param::sib_r12d_4_r11d_disp32: case Param::sib_r12d_4_r12d_disp32: case Param::sib_r12d_4_r13d_disp32: case Param::sib_r12d_4_r14d_disp32: case Param::sib_r12d_4_r15d_disp32:
			case Param::sib_r13d_4_eax_disp32: case Param::sib_r13d_4_ecx_disp32: case Param::sib_r13d_4_edx_disp32: case Param::sib_r13d_4_ebx_disp32: case Param::sib_r13d_4_esp_disp32: case Param::sib_r13d_4_ebp_disp32: case Param::sib_r13d_4_esi_disp32: case Param::sib_r13d_4_edi_disp32: case Param::sib_r13d_4_r8d_disp32: case Param::sib_r13d_4_r9d_disp32: case Param::sib_r13d_4_r10d_disp32: case Param::sib_r13d_4_r11d_disp32: case Param::sib_r13d_4_r12d_disp32: case Param::sib_r13d_4_r13d_disp32: case Param::sib_r13d_4_r14d_disp32: case Param::sib_r13d_4_r15d_disp32:
			case Param::sib_r14d_4_eax_disp32: case Param::sib_r14d_4_ecx_disp32: case Param::sib_r14d_4_edx_disp32: case Param::sib_r14d_4_ebx_disp32: case Param::sib_r14d_4_esp_disp32: case Param::sib_r14d_4_ebp_disp32: case Param::sib_r14d_4_esi_disp32: case Param::sib_r14d_4_edi_disp32: case Param::sib_r14d_4_r8d_disp32: case Param::sib_r14d_4_r9d_disp32: case Param::sib_r14d_4_r10d_disp32: case Param::sib_r14d_4_r11d_disp32: case Param::sib_r14d_4_r12d_disp32: case Param::sib_r14d_4_r13d_disp32: case Param::sib_r14d_4_r14d_disp32: case Param::sib_r14d_4_r15d_disp32:
			case Param::sib_r15d_4_eax_disp32: case Param::sib_r15d_4_ecx_disp32: case Param::sib_r15d_4_edx_disp32: case Param::sib_r15d_4_ebx_disp32: case Param::sib_r15d_4_esp_disp32: case Param::sib_r15d_4_ebp_disp32: case Param::sib_r15d_4_esi_disp32: case Param::sib_r15d_4_edi_disp32: case Param::sib_r15d_4_r8d_disp32: case Param::sib_r15d_4_r9d_disp32: case Param::sib_r15d_4_r10d_disp32: case Param::sib_r15d_4_r11d_disp32: case Param::sib_r15d_4_r12d_disp32: case Param::sib_r15d_4_r13d_disp32: case Param::sib_r15d_4_r14d_disp32: case Param::sib_r15d_4_r15d_disp32:
			case Param::sib_eax_8_eax_disp32: case Param::sib_eax_8_ecx_disp32: case Param::sib_eax_8_edx_disp32: case Param::sib_eax_8_ebx_disp32: case Param::sib_eax_8_esp_disp32: case Param::sib_eax_8_ebp_disp32: case Param::sib_eax_8_esi_disp32: case Param::sib_eax_8_edi_disp32: case Param::sib_eax_8_r8d_disp32: case Param::sib_eax_8_r9d_disp32: case Param::sib_eax_8_r10d_disp32: case Param::sib_eax_8_r11d_disp32: case Param::sib_eax_8_r12d_disp32: case Param::sib_eax_8_r13d_disp32: case Param::sib_eax_8_r14d_disp32: case Param::sib_eax_8_r15d_disp32:
			case Param::sib_ecx_8_eax_disp32: case Param::sib_ecx_8_ecx_disp32: case Param::sib_ecx_8_edx_disp32: case Param::sib_ecx_8_ebx_disp32: case Param::sib_ecx_8_esp_disp32: case Param::sib_ecx_8_ebp_disp32: case Param::sib_ecx_8_esi_disp32: case Param::sib_ecx_8_edi_disp32: case Param::sib_ecx_8_r8d_disp32: case Param::sib_ecx_8_r9d_disp32: case Param::sib_ecx_8_r10d_disp32: case Param::sib_ecx_8_r11d_disp32: case Param::sib_ecx_8_r12d_disp32: case Param::sib_ecx_8_r13d_disp32: case Param::sib_ecx_8_r14d_disp32: case Param::sib_ecx_8_r15d_disp32:
			case Param::sib_edx_8_eax_disp32: case Param::sib_edx_8_ecx_disp32: case Param::sib_edx_8_edx_disp32: case Param::sib_edx_8_ebx_disp32: case Param::sib_edx_8_esp_disp32: case Param::sib_edx_8_ebp_disp32: case Param::sib_edx_8_esi_disp32: case Param::sib_edx_8_edi_disp32: case Param::sib_edx_8_r8d_disp32: case Param::sib_edx_8_r9d_disp32: case Param::sib_edx_8_r10d_disp32: case Param::sib_edx_8_r11d_disp32: case Param::sib_edx_8_r12d_disp32: case Param::sib_edx_8_r13d_disp32: case Param::sib_edx_8_r14d_disp32: case Param::sib_edx_8_r15d_disp32:
			case Param::sib_ebx_8_eax_disp32: case Param::sib_ebx_8_ecx_disp32: case Param::sib_ebx_8_edx_disp32: case Param::sib_ebx_8_ebx_disp32: case Param::sib_ebx_8_esp_disp32: case Param::sib_ebx_8_ebp_disp32: case Param::sib_ebx_8_esi_disp32: case Param::sib_ebx_8_edi_disp32: case Param::sib_ebx_8_r8d_disp32: case Param::sib_ebx_8_r9d_disp32: case Param::sib_ebx_8_r10d_disp32: case Param::sib_ebx_8_r11d_disp32: case Param::sib_ebx_8_r12d_disp32: case Param::sib_ebx_8_r13d_disp32: case Param::sib_ebx_8_r14d_disp32: case Param::sib_ebx_8_r15d_disp32:
			case Param::sib_ebp_8_eax_disp32: case Param::sib_ebp_8_ecx_disp32: case Param::sib_ebp_8_edx_disp32: case Param::sib_ebp_8_ebx_disp32: case Param::sib_ebp_8_esp_disp32: case Param::sib_ebp_8_ebp_disp32: case Param::sib_ebp_8_esi_disp32: case Param::sib_ebp_8_edi_disp32: case Param::sib_ebp_8_r8d_disp32: case Param::sib_ebp_8_r9d_disp32: case Param::sib_ebp_8_r10d_disp32: case Param::sib_ebp_8_r11d_disp32: case Param::sib_ebp_8_r12d_disp32: case Param::sib_ebp_8_r13d_disp32: case Param::sib_ebp_8_r14d_disp32: case Param::sib_ebp_8_r15d_disp32:
			case Param::sib_esi_8_eax_disp32: case Param::sib_esi_8_ecx_disp32: case Param::sib_esi_8_edx_disp32: case Param::sib_esi_8_ebx_disp32: case Param::sib_esi_8_esp_disp32: case Param::sib_esi_8_ebp_disp32: case Param::sib_esi_8_esi_disp32: case Param::sib_esi_8_edi_disp32: case Param::sib_esi_8_r8d_disp32: case Param::sib_esi_8_r9d_disp32: case Param::sib_esi_8_r10d_disp32: case Param::sib_esi_8_r11d_disp32: case Param::sib_esi_8_r12d_disp32: case Param::sib_esi_8_r13d_disp32: case Param::sib_esi_8_r14d_disp32: case Param::sib_esi_8_r15d_disp32:
			case Param::sib_edi_8_eax_disp32: case Param::sib_edi_8_ecx_disp32: case Param::sib_edi_8_edx_disp32: case Param::sib_edi_8_ebx_disp32: case Param::sib_edi_8_esp_disp32: case Param::sib_edi_8_ebp_disp32: case Param::sib_edi_8_esi_disp32: case Param::sib_edi_8_edi_disp32: case Param::sib_edi_8_r8d_disp32: case Param::sib_edi_8_r9d_disp32: case Param::sib_edi_8_r10d_disp32: case Param::sib_edi_8_r11d_disp32: case Param::sib_edi_8_r12d_disp32: case Param::sib_edi_8_r13d_disp32: case Param::sib_edi_8_r14d_disp32: case Param::sib_edi_8_r15d_disp32:
			case Param::sib_r8d_8_eax_disp32: case Param::sib_r8d_8_ecx_disp32: case Param::sib_r8d_8_edx_disp32: case Param::sib_r8d_8_ebx_disp32: case Param::sib_r8d_8_esp_disp32: case Param::sib_r8d_8_ebp_disp32: case Param::sib_r8d_8_esi_disp32: case Param::sib_r8d_8_edi_disp32: case Param::sib_r8d_8_r8d_disp32: case Param::sib_r8d_8_r9d_disp32: case Param::sib_r8d_8_r10d_disp32: case Param::sib_r8d_8_r11d_disp32: case Param::sib_r8d_8_r12d_disp32: case Param::sib_r8d_8_r13d_disp32: case Param::sib_r8d_8_r14d_disp32: case Param::sib_r8d_8_r15d_disp32:
			case Param::sib_r9d_8_eax_disp32: case Param::sib_r9d_8_ecx_disp32: case Param::sib_r9d_8_edx_disp32: case Param::sib_r9d_8_ebx_disp32: case Param::sib_r9d_8_esp_disp32: case Param::sib_r9d_8_ebp_disp32: case Param::sib_r9d_8_esi_disp32: case Param::sib_r9d_8_edi_disp32: case Param::sib_r9d_8_r8d_disp32: case Param::sib_r9d_8_r9d_disp32: case Param::sib_r9d_8_r10d_disp32: case Param::sib_r9d_8_r11d_disp32: case Param::sib_r9d_8_r12d_disp32: case Param::sib_r9d_8_r13d_disp32: case Param::sib_r9d_8_r14d_disp32: case Param::sib_r9d_8_r15d_disp32:
			case Param::sib_r10d_8_eax_disp32: case Param::sib_r10d_8_ecx_disp32: case Param::sib_r10d_8_edx_disp32: case Param::sib_r10d_8_ebx_disp32: case Param::sib_r10d_8_esp_disp32: case Param::sib_r10d_8_ebp_disp32: case Param::sib_r10d_8_esi_disp32: case Param::sib_r10d_8_edi_disp32: case Param::sib_r10d_8_r8d_disp32: case Param::sib_r10d_8_r9d_disp32: case Param::sib_r10d_8_r10d_disp32: case Param::sib_r10d_8_r11d_disp32: case Param::sib_r10d_8_r12d_disp32: case Param::sib_r10d_8_r13d_disp32: case Param::sib_r10d_8_r14d_disp32: case Param::sib_r10d_8_r15d_disp32:
			case Param::sib_r11d_8_eax_disp32: case Param::sib_r11d_8_ecx_disp32: case Param::sib_r11d_8_edx_disp32: case Param::sib_r11d_8_ebx_disp32: case Param::sib_r11d_8_esp_disp32: case Param::sib_r11d_8_ebp_disp32: case Param::sib_r11d_8_esi_disp32: case Param::sib_r11d_8_edi_disp32: case Param::sib_r11d_8_r8d_disp32: case Param::sib_r11d_8_r9d_disp32: case Param::sib_r11d_8_r10d_disp32: case Param::sib_r11d_8_r11d_disp32: case Param::sib_r11d_8_r12d_disp32: case Param::sib_r11d_8_r13d_disp32: case Param::sib_r11d_8_r14d_disp32: case Param::sib_r11d_8_r15d_disp32:
			case Param::sib_r12d_8_eax_disp32: case Param::sib_r12d_8_ecx_disp32: case Param::sib_r12d_8_edx_disp32: case Param::sib_r12d_8_ebx_disp32: case Param::sib_r12d_8_esp_disp32: case Param::sib_r12d_8_ebp_disp32: case Param::sib_r12d_8_esi_disp32: case Param::sib_r12d_8_edi_disp32: case Param::sib_r12d_8_r8d_disp32: case Param::sib_r12d_8_r9d_disp32: case Param::sib_r12d_8_r10d_disp32: case Param::sib_r12d_8_r11d_disp32: case Param::sib_r12d_8_r12d_disp32: case Param::sib_r12d_8_r13d_disp32: case Param::sib_r12d_8_r14d_disp32: case Param::sib_r12d_8_r15d_disp32:
			case Param::sib_r13d_8_eax_disp32: case Param::sib_r13d_8_ecx_disp32: case Param::sib_r13d_8_edx_disp32: case Param::sib_r13d_8_ebx_disp32: case Param::sib_r13d_8_esp_disp32: case Param::sib_r13d_8_ebp_disp32: case Param::sib_r13d_8_esi_disp32: case Param::sib_r13d_8_edi_disp32: case Param::sib_r13d_8_r8d_disp32: case Param::sib_r13d_8_r9d_disp32: case Param::sib_r13d_8_r10d_disp32: case Param::sib_r13d_8_r11d_disp32: case Param::sib_r13d_8_r12d_disp32: case Param::sib_r13d_8_r13d_disp32: case Param::sib_r13d_8_r14d_disp32: case Param::sib_r13d_8_r15d_disp32:
			case Param::sib_r14d_8_eax_disp32: case Param::sib_r14d_8_ecx_disp32: case Param::sib_r14d_8_edx_disp32: case Param::sib_r14d_8_ebx_disp32: case Param::sib_r14d_8_esp_disp32: case Param::sib_r14d_8_ebp_disp32: case Param::sib_r14d_8_esi_disp32: case Param::sib_r14d_8_edi_disp32: case Param::sib_r14d_8_r8d_disp32: case Param::sib_r14d_8_r9d_disp32: case Param::sib_r14d_8_r10d_disp32: case Param::sib_r14d_8_r11d_disp32: case Param::sib_r14d_8_r12d_disp32: case Param::sib_r14d_8_r13d_disp32: case Param::sib_r14d_8_r14d_disp32: case Param::sib_r14d_8_r15d_disp32:
			case Param::sib_r15d_8_eax_disp32: case Param::sib_r15d_8_ecx_disp32: case Param::sib_r15d_8_edx_disp32: case Param::sib_r15d_8_ebx_disp32: case Param::sib_r15d_8_esp_disp32: case Param::sib_r15d_8_ebp_disp32: case Param::sib_r15d_8_esi_disp32: case Param::sib_r15d_8_edi_disp32: case Param::sib_r15d_8_r8d_disp32: case Param::sib_r15d_8_r9d_disp32: case Param::sib_r15d_8_r10d_disp32: case Param::sib_r15d_8_r11d_disp32: case Param::sib_r15d_8_r12d_disp32: case Param::sib_r15d_8_r13d_disp32: case Param::sib_r15d_8_r14d_disp32: case Param::sib_r15d_8_r15d_disp32:

			case Param::sib_rax_disp32: case Param::sib_rcx_disp32: case Param::sib_rdx_disp32: case Param::sib_rbx_disp32: case Param::sib_rbp_disp32: case Param::sib_rsi_disp32: case Param::sib_rdi_disp32: case Param::sib_r8_disp32: case Param::sib_r9_disp32: case Param::sib_r10_disp32: case Param::sib_r11_disp32: case Param::sib_r12_disp32: case Param::sib_r13_disp32: case Param::sib_r14_disp32: case Param::sib_r15_disp32:
			case Param::sib_rax_2_disp32: case Param::sib_rcx_2_disp32: case Param::sib_rdx_2_disp32: case Param::sib_rbx_2_disp32: case Param::sib_rbp_2_disp32: case Param::sib_rsi_2_disp32: case Param::sib_rdi_2_disp32: case Param::sib_r8_2_disp32: case Param::sib_r9_2_disp32: case Param::sib_r10_2_disp32: case Param::sib_r11_2_disp32: case Param::sib_r12_2_disp32: case Param::sib_r13_2_disp32: case Param::sib_r14_2_disp32: case Param::sib_r15_2_disp32:
			case Param::sib_rax_4_disp32: case Param::sib_rcx_4_disp32: case Param::sib_rdx_4_disp32: case Param::sib_rbx_4_disp32: case Param::sib_rbp_4_disp32: case Param::sib_rsi_4_disp32: case Param::sib_rdi_4_disp32: case Param::sib_r8_4_disp32: case Param::sib_r9_4_disp32: case Param::sib_r10_4_disp32: case Param::sib_r11_4_disp32: case Param::sib_r12_4_disp32: case Param::sib_r13_4_disp32: case Param::sib_r14_4_disp32: case Param::sib_r15_4_disp32:
			case Param::sib_rax_8_disp32: case Param::sib_rcx_8_disp32: case Param::sib_rdx_8_disp32: case Param::sib_rbx_8_disp32: case Param::sib_rbp_8_disp32: case Param::sib_rsi_8_disp32: case Param::sib_rdi_8_disp32: case Param::sib_r8_8_disp32: case Param::sib_r9_8_disp32: case Param::sib_r10_8_disp32: case Param::sib_r11_8_disp32: case Param::sib_r12_8_disp32: case Param::sib_r13_8_disp32: case Param::sib_r14_8_disp32: case Param::sib_r15_8_disp32:
			case Param::sib_rax_rax_disp32: case Param::sib_rax_rcx_disp32: case Param::sib_rax_rdx_disp32: case Param::sib_rax_rbx_disp32: case Param::sib_rax_rsp_disp32: case Param::sib_rax_rbp_disp32: case Param::sib_rax_rsi_disp32: case Param::sib_rax_rdi_disp32: case Param::sib_rax_r8_disp32: case Param::sib_rax_r9_disp32: case Param::sib_rax_r10_disp32: case Param::sib_rax_r11_disp32: case Param::sib_rax_r12_disp32: case Param::sib_rax_r13_disp32: case Param::sib_rax_r14_disp32: case Param::sib_rax_r15_disp32:
			case Param::sib_rcx_rax_disp32: case Param::sib_rcx_rcx_disp32: case Param::sib_rcx_rdx_disp32: case Param::sib_rcx_rbx_disp32: case Param::sib_rcx_rsp_disp32: case Param::sib_rcx_rbp_disp32: case Param::sib_rcx_rsi_disp32: case Param::sib_rcx_rdi_disp32: case Param::sib_rcx_r8_disp32: case Param::sib_rcx_r9_disp32: case Param::sib_rcx_r10_disp32: case Param::sib_rcx_r11_disp32: case Param::sib_rcx_r12_disp32: case Param::sib_rcx_r13_disp32: case Param::sib_rcx_r14_disp32: case Param::sib_rcx_r15_disp32:
			case Param::sib_rdx_rax_disp32: case Param::sib_rdx_rcx_disp32: case Param::sib_rdx_rdx_disp32: case Param::sib_rdx_rbx_disp32: case Param::sib_rdx_rsp_disp32: case Param::sib_rdx_rbp_disp32: case Param::sib_rdx_rsi_disp32: case Param::sib_rdx_rdi_disp32: case Param::sib_rdx_r8_disp32: case Param::sib_rdx_r9_disp32: case Param::sib_rdx_r10_disp32: case Param::sib_rdx_r11_disp32: case Param::sib_rdx_r12_disp32: case Param::sib_rdx_r13_disp32: case Param::sib_rdx_r14_disp32: case Param::sib_rdx_r15_disp32:
			case Param::sib_rbx_rax_disp32: case Param::sib_rbx_rcx_disp32: case Param::sib_rbx_rdx_disp32: case Param::sib_rbx_rbx_disp32: case Param::sib_rbx_rsp_disp32: case Param::sib_rbx_rbp_disp32: case Param::sib_rbx_rsi_disp32: case Param::sib_rbx_rdi_disp32: case Param::sib_rbx_r8_disp32: case Param::sib_rbx_r9_disp32: case Param::sib_rbx_r10_disp32: case Param::sib_rbx_r11_disp32: case Param::sib_rbx_r12_disp32: case Param::sib_rbx_r13_disp32: case Param::sib_rbx_r14_disp32: case Param::sib_rbx_r15_disp32:
			case Param::sib_rbp_rax_disp32: case Param::sib_rbp_rcx_disp32: case Param::sib_rbp_rdx_disp32: case Param::sib_rbp_rbx_disp32: case Param::sib_rbp_rsp_disp32: case Param::sib_rbp_rbp_disp32: case Param::sib_rbp_rsi_disp32: case Param::sib_rbp_rdi_disp32: case Param::sib_rbp_r8_disp32: case Param::sib_rbp_r9_disp32: case Param::sib_rbp_r10_disp32: case Param::sib_rbp_r11_disp32: case Param::sib_rbp_r12_disp32: case Param::sib_rbp_r13_disp32: case Param::sib_rbp_r14_disp32: case Param::sib_rbp_r15_disp32:
			case Param::sib_rsi_rax_disp32: case Param::sib_rsi_rcx_disp32: case Param::sib_rsi_rdx_disp32: case Param::sib_rsi_rbx_disp32: case Param::sib_rsi_rsp_disp32: case Param::sib_rsi_rbp_disp32: case Param::sib_rsi_rsi_disp32: case Param::sib_rsi_rdi_disp32: case Param::sib_rsi_r8_disp32: case Param::sib_rsi_r9_disp32: case Param::sib_rsi_r10_disp32: case Param::sib_rsi_r11_disp32: case Param::sib_rsi_r12_disp32: case Param::sib_rsi_r13_disp32: case Param::sib_rsi_r14_disp32: case Param::sib_rsi_r15_disp32:
			case Param::sib_rdi_rax_disp32: case Param::sib_rdi_rcx_disp32: case Param::sib_rdi_rdx_disp32: case Param::sib_rdi_rbx_disp32: case Param::sib_rdi_rsp_disp32: case Param::sib_rdi_rbp_disp32: case Param::sib_rdi_rsi_disp32: case Param::sib_rdi_rdi_disp32: case Param::sib_rdi_r8_disp32: case Param::sib_rdi_r9_disp32: case Param::sib_rdi_r10_disp32: case Param::sib_rdi_r11_disp32: case Param::sib_rdi_r12_disp32: case Param::sib_rdi_r13_disp32: case Param::sib_rdi_r14_disp32: case Param::sib_rdi_r15_disp32:
			case Param::sib_r8_rax_disp32: case Param::sib_r8_rcx_disp32: case Param::sib_r8_rdx_disp32: case Param::sib_r8_rbx_disp32: case Param::sib_r8_rsp_disp32: case Param::sib_r8_rbp_disp32: case Param::sib_r8_rsi_disp32: case Param::sib_r8_rdi_disp32: case Param::sib_r8_r8_disp32: case Param::sib_r8_r9_disp32: case Param::sib_r8_r10_disp32: case Param::sib_r8_r11_disp32: case Param::sib_r8_r12_disp32: case Param::sib_r8_r13_disp32: case Param::sib_r8_r14_disp32: case Param::sib_r8_r15_disp32:
			case Param::sib_r9_rax_disp32: case Param::sib_r9_rcx_disp32: case Param::sib_r9_rdx_disp32: case Param::sib_r9_rbx_disp32: case Param::sib_r9_rsp_disp32: case Param::sib_r9_rbp_disp32: case Param::sib_r9_rsi_disp32: case Param::sib_r9_rdi_disp32: case Param::sib_r9_r8_disp32: case Param::sib_r9_r9_disp32: case Param::sib_r9_r10_disp32: case Param::sib_r9_r11_disp32: case Param::sib_r9_r12_disp32: case Param::sib_r9_r13_disp32: case Param::sib_r9_r14_disp32: case Param::sib_r9_r15_disp32:
			case Param::sib_r10_rax_disp32: case Param::sib_r10_rcx_disp32: case Param::sib_r10_rdx_disp32: case Param::sib_r10_rbx_disp32: case Param::sib_r10_rsp_disp32: case Param::sib_r10_rbp_disp32: case Param::sib_r10_rsi_disp32: case Param::sib_r10_rdi_disp32: case Param::sib_r10_r8_disp32: case Param::sib_r10_r9_disp32: case Param::sib_r10_r10_disp32: case Param::sib_r10_r11_disp32: case Param::sib_r10_r12_disp32: case Param::sib_r10_r13_disp32: case Param::sib_r10_r14_disp32: case Param::sib_r10_r15_disp32:
			case Param::sib_r11_rax_disp32: case Param::sib_r11_rcx_disp32: case Param::sib_r11_rdx_disp32: case Param::sib_r11_rbx_disp32: case Param::sib_r11_rsp_disp32: case Param::sib_r11_rbp_disp32: case Param::sib_r11_rsi_disp32: case Param::sib_r11_rdi_disp32: case Param::sib_r11_r8_disp32: case Param::sib_r11_r9_disp32: case Param::sib_r11_r10_disp32: case Param::sib_r11_r11_disp32: case Param::sib_r11_r12_disp32: case Param::sib_r11_r13_disp32: case Param::sib_r11_r14_disp32: case Param::sib_r11_r15_disp32:
			case Param::sib_r12_rax_disp32: case Param::sib_r12_rcx_disp32: case Param::sib_r12_rdx_disp32: case Param::sib_r12_rbx_disp32: case Param::sib_r12_rsp_disp32: case Param::sib_r12_rbp_disp32: case Param::sib_r12_rsi_disp32: case Param::sib_r12_rdi_disp32: case Param::sib_r12_r8_disp32: case Param::sib_r12_r9_disp32: case Param::sib_r12_r10_disp32: case Param::sib_r12_r11_disp32: case Param::sib_r12_r12_disp32: case Param::sib_r12_r13_disp32: case Param::sib_r12_r14_disp32: case Param::sib_r12_r15_disp32:
			case Param::sib_r13_rax_disp32: case Param::sib_r13_rcx_disp32: case Param::sib_r13_rdx_disp32: case Param::sib_r13_rbx_disp32: case Param::sib_r13_rsp_disp32: case Param::sib_r13_rbp_disp32: case Param::sib_r13_rsi_disp32: case Param::sib_r13_rdi_disp32: case Param::sib_r13_r8_disp32: case Param::sib_r13_r9_disp32: case Param::sib_r13_r10_disp32: case Param::sib_r13_r11_disp32: case Param::sib_r13_r12_disp32: case Param::sib_r13_r13_disp32: case Param::sib_r13_r14_disp32: case Param::sib_r13_r15_disp32:
			case Param::sib_r14_rax_disp32: case Param::sib_r14_rcx_disp32: case Param::sib_r14_rdx_disp32: case Param::sib_r14_rbx_disp32: case Param::sib_r14_rsp_disp32: case Param::sib_r14_rbp_disp32: case Param::sib_r14_rsi_disp32: case Param::sib_r14_rdi_disp32: case Param::sib_r14_r8_disp32: case Param::sib_r14_r9_disp32: case Param::sib_r14_r10_disp32: case Param::sib_r14_r11_disp32: case Param::sib_r14_r12_disp32: case Param::sib_r14_r13_disp32: case Param::sib_r14_r14_disp32: case Param::sib_r14_r15_disp32:
			case Param::sib_r15_rax_disp32: case Param::sib_r15_rcx_disp32: case Param::sib_r15_rdx_disp32: case Param::sib_r15_rbx_disp32: case Param::sib_r15_rsp_disp32: case Param::sib_r15_rbp_disp32: case Param::sib_r15_rsi_disp32: case Param::sib_r15_rdi_disp32: case Param::sib_r15_r8_disp32: case Param::sib_r15_r9_disp32: case Param::sib_r15_r10_disp32: case Param::sib_r15_r11_disp32: case Param::sib_r15_r12_disp32: case Param::sib_r15_r13_disp32: case Param::sib_r15_r14_disp32: case Param::sib_r15_r15_disp32:
			case Param::sib_rax_2_rax_disp32: case Param::sib_rax_2_rcx_disp32: case Param::sib_rax_2_rdx_disp32: case Param::sib_rax_2_rbx_disp32: case Param::sib_rax_2_rsp_disp32: case Param::sib_rax_2_rbp_disp32: case Param::sib_rax_2_rsi_disp32: case Param::sib_rax_2_rdi_disp32: case Param::sib_rax_2_r8_disp32: case Param::sib_rax_2_r9_disp32: case Param::sib_rax_2_r10_disp32: case Param::sib_rax_2_r11_disp32: case Param::sib_rax_2_r12_disp32: case Param::sib_rax_2_r13_disp32: case Param::sib_rax_2_r14_disp32: case Param::sib_rax_2_r15_disp32:
			case Param::sib_rcx_2_rax_disp32: case Param::sib_rcx_2_rcx_disp32: case Param::sib_rcx_2_rdx_disp32: case Param::sib_rcx_2_rbx_disp32: case Param::sib_rcx_2_rsp_disp32: case Param::sib_rcx_2_rbp_disp32: case Param::sib_rcx_2_rsi_disp32: case Param::sib_rcx_2_rdi_disp32: case Param::sib_rcx_2_r8_disp32: case Param::sib_rcx_2_r9_disp32: case Param::sib_rcx_2_r10_disp32: case Param::sib_rcx_2_r11_disp32: case Param::sib_rcx_2_r12_disp32: case Param::sib_rcx_2_r13_disp32: case Param::sib_rcx_2_r14_disp32: case Param::sib_rcx_2_r15_disp32:
			case Param::sib_rdx_2_rax_disp32: case Param::sib_rdx_2_rcx_disp32: case Param::sib_rdx_2_rdx_disp32: case Param::sib_rdx_2_rbx_disp32: case Param::sib_rdx_2_rsp_disp32: case Param::sib_rdx_2_rbp_disp32: case Param::sib_rdx_2_rsi_disp32: case Param::sib_rdx_2_rdi_disp32: case Param::sib_rdx_2_r8_disp32: case Param::sib_rdx_2_r9_disp32: case Param::sib_rdx_2_r10_disp32: case Param::sib_rdx_2_r11_disp32: case Param::sib_rdx_2_r12_disp32: case Param::sib_rdx_2_r13_disp32: case Param::sib_rdx_2_r14_disp32: case Param::sib_rdx_2_r15_disp32:
			case Param::sib_rbx_2_rax_disp32: case Param::sib_rbx_2_rcx_disp32: case Param::sib_rbx_2_rdx_disp32: case Param::sib_rbx_2_rbx_disp32: case Param::sib_rbx_2_rsp_disp32: case Param::sib_rbx_2_rbp_disp32: case Param::sib_rbx_2_rsi_disp32: case Param::sib_rbx_2_rdi_disp32: case Param::sib_rbx_2_r8_disp32: case Param::sib_rbx_2_r9_disp32: case Param::sib_rbx_2_r10_disp32: case Param::sib_rbx_2_r11_disp32: case Param::sib_rbx_2_r12_disp32: case Param::sib_rbx_2_r13_disp32: case Param::sib_rbx_2_r14_disp32: case Param::sib_rbx_2_r15_disp32:
			case Param::sib_rbp_2_rax_disp32: case Param::sib_rbp_2_rcx_disp32: case Param::sib_rbp_2_rdx_disp32: case Param::sib_rbp_2_rbx_disp32: case Param::sib_rbp_2_rsp_disp32: case Param::sib_rbp_2_rbp_disp32: case Param::sib_rbp_2_rsi_disp32: case Param::sib_rbp_2_rdi_disp32: case Param::sib_rbp_2_r8_disp32: case Param::sib_rbp_2_r9_disp32: case Param::sib_rbp_2_r10_disp32: case Param::sib_rbp_2_r11_disp32: case Param::sib_rbp_2_r12_disp32: case Param::sib_rbp_2_r13_disp32: case Param::sib_rbp_2_r14_disp32: case Param::sib_rbp_2_r15_disp32:
			case Param::sib_rsi_2_rax_disp32: case Param::sib_rsi_2_rcx_disp32: case Param::sib_rsi_2_rdx_disp32: case Param::sib_rsi_2_rbx_disp32: case Param::sib_rsi_2_rsp_disp32: case Param::sib_rsi_2_rbp_disp32: case Param::sib_rsi_2_rsi_disp32: case Param::sib_rsi_2_rdi_disp32: case Param::sib_rsi_2_r8_disp32: case Param::sib_rsi_2_r9_disp32: case Param::sib_rsi_2_r10_disp32: case Param::sib_rsi_2_r11_disp32: case Param::sib_rsi_2_r12_disp32: case Param::sib_rsi_2_r13_disp32: case Param::sib_rsi_2_r14_disp32: case Param::sib_rsi_2_r15_disp32:
			case Param::sib_rdi_2_rax_disp32: case Param::sib_rdi_2_rcx_disp32: case Param::sib_rdi_2_rdx_disp32: case Param::sib_rdi_2_rbx_disp32: case Param::sib_rdi_2_rsp_disp32: case Param::sib_rdi_2_rbp_disp32: case Param::sib_rdi_2_rsi_disp32: case Param::sib_rdi_2_rdi_disp32: case Param::sib_rdi_2_r8_disp32: case Param::sib_rdi_2_r9_disp32: case Param::sib_rdi_2_r10_disp32: case Param::sib_rdi_2_r11_disp32: case Param::sib_rdi_2_r12_disp32: case Param::sib_rdi_2_r13_disp32: case Param::sib_rdi_2_r14_disp32: case Param::sib_rdi_2_r15_disp32:
			case Param::sib_r8_2_rax_disp32: case Param::sib_r8_2_rcx_disp32: case Param::sib_r8_2_rdx_disp32: case Param::sib_r8_2_rbx_disp32: case Param::sib_r8_2_rsp_disp32: case Param::sib_r8_2_rbp_disp32: case Param::sib_r8_2_rsi_disp32:  case Param::sib_r8_2_rdi_disp32:  case Param::sib_r8_2_r8_disp32:  case Param::sib_r8_2_r9_disp32:  case Param::sib_r8_2_r10_disp32:  case Param::sib_r8_2_r11_disp32:  case Param::sib_r8_2_r12_disp32:  case Param::sib_r8_2_r13_disp32:  case Param::sib_r8_2_r14_disp32:  case Param::sib_r8_2_r15_disp32:
			case Param::sib_r9_2_rax_disp32: case Param::sib_r9_2_rcx_disp32: case Param::sib_r9_2_rdx_disp32: case Param::sib_r9_2_rbx_disp32: case Param::sib_r9_2_rsp_disp32: case Param::sib_r9_2_rbp_disp32: case Param::sib_r9_2_rsi_disp32:  case Param::sib_r9_2_rdi_disp32:  case Param::sib_r9_2_r8_disp32:  case Param::sib_r9_2_r9_disp32:  case Param::sib_r9_2_r10_disp32:  case Param::sib_r9_2_r11_disp32:  case Param::sib_r9_2_r12_disp32:  case Param::sib_r9_2_r13_disp32:  case Param::sib_r9_2_r14_disp32:  case Param::sib_r9_2_r15_disp32:
			case Param::sib_r10_2_rax_disp32: case Param::sib_r10_2_rcx_disp32: case Param::sib_r10_2_rdx_disp32: case Param::sib_r10_2_rbx_disp32: case Param::sib_r10_2_rsp_disp32: case Param::sib_r10_2_rbp_disp32: case Param::sib_r10_2_rsi_disp32: case Param::sib_r10_2_rdi_disp32: case Param::sib_r10_2_r8_disp32: case Param::sib_r10_2_r9_disp32: case Param::sib_r10_2_r10_disp32: case Param::sib_r10_2_r11_disp32: case Param::sib_r10_2_r12_disp32: case Param::sib_r10_2_r13_disp32: case Param::sib_r10_2_r14_disp32: case Param::sib_r10_2_r15_disp32:
			case Param::sib_r11_2_rax_disp32: case Param::sib_r11_2_rcx_disp32: case Param::sib_r11_2_rdx_disp32: case Param::sib_r11_2_rbx_disp32: case Param::sib_r11_2_rsp_disp32: case Param::sib_r11_2_rbp_disp32: case Param::sib_r11_2_rsi_disp32: case Param::sib_r11_2_rdi_disp32: case Param::sib_r11_2_r8_disp32: case Param::sib_r11_2_r9_disp32: case Param::sib_r11_2_r10_disp32: case Param::sib_r11_2_r11_disp32: case Param::sib_r11_2_r12_disp32: case Param::sib_r11_2_r13_disp32: case Param::sib_r11_2_r14_disp32: case Param::sib_r11_2_r15_disp32:
			case Param::sib_r12_2_rax_disp32: case Param::sib_r12_2_rcx_disp32: case Param::sib_r12_2_rdx_disp32: case Param::sib_r12_2_rbx_disp32: case Param::sib_r12_2_rsp_disp32: case Param::sib_r12_2_rbp_disp32: case Param::sib_r12_2_rsi_disp32: case Param::sib_r12_2_rdi_disp32: case Param::sib_r12_2_r8_disp32: case Param::sib_r12_2_r9_disp32: case Param::sib_r12_2_r10_disp32: case Param::sib_r12_2_r11_disp32: case Param::sib_r12_2_r12_disp32: case Param::sib_r12_2_r13_disp32: case Param::sib_r12_2_r14_disp32: case Param::sib_r12_2_r15_disp32:
			case Param::sib_r13_2_rax_disp32: case Param::sib_r13_2_rcx_disp32: case Param::sib_r13_2_rdx_disp32: case Param::sib_r13_2_rbx_disp32: case Param::sib_r13_2_rsp_disp32: case Param::sib_r13_2_rbp_disp32: case Param::sib_r13_2_rsi_disp32: case Param::sib_r13_2_rdi_disp32: case Param::sib_r13_2_r8_disp32: case Param::sib_r13_2_r9_disp32: case Param::sib_r13_2_r10_disp32: case Param::sib_r13_2_r11_disp32: case Param::sib_r13_2_r12_disp32: case Param::sib_r13_2_r13_disp32: case Param::sib_r13_2_r14_disp32: case Param::sib_r13_2_r15_disp32:
			case Param::sib_r14_2_rax_disp32: case Param::sib_r14_2_rcx_disp32: case Param::sib_r14_2_rdx_disp32: case Param::sib_r14_2_rbx_disp32: case Param::sib_r14_2_rsp_disp32: case Param::sib_r14_2_rbp_disp32: case Param::sib_r14_2_rsi_disp32: case Param::sib_r14_2_rdi_disp32: case Param::sib_r14_2_r8_disp32: case Param::sib_r14_2_r9_disp32: case Param::sib_r14_2_r10_disp32: case Param::sib_r14_2_r11_disp32: case Param::sib_r14_2_r12_disp32: case Param::sib_r14_2_r13_disp32: case Param::sib_r14_2_r14_disp32: case Param::sib_r14_2_r15_disp32:
			case Param::sib_r15_2_rax_disp32: case Param::sib_r15_2_rcx_disp32: case Param::sib_r15_2_rdx_disp32: case Param::sib_r15_2_rbx_disp32: case Param::sib_r15_2_rsp_disp32: case Param::sib_r15_2_rbp_disp32: case Param::sib_r15_2_rsi_disp32: case Param::sib_r15_2_rdi_disp32: case Param::sib_r15_2_r8_disp32: case Param::sib_r15_2_r9_disp32: case Param::sib_r15_2_r10_disp32: case Param::sib_r15_2_r11_disp32: case Param::sib_r15_2_r12_disp32: case Param::sib_r15_2_r13_disp32: case Param::sib_r15_2_r14_disp32: case Param::sib_r15_2_r15_disp32:
			case Param::sib_rax_4_rax_disp32: case Param::sib_rax_4_rcx_disp32: case Param::sib_rax_4_rdx_disp32: case Param::sib_rax_4_rbx_disp32: case Param::sib_rax_4_rsp_disp32: case Param::sib_rax_4_rbp_disp32: case Param::sib_rax_4_rsi_disp32: case Param::sib_rax_4_rdi_disp32: case Param::sib_rax_4_r8_disp32: case Param::sib_rax_4_r9_disp32: case Param::sib_rax_4_r10_disp32: case Param::sib_rax_4_r11_disp32: case Param::sib_rax_4_r12_disp32: case Param::sib_rax_4_r13_disp32: case Param::sib_rax_4_r14_disp32: case Param::sib_rax_4_r15_disp32:
			case Param::sib_rcx_4_rax_disp32: case Param::sib_rcx_4_rcx_disp32: case Param::sib_rcx_4_rdx_disp32: case Param::sib_rcx_4_rbx_disp32: case Param::sib_rcx_4_rsp_disp32: case Param::sib_rcx_4_rbp_disp32: case Param::sib_rcx_4_rsi_disp32: case Param::sib_rcx_4_rdi_disp32: case Param::sib_rcx_4_r8_disp32: case Param::sib_rcx_4_r9_disp32: case Param::sib_rcx_4_r10_disp32: case Param::sib_rcx_4_r11_disp32: case Param::sib_rcx_4_r12_disp32: case Param::sib_rcx_4_r13_disp32: case Param::sib_rcx_4_r14_disp32: case Param::sib_rcx_4_r15_disp32:
			case Param::sib_rdx_4_rax_disp32: case Param::sib_rdx_4_rcx_disp32: case Param::sib_rdx_4_rdx_disp32: case Param::sib_rdx_4_rbx_disp32: case Param::sib_rdx_4_rsp_disp32: case Param::sib_rdx_4_rbp_disp32: case Param::sib_rdx_4_rsi_disp32: case Param::sib_rdx_4_rdi_disp32: case Param::sib_rdx_4_r8_disp32: case Param::sib_rdx_4_r9_disp32: case Param::sib_rdx_4_r10_disp32: case Param::sib_rdx_4_r11_disp32: case Param::sib_rdx_4_r12_disp32: case Param::sib_rdx_4_r13_disp32: case Param::sib_rdx_4_r14_disp32: case Param::sib_rdx_4_r15_disp32:
			case Param::sib_rbx_4_rax_disp32: case Param::sib_rbx_4_rcx_disp32: case Param::sib_rbx_4_rdx_disp32: case Param::sib_rbx_4_rbx_disp32: case Param::sib_rbx_4_rsp_disp32: case Param::sib_rbx_4_rbp_disp32: case Param::sib_rbx_4_rsi_disp32: case Param::sib_rbx_4_rdi_disp32: case Param::sib_rbx_4_r8_disp32: case Param::sib_rbx_4_r9_disp32: case Param::sib_rbx_4_r10_disp32: case Param::sib_rbx_4_r11_disp32: case Param::sib_rbx_4_r12_disp32: case Param::sib_rbx_4_r13_disp32: case Param::sib_rbx_4_r14_disp32: case Param::sib_rbx_4_r15_disp32:
			case Param::sib_rbp_4_rax_disp32: case Param::sib_rbp_4_rcx_disp32: case Param::sib_rbp_4_rdx_disp32: case Param::sib_rbp_4_rbx_disp32: case Param::sib_rbp_4_rsp_disp32: case Param::sib_rbp_4_rbp_disp32: case Param::sib_rbp_4_rsi_disp32: case Param::sib_rbp_4_rdi_disp32: case Param::sib_rbp_4_r8_disp32: case Param::sib_rbp_4_r9_disp32: case Param::sib_rbp_4_r10_disp32: case Param::sib_rbp_4_r11_disp32: case Param::sib_rbp_4_r12_disp32: case Param::sib_rbp_4_r13_disp32: case Param::sib_rbp_4_r14_disp32: case Param::sib_rbp_4_r15_disp32:
			case Param::sib_rsi_4_rax_disp32: case Param::sib_rsi_4_rcx_disp32: case Param::sib_rsi_4_rdx_disp32: case Param::sib_rsi_4_rbx_disp32: case Param::sib_rsi_4_rsp_disp32: case Param::sib_rsi_4_rbp_disp32: case Param::sib_rsi_4_rsi_disp32: case Param::sib_rsi_4_rdi_disp32: case Param::sib_rsi_4_r8_disp32: case Param::sib_rsi_4_r9_disp32: case Param::sib_rsi_4_r10_disp32: case Param::sib_rsi_4_r11_disp32: case Param::sib_rsi_4_r12_disp32: case Param::sib_rsi_4_r13_disp32: case Param::sib_rsi_4_r14_disp32: case Param::sib_rsi_4_r15_disp32:
			case Param::sib_rdi_4_rax_disp32: case Param::sib_rdi_4_rcx_disp32: case Param::sib_rdi_4_rdx_disp32: case Param::sib_rdi_4_rbx_disp32: case Param::sib_rdi_4_rsp_disp32: case Param::sib_rdi_4_rbp_disp32: case Param::sib_rdi_4_rsi_disp32: case Param::sib_rdi_4_rdi_disp32: case Param::sib_rdi_4_r8_disp32: case Param::sib_rdi_4_r9_disp32: case Param::sib_rdi_4_r10_disp32: case Param::sib_rdi_4_r11_disp32: case Param::sib_rdi_4_r12_disp32: case Param::sib_rdi_4_r13_disp32: case Param::sib_rdi_4_r14_disp32: case Param::sib_rdi_4_r15_disp32:
			case Param::sib_r8_4_rax_disp32: case Param::sib_r8_4_rcx_disp32: case Param::sib_r8_4_rdx_disp32: case Param::sib_r8_4_rbx_disp32: case Param::sib_r8_4_rsp_disp32: case Param::sib_r8_4_rbp_disp32: case Param::sib_r8_4_rsi_disp32:  case Param::sib_r8_4_rdi_disp32:  case Param::sib_r8_4_r8_disp32:  case Param::sib_r8_4_r9_disp32:  case Param::sib_r8_4_r10_disp32:  case Param::sib_r8_4_r11_disp32:  case Param::sib_r8_4_r12_disp32:  case Param::sib_r8_4_r13_disp32:  case Param::sib_r8_4_r14_disp32:  case Param::sib_r8_4_r15_disp32:
			case Param::sib_r9_4_rax_disp32: case Param::sib_r9_4_rcx_disp32: case Param::sib_r9_4_rdx_disp32: case Param::sib_r9_4_rbx_disp32: case Param::sib_r9_4_rsp_disp32: case Param::sib_r9_4_rbp_disp32: case Param::sib_r9_4_rsi_disp32:  case Param::sib_r9_4_rdi_disp32:  case Param::sib_r9_4_r8_disp32:  case Param::sib_r9_4_r9_disp32:  case Param::sib_r9_4_r10_disp32:  case Param::sib_r9_4_r11_disp32:  case Param::sib_r9_4_r12_disp32:  case Param::sib_r9_4_r13_disp32:  case Param::sib_r9_4_r14_disp32:  case Param::sib_r9_4_r15_disp32:
			case Param::sib_r10_4_rax_disp32: case Param::sib_r10_4_rcx_disp32: case Param::sib_r10_4_rdx_disp32: case Param::sib_r10_4_rbx_disp32: case Param::sib_r10_4_rsp_disp32: case Param::sib_r10_4_rbp_disp32: case Param::sib_r10_4_rsi_disp32: case Param::sib_r10_4_rdi_disp32: case Param::sib_r10_4_r8_disp32: case Param::sib_r10_4_r9_disp32: case Param::sib_r10_4_r10_disp32: case Param::sib_r10_4_r11_disp32: case Param::sib_r10_4_r12_disp32: case Param::sib_r10_4_r13_disp32: case Param::sib_r10_4_r14_disp32: case Param::sib_r10_4_r15_disp32:
			case Param::sib_r11_4_rax_disp32: case Param::sib_r11_4_rcx_disp32: case Param::sib_r11_4_rdx_disp32: case Param::sib_r11_4_rbx_disp32: case Param::sib_r11_4_rsp_disp32: case Param::sib_r11_4_rbp_disp32: case Param::sib_r11_4_rsi_disp32: case Param::sib_r11_4_rdi_disp32: case Param::sib_r11_4_r8_disp32: case Param::sib_r11_4_r9_disp32: case Param::sib_r11_4_r10_disp32: case Param::sib_r11_4_r11_disp32: case Param::sib_r11_4_r12_disp32: case Param::sib_r11_4_r13_disp32: case Param::sib_r11_4_r14_disp32: case Param::sib_r11_4_r15_disp32:
			case Param::sib_r12_4_rax_disp32: case Param::sib_r12_4_rcx_disp32: case Param::sib_r12_4_rdx_disp32: case Param::sib_r12_4_rbx_disp32: case Param::sib_r12_4_rsp_disp32: case Param::sib_r12_4_rbp_disp32: case Param::sib_r12_4_rsi_disp32: case Param::sib_r12_4_rdi_disp32: case Param::sib_r12_4_r8_disp32: case Param::sib_r12_4_r9_disp32: case Param::sib_r12_4_r10_disp32: case Param::sib_r12_4_r11_disp32: case Param::sib_r12_4_r12_disp32: case Param::sib_r12_4_r13_disp32: case Param::sib_r12_4_r14_disp32: case Param::sib_r12_4_r15_disp32:
			case Param::sib_r13_4_rax_disp32: case Param::sib_r13_4_rcx_disp32: case Param::sib_r13_4_rdx_disp32: case Param::sib_r13_4_rbx_disp32: case Param::sib_r13_4_rsp_disp32: case Param::sib_r13_4_rbp_disp32: case Param::sib_r13_4_rsi_disp32: case Param::sib_r13_4_rdi_disp32: case Param::sib_r13_4_r8_disp32: case Param::sib_r13_4_r9_disp32: case Param::sib_r13_4_r10_disp32: case Param::sib_r13_4_r11_disp32: case Param::sib_r13_4_r12_disp32: case Param::sib_r13_4_r13_disp32: case Param::sib_r13_4_r14_disp32: case Param::sib_r13_4_r15_disp32:
			case Param::sib_r14_4_rax_disp32: case Param::sib_r14_4_rcx_disp32: case Param::sib_r14_4_rdx_disp32: case Param::sib_r14_4_rbx_disp32: case Param::sib_r14_4_rsp_disp32: case Param::sib_r14_4_rbp_disp32: case Param::sib_r14_4_rsi_disp32: case Param::sib_r14_4_rdi_disp32: case Param::sib_r14_4_r8_disp32: case Param::sib_r14_4_r9_disp32: case Param::sib_r14_4_r10_disp32: case Param::sib_r14_4_r11_disp32: case Param::sib_r14_4_r12_disp32: case Param::sib_r14_4_r13_disp32: case Param::sib_r14_4_r14_disp32: case Param::sib_r14_4_r15_disp32:
			case Param::sib_r15_4_rax_disp32: case Param::sib_r15_4_rcx_disp32: case Param::sib_r15_4_rdx_disp32: case Param::sib_r15_4_rbx_disp32: case Param::sib_r15_4_rsp_disp32: case Param::sib_r15_4_rbp_disp32: case Param::sib_r15_4_rsi_disp32: case Param::sib_r15_4_rdi_disp32: case Param::sib_r15_4_r8_disp32: case Param::sib_r15_4_r9_disp32: case Param::sib_r15_4_r10_disp32: case Param::sib_r15_4_r11_disp32: case Param::sib_r15_4_r12_disp32: case Param::sib_r15_4_r13_disp32: case Param::sib_r15_4_r14_disp32: case Param::sib_r15_4_r15_disp32:
			case Param::sib_rax_8_rax_disp32: case Param::sib_rax_8_rcx_disp32: case Param::sib_rax_8_rdx_disp32: case Param::sib_rax_8_rbx_disp32: case Param::sib_rax_8_rsp_disp32: case Param::sib_rax_8_rbp_disp32: case Param::sib_rax_8_rsi_disp32: case Param::sib_rax_8_rdi_disp32: case Param::sib_rax_8_r8_disp32: case Param::sib_rax_8_r9_disp32: case Param::sib_rax_8_r10_disp32: case Param::sib_rax_8_r11_disp32: case Param::sib_rax_8_r12_disp32: case Param::sib_rax_8_r13_disp32: case Param::sib_rax_8_r14_disp32: case Param::sib_rax_8_r15_disp32:
			case Param::sib_rcx_8_rax_disp32: case Param::sib_rcx_8_rcx_disp32: case Param::sib_rcx_8_rdx_disp32: case Param::sib_rcx_8_rbx_disp32: case Param::sib_rcx_8_rsp_disp32: case Param::sib_rcx_8_rbp_disp32: case Param::sib_rcx_8_rsi_disp32: case Param::sib_rcx_8_rdi_disp32: case Param::sib_rcx_8_r8_disp32: case Param::sib_rcx_8_r9_disp32: case Param::sib_rcx_8_r10_disp32: case Param::sib_rcx_8_r11_disp32: case Param::sib_rcx_8_r12_disp32: case Param::sib_rcx_8_r13_disp32: case Param::sib_rcx_8_r14_disp32: case Param::sib_rcx_8_r15_disp32:
			case Param::sib_rdx_8_rax_disp32: case Param::sib_rdx_8_rcx_disp32: case Param::sib_rdx_8_rdx_disp32: case Param::sib_rdx_8_rbx_disp32: case Param::sib_rdx_8_rsp_disp32: case Param::sib_rdx_8_rbp_disp32: case Param::sib_rdx_8_rsi_disp32: case Param::sib_rdx_8_rdi_disp32: case Param::sib_rdx_8_r8_disp32: case Param::sib_rdx_8_r9_disp32: case Param::sib_rdx_8_r10_disp32: case Param::sib_rdx_8_r11_disp32: case Param::sib_rdx_8_r12_disp32: case Param::sib_rdx_8_r13_disp32: case Param::sib_rdx_8_r14_disp32: case Param::sib_rdx_8_r15_disp32:
			case Param::sib_rbx_8_rax_disp32: case Param::sib_rbx_8_rcx_disp32: case Param::sib_rbx_8_rdx_disp32: case Param::sib_rbx_8_rbx_disp32: case Param::sib_rbx_8_rsp_disp32: case Param::sib_rbx_8_rbp_disp32: case Param::sib_rbx_8_rsi_disp32: case Param::sib_rbx_8_rdi_disp32: case Param::sib_rbx_8_r8_disp32: case Param::sib_rbx_8_r9_disp32: case Param::sib_rbx_8_r10_disp32: case Param::sib_rbx_8_r11_disp32: case Param::sib_rbx_8_r12_disp32: case Param::sib_rbx_8_r13_disp32: case Param::sib_rbx_8_r14_disp32: case Param::sib_rbx_8_r15_disp32:
			case Param::sib_rbp_8_rax_disp32: case Param::sib_rbp_8_rcx_disp32: case Param::sib_rbp_8_rdx_disp32: case Param::sib_rbp_8_rbx_disp32: case Param::sib_rbp_8_rsp_disp32: case Param::sib_rbp_8_rbp_disp32: case Param::sib_rbp_8_rsi_disp32: case Param::sib_rbp_8_rdi_disp32: case Param::sib_rbp_8_r8_disp32: case Param::sib_rbp_8_r9_disp32: case Param::sib_rbp_8_r10_disp32: case Param::sib_rbp_8_r11_disp32: case Param::sib_rbp_8_r12_disp32: case Param::sib_rbp_8_r13_disp32: case Param::sib_rbp_8_r14_disp32: case Param::sib_rbp_8_r15_disp32:
			case Param::sib_rsi_8_rax_disp32: case Param::sib_rsi_8_rcx_disp32: case Param::sib_rsi_8_rdx_disp32: case Param::sib_rsi_8_rbx_disp32: case Param::sib_rsi_8_rsp_disp32: case Param::sib_rsi_8_rbp_disp32: case Param::sib_rsi_8_rsi_disp32: case Param::sib_rsi_8_rdi_disp32: case Param::sib_rsi_8_r8_disp32: case Param::sib_rsi_8_r9_disp32: case Param::sib_rsi_8_r10_disp32: case Param::sib_rsi_8_r11_disp32: case Param::sib_rsi_8_r12_disp32: case Param::sib_rsi_8_r13_disp32: case Param::sib_rsi_8_r14_disp32: case Param::sib_rsi_8_r15_disp32:
			case Param::sib_rdi_8_rax_disp32: case Param::sib_rdi_8_rcx_disp32: case Param::sib_rdi_8_rdx_disp32: case Param::sib_rdi_8_rbx_disp32: case Param::sib_rdi_8_rsp_disp32: case Param::sib_rdi_8_rbp_disp32: case Param::sib_rdi_8_rsi_disp32: case Param::sib_rdi_8_rdi_disp32: case Param::sib_rdi_8_r8_disp32: case Param::sib_rdi_8_r9_disp32: case Param::sib_rdi_8_r10_disp32: case Param::sib_rdi_8_r11_disp32: case Param::sib_rdi_8_r12_disp32: case Param::sib_rdi_8_r13_disp32: case Param::sib_rdi_8_r14_disp32: case Param::sib_rdi_8_r15_disp32:
			case Param::sib_r8_8_rax_disp32: case Param::sib_r8_8_rcx_disp32: case Param::sib_r8_8_rdx_disp32: case Param::sib_r8_8_rbx_disp32: case Param::sib_r8_8_rsp_disp32: case Param::sib_r8_8_rbp_disp32: case Param::sib_r8_8_rsi_disp32:  case Param::sib_r8_8_rdi_disp32:  case Param::sib_r8_8_r8_disp32:  case Param::sib_r8_8_r9_disp32:  case Param::sib_r8_8_r10_disp32:  case Param::sib_r8_8_r11_disp32:  case Param::sib_r8_8_r12_disp32:  case Param::sib_r8_8_r13_disp32:  case Param::sib_r8_8_r14_disp32:  case Param::sib_r8_8_r15_disp32:
			case Param::sib_r9_8_rax_disp32: case Param::sib_r9_8_rcx_disp32: case Param::sib_r9_8_rdx_disp32: case Param::sib_r9_8_rbx_disp32: case Param::sib_r9_8_rsp_disp32: case Param::sib_r9_8_rbp_disp32: case Param::sib_r9_8_rsi_disp32:  case Param::sib_r9_8_rdi_disp32:  case Param::sib_r9_8_r8_disp32:  case Param::sib_r9_8_r9_disp32:  case Param::sib_r9_8_r10_disp32:  case Param::sib_r9_8_r11_disp32:  case Param::sib_r9_8_r12_disp32:  case Param::sib_r9_8_r13_disp32:  case Param::sib_r9_8_r14_disp32:  case Param::sib_r9_8_r15_disp32:
			case Param::sib_r10_8_rax_disp32: case Param::sib_r10_8_rcx_disp32: case Param::sib_r10_8_rdx_disp32: case Param::sib_r10_8_rbx_disp32: case Param::sib_r10_8_rsp_disp32: case Param::sib_r10_8_rbp_disp32: case Param::sib_r10_8_rsi_disp32: case Param::sib_r10_8_rdi_disp32: case Param::sib_r10_8_r8_disp32: case Param::sib_r10_8_r9_disp32: case Param::sib_r10_8_r10_disp32: case Param::sib_r10_8_r11_disp32: case Param::sib_r10_8_r12_disp32: case Param::sib_r10_8_r13_disp32: case Param::sib_r10_8_r14_disp32: case Param::sib_r10_8_r15_disp32:
			case Param::sib_r11_8_rax_disp32: case Param::sib_r11_8_rcx_disp32: case Param::sib_r11_8_rdx_disp32: case Param::sib_r11_8_rbx_disp32: case Param::sib_r11_8_rsp_disp32: case Param::sib_r11_8_rbp_disp32: case Param::sib_r11_8_rsi_disp32: case Param::sib_r11_8_rdi_disp32: case Param::sib_r11_8_r8_disp32: case Param::sib_r11_8_r9_disp32: case Param::sib_r11_8_r10_disp32: case Param::sib_r11_8_r11_disp32: case Param::sib_r11_8_r12_disp32: case Param::sib_r11_8_r13_disp32: case Param::sib_r11_8_r14_disp32: case Param::sib_r11_8_r15_disp32:
			case Param::sib_r12_8_rax_disp32: case Param::sib_r12_8_rcx_disp32: case Param::sib_r12_8_rdx_disp32: case Param::sib_r12_8_rbx_disp32: case Param::sib_r12_8_rsp_disp32: case Param::sib_r12_8_rbp_disp32: case Param::sib_r12_8_rsi_disp32: case Param::sib_r12_8_rdi_disp32: case Param::sib_r12_8_r8_disp32: case Param::sib_r12_8_r9_disp32: case Param::sib_r12_8_r10_disp32: case Param::sib_r12_8_r11_disp32: case Param::sib_r12_8_r12_disp32: case Param::sib_r12_8_r13_disp32: case Param::sib_r12_8_r14_disp32: case Param::sib_r12_8_r15_disp32:
			case Param::sib_r13_8_rax_disp32: case Param::sib_r13_8_rcx_disp32: case Param::sib_r13_8_rdx_disp32: case Param::sib_r13_8_rbx_disp32: case Param::sib_r13_8_rsp_disp32: case Param::sib_r13_8_rbp_disp32: case Param::sib_r13_8_rsi_disp32: case Param::sib_r13_8_rdi_disp32: case Param::sib_r13_8_r8_disp32: case Param::sib_r13_8_r9_disp32: case Param::sib_r13_8_r10_disp32: case Param::sib_r13_8_r11_disp32: case Param::sib_r13_8_r12_disp32: case Param::sib_r13_8_r13_disp32: case Param::sib_r13_8_r14_disp32: case Param::sib_r13_8_r15_disp32:
			case Param::sib_r14_8_rax_disp32: case Param::sib_r14_8_rcx_disp32: case Param::sib_r14_8_rdx_disp32: case Param::sib_r14_8_rbx_disp32: case Param::sib_r14_8_rsp_disp32: case Param::sib_r14_8_rbp_disp32: case Param::sib_r14_8_rsi_disp32: case Param::sib_r14_8_rdi_disp32: case Param::sib_r14_8_r8_disp32: case Param::sib_r14_8_r9_disp32: case Param::sib_r14_8_r10_disp32: case Param::sib_r14_8_r11_disp32: case Param::sib_r14_8_r12_disp32: case Param::sib_r14_8_r13_disp32: case Param::sib_r14_8_r14_disp32: case Param::sib_r14_8_r15_disp32:
			case Param::sib_r15_8_rax_disp32: case Param::sib_r15_8_rcx_disp32: case Param::sib_r15_8_rdx_disp32: case Param::sib_r15_8_rbx_disp32: case Param::sib_r15_8_rsp_disp32: case Param::sib_r15_8_rbp_disp32: case Param::sib_r15_8_rsi_disp32: case Param::sib_r15_8_rdi_disp32: case Param::sib_r15_8_r8_disp32: case Param::sib_r15_8_r9_disp32: case Param::sib_r15_8_r10_disp32: case Param::sib_r15_8_r11_disp32: case Param::sib_r15_8_r12_disp32: case Param::sib_r15_8_r13_disp32: case Param::sib_r15_8_r14_disp32: case Param::sib_r15_8_r15_disp32:
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
			case Param::al: case Param::ax: case Param::eax: reg = 0; break;
			case Param::cl: case Param::cx: case Param::ecx: reg = 1; break;
			case Param::dl: case Param::dx: case Param::edx: reg = 2; break;
			case Param::bl: case Param::bx: case Param::ebx: reg = 3; break;
			case Param::ah: case Param::sp: case Param::esp: reg = 4; break;
			case Param::ch: case Param::bp: case Param::ebp: reg = 5; break;
			case Param::dh: case Param::si: case Param::esi: reg = 6; break;
			case Param::bh: case Param::di: case Param::edi: reg = 7; break;
			default:
				throw "Invalid parameter";
		}
	}

	void IntelAssembler::GetMod(Param p, size_t& mod)
	{

	}

	void IntelAssembler::GetRm(Param p, size_t& rm)
	{

	}

	void IntelAssembler::GetSS(Param p, size_t& scale)
	{

	}

	void IntelAssembler::GetIndex(Param p, size_t& index)
	{

	}

	void IntelAssembler::GetBase(Param p, size_t& base)
	{

	}

	void IntelAssembler::ModRegRm(AnalyzeInfo& info)
	{
		size_t mod = 0, reg = 0, rm = 0;
		size_t rmParam = 0;

		if (IsImm(info.params[1]))
		{
			if (IsReg(info.params[0]) || IsMem(info.params[0]))
			{
				GetMod(info.params[0], mod);
				reg = 0;		// The `reg` field contains an additional opcode in this case. 
				GetRm(info.params[0], rm);
				rmParam = 0;
			}
			else
			{
				throw "Invalid parameter";
			}
		}
		else
		{
			if (IsReg(info.params[0]))
			{
				if (IsReg(info.params[1]) || IsMem(info.params[1]))
				{
					GetMod(info.params[1], mod);
					GetReg(info.params[0], reg);
					GetRm(info.params[1], rm);
					rmParam = 1;
				}
				else
				{
					throw "Invalid parameter";
				}
			}
			else if (IsMem(info.params[0]))
			{
				if (IsReg(info.params[1]))
				{
					GetMod(info.params[0], mod);
					GetReg(info.params[1], reg);
					GetRm(info.params[0], rm);
					rmParam = 0;
				}
				else
				{
					throw "Invalid parameter";
				}
			}
			else
			{
				throw "Invalid parameter";
			}
		}

		// Check that the r/m parameter is a parameter using the SIB mechanism. 

		if (IsSib(info.params[rmParam]))
		{
			size_t scale = 0, index = 0, base = 0;

			GetSS(info.params[rmParam], scale);
			GetIndex(info.params[rmParam], index);
			GetBase(info.params[rmParam], base);
		}

	}

#pragma endregion "Private"

#pragma region "Base methods"

	void IntelAssembler::Assemble16(AnalyzeInfo& info)
	{
		info.prefixSize = 0;
		info.instrSize = 0;

		AssemblePrefixes(info);

		switch (info.instr)
		{
			// Instructions using ModRM / with an immediate operand

			case Instruction::adc:
				if (info.numParams != 2)
				{
					throw "Invalid parameters";
				}

				if (IsImm(info.params[1]))
				{

				}
				else
				{

				}

				break;


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

		AssemblePrefixes(info);

		switch (info.instr)
		{

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

		AssemblePrefixes(info);

		switch (info.instr)
		{

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
