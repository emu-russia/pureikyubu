// Low-level DSP core

#include "pch.h"

namespace DSP
{

	DspCore::DspCore(HWConfig* config)
	{

		Reset();
	}

	DspCore::~DspCore()
	{
	}

	void DspCore::Reset()
	{
		savedGekkoTicks = cpu.tb.uval;
	}

	void DspCore::AddBreakpoint(uint16_t imemAddress)
	{
		MySpinLock::Lock(&breakPointsSpinLock);
		breakpoints.push_back(imemAddress);
		MySpinLock::Unlock(&breakPointsSpinLock);
	}

	void DspCore::ClearBreakpoints()
	{
		MySpinLock::Lock(&breakPointsSpinLock);
		breakpoints.clear();
		MySpinLock::Unlock(&breakPointsSpinLock);
	}

	void DspCore::Run()
	{

	}

	void DspCore::Suspend()
	{

	}

	void DspCore::Step()
	{

	}

	void DspCore::Update()
	{

	}

}
