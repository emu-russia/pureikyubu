// DSP multiply instructions

// The Duddie documentation describing multiplication instructions everywhere states that operands are treated as signed 16-bit numbers.
// But there are also mentioned about 2 control bits of the status register:
// - Bit 15 (SU): Operands are signed (1 = unsigned)
// - Bit 13 (AM): Product multiply result by 2 (when AM = 0) 

// Did not notice that microcodes check flags after multiplication operations, so leave flags for now..

#include "pch.h"

namespace DSP
{
#pragma region "Multiplier Instructions"

	void DspInterpreter::Mul(int16_t a, int16_t b)
	{
		if (core->regs.sr.su)
		{
			b <<= 1;
		}
		core->regs.prod = DspCore::Muls(a, b);
	}

	void DspInterpreter::Madd(int16_t a, int16_t b)
	{
		if (core->regs.sr.su)
		{
			b <<= 1;
		}
		DspProduct temp = DspCore::Muls(a, b);
		
		DspCore::PackProd(core->regs.prod);
		DspCore::PackProd(temp);
		core->regs.prod.bitsPacked += temp.bitsPacked;
		DspCore::UnpackProd(core->regs.prod);
	}

	void DspInterpreter::Msub(int16_t a, int16_t b)
	{
		if (core->regs.sr.su)
		{
			b <<= 1;
		}
		DspProduct temp = DspCore::Muls(a, b);

		DspCore::PackProd(core->regs.prod);
		DspCore::PackProd(temp);
		core->regs.prod.bitsPacked -= temp.bitsPacked;
		DspCore::UnpackProd(core->regs.prod);
	}

	void DspInterpreter::Mulac(int16_t a, int16_t b, int r)
	{
		if (core->regs.sr.su)
		{
			b <<= 1;
		}

		DspCore::PackProd(core->regs.prod);
		core->regs.ac[r].sbits += core->regs.prod.bitsPacked;
		core->regs.ac[r].sbits &= 0xff'ffff'ffff;

		core->regs.prod = DspCore::Muls(a, b);
	}

	void DspInterpreter::Mulmv(int16_t a, int16_t b, int r)
	{
		if (core->regs.sr.su)
		{
			b <<= 1;
		}

		DspCore::PackProd(core->regs.prod);
		core->regs.ac[r].sbits = core->regs.prod.bitsPacked;
		core->regs.ac[r].sbits &= 0xff'ffff'ffff;

		core->regs.prod = DspCore::Muls(a, b);
	}

	void DspInterpreter::Mulmvz(int16_t a, int16_t b, int r)
	{
		if (core->regs.sr.su)
		{
			b <<= 1;
		}

		DspCore::PackProd(core->regs.prod);
		core->regs.ac[r].sbits = core->regs.prod.bitsPacked;
		core->regs.ac[r].sbits &= 0xff'ffff'0000;

		core->regs.prod = DspCore::Muls(a, b);
	}

	// MUL

	void DspInterpreter::MUL(AnalyzeInfo& info)
	{
		// Multiply $axS.l by high part $axS.h (treat them both as signed).

		Mul(core->regs.ax[info.paramBits[0]].l, core->regs.ax[info.paramBits[0]].h);
	}

	void DspInterpreter::MULC(AnalyzeInfo& info)
	{
		// Multiply mid part of accumulator register $acS.m by $axT.h (treat them both as signed).

		Mul(core->regs.ac[info.paramBits[0]].m, core->regs.ax[info.paramBits[1]].h);
	}

	void DspInterpreter::MULX(AnalyzeInfo& info)
	{
		// Multiply one part $ax0 by one part $ax1 (treat them both as signed). Part is selected by S and T bits. Zero selects low part, one selects high part.
		
		int16_t a = info.paramBits[0] ? core->regs.ax[0].h : core->regs.ax[0].l;
		int16_t b = info.paramBits[1] ? core->regs.ax[1].h : core->regs.ax[1].l;
		Mul(a, b);
	}

	// MADD

	void DspInterpreter::MADD(AnalyzeInfo& info)
	{
		// Multiply $axS.l by $axS.h (treat them both as signed)
		// and add result to product register. 

		Madd(core->regs.ax[info.paramBits[0]].l, core->regs.ax[info.paramBits[0]].h);
	}

	void DspInterpreter::MADDC(AnalyzeInfo& info)
	{
		// Multiply middle part of accumulator $acS.m by $axT.h (treat them both as signed)
		// and add result to product register.

		Madd(core->regs.ac[info.paramBits[0]].m, core->regs.ax[info.paramBits[1]].h);
	}

	void DspInterpreter::MADDX(AnalyzeInfo& info)
	{
		// Multiply one part of $ax0 (selected by S) by one part of $ax1 (selected by T) (treat them both as signed)
		// and add result to product register. 

		int16_t a = info.paramBits[0] ? core->regs.ax[0].h : core->regs.ax[0].l;
		int16_t b = info.paramBits[1] ? core->regs.ax[1].h : core->regs.ax[1].l;
		Madd(a, b);
	}

	// MSUB

