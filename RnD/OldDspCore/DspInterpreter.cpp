// GameCube DSP interpreter
#include "pch.h"

using namespace Debug;

namespace DSP
{
	DspInterpreter::DspInterpreter(DspCore* parent)
	{
		core = parent;
	}

	DspInterpreter::~DspInterpreter()
	{
	}

	bool DspInterpreter::Condition(ConditionCode cc)
	{
		switch (cc)
		{
			case ConditionCode::GE: return (core->regs.psr.n ^ core->regs.psr.v) == 0;
			case ConditionCode::LT: return (core->regs.psr.n ^ core->regs.psr.v) != 0;
			case ConditionCode::GT: return (core->regs.psr.z | (core->regs.psr.n ^ core->regs.psr.v)) == 0;
			case ConditionCode::LE: return (core->regs.psr.z | (core->regs.psr.n ^ core->regs.psr.v)) != 0;
			case ConditionCode::NZ: return core->regs.psr.z == 0;
			case ConditionCode::Z: return core->regs.psr.z != 0;
			case ConditionCode::NC: return core->regs.psr.c == 0;
			case ConditionCode::C: return core->regs.psr.c != 0;
			case ConditionCode::NE: return core->regs.psr.e == 0;
			case ConditionCode::E: return core->regs.psr.e != 0;
			case ConditionCode::NM: return (core->regs.psr.z | (~core->regs.psr.u & ~core->regs.psr.e)) == 0;
			case ConditionCode::M: return (core->regs.psr.z | (~core->regs.psr.u & ~core->regs.psr.e)) != 0;
			case ConditionCode::NT: return core->regs.psr.tb == 0;
			case ConditionCode::T: return core->regs.psr.tb != 0;
			case ConditionCode::V: return core->regs.psr.v != 0;
			case ConditionCode::Always: return true;
		}

		return false;
	}

	// 32-bit accumulator does not involved in flags calculations

	void DspInterpreter::Flags40(int64_t a, int64_t b, int64_t res)
	{
		bool carry = (res & 0x10000000000) != 0;
		bool zero = (res & 0xffffffffff) == 0;
		bool aboveS32 = (res & 0xff00000000) == 0;		// Correct?
		bool msb = (res & 0x8000000000) != 0 ? true : false;
		bool afterMsb = (res & 0x4000000000) != 0 ? true : false;
		// http://teaching.idallen.com/dat2343/10f/notes/040_overflow.txt
		//1. If the sum of two numbers with the sign bits off yields a result number
		//   with the sign bit on, the "overflow" flag is turned on.
		//   0100 + 0100 = 1000 (overflow flag is turned on)
		//2. If the sum of two numbers with the sign bits on yields a result number
		//   with the sign bit off, the "overflow" flag is turned on.
		//   1000 + 1000 = 0000 (overflow flag is turned on)
		//A human need only remember that, when doing signed math, adding
		//two numbers of the same sign must produce a result of the same sign,
		//otherwise overflow happened.
		bool aMsb = (a & 0x8000000000) != 0 ? true : false;
		bool bMsb = (b & 0x8000000000) != 0 ? true : false;
		bool ovf = false;
		if (aMsb == bMsb)
		{
			ovf = aMsb != msb;
		}

		core->regs.psr.c = carry;
		core->regs.psr.v = ovf;
		core->regs.psr.z = zero;
		core->regs.psr.n = msb;
		core->regs.psr.e = aboveS32;
		core->regs.psr.u = msb == afterMsb;
	}

	void DspInterpreter::Flags(DspLongAccumulator a, DspLongAccumulator b, DspLongAccumulator res)
	{
		Flags40(DspCore::SignExtend40(a.bits), DspCore::SignExtend40(b.bits), DspCore::SignExtend40(res.bits));
	}

	void DspInterpreter::FlagsLogic(DspLongAccumulator a)
	{

	}

