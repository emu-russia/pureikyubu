/*

The HW interface profiler: the counters, the sampling and the two report forms. See hwprof.h for
what the module is for and how the pieces fit together.

*/

#include "pch.h"

#include <chrono>

namespace Debug
{

namespace HwProfile
{

	// ------------------------------------------------------------------------------------
	// The counters
	// ------------------------------------------------------------------------------------

	struct CounterInfo
	{
		Counter counter;
		const char* name;
		bool bytes;			// the counter is a byte count (kB = 1024) rather than an event count
	};

	// The order is the order of the report. It is also what tells `Reset` and the reports how
	// many counters there are.
	static const CounterInfo counterInfo[] =
	{
		{ Counter::Bus60xRead,			"60x bus read",			true },
		{ Counter::Bus60xWrite,			"60x bus write",		true },
		{ Counter::SplashRead,			"Flipper/Splash read",	true },
		{ Counter::SplashWrite,			"Flipper/Splash write",	true },
		{ Counter::PiInterrupts,		"PI interrupts",		false },
		{ Counter::WriteGather,			"Write gather buffer",	true },
		{ Counter::CpFifo,				"PI/CP FIFO",			true },
		{ Counter::AudioMixer,			"Audio mixer input",	true },
		{ Counter::DmaExi,				"DMA EXI",				true },
		{ Counter::DmaDi,				"DMA DI",				true },
		{ Counter::DmaDsp,				"DMA DSP",				true },
		{ Counter::DmaAi,				"DMA AI",				true },
		{ Counter::DmaAram,				"DMA ARAM",				true },
		{ Counter::GfxPrimitives,		"GFX primitives",		false },
		{ Counter::GfxVertices,			"GFX vertices",			false },
		{ Counter::ViFrames,			"VI frames",			false },
		{ Counter::GekkoInstructions,	"Gekko instructions",	false },
		{ Counter::DspInstructions,		"DSP instructions",		false },
	};

	static const size_t counterInfoCount = sizeof(counterInfo) / sizeof(counterInfo[0]);

	const char* CounterName(Counter counter)
	{
		for (size_t i = 0; i < counterInfoCount; i++)
		{
			if (counterInfo[i].counter == counter)
				return counterInfo[i].name;
		}

		return "?";
	}

	bool CounterIsBytes(Counter counter)
	{
		for (size_t i = 0; i < counterInfoCount; i++)
		{
			if (counterInfo[i].counter == counter)
				return counterInfo[i].bytes;
		}

		return false;
	}

	// A counter that the emulated hardware bumps in place. The instructions retired are the
	// exception: the two cores already count them, and a second increment on every instruction
	// would be paid by the hottest path of the emulator, so the caller hands them in.
	static void Collect(uint64_t* values, uint64_t gekkoInstructions, uint64_t dspInstructions)
	{
		for (size_t i = 0; i < (size_t)Counter::Max; i++)
		{
			values[i] = counters[i];
		}

		values[(size_t)Counter::GekkoInstructions] = gekkoInstructions;
		values[(size_t)Counter::DspInstructions] = dspInstructions;
	}

	// ------------------------------------------------------------------------------------
	// The rate table
	// ------------------------------------------------------------------------------------

	void RateTable::Clear()
	{
		memset(previous, 0, sizeof(previous));
		memset(total, 0, sizeof(total));
		memset(delta, 0, sizeof(delta));
		window = 0.0;
		sampled = false;
	}

	void RateTable::Add(const uint64_t* values, double emulatedSeconds)
	{
		if (!sampled || emulatedSeconds <= 0.0)
		{
			// The first call only records where the counters start. A window of zero length says
			// nothing about a rate, so it must not be allowed to divide by it either.
			memcpy(previous, values, sizeof(previous));
			sampled = true;
			return;
		}

		for (size_t i = 0; i < (size_t)Counter::Max; i++)
		{
			// A counter of the emulated machine never goes backwards. A smaller value than the
			// previous one means the machine was rebuilt between the two samples, so the window
			// starts over for that counter instead of reporting a nonsense (negative) rate.
			delta[i] = (values[i] >= previous[i]) ? (values[i] - previous[i]) : values[i];
			previous[i] = values[i];
			total[i] += delta[i];
		}

		window = emulatedSeconds;
	}

	double RateTable::PerSecond(Counter counter) const
	{
		if (window <= 0.0)
			return 0.0;

		return (double)delta[(size_t)counter] / window;
	}

	// ------------------------------------------------------------------------------------
	// The sampler
	// ------------------------------------------------------------------------------------

	static SpinLock sampleLock;
	static RateTable rates;
	static uint64_t baselineTicks = 0;
	static uint64_t baselineWall = 0;
	static bool haveBaseline = false;
	static double windowWall = 0.0;

	static uint64_t NowMs()
	{
		static const auto origin = std::chrono::steady_clock::now();
		auto now = std::chrono::steady_clock::now();
		return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - origin).count();
	}

	void Reset()
	{
		sampleLock.Lock();

		memset(counters, 0, sizeof(counters));
		rates.Clear();
		haveBaseline = false;
		baselineTicks = 0;
		baselineWall = 0;
		windowWall = 0.0;

		sampleLock.Unlock();
	}

