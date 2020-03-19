/*

# GameCube DSP interpreter

The development idea is as follows - to do at least something (critical mass of code), then do some reverse engineering
of the microcodes and IROM and bring the emulation to an adequate state.

## Interpreter architecture

The interpreter is not involved in instruction decoding. It receives ready-made information from the analyzer (AnalyzeInfo struct).

This is a new concept of emulation of processor systems, which I decided to try on the GameCube DSP.

*/

#include "pch.h"

namespace DSP
{
	DspInterpreter::DspInterpreter(DspCore* parent)
	{
		core = parent;
	}

	DspInterpreter::~DspInterpreter()
	{
	}

	#pragma region "Top Instructions"

	void DspInterpreter::ADD(AnalyzeInfo& info)
	{
		int d = info.paramBits[0];
		core->regs.ac[d].sbits += core->regs.ac[1 - d].sbits;
		Flags(core->regs.ac[d]);
	}

	void DspInterpreter::ADDARN(AnalyzeInfo& info)
	{
		core->regs.ar[info.paramBits[0]] += core->regs.ix[info.paramBits[1]];
	}

	void DspInterpreter::ADDAX(AnalyzeInfo& info)
	{
		core->regs.ac[info.paramBits[0]].sbits += (int64_t)core->regs.ax[info.paramBits[1]].sbits;
		Flags(core->regs.ac[info.paramBits[0]]);
	}

	void DspInterpreter::ADDAXL(AnalyzeInfo& info)
	{
		core->regs.ac[info.paramBits[0]].sbits += (int64_t)(int32_t)core->regs.ax[info.paramBits[1]].l;
		Flags(core->regs.ac[info.paramBits[0]]);
	}

	void DspInterpreter::ADDI(AnalyzeInfo& info)
	{
		core->regs.ac[info.paramBits[0]].shm += (int32_t)(int16_t)info.ImmOperand.UnsignedShort;
		Flags(core->regs.ac[info.paramBits[0]]);
	}

	void DspInterpreter::ADDIS(AnalyzeInfo& info)
	{
		core->regs.ac[info.paramBits[0]].shm += (int32_t)(int16_t)info.ImmOperand.SignedByte;
		Flags(core->regs.ac[info.paramBits[0]]);
	}

	void DspInterpreter::ADDP(AnalyzeInfo& info)
	{
		DBHalt("DspInterpreter::ADDP\n");
	}

	void DspInterpreter::ADDPAXZ(AnalyzeInfo& info)
	{
		DBHalt("DspInterpreter::ADDPAXZ\n");
	}

	void DspInterpreter::ADDR(AnalyzeInfo& info)
	{
		core->regs.ac[info.paramBits[0]].sbits += (int64_t)(int32_t)(int16_t)core->MoveFromReg(info.paramBits[1]);
		Flags(core->regs.ac[info.paramBits[0]]);
	}

	void DspInterpreter::ANDC(AnalyzeInfo& info)
	{
		int n = info.paramBits[0];
		core->regs.ac[n].m &= core->regs.ac[1 - n].m;
		Flags(core->regs.ac[n]);
	}

	void DspInterpreter::TCLR(AnalyzeInfo& info)
	{
		core->regs.sr.ok = (core->regs.ac[info.paramBits[0]].m & info.ImmOperand.UnsignedShort) == 0;
	}

	void DspInterpreter::TSET(AnalyzeInfo& info)
	{
		core->regs.sr.ok = (core->regs.ac[info.paramBits[0]].m & info.ImmOperand.UnsignedShort) == info.ImmOperand.UnsignedShort;
	}

	void DspInterpreter::ANDI(AnalyzeInfo& info)
	{
		int n = info.paramBits[0];
		core->regs.ac[n].m &= info.ImmOperand.UnsignedShort;
		Flags(core->regs.ac[n]);
	}

	void DspInterpreter::ANDR(AnalyzeInfo& info)
	{
		int d = info.paramBits[0];
		core->regs.ac[d].m &= core->regs.ax[info.paramBits[1]].h;
		Flags(core->regs.ac[d]);
	}

	void DspInterpreter::BLOOP(AnalyzeInfo& info)
	{
		SetLoop(core->regs.pc + 2, info.ImmOperand.Address, core->MoveFromReg(info.paramBits[0]));
		core->regs.pc += 2;
	}

	void DspInterpreter::BLOOPI(AnalyzeInfo& info)
	{
		SetLoop(core->regs.pc + 2, info.ImmOperand2.Address, info.ImmOperand.Byte);
		core->regs.pc += 2;
	}

