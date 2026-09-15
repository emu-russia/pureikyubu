// HW interface profiler unit tests (issue #394).
//
// The subject is the pure half of src/hwprof.cpp: the rate table that turns the monotone counters
// of the emulated hardware into rates, and the two report forms built from it. The profiler is
// deliberately a function of a reading rather than of a running emulator (the caller hands the
// instruction counts and the time base in), so the sampling can be driven here with exact numbers.
//
// What the tests pin down:
//
//   * the first reading only records the baseline - there is no rate to report before a window;
//   * a window is measured in *emulated* seconds, so the rates do not depend on the host speed;
//   * a window of zero length is not a window (no division by it);
//   * a counter that went backwards (the machine was rebuilt between two samples) starts the
//     window over instead of reporting a negative rate;
//   * the units the report prints (kB = 1024 for traffic, k = 1000 for the event counters);
//   * every channel of the issue shows up in both report forms.

#include "pch.h"

using namespace Debug;

namespace HwProfUnitTest
{
	TEST_CLASS(HwProfTest)
	{
		// The counters are process-wide; every test starts from a machine that has just been
		// built, which is exactly what EMUOpen does.
		TEST_METHOD_INITIALIZE(Setup)
		{
			HwProfile::Reset();
		}

		static bool Close(double a, double b, double epsilon = 1e-6)
		{
			return fabs(a - b) <= epsilon;
		}

	public:

		// ------------------------------------------------------------------
		// The counter array
		// ------------------------------------------------------------------

		TEST_METHOD(Count_AddsToTheChannelAndResetClearsIt)
		{
			HwProfile::Count(HwProfile::Counter::Bus60xRead, 4);
			HwProfile::Count(HwProfile::Counter::Bus60xRead, 8);
			HwProfile::Count(HwProfile::Counter::DmaAram, 1024);

			Assert::AreEqual((uint64_t)12, HwProfile::counters[(size_t)HwProfile::Counter::Bus60xRead]);
			Assert::AreEqual((uint64_t)1024, HwProfile::counters[(size_t)HwProfile::Counter::DmaAram]);
			Assert::AreEqual((uint64_t)0, HwProfile::counters[(size_t)HwProfile::Counter::Bus60xWrite]);

			HwProfile::Reset();

			Assert::AreEqual((uint64_t)0, HwProfile::counters[(size_t)HwProfile::Counter::Bus60xRead]);
			Assert::AreEqual((uint64_t)0, HwProfile::counters[(size_t)HwProfile::Counter::DmaAram]);
		}

		// ------------------------------------------------------------------
		// The rate table
		// ------------------------------------------------------------------

		TEST_METHOD(RateTable_FirstReadingOnlyRecordsTheBaseline)
		{
			HwProfile::RateTable table;

			uint64_t values[(size_t)HwProfile::Counter::Max] = {};
			values[(size_t)HwProfile::Counter::Bus60xRead] = 4096;

			table.Add(values, 0.0);

			Assert::IsTrue(table.Sampled(), L"the baseline is a sample");
			Assert::IsFalse(table.Window() > 0.0, L"but it covers no time");
			Assert::IsTrue(Close(table.PerSecond(HwProfile::Counter::Bus60xRead), 0.0),
				L"a rate needs a window, and there is none yet");
			Assert::AreEqual((uint64_t)0, table.Total(HwProfile::Counter::Bus60xRead),
				L"the baseline is not traffic");
		}

		TEST_METHOD(RateTable_TurnsCounterDeltasIntoRatesPerEmulatedSecond)
		{
			HwProfile::RateTable table;

			uint64_t values[(size_t)HwProfile::Counter::Max] = {};

			values[(size_t)HwProfile::Counter::Bus60xRead] = 1000;
			table.Add(values, 0.0);				// baseline at t = 0

			// Half a second later the CPU read 1 MB and the DSP retired 30 000 instructions.
			values[(size_t)HwProfile::Counter::Bus60xRead] = 1000 + 1024 * 1024;
			values[(size_t)HwProfile::Counter::DspInstructions] = 30000;

			table.Add(values, 0.5);

			Assert::IsTrue(Close(table.Window(), 0.5), L"the window is what the caller measured");
			Assert::IsTrue(Close((double)table.Delta(HwProfile::Counter::Bus60xRead), 1024.0 * 1024.0), L"the delta is the traffic of the window");
			Assert::IsTrue(Close(table.PerSecond(HwProfile::Counter::Bus60xRead), 2.0 * 1024 * 1024), L"1 MB in half a second is 2 MB/s");
			Assert::IsTrue(Close(table.PerSecond(HwProfile::Counter::DspInstructions), 60000.0), L"30000 instructions in half a second is 60k/s");

			// The next window measures its own delta, and the totals accumulate.
			values[(size_t)HwProfile::Counter::Bus60xRead] = 1000 + 1024 * 1024;
			table.Add(values, 1.0);

			Assert::IsTrue(Close(table.PerSecond(HwProfile::Counter::Bus60xRead), 0.0), L"nothing moved in the second window");
			Assert::IsTrue(Close((double)table.Total(HwProfile::Counter::Bus60xRead), 1024.0 * 1024.0), L"the total is what the machine ever moved");
		}

		TEST_METHOD(RateTable_NoWindowIsNotAWindow)
		{
			HwProfile::RateTable table;

			uint64_t values[(size_t)HwProfile::Counter::Max] = {};
			values[(size_t)HwProfile::Counter::SplashRead] = 32;

			table.Add(values, 0.0);

			values[(size_t)HwProfile::Counter::SplashRead] = 64;
			table.Add(values, 0.0);				// a window of zero length

			Assert::IsTrue(Close(table.PerSecond(HwProfile::Counter::SplashRead), 0.0),
				L"a zero-length window must not be divided by");
			Assert::AreEqual((uint64_t)0, table.Total(HwProfile::Counter::SplashRead),
				L"and it must not be counted as traffic either");
		}

		TEST_METHOD(RateTable_CounterGoingBackwardsRestartsTheWindow)
		{
			HwProfile::RateTable table;

			uint64_t values[(size_t)HwProfile::Counter::Max] = {};
			values[(size_t)HwProfile::Counter::ViFrames] = 5000;

			table.Add(values, 0.0);

			// The machine was rebuilt between the two samples: the counter of the new one starts
			// from zero, and the report must not claim a negative rate.
			values[(size_t)HwProfile::Counter::ViFrames] = 60;
			table.Add(values, 1.0);

			Assert::IsTrue(Close(table.PerSecond(HwProfile::Counter::ViFrames), 60.0),
				L"the new machine's frames are what the window covers");
			Assert::AreEqual((uint64_t)60, table.Total(HwProfile::Counter::ViFrames));
		}

		// ------------------------------------------------------------------
		// The sampler
		// ------------------------------------------------------------------

		TEST_METHOD(Sample_NeedsAWholeWindowOfEmulatedTime)
		{
			const uint64_t ticksPerSecond = 486000000;

			// The first reading is the baseline.
			Assert::IsFalse(HwProfile::Sample(0, 0, 1000, ticksPerSecond), L"the baseline is not a measurement");

			HwProfile::Count(HwProfile::Counter::Bus60xWrite, 486);

			// A tenth of a second later the window is not due yet.
			Assert::IsFalse(HwProfile::Sample(0, 0, 1000 + ticksPerSecond / 10, ticksPerSecond), L"a tenth of a second is not a window");

			// A whole second later it is.
			Assert::IsTrue(HwProfile::Sample(0, 0, 1000 + ticksPerSecond, ticksPerSecond), L"a second is a window");
			Assert::IsTrue(Close(HwProfile::WindowSeconds(), 1.0), L"the window is one emulated second");
			Assert::IsTrue(Close(HwProfile::Rates().PerSecond(HwProfile::Counter::Bus60xWrite), 486.0), L"486 bytes over one emulated second");

			// Reporting again does not move the window.
			Assert::IsFalse(HwProfile::Sample(0, 0, 1000 + ticksPerSecond, ticksPerSecond), L"the window is already measured");

			// The instructions come from the caller, which is what the two cores count.
			Assert::IsTrue(HwProfile::Sample(486000000, 81000000, 1000 + 2 * ticksPerSecond, ticksPerSecond), L"the next window is due");
			Assert::IsTrue(Close(HwProfile::Rates().PerSecond(HwProfile::Counter::GekkoInstructions), 486000000.0), L"one emulated second of Gekko instructions");
			Assert::IsTrue(Close(HwProfile::Rates().PerSecond(HwProfile::Counter::DspInstructions), 81000000.0), L"one emulated second of DSP instructions");
		}

		// ------------------------------------------------------------------
		// The reports
		// ------------------------------------------------------------------

		TEST_METHOD(Format_UsesTheUnitsAReportIsReadIn)
		{
			std::string text;

			HwProfile::FormatBytes(512, text);
			Assert::IsTrue(text == "512 B", L"a byte count below a kilobyte is exact");

			HwProfile::FormatBytes(1024, text);
			Assert::IsTrue(text == "1.00 KB", L"traffic counts in powers of 1024");

			HwProfile::FormatBytes(3 * 1024 * 1024 + 512 * 1024, text);
			Assert::IsTrue(text == "3.50 MB");

			HwProfile::FormatCount(999, text);
			Assert::IsTrue(text == "999");

			HwProfile::FormatCount(1500, text);
			Assert::IsTrue(text == "1.50K", L"event counts in powers of 1000");

			HwProfile::FormatRate(2.5 * 1024 * 1024, true, text);
			Assert::IsTrue(text == "2.50 MB/s", L"a rate keeps its fraction");

			HwProfile::FormatRate(59.94, false, text);
			Assert::IsTrue(text == "60 /s", L"a small event rate is rounded to whole events");
		}

		TEST_METHOD(ReportToLines_CoversEveryChannel)
		{
			// One measured window with a little traffic on a few channels.
			HwProfile::Sample(0, 0, 0, 486000000);
			HwProfile::Count(HwProfile::Counter::Bus60xRead, 1234);
			HwProfile::Count(HwProfile::Counter::GfxPrimitives, 7);
			HwProfile::Count(HwProfile::Counter::ViFrames, 60);
			HwProfile::Sample(486000000, 0, 486000000, 486000000);

			std::vector<std::string> lines;
			HwProfile::ReportToLines(lines);

			Assert::AreEqual((size_t)1 + 18, lines.size(), L"a header and one line per channel");

			std::string all;
			for (size_t i = 0; i < lines.size(); i++)
			{
				all += lines[i];
				all += "\n";
			}

			// The channels the issue asks for, by the names the report uses.
			const char* expected[] =
			{
				"60x bus read", "60x bus write",
				"Flipper/Splash read", "Flipper/Splash write",
				"PI interrupts", "Write gather buffer", "PI/CP FIFO", "Audio mixer input",
				"DMA EXI", "DMA DI", "DMA DSP", "DMA AI", "DMA ARAM",
				"GFX primitives", "GFX vertices", "VI frames",
				"Gekko instructions", "DSP instructions",
			};

			for (size_t i = 0; i < _countof(expected); i++)
			{
				Assert::IsTrue(all.find(expected[i]) != std::string::npos,
					L"the report must have a line for every profiled channel");
			}

			Assert::IsTrue(all.find("1.21 KB/s") != std::string::npos, L"the measured traffic is in the table");
			Assert::IsTrue(all.find("60 /s") != std::string::npos, L"... and so is the frame rate");
		}

		TEST_METHOD(ReportToMarkdown_IsATableOfTheSameNumbers)
		{
			HwProfile::Sample(0, 0, 0, 486000000);
			HwProfile::Count(HwProfile::Counter::SplashRead, 1024 * 1024);
			HwProfile::Sample(0, 0, 486000000, 486000000);

			std::string markdown;
			HwProfile::ReportToMarkdown(markdown);

			Assert::IsTrue(markdown.find("# HW interface profile") == 0, L"the report has a heading");

			// The header row and the separator of the Markdown table.
			Assert::IsTrue(markdown.find("| Channel | Rate | Total |\n|---|---|---|\n") != std::string::npos);

			Assert::IsTrue(markdown.find("| Flipper/Splash read | 1.00 MB/s |") != std::string::npos,
				L"a megabyte over one emulated second is a megabyte per second");
		}
	};
}
