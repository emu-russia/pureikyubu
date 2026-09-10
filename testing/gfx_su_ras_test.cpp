// Setup Unit (SU) and rasterizer register tests.
//
// The SU and RAS blocks own the part of the BP register space that describes *how* a primitive is
// rasterized: the scissor box, the line/point size, the sub-sample mask, the texture coordinate
// sizes and the per-stage texture bindings (RAS1_TREF / SS0 / SS1 / IREF).
//
// See specs: gfx-su.md (0x00-0x05, 0x20-0x23, 0x30-0x3F, 0xFE), gfx-ras1.md (0x24-0x2F),
// gfx-ras2.md (GEN_MODE and GEN_MSLOC).
//
// The registers reach the SU through the bypass (BP) chain, which walks
// SU -> RAS -> PE -> BUMP -> TX -> TEV; every test programs the block through that chain, so a
// register that is swallowed by the wrong block (or by none at all) fails here.

#include "pch.h"
#include "gfx_test_common.h"

using namespace GfxUnitTest;

namespace pureikyubutest
{
	TEST_CLASS(GfxSuRasTest)
	{
		static GfxTestMachine& M()
		{
			GfxTestMachine& m = Machine();
			m.Reset();
			return m;
		}

		//! Load one BP register and return what the emulator reported about it.
		static std::string LoadAndLog(GfxTestMachine& m, unsigned index, uint32_t value)
		{
			ClearTestLog();
			EnableTestLog(true);
			m.BpLoad(index, value);
			std::string log = TestLogText();
			EnableTestLog(false);
			return log;
		}

		static void AssertDecoded(const std::string& log, const char* regName)
		{
			Assert::IsTrue(log.find("Unknown") == std::string::npos,
				Widen(std::string(regName) + " is not decoded by any block: " + log).c_str());
		}

	public:

		// =========================================================================================
		// GEN_MODE (0x00) and GEN_MSLOC (0x01-0x04)
		// =========================================================================================

		TEST_METHOD(Su_GenModeIsDecodedFieldByField)
		{
			GfxTestMachine& m = M();

			// gfx-su.md 4.1 / su.h: ntex 3:0, ncol 6:4, flat_en 8, ms_en 9, ntev 13:10,
			// reject_en 15:14, nbmp 18:16, zfreeze 19
			uint32_t value =
				(3u << 0) |		// ntex
				(1u << 4) |		// ncol
				(1u << 8) |		// flat_en
				(1u << 9) |		// ms_en
				(2u << 10) |	// ntev
				(1u << 14) |	// reject_en
				(1u << 16) |	// nbmp
				(1u << 19);		// zfreeze

			m.BpLoad(GEN_MODE_ID, value);

			const GFX::GenMode& gm = m.gfx->genmode;
			Assert::AreEqual<unsigned>(3, gm.ntex, L"ntex");
			Assert::AreEqual<unsigned>(1, gm.ncol, L"ncol");
			Assert::AreEqual<unsigned>(1, gm.flat_en, L"flat_en");
			Assert::AreEqual<unsigned>(1, gm.ms_en, L"ms_en");
			Assert::AreEqual<unsigned>(2, gm.ntev, L"ntev");
			Assert::AreEqual<unsigned>(1, gm.reject_en, L"reject_en");
			Assert::AreEqual<unsigned>(1, gm.nbmp, L"nbmp");
			Assert::AreEqual<unsigned>(1, gm.zfreeze, L"zfreeze");
		}

		TEST_METHOD(Su_MslocRegistersAreDecoded)
		{
			GfxTestMachine& m = M();

			// GEN_MSLOC0..3 hold four 4-bit X/Y sample offsets packed as
			// xs0 3:0, ys0 7:4, xs1 11:8, ys1 15:12, xs2 19:16, ys2 23:20
			for (unsigned i = 0; i < 4; i++)
			{
				uint32_t value = (1u + i) | ((4u + i) << 4) | ((8u + i) << 8) | ((12u + i) << 12) |
					((2u + i) << 16) | ((6u + i) << 20);
				m.BpLoad(GEN_MSLOC0_ID + i, value);
			}

			for (unsigned i = 0; i < 4; i++)
			{
				const GFX::GenMsloc& loc = m.gfx->msloc[i];
				Assert::AreEqual<unsigned>(1u + i, loc.xs0, L"xs0");
				Assert::AreEqual<unsigned>(4u + i, loc.ys0, L"ys0");
				Assert::AreEqual<unsigned>(8u + i, loc.xs1, L"xs1");
				Assert::AreEqual<unsigned>(12u + i, loc.ys1, L"ys1");
				Assert::AreEqual<unsigned>(2u + i, loc.xs2, L"xs2");
				Assert::AreEqual<unsigned>(6u + i, loc.ys2, L"ys2");
			}
		}

		// =========================================================================================
		// Scissor box (0x20, 0x21)
		// =========================================================================================

		// gfx-su.md 4.1: the scissor box is kept in the SU, and the hardware screen origin is
		// (-342, -342), so the register values carry that offset. SU_SCIS0 packs suy in bits 11:0 and
		// sux in bits 23:12 (SU_SCIS1 packs suh and suw the same way).
		TEST_METHOD(Su_ScissorRegistersAreDecoded)
		{
			GfxTestMachine& m = M();

			m.BpLoad(SU_SCIS0_ID, (50 + 342) | ((100 + 342) << 12));
			m.BpLoad(SU_SCIS1_ID, (99 + 342) | ((199 + 342) << 12));

			const GFX::SUState& su = m.gfx->su->State();
			Assert::AreEqual<unsigned>(442, su.scis0.sux, L"sux");
			Assert::AreEqual<unsigned>(392, su.scis0.suy, L"suy");
			Assert::AreEqual<unsigned>(541, su.scis1.suw, L"suw");
			Assert::AreEqual<unsigned>(441, su.scis1.suh, L"suh");
		}

		// =========================================================================================
		// Line / point size (0x22) and sub-sample mask (0xFE)
		// =========================================================================================

		TEST_METHOD(Su_LinePointSizeIsDecoded)
		{
			GfxTestMachine& m = M();

			uint32_t value =
				(8u << 0) |		// lsize
				(4u << 8) |		// psize
				(1u << 16) |	// ltoff
				(2u << 19) |	// ptoff
				(1u << 22);		// fieldmode

			AssertDecoded(LoadAndLog(m, SU_LPSIZE_ID, value), "SU_LPSIZE (0x22)");
		}

		TEST_METHOD(Su_SsMaskIsDecoded)
		{
			GfxTestMachine& m = M();
			AssertDecoded(LoadAndLog(m, SU_SSMASK_ID, 0x00aaaaaa), "SU_SSMASK (0xFE)");
		}

		TEST_METHOD(Su_PerfRegisterIsDecoded)
		{
			GfxTestMachine& m = M();
			AssertDecoded(LoadAndLog(m, SU_PERF_ID, 0x00000000), "SU_PERF (0x23)");
		}

		// =========================================================================================
		// Texture coordinate sizes (0x30-0x3F)
		// =========================================================================================

		// gfx-su.md 4.4-4.6: the sixteen registers 0x30-0x3F are eight pairs, one pair per texture
		// coordinate unit: an even index is the S size (su_ts0) and the odd one its T size (su_ts1).
		TEST_METHOD(Su_EveryTextureUnitHasItsOwnSsizeAndTsizeRegister)
		{
			GfxTestMachine& m = M();

			for (unsigned i = 0; i < 8; i++)
			{
				uint32_t ssize = (0x100 + i) | (1u << 16);		// + bs
				uint32_t tsize = (0x200 + i) | (1u << 16);		// + bt
				m.BpLoad(SU_SSIZE0_ID + i * 2, ssize);
				m.BpLoad(SU_TSIZE0_ID + i * 2, tsize);
			}

			const GFX::SUState& su = m.gfx->su->State();

			for (unsigned i = 0; i < 8; i++)
			{
				wchar_t msg[64];
				swprintf_s(msg, L"ssize of texture unit %u", i);
				Assert::AreEqual<unsigned>(0x100 + i, su.ssize[i].ssize, msg);
				Assert::AreEqual<unsigned>(1, su.ssize[i].bs, L"bs");

				swprintf_s(msg, L"tsize of texture unit %u", i);
				Assert::AreEqual<unsigned>(0x200 + i, su.tsize[i].tsize, msg);
				Assert::AreEqual<unsigned>(1, su.tsize[i].bt, L"bt");
			}
		}

		// The S and T size registers of one unit must not overwrite each other.
		TEST_METHOD(Su_SsizeAndTsizeOfTheSameUnitAreIndependent)
		{
			GfxTestMachine& m = M();

			m.BpLoad(SU_SSIZE3_ID, 0x00000011);
			m.BpLoad(SU_TSIZE3_ID, 0x00000022);

			const GFX::SUState& su = m.gfx->su->State();
			Assert::AreEqual<unsigned>(0x11, su.ssize[3].ssize, L"ssize[3]");
			Assert::AreEqual<unsigned>(0x22, su.tsize[3].tsize, L"tsize[3]");
		}

		// =========================================================================================
		// RAS1 registers (0x24-0x2F)
		// =========================================================================================

		TEST_METHOD(Ras_TrefRegistersAreDecodedFieldByField)
		{
			GfxTestMachine& m = M();

			// Even stage: ti0 = 3, tc0 = 2, te0 = 1, cc0 = 5; odd stage: ti1 = 4, tc1 = 1, te1 = 0, cc1 = 6
			uint32_t value =
				(3u << 0) | (2u << 3) | (1u << 6) | (5u << 7) |
				(4u << 12) | (1u << 15) | (0u << 18) | (6u << 19);

			m.BpLoad(RAS1_TREF0_ID, value);

			const GFX::RAS1_TREF& tref = m.gfx->ras->Tref(0);
			Assert::AreEqual<unsigned>(3, tref.ti0, L"ti0");
			Assert::AreEqual<unsigned>(2, tref.tc0, L"tc0");
			Assert::AreEqual<unsigned>(1, tref.te0, L"te0");
			Assert::AreEqual<unsigned>(5, tref.cc0, L"cc0");
			Assert::AreEqual<unsigned>(4, tref.ti1, L"ti1");
			Assert::AreEqual<unsigned>(1, tref.tc1, L"tc1");
			Assert::AreEqual<unsigned>(0, tref.te1, L"te1");
			Assert::AreEqual<unsigned>(6, tref.cc1, L"cc1");
		}

		// gfx-ras1.md 4.4 / gfx-ras2.md 4.3: one TREF register per *pair* of TEV stages, so stages
		// 0 and 1 share TREF0, stages 2 and 3 share TREF1, and so on. The even stage of the pair
		// reads the *0 fields of the register and the odd stage the *1 fields.
		TEST_METHOD(Ras_EveryTevStageIsBoundToItsTrefPair)
		{
			GfxTestMachine& m = M();

			for (unsigned pair = 0; pair < 8; pair++)
			{
				// Every field is 3 bits wide, so the test values have to stay inside that range
				unsigned ti1 = (pair ^ 4u) & 7u;
				unsigned cc1 = (pair ^ 2u) & 7u;

				uint32_t value =
					(pair << 0) | (pair << 3) | (1u << 6) | (pair << 7) |
					(ti1 << 12) | (1u << 15) | (1u << 18) | (cc1 << 19);
				m.BpLoad(RAS1_TREF0_ID + pair, value);
			}

			for (unsigned stage = 0; stage < 16; stage++)
			{
				unsigned pair = stage >> 1;
				bool odd = (stage & 1) != 0;

				const GFX::RAS1_TREF* tref = m.gfx->ras->GetTref(stage);

				wchar_t msg[64];

				swprintf_s(msg, L"stage %u uses TREF%u", stage, pair);
				Assert::IsTrue(tref == &m.gfx->ras->Tref(pair), msg);

				swprintf_s(msg, L"stage %u texture image id", stage);
				Assert::AreEqual<unsigned>(odd ? ((pair ^ 4u) & 7u) : pair, odd ? tref->ti1 : tref->ti0, msg);

				swprintf_s(msg, L"stage %u texture enable", stage);
				Assert::AreEqual<unsigned>(1, odd ? tref->te1 : tref->te0, msg);

				swprintf_s(msg, L"stage %u colour source", stage);
				Assert::AreEqual<unsigned>(odd ? ((pair ^ 2u) & 7u) : pair, odd ? tref->cc1 : tref->cc0, msg);
			}
		}

		// RAS1_SS0/1 hold four 4-bit shift scales: the S and T shift of the first indirect stage of
		// the register (`ss0` in bits 3:0, `ts0` in bits 7:4) and those of the second one (`ss1` in
		// bits 11:8, `ts1` in bits 15:12), see gfx-ras1.md 4.2. (This test used to assert a 16-bit
		// layout - ss0 in the low half of the word and ts0 in the high one - but the fields are 4 bits
		// wide in the hardware definition, so the decode was corrected when the register was given its
		// backend effect.)
		TEST_METHOD(Ras_SsAndIrefRegistersAreDecoded)
		{
			GfxTestMachine& m = M();

			// SS0: stage 0 shifts by 1/2, stage 1 by 3/4; SS1: stage 2 by 5/6, stage 3 by 7/8
			m.BpLoad(RAS1_SS0_ID, 1u | (2u << 4) | (3u << 8) | (4u << 12));
			m.BpLoad(RAS1_SS1_ID, 5u | (6u << 4) | (7u << 8) | (8u << 12));
			m.BpLoad(RAS1_IREF_ID, 0x00000007);

			Assert::AreEqual<unsigned>(1, m.gfx->ras->SS(0).ss0, L"SS0.ss0");
			Assert::AreEqual<unsigned>(2, m.gfx->ras->SS(0).ts0, L"SS0.ts0");
			Assert::AreEqual<unsigned>(3, m.gfx->ras->SS(0).ss1, L"SS0.ss1");
			Assert::AreEqual<unsigned>(4, m.gfx->ras->SS(0).ts1, L"SS0.ts1");
			Assert::AreEqual<unsigned>(5, m.gfx->ras->SS(1).ss0, L"SS1.ss0");
			Assert::AreEqual<unsigned>(6, m.gfx->ras->SS(1).ts0, L"SS1.ts0");
			Assert::AreEqual<unsigned>(7, m.gfx->ras->SS(1).ss1, L"SS1.ss1");
			Assert::AreEqual<unsigned>(8, m.gfx->ras->SS(1).ts1, L"SS1.ts1");
			Assert::AreEqual<uint32_t>(7, m.gfx->ras->Iref(), L"IREF");

			// The shift of an indirect stage is a division by 2^value, and RAS1_SS0 covers the stages
			// 0/1 while RAS1_SS1 covers the stages 2/3 (gfx-ras1.md 4.2).
			Assert::AreEqual(1.0f / 2.0f, m.gfx->ras->IndirectScale(0, false), L"stage 0 S");
			Assert::AreEqual(1.0f / 4.0f, m.gfx->ras->IndirectScale(0, true), L"stage 0 T");
			Assert::AreEqual(1.0f / 8.0f, m.gfx->ras->IndirectScale(1, false), L"stage 1 S");
			Assert::AreEqual(1.0f / 16.0f, m.gfx->ras->IndirectScale(1, true), L"stage 1 T");
			Assert::AreEqual(1.0f / 32.0f, m.gfx->ras->IndirectScale(2, false), L"stage 2 S");
			Assert::AreEqual(1.0f / 64.0f, m.gfx->ras->IndirectScale(2, true), L"stage 2 T");
			Assert::AreEqual(1.0f / 128.0f, m.gfx->ras->IndirectScale(3, false), L"stage 3 S");
			Assert::AreEqual(1.0f / 256.0f, m.gfx->ras->IndirectScale(3, true), L"stage 3 T");
		}

		TEST_METHOD(Ras_PerfRegisterIsDecoded)
		{
			GfxTestMachine& m = M();
			AssertDecoded(LoadAndLog(m, RAS1_PERF_ID, 0), "RAS1_PERF (0x24)");
		}
	};
}
