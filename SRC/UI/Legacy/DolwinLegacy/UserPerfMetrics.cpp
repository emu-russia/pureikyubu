/*

# Performance Counters

Interesting to track :
- The number of emulated Gekko instructions (million per second, mips)
- Number of DSP instructions emulated (million per second, mips)
- Number of VI interrupts (frames per second)
- Number of draw operations (PE DrawDone / second)
- Show formatted value of TBR register (OSSystemTime)

*/

#include "pch.h"

namespace UI
{

	// Global instance of the utility, which is controlled in the UserWindow.cpp module
	PerfMetrics* g_perfMetrics = nullptr;


	void PerfMetrics::PerfThreadProc(void* param)
	{
		PerfMetrics* perf = (PerfMetrics*)param;

		// Get and reset counters

		int64_t gekkoMips = perf->GetGekkoInstructionsCounter();
		perf->ResetGekkoInstructionsCounter();

		int64_t dspMips = perf->GetDspInstructionsCounter();
		perf->ResetDspInstructionsCounter();

		char str[0x100];
		sprintf_s(str, sizeof(str), "gekko: %.02f mips, dsp: %.02f mips", (float)gekkoMips / 1000000.f, (float)dspMips / 1000000.f);

		int32_t vis = perf->GetVICounter();
		perf->ResetVICounter();

		int32_t pes = perf->GetPECounter();
		perf->ResetPECounter();

		// Display information in the status bar

		SetStatusText(STATUS_ENUM::Progress, Util::StringToWstring(str));
		SetStatusText(STATUS_ENUM::VIs, std::to_wstring(vis) + L" VI/s");
		SetStatusText(STATUS_ENUM::PEs, std::to_wstring(pes) + L" PE/s");
		SetStatusText(STATUS_ENUM::SystemTime, Util::StringToWstring(perf->GetSystemTime()));

		Thread::Sleep(perf->metricsInterval);
	}

	PerfMetrics::PerfMetrics()
	{
		perfThread = new Thread(PerfThreadProc, false, this, "PerfThread");
	}

	PerfMetrics::~PerfMetrics()
	{
		delete perfThread;
	}

	int64_t PerfMetrics::GetGekkoInstructionsCounter()
	{
		return Jdi->GetPerformanceCounter(0);
	}

	void PerfMetrics::ResetGekkoInstructionsCounter()
	{
		Jdi->ResetPerformanceCounter(0);
	}

	int64_t PerfMetrics::GetDspInstructionsCounter()
	{
		return Jdi->GetPerformanceCounter(1);
	}

	void PerfMetrics::ResetDspInstructionsCounter()
	{
		Jdi->ResetPerformanceCounter(1);
	}

	int32_t PerfMetrics::GetVICounter()
	{
		return Jdi->GetPerformanceCounter(2);
	}

	void PerfMetrics::ResetVICounter()
	{
		Jdi->ResetPerformanceCounter(2);
	}

	int32_t PerfMetrics::GetPECounter()
	{
		return Jdi->GetPerformanceCounter(3);
	}

	void PerfMetrics::ResetPECounter()
	{
		Jdi->ResetPerformanceCounter(3);
	}

	std::string PerfMetrics::GetSystemTime()
	{
		return Jdi->GetSystemTime();
	}

}
