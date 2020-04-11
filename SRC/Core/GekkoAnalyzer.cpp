/*

PowerPC architecture has a fairly clean instruction format.
This module in the first half contains a switch-case for the Primary/Secondary instruction code, 
and in the second half there are parameter parsers.

In theory such switch-cases are deeply optimized by the x64 compiler, so there is no need for JumpTables.

The "fast" versions are used for a consumer who knows in advance the number of parameters (for example, an interpreter).
Thus, we speed up the decoding process.

*/

#include "pch.h"

namespace Gekko
{
	// Simple decoder
	#define DIS_RD      ((instr >> 21) & 0x1f)
	#define DIS_RS      DIS_RD
	#define DIS_RA      ((instr >> 16) & 0x1f)
	#define DIS_RB      ((instr >> 11) & 0x1f)
	#define DIS_RC      ((instr >>  6) & 0x1f)
	#define DIS_RE      ((instr >>  1) & 0x1f)
	#define DIS_MB      DIS_RC
	#define DIS_ME      DIS_RE
	#define	OEBit		0x400
	#define DIS_OE      (instr & 0x400)
	#define DIS_SIMM    ((int16_t)instr)
	#define DIS_UIMM    (instr & 0xffff)
	#define DIS_CRM     ((instr >> 12) & 0xff)
	#define AA          (instr & 2)
	#define LK          (instr & 1)
	#define AALK        (instr & 3)
	#define Rc          LK
	#define RcBit       1
	#define LKBit       1

	void Analyzer::OpMain(uint32_t instr, AnalyzeInfo* info)
	{
		switch (instr >> 26)
		{
			case 12: info->instr = Instruction::addic; DaSimm(instr, info); break;
			case 13: info->instr = Instruction::addic_d; DaSimm(instr, info); break;
			case 14: info->instr = Instruction::addi; DaSimm(instr, info); break;
			case 15: info->instr = Instruction::addis; DaSimm(instr, info); break;
			case 16:
				switch (instr & 3)
				{
					case 0: info->instr = Instruction::bc; break;
					case 1: info->instr = Instruction::bcl; break;
					case 2: info->instr = Instruction::bca; break;
					case 3: info->instr = Instruction::bcla; break;
				}
				BoBiTargetAddr(instr, info);
				break;
			case 18:
				switch (instr & 3)
				{
					case 0: info->instr = Instruction::b; break;
					case 1: info->instr = Instruction::bl; break;
					case 2: info->instr = Instruction::ba; break;
					case 3: info->instr = Instruction::bla; break;
				}
				TargetAddr(instr, info);
				break;
			case 28: info->instr = Instruction::andi_d; AsUimm(instr, info); break;
			case 29: info->instr = Instruction::andis_d; AsUimm(instr, info); break;
		}
	}

	void Analyzer::OpMainFast(uint32_t instr, AnalyzeInfo* info)
	{
		switch (instr >> 26)
		{
			case 12: info->instr = Instruction::addic; DaSimmFast(instr, info); break;
			case 13: info->instr = Instruction::addic_d; DaSimmFast(instr, info); break;
			case 14: info->instr = Instruction::addi; DaSimmFast(instr, info); break;
			case 15: info->instr = Instruction::addis; DaSimmFast(instr, info); break;
			case 16:
				switch (instr & 3)
				{
					case 0: info->instr = Instruction::bc; break;
					case 1: info->instr = Instruction::bcl; break;
					case 2: info->instr = Instruction::bca; break;
					case 3: info->instr = Instruction::bcla; break;
				}
				BoBiTargetAddrFast(instr, info);
				break;
			case 18:
				switch (instr & 3)
				{
					case 0: info->instr = Instruction::b; break;
					case 1: info->instr = Instruction::bl; break;
					case 2: info->instr = Instruction::ba; break;
					case 3: info->instr = Instruction::bla; break;
				}
				TargetAddrFast(instr, info);
				break;
			case 28: info->instr = Instruction::andi_d; AsUimmFast(instr, info); break;
			case 29: info->instr = Instruction::andis_d; AsUimmFast(instr, info); break;
		}
	}

	#pragma region "Primary 19"

	void Analyzer::Op19(uint32_t instr, AnalyzeInfo* info)
	{
		switch (instr & 0x7ff)
		{
			case 528 * 2: info->instr = Instruction::bcctr; BoBi(instr, info); break;
			case (528 * 2) | LKBit: info->instr = Instruction::bcctrl; BoBi(instr, info); break;

			case 16 * 2: info->instr = Instruction::bclr; BoBi(instr, info); break;
			case (16 * 2) | LKBit: info->instr = Instruction::bclrl; BoBi(instr, info); break;
		}
	}

