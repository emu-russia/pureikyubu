// DSP packed load/store which-can-burn-your-ass instructions

#include "pch.h"

namespace DSP
{
#pragma region "Bottom Instructions"

	void DspInterpreter::DR(AnalyzeInfo& info)
	{
		core->ArAdvance(info.paramExBits[0], -1);
	}

	void DspInterpreter::IR(AnalyzeInfo& info)
	{
		core->ArAdvance(info.paramExBits[0], +1);
	}

	void DspInterpreter::NR(AnalyzeInfo& info)
	{
		core->ArAdvance(info.paramExBits[0], core->regs.ix[info.paramExBits[0]]);
	}

	void DspInterpreter::MV(AnalyzeInfo& info)
	{
		core->MoveToReg(info.paramExBits[0], core->MoveFromReg(info.paramExBits[1]));
	}

	void DspInterpreter::S(AnalyzeInfo& info)
	{
		core->WriteDMem(core->regs.ar[info.paramExBits[0]], core->MoveFromReg(info.paramExBits[1]));
		core->ArAdvance(info.paramExBits[0], +1);
	}

	void DspInterpreter::SN(AnalyzeInfo& info)
	{
		core->WriteDMem(core->regs.ar[info.paramExBits[0]], core->MoveFromReg(info.paramExBits[1]));
		core->ArAdvance(info.paramExBits[0], core->regs.ix[info.paramExBits[0]]);
	}

	void DspInterpreter::L(AnalyzeInfo& info)
	{
		core->MoveToReg(info.paramExBits[0], core->ReadDMem(core->regs.ar[info.paramExBits[1]]));
		core->ArAdvance(info.paramExBits[1], +1);
	}

	void DspInterpreter::LN(AnalyzeInfo& info)
	{
		core->MoveToReg(info.paramExBits[0], core->ReadDMem(core->regs.ar[info.paramExBits[1]]));
		core->ArAdvance(info.paramExBits[1], core->regs.ix[info.paramExBits[1]]);
	}

	void DspInterpreter::LS(AnalyzeInfo& info)
	{
		core->MoveToReg(info.paramExBits[0], core->ReadDMem(core->regs.ar[0]));
		core->WriteDMem(core->regs.ar[3], core->MoveFromReg((int)info.paramsEx[1]));
		core->ArAdvance(0, +1);
		core->ArAdvance(3, +1);
	}

	void DspInterpreter::SL(AnalyzeInfo& info)
	{
		core->WriteDMem(core->regs.ar[0], core->MoveFromReg((int)info.paramsEx[0]));
		core->MoveToReg(info.paramExBits[1], core->ReadDMem(core->regs.ar[3]));
		core->ArAdvance(0, +1);
		core->ArAdvance(3, +1);
	}

	void DspInterpreter::LSN(AnalyzeInfo& info)
	{
		core->MoveToReg(info.paramExBits[0], core->ReadDMem(core->regs.ar[0]));
		core->WriteDMem(core->regs.ar[3], core->MoveFromReg((int)info.paramsEx[1]));
		core->ArAdvance(0, core->regs.ix[0]);
		core->ArAdvance(3, +1);
	}

	void DspInterpreter::SLN(AnalyzeInfo& info)
	{
		core->WriteDMem(core->regs.ar[0], core->MoveFromReg((int)info.paramsEx[0]));
		core->MoveToReg(info.paramExBits[1], core->ReadDMem(core->regs.ar[3]));
		core->ArAdvance(0, core->regs.ix[0]);
		core->ArAdvance(3, +1);
	}

	void DspInterpreter::LSM(AnalyzeInfo& info)
	{
		core->MoveToReg(info.paramExBits[0], core->ReadDMem(core->regs.ar[0]));
		core->WriteDMem(core->regs.ar[3], core->MoveFromReg((int)info.paramsEx[1]));
		core->ArAdvance(0, +1);
		core->ArAdvance(3, core->regs.ix[3]);
	}

	void DspInterpreter::SLM(AnalyzeInfo& info)
	{
		core->WriteDMem(core->regs.ar[0], core->MoveFromReg((int)info.paramsEx[0]));
		core->MoveToReg(info.paramExBits[1], core->ReadDMem(core->regs.ar[3]));
		core->ArAdvance(0, +1);
		core->ArAdvance(3, core->regs.ix[3]);
	}

