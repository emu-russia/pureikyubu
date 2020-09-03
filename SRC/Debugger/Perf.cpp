
#include "pch.h"

namespace Debug
{
	PerfCounters* g_PerfCounters = nullptr;

	PerfCounters::PerfCounters()
	{
	}

	PerfCounters::~PerfCounters()
	{
	}

	int64_t PerfCounters::GetCounter(PerfCounter counter)
	{
		int64_t value = 0;

		switch (counter)
		{
			case PerfCounter::GekkoInstructions:
				return Gekko::Gekko->GetInstructionCounter();
				break;
			case PerfCounter::DspInstructions:
				return Flipper::DSP->core->GetInstructionCounter();
				break;
			case PerfCounter::VIs:
				return pi.intCounters[(size_t)PIInterruptSource::VI];
				break;
			case PerfCounter::PEs:
				return pi.intCounters[(size_t)PIInterruptSource::PE_FINISH];
				break;
		}

		return value;
	}

	void PerfCounters::ResetCounter(PerfCounter counter)
	{
		switch (counter)
		{
			case PerfCounter::GekkoInstructions:
				Gekko::Gekko->ResetInstructionCounter();
				break;
			case PerfCounter::DspInstructions:
				Flipper::DSP->core->ResetInstructionCounter();
				break;
			case PerfCounter::VIs:
				pi.intCounters[(size_t)PIInterruptSource::VI] = 0;
				break;
			case PerfCounter::PEs:
				pi.intCounters[(size_t)PIInterruptSource::PE_FINISH] = 0;
				break;
		}
	}

	void PerfCounters::ResetAllCounters()
	{
		for (int i = 0; i < (int)PerfCounter::Max; i++)
		{
			ResetCounter((PerfCounter)i);
		}
	}

}