	bool Sample(uint64_t gekkoInstructions, uint64_t dspInstructions, uint64_t ticks, uint64_t ticksPerSecond)
	{
		sampleLock.Lock();

		uint64_t wall = NowMs();

		// The time base starts at zero when a new machine is built, and the counters are cleared
		// with it (HwProfile::Reset). A backwards jump that the reset did not cover (a CPU reset
		// while the machine stays loaded) must not produce a negative window.
		if (!haveBaseline || ticks < baselineTicks || ticksPerSecond == 0)
		{
			baselineTicks = ticks;
			baselineWall = wall;
			haveBaseline = true;

			uint64_t values[(size_t)Counter::Max];
			Collect(values, gekkoInstructions, dspInstructions);
			rates.Add(values, 0.0);

			sampleLock.Unlock();
			return false;
		}

		double emulatedSeconds = (double)(ticks - baselineTicks) / (double)ticksPerSecond;

		if (emulatedSeconds < DefaultWindow)
		{
			sampleLock.Unlock();
			return false;
		}

		uint64_t values[(size_t)Counter::Max];
		Collect(values, gekkoInstructions, dspInstructions);
		rates.Add(values, emulatedSeconds);

		windowWall = (wall > baselineWall) ? (double)(wall - baselineWall) / 1000.0 : 0.0;
		baselineTicks = ticks;
		baselineWall = wall;

		sampleLock.Unlock();
		return true;
	}

	const RateTable& Rates()
	{
		return rates;
	}

	double WindowSeconds()
	{
		return rates.Window();
	}

	double WindowWallSeconds()
	{
		return windowWall;
	}

	// ------------------------------------------------------------------------------------
	// Formatting
	// ------------------------------------------------------------------------------------

	void FormatBytes(uint64_t value, std::string& text)
	{
		static const char* suffix[] = { "B", "KB", "MB", "GB", "TB" };

		double scaled = (double)value;
		int unit = 0;

		while (scaled >= 1024.0 && unit < 4)
		{
			scaled /= 1024.0;
			unit++;
		}

		char line[0x40];
		sprintf(line, (unit == 0) ? "%.0f %s" : "%.2f %s", scaled, suffix[unit]);
		text = line;
	}

	void FormatCount(uint64_t value, std::string& text)
	{
		static const char* suffix[] = { "", "K", "M", "G", "T" };

		double scaled = (double)value;
		int unit = 0;

		while (scaled >= 1000.0 && unit < 4)
		{
			scaled /= 1000.0;
			unit++;
		}

		char line[0x40];
		sprintf(line, (unit == 0) ? "%.0f%s" : "%.2f%s", scaled, suffix[unit]);
		text = line;
	}

	void FormatRate(double value, bool bytes, std::string& text)
	{
		static const char* byteSuffix[] = { "B", "KB", "MB", "GB", "TB" };
		static const char* countSuffix[] = { "", "K", "M", "G", "T" };

		// A rate keeps its fraction: 12.35 MB/s rounded to 12 B/s is the kind of report that sends
		// the reader looking for a bug in the profiler.
		double scaled = value;
		int unit = 0;

		while (scaled >= (bytes ? 1024.0 : 1000.0) && unit < 4)
		{
			scaled /= (bytes ? 1024.0 : 1000.0);
			unit++;
		}

		char line[0x40];
		sprintf(line, (unit == 0) ? "%.0f %s/s" : "%.2f %s/s",
			scaled, bytes ? byteSuffix[unit] : countSuffix[unit]);
		text = line;
	}

	// ------------------------------------------------------------------------------------
	// The reports
	// ------------------------------------------------------------------------------------

	static void ReportHeader(std::string& text)
	{
		char line[0x100];
		double window = rates.Window();
		double real = (window > 0.0 && windowWall > 0.0) ? (window / windowWall) : 0.0;

		if (!rates.Sampled() || window <= 0.0)
		{
			sprintf(line, "no window measured yet (the first one needs %.1f emulated seconds)", DefaultWindow);
		}
		else
		{
			sprintf(line, "window %.3f s emulated, %.2fx real time", window, real);
		}

		text = line;
	}

	void ReportToMarkdown(std::string& text)
	{
		text.clear();

		// The rates are written by the thread that takes the sample; a report can be asked for
		// from another one (the debugger, or the GFX thread that draws the overlay).
		sampleLock.Lock();

		std::string header;
		ReportHeader(header);

		text += "# HW interface profile\n\n";
		text += header + "\n\n";
		text += "| Channel | Rate | Total |\n";
		text += "|---|---|---|\n";

		for (size_t i = 0; i < counterInfoCount; i++)
		{
			const CounterInfo& info = counterInfo[i];

			std::string rate, total;
			FormatRate(rates.PerSecond(info.counter), info.bytes, rate);
			if (info.bytes)
				FormatBytes(rates.Total(info.counter), total);
			else
				FormatCount(rates.Total(info.counter), total);

			text += "| " + std::string(info.name) + " | " + rate + " | " + total + " |\n";
		}

		sampleLock.Unlock();
	}

	void ReportToLines(std::vector<std::string>& lines)
	{
		lines.clear();

		sampleLock.Lock();

		std::string header;
		ReportHeader(header);
		lines.push_back("HW interface profile  " + header);

		for (size_t i = 0; i < counterInfoCount; i++)
		{
			const CounterInfo& info = counterInfo[i];

			std::string rate, total;
			FormatRate(rates.PerSecond(info.counter), info.bytes, rate);
			if (info.bytes)
				FormatBytes(rates.Total(info.counter), total);
			else
				FormatCount(rates.Total(info.counter), total);

			char line[0x100];
			sprintf(line, "%-24s %14s %12s", info.name, rate.c_str(), total.c_str());
			lines.push_back(line);
		}

		sampleLock.Unlock();
	}

}

}