	void DspInterpreter::MSUB(AnalyzeInfo& info)
	{
		// Multiply $axS.l by $axS.h (treat them both as signed)
		// and subtract result from product register. 

		Msub(core->regs.ax[info.paramBits[0]].l, core->regs.ax[info.paramBits[0]].h);
	}

	void DspInterpreter::MSUBC(AnalyzeInfo& info)
	{
		// Multiply middle part of accumulator $acS.m by $axT.h (treat them both as signed) 
		// and subtract result from product register.

		Msub(core->regs.ac[info.paramBits[0]].m, core->regs.ax[info.paramBits[1]].h);
	}

	void DspInterpreter::MSUBX(AnalyzeInfo& info)
	{
		// Multiply one part of $ax0 (selected by S) by one part of $ax1 (selected by T) (treat them both as signed) 
		// and subtract result from product register. 

		int16_t a = info.paramBits[0] ? core->regs.ax[0].h : core->regs.ax[0].l;
		int16_t b = info.paramBits[1] ? core->regs.ax[1].h : core->regs.ax[1].l;
		Msub(a, b);
	}

	// AC

	void DspInterpreter::MULAC(AnalyzeInfo& info)
	{
		// Add product register to accumulator register $acR.
		// Multiply $axS.l by $axS.h (treat them both as signed).

		Mulac(core->regs.ax[info.paramBits[0]].l, core->regs.ax[info.paramBits[0]].h, info.paramBits[2]);
	}

	void DspInterpreter::MULCAC(AnalyzeInfo& info)
	{
		// Add product register before multiplication to accumulator $acR.
		// Multiply mid part of accumulator register $acS.m by $axT.h (treat them both as signed).

		Mulac(core->regs.ac[info.paramBits[0]].m, core->regs.ax[info.paramBits[1]].h, info.paramBits[2]);
	}

	void DspInterpreter::MULXAC(AnalyzeInfo& info)
	{
		// Add product register to accumulator register $acR. 
		// Multiply one part $ax0 by one part $ax1 (treat them both as signed). Part is selected by S and T bits. Zero selects low part, one selects high part. 

		int16_t a = info.paramBits[0] ? core->regs.ax[0].h : core->regs.ax[0].l;
		int16_t b = info.paramBits[1] ? core->regs.ax[1].h : core->regs.ax[1].l;
		Mulac(a, b, info.paramBits[2]);
	}

	// MV

	void DspInterpreter::MULMV(AnalyzeInfo& info)
	{
		// Move product register to accumulator register $acR.
		// Multiply $axS.l by $axS.h (treat them both as signed). 

		Mulmv(core->regs.ax[info.paramBits[0]].l, core->regs.ax[info.paramBits[0]].h, info.paramBits[2]);
	}

	void DspInterpreter::MULCMV(AnalyzeInfo& info)
	{
		// Move product register before multiplication to accumulator $acR.
		// Multiply mid part of accumulator register $acS.m by $axT.h (treat them both as signed).

		Mulmv(core->regs.ac[info.paramBits[0]].m, core->regs.ax[info.paramBits[1]].h, info.paramBits[2]);
	}

	void DspInterpreter::MULXMV(AnalyzeInfo& info)
	{
		// Move product register to accumulator register $acR.
		// Multiply one part $ax0 by one part $ax1 (treat them both as signed). Part is selected by Sand T bits. Zero selects low part, one selects high part.

		int16_t a = info.paramBits[0] ? core->regs.ax[0].h : core->regs.ax[0].l;
		int16_t b = info.paramBits[1] ? core->regs.ax[1].h : core->regs.ax[1].l;
		Mulmv(a, b, info.paramBits[2]);
	}

	// MVZ

	void DspInterpreter::MULMVZ(AnalyzeInfo& info)
	{
		// Move product register to accumulator register $acR and clear low part of accumulator register $acR.l.
		// Multiply $axS.l by $axS.h (treat them both as signed).

		Mulmvz(core->regs.ax[info.paramBits[0]].l, core->regs.ax[info.paramBits[0]].h, info.paramBits[2]);
	}

	void DspInterpreter::MULCMVZ(AnalyzeInfo& info)
	{
		// Move product register before multiplication to accumulator $acR. Set low part of accumulator $acR.l to zero. 
		// Multiply mid part of accumulator register $acS.m by $axT.h (treat them both as signed). 

		Mulmvz(core->regs.ac[info.paramBits[0]].m, core->regs.ax[info.paramBits[1]].h, info.paramBits[2]);
	}

	void DspInterpreter::MULXMVZ(AnalyzeInfo& info)
	{
		// Move product register to accumulator register $acR and clear low part of accumulator register $acR.l. 
		// Multiply one part $ax0 by one part $ax1 (treat them both as signed). Part is selected by S and T bits. Zero selects low part, one selects high part.

		int16_t a = info.paramBits[0] ? core->regs.ax[0].h : core->regs.ax[0].l;
		int16_t b = info.paramBits[1] ? core->regs.ax[1].h : core->regs.ax[1].l;
		Mulmvz(a, b, info.paramBits[2]);
	}

#pragma region "Multiplier Instructions"
}
