// Guest frame profiler unit tests.
//
// The subject is src/guestprof.cpp. The interesting half of it - the exclusive host cycle
// attribution and the frame records - does not need a running emulator: the profiler is armed with
// `CaptureOptions::ticksPerSecond` and then driven with synthetic frame boundaries, synthetic
// basic blocks and synthetic disc reads. That is what these tests do, and it is also why the
// seam exists (see the comment on the field).
//
// What the tests pin down:
//
//   * the unit table and the channel table the report is built from are complete - every entry of
//     the two enumerations has a name, a group and a description, and none is listed twice;
//   * the first frame boundary only records the baseline (a delta needs two readings);
//   * one boundary is one frame record, and the frame counter and the phase histogram follow it;
//   * a disc read that overlaps the movie range marks its frame, and every later frame stays
//     marked (the drive reads the stream in bursts, so the odd frame in between may not read);
//   * the capture stops by itself once it holds the movie frames it was asked for;
//   * the scopes are EXCLUSIVE: a nested scope's cycles are subtracted from its parent's, so one
//     cycle of host time is never charged to two units;
//   * the document is valid JSON and holds what the report reads back.

#include "pch.h"

#include <chrono>

using namespace Debug;

namespace GuestProfUnitTest
{
	TEST_CLASS(GuestProfTest)
	{
		// The counters are process-wide; every test starts from a fresh machine.
		TEST_METHOD_INITIALIZE(Setup)
		{
			GuestProf::StopCapture();
			HwProfile::Reset();
		}

		TEST_METHOD_CLEANUP(Cleanup)
		{
			GuestProf::StopCapture();
			remove(CaptureFile());
		}

	public:

		static const char* CaptureFile()
		{
			return "guestprof_test.json";
		}

		// Arm a capture that needs no machine.
		static GuestProf::CaptureOptions Arm(size_t movieFrames = 4, size_t maxFrames = 100,
			uint64_t movieOffset = 0x1000, uint64_t movieLength = 0x100)
		{
			GuestProf::CaptureOptions options;
			options.outputFile = CaptureFile();
			options.image = "test";
			options.ticksPerSecond = 40500000;	// NTSC one_second, so a frame is 1350000 ticks
			options.movieOffset = movieOffset;
			options.movieLength = movieLength;
			options.movieFrames = movieFrames;
			options.maxFrames = maxFrames;

			GuestProf::StartCapture(options);
			return options;
		}

		// An NTSC frame of emulated time.
		static const uint64_t FrameTicks = 1350000;

		static void ReadCapture(std::string& text)
		{
			auto bytes = Util::FileLoad(Util::StringToWstring(CaptureFile()));
			text.assign((const char*)bytes.data(), bytes.size());
		}

		// ------------------------------------------------------------------
		// The tables
		// ------------------------------------------------------------------

		TEST_METHOD(Units_EveryUnitHasANameAGroupAndADescription)
		{
			for (size_t i = 0; i < (size_t)GuestProf::Unit::Max; i++)
			{
				GuestProf::Unit unit = (GuestProf::Unit)i;

				const char* name = GuestProf::UnitName(unit);
				const char* group = GuestProf::UnitGroup(unit);
				const char* about = GuestProf::UnitDescription(unit);

				Assert::IsTrue(name != nullptr && name[0] != 0 && strcmp(name, "?") != 0,
					L"a unit must have a name");
				Assert::IsTrue(group != nullptr && group[0] != 0 && strcmp(group, "?") != 0,
					L"a unit must belong to a group");
				Assert::IsTrue(about != nullptr && about[0] != 0,
					L"a unit must describe what it covers");
			}
		}

		TEST_METHOD(Units_AreListedExactlyOnce)
		{
			// The table is built by hand, so a copy/paste that loses or repeats an entry has to be
			// caught. The names are unique, which is what the report relies on.
			std::vector<std::string> names;

			for (size_t i = 0; i < (size_t)GuestProf::Unit::Max; i++)
			{
				names.push_back(GuestProf::UnitName((GuestProf::Unit)i));
			}

			for (size_t i = 0; i < names.size(); i++)
			{
				for (size_t j = i + 1; j < names.size(); j++)
				{
					Assert::IsTrue(names[i] != names[j], L"two units share a name");
				}
			}
		}

		TEST_METHOD(Channels_EveryChannelHasAName)
		{
			for (size_t i = 0; i < (size_t)HwProfile::Counter::Max; i++)
			{
				const char* name = HwProfile::CounterName((HwProfile::Counter)i);
				Assert::IsTrue(name != nullptr && name[0] != 0 && strcmp(name, "?") != 0,
					L"every channel the frame record carries must have a name");
			}
		}

		// ------------------------------------------------------------------
		// The frames
		// ------------------------------------------------------------------

		TEST_METHOD(Capture_FirstBoundaryIsOnlyTheBaseline)
		{
			Arm();

			Assert::IsTrue(GuestProf::Capturing(), L"the capture is armed");

			// Nothing is recorded until two boundaries have been seen: a delta needs two readings.
			Assert::IsFalse(GuestProf::CaptureComplete());

			uint64_t ticks = 1000;
			GuestProf::NoteDiscRead(0x2000, 32);
			GuestProf::NoteBlock(0x80003100, 40);
			GuestProf::FrameBoundary(ticks);

			Assert::IsFalse(GuestProf::CaptureComplete(), L"the baseline records no frame");

			// The second boundary closes the first real frame.
			ticks += FrameTicks;
			GuestProf::NoteBlock(0x80003100, 40);
			GuestProf::FrameBoundary(ticks);

			GuestProf::StopCapture();

			std::string document;
			ReadCapture(document);

			Assert::IsTrue(document.find("\"format\": \"pureikyubu-guestprof\"") != std::string::npos,
				L"the document names its format");

			// Exactly one frame record: `"i": 1` is the second frame and must not be there.
			Assert::IsTrue(document.find("\"i\": 0") != std::string::npos, L"the first frame is recorded");
			Assert::IsTrue(document.find("\"i\": 1") == std::string::npos, L"the baseline is not a frame");
		}

		TEST_METHOD(Capture_MarksTheFramesThatReadTheMovieStream)
		{
			// Two movie frames are enough to finish the capture.
			Arm(2);

			uint64_t ticks = 0;

			// Frame 0 (the baseline): a read outside the movie range.
			GuestProf::NoteDiscRead(0x2000, 32);
			ticks += FrameTicks;
			GuestProf::FrameBoundary(ticks);

			// Frame 1: still no movie.
			GuestProf::NoteDiscRead(0x3000, 32);
			ticks += FrameTicks;
			GuestProf::FrameBoundary(ticks);

			// Frame 2: the first read that overlaps the movie range starts the stream.
			GuestProf::NoteDiscRead(0x1080, 32);
			ticks += FrameTicks;
			GuestProf::FrameBoundary(ticks);

			Assert::IsFalse(GuestProf::CaptureComplete(), L"one movie frame is not the two that were asked for");

			// Frame 3: no read at all, and the frame is still a movie frame.
			ticks += FrameTicks;
			GuestProf::FrameBoundary(ticks);

			Assert::IsTrue(GuestProf::CaptureComplete(), L"the capture has its two movie frames");

			GuestProf::StopCapture();
			Assert::IsFalse(GuestProf::Capturing(), L"the capture is disarmed");

			std::string document;
			ReadCapture(document);

			// One record before the movie and two of it: the first boundary was the baseline, the
			// second closed the frame that read nothing, the third started the stream and the
			// fourth stayed in it.
			Assert::IsTrue(document.find("\"startFrame\": 1") != std::string::npos,
				L"the movie starts at the second frame record");
			Assert::IsTrue(document.find("\"frames\": 2 }") != std::string::npos,
				L"two movie frames were counted");

			// The phase histograms are per phase: the movie phase saw the two later frames.
			Assert::IsTrue(document.find("\"phase1\": { \"frames\": 2") != std::string::npos,
				L"phase 1 is the frames that read the movie");
			Assert::IsTrue(document.find("\"phase0\": { \"frames\": 1") != std::string::npos,
				L"phase 0 is the frames that did not");
		}

		TEST_METHOD(Capture_RecordsTheHotBlocksOfAFrame)
		{
			Arm();

			uint64_t ticks = 0;
			GuestProf::FrameBoundary(ticks);

			ticks += FrameTicks;
			GuestProf::NoteBlock(0x80003100, 10);
			GuestProf::NoteBlock(0x80003100, 30);
			GuestProf::NoteBlock(0x80300000, 5);
			GuestProf::FrameBoundary(ticks);

			GuestProf::StopCapture();

			std::string document;
			ReadCapture(document);

			// The histogram is bucketed by 4 KB, and the top list is sorted by weight.
			// 0x80003100 falls in the bucket that starts at 0x80003000 (2147495936).
			Assert::IsTrue(document.find("\"pcTop\": [[2147495936, 40], [2150629376, 5]]") != std::string::npos,
				L"the two heaviest buckets are reported, heaviest first");
			Assert::IsTrue(document.find("\"pcTotal\": 45") != std::string::npos,
				L"the frame's whole block weight is reported with them");
		}

		TEST_METHOD(Capture_StopsAtTheFrameCapWhenNoMovieIsPlayed)
		{
			// No movie range at all: the capture can only stop at the hard cap.
			Arm(60, 3, 0, 0);

			uint64_t ticks = 0;
			GuestProf::FrameBoundary(ticks);			// the baseline

			for (int i = 0; i < 3; i++)
			{
				ticks += FrameTicks;
				GuestProf::FrameBoundary(ticks);
			}

			Assert::IsTrue(GuestProf::CaptureComplete(), L"the frame cap ends the capture");

			GuestProf::StopCapture();

			std::string document;
			ReadCapture(document);

			Assert::IsTrue(document.find("\"phase1\": { \"frames\": 0") != std::string::npos,
				L"no frame was a movie frame");
			Assert::IsTrue(document.find("\"phase0\": { \"frames\": 3") != std::string::npos,
				L"all three frames are in phase 0");
		}

		// ------------------------------------------------------------------
		// The exclusive attribution
		// ------------------------------------------------------------------

		TEST_METHOD(Scopes_ChargeTheParentOnlyWhatTheChildrenDidNotTake)
		{
			Arm();

			uint64_t ticks = 0;
			GuestProf::FrameBoundary(ticks);			// the baseline

			ticks += FrameTicks;

			// Two sleeps of the same length, one inside the other. If the parent swallowed the
			// child, its count would be the whole elapsed time; the test is written against the
			// *measured* elapsed time rather than against the requested sleep, because a host
			// sleep is only as accurate as its scheduler's timer granularity.
			auto wall0 = std::chrono::steady_clock::now();
			{
				GuestProf::Scope outer(GuestProf::Unit::Rasterizer);
				Thread::Sleep(20);

				{
					GuestProf::Scope inner(GuestProf::Unit::TextureEnv);
					Thread::Sleep(20);
				}
			}
			auto wall1 = std::chrono::steady_clock::now();
			double elapsed = std::chrono::duration<double>(wall1 - wall0).count();

			GuestProf::FrameBoundary(ticks);
			GuestProf::StopCapture();

			std::string document;
			ReadCapture(document);

			auto host = ExtractHostArray(document);
			Assert::AreEqual((size_t)GuestProf::Unit::Max, host.size(),
				L"the frame record carries one host cycle count per unit");

			double raster = (double)host[(size_t)GuestProf::Unit::Rasterizer] / ExtractNumber(document, "\"tscHz\": ");
			double tev = (double)host[(size_t)GuestProf::Unit::TextureEnv] / ExtractNumber(document, "\"tscHz\": ");

			// Both scopes slept, so both must have been charged something ...
			Assert::IsTrue(tev > elapsed * 0.25, L"the child reports its own sleep");
			Assert::IsTrue(raster > elapsed * 0.25, L"the parent reports the time it owned itself");

			// ... and the parent must NOT report the child's: its share is one of the two sleeps,
			// not both.
			Assert::IsTrue(raster < elapsed * 0.85,
				L"the parent's count is the part its children did not take");
		}

		TEST_METHOD(Charge_AddsToTheUnitWithoutAScope)
		{
			Arm();

			uint64_t ticks = 0;
			GuestProf::FrameBoundary(ticks);
			ticks += FrameTicks;

			GuestProf::Charge(GuestProf::Unit::GekkoIdle, 123456, 7);
			GuestProf::FrameBoundary(ticks);
			GuestProf::StopCapture();

			std::string document;
			ReadCapture(document);

			// The charge is exact: nothing else was charged to the idle skip.
			Assert::IsTrue(document.find("\"host\": [") != std::string::npos, L"the record has the host array");

			auto host = ExtractHostArray(document);
			Assert::AreEqual((uint64_t)123456, host[(size_t)GuestProf::Unit::GekkoIdle]);
		}

		// ------------------------------------------------------------------
		// Helpers
		// ------------------------------------------------------------------

		// Read the first number that follows `key` out of the document. The profiler writes its
		// scalars as plain JSON numbers, so this is enough for the two the tests need.
		static double ExtractNumber(const std::string& document, const std::string& key)
		{
			size_t at = document.find(key);
			Assert::IsTrue(at != std::string::npos, L"the document holds the number the test asks for");
			at += key.size();

			size_t end = at;
			while (end < document.size() &&
				((document[end] >= '0' && document[end] <= '9') || document[end] == '.' || document[end] == 'e' || document[end] == 'E' || document[end] == '+' || document[end] == '-'))
			{
				end++;
			}

			return strtod(document.substr(at, end - at).c_str(), nullptr);
		}

		// Read the first frame's `"host": [...]` array out of the document.
		static std::vector<uint64_t> ExtractHostArray(const std::string& document)
		{
			std::vector<uint64_t> values;

			size_t at = document.find("\"host\": [");
			Assert::IsTrue(at != std::string::npos, L"the document holds a host cycle array");

			at = document.find('[', at) + 1;
			size_t end = document.find(']', at);
			Assert::IsTrue(end != std::string::npos, L"the host array is closed");

			size_t cursor = at;
			while (cursor < end)
			{
				while (cursor < end && (document[cursor] == ' ' || document[cursor] == ','))
					cursor++;

				size_t start = cursor;
				while (cursor < end && document[cursor] >= '0' && document[cursor] <= '9')
					cursor++;

				if (cursor > start)
				{
					values.push_back(strtoull(document.substr(start, cursor - start).c_str(), nullptr, 10));
				}
				else
				{
					cursor++;
				}
			}

			return values;
		}
	};
}
