/*

The `--bench` measurement and its report. See bench.h for what the run is for and who uses it.

*/

#include "pch.h"

#include "bench.h"

#include <chrono>

namespace Bench
{

	// The wall clock of a run. `std::chrono::steady_clock` is the portable replacement for the
	// `SDL_GetTicks64` the SDL front end used to call from here.
	static uint64_t NowMs()
	{
		static const auto origin = std::chrono::steady_clock::now();
		auto now = std::chrono::steady_clock::now();
		return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - origin).count();
	}

	// The cycle counters are raw TSC ticks; measure the rate against the wall clock once, so that
	// the report can show seconds rather than ticks.
	static double MeasureTscFrequency()
	{
		auto wall0 = std::chrono::steady_clock::now();
		uint64_t tsc0 = Gekko::ReadCycleCounter();
		Thread::Sleep(100);
		auto wall1 = std::chrono::steady_clock::now();
		uint64_t tsc1 = Gekko::ReadCycleCounter();

		double wallSeconds = std::chrono::duration<double>(wall1 - wall0).count();
		if (wallSeconds <= 0.0)
		{
			return 1.0;
		}

		return (double)(tsc1 - tsc0) / wallSeconds;
	}

	static int64_t bench_counter(Debug::PerfCounter counter)
	{
		return Debug::g_PerfCounters->GetCounter(counter);
	}

	void Measure(const std::wstring& file, uint32_t seconds, PumpProc pump)
	{
		// The cycle counters cost two `rdtsc` per block (about 20% of the throughput), so they are
		// opt-in: `set BENCH_PROFILE=1` turns the host cycle breakdown on, the plain run measures the
		// real throughput.
		bool profiled = getenv("BENCH_PROFILE") != nullptr;
		Gekko::cycleProfile = profiled;
		bool stats = getenv("BENCH_STATS") != nullptr;
		if (stats)
		{
			Core->EnableOpcodeStats(true);
		}
		Debug::g_PerfCounters->ResetAllCounters();
		Gekko::stats.Reset();
		Core->ResetInstructionCounter();

		uint64_t start = NowMs();
		// A duration of zero means "until the front end stops the run" (the plain headless run
		// waits for Ctrl+C); the windowed front end always passes a real duration.
		uint64_t durationMs = (uint64_t)seconds * 1000;
		uint64_t lastReport = start;
		int64_t lastOps = 0;
		int64_t lastTb = 0;
		uint64_t lastBlocks = 0;
		uint64_t lastCompiles = 0;
		uint64_t lastInval = 0;

		while (durationMs == 0 || NowMs() - start < durationMs)
		{
			// Give the front end a chance to keep its window responsive (and to end the run on
			// `SDL_QUIT`); a headless front end passes no pump at all.
			if (pump != nullptr && !pump())
			{
				break;
			}
			Thread::Sleep(20);

			uint64_t now = NowMs();
			if (now - lastReport >= 1000)
			{
				int64_t ops = (int64_t)Core->GetInstructionCounter();
				double sec = (double)(now - lastReport) / 1000.0;

				Debug::Report(Debug::Channel::Info,
					"profile: %.2f MIPS (%.2fx real), %.2fM blocks/s (%.2f instr/block), %.0fK compiles/s, %.0fK invalidations/s\n",
					(double)(ops - lastOps) / sec / 1e6,
					(double)(Core->regs.tb.sval - lastTb) / (double)Core->OneSecond() / sec,
					(double)(Gekko::stats.jitBlocks - lastBlocks) / sec / 1e6,
					(Gekko::stats.jitBlocks - lastBlocks) ? (double)(Gekko::stats.jitInstrs) / (double)Gekko::stats.jitBlocks : 0.0,
					(double)(Gekko::stats.jitCompiles - lastCompiles) / sec / 1e3,
					(double)(Gekko::stats.jitInvalidations - lastInval) / sec / 1e3);

				lastOps = ops;
				lastTb = Core->regs.tb.sval;
				lastBlocks = Gekko::stats.jitBlocks;
				lastCompiles = Gekko::stats.jitCompiles;
				lastInval = Gekko::stats.jitInvalidations;
				lastReport = now;
			}
		}

		Gekko::cycleProfile = false;
		if (stats)
		{
			Core->EnableOpcodeStats(false);
		}

		uint64_t elapsed = NowMs() - start;
		uint64_t ops = (uint64_t)Core->GetInstructionCounter();
		double wall = (double)elapsed / 1000.0;

		const Gekko::CpuStats& st = Gekko::stats;
		double tscHz = MeasureTscFrequency();

		Debug::Report(Debug::Channel::Norm, "\n");
		Debug::Report(Debug::Channel::Norm, "--- benchmark: %s ---\n", Util::WstringToString(file).c_str());
		// The emulated time the run covered, which is the metric to compare two configurations by:
		// a faster build gets further into the game in the same wall time, and a game's instruction
		// density per emulated second depends on where it is.
		double emulated = (double)Core->regs.tb.uval / (double)Core->OneSecond();

		Debug::Report(Debug::Channel::Norm, "elapsed            : %.3f s\n", wall);
		Debug::Report(Debug::Channel::Norm, "emulated time      : %.3f s (%.2fx real time)\n", emulated, emulated / wall);
		Debug::Report(Debug::Channel::Norm, "instructions       : %llu\n", (unsigned long long)ops);
		Debug::Report(Debug::Channel::Norm, "throughput         : %.2f MIPS\n", (double)ops / wall / 1e6);
		Debug::Report(Debug::Channel::Norm, "basic blocks run   : %llu (%.2f instructions per block)\n",
			(unsigned long long)st.jitBlocks, st.jitBlocks ? (double)st.jitInstrs / (double)st.jitBlocks : 0.0);
		Debug::Report(Debug::Channel::Norm, "blocks translated  : %llu (%.0f/s, %.2f%% of the blocks run)\n",
			(unsigned long long)st.jitCompiles, (double)st.jitCompiles / wall,
			st.jitBlocks ? (double)st.jitCompiles / (double)st.jitBlocks * 100.0 : 0.0);
		Debug::Report(Debug::Channel::Norm, "block invalidations: %llu (%.0f/s)\n",
			(unsigned long long)st.jitInvalidations, (double)st.jitInvalidations / wall);
		Debug::Report(Debug::Channel::Norm, "  exception entry  : %llu\n", (unsigned long long)st.invException);
		Debug::Report(Debug::Channel::Norm, "  rfi              : %llu\n", (unsigned long long)st.invRfi);
		Debug::Report(Debug::Channel::Norm, "  mtmsr            : %llu\n", (unsigned long long)st.invMtmsr);
		Debug::Report(Debug::Channel::Norm, "  mtspr bat/sdr/hid: %llu\n", (unsigned long long)st.invMtspr);
		Debug::Report(Debug::Channel::Norm, "  icbi             : %llu\n", (unsigned long long)st.invIcbi);
		Debug::Report(Debug::Channel::Norm, "  tlbie/tlbsync    : %llu\n", (unsigned long long)st.invTlb);
		Debug::Report(Debug::Channel::Norm, "  cache flush      : %llu\n", (unsigned long long)st.invFlash);
		Debug::Report(Debug::Channel::Norm, "jit fallbacks      : %llu (%.2f%% of the instructions)\n",
			(unsigned long long)st.jitFallbacks, ops ? (double)st.jitFallbacks / (double)ops * 100.0 : 0.0);
		Debug::Report(Debug::Channel::Norm, "interp instructions: %llu\n", (unsigned long long)st.interpInstrs);
		Debug::Report(Debug::Channel::Norm, "data cache fills   : %llu\n", (unsigned long long)st.dcacheFills);
		Debug::Report(Debug::Channel::Norm, "instr cache fills  : %llu\n", (unsigned long long)st.icacheFills);
		Debug::Report(Debug::Channel::Norm, "pi reads           : %llu (mmio %llu)\n",
			(unsigned long long)st.piReads, (unsigned long long)st.mmioReads);
		Debug::Report(Debug::Channel::Norm, "pi writes          : %llu (mmio %llu)\n",
			(unsigned long long)st.piWrites, (unsigned long long)st.mmioWrites);
		if (stats)
		{
			Debug::Report(Debug::Channel::Norm, "top guest instructions:\n");
			Core->PrintOpcodeStats(25);
		}

		Debug::Report(Debug::Channel::Norm, "dsp instructions   : %llu (%llu wakes)\n",
			(unsigned long long)st.dspInstrs, (unsigned long long)st.dspWakes);
		Debug::Report(Debug::Channel::Norm, "ai dma             : %llu feeds, %llu ints\n",
			(unsigned long long)st.aiFeeds, (unsigned long long)st.aiInts);
		Debug::Report(Debug::Channel::Norm, "vi interrupts      : %lld\n", (long long)bench_counter(Debug::PerfCounter::VIs));
		Debug::Report(Debug::Channel::Norm, "pe finishes        : %lld\n", (long long)bench_counter(Debug::PerfCounter::PEs));

		if (!profiled)
		{
			return;
		}

		// Where the host cycles went. `jit total` is the time the CPU thread spent inside the
		// recompiler; the rest of the wall time belongs to the other threads, to the UI and to idle time.
		Debug::Report(Debug::Channel::Norm, "host cycles (TSC %.2f GHz):\n", tscHz / 1e9);
		Debug::Report(Debug::Channel::Norm, "  jit total        : %.3f s (%.1f%% of the wall), %llu cycles\n",
			(double)st.jitRunCycles / tscHz, wall > 0 ? (double)st.jitRunCycles / tscHz / wall * 100.0 : 0.0,
			(unsigned long long)st.jitRunCycles);
		Debug::Report(Debug::Channel::Norm, "    generated block: %.3f s (%.1f%% of jit), %.1f cycles/block\n",
			(double)st.blockCallCycles / tscHz, st.jitRunCycles ? (double)st.blockCallCycles / st.jitRunCycles * 100.0 : 0.0,
			st.jitBlocks ? (double)st.blockCallCycles / (double)st.jitBlocks : 0.0);
		Debug::Report(Debug::Channel::Norm, "    translating    : %.3f s (%.1f%% of jit), %.1f cycles/block\n",
			(double)st.compileCycles / tscHz, st.jitRunCycles ? (double)st.compileCycles / st.jitRunCycles * 100.0 : 0.0,
			st.jitCompiles ? (double)st.compileCycles / (double)st.jitCompiles : 0.0);
		Debug::Report(Debug::Channel::Norm, "    dispatch       : %.3f s (%.1f%% of jit)\n",
			(double)(st.jitRunCycles > st.blockCallCycles + st.compileCycles ? st.jitRunCycles - st.blockCallCycles - st.compileCycles : 0) / tscHz,
			st.jitRunCycles ? (double)(st.jitRunCycles > st.blockCallCycles + st.compileCycles ? st.jitRunCycles - st.blockCallCycles - st.compileCycles : 0) / st.jitRunCycles * 100.0 : 0.0);
		Debug::Report(Debug::Channel::Norm, "    interp fallback: %.3f s (%.1f%% of jit)\n",
			(double)st.fallbackCycles / tscHz, st.jitRunCycles ? (double)st.fallbackCycles / st.jitRunCycles * 100.0 : 0.0);
		Debug::Report(Debug::Channel::Norm, "    cache fills    : %.3f s (%.1f%% of jit)\n",
			(double)st.castInCycles / tscHz, st.jitRunCycles ? (double)st.castInCycles / st.jitRunCycles * 100.0 : 0.0);
		Debug::Report(Debug::Channel::Norm, "    mem helpers    : %.3f s (%.1f%% of jit), %llu calls, %.1f cycles/call\n",
			(double)st.memHelperCycles / tscHz, st.jitRunCycles ? (double)st.memHelperCycles / st.jitRunCycles * 100.0 : 0.0,
			(unsigned long long)st.memHelperCalls, st.memHelperCalls ? (double)st.memHelperCycles / (double)st.memHelperCalls : 0.0);
		Debug::Report(Debug::Channel::Norm, "  interpreted instr: %.3f s\n", (double)st.interpCycles / tscHz);
	}

}
