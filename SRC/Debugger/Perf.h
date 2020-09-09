// Performance Counters.

// Statistics are sent to the UI through the GetPerformanceCounter / ResetPerformanceCounter Jdi commands.

#pragma once

namespace Debug
{

	enum class PerfCounter
	{
		GekkoInstructions = 0,		// Number of Gekko instructions executed
		DspInstructions,		// Number of DSP instructions executed
		VIs,				// Number of VI VBlank interrupts (based on PI interrupt counters)
		PEs,				// Number of PE DRAW_DONE operations (based on PI interrupt counters)

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

	// A global instance, created by the emulator in the EMUCtor method and is available throughout the life of the emulator
	// (another thing is that statistics are updated only when the emulator is running).
	extern PerfCounters* g_PerfCounters;
}