	void DspInterpreter::CALLcc(AnalyzeInfo& info)
	{
		if (Condition(info.cc))
		{
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
		core->regs.st[0].push_back(core->regs.pc + 1);
		core->regs.pc = core->MoveFromReg(info.paramBits[0]);
	}

	void DspInterpreter::CLR(AnalyzeInfo& info)
	{
		int n = info.paramBits[0];
		core->regs.ac[n].bits = 0;
		Flags(core->regs.ac[n]);
	}

	void DspInterpreter::CMP(AnalyzeInfo& info)
	{
		int64_t a = SignExtend40(core->regs.ac[0].sbits);
		int64_t b = SignExtend40(core->regs.ac[1].sbits);
		Flags40(a - b);
	}

	void DspInterpreter::HALT(AnalyzeInfo& info)
	{
		core->Suspend();
	}

	void DspInterpreter::IAR(AnalyzeInfo& info)
	{
		core->regs.ar[info.paramBits[0]]++;
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
		core->regs.ac[info.paramBits[0]].m = core->ReadDMem(core->regs.ar[info.paramBits[1]]);
	}

	void DspInterpreter::ILRRD(AnalyzeInfo& info)
	{
		core->regs.ac[info.paramBits[0]].m = core->ReadDMem(core->regs.ar[info.paramBits[1]]);
		core->regs.ar[info.paramBits[1]]--;
	}

	void DspInterpreter::ILRRI(AnalyzeInfo& info)
	{
		core->regs.ac[info.paramBits[0]].m = core->ReadDMem(core->regs.ar[info.paramBits[1]]);
		core->regs.ar[info.paramBits[1]]++;
	}

	void DspInterpreter::ILRRN(AnalyzeInfo& info)
	{
		core->regs.ac[info.paramBits[0]].m = core->ReadDMem(core->regs.ar[info.paramBits[1]]);
		core->regs.ar[info.paramBits[1]] += core->regs.ix[info.paramBits[1]];
	}

	void DspInterpreter::Jcc(AnalyzeInfo& info)
	{
		if (Condition(info.cc))
		{
			core->regs.pc = info.ImmOperand.Address;
		}
		else
		{
			core->regs.pc += 2;
		}
	}

	void DspInterpreter::JMPR(AnalyzeInfo& info)
	{
		core->regs.pc = core->MoveFromReg(info.paramBits[0]);
	}

	void DspInterpreter::LOOP(AnalyzeInfo& info)
	{
		SetLoop(core->regs.pc + 1, core->regs.pc + 1, core->MoveFromReg(info.paramBits[0]));
		core->regs.pc++;
	}

	void DspInterpreter::LOOPI(AnalyzeInfo& info)
	{
		SetLoop(core->regs.pc + 1, core->regs.pc + 1, info.ImmOperand.Byte);
		core->regs.pc++;
	}

	void DspInterpreter::LR(AnalyzeInfo& info)
	{
		core->MoveToReg(info.paramBits[0], info.ImmOperand.UnsignedShort);
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
		core->MoveToReg(info.paramBits[0], core->regs.ar[info.paramBits[1]]);
	}

	void DspInterpreter::LRRD(AnalyzeInfo& info)
	{
		core->MoveToReg(info.paramBits[0], core->regs.ar[info.paramBits[1]]);
		core->regs.ar[info.paramBits[1]]--;
	}

	void DspInterpreter::LRRI(AnalyzeInfo& info)
	{
		core->MoveToReg(info.paramBits[0], core->regs.ar[info.paramBits[1]]);
		core->regs.ar[info.paramBits[1]]++;
	}

	void DspInterpreter::LRRN(AnalyzeInfo& info)
	{
		core->MoveToReg(info.paramBits[0], core->regs.ar[info.paramBits[1]]);
		core->regs.ar[info.paramBits[1]] += core->regs.ix[info.paramBits[1]];
	}

	void DspInterpreter::LRS(AnalyzeInfo& info)
	{
		core->MoveToReg(info.paramBits[0], core->ReadDMem(info.ImmOperand.Address));
	}

	void DspInterpreter::LSL(AnalyzeInfo& info)
	{
		int n = info.paramBits[0];
		core->regs.ac[n].bits <<= info.ImmOperand.Byte;
		Flags(core->regs.ac[n]);
	}

	void DspInterpreter::LSL16(AnalyzeInfo& info)
	{
		int n = info.paramBits[0];
		core->regs.ac[n].bits <<= 16;
		Flags(core->regs.ac[n]);
	}

	void DspInterpreter::LSR(AnalyzeInfo& info)
	{
		int n = info.paramBits[0];
		core->regs.ac[n].bits >>= -info.ImmOperand.SignedByte;
		Flags(core->regs.ac[n]);
	}

	void DspInterpreter::LSR16(AnalyzeInfo& info)
	{
		int n = info.paramBits[0];
		core->regs.ac[n].bits >>= 16;
		Flags(core->regs.ac[n]);
	}

	void DspInterpreter::M2(AnalyzeInfo& info)
	{
		core->regs.sr.am = 0;
	}

	void DspInterpreter::M0(AnalyzeInfo& info)
	{
		core->regs.sr.am = 1;
	}

	void DspInterpreter::CLR15(AnalyzeInfo& info)
	{
		core->regs.sr.su = 0;
	}

	void DspInterpreter::SET15(AnalyzeInfo& info)
	{
		core->regs.sr.su = 1;
	}

	void DspInterpreter::CLR40(AnalyzeInfo& info)
	{
		core->regs.sr.sxm = 0;
	}

	void DspInterpreter::SET40(AnalyzeInfo& info)
	{
		core->regs.sr.sxm = 1;
	}

	void DspInterpreter::MRR(AnalyzeInfo& info)
	{
		core->MoveToReg(info.paramBits[0], core->MoveFromReg(info.paramBits[1]));
	}

	void DspInterpreter::ORC(AnalyzeInfo& info)
	{
		int n = info.paramBits[0];
		core->regs.ac[n].m |= core->regs.ac[1 - n].m;
		Flags(core->regs.ac[n]);
	}

	void DspInterpreter::ORI(AnalyzeInfo& info)
	{
		int n = info.paramBits[0];
		core->regs.ac[n].m |= info.ImmOperand.UnsignedShort;
		Flags(core->regs.ac[n]);
	}

	void DspInterpreter::ORR(AnalyzeInfo& info)
	{
		int d = info.paramBits[0];
		core->regs.ac[d].m |= core->regs.ax[info.paramBits[1]].h;
		Flags(core->regs.ac[d]);
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
		core->ReturnFromException();
	}

	void DspInterpreter::SBSET(AnalyzeInfo& info)
	{
		core->regs.sr.bits |= (1 << info.ImmOperand.Byte);
	}

	void DspInterpreter::SBCLR(AnalyzeInfo& info)
	{
		core->regs.sr.bits &= ~(1 << info.ImmOperand.Byte);
	}

	void DspInterpreter::SI(AnalyzeInfo& info)
	{
		core->WriteDMem(info.ImmOperand.Address, info.ImmOperand2.UnsignedShort);
	}

	void DspInterpreter::SR(AnalyzeInfo& info)
	{
		core->WriteDMem(info.ImmOperand.Address, core->MoveFromReg(info.paramBits[1]));
	}

	void DspInterpreter::SRR(AnalyzeInfo& info)
	{
		core->WriteDMem(core->regs.ar[info.paramBits[0]], core->MoveFromReg(info.paramBits[1]));
	}

	void DspInterpreter::SRRD(AnalyzeInfo& info)
	{
		core->WriteDMem(core->regs.ar[info.paramBits[0]], core->MoveFromReg(info.paramBits[1]));
		core->regs.ar[info.paramBits[0]]--;
	}

	void DspInterpreter::SRRI(AnalyzeInfo& info)
	{
		core->WriteDMem(core->regs.ar[info.paramBits[0]], core->MoveFromReg(info.paramBits[1]));
		core->regs.ar[info.paramBits[0]]++;
	}

	void DspInterpreter::SRRN(AnalyzeInfo& info)
	{
		core->WriteDMem(core->regs.ar[info.paramBits[0]], core->MoveFromReg(info.paramBits[1]));
		core->regs.ar[info.paramBits[0]] += core->regs.ix[info.paramBits[0]];
	}

	void DspInterpreter::SRS(AnalyzeInfo& info)
	{
		core->WriteDMem(info.ImmOperand.Address, core->MoveFromReg(info.paramBits[1]));
	}

	void DspInterpreter::TST(AnalyzeInfo& info)
	{
		Flags(core->regs.ac[info.paramBits[0]]);
	}

	void DspInterpreter::XORI(AnalyzeInfo& info)
	{
		int n = info.paramBits[0];
		core->regs.ac[n].m ^= info.ImmOperand.UnsignedShort;
		Flags(core->regs.ac[n]);
	}

	void DspInterpreter::XORR(AnalyzeInfo& info)
	{
		int d = info.paramBits[0];
		core->regs.ac[d].m ^= core->regs.ax[info.paramBits[1]].h;
		Flags(core->regs.ac[d]);
	}

	#pragma endregion "Top Instructions"


	#pragma region "Bottom Instructions"

	#pragma endregion "Bottom Instructions"

	int64_t DspInterpreter::SignExtend40(int64_t a)
	{
		if (a & 0x8000000000)
		{
			a |= 0xffffff0000000000;
		}
		return a;
	}

	bool DspInterpreter::Condition(ConditionCode cc)
	{
		switch (cc)
		{
			case ConditionCode::GE: return core->regs.sr.s == core->regs.sr.o;
			case ConditionCode::L: return core->regs.sr.s != core->regs.sr.o;
			case ConditionCode::G: return (core->regs.sr.s == core->regs.sr.o) && (core->regs.sr.z == 0);
			case ConditionCode::LE: return (core->regs.sr.s != core->regs.sr.o) && (core->regs.sr.z != 0);
			case ConditionCode::NE: return core->regs.sr.z == 0;
			case ConditionCode::EQ: return core->regs.sr.z != 0;
			case ConditionCode::NC: return core->regs.sr.o == 0;
			case ConditionCode::C: return core->regs.sr.c != 0;
			case ConditionCode::BelowS32: return core->regs.sr.as == 0;
			case ConditionCode::AboveS32: return core->regs.sr.as != 0;
			case ConditionCode::NOK: return core->regs.sr.ok == 0;
			case ConditionCode::OK: return core->regs.sr.ok != 0;
			case ConditionCode::O: return core->regs.sr.o != 0;
			case ConditionCode::Always: return true;
		}

		return false;
	}

	// 32-bit accumulator does not involved in flags calculations

	void DspInterpreter::Flags40(int64_t ac)
	{
		bool carry = (ac & 0x10000000000) != 0;
		bool zero = (ac & 0xffffffffff) == 0;
		bool aboveS32 = (ac & 0xff00000000) == 0;		// Correct?
		bool msb = (ac & 0x8000000000) != 0 ? true : false;
		bool afterMsb = (ac & 0x4000000000) != 0 ? true : false;
		bool ovf = msb ^ afterMsb ^ carry;		// Afair

		core->regs.sr.c = carry;
		core->regs.sr.o = ovf;
		core->regs.sr.z = zero;
		core->regs.sr.s = msb;
		core->regs.sr.tt = msb == afterMsb;
		core->regs.sr.as = aboveS32;
	}

	void DspInterpreter::Flags(DspLongAccumulator ac)
	{
		Flags40(SignExtend40(ac.bits));
	}

	void DspInterpreter::SetLoop(DspAddress startAddr, DspAddress endAddr, uint16_t count)
	{
		core->regs.st[0].push_back(startAddr);
		core->regs.st[2].push_back(endAddr);
		core->regs.st[3].push_back(count);
	}

	bool DspInterpreter::CheckLoop()
	{
		while (!core->regs.st[3].empty())
		{
			if (core->regs.st[3].back() != 0)
			{
				if (core->regs.pc == core->regs.st[2].back())
				{
					core->regs.pc = core->regs.st[0].back();
					core->regs.st[3].back()--;
					return true;
				}
				break;
			}
			else
			{
				core->regs.st[0].pop_back();
				core->regs.st[2].pop_back();
				core->regs.st[3].pop_back();
			}
		}

		return false;
	}

	void DspInterpreter::Dispatch(AnalyzeInfo& info)
	{
		// Test breakpoints and canaries
		if (core->IsRunning())
		{
			if (core->TestBreakpoint(core->regs.pc))
			{
				DBReport2(DbgChannel::DSP, "IMEM breakpoint at 0x%04X\n", core->regs.pc);
				core->Suspend();
				return;
			}

			core->TestCanary(core->regs.pc);
		}

		// Regular instructions ("top")
		switch (info.instr)
		{
			case DspInstruction::ADD: ADD(info); break;
			case DspInstruction::ADDARN: ADDARN(info); break;
			case DspInstruction::ADDAX: ADDAX(info); break;
			case DspInstruction::ADDAXL: ADDAXL(info); break;
			case DspInstruction::ADDI: ADDI(info); break;
			case DspInstruction::ADDIS: ADDIS(info); break;
			case DspInstruction::ADDP: ADDP(info); break;
			case DspInstruction::ADDPAXZ: ADDPAXZ(info); break;
			case DspInstruction::ADDR: ADDR(info); break;

			case DspInstruction::ANDC: ANDC(info); break;
			case DspInstruction::TCLR: TCLR(info); break;
			case DspInstruction::TSET: TSET(info); break;
			case DspInstruction::ANDI: ANDI(info); break;
			case DspInstruction::ANDR: ANDR(info); break;

			case DspInstruction::BLOOP: BLOOP(info); break;
			case DspInstruction::BLOOPI: BLOOPI(info); break;
			case DspInstruction::CALLcc: CALLcc(info); break;
			case DspInstruction::CALLR: CALLR(info); break;

			case DspInstruction::CLR: CLR(info); break;

			case DspInstruction::CMP: CMP(info); break;

			case DspInstruction::HALT: HALT(info); break;

			case DspInstruction::IAR: IAR(info); break;

			case DspInstruction::IFcc: IFcc(info); break;

			case DspInstruction::ILRR: ILRR(info); break;
			case DspInstruction::ILRRD: ILRRD(info); break;
			case DspInstruction::ILRRI: ILRRI(info); break;
			case DspInstruction::ILRRN: ILRRN(info); break;

			case DspInstruction::Jcc: Jcc(info); break;
			case DspInstruction::JMPR: JMPR(info); break;
			case DspInstruction::LOOP: LOOP(info); break;
			case DspInstruction::LOOPI: LOOPI(info); break;

			case DspInstruction::LR: LR(info); break;
			case DspInstruction::LRI: LRI(info); break;
			case DspInstruction::LRIS: LRIS(info); break;
			case DspInstruction::LRR: LRR(info); break;
			case DspInstruction::LRRD: LRRD(info); break;
			case DspInstruction::LRRI: LRRI(info); break;
			case DspInstruction::LRRN: LRRN(info); break;
			case DspInstruction::LRS: LRS(info); break;

			case DspInstruction::LSL: LSL(info); break;
			case DspInstruction::LSL16: LSL16(info); break;
			case DspInstruction::LSR: LSR(info); break;
			case DspInstruction::LSR16: LSR16(info); break;

			case DspInstruction::M2: M2(info); break;
			case DspInstruction::M0: M0(info); break;
			case DspInstruction::CLR15: CLR15(info); break;
			case DspInstruction::SET15: SET15(info); break;
			case DspInstruction::CLR40: CLR40(info); break;
			case DspInstruction::SET40: SET40(info); break;

			case DspInstruction::MRR: MRR(info); break;

			case DspInstruction::ORC: ORC(info); break;
			case DspInstruction::ORI: ORI(info); break;
			case DspInstruction::ORR: ORR(info); break;

			case DspInstruction::RETcc: RETcc(info); break;
			case DspInstruction::RTI: RTI(info); break;

			case DspInstruction::SBSET: SBSET(info); break;
			case DspInstruction::SBCLR: SBCLR(info); break;

			case DspInstruction::SI: SI(info); break;
			case DspInstruction::SR: SR(info); break;
			case DspInstruction::SRR: SRR(info); break;
			case DspInstruction::SRRD: SRRD(info); break;
			case DspInstruction::SRRI: SRRI(info); break;
			case DspInstruction::SRRN: SRRN(info); break;
			case DspInstruction::SRS: SRS(info); break;

			case DspInstruction::TST: TST(info); break;

			case DspInstruction::XORI: XORI(info); break;
			case DspInstruction::XORR: XORR(info); break;

			case DspInstruction::NOP:
			case DspInstruction::NX:
				break;

			default:
				DBHalt("DSP Unknown instruction at 0x%04X\n", core->regs.pc);
				core->Suspend();
				return;
		}

		// Packed tuple ("bottom")
		if (info.extendedOpcodePresent)
		{
			switch (info.instrEx)
			{
				case DspInstructionEx::NOP2: break;

				default:
					DBHalt("DSP Unknown packed instruction at 0x%04X\n", core->regs.pc);
					core->Suspend();
					return;
			}
		}

		if (!info.flowControl && !CheckLoop())
		{
			core->regs.pc += (DspAddress)(info.sizeInBytes >> 1);
		}
	}

	void DspInterpreter::ExecuteInstr()
	{
		AnalyzeInfo info;

		// Fetch, analyze and dispatch instruction at pc addr

		DspAddress imemAddr = core->regs.pc;

		uint8_t* imemPtr = core->TranslateIMem(imemAddr);
		if (imemPtr == nullptr)
		{
			DBHalt("DSP TranslateIMem failed on dsp addr: 0x%04X\n", imemAddr);
			return;
		}

		if (!Analyzer::Analyze(imemPtr, DspCore::MaxInstructionSizeInBytes, info))
		{
			DBHalt("DSP Analyzer failed on dsp addr: 0x%04X\n", imemAddr);
			return;
		}

		Dispatch(info);
	}

}
