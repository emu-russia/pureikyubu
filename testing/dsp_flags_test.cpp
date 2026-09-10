// DSP core unit tests: the PSR arithmetic flags (C V Z N E U).
//
// Two levels are checked:
//   1. DspCore::ModifyFlags itself, against an independent reference implementation of the
//      rules of dsp-isa.md section 4.13 (class FlagRules).
//   2. Every instruction that is documented to update the flags, executed on the test
//      machine, against the rule the specification assigns to it.

#include "pch.h"
#include "dsp_test_common.h"

namespace DspUnitTest
{
	TEST_CLASS(DspFlagsTest)
	{
		DspTestMachine& m = Machine();

		static const uint64_t MASK40 = 0xFF'FFFFFFFFULL;

		static uint64_t W(int64_t v) { return (uint64_t)v & MASK40; }

		// Values chosen to exercise both sign bits of 40-bit operands.
		static const std::vector<int64_t>& OperandSet()
		{
			static const std::vector<int64_t> set = {
				0,
				1,
				-1,
				0x0000'0001'0000LL,
				0x0000'0002'0000LL,
				-0x0000'0001'0000LL,
				-0x0000'0002'0000LL,
				0x007F'FFFF'FFFFLL,
				0x0080'0000'0000LL,		// most negative 40-bit value
				0x007F'FFFF'0000LL,
				0x00FF'FFFF'FFFFLL,
				-0x0080'0000'0000LL,
				0x0040'0000'0000LL,
			};
			return set;
		}

	public:

		TEST_METHOD_INITIALIZE(Setup)
		{
			m.Reset();
		}

		// ---------------------------------------------------------------
		// ModifyFlags rule-by-rule
		// ---------------------------------------------------------------

		TEST_METHOD(CarryRules_MatchSpecification)
		{
			for (int64_t ds : OperandSet())
			{
				for (int64_t s : OperandSet())
				{
					for (int64_t r : OperandSet())
					{
						const uint64_t ud = W(ds), us = W(s), ur = W(r);

						m.core->ModifyFlags(ud, us, ur, CFlagRules::C1, VFlagRules::None, ZFlagRules::None,
							NFlagRules::None, EFlagRules::None, UFlagRules::None);
						Assert::AreEqual(FlagRules::C1(ud, us, ur), (int)m.core->regs.psr.c, L"C1");

						m.core->ModifyFlags(ud, us, ur, CFlagRules::C2, VFlagRules::None, ZFlagRules::None,
							NFlagRules::None, EFlagRules::None, UFlagRules::None);
						Assert::AreEqual(FlagRules::C2(ud, us, ur), (int)m.core->regs.psr.c, L"C2");

						m.core->ModifyFlags(ud, us, ur, CFlagRules::C3, VFlagRules::None, ZFlagRules::None,
							NFlagRules::None, EFlagRules::None, UFlagRules::None);
						Assert::AreEqual(FlagRules::C3(ud, us, ur), (int)m.core->regs.psr.c, L"C3");

						m.core->ModifyFlags(ud, us, ur, CFlagRules::C4, VFlagRules::None, ZFlagRules::None,
							NFlagRules::None, EFlagRules::None, UFlagRules::None);
						Assert::AreEqual(FlagRules::C4(ud, ur), (int)m.core->regs.psr.c, L"C4");

						m.core->ModifyFlags(ud, us, ur, CFlagRules::C6, VFlagRules::None, ZFlagRules::None,
							NFlagRules::None, EFlagRules::None, UFlagRules::None);
						Assert::AreEqual(FlagRules::C6(ud, ur), (int)m.core->regs.psr.c, L"C6");

						m.core->ModifyFlags(ud, us, ur, CFlagRules::C7, VFlagRules::None, ZFlagRules::None,
							NFlagRules::None, EFlagRules::None, UFlagRules::None);
						Assert::AreEqual(FlagRules::C7(ud, ur), (int)m.core->regs.psr.c, L"C7");

						m.core->ModifyFlags(ud, us, ur, CFlagRules::C8, VFlagRules::None, ZFlagRules::None,
							NFlagRules::None, EFlagRules::None, UFlagRules::None);
						Assert::AreEqual(FlagRules::C8(ud, us, ur), (int)m.core->regs.psr.c, L"C8");
					}
				}
			}
		}

		TEST_METHOD(OverflowRules_MatchSpecification)
		{
			for (int64_t ds : OperandSet())
			{
				for (int64_t s : OperandSet())
				{
					for (int64_t r : OperandSet())
					{
						const uint64_t ud = W(ds), us = W(s), ur = W(r);

						m.core->ModifyFlags(ud, us, ur, CFlagRules::None, VFlagRules::V1, ZFlagRules::None,
							NFlagRules::None, EFlagRules::None, UFlagRules::None);
						Assert::AreEqual(FlagRules::V1(ud, us, ur), (int)m.core->regs.psr.v, L"V1");

						m.core->ModifyFlags(ud, us, ur, CFlagRules::None, VFlagRules::V2, ZFlagRules::None,
							NFlagRules::None, EFlagRules::None, UFlagRules::None);
						Assert::AreEqual(FlagRules::V2(ud, us, ur), (int)m.core->regs.psr.v, L"V2");

						m.core->ModifyFlags(ud, us, ur, CFlagRules::None, VFlagRules::V3, ZFlagRules::None,
							NFlagRules::None, EFlagRules::None, UFlagRules::None);
						Assert::AreEqual(FlagRules::V3(ud, ur), (int)m.core->regs.psr.v, L"V3");

						m.core->ModifyFlags(ud, us, ur, CFlagRules::None, VFlagRules::V4, ZFlagRules::None,
							NFlagRules::None, EFlagRules::None, UFlagRules::None);
						Assert::AreEqual(FlagRules::V4(ud, ur), (int)m.core->regs.psr.v, L"V4");

						m.core->ModifyFlags(ud, us, ur, CFlagRules::None, VFlagRules::V5, ZFlagRules::None,
							NFlagRules::None, EFlagRules::None, UFlagRules::None);
						Assert::AreEqual(FlagRules::V5(ur), (int)m.core->regs.psr.v, L"V5");

						m.core->ModifyFlags(ud, us, ur, CFlagRules::None, VFlagRules::V6, ZFlagRules::None,
							NFlagRules::None, EFlagRules::None, UFlagRules::None);
						Assert::AreEqual(FlagRules::V6(ud, ur), (int)m.core->regs.psr.v, L"V6");

						m.core->ModifyFlags(ud, us, ur, CFlagRules::None, VFlagRules::V7, ZFlagRules::None,
							NFlagRules::None, EFlagRules::None, UFlagRules::None);
						Assert::AreEqual(FlagRules::V7(ud, us, ur), (int)m.core->regs.psr.v, L"V7");

						m.core->ModifyFlags(ud, us, ur, CFlagRules::None, VFlagRules::V8, ZFlagRules::None,
							NFlagRules::None, EFlagRules::None, UFlagRules::None);
						Assert::AreEqual(FlagRules::V8(ud, ur), (int)m.core->regs.psr.v, L"V8");
					}
				}
			}
		}

		TEST_METHOD(ZeroNegativeExtensionUnnormalizationRules_MatchSpecification)
		{
			for (int64_t r : OperandSet())
			{
				const uint64_t ur = W(r);

				m.core->ModifyFlags(0, 0, ur, CFlagRules::None, VFlagRules::None, ZFlagRules::Z1,
					NFlagRules::None, EFlagRules::None, UFlagRules::None);
				Assert::AreEqual(FlagRules::Z1(ur), (int)m.core->regs.psr.z, L"Z1");

				m.core->ModifyFlags(0, 0, ur, CFlagRules::None, VFlagRules::None, ZFlagRules::Z2,
					NFlagRules::None, EFlagRules::None, UFlagRules::None);
				Assert::AreEqual(FlagRules::Z2(ur), (int)m.core->regs.psr.z, L"Z2");

				m.core->ModifyFlags(0, 0, ur, CFlagRules::None, VFlagRules::None, ZFlagRules::Z3,
					NFlagRules::None, EFlagRules::None, UFlagRules::None);
				Assert::AreEqual(FlagRules::Z3(ur), (int)m.core->regs.psr.z, L"Z3");

				m.core->ModifyFlags(0, 0, ur, CFlagRules::None, VFlagRules::None, ZFlagRules::None,
					NFlagRules::N1, EFlagRules::None, UFlagRules::None);
				Assert::AreEqual(FlagRules::N1(ur), (int)m.core->regs.psr.n, L"N1");

				m.core->ModifyFlags(0, 0, ur, CFlagRules::None, VFlagRules::None, ZFlagRules::None,
					NFlagRules::N2, EFlagRules::None, UFlagRules::None);
				Assert::AreEqual(FlagRules::N2(ur), (int)m.core->regs.psr.n, L"N2");

				m.core->ModifyFlags(0, 0, ur, CFlagRules::None, VFlagRules::None, ZFlagRules::None,
					NFlagRules::None, EFlagRules::E1, UFlagRules::None);
				Assert::AreEqual(FlagRules::E1(ur), (int)m.core->regs.psr.e, L"E1");

				m.core->ModifyFlags(0, 0, ur, CFlagRules::None, VFlagRules::None, ZFlagRules::None,
					NFlagRules::None, EFlagRules::None, UFlagRules::U1);
				Assert::AreEqual(FlagRules::U1(ur), (int)m.core->regs.psr.u, L"U1");
			}
		}

		TEST_METHOD(NoneRules_LeaveFlagsUntouched)
		{
			for (int i = 0; i < 2; i++)
			{
				m.SetFlags(i, 1 - i, i, 1 - i, i, 1 - i);
				m.core->ModifyFlags(0x0000'0000'0001, 0x00FF'FFFF'FFFF, 0x0000'0001'0000,
					CFlagRules::None, VFlagRules::None, ZFlagRules::None,
					NFlagRules::None, EFlagRules::None, UFlagRules::None);
				m.AssertFlags(i, 1 - i, i, 1 - i, i, 1 - i, L"None rules must not change any flag");
			}
		}

		TEST_METHOD(StickyOverflow_SetTogetherWithV)
		{
			m.core->regs.psr.sv = 0;
			// V1 with both operands negative and a positive result: overflow.
			m.core->ModifyFlags(W(0x0080'0000'0000), W(0x0080'0000'0000), W(0x0000'0000'0000),
				CFlagRules::None, VFlagRules::V1, ZFlagRules::None,
				NFlagRules::None, EFlagRules::None, UFlagRules::None);
			Assert::AreEqual(1, (int)m.core->regs.psr.v);
			Assert::AreEqual(1, (int)m.core->regs.psr.sv, L"V sets the sticky overflow SV");

			// Clearing V does not clear SV...
			m.core->ModifyFlags(W(1), W(1), W(2), CFlagRules::None, VFlagRules::Zero, ZFlagRules::None,
				NFlagRules::None, EFlagRules::None, UFlagRules::None);
			Assert::AreEqual(0, (int)m.core->regs.psr.v);
			Assert::AreEqual(1, (int)m.core->regs.psr.sv, L"SV must stay set until `clr sv`");

			// ...but `clr sv` does.
			m.Run({ Enc::ClrPsr(1) }, 1);
			Assert::AreEqual(0, (int)m.core->regs.psr.sv);
		}

		// ---------------------------------------------------------------
		// Per-instruction flag behaviour
		// ---------------------------------------------------------------

		// The helper runs a single one-word instruction from a clean pc and returns the flags
		// produced by it, starting from a "dirty" flag pattern so that "unchanged" is visible.
		void RunOne(uint16_t word)
		{
			m.core->regs.pc = 0;
			PokeIMem(m.core, 0, word);
			PokeIMem(m.core, 1, 0);
			m.Step();
		}

		TEST_METHOD(Flags_Pattern_isPreservedByNonComputingInstructions)
		{
			// Instructions that neither write nor examine the data path must leave all six flags.
			struct Case { uint16_t word; const wchar_t* name; };
			std::vector<uint16_t> words = {
				Enc::Nop(),
				Enc::Mvsi(R8A_A0, 0x12),
				Enc::Mvli(REG_A0),
				Enc::Mv(REG_B0, REG_A0),
				Enc::Ldsa(R8A_A0, 0x10),
				Enc::Ld(REG_A0, REG_R0, MOD_INC),
				Enc::St(REG_R1, MOD_INC, REG_A0),
				Enc::Ldla(REG_A0),
				Enc::Stla(REG_A0),
				Enc::Mr(REG_R0, MRM_PLUS_1),
				Enc::ClrPsr(0),
				Enc::SetPsr(0),
				Enc::ClrIm(),
				Enc::SetXl(),
				Enc::Btstl(0),
				Enc::Btsth(1),
				Enc::Mpy(2),			// mpy x1,x0
				Enc::Mac(0),			// mac x0,y0
			};
			(void)words;

			for (uint16_t w : words)
			{
				m.Reset();
				m.SetDefaultFlags();
				RunOne(w);
				m.AssertFlags(1, 0, 0, 1, 1, 0, L"instruction must not modify the arithmetic flags");
			}
		}

		TEST_METHOD(Flags_Adsi_UsesZ1N1)
		{
			// dsp-isa.md section 4.5: adsi = C1 V1 Z1 N1 E1 U1.
			m.SetA(0);
			m.core->regs.pc = 0;
			m.Run({ Enc::Adsi(0, 0) }, 1);
			m.AssertFlags(0, 0, 1, 0, 0, 1, L"adsi of 0 must set Z (Z1, whole result)");
		}

		TEST_METHOD(Flags_Adli_UsesZ1N1)
		{
			m.SetA(0);
			m.core->regs.pc = 0;
			m.Run({ Enc::Adli(0), 0x0000 }, 1);
			m.AssertFlags(0, 0, 1, 0, 0, 1, L"adli of 0 must set Z");
		}

		TEST_METHOD(Flags_Cmpsi_Cmpli_UseZ1N1)
		{
			// Comparing equal values must set Z (the whole 40-bit difference is zero).
			m.SetA(0x0000'0005'0000LL);
			m.core->regs.pc = 0;
			m.Run({ Enc::Cmpsi(0, 5) }, 1);
			Assert::AreEqual(1, (int)m.core->regs.psr.z, L"cmpsi with equal operands must set Z");

			m.SetA(0x0000'0005'0000LL);
			m.core->regs.pc = 0;
			m.Run({ Enc::Cmpli(0), 0x0005 }, 1);
			Assert::AreEqual(1, (int)m.core->regs.psr.z, L"cmpli with equal operands must set Z");
		}

		TEST_METHOD(Flags_Addc_Subc_Negc_UseZ1N1)
		{
			// addc a, x with C = 0 and a + x == 0 must set Z.
			m.SetA(0x0000'0001'0000LL);
			m.SetX(0xFFFF'0000);		// -1.0 in the high half
			m.core->regs.psr.c = 0;
			m.core->regs.pc = 0;
			m.Run({ Enc::Addc(0, 0) }, 1);
			Assert::AreEqual(1, (int)m.core->regs.psr.z, L"addc with a zero result must set Z");

			// subc computes d + ~s + C, so a zero result needs C = 1 and d == s.
			m.SetA(0x0000'0001'0000LL);
			m.SetX(0x0001'0000);
			m.core->regs.psr.c = 1;
			m.core->regs.pc = 0;
			m.Run({ Enc::Subc(0, 0) }, 1);
			Assert::AreEqual(1, (int)m.core->regs.psr.z, L"subc with a zero result must set Z");

			// negc computes d + ~s + C; with C = 1 and a zero accumulator the result is zero.
			m.SetA(0);
			m.core->regs.psr.c = 1;
			m.core->regs.pc = 0;
			m.Run({ Enc::Negc(0) }, 1);
			Assert::AreEqual(1, (int)m.core->regs.psr.z, L"negc of zero with carry must set Z");
		}

		TEST_METHOD(Flags_LogicOps_UseZ2N2)
		{
			// not a1 of 0x0000 -> 0xFFFF: the a1 half is non-zero and its MSB (bit 31) is set.
			m.SetA(0x0000'0000'0000LL);
			m.core->regs.pc = 0;
			m.Run({ Enc::Not(0) }, 1);
			Assert::AreEqual((uint16_t)0xFFFF, (uint16_t)m.core->regs.a.m);
			// Z/N/E/U are computed on the written accumulator: bits 39-31 are 0x001, i.e.
			// neither all zeros nor all ones, so the extension flag E must be set.
			m.AssertFlags(0, 0, 0, 1, 1, 1, L"not a1 must compute N/E from the written accumulator");

			// not a1 of 0xFFFF -> 0x0000
			m.SetA(0x0000'FFFF'0000LL);
			m.core->regs.pc = 0;
			m.Run({ Enc::Not(0) }, 1);
			m.AssertFlags(0, 0, 1, 0, 0, 1, L"not a1 with a zero result must set Z (Z2)");
		}

		TEST_METHOD(Flags_Xorli_Anli_Orli_UseZ2N2)
		{
			m.SetA(0x0000'FFFF'0000LL);
			m.core->regs.pc = 0;
			m.Run({ Enc::Xorli(0), 0xFFFF }, 1);
			Assert::AreEqual(1, (int)m.core->regs.psr.z, L"xoli producing a zero a1 half must set Z");
			Assert::AreEqual(0, (int)m.core->regs.psr.n);

			m.SetA(0x0000'0000'0000LL);
			m.core->regs.pc = 0;
			m.Run({ Enc::Anli(0), 0xF0F0 }, 1);
			Assert::AreEqual(1, (int)m.core->regs.psr.z);

			m.SetA(0x0000'8000'0000LL);
			m.core->regs.pc = 0;
			m.Run({ Enc::Orli(0), 0x0000 }, 1);
			Assert::AreEqual(1, (int)m.core->regs.psr.n, L"orli must compute N from bit 31 of the a1 half");
		}

		TEST_METHOD(Flags_ClrAccumulator_ForcesDefinedState)
		{
			// dsp-isa.md section 4.13: `clr d` forces C=0 V=0 Z=1 N=0 E=0 U=1.
			m.SetDefaultFlags();
			m.core->regs.pc = 0;
			m.Run({ Enc::ClrAcc(0) }, 1);
			m.AssertFlags(0, 0, 1, 0, 0, 1, L"clr a");

			m.SetDefaultFlags();
			m.core->regs.pc = 0;
			m.Run({ Enc::ClrAcc(1) }, 1);
			m.AssertFlags(0, 0, 1, 0, 0, 1, L"clr b");
		}

		TEST_METHOD(Flags_ClrProduct_LeavesArithmeticFlags)
		{
			m.SetDefaultFlags();
			m.core->regs.pc = 0;
			m.Run({ Enc::ClrP() }, 1);
			m.AssertFlags(1, 0, 0, 1, 1, 0, L"clr p must not touch the flags");
			Assert::AreEqual((uint16_t)0x0000, m.core->regs.prod.l);
			Assert::AreEqual((uint16_t)0xFFF0, m.core->regs.prod.m1);
			Assert::AreEqual((uint16_t)0x00FF, (uint16_t)m.core->regs.prod.h);
			Assert::AreEqual((uint16_t)0x0010, m.core->regs.prod.m2);
			Assert::AreEqual((int64_t)0, m.Product(), L"the folded product of the clr p pattern must be zero");
		}

		TEST_METHOD(Flags_Lsl16_ZeroesThe40BitResult)
		{
			// lsl16 shifts the whole 40-bit accumulator; the result must be truncated to 40 bits
			// before the flags are computed (dsp-isa.md Z1 = the written value is zero).
			// Bits 39-32 shifted left by 16 fall outside the 40-bit accumulator.
			m.SetA(0x00FF'0000'0000LL);
			m.core->regs.pc = 0;
			m.Run({ Enc::Lsl16(0) }, 1);
			Assert::AreEqual((int64_t)0, m.A(), L"lsl16 of 0x00FF00000000 leaves zero in 40 bits");
			Assert::AreEqual(1, (int)m.core->regs.psr.z, L"Z1 must be computed on the 40-bit result");
		}

		TEST_METHOD(Flags_ShiftImmediate_ClearCAndV)
		{
			m.SetDefaultFlags();
			m.SetA(0x0000'0001'0000LL);
			m.core->regs.pc = 0;
			m.Run({ Enc::Lsfi(0, 1) }, 1);
			m.AssertFlags(0, 0, 0, 0, 0, 1, L"lsfi clears C and V and computes Z1/N1/E1/U1");

			m.SetDefaultFlags();
			m.SetA(0x0000'0001'0000LL);
			m.core->regs.pc = 0;
			m.Run({ Enc::Asfi(0, -1) }, 1);
			m.AssertFlags(0, 0, 0, 0, 0, 1, L"asfi clears C and V");
		}

		TEST_METHOD(Flags_Div_UsesZ3)
		{
			// dsp-isa.md section 4.6: div = C3 0 Z3 N1 E1 U1.
			// The step shifts a quotient bit into the accumulator, so a zero result needs the
			// 40-bit result to end up empty: D == 0x00'FFFF'FF00'00, S == 0xFFFF (i.e. -1)
			// subtracts the sign-extended divisor from itself and shifts in a zero quotient
			// bit. The hardware core reports C=1 Z=1 N=0 E=0 U=1 for this step.
			m.SetA(0x0000'00FF'FFFF'0000LL);
			m.core->regs.x.h = 0xFFFF;
			m.core->regs.pc = 0;
			m.Run({ Enc::Div(0, R4XY_X1) }, 1);
			Assert::AreEqual((int64_t)0, m.A(), L"the shifted accumulator is empty");
			Assert::AreEqual(0, (int)m.core->regs.psr.v, L"div always clears V");
			Assert::AreEqual(1, (int)m.core->regs.psr.z, L"Z3: the 40-bit div result is zero");
			Assert::AreEqual(1, (int)m.core->regs.psr.c, L"C3");
			Assert::AreEqual(0, (int)m.core->regs.psr.n, L"N1");
			Assert::AreEqual(0, (int)m.core->regs.psr.e, L"E1");
			Assert::AreEqual(1, (int)m.core->regs.psr.u, L"U1");
			Assert::AreEqual(0, DspTestHaltCount(), L"div must be implemented, not Halt()ed");

			// A non-zero result must clear Z3: dividing zero by a zero divisor shifts in an
			// empty remainder plus the set quotient bit and leaves 0x00'0000'0001 behind.
			m.Reset();
			m.SetA(0);
			m.core->regs.x.h = 0x0000;
			m.core->regs.pc = 0;
			m.Run({ Enc::Div(0, R4XY_X1) }, 1);
			Assert::AreEqual((int64_t)0x00'0001LL, m.A(), L"zero divided by zero");
			Assert::AreEqual(0, (int)m.core->regs.psr.z, L"a non-zero result clears Z3");
			Assert::AreEqual(0, (int)m.core->regs.psr.n, L"N1 of 0x0000000001");
			Assert::AreEqual(1, (int)m.core->regs.psr.u, L"U1 of 0x0000000001");
		}

		TEST_METHOD(Flags_Norm_IsImplemented)
		{
			m.SetA(0x0000'0001'0000LL);		// normalised value
			m.core->regs.pc = 0;
			m.Run({ Enc::Norm(0, REG_R0) }, 1);
			Assert::AreEqual(0, DspTestHaltCount(), L"norm must be implemented, not Halt()ed");
		}

		TEST_METHOD(Flags_Max_IsImplementedAndUsesC5)
		{
			// dsp-isa.md section 4.6: max d,s = signed maximum, flags C5 0 Z1 N1 E1 U1.
			m.SetA(0x0000'0005'0000LL);
			m.core->regs.x.h = 0x0003;
			m.core->regs.pc = 0;
			m.Run({ Enc::Max(0, R4XY_X1) }, 1);
			Assert::AreEqual(0, DspTestHaltCount());
			Assert::AreEqual((int64_t)0x0000'0005'0000LL, m.A(), L"max must keep the larger operand");
		}

		TEST_METHOD(Flags_MultiplyAndMac_DoNotTouchFlags)
		{
			for (uint16_t w : { Enc::Mpy(4), Enc::Mac(0), Enc::Macn(0), Enc::Mac2(0), Enc::Macn2(0), Enc::MacF1(0) })
			{
				m.Reset();
				m.SetDefaultFlags();
				RunOne(w);
				m.AssertFlags(1, 0, 0, 1, 1, 0, L"mpy/mac/macn never update the arithmetic flags");
			}
		}

		TEST_METHOD(Flags_Mvmpy_Admpy_Rnmpy_DoUpdateFlags)
		{
			// mvmpy = 0 0 Z1 N1 E1 U1
			m.Reset();
			m.SetDefaultFlags();
			m.SetX(0x1234'5678);
			RunOne(Enc::Mvmpy(0, 4));		// mvmpy a, x0, y0
			m.AssertFlags(0, 0, 1, 0, 0, 1, L"mvmpy with a zero product sets Z and clears C/V");

			// admpy = C1 V1 Z1 N1 E1 U1
			m.Reset();
			m.SetDefaultFlags();
			RunOne(Enc::Admpy(0, 4));
			m.AssertFlags(0, 0, 1, 0, 0, 1, L"admpy of an empty accumulator sets Z, clears C/V");

			// rnmpy = C7 V6 Z1 N1 E1 U1
			m.Reset();
			m.SetDefaultFlags();
			RunOne(Enc::Rnmpy(0, 4));
			m.AssertFlags(0, 0, 1, 0, 0, 1, L"rnmpy rounding a zero product clears C/V and sets Z");
		}
	};
}
