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

	void DspInterpreter::Dispatch(AnalyzeInfo& info)
	{
		// A non-flowControl instruction can change the interpreter's internal flag (for example, when trying to access the stack registers with overflow and generating an Error interrupt).

		flowControl = info.flowControl;

		if (!info.parallel)
		{
			switch (info.instr)
			{
				case DspRegularInstruction::jmp: jmp(info); break;
				case DspRegularInstruction::call: call(info); break;
				case DspRegularInstruction::rets: rets(info); break;
				case DspRegularInstruction::reti: reti(info); break;
				case DspRegularInstruction::trap: trap(info); break;
				case DspRegularInstruction::wait: wait(info); break;
				case DspRegularInstruction::exec: exec(info); break;
				case DspRegularInstruction::loop: loop(info); break;
				case DspRegularInstruction::rep: rep(info); break;
				case DspRegularInstruction::pld: pld(info); break;
				case DspRegularInstruction::nop:
					break;
				case DspRegularInstruction::mr: mr(info); break;
				case DspRegularInstruction::adsi: adsi(info); break;
				case DspRegularInstruction::adli: adli(info); break;
				case DspRegularInstruction::cmpsi: cmpsi(info); break;
				case DspRegularInstruction::cmpli: cmpli(info); break;
				case DspRegularInstruction::lsfi: lsfi(info); break;
				case DspRegularInstruction::asfi: asfi(info); break;
				case DspRegularInstruction::xorli: xorli(info); break;
				case DspRegularInstruction::anli: anli(info); break;
				case DspRegularInstruction::orli: orli(info); break;
				case DspRegularInstruction::norm: norm(info); break;
				case DspRegularInstruction::div: div(info); break;
				case DspRegularInstruction::addc: addc(info); break;
				case DspRegularInstruction::subc: subc(info); break;
				case DspRegularInstruction::negc: negc(info); break;
				case DspRegularInstruction::max: _max(info); break;
				case DspRegularInstruction::lsf: lsf(info); break;
				case DspRegularInstruction::asf: asf(info); break;
				case DspRegularInstruction::ld: ld(info); break;
				case DspRegularInstruction::st: st(info); break;
				case DspRegularInstruction::ldsa: ldsa(info); break;
				case DspRegularInstruction::stsa: stsa(info); break;
				case DspRegularInstruction::ldla: ldla(info); break;
				case DspRegularInstruction::stla: stla(info); break;
				case DspRegularInstruction::mv: mv(info); break;
				case DspRegularInstruction::mvsi: mvsi(info); break;
				case DspRegularInstruction::mvli: mvli(info); break;
				case DspRegularInstruction::stli: stli(info); break;
				case DspRegularInstruction::clr: clr(info); break;
				case DspRegularInstruction::set: set(info); break;
				case DspRegularInstruction::btstl: btstl(info); break;
				case DspRegularInstruction::btsth: btsth(info); break;
			}
		}
		else
		{
			switch (info.parallelInstr)
			{
				case DspParallelInstruction::add: p_add(info); break;
				case DspParallelInstruction::addl: p_addl(info); break;
				case DspParallelInstruction::sub: p_sub(info); break;
				case DspParallelInstruction::amv: p_amv(info); break;
				case DspParallelInstruction::cmp: p_cmp(info); break;
				case DspParallelInstruction::inc: p_inc(info); break;
				case DspParallelInstruction::dec: p_dec(info); break;
				case DspParallelInstruction::abs: p_abs(info); break;
				case DspParallelInstruction::neg: p_neg(info); break;
				case DspParallelInstruction::clr: p_clr(info); break;
				case DspParallelInstruction::rnd: p_rnd(info); break;
				case DspParallelInstruction::rndp: p_rndp(info); break;
				case DspParallelInstruction::tst: p_tst(info); break;
				case DspParallelInstruction::lsl16: p_lsl16(info); break;
				case DspParallelInstruction::lsr16: p_lsr16(info); break;
				case DspParallelInstruction::asr16: p_asr16(info); break;
				case DspParallelInstruction::addp: p_addp(info); break;
				case DspParallelInstruction::nop:
					break;
				case DspParallelInstruction::set: p_set(info); break;
				case DspParallelInstruction::mpy: p_mpy(info); break;
				case DspParallelInstruction::mac: p_mac(info); break;
				case DspParallelInstruction::macn: p_macn(info); break;
				case DspParallelInstruction::mvmpy: p_mvmpy(info); break;
				case DspParallelInstruction::rnmpy: p_rnmpy(info); break;
				case DspParallelInstruction::admpy: p_admpy(info); break;
				case DspParallelInstruction::_not: p_not(info); break;
				case DspParallelInstruction::_xor: p_xor(info); break;
				case DspParallelInstruction::_and: p_and(info); break;
				case DspParallelInstruction::_or: p_or(info); break;
				case DspParallelInstruction::lsf: p_lsf(info); break;
				case DspParallelInstruction::asf: p_asf(info); break;
			}

			switch (info.parallelMemInstr)
			{
				case DspParallelMemInstruction::ldd: p_ldd(info); break;
				case DspParallelMemInstruction::ls: p_ls(info); break;
				case DspParallelMemInstruction::ld: p_ld(info); break;
				case DspParallelMemInstruction::st: p_st(info); break;
				case DspParallelMemInstruction::mv: p_mv(info); break;
				case DspParallelMemInstruction::mr: p_mr(info); break;
				case DspParallelMemInstruction::nop:
					break;
			}
		}

		// If there were no control transfers, increase pc by the instruction size

		if (!flowControl)
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

		Analyzer::Analyze(imemPtr, DspCore::MaxInstructionSizeInBytes, info);

		Dispatch(info);
	}

}
