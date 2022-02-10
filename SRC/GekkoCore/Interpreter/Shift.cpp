// Integer Shift Instructions
#include "../pch.h"
#include "InterpreterPrivate.h"

namespace Gekko
{

	// We use macro programming to compress the source code.
	// Now I am not very willing to use such things.

	#define GPR(n) (core->regs.gpr[info.paramBits[(n)]])

	// n = rb[27-31]
	// r = ROTL(rs, n)
	// if rb[26] = 0
	// then m = MASK(0, 31-n)
	// else m = (32)0
	// ra = r & m
	// (simply : ra = rs << rb, or ra = 0, if rb[26] = 1)
	void Interpreter::slw()
	{
		uint32_t n = GPR(2);

		uint32_t res;

		if (n & 0x20) res = 0;
		else res = GPR(1) << (n & 0x1f);

		GPR(0) = res;
		core->regs.pc += 4;
	}

	void Interpreter::slw_d()
	{
		slw();
		COMPUTE_CR0(GPR(0));
	}

	// n = rb[27-31]
	// r = ROTL(rs, 32-n)
	// if rb[26] = 0
	// then m = MASK(n, 31)
	// else m = (32)0
	// S = rs(0)
	// ra = r & m | (32)S & ~m
	// XER[CA] = S & (r & ~m[0-31] != 0)
	void Interpreter::sraw()
	{
		uint32_t n = GPR(2);
		int32_t res;
		int32_t src = GPR(1);

		if (n == 0)
		{
			res = src;
			RESET_XER_CA();
		}
		else if (n & 0x20)
		{
			if (src < 0)
			{
				res = 0xffffffff;
				if (src & 0x7fffffff) SET_XER_CA(); else RESET_XER_CA();
			}
			else
			{
				res = 0;
				RESET_XER_CA();
			}
		}
		else
		{
			n = n & 0x1f;
			res = (int32_t)src >> n;
			if (src < 0 && (src << (32 - n)) != 0) SET_XER_CA(); else RESET_XER_CA();
		}

		GPR(0) = res;
		core->regs.pc += 4;
	}

	void Interpreter::sraw_d()
	{
		sraw();
		COMPUTE_CR0(GPR(0));
	}

	// n = SH
	// r = ROTL(rs, 32 - n)
	// m = MASK(n, 31)
	// sign = rs[0]
	// ra = r & m | (32)sign & ~m
	// XER[CA] = sign(0) & ((r & ~m) != 0)
	void Interpreter::srawi()
	{
		uint32_t n = info.paramBits[2];
		int32_t res;
		int32_t src = GPR(1);

		if (n == 0)
		{
			res = src;
			RESET_XER_CA();
		}
		else
		{
			res = src >> n;
			if (src < 0 && (src << (32 - n)) != 0) SET_XER_CA(); else RESET_XER_CA();
		}

		GPR(0) = res;
		core->regs.pc += 4;
	}

	void Interpreter::srawi_d()
	{
		srawi();
		COMPUTE_CR0(GPR(0));
	}

	// n = rb[27-31]
	// r = ROTL(rs, 32-n)
	// if rb[26] = 0
	// then m = MASK(n, 31)
	// else m = (32)0
	// ra = r & m
	// (simply : ra = rs >> rb, or ra = 0, if rb[26] = 1)
	void Interpreter::srw()
	{
		uint32_t n = GPR(2);

		uint32_t res;

		if (n & 0x20) res = 0;
		else res = GPR(1) >> (n & 0x1f);

		GPR(0) = res;
		core->regs.pc += 4;
	}

	void Interpreter::srw_d()
	{
		srw();
		COMPUTE_CR0(GPR(0));
	}

}