	void Analyzer::Op19Fast(uint32_t instr, AnalyzeInfo* info)
	{
		switch (instr & 0x7ff)
		{
			case 528 * 2: info->instr = Instruction::bcctr; BoBiFast(instr, info); break;
			case (528 * 2) | LKBit: info->instr = Instruction::bcctrl; BoBiFast(instr, info); break;

			case 16 * 2: info->instr = Instruction::bclr; BoBiFast(instr, info); break;
			case (16 * 2) | LKBit: info->instr = Instruction::bclrl; BoBiFast(instr, info); break;
		}
	}

	#pragma endregion "Primary 19"

	#pragma region "Primary 31"

	void Analyzer::Op31(uint32_t instr, AnalyzeInfo* info)
	{
		switch (instr & 0x7ff)
		{
			case 266 * 2: info->instr = Instruction::add; Dab(instr, info); break;
			case (266 * 2) | RcBit: info->instr = Instruction::add_d; Dab(instr, info); break;
			case (266 * 2) | OEBit: info->instr = Instruction::addo; Dab(instr, info); break;
			case (266 * 2) | OEBit | RcBit: info->instr = Instruction::addo_d; Dab(instr, info); break;

			case 10 * 2: info->instr = Instruction::addc; Dab(instr, info); break;
			case (10 * 2) | RcBit: info->instr = Instruction::addc_d; Dab(instr, info); break;
			case (10 * 2) | OEBit: info->instr = Instruction::addco; Dab(instr, info); break;
			case (10 * 2) | OEBit | RcBit: info->instr = Instruction::addco_d; Dab(instr, info); break;

			case 138 * 2: info->instr = Instruction::adde; Dab(instr, info); break;
			case (138 * 2) | RcBit: info->instr = Instruction::adde_d; Dab(instr, info); break;
			case (138 * 2) | OEBit: info->instr = Instruction::addeo; Dab(instr, info); break;
			case (138 * 2) | OEBit | RcBit: info->instr = Instruction::addeo_d; Dab(instr, info); break;

			case 234 * 2: info->instr = Instruction::addme; Da(instr, info); break;
			case (234 * 2) | RcBit: info->instr = Instruction::addme_d; Da(instr, info); break;
			case (234 * 2) | OEBit: info->instr = Instruction::addmeo; Da(instr, info); break;
			case (234 * 2) | OEBit | RcBit: info->instr = Instruction::addmeo_d; Da(instr, info); break;

			case 202 * 2: info->instr = Instruction::addze; Da(instr, info); break;
			case (202 * 2) | RcBit: info->instr = Instruction::addze_d; Da(instr, info); break;
			case (202 * 2) | OEBit: info->instr = Instruction::addzeo; Da(instr, info); break;
			case (202 * 2) | OEBit | RcBit: info->instr = Instruction::addzeo_d; Da(instr, info); break;

			case 28 * 2: info->instr = Instruction::and; Asb(instr, info); break;
			case (28 * 2) | RcBit: info->instr = Instruction::and_d; Asb(instr, info); break;

			case 60 * 2: info->instr = Instruction::andc; Asb(instr, info); break;
			case (60 * 2) | RcBit: info->instr = Instruction::andc_d; Asb(instr, info); break;

		}

	}

	void Analyzer::Op31Fast(uint32_t instr, AnalyzeInfo* info)
	{
		switch (instr & 0x7ff)
		{
			case 266 * 2: info->instr = Instruction::add; DabFast(instr, info); break;
			case (266 * 2) | RcBit: info->instr = Instruction::add_d; DabFast(instr, info); break;
			case (266 * 2) | OEBit: info->instr = Instruction::addo; DabFast(instr, info); break;
			case (266 * 2) | OEBit | RcBit: info->instr = Instruction::addo_d; DabFast(instr, info); break;

			case 10 * 2: info->instr = Instruction::addc; DabFast(instr, info); break;
			case (10 * 2) | RcBit: info->instr = Instruction::addc_d; DabFast(instr, info); break;
			case (10 * 2) | OEBit: info->instr = Instruction::addco; DabFast(instr, info); break;
			case (10 * 2) | OEBit | RcBit: info->instr = Instruction::addco_d; DabFast(instr, info); break;

			case 138 * 2: info->instr = Instruction::adde; DabFast(instr, info); break;
			case (138 * 2) | RcBit: info->instr = Instruction::adde_d; DabFast(instr, info); break;
			case (138 * 2) | OEBit: info->instr = Instruction::addeo; DabFast(instr, info); break;
			case (138 * 2) | OEBit | RcBit: info->instr = Instruction::addeo_d; DabFast(instr, info); break;

			case 234 * 2: info->instr = Instruction::addme; DaFast(instr, info); break;
			case (234 * 2) | RcBit: info->instr = Instruction::addme_d; DaFast(instr, info); break;
			case (234 * 2) | OEBit: info->instr = Instruction::addmeo; DaFast(instr, info); break;
			case (234 * 2) | OEBit | RcBit: info->instr = Instruction::addmeo_d; DaFast(instr, info); break;

			case 202 * 2: info->instr = Instruction::addze; DaFast(instr, info); break;
			case (202 * 2) | RcBit: info->instr = Instruction::addze_d; DaFast(instr, info); break;
			case (202 * 2) | OEBit: info->instr = Instruction::addzeo; DaFast(instr, info); break;
			case (202 * 2) | OEBit | RcBit: info->instr = Instruction::addzeo_d; DaFast(instr, info); break;

			case 28 * 2: info->instr = Instruction::and; AsbFast(instr, info); break;
			case (28 * 2) | RcBit: info->instr = Instruction::and_d; AsbFast(instr, info); break;

			case 60 * 2: info->instr = Instruction::andc; AsbFast(instr, info); break;
			case (60 * 2) | RcBit: info->instr = Instruction::andc_d; AsbFast(instr, info); break;

		}

	}

