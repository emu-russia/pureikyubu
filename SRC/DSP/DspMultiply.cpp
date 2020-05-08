// DSP multiply instructions

#include "pch.h"

namespace DSP
{
#pragma region "Multiplier Instructions"

	void DspInterpreter::MADD(AnalyzeInfo& info)
	{
		// Multiply low part $axS.l of secondary accumulator $axS by high part $axS.h of secondary accumulator $axS
		// (treat them both as signed) and add result to product register. 
		//Madd16x16
		//core->UnpackProd(0);
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MADDC(AnalyzeInfo& info)
	{
		// Multiply middle part of accumulator $acS.m by high part of secondary accumulator $axT.h
		// (treat them both as signed) and add result to product register.
		//Madd32x16
		//core->UnpackProd(0);
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MADDX(AnalyzeInfo& info)
	{
		//Multiply one part of secondary accumulator $ax0 (selected by S) by one part of secondary accumulator $ax1 (selected by T)
		// (treat them both as signed) and add result to product register. 
		//Madd16x16
		//core->UnpackProd(0);
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MSUB(AnalyzeInfo& info)
	{
		//Multiply low part $axS.l of secondary accumulator $axS by high part $axS.h of secondary accumulator $axS 
		//(treat them both as signed) and subtract result from product register. 
		//Msub16x16
		//core->UnpackProd(0);
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MSUBC(AnalyzeInfo& info)
	{
		//Multiply middle part of accumulator $acS.m by high part of secondary accumulator $axT.h
		// (treat them both as signed) and subtract result from product register.
		//Msub32x16
		//core->UnpackProd(0);
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MSUBX(AnalyzeInfo& info)
	{
		//Multiply one part of secondary accumulator $ax0 (selected by S) by one part of secondary accumulator $ax1 (selected by T) 
		// (treat them both as signed) and subtract result from product register. 
		//Msub16x16
		//core->UnpackProd(0);
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MUL(AnalyzeInfo& info)
	{
		//Multiply low part $axS.l of secondary accumulator $axS by high part $axS.h of secondary accumulator $axS (treat them both as signed). 
		//Mul16x16
		//core->UnpackProd(0);
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MULAC(AnalyzeInfo& info)
	{
		// Add product register to accumulator register $acR.
		// Multiply low part $axS.l of secondary accumulator $axS by high part $axS.h of secondary accumulator $axS(treat them both as signed).
		//Mul16x16
		//core->UnpackProd(0);
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MULC(AnalyzeInfo& info)
	{
		//Multiply mid part of accumulator register $acS.m by high part $axS.h of secondary accumulator $axS (treat them both as signed).
		//Mul32x16
		//core->UnpackProd(0);
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MULCAC(AnalyzeInfo& info)
	{
		// Multiply mid part of accumulator register $acS.m by high part $axS.h of secondary accumulator $axS(treat them both as signed).
		// Add product register before multiplication to accumulator $acR.
		//Mul32x16
		//core->UnpackProd(0);
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MULCMV(AnalyzeInfo& info)
	{
		// Multiply mid part of accumulator register $acS.m by high part $axS.h of secondary accumulator $axS(treat them both as signed).
		// Move product register before multiplication to accumulator $acR.
		//Mul32x16
		//core->UnpackProd(0);
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MULCMVZ(AnalyzeInfo& info)
	{
		// Multiply mid part of accumulator register $acS.m by high part $axS.h of secondary accumulator $axS (treat them both as signed). 
		// Move product register before multiplication to accumulator $acR. Set low part of accumulator $acR.l to zero. 
		//Mul32x16
		//core->UnpackProd(0);
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MULMV(AnalyzeInfo& info)
	{
		// Move product register to accumulator register $acR. Multiply low part $axS.l of secondary accumulator Register$axS by high part $axS.h of secondary accumulator $axS
		// (treat them both as signed). 
		//Mul16x16
		//core->UnpackProd(0);
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MULMVZ(AnalyzeInfo& info)
	{
		// Move product register to accumulator register $acRand clear low part of accumulator register $acR.l.
		// Multiply low part $axS.l of secondary accumulator $axS by high part $axS.h of secondary accumulator $axS(treat them both as signed).
		//Mul16x16
		//core->UnpackProd(0);
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MULX(AnalyzeInfo& info)
	{
		int64_t a = DspCore::SignExtend16(info.paramBits[0] ? core->regs.ax[0].h : core->regs.ax[0].l);
		int64_t b = DspCore::SignExtend16(info.paramBits[1] ? core->regs.ax[1].h : core->regs.ax[1].l);
		//core->UnpackProd(a * b);
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MULXAC(AnalyzeInfo& info)
	{
		//core->regs.ac[info.paramBits[2]].sbits += core->PackProd();
		int64_t a = DspCore::SignExtend16(info.paramBits[0] ? core->regs.ax[0].h : core->regs.ax[0].l);
		int64_t b = DspCore::SignExtend16(info.paramBits[1] ? core->regs.ax[1].h : core->regs.ax[1].l);
		//core->UnpackProd(a * b);
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MULXMV(AnalyzeInfo& info)
	{
		//core->regs.ac[info.paramBits[2]].sbits = core->PackProd();
		int64_t a = DspCore::SignExtend16(info.paramBits[0] ? core->regs.ax[0].h : core->regs.ax[0].l);
		int64_t b = DspCore::SignExtend16(info.paramBits[1] ? core->regs.ax[1].h : core->regs.ax[1].l);
		//core->UnpackProd(a * b);
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

	void DspInterpreter::MULXMVZ(AnalyzeInfo& info)
	{
		//Move product register to accumulator register $acR and clear low part of accumulator register $acR.l. 
		//Multiply one part $ax0 by one part $ax1 (treat them both as signed).
		//Part is selected by S and T bits. Zero selects low part, one selects high part.
		//Mul16x16
		core->regs.prod.h = core->regs.prod.l = core->regs.prod.m1 = core->regs.prod.m2 = 0;
	}

#pragma region "Multiplier Instructions"
}