	void DspInterpreter::LSNM(AnalyzeInfo& info)
	{
		core->MoveToReg(info.paramExBits[0], core->ReadDMem(core->regs.ar[0]));
		core->WriteDMem(core->regs.ar[3], core->MoveFromReg((int)info.paramsEx[1]));
		core->ArAdvance(0, core->regs.ix[0]);
		core->ArAdvance(3, core->regs.ix[3]);
	}

	void DspInterpreter::SLNM(AnalyzeInfo& info)
	{
		core->WriteDMem(core->regs.ar[0], core->MoveFromReg((int)info.paramsEx[0]));
		core->MoveToReg(info.paramExBits[1], core->ReadDMem(core->regs.ar[3]));
		core->ArAdvance(0, core->regs.ix[0]);
		core->ArAdvance(3, core->regs.ix[3]);
	}

	// TODO: The last 2 groups of opcodes look very crazy. Need to reverse UCodes and understand in context how they actually work.

	// LD $ax0.d, $ax1.r, @$arS 
	// ax0.d (d = Low/High) = *arS;  ax1.r (r = Low/High) = *ar3
	// Postincrement arS, ar3

	void DspInterpreter::LDCommon(AnalyzeInfo& info)
	{
		if (info.paramExBits[0])
		{
			core->regs.ax[0].h = core->ReadDMem(core->regs.ar[info.paramExBits[2]]);
		}
		else
		{
			core->regs.ax[0].l = core->ReadDMem(core->regs.ar[info.paramExBits[2]]);
		}

		if (info.paramExBits[1])
		{
			core->regs.ax[1].h = core->ReadDMem(core->regs.ar[3]);
		}
		else
		{
			core->regs.ax[1].l = core->ReadDMem(core->regs.ar[3]);
		}
	}

	void DspInterpreter::LD(AnalyzeInfo& info)
	{
		LDCommon(info);
		core->ArAdvance(info.paramExBits[2], +1);
		core->ArAdvance(3, +1);
	}

	void DspInterpreter::LDN(AnalyzeInfo& info)
	{
		LDCommon(info);
		core->ArAdvance(info.paramExBits[2], core->regs.ix[info.paramExBits[2]]);
		core->ArAdvance(3, +1);
	}

	void DspInterpreter::LDM(AnalyzeInfo& info)
	{
		LDCommon(info);
		core->ArAdvance(info.paramExBits[2], +1);
		core->ArAdvance(3, core->regs.ix[3]);
	}

	void DspInterpreter::LDNM(AnalyzeInfo& info)
	{
		LDCommon(info);
		core->ArAdvance(info.paramExBits[2], core->regs.ix[info.paramExBits[2]]);
		core->ArAdvance(3, core->regs.ix[3]);
	}

	// LDAX $axR, @$arS
	// axR.h = *arS;  axR.l = *ar3;
	// Postincrement arS, ar3

	void DspInterpreter::LDAXCommon(AnalyzeInfo& info)
	{
		core->regs.ax[info.paramExBits[0]].h = core->ReadDMem(core->regs.ar[info.paramExBits[1]]);
		core->regs.ax[info.paramExBits[0]].l = core->ReadDMem(core->regs.ar[3]);
	}

	void DspInterpreter::LDAX(AnalyzeInfo& info)
	{
		LDAXCommon(info);
		core->ArAdvance(info.paramExBits[1], +1);
		core->ArAdvance(3, +1);
	}

	void DspInterpreter::LDAXN(AnalyzeInfo& info)
	{
		LDAXCommon(info);
		core->ArAdvance(info.paramExBits[1], core->regs.ix[info.paramExBits[1]]);
		core->ArAdvance(3, +1);
	}

	void DspInterpreter::LDAXM(AnalyzeInfo& info)
	{
		LDAXCommon(info);
		core->ArAdvance(info.paramExBits[1], +1);
		core->ArAdvance(3, core->regs.ix[3]);
	}

	void DspInterpreter::LDAXNM(AnalyzeInfo& info)
	{
		LDAXCommon(info);
		core->ArAdvance(info.paramExBits[1], core->regs.ix[info.paramExBits[1]]);
		core->ArAdvance(3, core->regs.ix[3]);
	}

#pragma endregion "Bottom Instructions"
}
