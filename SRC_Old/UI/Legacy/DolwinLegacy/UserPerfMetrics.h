
#pragma once

// The counters are polled once a second after starting the emulation.
// Polling is performed in a separate thread that sleeps after polling so as not to load the CPU. The information is displayed in the status bar.

namespace UI
{

	class PerfMetrics
	{
		size_t metricsInterval = 1000;

		Thread* perfThread;
		static void PerfThreadProc(void* param);

		// The counter values are retrieved and cleared using JDI.

		int64_t GetGekkoInstructionsCounter();
		void ResetGekkoInstructionsCounter();

		int64_t GetGekkoCompiledSegments();
		void ResetGekkoCompiledSegments();

		int64_t GetGekkoExecutedSegments();
		void ResetGekkoExecutedSegments();

		int64_t GetDspInstructionsCounter();
		void ResetDspInstructionsCounter();

		int32_t GetVICounter();
		void ResetVICounter();

		int32_t GetPECounter();
		void ResetPECounter();

		std::string GetSystemTime();

	public:
		PerfMetrics();
		~PerfMetrics();
	};


	extern PerfMetrics* g_perfMetrics;

}