	#pragma endregion "Primary 31"



	void Analyzer::Op59(uint32_t instr, AnalyzeInfo* info)
	{

	}

	void Analyzer::Op63(uint32_t instr, AnalyzeInfo* info)
	{

	}

	void Analyzer::Op4(uint32_t instr, AnalyzeInfo* info)
	{

	}

	#pragma region "Parameters"

	void Analyzer::Dab(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 3;
		info->param[0] = Param::Reg;
		info->param[1] = Param::Reg;
		info->param[2] = Param::Reg;
		DabFast(instr, info);
	}

	void Analyzer::DabFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD;
		info->paramBits[1] = DIS_RA;
		info->paramBits[2] = DIS_RB;
	}

	void Analyzer::DaSimm(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 3;
		info->param[0] = Param::Reg;
		info->param[1] = Param::Reg;
		info->param[2] = Param::Simm;
		DaSimmFast(instr, info);
	}

	void Analyzer::DaSimmFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD;
		info->paramBits[1] = DIS_RA;
		info->Imm.Signed = DIS_SIMM;
	}

	void Analyzer::Da(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 2;
		info->param[0] = Param::Reg;
		info->param[1] = Param::Reg;
		DaFast(instr, info);
	}

	void Analyzer::DaFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD;
		info->paramBits[1] = DIS_RA;
	}

	void Analyzer::Asb(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 3;
		info->param[0] = Param::Reg;
		info->param[1] = Param::Reg;
		info->param[2] = Param::Reg;
		AsbFast(instr, info);
	}

	void Analyzer::AsbFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RA;
		info->paramBits[1] = DIS_RS;
		info->paramBits[2] = DIS_RB;
	}

	void Analyzer::AsUimm(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 3;
		info->param[0] = Param::Reg;
		info->param[1] = Param::Reg;
		info->param[2] = Param::Uimm;
		AsUimmFast(instr, info);
	}

	void Analyzer::AsUimmFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RA;
		info->paramBits[1] = DIS_RS;
		info->Imm.Unsigned = DIS_UIMM;
	}

	void Analyzer::TargetAddr(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 1;
		info->param[0] = Param::Address;
		TargetAddrFast(instr, info);
	}

	void Analyzer::TargetAddrFast(uint32_t instr, AnalyzeInfo* info)
	{
		uint32_t target = instr & 0x03fffffc;
		if (target & 0x02000000) target |= 0xfc000000;
		if (instr & 2) info->Imm.Address = target; // AA
		else info->Imm.Address = info->pc + target;

		info->flow = true;
	}

	void Analyzer::BoBiTargetAddr(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 3;
		info->param[0] = Param::Num;
		info->param[1] = Param::Num;
		info->param[2] = Param::Address;
		BoBiTargetAddrFast(instr, info);
	}

	void Analyzer::BoBiTargetAddrFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD;
		info->paramBits[1] = DIS_RA;
		
		uint32_t target = instr & 0xfffc;
		if (target & 0x8000) target |= 0xffff0000;
		if (instr & 2) info->Imm.Address = target; // AA
		else info->Imm.Address = info->pc + target;

		info->flow = true;
	}

	void Analyzer::BoBi(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 2;
		info->param[0] = Param::Num;
		info->param[1] = Param::Num;
		BoBiFast(instr, info);
	}

	void Analyzer::BoBiFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD;
		info->paramBits[1] = DIS_RA;
		info->flow = true;
	}

	#pragma endregion "Parameters"

	void Analyzer::Analyze(uint32_t pc, uint32_t instr, AnalyzeInfo* info)
	{
		info->instr = Instruction::Unknown;
		info->instrBits = instr;
		info->pc = pc;
		info->flow = false;

		switch (instr >> 26)
		{
			case 19: Op19(instr, info); break;
			case 31: Op31(instr, info); break;
			default: OpMain(instr, info); break;
		}
	}

	void Analyzer::AnalyzeFast(uint32_t pc, uint32_t instr, AnalyzeInfo* info)
	{
		info->instr = Instruction::Unknown;
		info->pc = pc;
		info->flow = false;

		switch (instr >> 26)
		{
			case 19: Op19Fast(instr, info); break;
			case 31: Op31Fast(instr, info); break;
			default: OpMainFast(instr, info); break;
		}
	}

}