	void DspInterpreter::SetLoop(DspAddress startAddr, DspAddress endAddr, uint16_t count)
	{
		core->regs.st[0].push_back(startAddr);
		core->regs.st[2].push_back(endAddr);
		core->regs.st[3].push_back(count - 1);
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
		// Regular instructions ("top")
		switch (info.instr)
		{
			case DspInstruction::ABS: ABS(info); break;

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

			case DspInstruction::ASL: ASL(info); break;
			case DspInstruction::ASR: ASR(info); break;
			case DspInstruction::ASR16: ASR16(info); break;

			case DspInstruction::BLOOP: BLOOP(info); break;
			case DspInstruction::BLOOPI: BLOOPI(info); break;
			case DspInstruction::CALLcc: CALLcc(info); break;
			case DspInstruction::CALLR: CALLR(info); break;

			case DspInstruction::CLR: CLR(info); break;
			case DspInstruction::CLRL: CLRL(info); break;
			case DspInstruction::CLRP: CLRP(info); break;

			case DspInstruction::CMP: CMP(info); break;
			case DspInstruction::CMPI: CMPI(info); break;
			case DspInstruction::CMPIS: CMPIS(info); break;
			case DspInstruction::CMPAR: CMPAR(info); break;

			case DspInstruction::DAR: DAR(info); break;
			case DspInstruction::DEC: DEC(info); break;
			case DspInstruction::DECM: DECM(info); break;

			case DspInstruction::HALT: HALT(info); break;

			case DspInstruction::IAR: IAR(info); break;
			case DspInstruction::INC: INC(info); break;
			case DspInstruction::INCM: INCM(info); break;

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

			case DspInstruction::MOV: MOV(info); break;
			case DspInstruction::MOVAX: MOVAX(info); break;
			case DspInstruction::MOVNP: MOVNP(info); break;
			case DspInstruction::MOVP: MOVP(info); break;
			case DspInstruction::MOVPZ: MOVPZ(info); break;
			case DspInstruction::MOVR: MOVR(info); break;
			case DspInstruction::MRR: MRR(info); break;

			case DspInstruction::NEG: NEG(info); break;

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

			case DspInstruction::SUB: SUB(info); break;
			case DspInstruction::SUBAX: SUBAX(info); break;
			case DspInstruction::SUBP: SUBP(info); break;
			case DspInstruction::SUBR: SUBR(info); break;

			case DspInstruction::TST: TST(info); break;
			case DspInstruction::TSTAXH: TSTAXH(info); break;

			case DspInstruction::XORI: XORI(info); break;
			case DspInstruction::XORR: XORR(info); break;

			case DspInstruction::MUL: MUL(info); break;
			case DspInstruction::MULC: MULC(info); break;
			case DspInstruction::MULX: MULX(info); break;

			case DspInstruction::MADD: MADD(info); break;
			case DspInstruction::MADDC: MADDC(info); break;
			case DspInstruction::MADDX: MADDX(info); break;

			case DspInstruction::MSUB: MSUB(info); break;
			case DspInstruction::MSUBC: MSUBC(info); break;
			case DspInstruction::MSUBX: MSUBX(info); break;

			case DspInstruction::MULAC: MULAC(info); break;
			case DspInstruction::MULCAC: MULCAC(info); break;
			case DspInstruction::MULXAC: MULXAC(info); break;

			case DspInstruction::MULMV: MULMV(info); break;
			case DspInstruction::MULCMV: MULCMV(info); break;
			case DspInstruction::MULXMV: MULXMV(info); break;

			case DspInstruction::MULMVZ: MULMVZ(info); break;
			case DspInstruction::MULCMVZ: MULCMVZ(info); break;
			case DspInstruction::MULXMVZ: MULXMVZ(info); break;

			case DspInstruction::NOP:
			case DspInstruction::NX:
				break;

			default:
				Report(Channel::DSP, "DSP Unknown instruction at 0x%04X\n", core->regs.pc);
				core->dsp->Suspend();
				return;
		}

		// Packed tuple ("bottom")
		if (info.extendedOpcodePresent)
		{
			switch (info.instrEx)
			{
				case DspInstructionEx::DR: DR(info); break;
				case DspInstructionEx::IR: IR(info); break;
				case DspInstructionEx::NR: NR(info); break;
				case DspInstructionEx::MV: MV(info); break;
				case DspInstructionEx::S: S(info); break;
				case DspInstructionEx::SN: SN(info); break;
				case DspInstructionEx::L: L(info); break;
				case DspInstructionEx::LN: LN(info); break;

				case DspInstructionEx::LS: LS(info); break;
				case DspInstructionEx::SL: SL(info); break;
				case DspInstructionEx::LSN: LSN(info); break;
				case DspInstructionEx::SLN: SLN(info); break;
				case DspInstructionEx::LSM: LSM(info); break;
				case DspInstructionEx::SLM: SLM(info); break;
				case DspInstructionEx::LSNM: LSNM(info); break;
				case DspInstructionEx::SLNM: SLNM(info); break;

				case DspInstructionEx::LD: LD(info); break;
				case DspInstructionEx::LDN: LDN(info); break;
				case DspInstructionEx::LDM: LDM(info); break;
				case DspInstructionEx::LDNM: LDNM(info); break;

				case DspInstructionEx::LDAX: LDAX(info); break;
				case DspInstructionEx::LDAXN: LDAXN(info); break;
				case DspInstructionEx::LDAXM: LDAXM(info); break;
				case DspInstructionEx::LDAXNM: LDAXNM(info); break;

				case DspInstructionEx::NOP2: break;

				default:
					Report(Channel::DSP, "DSP Unknown packed instruction at 0x%04X\n", core->regs.pc);
					core->dsp->Suspend();
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

		uint8_t* imemPtr = core->dsp->TranslateIMem(imemAddr);
		if (imemPtr == nullptr)
		{
			Halt("DSP TranslateIMem failed on dsp addr: 0x%04X\n", imemAddr);
			core->dsp->Suspend();
			return;
		}

		if (!Analyzer::Analyze(imemPtr, DspCore::MaxInstructionSizeInBytes, info))
		{
			Halt("DSP Analyzer failed on dsp addr: 0x%04X\n", imemAddr);
			core->dsp->Suspend();
			return;
		}

		Dispatch(info);
	}

}
