// Integer Shift Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

namespace Gekko
{

	// n = rb[27-31]
	// r = ROTL(rs, n)
	// if rb[26] = 0
	// then m = MASK(0, 31-n)
	// else m = (32)0
	// ra = r & m
	// (simply : ra = rs << rb, or ra = 0, if rb[26] = 1)
	void Interpreter::slw(AnalyzeInfo& info)
	{
		uint32_t n = core->regs.gpr[info.paramBits[2]];

		uint32_t res;

		if (n & 0x20) res = 0;
		else res = core->regs.gpr[info.paramBits[1]] << (n & 0x1f);

		core->regs.gpr[info.paramBits[0]] = res;
		core->regs.pc += 4;
	}

	void Interpreter::slw_d(AnalyzeInfo& info)
	{
		slw(info);
		COMPUTE_CR0(core->regs.gpr[info.paramBits[0]]);
	}

	// n = rb[27-31]
	// r = ROTL(rs, 32-n)
	// if rb[26] = 0
	// then m = MASK(n, 31)
	// else m = (32)0
	// S = rs(0)
	// ra = r & m | (32)S & ~m
	// XER[CA] = S & (r & ~m[0-31] != 0)
	void Interpreter::sraw(AnalyzeInfo& info)
	{
		uint32_t n = core->regs.gpr[info.paramBits[2]];
		int32_t res;
		int32_t src = core->regs.gpr[info.paramBits[1]];

		if (n == 0)
		{
			res = src;
			RESET_XER_CA;
		}
		else if (n & 0x20)
		{
			if (src < 0)
			{
				res = 0xffffffff;
				if (src & 0x7fffffff) SET_XER_CA; else RESET_XER_CA;
			}
			else
			{
				res = 0;
				RESET_XER_CA;
			}
		}
		else
		{
			n = n & 0x1f;
			res = (int32_t)src >> n;
			if (src < 0 && (src << (32 - n)) != 0) SET_XER_CA; else RESET_XER_CA;
		}

		core->regs.gpr[info.paramBits[0]] = res;
		core->regs.pc += 4;
	}

	void Interpreter::sraw_d(AnalyzeInfo& info)
	{
		sraw(info);
		COMPUTE_CR0(core->regs.gpr[info.paramBits[0]]);
		Gekko->regs.pc += 4;
	}

	// n = SH
	// r = ROTL(rs, 32 - n)
	// m = MASK(n, 31)
	// sign = rs[0]
	// ra = r & m | (32)sign & ~m
	// XER[CA] = sign(0) & ((r & ~m) != 0)
	void Interpreter::srawi(AnalyzeInfo& info)
	{
		uint32_t n = info.paramBits[2];
		int32_t res;
		int32_t src = core->regs.gpr[info.paramBits[1]];

		if (n == 0)
		{
			res = src;
			RESET_XER_CA;
		}
		else
		{
			res = src >> n;
			if (src < 0 && (src << (32 - n)) != 0) SET_XER_CA; else RESET_XER_CA;
		}

		core->regs.gpr[info.paramBits[0]] = res;
		core->regs.pc += 4;
	}

	void Interpreter::srawi_d(AnalyzeInfo& info)
	{
		srawi(info);
		COMPUTE_CR0(core->regs.gpr[info.paramBits[0]]);
	}

	// n = rb[27-31]
	// r = ROTL(rs, 32-n)
	// if rb[26] = 0
	// then m = MASK(n, 31)
	// else m = (32)0
	// ra = r & m
	// (simply : ra = rs >> rb, or ra = 0, if rb[26] = 1)
	void Interpreter::srw(AnalyzeInfo& info)
	{
		uint32_t n = core->regs.gpr[info.paramBits[2]];

		uint32_t res;

		if (n & 0x20) res = 0;
		else res = core->regs.gpr[info.paramBits[1]] >> (n & 0x1f);

		core->regs.gpr[info.paramBits[0]] = res;
		core->regs.pc += 4;
	}

	void Interpreter::srw_d(AnalyzeInfo& info)
	{
		srw(info);
		COMPUTE_CR0(core->regs.gpr[info.paramBits[0]]);
	}

}
