// BP register space coverage tests.
//
// Every register of the BP (bypass) space 0x00-0xFF belongs to one pipeline block, and the load
// walks the blocks in a fixed order (gfx.md 10.1):
//
//     SU -> RAS -> PE -> BUMP -> TX -> TEV
//
// A register that no block claims is reported as "Unknown reg load" and is silently lost, which is
// the failure mode this file guards against: the tables below are the ownership map of the
// specification (gfx-su.md 4, gfx-ras1.md 4, gfx-pe.md 6, gfx-bump.md 4.1, gfx-tc.md 4.2/4.3,
// gfx-tev.md 4.1), so a hole in the emulator shows up as a failing register here.

#include "pch.h"
#include "gfx_test_common.h"

using namespace GfxUnitTest;

namespace pureikyubutest
{
	TEST_CLASS(GfxBpMapTest)
	{
		static GfxTestMachine& M()
		{
			GfxTestMachine& m = Machine();
			m.Reset();
			return m;
		}

		//! Load a BP register and report what the emulator logged about it.
		static std::string LoadAndLog(GfxTestMachine& m, unsigned index, uint32_t value)
		{
			ClearTestLog();
			EnableTestLog(true);
			m.BpLoad(index, value);
			std::string log = TestLogText();
			EnableTestLog(false);
			return log;
		}

	public:

		// =========================================================================================
		// The ownership map
		// =========================================================================================

		// gen mode (SU), 0x00-0x04
		TEST_METHOD(BpMap_GenModeAndSampleLocationsAreDecoded)
		{
			GfxTestMachine& m = M();

			for (unsigned index = 0x00; index <= 0x04; index++)
			{
				std::string log = LoadAndLog(m, index, 0x00010101);
				wchar_t msg[64];
				swprintf_s(msg, L"register 0x%02X", index);
				Assert::IsTrue(log.find("Unknown") == std::string::npos, msg);
			}
		}

		// the bump / indirect unit, 0x06-0x1F
		TEST_METHOD(BpMap_TheBumpUnitOwnsTheIndirectRegisters)
		{
			GfxTestMachine& m = M();

			for (unsigned index = 0x06; index <= 0x1f; index++)
			{
				std::string log = LoadAndLog(m, index, 0x00123456);

				wchar_t msg[128];
				swprintf_s(msg, L"register 0x%02X (%s)", index, Widen(log).c_str());
				Assert::IsTrue(log.find("Unknown") == std::string::npos, msg);
			}
		}

		// setup unit, 0x20-0x23 and 0x30-0x3F
		TEST_METHOD(BpMap_TheSetupUnitOwnsItsRegisters)
		{
			GfxTestMachine& m = M();

			const unsigned indices[] = {
				0x20, 0x21, 0x22, 0x23,
				0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
				0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
			};

			for (unsigned index : indices)
			{
				std::string log = LoadAndLog(m, index, 0x00010101);
				wchar_t msg[128];
				swprintf_s(msg, L"register 0x%02X (%s)", index, Widen(log).c_str());
				Assert::IsTrue(log.find("Unknown") == std::string::npos, msg);
			}
		}

		// rasterizer 1, 0x24-0x2F
		TEST_METHOD(BpMap_TheRasterizerOwnsItsRegisters)
		{
			GfxTestMachine& m = M();

			for (unsigned index = 0x24; index <= 0x2f; index++)
			{
				std::string log = LoadAndLog(m, index, 0x00010101);
				wchar_t msg[128];
				swprintf_s(msg, L"register 0x%02X (%s)", index, Widen(log).c_str());
				Assert::IsTrue(log.find("Unknown") == std::string::npos, msg);
			}
		}

		// the texture unit, 0x60-0x69 and 0x80-0xBB
		TEST_METHOD(BpMap_TheTextureUnitOwnsItsRegisters)
		{
			GfxTestMachine& m = M();

			// The texture registers that load data (LOADTLUT) need a valid main memory image; the
			// addresses used here point into the zeroed test memory, which the decoder tolerates.
			for (unsigned index = 0x60; index <= 0x69; index++)
			{
				std::string log = LoadAndLog(m, index, 0x00000000);
				wchar_t msg[128];
				swprintf_s(msg, L"register 0x%02X (%s)", index, Widen(log).c_str());
				Assert::IsTrue(log.find("Unknown") == std::string::npos, msg);
			}

			// 0x9C-0x9F are not assigned: the per-image block is 0x80-0x9B and 0xA0-0xBB
			const unsigned perImage[] = {
				0x80, 0x84, 0x88, 0x8c, 0x90, 0x94, 0x98, 0x9b,
				0xa0, 0xa4, 0xa8, 0xac, 0xb0, 0xb4, 0xb8, 0xbb,
			};

			for (unsigned index : perImage)
			{
				std::string log = LoadAndLog(m, index, 0x00000000);
				wchar_t msg[128];
				swprintf_s(msg, L"register 0x%02X (%s)", index, Widen(log).c_str());
				Assert::IsTrue(log.find("Unknown") == std::string::npos, msg);
			}
		}

		// the TEV, 0xC0-0xFD
		TEST_METHOD(BpMap_TheTevOwnsItsRegisters)
		{
			GfxTestMachine& m = M();

			for (unsigned index = 0xc0; index <= 0xfd; index++)
			{
				std::string log = LoadAndLog(m, index, 0x00010101);
				wchar_t msg[128];
				swprintf_s(msg, L"register 0x%02X (%s)", index, Widen(log).c_str());
				Assert::IsTrue(log.find("Unknown") == std::string::npos, msg);
			}
		}

		// the sub-sample mask, 0xFE
		TEST_METHOD(BpMap_TheSubSampleMaskIsDecoded)
		{
			GfxTestMachine& m = M();
			std::string log = LoadAndLog(m, 0xfe, 0x00aaaaaa);
			Assert::IsTrue(log.find("Unknown") == std::string::npos,
				Widen("register 0xFE: " + log).c_str());
		}

		// =========================================================================================
		// The registers the map does not assign
		// =========================================================================================

		// 0x05, 0x6A-0x7F and 0xFF are not claimed by any block in the specification. They are
		// expected to be reported as unknown, and pinning that down keeps the map honest: if one of
		// them turns out to be a real register, the test reminds us to add it.
		TEST_METHOD(BpMap_TheUnassignedRegistersAreReported)
		{
			GfxTestMachine& m = M();

			const unsigned unassigned[] = { 0x05, 0x6a, 0x7f, 0xff };

			for (unsigned index : unassigned)
			{
				std::string log = LoadAndLog(m, index, 0);

				wchar_t msg[128];
				swprintf_s(msg, L"register 0x%02X", index);
				Assert::IsTrue(log.find("Unknown") != std::string::npos, msg);
			}
		}
	};
}
