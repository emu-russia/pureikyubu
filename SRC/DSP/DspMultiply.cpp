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

	// MUL

	void DspInterpreter::MUL(AnalyzeInfo& info)
	{
		// Multiply low part $axS.l of secondary accumulator $axS by high part $axS.h of secondary accumulator $axS (treat them both as signed). 
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MULC(AnalyzeInfo& info)
	{
		// Multiply mid part of accumulator register $acS.m by high part $axS.h of secondary accumulator $axS (treat them both as signed).
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MULX(AnalyzeInfo& info)
	{
		// Multiply one part $ax0 by one part $ax1 (treat them both as signed). Part is selected by S and T bits. Zero selects low part, one selects high part.
		int64_t a = DspCore::SignExtend16(info.paramBits[0] ? core->regs.ax[0].h : core->regs.ax[0].l);
		int64_t b = DspCore::SignExtend16(info.paramBits[1] ? core->regs.ax[1].h : core->regs.ax[1].l);
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	// MADD

	void DspInterpreter::MADD(AnalyzeInfo& info)
	{
		// Multiply low part $axS.l of secondary accumulator $axS by high part $axS.h of secondary accumulator $axS (treat them both as signed)
		// and add result to product register. 
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MADDC(AnalyzeInfo& info)
	{
		// Multiply middle part of accumulator $acS.m by high part of secondary accumulator $axT.h (treat them both as signed)
		// and add result to product register.
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MADDX(AnalyzeInfo& info)
	{
		// Multiply one part of secondary accumulator $ax0 (selected by S) by one part of secondary accumulator $ax1 (selected by T) (treat them both as signed)
		// and add result to product register. 
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	// MSUB

	void DspInterpreter::MSUB(AnalyzeInfo& info)
	{
		// Multiply low part $axS.l of secondary accumulator $axS by high part $axS.h of secondary accumulator $axS (treat them both as signed)
		// and subtract result from product register. 
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MSUBC(AnalyzeInfo& info)
	{
		// Multiply middle part of accumulator $acS.m by high part of secondary accumulator $axT.h(treat them both as signed) 
		// and subtract result from product register.
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MSUBX(AnalyzeInfo& info)
	{
		// Multiply one part of secondary accumulator $ax0 (selected by S) by one part of secondary accumulator $ax1 (selected by T) (treat them both as signed) 
		// and subtract result from product register. 
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	// AC

	void DspInterpreter::MULAC(AnalyzeInfo& info)
	{
		// Add product register to accumulator register $acR.
		// Multiply low part $axS.l of secondary accumulator $axS by high part $axS.h of secondary accumulator $axS (treat them both as signed).
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MULCAC(AnalyzeInfo& info)
	{
		// Add product register before multiplication to accumulator $acR.
		// Multiply mid part of accumulator register $acS.m by high part $axS.h of secondary accumulator $axS (treat them both as signed).
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MULXAC(AnalyzeInfo& info)
	{
		// Add product register to accumulator register $acR. 
		// Multiply one part $ax0 by one part $ax1 (treat them both as signed). Part is selected by S and T bits. Zero selects low part, one selects high part. 
		int64_t a = DspCore::SignExtend16(info.paramBits[0] ? core->regs.ax[0].h : core->regs.ax[0].l);
		int64_t b = DspCore::SignExtend16(info.paramBits[1] ? core->regs.ax[1].h : core->regs.ax[1].l);
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	// MV

	void DspInterpreter::MULMV(AnalyzeInfo& info)
	{
		// Move product register to accumulator register $acR.
		// Multiply low part $axS.l of secondary accumulator Register$axS by high part $axS.h of secondary accumulator $axS (treat them both as signed). 
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MULCMV(AnalyzeInfo& info)
	{
		// Move product register before multiplication to accumulator $acR.
		// Multiply mid part of accumulator register $acS.m by high part $axS.h of secondary accumulator $axS(treat them both as signed).
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MULXMV(AnalyzeInfo& info)
	{
		// Move product register to accumulator register $acR.
		// Multiply one part $ax0 by one part $ax1(treat them both as signed).Part is selected by Sand T bits.Zero selects low part, one selects high part.
		int64_t a = DspCore::SignExtend16(info.paramBits[0] ? core->regs.ax[0].h : core->regs.ax[0].l);
		int64_t b = DspCore::SignExtend16(info.paramBits[1] ? core->regs.ax[1].h : core->regs.ax[1].l);
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	// MVZ

	void DspInterpreter::MULMVZ(AnalyzeInfo& info)
	{
		// Move product register to accumulator register $acR and clear low part of accumulator register $acR.l.
		// Multiply low part $axS.l of secondary accumulator $axS by high part $axS.h of secondary accumulator $axS (treat them both as signed).
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MULCMVZ(AnalyzeInfo& info)
	{
		// Move product register before multiplication to accumulator $acR. Set low part of accumulator $acR.l to zero. 
		// Multiply mid part of accumulator register $acS.m by high part $axS.h of secondary accumulator $axS (treat them both as signed). 
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MULXMVZ(AnalyzeInfo& info)
	{
		// Move product register to accumulator register $acR and clear low part of accumulator register $acR.l. 
		// Multiply one part $ax0 by one part $ax1 (treat them both as signed). Part is selected by S and T bits. Zero selects low part, one selects high part.
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

#pragma region "Multiplier Instructions"
}
