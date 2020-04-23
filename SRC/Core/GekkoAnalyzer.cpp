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
	#define DIS_FM		((instr >> 17) & 0xff)
	#define AA          (instr & 2)
	#define LK          (instr & 1)
	#define AALK        (instr & 3)
	#define Rc          LK
	#define RcBit       1
	#define LKBit       1
	#define DIS_SPR     ((instr >> 11) & 0x3ff)
	#define DIS_TBR     ((instr >> 11) & 0x3ff)

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
			case 11:
				if ((DIS_RD & 1) == 0)
				{
					info->instr = Instruction::cmpi; CrfDaSimm(instr, info);
				}
				break;
			case 10:
				if ((DIS_RD & 1) == 0)
				{
					info->instr = Instruction::cmpli; CrfDaUimm(instr, info);
				}
				break;

			case 34: info->instr = Instruction::lbz; DaOffset(instr, info); break;
			case 35: info->instr = Instruction::lbzu; DaOffset(instr, info); break;
			case 50: info->instr = Instruction::lfd; FrdaOffset(instr, info); break;
			case 51: info->instr = Instruction::lfdu; FrdaOffset(instr, info); break;
			case 48: info->instr = Instruction::lfs; FrdaOffset(instr, info); break;
			case 49: info->instr = Instruction::lfsu; FrdaOffset(instr, info); break;
			case 42: info->instr = Instruction::lha; DaOffset(instr, info); break;
			case 43: info->instr = Instruction::lhau; DaOffset(instr, info); break;
			case 40: info->instr = Instruction::lhz; DaOffset(instr, info); break;
			case 41: info->instr = Instruction::lhzu; DaOffset(instr, info); break;
			case 46: info->instr = Instruction::lmw; DaOffset(instr, info); break;
			case 32: info->instr = Instruction::lwz; DaOffset(instr, info); break;
			case 33: info->instr = Instruction::lwzu; DaOffset(instr, info); break;

			case 7: info->instr = Instruction::mulli; DaSimm(instr, info); break;
			case 24: info->instr = Instruction::ori; AsUimm(instr, info); break;
			case 25: info->instr = Instruction::oris; AsUimm(instr, info); break;

			case 56: info->instr = Instruction::psq_l; FrRegOffsetWi(instr, info); break;
			case 57: info->instr = Instruction::psq_lu; FrRegOffsetWi(instr, info); break;
			case 60: info->instr = Instruction::psq_st; FrRegOffsetWi(instr, info); break;
			case 61: info->instr = Instruction::psq_stu; FrRegOffsetWi(instr, info); break;

			case 20:
				if (Rc) info->instr = Instruction::rlwimi;
				else  info->instr = Instruction::rlwimi_d;
				AsImm3(instr, info);
				break;
			case 21:
				if (Rc) info->instr = Instruction::rlwinm;
				else  info->instr = Instruction::rlwinm_d;
				AsImm3(instr, info);
				break;
			case 23:
				if (Rc) info->instr = Instruction::rlwnm;
				else  info->instr = Instruction::rlwnm_d;
				AsbImm2(instr, info);
				break;

			case 17:
				if (instr & 2)
				{
					info->instr = Instruction::sc;
					info->flow = true;
				}
				break;

			case 38: info->instr = Instruction::stb; DaOffset(instr, info); break;
			case 39: info->instr = Instruction::stbu; DaOffset(instr, info); break;
			case 54: info->instr = Instruction::stfd; FrdaOffset(instr, info); break;
			case 55: info->instr = Instruction::stfdu; FrdaOffset(instr, info); break;
			case 52: info->instr = Instruction::stfs; FrdaOffset(instr, info); break;
			case 53: info->instr = Instruction::stfsu; FrdaOffset(instr, info); break;
			case 44: info->instr = Instruction::sth; DaOffset(instr, info); break;
			case 45: info->instr = Instruction::sthu; DaOffset(instr, info); break;
			case 47: info->instr = Instruction::stmw; DaOffset(instr, info); break;
			case 36: info->instr = Instruction::stw; DaOffset(instr, info); break;
			case 37: info->instr = Instruction::stwu; DaOffset(instr, info); break;

			case 8: info->instr = Instruction::subfic; DaSimm(instr, info); break;

			case 3: info->instr = Instruction::twi; ImmASimm(instr, info); info->flow = true; break;

			case 26: info->instr = Instruction::xori; AsUimm(instr, info); break;
			case 27: info->instr = Instruction::xoris; AsUimm(instr, info); break;
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
			case 11:
				if ((DIS_RD & 1) == 0)
				{
					info->instr = Instruction::cmpi; CrfDaSimmFast(instr, info);
				}
				break;
			case 10:
				if ((DIS_RD & 1) == 0)
				{
					info->instr = Instruction::cmpli; CrfDaUimmFast(instr, info);
				}
				break;

			case 34: info->instr = Instruction::lbz; DaOffsetFast(instr, info); break;
			case 35: info->instr = Instruction::lbzu; DaOffsetFast(instr, info); break;
			case 50: info->instr = Instruction::lfd; FrdaOffsetFast(instr, info); break;
			case 51: info->instr = Instruction::lfdu; FrdaOffsetFast(instr, info); break;
			case 48: info->instr = Instruction::lfs; FrdaOffsetFast(instr, info); break;
			case 49: info->instr = Instruction::lfsu; FrdaOffsetFast(instr, info); break;
			case 42: info->instr = Instruction::lha; DaOffsetFast(instr, info); break;
			case 43: info->instr = Instruction::lhau; DaOffsetFast(instr, info); break;
			case 40: info->instr = Instruction::lhz; DaOffsetFast(instr, info); break;
			case 41: info->instr = Instruction::lhzu; DaOffsetFast(instr, info); break;
			case 46: info->instr = Instruction::lmw; DaOffsetFast(instr, info); break;
			case 32: info->instr = Instruction::lwz; DaOffsetFast(instr, info); break;
			case 33: info->instr = Instruction::lwzu; DaOffsetFast(instr, info); break;

			case 7: info->instr = Instruction::mulli; DaSimmFast(instr, info); break;
			case 24: info->instr = Instruction::ori; AsUimmFast(instr, info); break;
			case 25: info->instr = Instruction::oris; AsUimmFast(instr, info); break;

			case 56: info->instr = Instruction::psq_l; FrRegOffsetWiFast(instr, info); break;
			case 57: info->instr = Instruction::psq_lu; FrRegOffsetWiFast(instr, info); break;
			case 60: info->instr = Instruction::psq_st; FrRegOffsetWiFast(instr, info); break;
			case 61: info->instr = Instruction::psq_stu; FrRegOffsetWiFast(instr, info); break;

			case 20:
				if (Rc) info->instr = Instruction::rlwimi;
				else  info->instr = Instruction::rlwimi_d;
				AsImm3Fast(instr, info);
				break;
			case 21:
				if (Rc) info->instr = Instruction::rlwinm;
				else  info->instr = Instruction::rlwinm_d;
				AsImm3Fast(instr, info);
				break;
			case 23:
				if (Rc) info->instr = Instruction::rlwnm;
				else  info->instr = Instruction::rlwnm_d;
				AsbImm2Fast(instr, info);
				break;

			case 17:
				if (instr & 2)
				{
					info->instr = Instruction::sc;
					info->flow = true;
				}
				break;

			case 38: info->instr = Instruction::stb; DaOffsetFast(instr, info); break;
			case 39: info->instr = Instruction::stbu; DaOffsetFast(instr, info); break;
			case 54: info->instr = Instruction::stfd; FrdaOffsetFast(instr, info); break;
			case 55: info->instr = Instruction::stfdu; FrdaOffsetFast(instr, info); break;
			case 52: info->instr = Instruction::stfs; FrdaOffsetFast(instr, info); break;
			case 53: info->instr = Instruction::stfsu; FrdaOffsetFast(instr, info); break;
			case 44: info->instr = Instruction::sth; DaOffsetFast(instr, info); break;
			case 45: info->instr = Instruction::sthu; DaOffsetFast(instr, info); break;
			case 47: info->instr = Instruction::stmw; DaOffsetFast(instr, info); break;
			case 36: info->instr = Instruction::stw; DaOffsetFast(instr, info); break;
			case 37: info->instr = Instruction::stwu; DaOffsetFast(instr, info); break;

			case 8: info->instr = Instruction::subfic; DaSimmFast(instr, info); break;

			case 3: info->instr = Instruction::twi; ImmASimmFast(instr, info); info->flow = true; break;

			case 26: info->instr = Instruction::xori; AsUimmFast(instr, info); break;
			case 27: info->instr = Instruction::xoris; AsUimmFast(instr, info); break;
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

			case 257 * 2: info->instr = Instruction::crand; CrbDab(instr, info); break;
			case 129 * 2: info->instr = Instruction::crandc; CrbDab(instr, info); break;
			case 289 * 2: info->instr = Instruction::creqv; CrbDab(instr, info); break;
			case 225 * 2: info->instr = Instruction::crnand; CrbDab(instr, info); break;
			case 33 * 2: info->instr = Instruction::crnor; CrbDab(instr, info); break;
			case 449 * 2: info->instr = Instruction::cror; CrbDab(instr, info); break;
			case 417 * 2: info->instr = Instruction::crorc; CrbDab(instr, info); break;
			case 193 * 2: info->instr = Instruction::crxor; CrbDab(instr, info); break;

			case 150 * 2: info->instr = Instruction::isync; break;

			case 0: info->instr = Instruction::mcrf; Crfds(instr, info); break;

			case 50 * 2: info->instr = Instruction::rfi; info->flow = true; break;
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

			case 257 * 2: info->instr = Instruction::crand; CrbDabFast(instr, info); break;
			case 129 * 2: info->instr = Instruction::crandc; CrbDabFast(instr, info); break;
			case 289 * 2: info->instr = Instruction::creqv; CrbDabFast(instr, info); break;
			case 225 * 2: info->instr = Instruction::crnand; CrbDabFast(instr, info); break;
			case 33 * 2: info->instr = Instruction::crnor; CrbDabFast(instr, info); break;
			case 449 * 2: info->instr = Instruction::cror; CrbDabFast(instr, info); break;
			case 417 * 2: info->instr = Instruction::crorc; CrbDabFast(instr, info); break;
			case 193 * 2: info->instr = Instruction::crxor; CrbDabFast(instr, info); break;

			case 150 * 2: info->instr = Instruction::isync; break;

			case 0: info->instr = Instruction::mcrf; CrfdsFast(instr, info); break;

			case 50 * 2: info->instr = Instruction::rfi; info->flow = true; break;
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

			case 28 * 2: info->instr = Instruction::_and; Asb(instr, info); break;
			case (28 * 2) | RcBit: info->instr = Instruction::and_d; Asb(instr, info); break;

			case 60 * 2: info->instr = Instruction::andc; Asb(instr, info); break;
			case (60 * 2) | RcBit: info->instr = Instruction::andc_d; Asb(instr, info); break;

			case 0: 
				if ((DIS_RD & 1) == 0)
				{
					info->instr = Instruction::cmp; CrfDab(instr, info);
				}
				break;

			case 32*2:
				if ((DIS_RD & 1) == 0)
				{
					info->instr = Instruction::cmpl; CrfDab(instr, info);
				}
				break;

			case 26 * 2: info->instr = Instruction::cntlzw; As(instr, info); break;
			case (26 * 2) | RcBit: info->instr = Instruction::cntlzw_d; As(instr, info); break;

			case 86 * 2: info->instr = Instruction::dcbf; Ab(instr, info); break;
			case 470 * 2: info->instr = Instruction::dcbi; Ab(instr, info); break;
			case 54 * 2: info->instr = Instruction::dcbst; Ab(instr, info); break;
			case 278 * 2: info->instr = Instruction::dcbt; Ab(instr, info); break;
			case 246 * 2: info->instr = Instruction::dcbtst; Ab(instr, info); break;
			case 1014 * 2: info->instr = Instruction::dcbz; Ab(instr, info); break;

			case 491 * 2: info->instr = Instruction::divw; Dab(instr, info); break;
			case (491 * 2) | RcBit: info->instr = Instruction::divw_d; Dab(instr, info); break;
			case (491 * 2) | OEBit: info->instr = Instruction::divwo; Dab(instr, info); break;
			case (491 * 2) | OEBit | RcBit: info->instr = Instruction::divwo_d; Dab(instr, info); break;

			case 459 * 2: info->instr = Instruction::divwu; Dab(instr, info); break;
			case (459 * 2) | RcBit: info->instr = Instruction::divwu_d; Dab(instr, info); break;
			case (459 * 2) | OEBit: info->instr = Instruction::divwuo; Dab(instr, info); break;
			case (459 * 2) | OEBit | RcBit: info->instr = Instruction::divwuo_d; Dab(instr, info); break;

			case 310 * 2: info->instr = Instruction::eciwx; Dab(instr, info); break;
			case 438 * 2: info->instr = Instruction::ecowx; Dab(instr, info); break;

			case 854 * 2: info->instr = Instruction::eieio; break;

			case 284* 2: info->instr = Instruction::eqv; Asb(instr, info); break;
			case (284 * 2) | RcBit: info->instr = Instruction::eqv_d; Asb(instr, info); break;

			case 954 * 2: info->instr = Instruction::extsb; As(instr, info); break;
			case (954 * 2) | RcBit: info->instr = Instruction::extsb_d; As(instr, info); break;
			case 922 * 2: info->instr = Instruction::extsh; As(instr, info); break;
			case (922 * 2) | RcBit: info->instr = Instruction::extsh_d; As(instr, info); break;

			case 982 * 2: info->instr = Instruction::icbi; Ab(instr, info); break;

			case 119 * 2: info->instr = Instruction::lbzux; Dab(instr, info); break;
			case 87 * 2: info->instr = Instruction::lbzx; Dab(instr, info); break;
			case 631 * 2: info->instr = Instruction::lfdux; FrDRegAb(instr, info); break;
			case 599 * 2: info->instr = Instruction::lfdx; FrDRegAb(instr, info); break;
			case 567 * 2: info->instr = Instruction::lfsux; FrDRegAb(instr, info); break;
			case 535 * 2: info->instr = Instruction::lfsx; FrDRegAb(instr, info); break;
			case 375 * 2: info->instr = Instruction::lhaux; Dab(instr, info); break;
			case 343 * 2: info->instr = Instruction::lhax; Dab(instr, info); break;
			case 790 * 2: info->instr = Instruction::lhbrx; Dab(instr, info); break;
			case 311 * 2: info->instr = Instruction::lhzux; Dab(instr, info); break;
			case 279 * 2: info->instr = Instruction::lhzx; Dab(instr, info); break;
			case 597 * 2: info->instr = Instruction::lswi; DaNb(instr, info); break;
			case 533 * 2: info->instr = Instruction::lswx; Dab(instr, info); break;
			case 20 * 2: info->instr = Instruction::lwarx; Dab(instr, info); break;
			case 534 * 2: info->instr = Instruction::lwbrx; Dab(instr, info); break;
			case 55 * 2: info->instr = Instruction::lwzux; Dab(instr, info); break;
			case 23 * 2: info->instr = Instruction::lwzx; Dab(instr, info); break;

			case 512 * 2: info->instr = Instruction::mcrxr; Crfd(instr, info); break;
			case 19 * 2: info->instr = Instruction::mfcr; D(instr, info); break;
			case 83 * 2: info->instr = Instruction::mfmsr; D(instr, info); break;
			case 339 * 2: info->instr = Instruction::mfspr; DSpr(instr, info); break;
			case 595 * 2: info->instr = Instruction::mfsr; DSr(instr, info); break;
			case 659 * 2: info->instr = Instruction::mfsrin; Db(instr, info); break;
			case 371 * 2: info->instr = Instruction::mftb; DTbr(instr, info); break;
			case 144 * 2: info->instr = Instruction::mtcrf; Crms(instr, info); break;
			case 146 * 2: info->instr = Instruction::mtmsr; D(instr, info); break;
			case 467 * 2: info->instr = Instruction::mtspr; SprS(instr, info); break;
			case 210 * 2: info->instr = Instruction::mtsr; SrS(instr, info); break;
			case 242 * 2: info->instr = Instruction::mtsrin; Db(instr, info); break;

			case 75 * 2: info->instr = Instruction::mulhw; Dab(instr, info); break;
			case (75 * 2) | RcBit: info->instr = Instruction::mulhw_d; Dab(instr, info); break;
			case 11 * 2: info->instr = Instruction::mulhwu; Dab(instr, info); break;
			case (11 * 2) | RcBit: info->instr = Instruction::mulhwu_d; Dab(instr, info); break;

			case 235 * 2: info->instr = Instruction::mullw; Dab(instr, info); break;
			case (235 * 2) | RcBit: info->instr = Instruction::mullw_d; Dab(instr, info); break;
			case (235 * 2) | OEBit: info->instr = Instruction::mullwo; Dab(instr, info); break;
			case (235 * 2) | OEBit | RcBit: info->instr = Instruction::mullwo_d; Dab(instr, info); break;

			case 476 * 2: info->instr = Instruction::nand; Asb(instr, info); break;
			case (476 * 2) | RcBit: info->instr = Instruction::nand_d; Asb(instr, info); break;
			case 104 * 2: info->instr = Instruction::neg; Da(instr, info); break;
			case (104 * 2) | RcBit: info->instr = Instruction::neg_d; Da(instr, info); break;
			case (104 * 2) | OEBit: info->instr = Instruction::nego; Da(instr, info); break;
			case (104 * 2) | OEBit | RcBit: info->instr = Instruction::nego_d; Da(instr, info); break;
			case 124 * 2: info->instr = Instruction::nor; Asb(instr, info); break;
			case (124 * 2) | RcBit: info->instr = Instruction::nor_d; Asb(instr, info); break;
			case 444 * 2: info->instr = Instruction::_or; Asb(instr, info); break;
			case (444 * 2) | RcBit: info->instr = Instruction::or_d; Asb(instr, info); break;
			case 412 * 2: info->instr = Instruction:: orc; Asb(instr, info); break;
			case (412 * 2) | RcBit: info->instr = Instruction::orc_d; Asb(instr, info); break;

			case 24 * 2: info->instr = Instruction::slw; Asb(instr, info); break;
			case (24 * 2) | RcBit: info->instr = Instruction::slw_d; Asb(instr, info); break;
			case 792 * 2: info->instr = Instruction::sraw; Asb(instr, info); break;
			case (792 * 2) | RcBit: info->instr = Instruction::sraw_d; Asb(instr, info); break;
			case 824 * 2: info->instr = Instruction::srawi; AsImm(instr, info); break;
			case (824 * 2) | RcBit: info->instr = Instruction::srawi_d; AsImm(instr, info); break;
			case 536 * 2: info->instr = Instruction::srw; Asb(instr, info); break;
			case (536 * 2) | RcBit: info->instr = Instruction::srw_d; Asb(instr, info); break;

			case 247 * 2: info->instr = Instruction::stbux; Dab(instr, info); break;
			case 215 * 2: info->instr = Instruction::stbx; Dab(instr, info); break;
			case 759 * 2: info->instr = Instruction::stfdux; FrDRegAb(instr, info); break;
			case 727 * 2: info->instr = Instruction::stfdx; FrDRegAb(instr, info); break;
			case 983 * 2: info->instr = Instruction::stfiwx; FrDRegAb(instr, info); break;
			case 695 * 2: info->instr = Instruction::stfsux; FrDRegAb(instr, info); break;
			case 663 * 2: info->instr = Instruction::stfsx; FrDRegAb(instr, info); break;
			case 918 * 2: info->instr = Instruction::sthbrx; Dab(instr, info); break;
			case 439 * 2: info->instr = Instruction::sthux; Dab(instr, info); break;
			case 407 * 2: info->instr = Instruction::sthx; Dab(instr, info); break;
			case 725 * 2: info->instr = Instruction::stswi; SaImm(instr, info); break;
			case 661 * 2: info->instr = Instruction::stswx; Dab(instr, info); break;
			case 662 * 2: info->instr = Instruction::stwbrx; Dab(instr, info); break;
			case (150 * 2) | RcBit: info->instr = Instruction::stwcx_d; Dab(instr, info); break;
			case 183 * 2: info->instr = Instruction::stwux; Dab(instr, info); break;
			case 151 * 2: info->instr = Instruction::stwux; Dab(instr, info); break;

			case 40 * 2: info->instr = Instruction::subf; Dab(instr, info); break;
			case (40 * 2) | RcBit: info->instr = Instruction::subf_d; Dab(instr, info); break;
			case (40 * 2) | OEBit: info->instr = Instruction::subfo; Dab(instr, info); break;
			case (40 * 2) | OEBit | RcBit: info->instr = Instruction::subfo_d; Dab(instr, info); break;
			case 8 * 2: info->instr = Instruction::subfc; Dab(instr, info); break;
			case (8 * 2) | RcBit: info->instr = Instruction::subfc_d; Dab(instr, info); break;
			case (8 * 2) | OEBit: info->instr = Instruction::subfco; Dab(instr, info); break;
			case (8 * 2) | OEBit | RcBit: info->instr = Instruction::subfco_d; Dab(instr, info); break;
			case 136 * 2: info->instr = Instruction::subfe; Dab(instr, info); break;
			case (136 * 2) | RcBit: info->instr = Instruction::subfe_d; Dab(instr, info); break;
			case (136 * 2) | OEBit: info->instr = Instruction::subfeo; Dab(instr, info); break;
			case (136 * 2) | OEBit | RcBit: info->instr = Instruction::subfeo_d; Dab(instr, info); break;

			case 232 * 2: info->instr = Instruction::subfme; Da(instr, info); break;
			case (232 * 2) | RcBit: info->instr = Instruction::subfme_d; Da(instr, info); break;
			case (232 * 2) | OEBit: info->instr = Instruction::subfmeo; Da(instr, info); break;
			case (232 * 2) | OEBit | RcBit: info->instr = Instruction::subfmeo_d; Da(instr, info); break;

			case 200 * 2: info->instr = Instruction::subfze; Da(instr, info); break;
			case (200 * 2) | RcBit: info->instr = Instruction::subfze_d; Da(instr, info); break;
			case (200 * 2) | OEBit: info->instr = Instruction::subfzeo; Da(instr, info); break;
			case (200 * 2) | OEBit | RcBit: info->instr = Instruction::subfzeo_d; Da(instr, info); break;

			case 598 * 2: info->instr = Instruction::sync; break;
			case 306 * 2: info->instr = Instruction::tlbie; B(instr, info); break;
			case 566 * 2: info->instr = Instruction::tlbsync; break;
			case 4 * 2: info->instr = Instruction::tw; ImmAb(instr, info); info->flow = true; break;

			case 316 * 2: info->instr = Instruction::_xor; Asb(instr, info); break;
			case (316 * 2) | RcBit: info->instr = Instruction::xor_d; Asb(instr, info); break;
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

			case 28 * 2: info->instr = Instruction::_and; AsbFast(instr, info); break;
			case (28 * 2) | RcBit: info->instr = Instruction::and_d; AsbFast(instr, info); break;

			case 60 * 2: info->instr = Instruction::andc; AsbFast(instr, info); break;
			case (60 * 2) | RcBit: info->instr = Instruction::andc_d; AsbFast(instr, info); break;

			case 0:
				if ((DIS_RD & 1) == 0)
				{
					info->instr = Instruction::cmp; CrfDabFast(instr, info);
				}
				break;

			case 32 * 2:
				if ((DIS_RD & 1) == 0)
				{
					info->instr = Instruction::cmpl; CrfDabFast(instr, info);
				}
				break;

			case 26 * 2: info->instr = Instruction::cntlzw; AsFast(instr, info); break;
			case (26 * 2) | RcBit: info->instr = Instruction::cntlzw_d; AsFast(instr, info); break;

			case 86 * 2: info->instr = Instruction::dcbf; AbFast(instr, info); break;
			case 470 * 2: info->instr = Instruction::dcbi; AbFast(instr, info); break;
			case 54 * 2: info->instr = Instruction::dcbst; AbFast(instr, info); break;
			case 278 * 2: info->instr = Instruction::dcbt; AbFast(instr, info); break;
			case 246 * 2: info->instr = Instruction::dcbtst; AbFast(instr, info); break;
			case 1014 * 2: info->instr = Instruction::dcbz; AbFast(instr, info); break;

			case 491 * 2: info->instr = Instruction::divw; DabFast(instr, info); break;
			case (491 * 2) | RcBit: info->instr = Instruction::divw_d; DabFast(instr, info); break;
			case (491 * 2) | OEBit: info->instr = Instruction::divwo; DabFast(instr, info); break;
			case (491 * 2) | OEBit | RcBit: info->instr = Instruction::divwo_d; DabFast(instr, info); break;

			case 459 * 2: info->instr = Instruction::divwu; DabFast(instr, info); break;
			case (459 * 2) | RcBit: info->instr = Instruction::divwu_d; DabFast(instr, info); break;
			case (459 * 2) | OEBit: info->instr = Instruction::divwuo; DabFast(instr, info); break;
			case (459 * 2) | OEBit | RcBit: info->instr = Instruction::divwuo_d; DabFast(instr, info); break;

			case 310 * 2: info->instr = Instruction::eciwx; DabFast(instr, info); break;
			case 438 * 2: info->instr = Instruction::ecowx; DabFast(instr, info); break;

			case 854 * 2: info->instr = Instruction::eieio; break;

			case 284 * 2: info->instr = Instruction::eqv; AsbFast(instr, info); break;
			case (284 * 2) | RcBit: info->instr = Instruction::eqv_d; AsbFast(instr, info); break;

			case 954 * 2: info->instr = Instruction::extsb; AsFast(instr, info); break;
			case (954 * 2) | RcBit: info->instr = Instruction::extsb_d; AsFast(instr, info); break;
			case 922 * 2: info->instr = Instruction::extsh; AsFast(instr, info); break;
			case (922 * 2) | RcBit: info->instr = Instruction::extsh_d; AsFast(instr, info); break;

			case 982 * 2: info->instr = Instruction::icbi; AbFast(instr, info); break;

			case 119 * 2: info->instr = Instruction::lbzux; DabFast(instr, info); break;
			case 87 * 2: info->instr = Instruction::lbzx; DabFast(instr, info); break;
			case 631 * 2: info->instr = Instruction::lfdux; FrDRegAbFast(instr, info); break;
			case 599 * 2: info->instr = Instruction::lfdx; FrDRegAbFast(instr, info); break;
			case 567 * 2: info->instr = Instruction::lfsux; FrDRegAbFast(instr, info); break;
			case 535 * 2: info->instr = Instruction::lfsx; FrDRegAbFast(instr, info); break;
			case 375 * 2: info->instr = Instruction::lhaux; DabFast(instr, info); break;
			case 343 * 2: info->instr = Instruction::lhax; DabFast(instr, info); break;
			case 790 * 2: info->instr = Instruction::lhbrx; DabFast(instr, info); break;
			case 311 * 2: info->instr = Instruction::lhzux; DabFast(instr, info); break;
			case 279 * 2: info->instr = Instruction::lhzx; DabFast(instr, info); break;
			case 597 * 2: info->instr = Instruction::lswi; DaNbFast(instr, info); break;
			case 533 * 2: info->instr = Instruction::lswx; DabFast(instr, info); break;
			case 20 * 2: info->instr = Instruction::lwarx; DabFast(instr, info); break;
			case 534 * 2: info->instr = Instruction::lwbrx; DabFast(instr, info); break;
			case 55 * 2: info->instr = Instruction::lwzux; DabFast(instr, info); break;
			case 23 * 2: info->instr = Instruction::lwzx; DabFast(instr, info); break;

			case 512 * 2: info->instr = Instruction::mcrxr; CrfdFast(instr, info); break;
			case 19 * 2: info->instr = Instruction::mfcr; DFast(instr, info); break;
			case 83 * 2: info->instr = Instruction::mfmsr; DFast(instr, info); break;
			case 339 * 2: info->instr = Instruction::mfspr; DSprFast(instr, info); break;
			case 595 * 2: info->instr = Instruction::mfsr; DSrFast(instr, info); break;
			case 659 * 2: info->instr = Instruction::mfsrin; DbFast(instr, info); break;
			case 371 * 2: info->instr = Instruction::mftb; DTbrFast(instr, info); break;
			case 144 * 2: info->instr = Instruction::mtcrf; CrmsFast(instr, info); break;
			case 146 * 2: info->instr = Instruction::mtmsr; DFast(instr, info); break;
			case 467 * 2: info->instr = Instruction::mtspr; SprSFast(instr, info); break;
			case 210 * 2: info->instr = Instruction::mtsr; SrSFast(instr, info); break;
			case 242 * 2: info->instr = Instruction::mtsrin; DbFast(instr, info); break;

			case 75 * 2: info->instr = Instruction::mulhw; DabFast(instr, info); break;
			case (75 * 2) | RcBit: info->instr = Instruction::mulhw_d; DabFast(instr, info); break;
			case 11 * 2: info->instr = Instruction::mulhwu; DabFast(instr, info); break;
			case (11 * 2) | RcBit: info->instr = Instruction::mulhwu_d; DabFast(instr, info); break;

			case 235 * 2: info->instr = Instruction::mullw; DabFast(instr, info); break;
			case (235 * 2) | RcBit: info->instr = Instruction::mullw_d; DabFast(instr, info); break;
			case (235 * 2) | OEBit: info->instr = Instruction::mullwo; DabFast(instr, info); break;
			case (235 * 2) | OEBit | RcBit: info->instr = Instruction::mullwo_d; DabFast(instr, info); break;

			case 476 * 2: info->instr = Instruction::nand; AsbFast(instr, info); break;
			case (476 * 2) | RcBit: info->instr = Instruction::nand_d; AsbFast(instr, info); break;
			case 104 * 2: info->instr = Instruction::neg; DaFast(instr, info); break;
			case (104 * 2) | RcBit: info->instr = Instruction::neg_d; DaFast(instr, info); break;
			case (104 * 2) | OEBit: info->instr = Instruction::nego; DaFast(instr, info); break;
			case (104 * 2) | OEBit | RcBit: info->instr = Instruction::nego_d; DaFast(instr, info); break;
			case 124 * 2: info->instr = Instruction::nor; AsbFast(instr, info); break;
			case (124 * 2) | RcBit: info->instr = Instruction::nor_d; AsbFast(instr, info); break;
			case 444 * 2: info->instr = Instruction::_or ; AsbFast(instr, info); break;
			case (444 * 2) | RcBit: info->instr = Instruction::or_d; AsbFast(instr, info); break;
			case 412 * 2: info->instr = Instruction::orc; AsbFast(instr, info); break;
			case (412 * 2) | RcBit: info->instr = Instruction::orc_d; AsbFast(instr, info); break;

			case 24 * 2: info->instr = Instruction::slw; AsbFast(instr, info); break;
			case (24 * 2) | RcBit: info->instr = Instruction::slw_d; AsbFast(instr, info); break;
			case 792 * 2: info->instr = Instruction::sraw; AsbFast(instr, info); break;
			case (792 * 2) | RcBit: info->instr = Instruction::sraw_d; AsbFast(instr, info); break;
			case 824 * 2: info->instr = Instruction::srawi; AsImmFast(instr, info); break;
			case (824 * 2) | RcBit: info->instr = Instruction::srawi_d; AsImmFast(instr, info); break;
			case 536 * 2: info->instr = Instruction::srw; AsbFast(instr, info); break;
			case (536 * 2) | RcBit: info->instr = Instruction::srw_d; AsbFast(instr, info); break;

			case 247 * 2: info->instr = Instruction::stbux; DabFast(instr, info); break;
			case 215 * 2: info->instr = Instruction::stbx; DabFast(instr, info); break;
			case 759 * 2: info->instr = Instruction::stfdux; FrDRegAbFast(instr, info); break;
			case 727 * 2: info->instr = Instruction::stfdx; FrDRegAbFast(instr, info); break;
			case 983 * 2: info->instr = Instruction::stfiwx; FrDRegAbFast(instr, info); break;
			case 695 * 2: info->instr = Instruction::stfsux; FrDRegAbFast(instr, info); break;
			case 663 * 2: info->instr = Instruction::stfsx; FrDRegAbFast(instr, info); break;
			case 918 * 2: info->instr = Instruction::sthbrx; DabFast(instr, info); break;
			case 439 * 2: info->instr = Instruction::sthux; DabFast(instr, info); break;
			case 407 * 2: info->instr = Instruction::sthx; DabFast(instr, info); break;
			case 725 * 2: info->instr = Instruction::stswi; SaImmFast(instr, info); break;
			case 661 * 2: info->instr = Instruction::stswx; DabFast(instr, info); break;
			case 662 * 2: info->instr = Instruction::stwbrx; DabFast(instr, info); break;
			case (150 * 2) | RcBit: info->instr = Instruction::stwcx_d; DabFast(instr, info); break;
			case 183 * 2: info->instr = Instruction::stwux; DabFast(instr, info); break;
			case 151 * 2: info->instr = Instruction::stwux; DabFast(instr, info); break;

			case 40 * 2: info->instr = Instruction::subf; DabFast(instr, info); break;
			case (40 * 2) | RcBit: info->instr = Instruction::subf_d; DabFast(instr, info); break;
			case (40 * 2) | OEBit: info->instr = Instruction::subfo; DabFast(instr, info); break;
			case (40 * 2) | OEBit | RcBit: info->instr = Instruction::subfo_d; DabFast(instr, info); break;
			case 8 * 2: info->instr = Instruction::subfc; DabFast(instr, info); break;
			case (8 * 2) | RcBit: info->instr = Instruction::subfc_d; DabFast(instr, info); break;
			case (8 * 2) | OEBit: info->instr = Instruction::subfco; DabFast(instr, info); break;
			case (8 * 2) | OEBit | RcBit: info->instr = Instruction::subfco_d; DabFast(instr, info); break;
			case 136 * 2: info->instr = Instruction::subfe; DabFast(instr, info); break;
			case (136 * 2) | RcBit: info->instr = Instruction::subfe_d; DabFast(instr, info); break;
			case (136 * 2) | OEBit: info->instr = Instruction::subfeo; DabFast(instr, info); break;
			case (136 * 2) | OEBit | RcBit: info->instr = Instruction::subfeo_d; DabFast(instr, info); break;

			case 232 * 2: info->instr = Instruction::subfme; DaFast(instr, info); break;
			case (232 * 2) | RcBit: info->instr = Instruction::subfme_d; DaFast(instr, info); break;
			case (232 * 2) | OEBit: info->instr = Instruction::subfmeo; DaFast(instr, info); break;
			case (232 * 2) | OEBit | RcBit: info->instr = Instruction::subfmeo_d; DaFast(instr, info); break;

			case 200 * 2: info->instr = Instruction::subfze; DaFast(instr, info); break;
			case (200 * 2) | RcBit: info->instr = Instruction::subfze_d; DaFast(instr, info); break;
			case (200 * 2) | OEBit: info->instr = Instruction::subfzeo; DaFast(instr, info); break;
			case (200 * 2) | OEBit | RcBit: info->instr = Instruction::subfzeo_d; DaFast(instr, info); break;

			case 598 * 2: info->instr = Instruction::sync; break;
			case 306 * 2: info->instr = Instruction::tlbie; BFast(instr, info); break;
			case 566 * 2: info->instr = Instruction::tlbsync; break;
			case 4 * 2: info->instr = Instruction::tw; ImmAbFast(instr, info); info->flow = true; break;

			case 316 * 2: info->instr = Instruction::_xor; AsbFast(instr, info); break;
			case (316 * 2) | RcBit: info->instr = Instruction::xor_d; AsbFast(instr, info); break;
		}
	}

	#pragma endregion "Primary 31"

	#pragma region "Primary 59"

	void Analyzer::Op59(uint32_t instr, AnalyzeInfo* info)
	{
		switch (instr & 0x3f)
		{
			case 21 * 2: info->instr = Instruction::fadds; FrDab(instr, info); break;
			case (21 * 2) | RcBit: info->instr = Instruction::fadds_d; FrDab(instr, info); break;

			case 18 * 2: info->instr = Instruction::fdivs; FrDab(instr, info); break;
			case (18 * 2) | RcBit: info->instr = Instruction::fdivs_d; FrDab(instr, info); break;

			case 29 * 2: info->instr = Instruction::fmadds; FrDacb(instr, info); break;
			case (29 * 2) | RcBit: info->instr = Instruction::fmadds_d; FrDacb(instr, info); break;

			case 28 * 2: info->instr = Instruction::fmsubs; FrDacb(instr, info); break;
			case (28 * 2) | RcBit: info->instr = Instruction::fmsubs_d; FrDacb(instr, info); break;

			case 25 * 2: info->instr = Instruction::fmuls; FrDac(instr, info); break;
			case (25 * 2) | RcBit: info->instr = Instruction::fmuls_d; FrDac(instr, info); break;

			case 31 * 2: info->instr = Instruction::fnmadds; FrDacb(instr, info); break;
			case (31 * 2) | RcBit: info->instr = Instruction::fnmadds_d; FrDacb(instr, info); break;

			case 30 * 2: info->instr = Instruction::fnmsubs; FrDacb(instr, info); break;
			case (30 * 2) | RcBit: info->instr = Instruction::fnmsubs_d; FrDacb(instr, info); break;

			case 24 * 2: info->instr = Instruction::fres; FrDb(instr, info); break;
			case (24 * 2) | RcBit: info->instr = Instruction::fres_d; FrDb(instr, info); break;

			case 20 * 2: info->instr = Instruction::fsubs; FrDab(instr, info); break;
			case (20 * 2) | RcBit: info->instr = Instruction::fsubs_d; FrDab(instr, info); break;
		}
	}

	void Analyzer::Op59Fast(uint32_t instr, AnalyzeInfo* info)
	{
		switch (instr & 0x3f)
		{
			case 21 * 2: info->instr = Instruction::fadds; FrDabFast(instr, info); break;
			case (21 * 2) | RcBit: info->instr = Instruction::fadds_d; FrDabFast(instr, info); break;

			case 18 * 2: info->instr = Instruction::fdivs; FrDabFast(instr, info); break;
			case (18 * 2) | RcBit: info->instr = Instruction::fdivs_d; FrDabFast(instr, info); break;

			case 29 * 2: info->instr = Instruction::fmadds; FrDacbFast(instr, info); break;
			case (29 * 2) | RcBit: info->instr = Instruction::fmadds_d; FrDacbFast(instr, info); break;

			case 28 * 2: info->instr = Instruction::fmsubs; FrDacbFast(instr, info); break;
			case (28 * 2) | RcBit: info->instr = Instruction::fmsubs_d; FrDacbFast(instr, info); break;

			case 25 * 2: info->instr = Instruction::fmuls; FrDacFast(instr, info); break;
			case (25 * 2) | RcBit: info->instr = Instruction::fmuls_d; FrDacFast(instr, info); break;

			case 31 * 2: info->instr = Instruction::fnmadds; FrDacbFast(instr, info); break;
			case (31 * 2) | RcBit: info->instr = Instruction::fnmadds_d; FrDacbFast(instr, info); break;

			case 30 * 2: info->instr = Instruction::fnmsubs; FrDacbFast(instr, info); break;
			case (30 * 2) | RcBit: info->instr = Instruction::fnmsubs_d; FrDacbFast(instr, info); break;

			case 24 * 2: info->instr = Instruction::fres; FrDbFast(instr, info); break;
			case (24 * 2) | RcBit: info->instr = Instruction::fres_d; FrDbFast(instr, info); break;

			case 20 * 2: info->instr = Instruction::fsubs; FrDabFast(instr, info); break;
			case (20 * 2) | RcBit: info->instr = Instruction::fsubs_d; FrDabFast(instr, info); break;
		}
	}

	#pragma endregion "Primary 59"

	#pragma region "Primary 63"

	void Analyzer::Op63(uint32_t instr, AnalyzeInfo* info)
	{
		// Check Madd first
		
		switch (instr & 0x3f)
		{
			case 29 * 2: info->instr = Instruction::fmadd; FrDacb(instr, info); return;
			case (29 * 2) | RcBit: info->instr = Instruction::fmadd_d; FrDacb(instr, info); return;
			case 28 * 2: info->instr = Instruction::fmsub; FrDacb(instr, info); return;
			case (28 * 2) | RcBit: info->instr = Instruction::fmsub_d; FrDacb(instr, info); return;
			case 25 * 2: info->instr = Instruction::fmul; FrDac(instr, info); return;
			case (25 * 2) | RcBit: info->instr = Instruction::fmul_d; FrDac(instr, info); return;
			case 31 * 2: info->instr = Instruction::fnmadd; FrDacb(instr, info); return;
			case (31 * 2) | RcBit: info->instr = Instruction::fnmadd_d; FrDacb(instr, info); return;
			case 30 * 2: info->instr = Instruction::fnmsub; FrDacb(instr, info); return;
			case (30 * 2) | RcBit: info->instr = Instruction::fnmsub_d; FrDacb(instr, info); return;
			case 23 * 2: info->instr = Instruction::fsel; FrDacb(instr, info); return;
			case (23 * 2) | RcBit: info->instr = Instruction::fsel_d; FrDacb(instr, info); return;
		}

		switch (instr & 0x7ff)
		{
			case 264 * 2: info->instr = Instruction::fabs; FrDb(instr, info); break;
			case (264 * 2) | RcBit: info->instr = Instruction::fabs_d; FrDb(instr, info); break;

			case 21 * 2: info->instr = Instruction::fadd; FrDab(instr, info); break;
			case (21 * 2) | RcBit: info->instr = Instruction::fadd_d; FrDab(instr, info); break;

			case 32 * 2: info->instr = Instruction::fcmpo; CrfdFrAb(instr, info); break;
			case 0: info->instr = Instruction::fcmpu; CrfdFrAb(instr, info); break;

			case 14 * 2: info->instr = Instruction::fctiw; FrDb(instr, info); break;
			case (14 * 2) | RcBit: info->instr = Instruction::fctiw_d; FrDb(instr, info); break;
			case 15 * 2: info->instr = Instruction::fctiwz; FrDb(instr, info); break;
			case (15 * 2) | RcBit: info->instr = Instruction::fctiwz_d; FrDb(instr, info); break;

			case 18 * 2: info->instr = Instruction::fdiv; FrDab(instr, info); break;
			case (18 * 2) | RcBit: info->instr = Instruction::fdiv_d; FrDab(instr, info); break;

			case 72 * 2: info->instr = Instruction::fmr; FrDb(instr, info); break;
			case (72 * 2) | RcBit: info->instr = Instruction::fmr_d; FrDb(instr, info); break;

			case 136 * 2: info->instr = Instruction::fnabs; FrDb(instr, info); break;
			case (136 * 2) | RcBit: info->instr = Instruction::fnabs_d; FrDb(instr, info); break;

			case 40 * 2: info->instr = Instruction::fneg; FrDb(instr, info); break;
			case (40 * 2) | RcBit: info->instr = Instruction::fneg_d; FrDb(instr, info); break;

			case 12 * 2: info->instr = Instruction::frsp; FrDb(instr, info); break;
			case (12 * 2) | RcBit: info->instr = Instruction::frsp_d; FrDb(instr, info); break;

			case 26 * 2: info->instr = Instruction::frsqrte; FrDb(instr, info); break;
			case (26 * 2) | RcBit: info->instr = Instruction::frsqrte_d; FrDb(instr, info); break;

			case 20 * 2: info->instr = Instruction::fsub; FrDab(instr, info); break;
			case (20 * 2) | RcBit: info->instr = Instruction::fsub_d; FrDab(instr, info); break;

			case 64 * 2: info->instr = Instruction::mcrfs; Crfds(instr, info); break;

			case 583 * 2: info->instr = Instruction::mffs; Frd(instr, info); break;
			case (583 * 2) | RcBit: info->instr = Instruction::mffs_d; Frd(instr, info); break;

			case 70 * 2: info->instr = Instruction::mtfsb0; Crbd(instr, info); break;
			case (70 * 2) | RcBit: info->instr = Instruction::mtfsb0_d; Crbd(instr, info); break;
			case 38 * 2: info->instr = Instruction::mtfsb1; Crbd(instr, info); break;
			case (38 * 2) | RcBit: info->instr = Instruction::mtfsb1_d; Crbd(instr, info); break;
			case 711 * 2: info->instr = Instruction::mtfsf; FmFrb(instr, info); break;
			case (711 * 2) | RcBit: info->instr = Instruction::mtfsf_d; FmFrb(instr, info); break;
			case 134 * 2: info->instr = Instruction::mtfsfi; CrfdImm(instr, info); break;
			case (134 * 2) | RcBit: info->instr = Instruction::mtfsfi_d; CrfdImm(instr, info); break;
		}
	}

	void Analyzer::Op63Fast(uint32_t instr, AnalyzeInfo* info)
	{
		// Check Madd first
		
		switch (instr & 0x3f)
		{
			case 29 * 2: info->instr = Instruction::fmadd; FrDacbFast(instr, info); return;
			case (29 * 2) | RcBit: info->instr = Instruction::fmadd_d; FrDacbFast(instr, info); return;
			case 28 * 2: info->instr = Instruction::fmsub; FrDacbFast(instr, info); return;
			case (28 * 2) | RcBit: info->instr = Instruction::fmsub_d; FrDacbFast(instr, info); return;
			case 25 * 2: info->instr = Instruction::fmul; FrDacFast(instr, info); return;
			case (25 * 2) | RcBit: info->instr = Instruction::fmul_d; FrDacFast(instr, info); return;
			case 31 * 2: info->instr = Instruction::fnmadd; FrDacbFast(instr, info); return;
			case (31 * 2) | RcBit: info->instr = Instruction::fnmadd_d; FrDacbFast(instr, info); return;
			case 30 * 2: info->instr = Instruction::fnmsub; FrDacbFast(instr, info); return;
			case (30 * 2) | RcBit: info->instr = Instruction::fnmsub_d; FrDacbFast(instr, info); return;
			case 23 * 2: info->instr = Instruction::fsel; FrDacbFast(instr, info); return;
			case (23 * 2) | RcBit: info->instr = Instruction::fsel_d; FrDacbFast(instr, info); return;
		}

		switch (instr & 0x7ff)
		{
			case 264 * 2: info->instr = Instruction::fabs; FrDbFast(instr, info); break;
			case (264 * 2) | RcBit: info->instr = Instruction::fabs_d; FrDbFast(instr, info); break;

			case 21 * 2: info->instr = Instruction::fadd; FrDabFast(instr, info); break;
			case (21 * 2) | RcBit: info->instr = Instruction::fadd_d; FrDabFast(instr, info); break;

			case 32 * 2: info->instr = Instruction::fcmpo; CrfdFrAbFast(instr, info); break;
			case 0: info->instr = Instruction::fcmpu; CrfdFrAbFast(instr, info); break;

			case 14 * 2: info->instr = Instruction::fctiw; FrDbFast(instr, info); break;
			case (14 * 2) | RcBit: info->instr = Instruction::fctiw_d; FrDbFast(instr, info); break;
			case 15 * 2: info->instr = Instruction::fctiwz; FrDbFast(instr, info); break;
			case (15 * 2) | RcBit: info->instr = Instruction::fctiwz_d; FrDbFast(instr, info); break;

			case 18 * 2: info->instr = Instruction::fdiv; FrDabFast(instr, info); break;
			case (18 * 2) | RcBit: info->instr = Instruction::fdiv_d; FrDabFast(instr, info); break;

			case 72 * 2: info->instr = Instruction::fmr; FrDbFast(instr, info); break;
			case (72 * 2) | RcBit: info->instr = Instruction::fmr_d; FrDbFast(instr, info); break;

			case 136 * 2: info->instr = Instruction::fnabs; FrDbFast(instr, info); break;
			case (136 * 2) | RcBit: info->instr = Instruction::fnabs_d; FrDbFast(instr, info); break;

			case 40 * 2: info->instr = Instruction::fneg; FrDbFast(instr, info); break;
			case (40 * 2) | RcBit: info->instr = Instruction::fneg_d; FrDbFast(instr, info); break;

			case 12 * 2: info->instr = Instruction::frsp; FrDbFast(instr, info); break;
			case (12 * 2) | RcBit: info->instr = Instruction::frsp_d; FrDbFast(instr, info); break;

			case 26 * 2: info->instr = Instruction::frsqrte; FrDbFast(instr, info); break;
			case (26 * 2) | RcBit: info->instr = Instruction::frsqrte_d; FrDbFast(instr, info); break;

			case 20 * 2: info->instr = Instruction::fsub; FrDabFast(instr, info); break;
			case (20 * 2) | RcBit: info->instr = Instruction::fsub_d; FrDabFast(instr, info); break;

			case 64 * 2: info->instr = Instruction::mcrfs; CrfdsFast(instr, info); break;

			case 583 * 2: info->instr = Instruction::mffs; FrdFast(instr, info); break;
			case (583 * 2) | RcBit: info->instr = Instruction::mffs_d; FrdFast(instr, info); break;

			case 70 * 2: info->instr = Instruction::mtfsb0; CrbdFast(instr, info); break;
			case (70 * 2) | RcBit: info->instr = Instruction::mtfsb0_d; CrbdFast(instr, info); break;
			case 38 * 2: info->instr = Instruction::mtfsb1; CrbdFast(instr, info); break;
			case (38 * 2) | RcBit: info->instr = Instruction::mtfsb1_d; CrbdFast(instr, info); break;
			case 711 * 2: info->instr = Instruction::mtfsf; FmFrbFast(instr, info); break;
			case (711 * 2) | RcBit: info->instr = Instruction::mtfsf_d; FmFrbFast(instr, info); break;
			case 134 * 2: info->instr = Instruction::mtfsfi; CrfdImmFast(instr, info); break;
			case (134 * 2) | RcBit: info->instr = Instruction::mtfsfi_d; CrfdImmFast(instr, info); break;
		}
	}

	#pragma endregion "Primary 63"

	#pragma region "Primary 4"

	void Analyzer::Op4(uint32_t instr, AnalyzeInfo* info)
	{
		switch (instr & 0x7f)
		{
			case 38 * 2: info->instr = Instruction::psq_lux; FrAbWi(instr, info); return;	// Watch for return
			case 6 * 2: info->instr = Instruction::psq_lx; FrAbWi(instr, info); return;
			case 39 * 2: info->instr = Instruction::psq_stux; FrAbWi(instr, info); return;
			case 7 * 2: info->instr = Instruction::psq_stx; FrAbWi(instr, info); return;
		}

		// If none of the PS Load/Store instructions fits, continue to check further

		// Check Madd first

		switch (instr & 0x3f)
		{
			case 29 * 2: info->instr = Instruction::ps_madd; FrDacb(instr, info); return;
			case (29 * 2) | RcBit: info->instr = Instruction::ps_madd_d; FrDacb(instr, info); return;
			case 14 * 2: info->instr = Instruction::ps_madds0; FrDacb(instr, info); return;
			case (14 * 2) | RcBit: info->instr = Instruction::ps_madds0_d; FrDacb(instr, info); return;
			case 15 * 2: info->instr = Instruction::ps_madds1; FrDacb(instr, info); return;
			case (15 * 2) | RcBit: info->instr = Instruction::ps_madds1_d; FrDacb(instr, info); return;
			case 28 * 2: info->instr = Instruction::ps_msub; FrDacb(instr, info); return;
			case (28 * 2) | RcBit: info->instr = Instruction::ps_msub_d; FrDacb(instr, info); return;
			case 25 * 2: info->instr = Instruction::ps_mul; FrDac(instr, info); return;
			case (25 * 2) | RcBit: info->instr = Instruction::ps_mul_d; FrDac(instr, info); return;
			case 12 * 2: info->instr = Instruction::ps_muls0; FrDac(instr, info); return;
			case (12 * 2) | RcBit: info->instr = Instruction::ps_muls0_d; FrDac(instr, info); return;
			case 13 * 2: info->instr = Instruction::ps_muls1; FrDac(instr, info); return;
			case (13 * 2) | RcBit: info->instr = Instruction::ps_muls1_d; FrDac(instr, info); return;
			case 31 * 2: info->instr = Instruction::ps_nmadd; FrDacb(instr, info); return;
			case (31 * 2) | RcBit: info->instr = Instruction::ps_nmadd_d; FrDacb(instr, info); return;
			case 30 * 2: info->instr = Instruction::ps_nmsub; FrDacb(instr, info); return;
			case (30 * 2) | RcBit: info->instr = Instruction::ps_nmsub_d; FrDacb(instr, info); return;
			case 23 * 2: info->instr = Instruction::ps_sel; FrDacb(instr, info); return;
			case (23 * 2) | RcBit: info->instr = Instruction::ps_sel_d; FrDacb(instr, info); return;
			case 10 * 2: info->instr = Instruction::ps_sum0; FrDacb(instr, info); return;
			case (10 * 2) | RcBit: info->instr = Instruction::ps_sum0_d; FrDacb(instr, info); return;
			case 11 * 2: info->instr = Instruction::ps_sum1; FrDacb(instr, info); return;
			case (11 * 2) | RcBit: info->instr = Instruction::ps_sum1_d; FrDacb(instr, info); return;
		}

		switch (instr & 0x7ff)
		{
			case 1014 * 2: info->instr = Instruction::dcbz_l; Ab(instr, info); break;

			case 264 * 2: info->instr = Instruction::ps_abs; FrDb(instr, info); break;
			case (264 * 2) | RcBit: info->instr = Instruction::ps_abs_d; FrDb(instr, info); break;
			case 21 * 2: info->instr = Instruction::ps_add; FrDab(instr, info); break;
			case (21 * 2) | RcBit: info->instr = Instruction::ps_add_d; FrDab(instr, info); break;
			case 32 * 2: info->instr = Instruction::ps_cmpo0; CrfdFrAb(instr, info); break;
			case 96 * 2: info->instr = Instruction::ps_cmpo1; CrfdFrAb(instr, info); break;
			case 0	   : info->instr = Instruction::ps_cmpu0; CrfdFrAb(instr, info); break;
			case 64 * 2: info->instr = Instruction::ps_cmpu1; CrfdFrAb(instr, info); break;
			case 18 * 2: info->instr = Instruction::ps_div; FrDab(instr, info); break;
			case (18 * 2) | RcBit: info->instr = Instruction::ps_div_d; FrDab(instr, info); break;
			case 528 * 2: info->instr = Instruction::ps_merge00; FrDab(instr, info); break;
			case (528 * 2) | RcBit: info->instr = Instruction::ps_merge00_d; FrDab(instr, info); break;
			case 560 * 2: info->instr = Instruction::ps_merge01; FrDab(instr, info); break;
			case (560 * 2) | RcBit: info->instr = Instruction::ps_merge01_d; FrDab(instr, info); break;
			case 592 * 2: info->instr = Instruction::ps_merge10; FrDab(instr, info); break;
			case (592 * 2) | RcBit: info->instr = Instruction::ps_merge10_d; FrDab(instr, info); break;
			case 624 * 2: info->instr = Instruction::ps_merge11; FrDab(instr, info); break;
			case (624 * 2) | RcBit: info->instr = Instruction::ps_merge11_d; FrDab(instr, info); break;
			case 72 * 2: info->instr = Instruction::ps_mr; FrDb(instr, info); break;
			case (72 * 2) | RcBit: info->instr = Instruction::ps_mr_d; FrDb(instr, info); break;
			case 136 * 2: info->instr = Instruction::ps_nabs; FrDb(instr, info); break;
			case (136 * 2) | RcBit: info->instr = Instruction::ps_nabs_d; FrDb(instr, info); break;
			case 40 * 2: info->instr = Instruction::ps_neg; FrDb(instr, info); break;
			case (40 * 2) | RcBit: info->instr = Instruction::ps_neg_d; FrDb(instr, info); break;
			case 24 * 2: info->instr = Instruction::ps_res; FrDb(instr, info); break;
			case (24 * 2) | RcBit: info->instr = Instruction::ps_res_d; FrDb(instr, info); break;
			case 26 * 2: info->instr = Instruction::ps_rsqrte; FrDb(instr, info); break;
			case (26 * 2) | RcBit: info->instr = Instruction::ps_rsqrte_d; FrDb(instr, info); break;
			case 20 * 2: info->instr = Instruction::ps_sub; FrDab(instr, info); break;
			case (20 * 2) | RcBit: info->instr = Instruction::ps_sub_d; FrDab(instr, info); break;
		}
	}

	void Analyzer::Op4Fast(uint32_t instr, AnalyzeInfo* info)
	{
		switch (instr & 0x7f)
		{
			case 38 * 2: info->instr = Instruction::psq_lux; FrAbWiFast(instr, info); return;	// Watch for return
			case 6 * 2: info->instr = Instruction::psq_lx; FrAbWiFast(instr, info); return;
			case 39 * 2: info->instr = Instruction::psq_stux; FrAbWiFast(instr, info); return;
			case 7 * 2: info->instr = Instruction::psq_stx; FrAbWiFast(instr, info); return;
		}

		// If none of the PS Load/Store instructions fits, continue to check further

		// Check Madd first

		switch (instr & 0x3f)
		{
			case 29 * 2: info->instr = Instruction::ps_madd; FrDacbFast(instr, info); return;
			case (29 * 2) | RcBit: info->instr = Instruction::ps_madd_d; FrDacbFast(instr, info); return;
			case 14 * 2: info->instr = Instruction::ps_madds0; FrDacbFast(instr, info); return;
			case (14 * 2) | RcBit: info->instr = Instruction::ps_madds0_d; FrDacbFast(instr, info); return;
			case 15 * 2: info->instr = Instruction::ps_madds1; FrDacbFast(instr, info); return;
			case (15 * 2) | RcBit: info->instr = Instruction::ps_madds1_d; FrDacbFast(instr, info); return;
			case 28 * 2: info->instr = Instruction::ps_msub; FrDacbFast(instr, info); return;
			case (28 * 2) | RcBit: info->instr = Instruction::ps_msub_d; FrDacbFast(instr, info); return;
			case 25 * 2: info->instr = Instruction::ps_mul; FrDacFast(instr, info); return;
			case (25 * 2) | RcBit: info->instr = Instruction::ps_mul_d; FrDacFast(instr, info); return;
			case 12 * 2: info->instr = Instruction::ps_muls0; FrDacFast(instr, info); return;
			case (12 * 2) | RcBit: info->instr = Instruction::ps_muls0_d; FrDacFast(instr, info); return;
			case 13 * 2: info->instr = Instruction::ps_muls1; FrDacFast(instr, info); return;
			case (13 * 2) | RcBit: info->instr = Instruction::ps_muls1_d; FrDacFast(instr, info); return;
			case 31 * 2: info->instr = Instruction::ps_nmadd; FrDacbFast(instr, info); return;
			case (31 * 2) | RcBit: info->instr = Instruction::ps_nmadd_d; FrDacbFast(instr, info); return;
			case 30 * 2: info->instr = Instruction::ps_nmsub; FrDacbFast(instr, info); return;
			case (30 * 2) | RcBit: info->instr = Instruction::ps_nmsub_d; FrDacbFast(instr, info); return;
			case 23 * 2: info->instr = Instruction::ps_sel; FrDacbFast(instr, info); return;
			case (23 * 2) | RcBit: info->instr = Instruction::ps_sel_d; FrDacbFast(instr, info); return;
			case 10 * 2: info->instr = Instruction::ps_sum0; FrDacbFast(instr, info); return;
			case (10 * 2) | RcBit: info->instr = Instruction::ps_sum0_d; FrDacbFast(instr, info); return;
			case 11 * 2: info->instr = Instruction::ps_sum1; FrDacbFast(instr, info); return;
			case (11 * 2) | RcBit: info->instr = Instruction::ps_sum1_d; FrDacbFast(instr, info); return;
		}

		switch (instr & 0x7ff)
		{
			case 1014 * 2: info->instr = Instruction::dcbz_l; AbFast(instr, info); break;

			case 264 * 2: info->instr = Instruction::ps_abs; FrDbFast(instr, info); break;
			case (264 * 2) | RcBit: info->instr = Instruction::ps_abs_d; FrDbFast(instr, info); break;
			case 21 * 2: info->instr = Instruction::ps_add; FrDabFast(instr, info); break;
			case (21 * 2) | RcBit: info->instr = Instruction::ps_add_d; FrDabFast(instr, info); break;
			case 32 * 2: info->instr = Instruction::ps_cmpo0; CrfdFrAbFast(instr, info); break;
			case 96 * 2: info->instr = Instruction::ps_cmpo1; CrfdFrAbFast(instr, info); break;
			case 0	   : info->instr = Instruction::ps_cmpu0; CrfdFrAbFast(instr, info); break;
			case 64 * 2: info->instr = Instruction::ps_cmpu1; CrfdFrAbFast(instr, info); break;
			case 18 * 2: info->instr = Instruction::ps_div; FrDabFast(instr, info); break;
			case (18 * 2) | RcBit: info->instr = Instruction::ps_div_d; FrDabFast(instr, info); break;
			case 528 * 2: info->instr = Instruction::ps_merge00; FrDabFast(instr, info); break;
			case (528 * 2) | RcBit: info->instr = Instruction::ps_merge00_d; FrDabFast(instr, info); break;
			case 560 * 2: info->instr = Instruction::ps_merge01; FrDabFast(instr, info); break;
			case (560 * 2) | RcBit: info->instr = Instruction::ps_merge01_d; FrDabFast(instr, info); break;
			case 592 * 2: info->instr = Instruction::ps_merge10; FrDabFast(instr, info); break;
			case (592 * 2) | RcBit: info->instr = Instruction::ps_merge10_d; FrDabFast(instr, info); break;
			case 624 * 2: info->instr = Instruction::ps_merge11; FrDabFast(instr, info); break;
			case (624 * 2) | RcBit: info->instr = Instruction::ps_merge11_d; FrDabFast(instr, info); break;
			case 72 * 2: info->instr = Instruction::ps_mr; FrDbFast(instr, info); break;
			case (72 * 2) | RcBit: info->instr = Instruction::ps_mr_d; FrDbFast(instr, info); break;
			case 136 * 2: info->instr = Instruction::ps_nabs; FrDbFast(instr, info); break;
			case (136 * 2) | RcBit: info->instr = Instruction::ps_nabs_d; FrDbFast(instr, info); break;
			case 40 * 2: info->instr = Instruction::ps_neg; FrDbFast(instr, info); break;
			case (40 * 2) | RcBit: info->instr = Instruction::ps_neg_d; FrDbFast(instr, info); break;
			case 24 * 2: info->instr = Instruction::ps_res; FrDbFast(instr, info); break;
			case (24 * 2) | RcBit: info->instr = Instruction::ps_res_d; FrDbFast(instr, info); break;
			case 26 * 2: info->instr = Instruction::ps_rsqrte; FrDbFast(instr, info); break;
			case (26 * 2) | RcBit: info->instr = Instruction::ps_rsqrte_d; FrDbFast(instr, info); break;
			case 20 * 2: info->instr = Instruction::ps_sub; FrDabFast(instr, info); break;
			case (20 * 2) | RcBit: info->instr = Instruction::ps_sub_d; FrDabFast(instr, info); break;
		}
	}

	#pragma endregion "Primary 4"

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

	void Analyzer::FrDRegAb(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 3;
		info->param[0] = Param::FReg;
		info->param[1] = Param::Reg;
		info->param[2] = Param::Reg;
		FrDRegAbFast(instr, info);
	}

	void Analyzer::FrDRegAbFast(uint32_t instr, AnalyzeInfo* info)
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

	void Analyzer::CrfDab(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 3;
		info->param[0] = Param::Crf;
		info->param[1] = Param::Reg;
		info->param[2] = Param::Reg;
		CrfDabFast(instr, info);
	}

	void Analyzer::CrfDabFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD >> 2;
		info->paramBits[1] = DIS_RA;
		info->paramBits[2] = DIS_RB;
	}

	void Analyzer::CrfDaSimm(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 3;
		info->param[0] = Param::Crf;
		info->param[1] = Param::Reg;
		info->param[2] = Param::Simm;
		CrfDaSimmFast(instr, info);
	}

	void Analyzer::CrfDaSimmFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD >> 2;
		info->paramBits[1] = DIS_RA;
		info->Imm.Signed = DIS_SIMM;
	}

	void Analyzer::CrfDaUimm(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 3;
		info->param[0] = Param::Crf;
		info->param[1] = Param::Reg;
		info->param[2] = Param::Simm;
		CrfDaUimmFast(instr, info);
	}

	void Analyzer::CrfDaUimmFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD >> 2;
		info->paramBits[1] = DIS_RA;
		info->Imm.Unsigned = DIS_UIMM;
	}

	void Analyzer::As(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 2;
		info->param[0] = Param::Reg;
		info->param[1] = Param::Reg;
		AsFast(instr, info);
	}

	void Analyzer::AsFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RA;
		info->paramBits[1] = DIS_RS;
	}

	void Analyzer::CrbDab(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 3;
		info->param[0] = Param::Crb;
		info->param[1] = Param::Crb;
		info->param[2] = Param::Crb;
		CrbDabFast(instr, info);
	}

	void Analyzer::CrbDabFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD;
		info->paramBits[1] = DIS_RA;
		info->paramBits[2] = DIS_RB;
	}

	void Analyzer::Ab(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 2;
		info->param[0] = Param::Reg;
		info->param[1] = Param::Reg;
		AbFast(instr, info);
	}

	void Analyzer::AbFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RA;
		info->paramBits[1] = DIS_RB;
	}

	void Analyzer::FrDb(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 2;
		info->param[0] = Param::FReg;
		info->param[1] = Param::FReg;
		FrDbFast(instr, info);
	}

	void Analyzer::FrDbFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD;
		info->paramBits[1] = DIS_RB;
	}

	void Analyzer::FrDab(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 3;
		info->param[0] = Param::FReg;
		info->param[1] = Param::FReg;
		info->param[2] = Param::FReg;
		FrDabFast(instr, info);
	}

	void Analyzer::FrDabFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD;
		info->paramBits[1] = DIS_RA;
		info->paramBits[2] = DIS_RB;
	}

	void Analyzer::CrfdFrAb(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 3;
		info->param[0] = Param::Crf;
		info->param[1] = Param::FReg;
		info->param[2] = Param::FReg;
		CrfdFrAbFast(instr, info);
	}

	void Analyzer::CrfdFrAbFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD >> 2;
		info->paramBits[1] = DIS_RA;
		info->paramBits[2] = DIS_RB;
	}

	void Analyzer::FrDacb(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 4;
		info->param[0] = Param::FReg;
		info->param[1] = Param::FReg;
		info->param[2] = Param::FReg;
		info->param[3] = Param::FReg;
		FrDacbFast(instr, info);
	}

	void Analyzer::FrDacbFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD;
		info->paramBits[1] = DIS_RA;
		info->paramBits[2] = DIS_RC;
		info->paramBits[3] = DIS_RB;
	}

	void Analyzer::FrDac(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 3;
		info->param[0] = Param::FReg;
		info->param[1] = Param::FReg;
		info->param[2] = Param::FReg;
		FrDacFast(instr, info);
	}

	void Analyzer::FrDacFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD;
		info->paramBits[1] = DIS_RA;
		info->paramBits[2] = DIS_RC;
	}

	void Analyzer::DaOffset(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 2;
		info->param[0] = Param::Reg;
		info->param[1] = Param::RegOffset;
		DaOffsetFast(instr, info);
	}

	void Analyzer::DaOffsetFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD;
		info->paramBits[1] = DIS_RA;
		info->Imm.Unsigned = DIS_UIMM;
	}

	void Analyzer::FrdaOffset(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 2;
		info->param[0] = Param::FReg;
		info->param[1] = Param::RegOffset;
		DaOffsetFast(instr, info);
	}

	void Analyzer::FrdaOffsetFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD;
		info->paramBits[1] = DIS_RA;
		info->Imm.Unsigned = DIS_UIMM;
	}

	void Analyzer::DaNb(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 3;
		info->param[0] = Param::Reg;
		info->param[1] = Param::Reg;
		info->param[2] = Param::Num;
		DaNbFast(instr, info);
	}

	void Analyzer::DaNbFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD;
		info->paramBits[1] = DIS_RA;
		info->paramBits[2] = DIS_RB;
	}

	void Analyzer::Crfds(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 2;
		info->param[0] = Param::Crf;
		info->param[1] = Param::Crf;
		CrfdsFast(instr, info);
	}

	void Analyzer::CrfdsFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD >> 2;
		info->paramBits[1] = DIS_RS >> 2;
	}

	void Analyzer::Crfd(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 1;
		info->param[0] = Param::Crf;
		CrfdFast(instr, info);
	}

	void Analyzer::CrfdFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD >> 2;
	}

	void Analyzer::D(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 1;
		info->param[0] = Param::Reg;
		DFast(instr, info);
	}

	void Analyzer::DFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD;
	}

	void Analyzer::B(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 1;
		info->param[0] = Param::Reg;
		BFast(instr, info);
	}

	void Analyzer::BFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RB;
	}

	void Analyzer::Frd(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 1;
		info->param[0] = Param::FReg;
		FrdFast(instr, info);
	}

	void Analyzer::FrdFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD;
	}

	void Analyzer::DSpr(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 2;
		info->param[0] = Param::Reg;
		info->param[1] = Param::Spr;
		DSprFast(instr, info);
	}

	void Analyzer::DSprFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD;
		info->paramBits[1] = DIS_SPR;
	}

	void Analyzer::DSr(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 2;
		info->param[0] = Param::Reg;
		info->param[1] = Param::Sr;
		DSrFast(instr, info);
	}

	void Analyzer::DSrFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD;
		info->paramBits[1] = DIS_RA & 0xF;
	}

	void Analyzer::Db(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 2;
		info->param[0] = Param::Reg;
		info->param[1] = Param::Reg;
		DbFast(instr, info);
	}

	void Analyzer::DbFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD;
		info->paramBits[1] = DIS_RB;
	}

	void Analyzer::DTbr(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 2;
		info->param[0] = Param::Reg;
		info->param[1] = Param::Tbr;
		DTbrFast(instr, info);
	}

	void Analyzer::DTbrFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD;
		info->paramBits[1] = DIS_TBR;
	}

	void Analyzer::Crms(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 2;
		info->param[0] = Param::CRM;
		info->param[1] = Param::Reg;
		CrmsFast(instr, info);
	}

	void Analyzer::CrmsFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD;
		info->paramBits[1] = DIS_CRM;
	}

	void Analyzer::Crbd(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 1;
		info->param[0] = Param::Crb;
		CrbdFast(instr, info);
	}

	void Analyzer::CrbdFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD;
	}

	void Analyzer::FmFrb(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 2;
		info->param[0] = Param::FM;
		info->param[1] = Param::FReg;
		FmFrbFast(instr, info);
	}

	void Analyzer::FmFrbFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_FM;
		info->paramBits[1] = DIS_RB;
	}

	void Analyzer::CrfdImm(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 2;
		info->param[0] = Param::Crf;
		info->param[1] = Param::Num;
		CrfdImmFast(instr, info);
	}

	void Analyzer::CrfdImmFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD >> 2;
		info->paramBits[1] = DIS_RB >> 1;
	}

	void Analyzer::SprS(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 2;
		info->param[0] = Param::Spr;
		info->param[1] = Param::Reg;
		SprSFast(instr, info);
	}

	void Analyzer::SprSFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_SPR;
		info->paramBits[1] = DIS_RD;
	}

	void Analyzer::SrS(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 2;
		info->param[0] = Param::Sr;
		info->param[1] = Param::Reg;
		SrSFast(instr, info);
	}

	void Analyzer::SrSFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD;
		info->paramBits[1] = DIS_RA & 0xF;
	}

	void Analyzer::FrRegOffsetWi(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 4;
		info->param[0] = Param::FReg;
		info->param[1] = Param::RegOffset;
		info->param[2] = Param::Num;
		info->param[3] = Param::Num;
		FrRegOffsetWiFast(instr, info);
	}

	void Analyzer::FrRegOffsetWiFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD;
		info->paramBits[1] = DIS_RA;
		info->paramBits[2] = (instr >> 15) & 1;		// W
		info->paramBits[3] = (instr >> 12) & 7;		// I
		info->Imm.Signed = (instr & 0xfff);
		if (instr & 0x800) info->Imm.Signed |= 0xF000;
	}

	void Analyzer::FrAbWi(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 5;
		info->param[0] = Param::FReg;
		info->param[1] = Param::Reg;
		info->param[2] = Param::Reg;
		info->param[3] = Param::Num;
		info->param[4] = Param::Num;
		FrAbWiFast(instr, info);
	}

	void Analyzer::FrAbWiFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RD;
		info->paramBits[1] = DIS_RA;
		info->paramBits[2] = DIS_RB;
		info->paramBits[3] = (instr >> 10) & 1;		// W
		info->paramBits[4] = (instr >> 7) & 7;		// I
	}

	void Analyzer::AsImm3(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 5;
		info->param[0] = Param::Reg;
		info->param[1] = Param::Reg;
		info->param[2] = Param::Num;
		info->param[3] = Param::Num;
		info->param[4] = Param::Num;
		AsImm3Fast(instr, info);
	}

	void Analyzer::AsImm3Fast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RA;
		info->paramBits[1] = DIS_RS;
		info->paramBits[2] = DIS_RB;
		info->paramBits[3] = DIS_RC;
		info->paramBits[4] = (instr >> 1) & 0x1F;
	}

	void Analyzer::AsbImm2(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 5;
		info->param[0] = Param::Reg;
		info->param[1] = Param::Reg;
		info->param[2] = Param::Reg;
		info->param[3] = Param::Num;
		info->param[4] = Param::Num;
		AsbImm2Fast(instr, info);
	}

	void Analyzer::AsbImm2Fast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RA;
		info->paramBits[1] = DIS_RS;
		info->paramBits[2] = DIS_RB;
		info->paramBits[3] = DIS_RC;
		info->paramBits[4] = (instr >> 1) & 0x1F;
	}

	void Analyzer::AsImm(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 3;
		info->param[0] = Param::Reg;
		info->param[1] = Param::Reg;
		info->param[2] = Param::Num;
		AsImmFast(instr, info);
	}

	void Analyzer::AsImmFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RA;
		info->paramBits[1] = DIS_RS;
		info->paramBits[2] = DIS_RB;
	}

	void Analyzer::SaImm(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 3;
		info->param[0] = Param::Reg;
		info->param[1] = Param::Reg;
		info->param[2] = Param::Num;
		SaImmFast(instr, info);
	}

	void Analyzer::SaImmFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RS;
		info->paramBits[1] = DIS_RA;
		info->paramBits[2] = DIS_RB;
	}

	void Analyzer::ImmAb(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 3;
		info->param[0] = Param::Num;
		info->param[1] = Param::Reg;
		info->param[2] = Param::Reg;
		ImmAbFast(instr, info);
	}

	void Analyzer::ImmAbFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RS;
		info->paramBits[1] = DIS_RA;
		info->paramBits[2] = DIS_RB;
	}

	void Analyzer::ImmASimm(uint32_t instr, AnalyzeInfo* info)
	{
		info->numParam = 3;
		info->param[0] = Param::Num;
		info->param[1] = Param::Reg;
		info->param[2] = Param::Simm;
		ImmASimmFast(instr, info);
	}

	void Analyzer::ImmASimmFast(uint32_t instr, AnalyzeInfo* info)
	{
		info->paramBits[0] = DIS_RS;
		info->paramBits[1] = DIS_RA;
		info->Imm.Signed = DIS_SIMM;
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
			case 4: Op4(instr, info); break;
			case 19: Op19(instr, info); break;
			case 31: Op31(instr, info); break;
			case 59: Op59(instr, info); break;
			case 63: Op63(instr, info); break;
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
			case 4: Op4Fast(instr, info); break;
			case 19: Op19Fast(instr, info); break;
			case 31: Op31Fast(instr, info); break;
			case 59: Op59Fast(instr, info); break;
			case 63: Op63Fast(instr, info); break;
			default: OpMainFast(instr, info); break;
		}
	}

}
