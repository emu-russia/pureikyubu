// DSP parallel load/store/move instructions

#include "pch.h"

using namespace Debug;

namespace DSP
{

	// ldd d1,rn,mn d2,r3,m3
	void DspInterpreter::p_ldd(DecoderInfo& info)
	{
		int r = (int)info.paramsEx[1];
		core->MoveToReg((int)info.paramsEx[0], core->dsp->ReadDMem(core->regs.r[r]));
		AdvanceAddress(r, info.paramsEx[2]);

		r = (int)info.paramsEx[4];
		core->MoveToReg((int)info.paramsEx[3], core->dsp->ReadDMem(core->regs.r[r]));
		AdvanceAddress(r, info.paramsEx[5]);
	}

	// ls d,r,m r,m,s
	void DspInterpreter::p_ls(DecoderInfo& info)
	{
		int r = (int)info.paramsEx[1];
		core->MoveToReg((int)info.paramsEx[0], core->dsp->ReadDMem(core->regs.r[r]));
		AdvanceAddress(r, info.paramsEx[2]);

		r = (int)info.paramsEx[3];
		core->dsp->WriteDMem(core->regs.r[r], core->MoveFromReg((int)info.paramsEx[5]));
		AdvanceAddress(r, info.paramsEx[4]);
	}

	// ld d,rn,mn
	void DspInterpreter::p_ld(DecoderInfo& info)
	{
		int r = (int)info.paramsEx[1];
		core->MoveToReg((int)info.paramsEx[0], core->dsp->ReadDMem(core->regs.r[r]));
		AdvanceAddress(r, info.paramsEx[2]);
	}

	// st rn,mn,s
	void DspInterpreter::p_st(DecoderInfo& info)
	{
		int r = (int)info.paramsEx[0];
		core->dsp->WriteDMem(core->regs.r[r], core->MoveFromReg((int)info.paramsEx[2]));
		AdvanceAddress(r, info.paramsEx[1]);
	}

	// mv d,s
	void DspInterpreter::p_mv(DecoderInfo& info)
	{
		core->MoveToReg((int)info.paramsEx[0], core->MoveFromReg((int)info.paramsEx[1]));
	}

	// mr rn,mn
	void DspInterpreter::p_mr(DecoderInfo& info)
	{
		int r = (int)info.paramsEx[0];
		AdvanceAddress(r, info.paramsEx[1]);
	}

}
