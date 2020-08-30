// Non-parallel instructions.
#include "pch.h"

using namespace Debug;

namespace DSP
{

	void DspInterpreter::ABS(AnalyzeInfo& info)
	{
		int n = info.paramBits[0];
		DspLongAccumulator a = core->regs.ac[n];
		core->regs.ac[n].sbits = DspCore::SignExtend40(core->regs.ac[n].sbits) >= 0 ? core->regs.ac[n].sbits : -core->regs.ac[n].sbits;
		Flags(a, a, core->regs.ac[n]);
		core->regs.ac[n].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::ADD(AnalyzeInfo& info)
	{
		int d = info.paramBits[0];
		DspLongAccumulator a = core->regs.ac[d];
		DspLongAccumulator b;
		b.sbits = DspCore::SignExtend40(core->regs.ac[1 - d].sbits);
		core->regs.ac[d].sbits = DspCore::SignExtend40(core->regs.ac[d].sbits);
		core->regs.ac[d].sbits += DspCore::SignExtend40(core->regs.ac[1 - d].sbits);
		Flags(a, b, core->regs.ac[d]);
		core->regs.ac[d].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::ADDARN(AnalyzeInfo& info)
	{
		core->regs.ar[info.paramBits[0]] += core->regs.ix[info.paramBits[1]];
	}

	void DspInterpreter::ADDAX(AnalyzeInfo& info)
	{
		DspLongAccumulator a = core->regs.ac[info.paramBits[0]];
		DspLongAccumulator b;
		b.sbits = DspCore::SignExtend40((int64_t)core->regs.ax[info.paramBits[1]].sbits);
		core->regs.ac[info.paramBits[0]].sbits = DspCore::SignExtend40(core->regs.ac[info.paramBits[0]].sbits);
		core->regs.ac[info.paramBits[0]].sbits += DspCore::SignExtend40((int64_t)core->regs.ax[info.paramBits[1]].sbits);
		Flags(a, b, core->regs.ac[info.paramBits[0]]);
		core->regs.ac[info.paramBits[0]].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::ADDAXL(AnalyzeInfo& info)
	{
		DspLongAccumulator a = core->regs.ac[info.paramBits[0]];
		DspLongAccumulator b;
		b.sbits = DspCore::SignExtend16(core->regs.ax[info.paramBits[1]].l);
		core->regs.ac[info.paramBits[0]].sbits = DspCore::SignExtend40(core->regs.ac[info.paramBits[0]].sbits);
		core->regs.ac[info.paramBits[0]].sbits += b.sbits;
		Flags(a, b, core->regs.ac[info.paramBits[0]]);
		core->regs.ac[info.paramBits[0]].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::ADDI(AnalyzeInfo& info)
	{
		DspLongAccumulator a = core->regs.ac[info.paramBits[0]];
		DspLongAccumulator b;
		b.shm = (int32_t)(int16_t)info.ImmOperand.UnsignedShort;
		b.l = 0;
		core->regs.ac[info.paramBits[0]].shm += (int32_t)(int16_t)info.ImmOperand.UnsignedShort;
		Flags(a, b, core->regs.ac[info.paramBits[0]]);
		core->regs.ac[info.paramBits[0]].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::ADDIS(AnalyzeInfo& info)
	{
		DspLongAccumulator a = core->regs.ac[info.paramBits[0]];
		DspLongAccumulator b;
		b.shm = (int32_t)(int16_t)info.ImmOperand.SignedByte;
		b.l = 0;
		core->regs.ac[info.paramBits[0]].shm += (int32_t)(int16_t)info.ImmOperand.SignedByte;
		Flags(a, b, core->regs.ac[info.paramBits[0]]);
		core->regs.ac[info.paramBits[0]].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::ADDP(AnalyzeInfo& info)
	{
		DspLongAccumulator a = core->regs.ac[info.paramBits[0]];
		DspLongAccumulator b;
		core->PackProd(core->regs.prod);
		b.sbits = core->regs.prod.bitsPacked;
		core->regs.ac[info.paramBits[0]].sbits += b.sbits;
		Flags(a, b, core->regs.ac[info.paramBits[0]]);
		core->regs.ac[info.paramBits[0]].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::ADDPAXZ(AnalyzeInfo& info)
	{
		DspLongAccumulator a, b, c;
		core->PackProd(core->regs.prod);
		a.sbits = core->regs.prod.bitsPacked;
		b.shm = info.paramBits[1] ? (int32_t)(int16_t)core->regs.ax[1].h : (int32_t)(int16_t)core->regs.ax[0].h;
		b.l = 0;
		c.shm = a.shm + b.shm;
		c.l = 0;
		core->regs.ac[info.paramBits[0]] = c;
		Flags(a, b, core->regs.ac[info.paramBits[0]]);
		core->regs.ac[info.paramBits[0]].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::ADDR(AnalyzeInfo& info)
	{
		DspLongAccumulator a = core->regs.ac[info.paramBits[0]];
		DspLongAccumulator b;
		b.sbits = DspCore::SignExtend40((int64_t)core->MoveFromReg(info.paramBits[1]) << 16);
		core->regs.ac[info.paramBits[0]].sbits = DspCore::SignExtend40(core->regs.ac[info.paramBits[0]].sbits);
		core->regs.ac[info.paramBits[0]].sbits += b.sbits;
		Flags(a, b, core->regs.ac[info.paramBits[0]]);
		core->regs.ac[info.paramBits[0]].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::ANDC(AnalyzeInfo& info)
	{
		int n = info.paramBits[0];
		DspLongAccumulator a = core->regs.ac[n];
		core->regs.ac[n].m &= core->regs.ac[1 - n].m;
		Flags(a, a, core->regs.ac[n]);
		core->regs.ac[n].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::TCLR(AnalyzeInfo& info)
	{
		core->regs.psr.tb = (core->regs.ac[info.paramBits[0]].m & info.ImmOperand.UnsignedShort) == 0;
	}

	void DspInterpreter::TSET(AnalyzeInfo& info)
	{
		core->regs.psr.tb = (core->regs.ac[info.paramBits[0]].m & info.ImmOperand.UnsignedShort) == info.ImmOperand.UnsignedShort;
	}

	void DspInterpreter::ANDI(AnalyzeInfo& info)
	{
		int n = info.paramBits[0];
		DspLongAccumulator a = core->regs.ac[n];
		core->regs.ac[n].m &= info.ImmOperand.UnsignedShort;
		Flags(a, a, core->regs.ac[n]);
		core->regs.ac[n].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::ANDR(AnalyzeInfo& info)
	{
		int d = info.paramBits[0];
		DspLongAccumulator a = core->regs.ac[d];
		core->regs.ac[d].m &= core->regs.ax[info.paramBits[1]].h;
		Flags(a, a, core->regs.ac[d]);
		core->regs.ac[d].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::ASL(AnalyzeInfo& info)
	{
		DspLongAccumulator a = core->regs.ac[info.paramBits[0]];
		core->regs.ac[info.paramBits[0]].sbits <<= info.ImmOperand.SignedByte;
		Flags(a, a, core->regs.ac[info.paramBits[0]]);
		core->regs.ac[info.paramBits[0]].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::ASR(AnalyzeInfo& info)
	{
		DspLongAccumulator a = core->regs.ac[info.paramBits[0]];
		core->regs.ac[info.paramBits[0]].sbits = DspCore::SignExtend40(core->regs.ac[info.paramBits[0]].sbits);
		core->regs.ac[info.paramBits[0]].sbits >>= -info.ImmOperand.SignedByte;
		Flags(a, a, core->regs.ac[info.paramBits[0]]);
		core->regs.ac[info.paramBits[0]].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::ASR16(AnalyzeInfo& info)
	{
		DspLongAccumulator a = core->regs.ac[info.paramBits[0]];
		core->regs.ac[info.paramBits[0]].sbits = DspCore::SignExtend40(core->regs.ac[info.paramBits[0]].sbits);
		core->regs.ac[info.paramBits[0]].sbits >>= 16;
		Flags(a, a, core->regs.ac[info.paramBits[0]]);
		core->regs.ac[info.paramBits[0]].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::BLOOP(AnalyzeInfo& info)
	{
		int oldXl = core->regs.psr.xl;
		core->regs.psr.xl = 0;
		if (core->MoveFromReg(info.paramBits[0]) != 0)
		{
			SetLoop(core->regs.pc + 2, info.ImmOperand.Address, core->MoveFromReg(info.paramBits[0]));
			core->regs.pc += 2;
		}
		else
		{
			core->regs.pc = info.ImmOperand.Address + 1;
		}
		core->regs.psr.xl = oldXl;
	}

	void DspInterpreter::BLOOPI(AnalyzeInfo& info)
	{
		if (info.ImmOperand.Byte != 0)
		{
			SetLoop(core->regs.pc + 2, info.ImmOperand2.Address, info.ImmOperand.Byte);
			core->regs.pc += 2;
		}
		else
		{
			core->regs.pc = info.ImmOperand2.Address + 1;
		}
	}

	void DspInterpreter::CALLcc(AnalyzeInfo& info)
	{
		if (Condition(info.cc))
		{
			if (core->dsp->logNonconditionalCallJmp)
			{
				Report(Channel::DSP, "0x%04X: CALL 0x%04X\n", core->regs.pc, info.ImmOperand.Address);
			}

			core->regs.st[0].push_back(core->regs.pc + 2);
			core->regs.pc = info.ImmOperand.Address;
		}
		else
		{
			core->regs.pc += 2;
		}
	}

	void DspInterpreter::CALLR(AnalyzeInfo& info)
	{
		int oldXl = core->regs.psr.xl;
		core->regs.psr.xl = 0;

		uint16_t address = core->MoveFromReg(info.paramBits[0]);

		if (core->dsp->logNonconditionalCallJmp)
		{
			Report(Channel::DSP, "0x%04X: CALLR 0x%04X\n", core->regs.pc, address);
		}

		core->regs.st[0].push_back(core->regs.pc + 1);
		core->regs.pc = address;
		core->regs.psr.xl = oldXl;
	}

	void DspInterpreter::CLR(AnalyzeInfo& info)
	{
		int n = info.paramBits[0];
		DspLongAccumulator a = core->regs.ac[n];
		core->regs.ac[n].bits = 0;
		Flags(a, a, core->regs.ac[n]);
	}

	void DspInterpreter::CLRL(AnalyzeInfo& info)
	{
		DspLongAccumulator a = core->regs.ac[info.paramBits[0]];
		core->regs.ac[info.paramBits[0]].l = 0;
		Flags(a, a, core->regs.ac[info.paramBits[0]]);
	}

	void DspInterpreter::CLRP(AnalyzeInfo& info)
	{
		core->regs.prod.l = 0;
		core->regs.prod.m1 = 0xfff0;
		core->regs.prod.h = 0xff;
		core->regs.prod.m2 = 0x10;
	}

	void DspInterpreter::CMP(AnalyzeInfo& info)
	{
		DspLongAccumulator a, b, c;
		a.sbits = DspCore::SignExtend40(core->regs.ac[0].sbits);
		b.sbits = -DspCore::SignExtend40(core->regs.ac[1].sbits);
		c.sbits = a.sbits + b.sbits;
		Flags(a, b, c);
	}

	void DspInterpreter::CMPI(AnalyzeInfo& info)
	{
		DspLongAccumulator a, b, c;
		a.hm = core->regs.ac[info.paramBits[0]].hm;
		a.l = 0;
		b.hm = (int32_t)(int16_t)info.ImmOperand.UnsignedShort;
		b.l = 0;
		b.sbits = -b.sbits;
		c.sbits = a.sbits + b.sbits;
		Flags(a, b, c);
	}

	void DspInterpreter::CMPIS(AnalyzeInfo& info)
	{
		DspLongAccumulator a, b, c;
		a.hm = core->regs.ac[info.paramBits[0]].hm;
		a.l = 0;
		b.hm = (int32_t)(int16_t)info.ImmOperand.SignedByte;
		b.l = 0;
		b.sbits = -b.sbits;
		c.sbits = a.sbits + b.sbits;
		Flags(a, b, c);
	}

	// Compares accumulator $acS.m with accumulator ax[0|1].h

	void DspInterpreter::CMPAR(AnalyzeInfo& info)
	{
		DspLongAccumulator a, b, c;
		a.sbits = DspCore::SignExtend16(core->regs.ac[info.paramBits[0]].m);
		b.sbits = info.paramBits[1] ? DspCore::SignExtend16(core->regs.ax[1].h) : DspCore::SignExtend16(core->regs.ax[0].h);
		b.sbits = -b.sbits;
		c.sbits = a.sbits + b.sbits;
		Flags(a, b, c);
	}

	void DspInterpreter::DAR(AnalyzeInfo& info)
	{
		core->regs.ar[info.paramBits[0]]--;
	}

	void DspInterpreter::DEC(AnalyzeInfo& info)
	{
		DspLongAccumulator a = core->regs.ac[info.paramBits[0]];
		core->regs.ac[info.paramBits[0]].sbits--;
		Flags(a, a, core->regs.ac[info.paramBits[0]]);
		core->regs.ac[info.paramBits[0]].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::DECM(AnalyzeInfo& info)
	{
		DspLongAccumulator a = core->regs.ac[info.paramBits[0]];
		core->regs.ac[info.paramBits[0]].hm--;
		Flags(a, a, core->regs.ac[info.paramBits[0]]);
		core->regs.ac[info.paramBits[0]].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::HALT(AnalyzeInfo& info)
	{
		core->dsp->Suspend();
	}

	void DspInterpreter::IAR(AnalyzeInfo& info)
	{
		core->regs.ar[info.paramBits[0]]++;
	}

	void DspInterpreter::INC(AnalyzeInfo& info)
	{
		DspLongAccumulator a = core->regs.ac[info.paramBits[0]];
		core->regs.ac[info.paramBits[0]].sbits++;
		Flags(a, a, core->regs.ac[info.paramBits[0]]);
		core->regs.ac[info.paramBits[0]].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::INCM(AnalyzeInfo& info)
	{
		DspLongAccumulator a = core->regs.ac[info.paramBits[0]];
		core->regs.ac[info.paramBits[0]].hm++;
		Flags(a, a, core->regs.ac[info.paramBits[0]]);
		core->regs.ac[info.paramBits[0]].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::IFcc(AnalyzeInfo& info)
	{
		if (Condition(info.cc))
		{
			core->regs.pc++;
		}
		else
		{
			core->regs.pc += 2;
		}
	}

	void DspInterpreter::ILRR(AnalyzeInfo& info)
	{
		core->MoveToReg(
			(int)DspRegister::ac0m + info.paramBits[0],
			core->dsp->ReadIMem(core->regs.ar[info.paramBits[1]]));
	}

	void DspInterpreter::ILRRD(AnalyzeInfo& info)
	{
		core->MoveToReg(
			(int)DspRegister::ac0m + info.paramBits[0],
			core->dsp->ReadIMem(core->regs.ar[info.paramBits[1]]));
		core->regs.ar[info.paramBits[1]]--;
	}

	void DspInterpreter::ILRRI(AnalyzeInfo& info)
	{
		core->MoveToReg(
			(int)DspRegister::ac0m + info.paramBits[0],
			core->dsp->ReadIMem(core->regs.ar[info.paramBits[1]]));
		core->regs.ar[info.paramBits[1]]++;
	}

	void DspInterpreter::ILRRN(AnalyzeInfo& info)
	{
		core->MoveToReg(
			(int)DspRegister::ac0m + info.paramBits[0],
			core->dsp->ReadIMem(core->regs.ar[info.paramBits[1]]));
		core->regs.ar[info.paramBits[1]] += core->regs.ix[info.paramBits[1]];
	}

	void DspInterpreter::Jcc(AnalyzeInfo& info)
	{
		if (Condition(info.cc))
		{
			if (core->dsp->logNonconditionalCallJmp && info.cc == ConditionCode::Always)
			{
				Report(Channel::DSP, "0x%04X: JMP 0x%04X\n", core->regs.pc, info.ImmOperand.Address);
			}

			core->regs.pc = info.ImmOperand.Address;
		}
		else
		{
			core->regs.pc += 2;
		}
	}

	void DspInterpreter::JMPR(AnalyzeInfo& info)
	{
		int oldXl = core->regs.psr.xl;
		core->regs.psr.xl = 0;

		uint16_t address = core->MoveFromReg(info.paramBits[0]);

		if (core->dsp->logNonconditionalCallJmp)
		{
			Report(Channel::DSP, "0x%04X: JMPR 0x%04X\n", core->regs.pc, address);
		}

		core->regs.pc = address;
		core->regs.psr.xl = oldXl;
	}

	void DspInterpreter::LOOP(AnalyzeInfo& info)
	{
		int oldSxm = core->regs.psr.xl;
		core->regs.psr.xl = 0;
		if (core->MoveFromReg(info.paramBits[0]) != 0)
		{
			SetLoop(core->regs.pc + 1, core->regs.pc + 1, core->MoveFromReg(info.paramBits[0]));
			core->regs.pc++;
		}
		else
		{
			core->regs.pc += 2;
		}
		core->regs.psr.xl = oldSxm;
	}

	void DspInterpreter::LOOPI(AnalyzeInfo& info)
	{
		if (info.ImmOperand.Byte != 0)
		{
			SetLoop(core->regs.pc + 1, core->regs.pc + 1, info.ImmOperand.Byte);
			core->regs.pc++;
		}
		else
		{
			core->regs.pc += 2;
		}
	}

	void DspInterpreter::LR(AnalyzeInfo& info)
	{
		core->MoveToReg(info.paramBits[0], core->dsp->ReadDMem(info.ImmOperand.UnsignedShort));
	}

	void DspInterpreter::LRI(AnalyzeInfo& info)
	{
		core->MoveToReg(info.paramBits[0], info.ImmOperand.UnsignedShort);
	}

	void DspInterpreter::LRIS(AnalyzeInfo& info)
	{
		core->MoveToReg(info.paramBits[0], (uint16_t)(int16_t)info.ImmOperand.SignedByte);
	}

	void DspInterpreter::LRR(AnalyzeInfo& info)
	{
		core->MoveToReg(info.paramBits[0], core->dsp->ReadDMem(core->regs.ar[info.paramBits[1]]));
	}

	void DspInterpreter::LRRD(AnalyzeInfo& info)
	{
		core->MoveToReg(info.paramBits[0], core->dsp->ReadDMem(core->regs.ar[info.paramBits[1]]));
		core->regs.ar[info.paramBits[1]]--;
	}

	void DspInterpreter::LRRI(AnalyzeInfo& info)
	{
		core->MoveToReg(info.paramBits[0], core->dsp->ReadDMem(core->regs.ar[info.paramBits[1]]));
		core->regs.ar[info.paramBits[1]]++;
	}

	void DspInterpreter::LRRN(AnalyzeInfo& info)
	{
		core->MoveToReg(info.paramBits[0], core->dsp->ReadDMem(core->regs.ar[info.paramBits[1]]));
		core->regs.ar[info.paramBits[1]] += core->regs.ix[info.paramBits[1]];
	}

	void DspInterpreter::LRS(AnalyzeInfo& info)
	{
		core->MoveToReg(info.paramBits[0], core->dsp->ReadDMem(
			(core->regs.dpp << 8) | (uint8_t)info.ImmOperand.Address));
	}

	void DspInterpreter::LSL(AnalyzeInfo& info)
	{
		int n = info.paramBits[0];
		DspLongAccumulator a = core->regs.ac[n];
		core->regs.ac[n].bits <<= info.ImmOperand.Byte;
		Flags(a, a, core->regs.ac[n]);
		core->regs.ac[n].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::LSL16(AnalyzeInfo& info)
	{
		int n = info.paramBits[0];
		DspLongAccumulator a = core->regs.ac[n];
		core->regs.ac[n].bits <<= 16;
		Flags(a, a, core->regs.ac[n]);
		core->regs.ac[n].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::LSR(AnalyzeInfo& info)
	{
		int n = info.paramBits[0];
		DspLongAccumulator a = core->regs.ac[n];
		core->regs.ac[n].bits >>= -info.ImmOperand.SignedByte;
		Flags(a, a, core->regs.ac[n]);
		core->regs.ac[n].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::LSR16(AnalyzeInfo& info)
	{
		int n = info.paramBits[0];
		DspLongAccumulator a = core->regs.ac[n];
		core->regs.ac[n].bits >>= 16;
		Flags(a, a, core->regs.ac[n]);
		core->regs.ac[n].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::M2(AnalyzeInfo& info)
	{
		core->regs.psr.im = 0;
	}

	void DspInterpreter::M0(AnalyzeInfo& info)
	{
		core->regs.psr.im = 1;
	}

	void DspInterpreter::CLR15(AnalyzeInfo& info)
	{
		core->regs.psr.dp = 0;
	}

	void DspInterpreter::SET15(AnalyzeInfo& info)
	{
		core->regs.psr.dp = 1;
	}

	void DspInterpreter::CLR40(AnalyzeInfo& info)
	{
		core->regs.psr.xl = 0;
	}

	void DspInterpreter::SET40(AnalyzeInfo& info)
	{
		core->regs.psr.xl = 1;
	}

	void DspInterpreter::MOV(AnalyzeInfo& info)
	{
		int d = info.paramBits[0];
		DspLongAccumulator a = core->regs.ac[d];
		DspLongAccumulator b = core->regs.ac[1 - d];
		core->regs.ac[d] = core->regs.ac[1 - d];
		Flags(a, b, core->regs.ac[d]);
		core->regs.ac[d].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::MOVAX(AnalyzeInfo& info)
	{
		DspLongAccumulator a = core->regs.ac[info.paramBits[0]];
		DspLongAccumulator b;
		b.sbits = (int64_t)core->regs.ax[info.paramBits[1]].sbits;
		core->regs.ac[info.paramBits[0]].sbits = (int64_t)core->regs.ax[info.paramBits[1]].sbits;
		Flags(a, b, core->regs.ac[info.paramBits[0]]);
		core->regs.ac[info.paramBits[0]].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::MOVNP(AnalyzeInfo& info)
	{
		DspLongAccumulator a = core->regs.ac[info.paramBits[0]];
		DspLongAccumulator b;
		core->PackProd(core->regs.prod);
		b.sbits = ~core->regs.prod.bitsPacked + 1;
		core->regs.ac[info.paramBits[0]].sbits = b.sbits;
		Flags(a, b, core->regs.ac[info.paramBits[0]]);
		core->regs.ac[info.paramBits[0]].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::MOVP(AnalyzeInfo& info)
	{
		DspLongAccumulator a = core->regs.ac[info.paramBits[0]];
		DspLongAccumulator b;
		core->PackProd(core->regs.prod);
		b.sbits = core->regs.prod.bitsPacked;
		core->regs.ac[info.paramBits[0]].sbits = b.sbits;
		Flags(a, b, core->regs.ac[info.paramBits[0]]);
		core->regs.ac[info.paramBits[0]].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::MOVPZ(AnalyzeInfo& info)
	{
		DspLongAccumulator a = core->regs.ac[info.paramBits[0]];
		DspLongAccumulator b;
		core->PackProd(core->regs.prod);
		b.sbits = core->regs.prod.bitsPacked;
		core->regs.ac[info.paramBits[0]].sbits = b.sbits;
		core->regs.ac[info.paramBits[0]].l = 0;
		Flags(a, b, core->regs.ac[info.paramBits[0]]);
		core->regs.ac[info.paramBits[0]].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::MOVR(AnalyzeInfo& info)
	{
		DspLongAccumulator a = core->regs.ac[info.paramBits[0]];
		DspLongAccumulator b;
		b.hm = (int32_t)(int16_t)core->MoveFromReg(info.paramBits[1]);
		b.l = 0;
		core->regs.ac[info.paramBits[0]].hm = (int32_t)(int16_t)core->MoveFromReg(info.paramBits[1]);
		core->regs.ac[info.paramBits[0]].l = 0;
		Flags(a, b, core->regs.ac[info.paramBits[0]]);
		core->regs.ac[info.paramBits[0]].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::MRR(AnalyzeInfo& info)
	{
		core->MoveToReg(info.paramBits[0], core->MoveFromReg(info.paramBits[1]));
	}

	void DspInterpreter::NEG(AnalyzeInfo& info)
	{
		int n = info.paramBits[0];
		DspLongAccumulator a = core->regs.ac[n];
		DspLongAccumulator b;
		b.sbits = -DspCore::SignExtend40(core->regs.ac[n].sbits);
		core->regs.ac[n].sbits = -DspCore::SignExtend40(core->regs.ac[n].sbits);
		Flags(a, b, core->regs.ac[n]);
		core->regs.ac[n].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::ORC(AnalyzeInfo& info)
	{
		int n = info.paramBits[0];
		DspLongAccumulator a = core->regs.ac[n];
		DspLongAccumulator b;
		b.sbits = core->regs.ac[1 - n].m;
		core->regs.ac[n].m |= core->regs.ac[1 - n].m;
		Flags(a, b, core->regs.ac[n]);
		core->regs.ac[n].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::ORI(AnalyzeInfo& info)
	{
		int n = info.paramBits[0];
		DspLongAccumulator a = core->regs.ac[n];
		DspLongAccumulator b;
		b.sbits = info.ImmOperand.UnsignedShort;
		core->regs.ac[n].m |= info.ImmOperand.UnsignedShort;
		Flags(a, b, core->regs.ac[n]);
		core->regs.ac[n].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::ORR(AnalyzeInfo& info)
	{
		int d = info.paramBits[0];
		DspLongAccumulator a = core->regs.ac[d];
		DspLongAccumulator b;
		b.sbits = core->regs.ax[info.paramBits[1]].h;
		core->regs.ac[d].m |= core->regs.ax[info.paramBits[1]].h;
		Flags(a, b, core->regs.ac[d]);
		core->regs.ac[d].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::RETcc(AnalyzeInfo& info)
	{
		if (Condition(info.cc))
		{
			core->regs.pc = core->regs.st[0].back();
			core->regs.st[0].pop_back();
		}
		else
		{
			core->regs.pc++;
		}
	}

	void DspInterpreter::RTI(AnalyzeInfo& info)
	{
		core->ReturnFromInterrupt();
	}

	void DspInterpreter::SBSET(AnalyzeInfo& info)
	{
		core->regs.psr.bits |= (1 << info.ImmOperand.Byte);
	}

	void DspInterpreter::SBCLR(AnalyzeInfo& info)
	{
		core->regs.psr.bits &= ~(1 << info.ImmOperand.Byte);
	}

	void DspInterpreter::SI(AnalyzeInfo& info)
	{
		core->dsp->WriteDMem(info.ImmOperand.Address, info.ImmOperand2.UnsignedShort);
	}

	void DspInterpreter::SR(AnalyzeInfo& info)
	{
		core->dsp->WriteDMem(info.ImmOperand.Address, core->MoveFromReg(info.paramBits[1]));
	}

	void DspInterpreter::SRR(AnalyzeInfo& info)
	{
		core->dsp->WriteDMem(core->regs.ar[info.paramBits[0]], core->MoveFromReg(info.paramBits[1]));
	}

	void DspInterpreter::SRRD(AnalyzeInfo& info)
	{
		core->dsp->WriteDMem(core->regs.ar[info.paramBits[0]], core->MoveFromReg(info.paramBits[1]));
		core->regs.ar[info.paramBits[0]]--;
	}

	void DspInterpreter::SRRI(AnalyzeInfo& info)
	{
		core->dsp->WriteDMem(core->regs.ar[info.paramBits[0]], core->MoveFromReg(info.paramBits[1]));
		core->regs.ar[info.paramBits[0]]++;
	}

	void DspInterpreter::SRRN(AnalyzeInfo& info)
	{
		core->dsp->WriteDMem(core->regs.ar[info.paramBits[0]], core->MoveFromReg(info.paramBits[1]));
		core->regs.ar[info.paramBits[0]] += core->regs.ix[info.paramBits[0]];
	}

	void DspInterpreter::SRS(AnalyzeInfo& info)
	{
		core->dsp->WriteDMem((core->regs.dpp << 8) | (uint8_t)info.ImmOperand.Address,
			core->MoveFromReg(info.paramBits[1]));
	}

	void DspInterpreter::SUB(AnalyzeInfo& info)
	{
		int d = info.paramBits[0];
		DspLongAccumulator a = core->regs.ac[d];
		DspLongAccumulator b;
		b.sbits = -DspCore::SignExtend40(core->regs.ac[1 - d].sbits);
		core->regs.ac[d].sbits = DspCore::SignExtend40(core->regs.ac[d].sbits);
		core->regs.ac[d].sbits -= DspCore::SignExtend40(core->regs.ac[1 - d].sbits);
		Flags(a, b, core->regs.ac[d]);
		core->regs.ac[d].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::SUBAX(AnalyzeInfo& info)
	{
		int64_t ax = (int64_t)core->regs.ax[info.paramBits[1]].sbits;
		DspLongAccumulator a = core->regs.ac[info.paramBits[0]];
		DspLongAccumulator b;
		b.sbits = -ax;
		core->regs.ac[info.paramBits[0]].sbits = DspCore::SignExtend40(core->regs.ac[info.paramBits[0]].sbits);
		core->regs.ac[info.paramBits[0]].sbits -= ax;
		Flags(a, b, core->regs.ac[info.paramBits[0]]);
		core->regs.ac[info.paramBits[0]].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::SUBP(AnalyzeInfo& info)
	{
		DspLongAccumulator a = core->regs.ac[info.paramBits[0]];
		DspLongAccumulator b;
		core->PackProd(core->regs.prod);
		b.sbits = ~core->regs.prod.bitsPacked + 1;
		core->regs.ac[info.paramBits[0]].sbits += b.sbits;
		Flags(a, b, core->regs.ac[info.paramBits[0]]);
		core->regs.ac[info.paramBits[0]].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::SUBR(AnalyzeInfo& info)
	{
		int64_t reg = DspCore::SignExtend16(core->MoveFromReg(info.paramBits[1]));
		DspLongAccumulator a = core->regs.ac[info.paramBits[0]];
		DspLongAccumulator b;
		b.sbits = -reg;
		core->regs.ac[info.paramBits[0]].sbits = DspCore::SignExtend40(core->regs.ac[info.paramBits[0]].sbits);
		core->regs.ac[info.paramBits[0]].sbits -= reg;
		Flags(a, b, core->regs.ac[info.paramBits[0]]);
		core->regs.ac[info.paramBits[0]].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::TST(AnalyzeInfo& info)
	{
		DspLongAccumulator zr = { 0 };
		Flags(zr, zr, core->regs.ac[info.paramBits[0]]);
	}

	void DspInterpreter::TSTAXH(AnalyzeInfo& info)
	{
		DspLongAccumulator zr = { 0 };
		DspLongAccumulator axh;
		axh.sbits = DspCore::SignExtend16(core->regs.ax[info.paramBits[0]].h);
		Flags(zr, zr, axh);
	}

	void DspInterpreter::XORI(AnalyzeInfo& info)
	{
		int n = info.paramBits[0];
		DspLongAccumulator a = core->regs.ac[n];
		DspLongAccumulator b;
		b.sbits = info.ImmOperand.UnsignedShort;
		core->regs.ac[n].m ^= info.ImmOperand.UnsignedShort;
		Flags(a, b, core->regs.ac[n]);
		core->regs.ac[n].sbits &= 0xff'ffff'ffff;
	}

	void DspInterpreter::XORR(AnalyzeInfo& info)
	{
		int d = info.paramBits[0];
		DspLongAccumulator a = core->regs.ac[d];
		DspLongAccumulator b;
		b.sbits = core->regs.ax[info.paramBits[1]].h;
		core->regs.ac[d].m ^= core->regs.ax[info.paramBits[1]].h;
		Flags(a, b, core->regs.ac[d]);
		core->regs.ac[d].sbits &= 0xff'ffff'ffff;
	}

}
