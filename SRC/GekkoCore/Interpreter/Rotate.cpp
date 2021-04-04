// Integer Rotate Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

namespace Gekko
{

	// n = SH
	// r = ROTL(rs, n)
	// m = MASK(mb, me)
	// ra = (r & m) | (ra & ~m)
	// CR0 (if .)
	void Interpreter::rlwimi(AnalyzeInfo& info)
	{
		uint32_t m = rotmask[info.paramBits[3]][info.paramBits[4]];
		uint32_t r = Rotl32(info.paramBits[2], core->regs.gpr[info.paramBits[1]]);
		uint32_t res = (r & m) | (core->regs.gpr[info.paramBits[0]] & ~m);
		core->regs.gpr[info.paramBits[0]] = res;
		core->regs.pc += 4;
	}

	void Interpreter::rlwimi_d(AnalyzeInfo& info)
	{
		rlwimi(info);
		COMPUTE_CR0(core->regs.gpr[info.paramBits[0]]);
	}

	// n = SH
	// r = ROTL(rs, n)
	// m = MASK(MB, ME)
	// ra = r & m
	// CR0 (if .)
	void Interpreter::rlwinm(AnalyzeInfo& info)
	{
		uint32_t m = rotmask[info.paramBits[3]][info.paramBits[4]];
		uint32_t r = Rotl32(info.paramBits[2], core->regs.gpr[info.paramBits[1]]);
		uint32_t res = r & m;
		core->regs.gpr[info.paramBits[0]] = res;
		core->regs.pc += 4;
	}

	void Interpreter::rlwinm_d(AnalyzeInfo& info)
	{
		rlwinm(info);
		COMPUTE_CR0(core->regs.gpr[info.paramBits[0]]);
	}

	// n = rb[27-31]
	// r = ROTL(rs, n)
	// m = MASK(MB, ME)
	// ra = r & m
	void Interpreter::rlwnm(AnalyzeInfo& info)
	{
		uint32_t m = rotmask[info.paramBits[3]][info.paramBits[4]];
		uint32_t r = Rotl32(core->regs.gpr[info.paramBits[2]] & 0x1f, core->regs.gpr[info.paramBits[1]]);
		uint32_t res = r & m;
		core->regs.gpr[info.paramBits[0]] = res;
		core->regs.pc += 4;
	}

	void Interpreter::rlwnm_d(AnalyzeInfo& info)
	{
		rlwnm(info);
		COMPUTE_CR0(core->regs.gpr[info.paramBits[0]]);
	}

}
