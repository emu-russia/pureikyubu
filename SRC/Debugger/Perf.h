// Performance Counters

#pragma once

namespace Debug
{

	enum class PerfCounter
	{
		GekkoInstructions = 0,
		DspInstructions,
		VIs,
		PEs,

		Max,
	};

	class PerfCounters
	{
	public:
		PerfCounters();
		~PerfCounters();

		int64_t GetCounter(PerfCounter counter);
		void ResetCounter(PerfCounter counter);
		void ResetAllCounters();
	};

	extern PerfCounters* g_PerfCounters;
}
