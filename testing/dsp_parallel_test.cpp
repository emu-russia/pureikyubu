// DSP core unit tests: parallel ("packed") instruction words, the dual/load-store move
// forms, circular addressing and the data-memory addressing modes.
//
// Reference: dsp-isa.md sections 2, 3 and 4.12 and dsp.md sections 2.4 and 3.2. Instruction
// words come from the builders of dsp_test_common.h.

#include "pch.h"
#include "dsp_test_common.h"

namespace DspUnitTest
{
	TEST_CLASS(DspParallelTest)
	{
		DspTestMachine& m = Machine();

		static const int64_t H = 0x0000'0001'0000LL;

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

		// ---------------------------------------------------------------
		// Both halves of a packed word execute in the same cycle
		// ---------------------------------------------------------------

		TEST_METHOD(Packed_AddAndLoad)
		{
			// add a, x1  +  ld x0, r0, +
			m.DMem(0x0010, 0x1234);
			m.core->regs.r[0] = 0x0010;
			m.core->regs.l[0] = 0xFFFF;
			m.core->regs.x.h = 3;
			m.SetA(2 * H);

			RunOne(Enc::Add(0, R8P_X1, Enc::PLd(R8A_X0, REG_R0, 0)));

			Assert::AreEqual(5 * H, m.A(), L"the ALU half must have run");
			Assert::AreEqual((uint16_t)0x1234, m.core->regs.x.l, L"the move half must have run");
			Assert::AreEqual((uint16_t)0x0011, m.core->regs.r[0], L"the move half post-increments rn");
		}

		TEST_METHOD(Packed_StoreAndSubtract)
		{
			// sub a, y1  +  st r1, +, a0
			m.core->regs.a.l = 0x0BAD;
			m.core->regs.y.h = 1;
			m.SetA(m.A() | (5 * H) | 0x0BAD);
			m.core->regs.r[1] = 0x0020;
			m.core->regs.l[1] = 0xFFFF;

			RunOne(Enc::Sub(0, R8P_Y1, Enc::PSt(REG_R1, 0, R4AB0_A0)));

			Assert::AreEqual((uint16_t)0x0BAD, m.DMem(0x0020));
			Assert::AreEqual((uint16_t)0x0021, m.core->regs.r[1]);
			Assert::AreEqual((int64_t)0x0000'0004'0BADLL, m.A(), L"5 - 1 in the high half, a0 untouched");
		}

		TEST_METHOD(Packed_RegisterMove)
		{
			// nop + mv x0, a1
			m.core->regs.a.m = 0xBEEF;
			RunOne((uint16_t)(0x8000 | Enc::PMv(R4XY_X0, R4AB0_A0) | 0x0010 | 2));
			Assert::AreEqual((uint16_t)0xBEEF, m.core->regs.x.l);
		}

		TEST_METHOD(Packed_AddressRegisterModify)
		{
			// nop + mr r2, +1
			m.core->regs.r[2] = 0x0100;
			m.core->regs.l[2] = 0xFFFF;
			RunOne((uint16_t)(0x8000 | Enc::PMr(REG_R2, MOD_INC)));
			Assert::AreEqual((uint16_t)0x0101, m.core->regs.r[2]);
		}

		TEST_METHOD(Packed_MemoryHalfReadsItsOperandAtTheStartOfTheCycle)
		{
			// `amv a, x1`  +  `ls x1, r0, +, r3, +, a`
			//
			// Both halves of a packed word are clocked by the same cycle: they sample the register
			// file at its start, and only write their results at its end. A store therefore still
			// sees the value the accumulator had before the compute half moved x1 into it.
			//
			// The shipped microcodes are built on this: their block moves are pipelined, one word
			// behind (`amv` prepares the word for the *next* store while the same cycle's `ls`
			// stores the previous one). The IPL audio microcode fills its voice state structure
			// that way, and getting it wrong shifts every copied word -- and therefore every
			// pointer read out of that structure -- by one word.
			m.core->regs.a.m = 0x1111;			// the word this cycle must store
			m.core->regs.x.h = 0x2222;			// the word the compute half moves into `a`
			m.core->regs.r[0] = 0x0010;
			m.core->regs.l[0] = 0xFFFF;
			m.core->regs.r[3] = 0x0020;
			m.core->regs.l[3] = 0xFFFF;

			RunOne(Enc::Amv(0, R8P_X1, Enc::PLs(R4XY_X1, 0, 0, R4AB0_A)));

			Assert::AreEqual((uint16_t)0x1111, m.DMem(0x0020), L"the store uses the accumulator value from the start of the cycle");
			Assert::AreEqual((uint16_t)0x2222, m.core->regs.a.m, L"the compute half still moves x1 into a");
			Assert::AreEqual((uint16_t)0x0011, m.core->regs.r[0], L"the load half post-increments its address register");
			Assert::AreEqual((uint16_t)0x0021, m.core->regs.r[3], L"the store half post-increments its address register");
		}

		TEST_METHOD(Packed_MemoryHalfLatchDoesNotLeakIntoTheNextWord)
		{
			// A packed word without a memory read (or a single-word instruction) must not reuse the
			// value latched by the previous word.
			m.core->regs.a.m = 0x1111;
			m.core->regs.x.h = 0x2222;
			m.core->regs.r[0] = 0x0010;
			m.core->regs.l[0] = 0xFFFF;
			m.core->regs.r[3] = 0x0020;
			m.core->regs.l[3] = 0xFFFF;

			RunOne(Enc::Amv(0, R8P_X1, Enc::PLs(R4XY_X1, 0, 0, R4AB0_A)));

			// The next word stores `a` through the standalone `st`, which reads the register file
			// directly and must see the value the compute half left there.
			m.core->regs.a.m = 0x3333;
			m.core->regs.r[3] = 0x0030;
			RunOne(Enc::St(REG_R3, MOD_INC, REG_A1));

			Assert::AreEqual((uint16_t)0x3333, m.DMem(0x0030), L"a standalone store reads the live register");
		}

		TEST_METHOD(Packed_FlagsComeFromTheComputeHalfOnly)
		{
			// The move half never updates the arithmetic flags (dsp-isa.md 4.13).
			m.DMem(0x0010, 0xFFFF);
			m.core->regs.r[0] = 0x0010;
			m.core->regs.l[0] = 0xFFFF;
			m.SetDefaultFlags();
			m.SetA(1 * H);
			m.core->regs.x.h = 1;

			RunOne(Enc::Add(0, R8P_X1, Enc::PLd(R8A_X0, REG_R0, 0)));

			// 2.0 is a clean value: C = 0, V = 0, Z = 0, N = 0, E = 0, U = 1.
			m.AssertFlags(0, 0, 0, 0, 0, 1, L"only the compute half may touch the flags");
		}

		TEST_METHOD(Packed_NoOperationInTheUpperHalf)
		{
			// 0x8000 in the compute slot is the packed nop (dsp-isa.md section 2).
			m.DMem(0x0030, 0x7777);
			m.core->regs.r[0] = 0x0030;
			m.core->regs.l[0] = 0xFFFF;
			RunOne((uint16_t)(0x8000 | Enc::PLd(R8A_A0, REG_R0, 0)));
			Assert::AreEqual((uint16_t)0x7777, m.core->regs.a.l);
		}

		// ---------------------------------------------------------------
		// Dual load (ldd) and load-and-store (ls)
		// ---------------------------------------------------------------

		TEST_METHOD(Ldd_LoadsTwoRegistersInOneCycle)
		{
			// ldd x0,r0,+ y0,r3,+
			m.DMem(0x0010, 0x1111);
			m.DMem(0x0020, 0x2222);
			m.core->regs.r[0] = 0x0010;
			m.core->regs.r[3] = 0x0020;
			m.core->regs.l[0] = 0xFFFF;
			m.core->regs.l[3] = 0xFFFF;

			// d_pair = 0 -> {x0, y0}
			RunOne((uint16_t)(0x8000 | Enc::PLdd(0, REG_R0, 0, 0)));

			Assert::AreEqual((uint16_t)0x1111, m.core->regs.x.l);
			Assert::AreEqual((uint16_t)0x2222, m.core->regs.y.l);
			Assert::AreEqual((uint16_t)0x0011, m.core->regs.r[0]);
			Assert::AreEqual((uint16_t)0x0021, m.core->regs.r[3]);
		}

		TEST_METHOD(Ldd_PairSelectsBothHalves)
		{
			m.DMem(0x0010, 0x3333);
			m.DMem(0x0020, 0x4444);
			m.core->regs.r[0] = 0x0010;
			m.core->regs.r[3] = 0x0020;
			m.core->regs.l[0] = 0xFFFF;
			m.core->regs.l[3] = 0xFFFF;

			// d_pair = 3 -> {x1, y1}
			RunOne((uint16_t)(0x8000 | Enc::PLdd(3, REG_R0, 0, 0)));

			Assert::AreEqual((uint16_t)0x3333, m.core->regs.x.h);
			Assert::AreEqual((uint16_t)0x4444, m.core->regs.y.h);
		}

		TEST_METHOD(Ls_LoadAndStoreInOneCycle)
		{
			// ls x0, r0, +  r3, +, a1 : load through r0, store through r3.
			m.DMem(0x0010, 0xABCD);
			m.core->regs.a.m = 0x5678;
			m.core->regs.r[0] = 0x0010;
			m.core->regs.r[3] = 0x0020;
			m.core->regs.l[0] = 0xFFFF;
			m.core->regs.l[3] = 0xFFFF;

			RunOne((uint16_t)(0x8000 | Enc::PLs(R4XY_X0, 0, 0, 0)));

			Assert::AreEqual((uint16_t)0xABCD, m.core->regs.x.l, L"the load must have happened");
			Assert::AreEqual((uint16_t)0x5678, m.DMem(0x0020), L"the store must have happened");
			Assert::AreEqual((uint16_t)0x0011, m.core->regs.r[0]);
			Assert::AreEqual((uint16_t)0x0021, m.core->regs.r[3]);
		}

		TEST_METHOD(Ls2_LoadsThroughR3AndStoresThroughR0)
		{
			m.DMem(0x0020, 0x1234);
			m.core->regs.a.m = 0x4321;
			m.core->regs.r[0] = 0x0010;
			m.core->regs.r[3] = 0x0020;
			m.core->regs.l[0] = 0xFFFF;
			m.core->regs.l[3] = 0xFFFF;

			RunOne((uint16_t)(0x8000 | Enc::PLs2(R4XY_X0, 0, 0, 0)));

			Assert::AreEqual((uint16_t)0x1234, m.core->regs.x.l, L"ls2 loads through r3");
			Assert::AreEqual((uint16_t)0x4321, m.DMem(0x0010), L"and stores through r0");
		}

		// ---------------------------------------------------------------
		// Data memory addressing
		// ---------------------------------------------------------------

		TEST_METHOD(Ld_WithModifierRegister)
		{
			m.DMem(0x0100, 0xAAAA);
			m.core->regs.r[1] = 0x0100;
			m.core->regs.m[1] = 0x0010;
			m.core->regs.l[1] = 0xFFFF;

			RunOne(Enc::Ld(REG_A0, REG_R1, 3));		// mn = 3 -> +m
			Assert::AreEqual((uint16_t)0xAAAA, m.core->regs.a.l);
			Assert::AreEqual((uint16_t)0x0110, m.core->regs.r[1]);
		}

		TEST_METHOD(St_WithModifierRegister)
		{
			m.core->regs.a.l = 0x1357;
			m.core->regs.r[2] = 0x0200;
			m.core->regs.m[2] = 8;
			m.core->regs.l[2] = 0xFFFF;

			RunOne(Enc::St(REG_R2, 3, REG_A0));
			Assert::AreEqual((uint16_t)0x1357, m.DMem(0x0200));
			Assert::AreEqual((uint16_t)0x0208, m.core->regs.r[2]);
		}

		// ---------------------------------------------------------------
		// Circular addressing (dsp.md 2.4)
		// ---------------------------------------------------------------

		TEST_METHOD(CircularAddressing_GoldenVectors)
		{
			// The address unit keeps the pointer inside a buffer of l+1 entries, but the
			// buffer is aligned to the next power of two above l rather than to its own
			// length: for 2^(n-1) <= l < 2^n it is the top (l+1) entries of the 2^n-aligned
			// block that contains the current address. (dsp.md section 2.4 describes the
			// buffer as "l+1 entries"; the alignment rule is what makes a length register of
			// 2^k - 1 wrap cleanly, which is the only kind real microcode uses.)
			//
			// The table below is the recorded behaviour of the address unit for every
			// interesting combination of length, address and modifier:
			//   { address, length, modifier, expected address }
			struct Case { uint16_t r, l; int16_t m; uint16_t expected; };
			static const Case golden[] = {
			{ 0x3529, 0x000C,      1, 0x352A },
			{ 0x352F, 0x000C,      1, 0x3523 },
			{ 0x3523, 0x000C,     -1, 0x352F },
			{ 0x3529, 0x000C,     -1, 0x3528 },
			{ 0x3523, 0x000C,      1, 0x3524 },
			{ 0x352F, 0x000C,     -1, 0x352E },
			{ 0x3520, 0x000C,     -1, 0x352C },
			{ 0x352F, 0x000C,     12, 0x352E },
			{ 0x3523, 0x000C,    -12, 0x3524 },
			{ 0x352C, 0x000C,      4, 0x3523 },
			{ 0x0000, 0x0000,      1, 0x0001 },
			{ 0x0000, 0x0000,     -1, 0x0000 },
			{ 0x0000, 0x0000,      0, 0x0000 },
			{ 0x0001, 0x0000,      1, 0x0001 },
			{ 0x0001, 0x0000,     -1, 0x0001 },
			{ 0x0001, 0x0000,      0, 0x0001 },
			{ 0x1000, 0x0000,      1, 0x1001 },
			{ 0x1000, 0x0000,     -1, 0x1000 },
			{ 0x1000, 0x0000,      0, 0x1000 },
			{ 0x0000, 0x0001,      1, 0x0001 },
			{ 0x0000, 0x0001,     -1, 0x0001 },
			{ 0x0001, 0x0001,      1, 0x0000 },
			{ 0x0001, 0x0001,     -1, 0x0000 },
			{ 0x0002, 0x0001,      1, 0x0003 },
			{ 0x0002, 0x0001,     -1, 0x0003 },
			{ 0x0003, 0x0001,      1, 0x0002 },
			{ 0x0003, 0x0001,     -1, 0x0002 },
			{ 0x1000, 0x0001,      1, 0x1001 },
			{ 0x1000, 0x0001,     -1, 0x1001 },
			{ 0x1001, 0x0001,      1, 0x1000 },
			{ 0x1001, 0x0001,     -1, 0x1000 },
			{ 0x0000, 0x0003,      1, 0x0001 },
			{ 0x0000, 0x0003,     -1, 0x0003 },
			{ 0x0000, 0x0003,      3, 0x0003 },
			{ 0x0000, 0x0003,     -3, 0x0001 },
			{ 0x0003, 0x0003,      1, 0x0000 },
			{ 0x0003, 0x0003,     -1, 0x0002 },
			{ 0x0003, 0x0003,      3, 0x0002 },
			{ 0x0003, 0x0003,     -3, 0x0000 },
			{ 0x0002, 0x0003,      1, 0x0003 },
			{ 0x0002, 0x0003,     -1, 0x0001 },
			{ 0x0002, 0x0003,      3, 0x0001 },
			{ 0x0002, 0x0003,     -3, 0x0003 },
			{ 0x0004, 0x0003,      1, 0x0005 },
			{ 0x0004, 0x0003,     -1, 0x0007 },
			{ 0x0004, 0x0003,      3, 0x0007 },
			{ 0x0004, 0x0003,     -3, 0x0005 },
			{ 0x0007, 0x0003,      1, 0x0004 },
			{ 0x0007, 0x0003,     -1, 0x0006 },
			{ 0x0007, 0x0003,      3, 0x0006 },
			{ 0x0007, 0x0003,     -3, 0x0004 },
			{ 0x0006, 0x0003,      1, 0x0007 },
			{ 0x0006, 0x0003,     -1, 0x0005 },
			{ 0x0006, 0x0003,      3, 0x0005 },
			{ 0x0006, 0x0003,     -3, 0x0007 },
			{ 0x1000, 0x0003,      1, 0x1001 },
			{ 0x1000, 0x0003,     -1, 0x1003 },
			{ 0x1000, 0x0003,      3, 0x1003 },
			{ 0x1000, 0x0003,     -3, 0x1001 },
			{ 0x1003, 0x0003,      1, 0x1000 },
			{ 0x1003, 0x0003,     -1, 0x1002 },
			{ 0x1003, 0x0003,      3, 0x1002 },
			{ 0x1003, 0x0003,     -3, 0x1000 },
			{ 0x1002, 0x0003,      1, 0x1003 },
			{ 0x1002, 0x0003,     -1, 0x1001 },
			{ 0x1002, 0x0003,      3, 0x1001 },
			{ 0x1002, 0x0003,     -3, 0x1003 },
			{ 0x0000, 0x0007,      1, 0x0001 },
			{ 0x0000, 0x0007,     -1, 0x0007 },
			{ 0x0000, 0x0007,      3, 0x0003 },
			{ 0x0000, 0x0007,     -3, 0x0005 },
			{ 0x0007, 0x0007,      1, 0x0000 },
			{ 0x0007, 0x0007,     -1, 0x0006 },
			{ 0x0007, 0x0007,      3, 0x0002 },
			{ 0x0007, 0x0007,     -3, 0x0004 },
			{ 0x0004, 0x0007,      1, 0x0005 },
			{ 0x0004, 0x0007,     -1, 0x0003 },
			{ 0x0004, 0x0007,      3, 0x0007 },
			{ 0x0004, 0x0007,     -3, 0x0001 },
			{ 0x0008, 0x0007,      1, 0x0009 },
			{ 0x0008, 0x0007,     -1, 0x000F },
			{ 0x0008, 0x0007,      3, 0x000B },
			{ 0x0008, 0x0007,     -3, 0x000D },
			{ 0x000F, 0x0007,      1, 0x0008 },
			{ 0x000F, 0x0007,     -1, 0x000E },
			{ 0x000F, 0x0007,      3, 0x000A },
			{ 0x000F, 0x0007,     -3, 0x000C },
			{ 0x000C, 0x0007,      1, 0x000D },
			{ 0x000C, 0x0007,     -1, 0x000B },
			{ 0x000C, 0x0007,      3, 0x000F },
			{ 0x000C, 0x0007,     -3, 0x0009 },
			{ 0x1000, 0x0007,      1, 0x1001 },
			{ 0x1000, 0x0007,     -1, 0x1007 },
			{ 0x1000, 0x0007,      3, 0x1003 },
			{ 0x1000, 0x0007,     -3, 0x1005 },
			{ 0x1007, 0x0007,      1, 0x1000 },
			{ 0x1007, 0x0007,     -1, 0x1006 },
			{ 0x1007, 0x0007,      3, 0x1002 },
			{ 0x1007, 0x0007,     -3, 0x1004 },
			{ 0x1004, 0x0007,      1, 0x1005 },
			{ 0x1004, 0x0007,     -1, 0x1003 },
			{ 0x1004, 0x0007,      3, 0x1007 },
			{ 0x1004, 0x0007,     -3, 0x1001 },
			{ 0x0000, 0x000F,      1, 0x0001 },
			{ 0x0000, 0x000F,     -1, 0x000F },
			{ 0x0000, 0x000F,      3, 0x0003 },
			{ 0x0000, 0x000F,     -3, 0x000D },
			{ 0x000F, 0x000F,      1, 0x0000 },
			{ 0x000F, 0x000F,     -1, 0x000E },
			{ 0x000F, 0x000F,      3, 0x0002 },
			{ 0x000F, 0x000F,     -3, 0x000C },
			{ 0x0008, 0x000F,      1, 0x0009 },
			{ 0x0008, 0x000F,     -1, 0x0007 },
			{ 0x0008, 0x000F,      3, 0x000B },
			{ 0x0008, 0x000F,     -3, 0x0005 },
			{ 0x0010, 0x000F,      1, 0x0011 },
			{ 0x0010, 0x000F,     -1, 0x001F },
			{ 0x0010, 0x000F,      3, 0x0013 },
			{ 0x0010, 0x000F,     -3, 0x001D },
			{ 0x001F, 0x000F,      1, 0x0010 },
			{ 0x001F, 0x000F,     -1, 0x001E },
			{ 0x001F, 0x000F,      3, 0x0012 },
			{ 0x001F, 0x000F,     -3, 0x001C },
			{ 0x0018, 0x000F,      1, 0x0019 },
			{ 0x0018, 0x000F,     -1, 0x0017 },
			{ 0x0018, 0x000F,      3, 0x001B },
			{ 0x0018, 0x000F,     -3, 0x0015 },
			{ 0x1000, 0x000F,      1, 0x1001 },
			{ 0x1000, 0x000F,     -1, 0x100F },
			{ 0x1000, 0x000F,      3, 0x1003 },
			{ 0x1000, 0x000F,     -3, 0x100D },
			{ 0x100F, 0x000F,      1, 0x1000 },
			{ 0x100F, 0x000F,     -1, 0x100E },
			{ 0x100F, 0x000F,      3, 0x1002 },
			{ 0x100F, 0x000F,     -3, 0x100C },
			{ 0x1008, 0x000F,      1, 0x1009 },
			{ 0x1008, 0x000F,     -1, 0x1007 },
			{ 0x1008, 0x000F,      3, 0x100B },
			{ 0x1008, 0x000F,     -3, 0x1005 },
			{ 0x0000, 0x001F,      1, 0x0001 },
			{ 0x0000, 0x001F,     -1, 0x001F },
			{ 0x0000, 0x001F,      3, 0x0003 },
			{ 0x0000, 0x001F,     -3, 0x001D },
			{ 0x001F, 0x001F,      1, 0x0000 },
			{ 0x001F, 0x001F,     -1, 0x001E },
			{ 0x001F, 0x001F,      3, 0x0002 },
			{ 0x001F, 0x001F,     -3, 0x001C },
			{ 0x0010, 0x001F,      1, 0x0011 },
			{ 0x0010, 0x001F,     -1, 0x000F },
			{ 0x0010, 0x001F,      3, 0x0013 },
			{ 0x0010, 0x001F,     -3, 0x000D },
			{ 0x0020, 0x001F,      1, 0x0021 },
			{ 0x0020, 0x001F,     -1, 0x003F },
			{ 0x0020, 0x001F,      3, 0x0023 },
			{ 0x0020, 0x001F,     -3, 0x003D },
			{ 0x003F, 0x001F,      1, 0x0020 },
			{ 0x003F, 0x001F,     -1, 0x003E },
			{ 0x003F, 0x001F,      3, 0x0022 },
			{ 0x003F, 0x001F,     -3, 0x003C },
			{ 0x0030, 0x001F,      1, 0x0031 },
			{ 0x0030, 0x001F,     -1, 0x002F },
			{ 0x0030, 0x001F,      3, 0x0033 },
			{ 0x0030, 0x001F,     -3, 0x002D },
			{ 0x1000, 0x001F,      1, 0x1001 },
			{ 0x1000, 0x001F,     -1, 0x101F },
			{ 0x1000, 0x001F,      3, 0x1003 },
			{ 0x1000, 0x001F,     -3, 0x101D },
			{ 0x101F, 0x001F,      1, 0x1000 },
			{ 0x101F, 0x001F,     -1, 0x101E },
			{ 0x101F, 0x001F,      3, 0x1002 },
			{ 0x101F, 0x001F,     -3, 0x101C },
			{ 0x1010, 0x001F,      1, 0x1011 },
			{ 0x1010, 0x001F,     -1, 0x100F },
			{ 0x1010, 0x001F,      3, 0x1013 },
			{ 0x1010, 0x001F,     -3, 0x100D },
			{ 0x0000, 0x00FF,      1, 0x0001 },
			{ 0x0000, 0x00FF,     -1, 0x00FF },
			{ 0x0000, 0x00FF,      3, 0x0003 },
			{ 0x0000, 0x00FF,     -3, 0x00FD },
			{ 0x00FF, 0x00FF,      1, 0x0000 },
			{ 0x00FF, 0x00FF,     -1, 0x00FE },
			{ 0x00FF, 0x00FF,      3, 0x0002 },
			{ 0x00FF, 0x00FF,     -3, 0x00FC },
			{ 0x0080, 0x00FF,      1, 0x0081 },
			{ 0x0080, 0x00FF,     -1, 0x007F },
			{ 0x0080, 0x00FF,      3, 0x0083 },
			{ 0x0080, 0x00FF,     -3, 0x007D },
			{ 0x0100, 0x00FF,      1, 0x0101 },
			{ 0x0100, 0x00FF,     -1, 0x01FF },
			{ 0x0100, 0x00FF,      3, 0x0103 },
			{ 0x0100, 0x00FF,     -3, 0x01FD },
			{ 0x01FF, 0x00FF,      1, 0x0100 },
			{ 0x01FF, 0x00FF,     -1, 0x01FE },
			{ 0x01FF, 0x00FF,      3, 0x0102 },
			{ 0x01FF, 0x00FF,     -3, 0x01FC },
			{ 0x0180, 0x00FF,      1, 0x0181 },
			{ 0x0180, 0x00FF,     -1, 0x017F },
			{ 0x0180, 0x00FF,      3, 0x0183 },
			{ 0x0180, 0x00FF,     -3, 0x017D },
			{ 0x1000, 0x00FF,      1, 0x1001 },
			{ 0x1000, 0x00FF,     -1, 0x10FF },
			{ 0x1000, 0x00FF,      3, 0x1003 },
			{ 0x1000, 0x00FF,     -3, 0x10FD },
			{ 0x10FF, 0x00FF,      1, 0x1000 },
			{ 0x10FF, 0x00FF,     -1, 0x10FE },
			{ 0x10FF, 0x00FF,      3, 0x1002 },
			{ 0x10FF, 0x00FF,     -3, 0x10FC },
			{ 0x1080, 0x00FF,      1, 0x1081 },
			{ 0x1080, 0x00FF,     -1, 0x107F },
			{ 0x1080, 0x00FF,      3, 0x1083 },
			{ 0x1080, 0x00FF,     -3, 0x107D },
			{ 0x0000, 0x0FFF,      1, 0x0001 },
			{ 0x0000, 0x0FFF,     -1, 0x0FFF },
			{ 0x0000, 0x0FFF,      3, 0x0003 },
			{ 0x0000, 0x0FFF,     -3, 0x0FFD },
			{ 0x0FFF, 0x0FFF,      1, 0x0000 },
			{ 0x0FFF, 0x0FFF,     -1, 0x0FFE },
			{ 0x0FFF, 0x0FFF,      3, 0x0002 },
			{ 0x0FFF, 0x0FFF,     -3, 0x0FFC },
			{ 0x0800, 0x0FFF,      1, 0x0801 },
			{ 0x0800, 0x0FFF,     -1, 0x07FF },
			{ 0x0800, 0x0FFF,      3, 0x0803 },
			{ 0x0800, 0x0FFF,     -3, 0x07FD },
			{ 0x1000, 0x0FFF,      1, 0x1001 },
			{ 0x1000, 0x0FFF,     -1, 0x1FFF },
			{ 0x1000, 0x0FFF,      3, 0x1003 },
			{ 0x1000, 0x0FFF,     -3, 0x1FFD },
			{ 0x1FFF, 0x0FFF,      1, 0x1000 },
			{ 0x1FFF, 0x0FFF,     -1, 0x1FFE },
			{ 0x1FFF, 0x0FFF,      3, 0x1002 },
			{ 0x1FFF, 0x0FFF,     -3, 0x1FFC },
			{ 0x1800, 0x0FFF,      1, 0x1801 },
			{ 0x1800, 0x0FFF,     -1, 0x17FF },
			{ 0x1800, 0x0FFF,      3, 0x1803 },
			{ 0x1800, 0x0FFF,     -3, 0x17FD },
			{ 0x0000, 0x7FFF,      1, 0x0001 },
			{ 0x0000, 0x7FFF,     -1, 0x7FFF },
			{ 0x0000, 0x7FFF,      3, 0x0003 },
			{ 0x0000, 0x7FFF,     -3, 0x7FFD },
			{ 0x7FFF, 0x7FFF,      1, 0x0000 },
			{ 0x7FFF, 0x7FFF,     -1, 0x7FFE },
			{ 0x7FFF, 0x7FFF,      3, 0x0002 },
			{ 0x7FFF, 0x7FFF,     -3, 0x7FFC },
			{ 0x4000, 0x7FFF,      1, 0x4001 },
			{ 0x4000, 0x7FFF,     -1, 0x3FFF },
			{ 0x4000, 0x7FFF,      3, 0x4003 },
			{ 0x4000, 0x7FFF,     -3, 0x3FFD },
			{ 0x8000, 0x7FFF,      1, 0x8001 },
			{ 0x8000, 0x7FFF,     -1, 0xFFFF },
			{ 0x8000, 0x7FFF,      3, 0x8003 },
			{ 0x8000, 0x7FFF,     -3, 0xFFFD },
			{ 0xFFFF, 0x7FFF,      1, 0x8000 },
			{ 0xFFFF, 0x7FFF,     -1, 0xFFFE },
			{ 0xFFFF, 0x7FFF,      3, 0x8002 },
			{ 0xFFFF, 0x7FFF,     -3, 0xFFFC },
			{ 0xC000, 0x7FFF,      1, 0xC001 },
			{ 0xC000, 0x7FFF,     -1, 0xBFFF },
			{ 0xC000, 0x7FFF,      3, 0xC003 },
			{ 0xC000, 0x7FFF,     -3, 0xBFFD },
			{ 0x0001, 0x0002,      1, 0x0002 },
			{ 0x0001, 0x0002,     -1, 0x0003 },
			{ 0x0003, 0x0002,      1, 0x0001 },
			{ 0x0003, 0x0002,     -1, 0x0002 },
			{ 0x0002, 0x0002,      1, 0x0003 },
			{ 0x0002, 0x0002,     -1, 0x0001 },
			{ 0x0000, 0x0002,      1, 0x0001 },
			{ 0x0000, 0x0002,     -1, 0x0002 },
			{ 0x0005, 0x0002,      1, 0x0006 },
			{ 0x0005, 0x0002,     -1, 0x0007 },
			{ 0x0007, 0x0002,      1, 0x0005 },
			{ 0x0007, 0x0002,     -1, 0x0006 },
			{ 0x0006, 0x0002,      1, 0x0007 },
			{ 0x0006, 0x0002,     -1, 0x0005 },
			{ 0x0004, 0x0002,      1, 0x0005 },
			{ 0x0004, 0x0002,     -1, 0x0006 },
			{ 0x2001, 0x0002,      1, 0x2002 },
			{ 0x2001, 0x0002,     -1, 0x2003 },
			{ 0x2003, 0x0002,      1, 0x2001 },
			{ 0x2003, 0x0002,     -1, 0x2002 },
			{ 0x2002, 0x0002,      1, 0x2003 },
			{ 0x2002, 0x0002,     -1, 0x2001 },
			{ 0x2000, 0x0002,      1, 0x2001 },
			{ 0x2000, 0x0002,     -1, 0x2002 },
			{ 0x0002, 0x0005,      1, 0x0003 },
			{ 0x0002, 0x0005,     -1, 0x0007 },
			{ 0x0007, 0x0005,      1, 0x0002 },
			{ 0x0007, 0x0005,     -1, 0x0006 },
			{ 0x0003, 0x0005,      1, 0x0004 },
			{ 0x0003, 0x0005,     -1, 0x0002 },
			{ 0x0000, 0x0005,      1, 0x0001 },
			{ 0x0000, 0x0005,     -1, 0x0005 },
			{ 0x000A, 0x0005,      1, 0x000B },
			{ 0x000A, 0x0005,     -1, 0x000F },
			{ 0x000F, 0x0005,      1, 0x000A },
			{ 0x000F, 0x0005,     -1, 0x000E },
			{ 0x000B, 0x0005,      1, 0x000C },
			{ 0x000B, 0x0005,     -1, 0x000A },
			{ 0x0008, 0x0005,      1, 0x0009 },
			{ 0x0008, 0x0005,     -1, 0x000D },
			{ 0x2002, 0x0005,      1, 0x2003 },
			{ 0x2002, 0x0005,     -1, 0x2007 },
			{ 0x2007, 0x0005,      1, 0x2002 },
			{ 0x2007, 0x0005,     -1, 0x2006 },
			{ 0x2003, 0x0005,      1, 0x2004 },
			{ 0x2003, 0x0005,     -1, 0x2002 },
			{ 0x2000, 0x0005,      1, 0x2001 },
			{ 0x2000, 0x0005,     -1, 0x2005 },
			{ 0x0001, 0x0006,      1, 0x0002 },
			{ 0x0001, 0x0006,     -1, 0x0007 },
			{ 0x0007, 0x0006,      1, 0x0001 },
			{ 0x0007, 0x0006,     -1, 0x0006 },
			{ 0x0002, 0x0006,      1, 0x0003 },
			{ 0x0002, 0x0006,     -1, 0x0001 },
			{ 0x0000, 0x0006,      1, 0x0001 },
			{ 0x0000, 0x0006,     -1, 0x0006 },
			{ 0x0009, 0x0006,      1, 0x000A },
			{ 0x0009, 0x0006,     -1, 0x000F },
			{ 0x000F, 0x0006,      1, 0x0009 },
			{ 0x000F, 0x0006,     -1, 0x000E },
			{ 0x000A, 0x0006,      1, 0x000B },
			{ 0x000A, 0x0006,     -1, 0x0009 },
			{ 0x0008, 0x0006,      1, 0x0009 },
			{ 0x0008, 0x0006,     -1, 0x000E },
			{ 0x2001, 0x0006,      1, 0x2002 },
			{ 0x2001, 0x0006,     -1, 0x2007 },
			{ 0x2007, 0x0006,      1, 0x2001 },
			{ 0x2007, 0x0006,     -1, 0x2006 },
			{ 0x2002, 0x0006,      1, 0x2003 },
			{ 0x2002, 0x0006,     -1, 0x2001 },
			{ 0x2000, 0x0006,      1, 0x2001 },
			{ 0x2000, 0x0006,     -1, 0x2006 },
			{ 0x0006, 0x0009,      1, 0x0007 },
			{ 0x0006, 0x0009,     -1, 0x000F },
			{ 0x000F, 0x0009,      1, 0x0006 },
			{ 0x000F, 0x0009,     -1, 0x000E },
			{ 0x0007, 0x0009,      1, 0x0008 },
			{ 0x0007, 0x0009,     -1, 0x0006 },
			{ 0x0000, 0x0009,      1, 0x0001 },
			{ 0x0000, 0x0009,     -1, 0x0009 },
			{ 0x0016, 0x0009,      1, 0x0017 },
			{ 0x0016, 0x0009,     -1, 0x001F },
			{ 0x001F, 0x0009,      1, 0x0016 },
			{ 0x001F, 0x0009,     -1, 0x001E },
			{ 0x0017, 0x0009,      1, 0x0018 },
			{ 0x0017, 0x0009,     -1, 0x0016 },
			{ 0x0010, 0x0009,      1, 0x0011 },
			{ 0x0010, 0x0009,     -1, 0x0019 },
			{ 0x2006, 0x0009,      1, 0x2007 },
			{ 0x2006, 0x0009,     -1, 0x200F },
			{ 0x200F, 0x0009,      1, 0x2006 },
			{ 0x200F, 0x0009,     -1, 0x200E },
			{ 0x2007, 0x0009,      1, 0x2008 },
			{ 0x2007, 0x0009,     -1, 0x2006 },
			{ 0x2000, 0x0009,      1, 0x2001 },
			{ 0x2000, 0x0009,     -1, 0x2009 },
			{ 0x0003, 0x000C,      1, 0x0004 },
			{ 0x0003, 0x000C,     -1, 0x000F },
			{ 0x000F, 0x000C,      1, 0x0003 },
			{ 0x000F, 0x000C,     -1, 0x000E },
			{ 0x0004, 0x000C,      1, 0x0005 },
			{ 0x0004, 0x000C,     -1, 0x0003 },
			{ 0x0000, 0x000C,      1, 0x0001 },
			{ 0x0000, 0x000C,     -1, 0x000C },
			{ 0x0013, 0x000C,      1, 0x0014 },
			{ 0x0013, 0x000C,     -1, 0x001F },
			{ 0x001F, 0x000C,      1, 0x0013 },
			{ 0x001F, 0x000C,     -1, 0x001E },
			{ 0x0014, 0x000C,      1, 0x0015 },
			{ 0x0014, 0x000C,     -1, 0x0013 },
			{ 0x0010, 0x000C,      1, 0x0011 },
			{ 0x0010, 0x000C,     -1, 0x001C },
			{ 0x2003, 0x000C,      1, 0x2004 },
			{ 0x2003, 0x000C,     -1, 0x200F },
			{ 0x200F, 0x000C,      1, 0x2003 },
			{ 0x200F, 0x000C,     -1, 0x200E },
			{ 0x2004, 0x000C,      1, 0x2005 },
			{ 0x2004, 0x000C,     -1, 0x2003 },
			{ 0x2000, 0x000C,      1, 0x2001 },
			{ 0x2000, 0x000C,     -1, 0x200C },
			{ 0x000E, 0x0011,      1, 0x000F },
			{ 0x000E, 0x0011,     -1, 0x001F },
			{ 0x001F, 0x0011,      1, 0x000E },
			{ 0x001F, 0x0011,     -1, 0x001E },
			{ 0x000F, 0x0011,      1, 0x0010 },
			{ 0x000F, 0x0011,     -1, 0x000E },
			{ 0x0000, 0x0011,      1, 0x0001 },
			{ 0x0000, 0x0011,     -1, 0x0011 },
			{ 0x002E, 0x0011,      1, 0x002F },
			{ 0x002E, 0x0011,     -1, 0x003F },
			{ 0x003F, 0x0011,      1, 0x002E },
			{ 0x003F, 0x0011,     -1, 0x003E },
			{ 0x002F, 0x0011,      1, 0x0030 },
			{ 0x002F, 0x0011,     -1, 0x002E },
			{ 0x0020, 0x0011,      1, 0x0021 },
			{ 0x0020, 0x0011,     -1, 0x0031 },
			{ 0x200E, 0x0011,      1, 0x200F },
			{ 0x200E, 0x0011,     -1, 0x201F },
			{ 0x201F, 0x0011,      1, 0x200E },
			{ 0x201F, 0x0011,     -1, 0x201E },
			{ 0x200F, 0x0011,      1, 0x2010 },
			{ 0x200F, 0x0011,     -1, 0x200E },
			{ 0x2000, 0x0011,      1, 0x2001 },
			{ 0x2000, 0x0011,     -1, 0x2011 },
			{ 0x0DCB, 0x1234,      1, 0x0DCC },
			{ 0x0DCB, 0x1234,     -1, 0x1FFF },
			{ 0x1FFF, 0x1234,      1, 0x0DCB },
			{ 0x1FFF, 0x1234,     -1, 0x1FFE },
			{ 0x0DCC, 0x1234,      1, 0x0DCD },
			{ 0x0DCC, 0x1234,     -1, 0x0DCB },
			{ 0x0000, 0x1234,      1, 0x0001 },
			{ 0x0000, 0x1234,     -1, 0x1234 },
			{ 0x2DCB, 0x1234,      1, 0x2DCC },
			{ 0x2DCB, 0x1234,     -1, 0x3FFF },
			{ 0x3FFF, 0x1234,      1, 0x2DCB },
			{ 0x3FFF, 0x1234,     -1, 0x3FFE },
			{ 0x2DCC, 0x1234,      1, 0x2DCD },
			{ 0x2DCC, 0x1234,     -1, 0x2DCB },
			{ 0x2000, 0x1234,      1, 0x2001 },
			{ 0x2000, 0x1234,     -1, 0x3234 },
			{ 0x0001, 0xFFFE,      1, 0x0002 },
			{ 0x0001, 0xFFFE,     -1, 0xFFFF },
			{ 0xFFFF, 0xFFFE,      1, 0x0001 },
			{ 0xFFFF, 0xFFFE,     -1, 0xFFFE },
			{ 0x0002, 0xFFFE,      1, 0x0003 },
			{ 0x0002, 0xFFFE,     -1, 0x0001 },
			{ 0x0000, 0xFFFE,      1, 0x0001 },
			{ 0x0000, 0xFFFE,     -1, 0xFFFE },
			{ 0x2000, 0xFFFE,      1, 0x2001 },
			{ 0x2000, 0xFFFE,     -1, 0x1FFF },
			{ 0x1FFF, 0xFFFE,      1, 0x2000 },
			{ 0x1FFF, 0xFFFE,     -1, 0x1FFE },
			{ 0x0140, 0xFFFF,    100, 0x01A4 },
			{ 0x0140, 0xFFFF,   -100, 0x00DC },
			{ 0xFFF0, 0xFFFF,     32, 0x0010 },
			{ 0x0010, 0xFFFF,    -32, 0xFFF0 },
			};

			for (const Case& c : golden)
			{
				m.core->regs.r[0] = c.r;
				m.core->regs.l[0] = c.l;
				m.core->regs.pc = 0;

				m.core->ArAdvance(0, c.m);

				wchar_t msg[160];
				swprintf_s(msg, _countof(msg),
					L"r=0x%04X l=0x%04X m=%d must give 0x%04X, got 0x%04X",
					c.r, c.l, c.m, c.expected, m.core->regs.r[0]);
				Assert::AreEqual(c.expected, m.core->regs.r[0], msg);
			}
		}

		TEST_METHOD(CircularAddressing_LengthFFFFIsLinear)
		{
			m.core->regs.l[0] = 0xFFFF;
			m.core->regs.r[0] = 0xFFF0;
			m.core->regs.m[0] = 0x0020;

			RunOne(Enc::Mr(REG_R0, MRM_PLUS_M0));
			Assert::AreEqual((uint16_t)0x0010, m.core->regs.r[0], L"l = 0xFFFF disables wrapping");

			m.core->regs.r[0] = 0x0004;
			RunOne(Enc::Mr(REG_R0, MRM_MINUS_1));
			Assert::AreEqual((uint16_t)0x0003, m.core->regs.r[0], L"linear addressing also steps down");
		}

		TEST_METHOD(CircularAddressing_ModifierZeroIsNop)
		{
			m.core->regs.l[0] = 6;
			m.core->regs.r[0] = 3;
			RunOne(Enc::Mr(REG_R0, MRM_PLUS_0));
			Assert::AreEqual((uint16_t)3, m.core->regs.r[0]);
		}

		TEST_METHOD(CircularAddressing_PowerOfTwoBufferWrapsAtItsBoundaries)
		{
			// l = 7 gives a buffer of eight entries filling a whole 8-aligned block. Stepping
			// forward off the top of the block comes back to the bottom of the same block, and
			// stepping below the bottom goes to the top.
			m.core->regs.l[0] = 7;
			m.core->regs.r[0] = 0x0107;

			RunOne(Enc::Mr(REG_R0, MRM_PLUS_1));
			Assert::AreEqual((uint16_t)0x0100, m.core->regs.r[0], L"++ past the block top wraps");

			RunOne(Enc::Mr(REG_R0, MRM_MINUS_1));
			Assert::AreEqual((uint16_t)0x0107, m.core->regs.r[0], L"-- below the block bottom wraps");
		}

		TEST_METHOD(CircularAddressing_MultiStepModifier)
		{
			m.core->regs.l[0] = 7;
			m.core->regs.r[0] = 0x0100;
			m.core->regs.m[0] = 5;

			RunOne(Enc::Mr(REG_R0, MRM_PLUS_M0));
			Assert::AreEqual((uint16_t)0x0105, m.core->regs.r[0]);

			m.core->regs.r[0] = 0x0106;
			RunOne(Enc::Mr(REG_R0, MRM_PLUS_M0));
			Assert::AreEqual((uint16_t)0x0103, m.core->regs.r[0], L"6 + 5 = 11 -> 11 - 8");

			m.core->regs.r[0] = 0x0101;
			m.core->regs.m[0] = (uint16_t)-3;
			RunOne(Enc::Mr(REG_R0, MRM_PLUS_M0));
			Assert::AreEqual((uint16_t)0x0106, m.core->regs.r[0], L"1 - 3 -> below the block, +8");
		}

		TEST_METHOD(CircularAddressing_HugeModifierDoesNotOverflowTheNegation)
		{
			// m = -32768 is the one modifier whose negation does not fit in 16 bits; the
			// address unit adds the complement with a carry-in of one instead. Only one block
			// correction is applied per update, so |m| <= l is a software responsibility.
			m.core->regs.l[0] = 0x7FFF;
			m.core->regs.r[0] = 0x0002;
			m.core->regs.m[0] = 0x8000;

			// 2 - 32768 + 32768 (one block correction) == 2
			RunOne(Enc::Mr(REG_R0, MRM_PLUS_M0));
			Assert::AreEqual((uint16_t)0x0002, m.core->regs.r[0]);
		}

		// ---------------------------------------------------------------
		// Register-file access
		// ---------------------------------------------------------------

		TEST_METHOD(RegisterFile_EveryReg32CodeIsAddressable)
		{
			// dsp-isa.md 3.1: the 32 register file codes are all valid `mv` destinations.
			for (uint16_t code = 0; code < 32; code++)
			{
				m.Reset();
				m.core->regs.a.l = 0x00A5;
				RunOne(Enc::Mv(code, REG_A0));
				Assert::AreEqual(0, DspTestHaltCount(), L"every reg32 code must be accepted by mv");
			}
		}

		TEST_METHOD(RegisterFile_MvBetweenHalves)
		{
			m.core->regs.a.m = 0xDEAD;
			RunOne(Enc::Mv(REG_B0, REG_A1));
			Assert::AreEqual((uint16_t)0xDEAD, m.core->regs.b.l);

			m.core->regs.a.l = 0xFEED;
			RunOne(Enc::Mv(REG_X1, REG_A0));
			Assert::AreEqual((uint16_t)0xFEED, m.core->regs.x.h);
		}

		TEST_METHOD(XlMode_StoreClampsToSixteenBits)
		{
			// dsp.md 2.3: in XL mode a store of a/b clamps the value to 0x7FFF / 0x8000
			// when the extension is in use.
			m.core->regs.psr.xl = 1;
			m.SetA(0x0001'0000'0000LL);			// E is in use
			RunOne(Enc::Mv(REG_A0, REG_A1));
			Assert::IsTrue(m.core->regs.a.l == 0x7FFF || m.core->regs.a.l == 0x8000,
				L"the XL-mode store must clamp");
			m.core->regs.psr.xl = 0;
		}
	};
}
