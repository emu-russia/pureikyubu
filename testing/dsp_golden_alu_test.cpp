// -----------------------------------------------------------------------------
// Differential test of the data-path instructions against the golden vectors in
// dsp_golden_alu_vectors.h.
//
// Every vector is one instruction word run from a fixed register state, with the
// result the hardware core produced for that word. Nothing here is derived from
// the emulator's own tables, so a rule that the specifications describe wrongly
// (the carry of `max` is one) still gets checked against what the core does.
// -----------------------------------------------------------------------------

#include "pch.h"
#include "dsp_test_common.h"
#include "dsp_golden_alu_vectors.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace DspUnitTest
{
namespace
{
	using DspGoldenAluTestDivergence = struct Divergence
	{
		uint16_t word;
		uint16_t set;
	};

	/// <summary>
	/// Vector positions where the emulator still disagrees with the core. The list is
	/// asserted to match the disagreements exactly, so it shrinks as the remaining bugs
	/// are fixed and any new disagreement fails the test.
	///
	/// Open items behind these vectors:
	///
	///   * `addp d,s` (0xf800-0xfbff) computes the product-and-source sum differently from
	///     the core. The core's result matches `d + (s &lt;&lt; 16) + (P &amp; ~0xffff)` for
	///     most operands but not all, so the exact rule has not been pinned down yet.
	///   * `sub d,p` (0x5e00/0x5f00) reports the carry of the ordinary C2 rule, while the
	///     core reports the borrow of `d - p` itself (C = 1 while d &lt; p).
	///   * the "other accumulator half" shift forms (0x3c80-0x3f80, 0x02ca-0x03cb) with a
	///     source of 0x7fff (low byte 0xff, i.e. -1) shift by the full 16-bit value
	///     instead of by the low byte.
	/// </summary>
	const Divergence kOpenDivergences[] =
	{
		{ 0x02CA, 8 },
		{ 0x02CB, 8 },
		{ 0x03CA, 8 },
		{ 0x03CB, 8 },
		{ 0x3C80, 8 },
		{ 0x3D80, 8 },
		{ 0x3E80, 8 },
		{ 0x3F80, 8 },
		{ 0x5E00, 0 },
		{ 0x5E00, 1 },
		{ 0x5E00, 2 },
		{ 0x5E00, 3 },
		{ 0x5E00, 4 },
		{ 0x5E00, 5 },
		{ 0x5E00, 6 },
		{ 0x5E00, 7 },
		{ 0x5E00, 8 },
		{ 0x5E00, 9 },
		{ 0xF800, 1 },
		{ 0xF800, 2 },
		{ 0xF800, 3 },
		{ 0xF800, 5 },
		{ 0xF800, 6 },
		{ 0xF800, 7 },
		{ 0xF800, 8 },
		{ 0xF800, 9 },
		{ 0xF900, 1 },
		{ 0xF900, 2 },
		{ 0xF900, 3 },
		{ 0xF900, 5 },
		{ 0xF900, 6 },
		{ 0xF900, 7 },
		{ 0xF900, 8 },
		{ 0xF900, 9 },
		{ 0xFA00, 1 },
		{ 0xFA00, 2 },
		{ 0xFA00, 3 },
		{ 0xFA00, 5 },
		{ 0xFA00, 6 },
		{ 0xFA00, 7 },
		{ 0xFA00, 8 },
		{ 0xFA00, 9 },
		{ 0xFB00, 1 },
		{ 0xFB00, 2 },
		{ 0xFB00, 3 },
		{ 0xFB00, 5 },
		{ 0xFB00, 6 },
		{ 0xFB00, 7 },
		{ 0xFB00, 8 },
		{ 0xFB00, 9 },
	};

	bool IsOpenDivergence(uint16_t word, uint16_t set)
	{
		for (const Divergence& d : kOpenDivergences)
		{
		if (d.word == word && d.set == set)
		{
			return true;
		}
		}
		return false;
	}

}

	TEST_CLASS(DspGoldenAluTest)
	{
		DspTestMachine m;

		static const uint64_t MASK40 = 0x0000'00FF'FFFF'FFFFULL;

		void ApplyOperandSet(const DspGolden::OperandSet& s)
		{
			m.Reset();

			m.core->regs.a.bits = s.a;
			m.core->regs.b.bits = s.b;

			m.core->regs.x.h = s.x1;
			m.core->regs.x.l = s.x0;
			m.core->regs.y.h = s.y1;
			m.core->regs.y.l = s.y0;

			// The product register pair holds the true product as
			// (prod.h << 32 | prod.m1 << 16 | prod.l) + (prod.m2 << 16), so a vector's
			// 40-bit product goes into the three low fields with a zero carry field.
			m.core->regs.prod.h = (uint8_t)((s.p >> 32) & 0xFF);
			m.core->regs.prod.m1 = (uint16_t)((s.p >> 16) & 0xFFFF);
			m.core->regs.prod.l = (uint16_t)(s.p & 0xFFFF);
			m.core->regs.prod.m2 = 0;

			// The vector table was captured with a clean status register.
			m.core->regs.psr.bits = 0;

			m.core->regs.pc = 0;
		}

		TEST_METHOD(DataPath_GoldenResultsAndFlags)
		{
			size_t checked = 0;
			size_t failed = 0;
			size_t openDivergences = 0;
			size_t fixedDivergences = 0;

			for (size_t i = 0; i < DspGolden::kAluVectorCount; i++)
			{
				const DspGolden::Vector& v = DspGolden::kAluVectors[i];
				const DspGolden::OperandSet& s = DspGolden::kAluOperandSets[v.set % DspGolden::kAluOperandSetCount];

				ApplyOperandSet(s);

				if (v.twoWord)
				{
					m.Run({ v.word, v.word2 }, 1);
				}
				else
				{
					m.Run({ v.word }, 1);
				}

				checked++;

				uint64_t gotA = m.core->regs.a.bits & MASK40;
				uint64_t gotB = m.core->regs.b.bits & MASK40;
				uint64_t gotP = (uint64_t)m.Product() & MASK40;
				uint16_t gotPsr = (uint16_t)(m.core->regs.psr.bits & 0x3F);

				bool matches = (gotA == v.a && gotB == v.b && gotP == v.p && gotPsr == (v.psr & 0x3F));
				bool expectedDivergence = IsOpenDivergence(v.word, v.set);

				if (matches)
				{
					if (expectedDivergence)
					{
						// The known issue is gone: drop the entry from kOpenDivergences.
						fixedDivergences++;
					}
					continue;
				}

				if (expectedDivergence)
				{
					openDivergences++;
					continue;
				}

				failed++;

				if (failed <= 4000)
				{
					wchar_t message[512];
					swprintf_s(message,
						L"GOLDENFAIL word %04X (set %u): A %010llX/%010llX  B %010llX/%010llX  P %010llX/%010llX  psr %02X/%02X",
						v.word, (unsigned)v.set,
						(unsigned long long)gotA, (unsigned long long)v.a,
						(unsigned long long)gotB, (unsigned long long)v.b,
						(unsigned long long)gotP, (unsigned long long)v.p,
						(unsigned)gotPsr, (unsigned)(v.psr & 0x3F));
					Logger::WriteMessage(message);
				}
			}

			Logger::WriteMessage(L"golden vectors checked");

			Assert::AreEqual((size_t)0, failed, L"new disagreements with the hardware core");
			Assert::AreEqual((size_t)0, fixedDivergences,
				L"an entry of kOpenDivergences now matches the core and must be removed from the list");
			Assert::AreEqual((size_t)(sizeof(kOpenDivergences) / sizeof(kOpenDivergences[0])), openDivergences,
				L"every open divergence must still be reproducible");
			Assert::IsTrue(checked > 1000, L"the vector table must actually be populated");
		}
	};
}
