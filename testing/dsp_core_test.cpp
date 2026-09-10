// DSP core unit tests: every non-parallel computing instruction of the core.
//
// The class is deliberately named DspUnitTest::DspUnitTest - that is the class the emulator
// headers declare as a friend of DspCore and Dsp16.
//
// Reference: dsp-isa.md sections 4.5-4.11 (the instruction tables and the flag rules of
// section 4.13) and dsp.md sections 2.3 and 2.10 for the accumulator and multiplier model.

#include "pch.h"
#include "dsp_test_common.h"

namespace DspUnitTest
{
	TEST_CLASS(DspUnitTest)
	{
		DspTestMachine& m = Machine();

		static const int64_t H = 0x0000'0001'0000LL;		// 1.0 in the accumulator's high half

		// Helper: run a single one-word instruction written at address 0.
		void RunOne(uint16_t w)
		{
			m.core->regs.pc = 0;
			PokeIMem(m.core, 0, w);
			PokeIMem(m.core, 1, 0);
			m.Step();
		}

	public:

		TEST_METHOD_INITIALIZE(Setup)
		{
			m.Reset();
		}

		// ===============================================================
		// 4.5  Immediate ALU / compare
		// ===============================================================

		TEST_METHOD(Adsi_AddsSignExtendedShortImmediate)
		{
			// dsp-isa.md section 4.5: adsi adds the sign-extended short immediate.
			m.SetA(10 * H);
			RunOne(Enc::Adsi(0, 5));
			Assert::AreEqual(15 * H, m.A());

			m.SetA(10 * H);
			RunOne(Enc::Adsi(0, (int8_t)-3));
			Assert::AreEqual(7 * H, m.A());
		}

		TEST_METHOD(Adsi_ReachesB)
		{
			m.SetB(2 * H);
			RunOne(Enc::Adsi(1, 3));
			Assert::AreEqual(5 * H, m.B());
		}

		TEST_METHOD(Adli_AddsLongImmediate)
		{
			m.SetA(0x0000'0001'0000LL);
			m.core->regs.pc = 0;
			m.Run({ Enc::Adli(0), 0x0002 }, 1);
			Assert::AreEqual(0x0000'0003'0000LL, m.A());
		}

		TEST_METHOD(Cmpsi_DoesNotWriteDestination)
		{
			m.SetA(7 * H);
			RunOne(Enc::Cmpsi(0, 7));
			Assert::AreEqual(7 * H, m.A(), L"cmpsi must not modify the accumulator");
			Assert::AreEqual(1, (int)m.core->regs.psr.z);
			Assert::AreEqual(0, (int)m.core->regs.psr.n);
			// C2 is the carry out of Ds + ~S, which is 1 for equal operands.
			Assert::AreEqual(1, (int)m.core->regs.psr.c);
		}

		TEST_METHOD(Cmpli_DoesNotWriteDestination)
		{
			m.SetA(3 * H);
			m.core->regs.pc = 0;
			m.Run({ Enc::Cmpli(0), 0x0009 }, 1);
			Assert::AreEqual(3 * H, m.A());
			Assert::AreEqual(0, (int)m.core->regs.psr.z);
			Assert::AreEqual(1, (int)m.core->regs.psr.n, L"3 - 9 < 0");
			// C2 = carry of (Ds + ~S): 3 + ~9 does not carry out of bit 39.
			Assert::AreEqual(0, (int)m.core->regs.psr.c);
		}

		TEST_METHOD(Lsfi_LogicalShiftImmediate)
		{
			// lsfi shifts the 40-bit accumulator; a positive immediate shifts left (LSFI), and
			// the sign of the immediate selects the direction (dsp-isa.md 4.5).
			m.SetA(1);
			RunOne(Enc::Lsfi(0, 4));
			Assert::AreEqual((int64_t)16, m.A());

			m.SetA(16);
			RunOne(Enc::Lsfi(0, -4));
			Assert::AreEqual((int64_t)1, m.A());
		}

		TEST_METHOD(Asfi_ArithmeticShiftImmediate)
		{
			m.SetA(-16);
			RunOne(Enc::Asfi(0, -2));
			Assert::AreEqual((int64_t)-4, m.A(), L"arithmetic right shift keeps the sign");

			m.SetA(-4);
			RunOne(Enc::Asfi(0, 2));
			Assert::AreEqual((int64_t)-16, m.A(), L"arithmetic left shift");
		}

		TEST_METHOD(Xorli_Anli_Orli_OperateOnTheA1B1Half)
		{
			m.SetA(0x0000'F0F0'1234LL);
			m.core->regs.pc = 0;
			m.Run({ Enc::Xorli(0), 0xFFFF }, 1);
			Assert::AreEqual((uint16_t)0x0F0F, (uint16_t)m.core->regs.a.m);
			Assert::AreEqual((uint16_t)0x1234, (uint16_t)m.core->regs.a.l, L"a0 must be untouched");

			m.SetA(0x0000'F0F0'0000LL);
			m.core->regs.pc = 0;
			m.Run({ Enc::Anli(0), 0x0FF0 }, 1);
			Assert::AreEqual((uint16_t)0x00F0, (uint16_t)m.core->regs.a.m);

			m.SetA(0x0000'0000'0000LL);
			m.core->regs.pc = 0;
			m.Run({ Enc::Orli(1), 0x8000 }, 1);
			Assert::AreEqual((uint16_t)0x8000, (uint16_t)m.core->regs.b.m);
		}

		// ===============================================================
		// 4.6  Step operations
		// ===============================================================

		TEST_METHOD(Norm_NopWhenNormalized)
		{
			// A normalised value has nothing to do: neither the accumulator nor rn change.
			m.SetA(0x0000'4000'0000LL);		// 0.5: bits 31:30 = 01
			m.core->regs.r[0] = 0x0100;
			m.core->regs.l[0] = 0xFFFF;
			m.core->regs.psr.e = 0;
			m.core->regs.psr.u = 1 - 1;
			RunOne(Enc::Norm(0, REG_R0));
			Assert::AreEqual(0x0000'4000'0000LL, m.A());
			Assert::AreEqual((uint16_t)0x0100, m.core->regs.r[0], L"a NOP norm must not touch rn");
		}

		TEST_METHOD(Norm_ShiftsLeftWhenUnnormalized)
		{
			// U = 1 and Z = 0 and E = 0: the value is unnormalised, so it is shifted left by 1
			// and rn is stepped in the opposite direction (dsp-isa.md section 4.6).
			m.SetA(0x0000'0001'0000LL);		// bits 31:30 = 00 -> U = 1, Z = 0, E = 0
			m.core->regs.r[0] = 0x0100;
			m.core->regs.l[0] = 0xFFFF;
			// norm decides on the *current* PSR flags: the left shift needs E=0, U=1 and Z=0.
			m.core->regs.psr.e = 0;
			m.core->regs.psr.u = 1;
			m.core->regs.psr.z = 0;
			RunOne(Enc::Norm(0, REG_R0));
			Assert::AreEqual(0x0000'0002'0000LL, m.A());
			Assert::AreEqual((uint16_t)0x00FF, m.core->regs.r[0], L"the left shift decrements rn");
		}

		TEST_METHOD(Norm_ShiftsRightWhenExtended)
		{
			// E = 1: the accumulator is shifted right by 1 and rn is incremented.
			m.SetA(0x0001'0000'0000LL);
			m.core->regs.r[0] = 0x0100;
			m.core->regs.l[0] = 0xFFFF;
			m.core->regs.psr.e = 1;
			m.core->regs.psr.u = 0;
			RunOne(Enc::Norm(0, REG_R0));
			Assert::AreEqual(0x0000'8000'0000LL, m.A());
			Assert::AreEqual((uint16_t)0x0101, m.core->regs.r[0], L"the right shift increments rn");
		}

		TEST_METHOD(Norm_UpdatesFlags)
		{
			// dsp-isa.md 4.13: norm = 0 0 Z1 N1 E1 U1.
			m.SetDefaultFlags();
			m.SetA(0);
			RunOne(Enc::Norm(0, REG_R0));
			m.AssertFlags(0, 0, 1, 0, 0, 1, L"norm of zero clears C/V and sets Z, U");
		}

		TEST_METHOD(Div_DividesByAPowerOfTwo)
		{
			// A full divide is `rep 16` + `div` (dsp-isa.md 7.2). 0.25 / 0.5 = 0.5.
			// The divisor lives in the high half of the source operand.
			//
			// The quotient is shifted into a0/b0, the remainder stays in a1/b1
			// (dsp-isa.md sections 4.6 and 7.2).
			m.SetA(0x0000'2000'0000LL);		// 0.25
			m.core->regs.x.h = 0x4000;		// 0.5
			m.Run({ Enc::Rep(16), Enc::Div(0, R4XY_X1), Enc::Nop() }, 1 + 16);
			Assert::AreEqual(0, DspTestHaltCount());
			Assert::AreEqual(0, (int)m.core->regs.psr.v, L"div always clears V");
			// Bit 31 of the result is the sign of the remainder, bits 30-15 hold the
			// remainder, bits 14-0 the quotient.
			int64_t quotient = m.A() & 0x7FFF;
			Assert::AreEqual((int64_t)0x4000, quotient, L"0.25 / 0.5 = 0.5");
		}

		TEST_METHOD(Div_DoesNotHaltAndNeverOverflows)
		{
			// A full 16-step divide must run to completion for every dividend/divisor pair
			// (a correct quotient is only guaranteed for |D| < |S|, but the step
			// itself must never trap).
			for (uint16_t divisor : { (uint16_t)0x4000, (uint16_t)0xC000, (uint16_t)0x0000 })
			{
				m.Reset();
				m.SetA(0x0000'1000'0000LL);
				m.core->regs.x.h = divisor;
				m.Run({ Enc::Rep(16), Enc::Div(0, R4XY_X1), Enc::Nop() }, 1 + 16);
				Assert::AreEqual(0, DspTestHaltCount(), L"div must be implemented");
				Assert::AreEqual(0, (int)m.core->regs.psr.v, L"div always clears V");
			}
		}

		TEST_METHOD(Norm_Div_Max_GoldenVectorsFromTheCore)
		{
			// Recorded behaviour of the three step operations for a set of operand combinations
			// ({ accumulator, source, expected result, C V Z N E U }). The `max` rows leave the
			// accumulator untouched, and `norm` also reports the address register it stepped.

			struct NormCase { int64_t a; int e, u, z; int64_t expected; int c, v, zz, n, ee, uu; uint16_t rn; };
			static const NormCase normCases[] =
			{
				// A = 0x5A12345678, norm a,r3 with r3 = 0x1000
				{ 0x5A'1234'5678LL, 1, 0, 0, 0x2D'091A'2B3CLL, 0, 0, 0, 0, 1, 1, 0x1001 },	// E set: ASR 1
				// ~E & U & ~Z: LSL 1. The result has bit 39 set, i.e. it is negative as a
				// 40-bit value.
				{ 0x5A'1234'5678LL, 0, 1, 0, DspCore::SignExtend40((int64_t)0xB4'2468'ACF0LL), 0, 0, 0, 1, 1, 1, 0x0FFF },
				{ 0x5A'1234'5678LL, 0, 0, 0, 0x5A'1234'5678LL, 0, 0, 0, 0, 1, 1, 0x1000 },	// NOP
			};

			for (const NormCase& c : normCases)
			{
				m.Reset();
				m.SetA(c.a);
				m.core->regs.r[3] = 0x1000;
				m.core->regs.l[3] = 0xFFFF;
				m.core->regs.psr.e = c.e;
				m.core->regs.psr.u = c.u;
				m.core->regs.psr.z = c.z;

				RunOne(Enc::Norm(0, REG_R3));

				Assert::AreEqual(c.expected, m.A(), L"norm result");
				m.AssertFlags(c.c, c.v, c.zz, c.n, c.ee, c.uu, L"norm flags");
				Assert::AreEqual(c.rn, m.core->regs.r[3], L"norm must step rn");
			}

			struct StepCase { int64_t a; uint16_t src; int64_t expected; int c, v, z, n, e, u; };
			static const StepCase divCases[] =
			{
				{ 0x00'0000'0000LL, 0x0000, 0x00'0000'0001LL, 1, 0, 0, 0, 0, 1 },
				{ 0x00'0000'0000LL, 0x0001, DspCore::SignExtend40((int64_t)0xFF'FFFE'0000LL), 0, 0, 0, 1, 0, 1 },
				{ 0x00'0000'0001LL, 0x0001, DspCore::SignExtend40((int64_t)0xFF'FFFE'0002LL), 0, 0, 0, 1, 0, 1 },
				{ 0x00'0100'0000LL, 0x0002, 0x00'01FC'0001LL, 1, 0, 0, 0, 0, 1 },
				{ 0x5A'1234'5678LL, 0x2222, 0x59'E0'24AC'F1LL, 1, 0, 0, 0, 1, 1 },
				{ 0x5A'1234'5678LL, 0xE222, 0x59'E8'ACAC'F0LL, 1, 0, 0, 0, 1, 1 },
				{ 0x7F'FFFF'FFFFLL, 0x0001, 0x7F'FFFD'FFFFLL, 1, 0, 0, 0, 1, 1 },
				{ 0x80'0000'0000LL, 0x8000, DspCore::SignExtend40((int64_t)0x80'0000'0001LL), 0, 0, 0, 1, 1, 1 },
				{ 0xA5'EDCB'A987LL, 0x2222, DspCore::SignExtend40((int64_t)0xA6'1F'DB53'0ELL), 0, 0, 0, 1, 1, 1 },
				{ 0xFF'FFFF'FFFFLL, 0xFFFF, 0x00'0001'FFFELL, 1, 0, 0, 0, 0, 1 },
				{ 0x00'FFFF'0000LL, 0x00FF, 0x00'FE00'0001LL, 1, 0, 0, 0, 1, 1 },
				{ 0x0000'00FF'FFFF'0000LL, 0xFFFF, 0x00'0000'0000LL, 1, 0, 1, 0, 0, 1 },
			};
			static const StepCase maxCases[] =
			{
				{ 0x00'0000'0000LL, 0x0000, 0x00'0000'0000LL, 1, 0, 1, 0, 0, 1 },
				{ 0x00'0000'0014LL, 0x0005, 0x00'0000'0014LL, 0, 0, 0, 1, 0, 1 },
				{ 0x00'0005'0000LL, 0x0005, 0x00'0005'0000LL, 1, 0, 1, 0, 0, 1 },
				{ 0x5A'1234'5678LL, 0x2222, 0x5A'1234'5678LL, 1, 0, 0, 0, 1, 1 },
				{ 0x7F'FFFF'FFFFLL, 0x8000, 0x7F'FFFF'FFFFLL, 1, 0, 0, 0, 1, 0 },
				{ 0x00'0080'0000LL, 0x0000, 0x00'0080'0000LL, 1, 0, 0, 0, 0, 1 },
				{ 0xFF'FFFF'FFFFLL, 0x0000, DspCore::SignExtend40((int64_t)0xFF'FFFF'FFFFLL), 1, 0, 0, 0, 0, 1 },
				{ 0xFF'0000'0000LL, 0x0000, DspCore::SignExtend40((int64_t)0xFF'0000'0000LL), 1, 0, 0, 0, 1, 1 },
				{ 0x00'FFFF'FFFFLL, 0x0000, 0x00'FFFF'FFFFLL, 1, 0, 0, 0, 1, 1 },
				{ 0x00'2222'0000LL, 0x2222, 0x00'2222'0000LL, 1, 0, 1, 0, 0, 1 },
			};

			for (const StepCase& c : divCases)
			{
				m.Reset();
				m.SetA(c.a);
				m.core->regs.x.h = c.src;
				RunOne(Enc::Div(0, R4XY_X1));
				Assert::AreEqual(c.expected, m.A(), L"div result");
				m.AssertFlags(c.c, c.v, c.z, c.n, c.e, c.u, L"div flags");
			}

			for (const StepCase& c : maxCases)
			{
				m.Reset();
				m.SetA(c.a);
				m.core->regs.x.h = c.src;
				RunOne(Enc::Max(0, R4XY_X1));
				Assert::AreEqual(c.expected, m.A(), L"max must not store its result");
				m.AssertFlags(c.c, c.v, c.z, c.n, c.e, c.u, L"max flags");
			}
		}

		TEST_METHOD(Addc_Subc_UseTheCarryFlag)
		{
			// The source of addc/subc is the whole 32-bit x or y pair (dsp-isa.md section 4.6).
			m.SetA(1);
			m.SetX(2 << 16);
			m.core->regs.psr.c = 0;
			RunOne(Enc::Addc(0, 0));
			Assert::AreEqual((int64_t)((2 << 16) + 1), m.A());

			m.SetA(1);
			m.core->regs.psr.c = 1;
			RunOne(Enc::Addc(0, 0));
			Assert::AreEqual((int64_t)((2 << 16) + 2), m.A());

			m.SetA(0x30000);
			m.SetY(1 << 16);
			m.core->regs.psr.c = 1;
			RunOne(Enc::Subc(0, 1));
			Assert::AreEqual((int64_t)0x20000, m.A(), L"subc computes d + ~s + C");
		}

		TEST_METHOD(Negc_NegatesWithCarry)
		{
			m.SetA(5 * H);
			m.core->regs.psr.c = 1;
			RunOne(Enc::Negc(0));
			Assert::AreEqual(-5 * H, m.A());
		}

		TEST_METHOD(Max_ComparesMagnitudesWithoutStoring)
		{
			// max compares magnitudes: it subtracts |S| from |D|, stores nothing and keeps
			// only the flags (dsp-isa.md section 4.6 lists it among the step operations).
			m.SetA(7 * H);
			m.core->regs.x.h = 0x0003;
			RunOne(Enc::Max(0, R4XY_X1));
			Assert::AreEqual(7 * H, m.A(), L"max does not store its result");
			Assert::AreEqual(0, (int)m.core->regs.psr.v, L"max always clears V");
			Assert::AreEqual(0, (int)m.core->regs.psr.z, L"|D| != |S|");

			m.SetA(-7 * H);
			m.core->regs.x.h = (uint16_t)-7;
			RunOne(Enc::Max(0, R4XY_X1));
			Assert::AreEqual(1, (int)m.core->regs.psr.z, L"|-7| == |-7|");
			Assert::AreEqual(1, (int)m.core->regs.psr.c,
				L"C is the carry out of |D| - |S|: it is set when the magnitudes are equal");

			// C is set exactly while |D| >= |S| (no borrow out of the magnitude subtraction).
			m.SetA(0x0006'0000'0000LL);			// |D| = 0x600000000
			m.core->regs.x.h = 0x0005;			// |S| = 5 * 2^16
			RunOne(Enc::Max(0, R4XY_X1));
			Assert::AreEqual(1, (int)m.core->regs.psr.c, L"|D| > |S| clears the borrow");

			m.SetA(0x0000'0001'0000LL);			// |D| = 2^16
			m.core->regs.x.h = 0x0005;			// |S| = 5 * 2^16
			RunOne(Enc::Max(0, R4XY_X1));
			Assert::AreEqual(0, (int)m.core->regs.psr.c, L"|D| < |S| sets the borrow");

			// The magnitude comparison ignores the operand signs.
			m.SetA(-0x0006'0000'0000LL);
			m.core->regs.x.h = (uint16_t)-5;
			RunOne(Enc::Max(0, R4XY_X1));
			Assert::AreEqual(1, (int)m.core->regs.psr.c, L"|D| > |S| for negative operands too");
		}

		TEST_METHOD(Lsf_Asf_ShiftByTheSourceValue)
		{
			// `lsf d,s`/`asf d,s` shift by the signed value of the source, not by its sign
			// alone: a positive count shifts left, a negative one shifts right by its
			// magnitude, and the count is the *low byte* of the source (0x0100 does not
			// shift at all, 0xffff shifts right by one). The behaviour was read off the
			// hardware core; dsp-isa.md 4.6, which says "shift by the sign of S", agrees for
			// the +-1 counts the unit codes use but not for larger ones.
			m.SetA(16);
			m.core->regs.x.h = 0x0004;		// +4
			RunOne(Enc::LsfXY(0, 0));
			Assert::AreEqual((int64_t)256, m.A(), L"lsf a,x1 by +4 shifts left");

			m.SetA(16);
			m.core->regs.x.h = (uint16_t)0xFFFC;	// -4
			RunOne(Enc::LsfXY(0, 0));
			Assert::AreEqual((int64_t)1, m.A(), L"lsf a,x1 by -4 shifts right");

			m.SetA(16);
			m.core->regs.y.h = 0x0004;
			RunOne(Enc::LsfXY(0, 1));
			Assert::AreEqual((int64_t)256, m.A(), L"lsf a,y1 by +4 shifts left");

			// The count is the low byte, so a source of 0x0100 is a count of zero.
			m.SetA(16);
			m.core->regs.x.h = 0x0100;
			RunOne(Enc::LsfXY(0, 0));
			Assert::AreEqual((int64_t)16, m.A(), L"a source of 0x0100 does not shift");

			// 0xffff is a count of -1.
			m.SetA(16);
			m.core->regs.x.h = 0xFFFF;
			RunOne(Enc::LsfXY(0, 0));
			Assert::AreEqual((int64_t)8, m.A(), L"a source of 0xffff shifts right by one");

			// lsf shifts zeros in from the top, asf replicates the sign.
			m.SetA(-16);
			m.core->regs.x.h = (uint16_t)0xFFFC;
			RunOne(Enc::LsfXY(0, 0));
			Assert::AreEqual((int64_t)0x0FFF'FFFF'FFLL, m.A(), L"lsf fills with zeros");

			m.SetA(-16);
			m.core->regs.x.h = (uint16_t)0xFFFC;
			RunOne(Enc::AsfXY(0, 0));
			Assert::AreEqual((int64_t)-1, m.A(), L"asf fills with the sign");

			// The accumulator is 40 bits wide, so the flags describe the shifted value and a
			// shift that pushes everything out of bit 39 reads back as zero.
			m.SetA(0x80'0000'0000LL);
			m.core->regs.x.h = 0x0004;
			RunOne(Enc::LsfXY(0, 0));
			Assert::AreEqual((int64_t)0, m.A(), L"the shifted-out value is zero");
			Assert::AreEqual(1, (int)m.core->regs.psr.z, L"Z1 describes the 40-bit result");
		}

		TEST_METHOD(Lsf_Asf_NegatedSourceFormsAreDecoded)
		{
			// The negated forms shift by the *negative* of the source value, so they move
			// the accumulator the other way: lsf a,x1 shifts left for a positive x1 while
			// lsf a,-x1 shifts right.
			m.SetA(16);
			m.core->regs.x.h = 0x0004;		// -x1 = -4 -> shift right by 4
			RunOne(Enc::LsfNeg(0, 0));
			Assert::AreEqual((int64_t)1, m.A(), L"lsf a,-x1 shifts right for a positive x1");

			m.SetA(16);
			m.core->regs.x.h = (uint16_t)0xFFFC;	// -x1 = +4 -> shift left by 4
			RunOne(Enc::LsfNeg(0, 0));
			Assert::AreEqual((int64_t)256, m.A(), L"lsf a,-x1 shifts left for a negative x1");

			m.SetA(-16);
			m.core->regs.y.h = 0x0004;
			RunOne(Enc::AsfNeg(0, 1));
			Assert::AreEqual((int64_t)-1, m.A(), L"asf a,-y1 shifts right arithmetically");

			// lsf d,-(other accumulator half): 0x02CA / 0x02CB
			m.SetA(16);
			m.core->regs.b.m = 0x0004;		// -b1 = -4 -> shift right by 4
			RunOne(Enc::LsfNegOther(0));
			Assert::AreEqual((int64_t)1, m.A(), L"lsf a,-b1 shifts right");

			// ... and the non-negated "other half" form shifts the other way.
			m.SetA(16);
			m.core->regs.b.m = 0x0004;
			RunOne(Enc::LsfOther2(0));
			Assert::AreEqual((int64_t)256, m.A(), L"lsf a,b1 shifts left");
		}

		// ===============================================================
		// 4.7  Accumulator arithmetic
		// ===============================================================

		TEST_METHOD(Add_Sub_AddTheHighHalfSource)
		{
			m.SetA(2 * H);
			m.core->regs.x.h = 3;
			RunOne(Enc::Add(0, R8P_X1));
			Assert::AreEqual(5 * H, m.A());

			m.SetA(5 * H);
			m.core->regs.y.h = 3;
			RunOne(Enc::Sub(0, R8P_Y1));
			Assert::AreEqual(2 * H, m.A());
		}

		TEST_METHOD(Add_CanUseTheWhole32BitOperand)
		{
			m.SetA(0);
			m.core->regs.x.bits = 0x0003'0000;
			RunOne(Enc::Add(0, R8P_X));
			Assert::AreEqual(3 * H, m.A());
		}

		TEST_METHOD(Add_AddsTheOtherAccumulator)
		{
			m.SetA(2 * H);
			m.SetB(5 * H);
			RunOne(Enc::Add(0, R8P_AB));		// add a,b
			Assert::AreEqual(7 * H, m.A());
		}

		TEST_METHOD(Amv_IsMoveThroughTheAlu)
		{
			// amv moves the source operand through the ALU into the destination: the old value
			// of the destination is NOT an input (dsp-isa.md section 4.7 lists it as an
			// arithmetic move whose C and V are forced to zero, which no real add could do).
			m.SetA(0x00AB'CDEF'1234LL);
			m.core->regs.y.h = 0x4321;
			m.core->regs.pc = 0;
			PokeIMem(m.core, 0, Enc::Amv(0, R8P_Y1));
			m.Step();
			Assert::AreEqual(0x0000'4321'0000LL, m.A(), L"amv replaces the accumulator");
			Assert::AreEqual(0, (int)m.core->regs.psr.c, L"amv never sets the carry");
			Assert::AreEqual(0, (int)m.core->regs.psr.v, L"amv never sets the overflow");
		}

		TEST_METHOD(Cmp_CompareAccumulatorWithX1)
		{
			m.SetA(5 * H);
			m.core->regs.x.h = 7;
			RunOne(Enc::CmpDX(0, 0));
			Assert::AreEqual(5 * H, m.A(), L"cmp does not write the accumulator");
			Assert::AreEqual(1, (int)m.core->regs.psr.n, L"5 - 7 < 0");
			Assert::AreEqual(0, (int)m.core->regs.psr.z);

			m.SetA(7 * H);
			m.core->regs.x.h = 7;
			RunOne(Enc::CmpDX(0, 0));
			Assert::AreEqual(1, (int)m.core->regs.psr.z);
		}

		TEST_METHOD(Cmp_ComparesTheTwoAccumulators)
		{
			m.SetA(4 * H);
			m.SetB(9 * H);
			RunOne(Enc::CmpAB());
			Assert::AreEqual(1, (int)m.core->regs.psr.n);
			Assert::AreEqual(0, (int)m.core->regs.psr.z);
		}

		TEST_METHOD(Inc_Dec_AccumulatorAndHalves)
		{
			m.SetA(5);
			RunOne(Enc::Inc(2));					// inc a
			Assert::AreEqual((int64_t)6, m.A());

			m.SetA(5);
			RunOne(Enc::Inc(0));					// inc a1 : increments bit 16
			Assert::AreEqual((int64_t)5 + H, m.A());

			m.SetA(H);
			RunOne(Enc::Dec(2));					// dec a
			Assert::AreEqual(H - 1, m.A());

			m.SetA(H);
			RunOne(Enc::Dec(0));					// dec a1
			Assert::AreEqual((int64_t)0, m.A());
		}

		TEST_METHOD(Dec_CarryIsTheComplementOfTheBorrow)
		{
			// Decrementing subtracts one, so its carry is the carry of that subtraction: the
			// borrow is taken only when the operand is zero, so C is set for every non-zero
			// operand, negative ones included.
			m.SetA(5);
			RunOne(Enc::Dec(2));				// dec a
			Assert::AreEqual((int64_t)4, m.A());
			Assert::AreEqual(1, (int)m.core->regs.psr.c, L"5 - 1 does not borrow");

			m.SetA(-5);
			RunOne(Enc::Dec(2));
			Assert::AreEqual((int64_t)-6, m.A());
			Assert::AreEqual(1, (int)m.core->regs.psr.c, L"-5 - 1 does not borrow either");

			m.SetA(0);
			RunOne(Enc::Dec(2));
			Assert::AreEqual((int64_t)-1, m.A());
			Assert::AreEqual(0, (int)m.core->regs.psr.c, L"0 - 1 borrows");

			// Decrementing the most negative value wraps into the positive range.
			m.SetA(-0x0080'0000'0000LL);
			RunOne(Enc::Dec(2));
			Assert::AreEqual(1, (int)m.core->regs.psr.v, L"V8: decrement overflow");
		}

		TEST_METHOD(Abs_Negate)
		{
			m.SetA(-7 * H);
			RunOne(Enc::Abs(0));
			Assert::AreEqual(7 * H, m.A());

			m.SetA(7 * H);
			RunOne(Enc::Abs(0));
			Assert::AreEqual(7 * H, m.A());

			m.SetA(7 * H);
			RunOne(Enc::Neg(0));
			Assert::AreEqual(-7 * H, m.A());
		}

		TEST_METHOD(Abs_OfMostNegativeSetsOverflow)
		{
			// V5: abs overflow - the result is still negative.
			const int64_t mostNegative = -0x0080'0000'0000LL;
			m.SetA(mostNegative);
			RunOne(Enc::Abs(0));
			Assert::AreEqual(1, (int)m.core->regs.psr.v);
		}

		TEST_METHOD(Neg_OfMostNegativeSetsOverflow)
		{
			const int64_t mostNegative = -0x0080'0000'0000LL;
			m.SetA(mostNegative);
			RunOne(Enc::Neg(0));
			Assert::AreEqual(1, (int)m.core->regs.psr.v, L"V3: old and new value both negative");
		}

		TEST_METHOD(NegByProduct_SubtractsTheProduct)
		{
			m.core->regs.psr.im = 1;				// integer mode -> raw 2*3 product
			m.SetA(5 * H);
			m.core->regs.x.bits = 0x0000'0002;		// x0 = 2
			m.core->regs.y.bits = 0x0000'0003;		// y0 = 3 -> product 6
			m.core->regs.pc = 0;
			PokeIMem(m.core, 0, Enc::Mpy(4));		// mpy x0,y0
			PokeIMem(m.core, 1, Enc::NegP(0));		// neg a, p
			m.Steps(2);
			Assert::AreEqual((int64_t)-6, m.A(), L"neg a,p negates the product");
		}

		TEST_METHOD(Clr_ClearsAccumulator)
		{
			m.SetA(0x1234'5678'9ALL);
			RunOne(Enc::ClrAcc(0));
			Assert::AreEqual((int64_t)0, m.A());
		}

		TEST_METHOD(Rnd_RoundToEven)
		{
			// The rounding step adds 0 or 1 at bit 16 depending on bit 15 (round half to even)
			// and drops the low word.
			m.SetA(0x0000'0001'7FFFLL);
			RunOne(Enc::Rnd(0));
			Assert::AreEqual(0x0000'0001'0000LL, m.A(), L"0x7FFF rounds down");

			m.SetA(0x0000'0001'8001LL);
			RunOne(Enc::Rnd(0));
			Assert::AreEqual(0x0000'0002'0000LL, m.A(), L"0x8001 rounds up");

			m.SetA(0x0000'0001'8000LL);
			RunOne(Enc::Rnd(0));
			Assert::AreEqual(0x0000'0002'0000LL, m.A(), L"0x8000 with an odd lsb rounds up");
		}

		TEST_METHOD(Tst_AccumulatorX1AndProduct)
		{
			m.SetA(0);
			RunOne(Enc::TstAB(0));
			Assert::AreEqual(1, (int)m.core->regs.psr.z);

			m.SetA(0x0000'0001'0000LL);
			RunOne(Enc::TstAB(0));
			Assert::AreEqual(0, (int)m.core->regs.psr.z);
			Assert::AreEqual(0, (int)m.core->regs.psr.n);

			m.core->regs.x.h = 0x8000;
			RunOne(Enc::TstXY(0));			// tst x1
			Assert::AreEqual(1, (int)m.core->regs.psr.n);

			// tst p of the documented `clr p` pattern (a zero product) must set Z.
			RunOne(Enc::ClrP());
			RunOne(Enc::TstP());
			Assert::AreEqual(1, (int)m.core->regs.psr.z, L"the clr p pattern is a zero product");
		}

		TEST_METHOD(Lsl16_Lsr16_Asr16)
		{
			m.SetA(0x0000'0001'0000LL);
			RunOne(Enc::Lsl16(0));
			Assert::AreEqual(0x0000'0000'0000LL | (1LL << 32), m.A());

			m.SetA(0x0000'0001'0000LL);
			RunOne(Enc::Lsr16(0));
			Assert::AreEqual((int64_t)1, m.A());

			m.SetA(-0x0001'0000'0000LL);
			RunOne(Enc::Asr16(0));
			Assert::AreEqual(-0x0000'0001'0000LL, m.A(), L"asr16 keeps the sign");
		}

		TEST_METHOD(Addp_AddsX1AndTheProduct)
		{
			// Manual: addp adds x1/y1 and the product into the accumulator.
			m.core->regs.x.bits = 0x0002'0003;
			m.core->regs.pc = 0;
			PokeIMem(m.core, 0, Enc::Mpy(4));				// mpy x0,y0 -> 3*? ...
			m.core->regs.y.bits = 0x0000'0003;
			PokeIMem(m.core, 0, Enc::Mpy(4));
			PokeIMem(m.core, 1, Enc::Addp(0, 0));			// addp a, x1
			m.Steps(2);

			// x1 = 2, y0 = 3 -> product = 6 (integer mode scaling aside, both are exact here)
			int64_t expected = m.Product() + (2 * H);
			Assert::AreEqual(expected, m.A());
		}

		// ===============================================================
		// 4.8  Logic
		// ===============================================================

		TEST_METHOD(LogicOps_OnAccumulatorHalves)
		{
			m.SetA(0x0000'FF00'0000LL);
			RunOne(Enc::Not(0));
			Assert::AreEqual((uint16_t)0x00FF, (uint16_t)m.core->regs.a.m);

			m.SetA(0x0000'FF00'0000LL);
			m.SetB(0x0000'0FF0'0000LL);
			RunOne(Enc::XorOther(0));		// xor a1, b1
			Assert::AreEqual((uint16_t)0xF0F0, (uint16_t)m.core->regs.a.m);

			m.SetA(0x0000'FF00'0000LL);
			m.core->regs.x.h = 0x0FF0;
			RunOne(Enc::AndXY(0, 0));		// and a1, x1
			Assert::AreEqual((uint16_t)0x0F00, (uint16_t)m.core->regs.a.m);

			m.SetA(0x0000'F000'0000LL);
			m.core->regs.y.h = 0x000F;
			RunOne(Enc::OrXY(0, 1));		// or a1, y1
			Assert::AreEqual((uint16_t)0xF00F, (uint16_t)m.core->regs.a.m);
		}

		// ===============================================================
		// 4.11  Multiply and multiply-accumulate
		// ===============================================================

		TEST_METHOD(Mpy_Signed16x16)
		{
			// dsp.md section 2.3: integer mode multiplies S1*S2, fraction mode S1*2*S2.
			m.core->regs.psr.im = 1;			// integer mode
			m.core->regs.x.bits = 0x0002'0003;	// x1 = 2, x0 = 3
			RunOne(Enc::Mpy(2));				// mpy x1,x0
			Assert::AreEqual((int64_t)(2 * 3), m.Product());

			// A negative multiplier must produce a negative product.
			m.core->regs.x.bits = 0x0002'FFFD;	// x1 = 2, x0 = -3
			RunOne(Enc::Mpy(2));
			Assert::AreEqual((int64_t)(2 * -3), m.Product());

			// A negative multiplicand too.
			m.core->regs.x.bits = 0xFFFE'0003;	// x1 = -2, x0 = 3
			RunOne(Enc::Mpy(2));
			Assert::AreEqual((int64_t)(-2 * 3), m.Product());

			m.core->regs.x.bits = 0x8000'8000;	// x1 = -32768, x0 = -32768
			RunOne(Enc::Mpy(2));
			Assert::AreEqual((int64_t)32768 * 32768, m.Product());
		}

		TEST_METHOD(Mpy_FractionModeDoublesTheMultiplier)
		{
			m.core->regs.psr.im = 0;			// fraction mode
			m.core->regs.x.bits = 0x4000'4000;	// x1 = 0.5, x0 = 0.5 (Q1.15)
			RunOne(Enc::Mpy(2));
			// P = x1 * 2*x0 = 0x4000 * 0x8000 = 2^29, which is 0.5*0.5 = 0.25 in Q1.31.
			Assert::AreEqual((int64_t)0x2000'0000, m.Product());
		}

		TEST_METHOD(Mpy_MixedSignedUnsignedForms)
		{
			// The mixed forms -- x1*y0 -> sign * unsign, x0*y1 -> unsign * sign and
			// x0*y0 -> unsign * unsign -- are selected by the DP mode bit (dsp.md sections 2.3
			// and 2.10); while DP is clear every half is signed.
			m.core->regs.psr.im = 1;
			m.core->regs.psr.dp = 1;

			// mpy x1,y0 with y0 = 0xFFFF: in DP mode the multiplier is UNSIGNED -> 65535.
			m.core->regs.x.h = 1;
			m.core->regs.y.l = 0xFFFF;
			RunOne(Enc::Mpy(6));				// mpy x1,y0
			Assert::AreEqual((int64_t)65535, m.Product());

			// mpy x0,y1 with x0 = 0xFFFF: in DP mode the multiplicand is UNSIGNED.
			m.core->regs.x.l = 0xFFFF;
			m.core->regs.y.h = 1;
			RunOne(Enc::Mpy(5));				// mpy x0,y1
			Assert::AreEqual((int64_t)65535, m.Product());

			// mpy x0,y0 in DP mode: both halves are unsigned.
			m.core->regs.x.l = 0xFFFF;
			m.core->regs.y.l = 0xFFFF;
			RunOne(Enc::Mpy(4));				// mpy x0,y0
			Assert::AreEqual((int64_t)65535 * 65535, m.Product());

			// With DP clear every one of those forms is signed * signed.
			m.core->regs.psr.dp = 0;

			m.core->regs.x.h = 1;
			m.core->regs.y.l = 0xFFFF;
			RunOne(Enc::Mpy(6));				// mpy x1,y0
			Assert::AreEqual((int64_t)1 * -1, m.Product());

			m.core->regs.x.l = 0xFFFF;
			m.core->regs.y.h = 1;
			RunOne(Enc::Mpy(5));				// mpy x0,y1
			Assert::AreEqual((int64_t)-1 * 1, m.Product());

			m.core->regs.x.l = 0xFFFF;
			m.core->regs.y.l = 0xFFFF;
			RunOne(Enc::Mpy(4));				// mpy x0,y0
			Assert::AreEqual((int64_t)(-1) * (-1), m.Product());
		}

		TEST_METHOD(Mac_AccumulatesIntoTheProduct)
		{
			m.core->regs.psr.im = 1;
			m.core->regs.psr.dp = 0;

			RunOne(Enc::ClrP());
			m.core->regs.x.l = 0x0002;
			m.core->regs.y.l = 0x0003;
			RunOne(Enc::Mac(0));				// mac x0,y0
			Assert::AreEqual((int64_t)6, m.Product());

			RunOne(Enc::Mac(0));
			Assert::AreEqual((int64_t)12, m.Product());

			RunOne(Enc::Macn(0));				// macn subtracts
			Assert::AreEqual((int64_t)6, m.Product());
		}

		TEST_METHOD(Mvmpy_MovesTheProductAndMultiplies)
		{
			m.core->regs.psr.im = 1;
			m.core->regs.psr.dp = 0;

			// First produce a known product.
			m.core->regs.x.l = 0x0004;
			m.core->regs.y.l = 0x0005;
			RunOne(Enc::Mpy(4));
			Assert::AreEqual((int64_t)20, m.Product());

			// mvmpy a, x0, y0 : move the old product into a, then multiply.
			m.SetA(0);
			RunOne(Enc::Mvmpy(0, 4));
			Assert::AreEqual((int64_t)20, m.A(), L"mvmpy moves the previous product into the accumulator");
			Assert::AreEqual((int64_t)20, m.Product(), L"and then replaces the product with the new one");
		}

		TEST_METHOD(Admpy_AddsTheProductToTheAccumulator)
		{
			m.core->regs.psr.im = 1;
			m.core->regs.psr.dp = 0;

			m.core->regs.x.l = 0x0004;
			m.core->regs.y.l = 0x0005;
			RunOne(Enc::Mpy(4));
			m.SetA(1000);
			RunOne(Enc::Admpy(0, 4));
			Assert::AreEqual((int64_t)1000 + 20, m.A());
			Assert::AreEqual((int64_t)20, m.Product());
		}

		TEST_METHOD(Rnmpy_RoundsTheProductIntoTheAccumulator)
		{
			m.core->regs.psr.im = 1;
			m.core->regs.psr.dp = 0;

			m.core->regs.x.l = 0x0006;
			m.core->regs.y.l = 0x3FFF;			// +16383
			RunOne(Enc::Mpy(4));				// product = 6 * 16383 = 0x17FFA
			Assert::AreEqual((int64_t)0x17FFA, m.Product());
			m.SetA(0);
			RunOne(Enc::Rnmpy(0, 4));
			// 0x7FFA < 0x8000, so the low word is simply dropped.
			Assert::AreEqual((int64_t)0x10000, m.A(), L"rnmpy rounds the product and keeps bits 39..16");

			// A tie (low word exactly 0x8000) rounds half to even.
			m.core->regs.x.l = 0x0006;
			m.core->regs.y.l = 0x4000;			// product = 0x18000
			RunOne(Enc::Mpy(4));
			m.SetA(0);
			RunOne(Enc::Rnmpy(0, 4));
			Assert::AreEqual((int64_t)0x20000, m.A(), L"0x18000 ties upwards because its lsb is odd");
		}

		TEST_METHOD(MpyX1X1_Form)
		{
			m.core->regs.psr.im = 1;
			m.core->regs.psr.dp = 0;
			m.core->regs.x.h = 0x0005;
			RunOne(Enc::MpyX1X1());
			Assert::AreEqual((int64_t)25, m.Product());
		}
	};
}
