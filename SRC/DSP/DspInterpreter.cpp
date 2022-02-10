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

	void DspInterpreter::FetchMpyParams(DspParameter s1p, DspParameter s2p, int64_t& s1, int64_t& s2, bool checkDp)
	{
		switch (s1p)
		{
			case DspParameter::x0:
				s1 = core->regs.x.l;
				break;
			case DspParameter::x1:
				s1 = core->regs.x.h;
				break;
			case DspParameter::y1:
				s1 = core->regs.y.h;
				break;
			case DspParameter::a1:
				s1 = core->regs.a.m;
				break;
			case DspParameter::b1:
				s1 = core->regs.b.m;
				break;
		}

		switch (s2p)
		{
			case DspParameter::x0:
				s2 = core->regs.x.l;
				break;
			case DspParameter::x1:
				s2 = core->regs.x.h;
				break;
			case DspParameter::y0:
				s2 = core->regs.y.l;
				break;
			case DspParameter::y1:
				s2 = core->regs.y.h;
				break;
		}

		if (core->regs.psr.dp && checkDp)
		{
			if (s1p == DspParameter::x0 && s2p == DspParameter::y1)
			{
				s2 = DspCore::SignExtend16((uint16_t)s2);
			}
			else if (s1p == DspParameter::x1 && s2p == DspParameter::y0)
			{
				s1 = DspCore::SignExtend16((uint16_t)s1);
			}
			else if (s1p == DspParameter::x1 && s2p == DspParameter::y1)
			{
				s1 = DspCore::SignExtend16((uint16_t)s1);
				s2 = DspCore::SignExtend16((uint16_t)s2);
			}
		}

		if (core->regs.psr.im == 0)
		{
			s2 *= 2;
		}
	}

	void DspInterpreter::AdvanceAddress(int r, DspParameter param)
	{
		switch (param)
		{
			case DspParameter::mod_none:
				break;
			case DspParameter::mod_dec:
				core->ArAdvance(r, -1);
				break;
			case DspParameter::mod_inc:
				core->ArAdvance(r, +1);
				break;
			case DspParameter::mod_plus_m0:
				core->ArAdvance(r, core->regs.m[0]);
				break;
			case DspParameter::mod_plus_m1:
				core->ArAdvance(r, core->regs.m[1]);
				break;
			case DspParameter::mod_plus_m2:
				core->ArAdvance(r, core->regs.m[2]);
				break;
			case DspParameter::mod_plus_m3:
				core->ArAdvance(r, core->regs.m[3]);
				break;
			case DspParameter::mod_minus_m:
				core->ArAdvance(r, -core->regs.m[r]);
				break;
			case DspParameter::mod_plus_m:
				core->ArAdvance(r, core->regs.m[r]);
				break;
		}
	}

	bool DspInterpreter::ConditionTrue(ConditionCode cc)
	{
		switch (cc)
		{
			case ConditionCode::ge: return (core->regs.psr.n ^ core->regs.psr.v) == 0;
			case ConditionCode::lt: return (core->regs.psr.n ^ core->regs.psr.v) != 0;
			case ConditionCode::gt: return (core->regs.psr.z | (core->regs.psr.n ^ core->regs.psr.v)) == 0;
			case ConditionCode::le: return (core->regs.psr.z | (core->regs.psr.n ^ core->regs.psr.v)) != 0;
			case ConditionCode::nz: return core->regs.psr.z == 0;
			case ConditionCode::z: return core->regs.psr.z != 0;
			case ConditionCode::nc: return core->regs.psr.c == 0;
			case ConditionCode::c: return core->regs.psr.c != 0;
			case ConditionCode::ne: return core->regs.psr.e == 0;
			case ConditionCode::e: return core->regs.psr.e != 0;
			case ConditionCode::nm: return (core->regs.psr.z | (~core->regs.psr.u & ~core->regs.psr.e)) == 0;
			case ConditionCode::m: return (core->regs.psr.z | (~core->regs.psr.u & ~core->regs.psr.e)) != 0;
			case ConditionCode::nt: return core->regs.psr.tb == 0;
			case ConditionCode::t: return core->regs.psr.tb != 0;
			case ConditionCode::v: return core->regs.psr.v != 0;
			case ConditionCode::always: return true;
		}

		return false;
	}

	void DspInterpreter::Dispatch()
	{
		// A non-flowControl instruction can change the interpreter's internal flag (for example, when trying to access the stack registers with overflow and generating an Error interrupt).

		flowControl = info.flowControl;

		if (!info.parallel)
		{
			switch (info.instr)
			{
				case DspRegularInstruction::jmp: jmp(); break;
				case DspRegularInstruction::call: call(); break;
				case DspRegularInstruction::rets: rets(); break;
				case DspRegularInstruction::reti: reti(); break;
				case DspRegularInstruction::trap: trap(); break;
				case DspRegularInstruction::wait: wait(); break;
				case DspRegularInstruction::exec: exec(); break;
				case DspRegularInstruction::loop: loop(); break;
				case DspRegularInstruction::rep: rep(); break;
				case DspRegularInstruction::pld: pld(); break;
				case DspRegularInstruction::nop:
					break;
				case DspRegularInstruction::mr: mr(); break;
				case DspRegularInstruction::adsi: adsi(); break;
				case DspRegularInstruction::adli: adli(); break;
				case DspRegularInstruction::cmpsi: cmpsi(); break;
				case DspRegularInstruction::cmpli: cmpli(); break;
				case DspRegularInstruction::lsfi: lsfi(); break;
				case DspRegularInstruction::asfi: asfi(); break;
				case DspRegularInstruction::xorli: xorli(); break;
				case DspRegularInstruction::anli: anli(); break;
				case DspRegularInstruction::orli: orli(); break;
				case DspRegularInstruction::norm: norm(); break;
				case DspRegularInstruction::div: div(); break;
				case DspRegularInstruction::addc: addc(); break;
				case DspRegularInstruction::subc: subc(); break;
				case DspRegularInstruction::negc: negc(); break;
				case DspRegularInstruction::max: _max(); break;
				case DspRegularInstruction::lsf: lsf(); break;
				case DspRegularInstruction::asf: asf(); break;
				case DspRegularInstruction::ld: ld(); break;
				case DspRegularInstruction::st: st(); break;
				case DspRegularInstruction::ldsa: ldsa(); break;
				case DspRegularInstruction::stsa: stsa(); break;
				case DspRegularInstruction::ldla: ldla(); break;
				case DspRegularInstruction::stla: stla(); break;
				case DspRegularInstruction::mv: mv(); break;
				case DspRegularInstruction::mvsi: mvsi(); break;
				case DspRegularInstruction::mvli: mvli(); break;
				case DspRegularInstruction::stli: stli(); break;
				case DspRegularInstruction::clr: clr(); break;
				case DspRegularInstruction::set: set(); break;
				case DspRegularInstruction::btstl: btstl(); break;
				case DspRegularInstruction::btsth: btsth(); break;
			}

			core->instructionCounter++;
		}
		else
		{
			switch (info.parallelInstr)
			{
				case DspParallelInstruction::add: p_add(); break;
				case DspParallelInstruction::addl: p_addl(); break;
				case DspParallelInstruction::sub: p_sub(); break;
				case DspParallelInstruction::amv: p_amv(); break;
				case DspParallelInstruction::cmp: p_cmp(); break;
				case DspParallelInstruction::inc: p_inc(); break;
				case DspParallelInstruction::dec: p_dec(); break;
				case DspParallelInstruction::abs: p_abs(); break;
				case DspParallelInstruction::neg: p_neg(); break;
				case DspParallelInstruction::clr: p_clr(); break;
				case DspParallelInstruction::rnd: p_rnd(); break;
				case DspParallelInstruction::rndp: p_rndp(); break;
				case DspParallelInstruction::tst: p_tst(); break;
				case DspParallelInstruction::lsl16: p_lsl16(); break;
				case DspParallelInstruction::lsr16: p_lsr16(); break;
				case DspParallelInstruction::asr16: p_asr16(); break;
				case DspParallelInstruction::addp: p_addp(); break;
				case DspParallelInstruction::nop:
					break;
				case DspParallelInstruction::set: p_set(); break;
				case DspParallelInstruction::mpy: p_mpy(); break;
				case DspParallelInstruction::mac: p_mac(); break;
				case DspParallelInstruction::macn: p_macn(); break;
				case DspParallelInstruction::mvmpy: p_mvmpy(); break;
				case DspParallelInstruction::rnmpy: p_rnmpy(); break;
				case DspParallelInstruction::admpy: p_admpy(); break;
				case DspParallelInstruction::_not: p_not(); break;
				case DspParallelInstruction::_xor: p_xor(); break;
				case DspParallelInstruction::_and: p_and(); break;
				case DspParallelInstruction::_or: p_or(); break;
				case DspParallelInstruction::lsf: p_lsf(); break;
				case DspParallelInstruction::asf: p_asf(); break;
			}

			switch (info.parallelMemInstr)
			{
				case DspParallelMemInstruction::ldd: p_ldd(); break;
				case DspParallelMemInstruction::ls: p_ls(); break;
				case DspParallelMemInstruction::ld: p_ld(); break;
				case DspParallelMemInstruction::st: p_st(); break;
				case DspParallelMemInstruction::mv: p_mv(); break;
				case DspParallelMemInstruction::mr: p_mr(); break;
				case DspParallelMemInstruction::nop:
					break;
			}

			core->instructionCounter += 2;
		}

		if (core->resetInstructionCounter)
		{
			core->resetInstructionCounter = false;
			core->instructionCounter = 0;
		}

		// If there were no control transfers, increase pc by the instruction size

		if (!flowControl)
		{
			// Checking the logic of the `rep` instruction.
			// If the value of the repeat register is not equal to 0, then instead of the usual PC increment, it is not performed.

			if (core->repeatCount)
			{
				core->repeatCount--;
			}

			if (core->repeatCount == 0)
			{
				// Checking the current pc for loop is done only if the eas/lcs stack is not empty

				if (core->regs.pc == core->regs.eas->top() && !core->regs.lcs->empty())
				{
					// If pc is equal to eas then lcs = lcs - 1. 
						
					uint16_t lc;
					core->regs.lcs->pop(lc);
					core->regs.lcs->push(lc - 1);

					// If after that lcs is not equal to zero, then pc = pcs. Otherwise pop pcs/eas/lcs and pc = pc + 1 (exit the loop)

					if (core->regs.lcs->top() != 0)
					{
						core->regs.pc = core->regs.pcs->top();
					}
					else
					{
						uint16_t dummy;
						core->regs.pcs->pop(dummy);
						core->regs.eas->pop(dummy);
						core->regs.lcs->pop(dummy);

						// The DSP behaves strangely when the last loop instruction is a branch instruction. 
						// The exact work of the DSP in this case is on the verge of unpredictable behavior, so we will not bother and complicate the code. 
						// All the same, microcode developers are adequate people and will never deal with placing branch instructions at the end of a loop.

						core->regs.pc += 1;
					}
				}
				else
				{
					core->regs.pc += (DspAddress)(info.sizeInBytes >> 1);
				}
			}
		}
	}

	void DspInterpreter::ExecuteInstr()
	{
		// Fetch, decode and dispatch instruction at pc addr

		DspAddress imemAddr = core->regs.pc;

		uint8_t* imemPtr = core->dsp->TranslateIMem(imemAddr);
		if (imemPtr == nullptr)
		{
			Halt("DSP TranslateIMem failed on dsp addr: 0x%04X\n", imemAddr);
			core->dsp->Suspend();
			return;
		}

		Decoder::Decode(imemPtr, DspCore::MaxInstructionSizeInBytes, info);

		Dispatch();
	}

}
