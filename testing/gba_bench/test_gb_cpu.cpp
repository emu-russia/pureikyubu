// The LR35902 interpreter: every instruction group, the flag rules and the interrupt behaviour.
//
// The tests hand the CPU a flat 64 KByte memory and hand-encoded instruction bytes, so the
// expected values are built here from the specification (the Pan Docs instruction tables and the
// RGBDS gbz80(7) flag rules) and never from the emulator's own answer. The instruction bytes are
// written out one at a time with the mnemonic in a comment, which is what makes the decoding
// checkable by reading the test.
//
// Timing is checked too, because the M-cycle column of the tables is a specification in its own
// right and the interrupt dispatch is what a frame's scheduling is built on.

#include "gba_test.h"
#include "gb_cpu.h"

#include <string>
#include <vector>

using namespace GBA;

namespace
{
	/// <summary>A flat 64 KByte memory for the CPU to execute out of.</summary>
	struct FlatBus : GbCpuBus
	{
		u8 memory[0x10000]{};

		u8 ReadByte(u16 address) override { return memory[address]; }
		void WriteByte(u16 address, u8 value) override { memory[address] = value; }
		bool InterruptsPending() const override { return (memory[0xFFFF] & memory[0xFF0F] & 0x1F) != 0; }
	};

	/// <summary>A CPU with its memory, loaded with a program at 0x0100.</summary>
	struct Machine
	{
		FlatBus bus;
		GbCpu cpu;

		Machine()
		{
			cpu.bus = &bus;
			cpu.Reset();
			cpu.pc = 0x0100;
			cpu.sp = 0xFFFE;
		}

		/// <summary>Load a program at 0x0100.</summary>
		void Load(const std::vector<u8>& code)
		{
			for (size_t i = 0; i < code.size(); i++)
				bus.memory[0x0100 + i] = code[i];
		}

		/// <summary>Run one instruction and return its M-cycle count.</summary>
		int Step() { return cpu.Step(); }

		/// <summary>Run `count` instructions.</summary>
		void Steps(int count)
		{
			for (int i = 0; i < count; i++)
				cpu.Step();
		}
	};

	/// <summary>The flag bits as the tests spell them (Z N H C).</summary>
	std::string Flags(u8 f)
	{
		std::string text;
		text += (f & GbFlagZ) ? 'Z' : '-';
		text += (f & GbFlagN) ? 'N' : '-';
		text += (f & GbFlagH) ? 'H' : '-';
		text += (f & GbFlagC) ? 'C' : '-';
		return text;
	}

	/// <summary>Check AF against the expected A and flags, with a readable failure.</summary>
	void CheckAF(const GbCpu& cpu, u8 a, const char* flags)
	{
		std::string actual = Flags(cpu.f);
		GBA_CHECK_MSG(cpu.a == a && actual == flags,
			"A=" + GbaTest::Hex(cpu.a) + " flags=" + actual + ", expected A=" + GbaTest::Hex(a)
			+ " flags=" + flags);
	}

	// The opcodes the tests use most, spelled out so the programs read like the tables.
	const u8 OpNop = 0x00;
	const u8 OpLdBCn = 0x01;
	const u8 OpLdSPn = 0x31;
	const u8 OpIncBC = 0x03;
	const u8 OpDecBC = 0x0B;
	const u8 OpAddHLBC = 0x09;
	const u8 OpLdA16 = 0xFA;
	const u8 OpLd16A = 0xEA;
	const u8 OpLdAHLI = 0x22;
	const u8 OpLdAHLA = 0x2A;
	const u8 OpDaa = 0x27;
	const u8 OpCpl = 0x2F;
	const u8 OpScf = 0x37;
	const u8 OpCcf = 0x3F;
	const u8 OpHalt = 0x76;
	const u8 OpStop = 0x10;
	const u8 OpDi = 0xF3;
	const u8 OpEi = 0xFB;
	const u8 OpRet = 0xC9;
	const u8 OpReti = 0xD9;
	const u8 OpPushAF = 0xF5;
	const u8 OpPopAF = 0xF1;
	const u8 OpCb = 0xCB;
	const u8 OpCall = 0xCD;
	const u8 OpRst38 = 0xFF;
	const u8 OpJrNz = 0x20;
	const u8 OpJp = 0xC3;
	const u8 OpLdhA = 0xF0;
	const u8 OpLdhAn = 0xE0;
	const u8 OpAddSPe = 0xE8;
	const u8 OpLdHLSPe = 0xF8;
}

// ---------------------------------------------------------------------------------------
// The 8-bit arithmetic and logic family (Pan Docs "Block 2" and "Block 3")
// ---------------------------------------------------------------------------------------

GBA_TEST(GbCpu, add_and_adc_flags)
{
	Machine m;
	m.Load({ 0x3E, 0x0F, 0xC6, 0x01 });		// LD A,0x0F ; ADD A,0x01
	m.Step();
	m.Step();
	CheckAF(m.cpu, 0x10, "--H-");

	m.Load({ 0x3E, 0xFF, 0xC6, 0x01 });		// LD A,0xFF ; ADD A,0x01
	m.cpu.pc = 0x0100;
	m.Step();
	m.Step();
	CheckAF(m.cpu, 0x00, "Z-HC");

	m.Load({ 0x3E, 0x7F, 0xCE, 0x00 });		// LD A,0x7F ; ADC A,0x00 with C clear
	m.cpu.pc = 0x0100;
	m.cpu.f = GbFlagC;						// carry in
	m.Step();
	m.Step();
	CheckAF(m.cpu, 0x80, "--H-");
}

GBA_TEST(GbCpu, sub_sbc_and_cp)
{
	Machine m;
	m.Load({ 0x3E, 0x10, 0xD6, 0x01 });		// LD A,0x10 ; SUB 0x01
	m.Step();
	m.Step();
	CheckAF(m.cpu, 0x0F, "-NH-");

	m.Load({ 0x3E, 0x00, 0xD6, 0x01 });		// LD A,0x00 ; SUB 0x01
	m.cpu.pc = 0x0100;
	m.Step();
	m.Step();
	CheckAF(m.cpu, 0xFF, "-NHC");

	// CP sets the same flags as SUB but stores nothing (gbz80(7) "CP A,r8").
	m.Load({ 0x3E, 0x3C, 0xFE, 0x3C });		// LD A,0x3C ; CP 0x3C
	m.cpu.pc = 0x0100;
	m.Step();
	m.Step();
	CheckAF(m.cpu, 0x3C, "ZN--");

	// SBC with the carry in: 0x00 - 0x00 - 1 = 0xFF with borrow.
	m.Load({ 0x3E, 0x00, 0xDE, 0x00 });		// LD A,0x00 ; SBC A,0x00
	m.cpu.pc = 0x0100;
	m.cpu.f = GbFlagC;
	m.Step();
	m.Step();
	CheckAF(m.cpu, 0xFF, "-NHC");
}

GBA_TEST(GbCpu, logic_family_flags)
{
	Machine m;

	// AND clears N and C and sets H (gbz80(7) "AND A,r8").
	m.Load({ 0x3E, 0xFF, 0xE6, 0x0F });		// LD A,0xFF ; AND 0x0F
	m.Step();
	m.Step();
	CheckAF(m.cpu, 0x0F, "--H-");

	// XOR clears Z, N, H and C unless the result is zero.
	m.Load({ 0x3E, 0xF0, 0xEE, 0xF0 });		// LD A,0xF0 ; XOR 0xF0
	m.cpu.pc = 0x0100;
	m.Step();
	m.Step();
	CheckAF(m.cpu, 0x00, "Z---");

	// OR clears N, H and C.
	m.Load({ 0x3E, 0x00, 0xF6, 0x00 });		// LD A,0x00 ; OR 0x00
	m.cpu.pc = 0x0100;
	m.Step();
	m.Step();
	CheckAF(m.cpu, 0x00, "Z---");
}

GBA_TEST(GbCpu, inc_dec_leave_carry_alone)
{
	// INC r8 and DEC r8 affect Z, N and H but never C (gbz80(7)).
	Machine m;
	m.Load({ 0x3E, 0x0F, 0x3C });			// LD A,0x0F ; INC A
	m.cpu.f = GbFlagC;
	m.Step();
	m.Step();
	CheckAF(m.cpu, 0x10, "--HC");

	m.Load({ 0x3E, 0x00, 0x3D });			// LD A,0x00 ; DEC A
	m.cpu.pc = 0x0100;
	m.cpu.f = GbFlagC;
	m.Step();
	m.Step();
	CheckAF(m.cpu, 0xFF, "-NHC");
}

GBA_TEST(GbCpu, add_hl_r16_uses_bit_11_for_half_carry)
{
	// ADD HL,r16 sets H from bit 11 and C from bit 15 and leaves Z alone (gbz80(7)).
	Machine m;
	m.Load({ 0x01, 0xFF, 0x0F, 0x21, 0x01, 0x00, 0x09 });	// LD BC,0x0FFF ; LD HL,0x0001 ; ADD HL,BC
	m.Step();
	m.Step();
	m.cpu.f = GbFlagZ;
	m.Step();
	GBA_CHECK_EQ(m.cpu.HL(), 0x1000);
	GBA_CHECK_EQ(m.cpu.f, (u8)(GbFlagZ | GbFlagH));
}

GBA_TEST(GbCpu, add_sp_e8_flags_come_from_the_low_byte)
{
	// ADD SP,e8 sets H from bit 3 and C from bit 7 *of the low byte* (gbz80(7)): 0xFFF0 + 0x11
	// carries out of bit 7 of the low byte (0xF0 + 0x11 > 0xFF) even though the 16-bit addition
	// does not carry at all.
	Machine m;
	m.Load({ 0xE8, 0x11 });					// ADD SP,0x11
	m.cpu.sp = 0xFFF0;
	m.cpu.f = GbFlagZ | GbFlagN;
	m.Step();
	GBA_CHECK_EQ(m.cpu.sp, 0x0001);
	GBA_CHECK_EQ(m.cpu.f, (u8)GbFlagC);

	// A low nibble sum above 0x0F sets H: 0xFF0F + 0x01.
	m.Load({ 0xE8, 0x01 });					// ADD SP,0x01
	m.cpu.pc = 0x0100;
	m.cpu.sp = 0xFF0F;
	m.cpu.f = GbFlagZ;
	m.Step();
	GBA_CHECK_EQ(m.cpu.sp, 0xFF10);
	GBA_CHECK_EQ(m.cpu.f, (u8)GbFlagH);

	// 0xFFF0 + 0x0F: neither flag.
	m.Load({ 0xE8, 0x0F });					// ADD SP,0x0F
	m.cpu.pc = 0x0100;
	m.cpu.sp = 0xFFF0;
	m.cpu.f = GbFlagZ;
	m.Step();
	GBA_CHECK_EQ(m.cpu.sp, 0xFFFF);
	GBA_CHECK_EQ(m.cpu.f, 0x00);
}

GBA_TEST(GbCpu, ld_hl_sp_plus_e8)
{
	// LD HL,SP+e8 has the same flag rule as ADD SP,e8 (gbz80(7)) and adds the offset signed.
	Machine m;
	m.Load({ 0xF8, 0x02 });					// LD HL,SP+0x02
	m.cpu.sp = 0xFFF0;
	m.Step();
	GBA_CHECK_EQ(m.cpu.HL(), 0xFFF2);
	GBA_CHECK_EQ(m.cpu.f, 0x00);

	m.Load({ 0xF8, 0xFF });					// LD HL,SP+0xFF (-1)
	m.cpu.pc = 0x0100;
	m.cpu.sp = 0x8000;
	m.Step();
	GBA_CHECK_EQ(m.cpu.HL(), 0x7FFF);
	GBA_CHECK_EQ(m.cpu.f, 0x00);			// 0x00 + 0xFF carries out of neither bit 3 nor bit 7

	m.Load({ 0xF8, 0x01 });					// LD HL,SP+0x01 with a half carry
	m.cpu.pc = 0x0100;
	m.cpu.sp = 0x800F;
	m.Step();
	GBA_CHECK_EQ(m.cpu.HL(), 0x8010);
	GBA_CHECK_EQ(m.cpu.f, (u8)GbFlagH);
}

GBA_TEST(GbCpu, daa_all_four_nh_combinations)
{
	// DAA corrects A after a BCD addition or subtraction (Pan Docs "CPU Registers and Flags").
	// The four combinations of N and H, plus the carry cases, are the whole specification.
	struct Case
	{
		u8 a, f, result, flags;
	};

	// N = 0 (an addition): a digit above 9 or a half carry adds 6 to the low digit, and a carry
	// or a value above 0x99 adds 0x60 to the high one.
	const Case additions[] =
	{
		{ 0x09, 0x00, 0x09, 0x00 },			// 9 -> 9 (no correction)
		{ 0x0A, 0x00, 0x10, 0x00 },			// 10 -> 10 (low digit correction)
		{ 0x0F, GbFlagH, 0x15, 0x00 },		// half carry: 0x0F + 6
		{ 0x99, 0x00, 0x99, 0x00 },			// 0x99 -> 0x99 (nothing to correct)
		{ 0x9A, 0x00, 0x00, GbFlagZ | GbFlagC },	// 0x9A: high digit correction, wraps
		{ 0x00, GbFlagC, 0x60, GbFlagC },	// carry set: add 0x60
	};

	for (const Case& c : additions)
	{
		Machine m;
		m.Load({ OpDaa });
		m.cpu.a = c.a;
		m.cpu.f = c.f;
		m.Step();
		GBA_CHECK_MSG(m.cpu.a == c.result, "A=" + GbaTest::Hex(m.cpu.a) + " expected "
			+ GbaTest::Hex(c.result) + " for A=" + GbaTest::Hex(c.a) + " F=" + Flags(c.f));
		GBA_CHECK_MSG((m.cpu.f & GbFlagH) == 0, "DAA always clears H");
		GBA_CHECK_MSG(((m.cpu.f & GbFlagC) != 0) == ((c.flags & GbFlagC) != 0),
			"the carry is " + std::string((m.cpu.f & GbFlagC) ? "set" : "clear"));
		GBA_CHECK_MSG(((m.cpu.f & GbFlagZ) != 0) == ((c.flags & GbFlagZ) != 0),
			"the zero flag is " + std::string((m.cpu.f & GbFlagZ) ? "set" : "clear"));
	}

	// N = 1 (a subtraction): the correction is subtracted, and H is always cleared.
	const Case subtractions[] =
	{
		{ 0x00, GbFlagN, 0x00, GbFlagZ },			// the low digit is already BCD
		{ 0x05, GbFlagN | GbFlagH, 0xFF, GbFlagN },	// 0x05 - 6 = 0xFF (no Z, N stays set)
		{ 0x00, GbFlagN | GbFlagC, 0xA0, GbFlagN | GbFlagC },
		{ 0x60, GbFlagN | GbFlagC | GbFlagH, 0xFA, GbFlagN | GbFlagC },	// 0x60 - 0x66
	};

	for (const Case& c : subtractions)
	{
		Machine m;
		m.Load({ OpDaa });
		m.cpu.a = c.a;
		m.cpu.f = c.f;
		m.Step();
		// The N flag is always set after a DAA that follows a subtraction.
		GBA_CHECK_MSG(m.cpu.a == c.result, "A=" + GbaTest::Hex(m.cpu.a) + " expected "
			+ GbaTest::Hex(c.result));
		GBA_CHECK_MSG((m.cpu.f & GbFlagN) != 0, "the N flag must survive DAA");
		GBA_CHECK_MSG((m.cpu.f & GbFlagH) == 0, "DAA always clears H");
		GBA_CHECK_MSG(((m.cpu.f & GbFlagC) != 0) == ((c.flags & GbFlagC) != 0),
			"the carry is " + std::string((m.cpu.f & GbFlagC) ? "set" : "clear"));
	}
}

GBA_TEST(GbCpu, cpl_scf_ccf_and_rotates)
{
	Machine m;

	m.Load({ 0x3E, 0x55, OpCpl });			// LD A,0x55 ; CPL
	m.cpu.f = GbFlagZ | GbFlagC;
	m.Step();
	m.Step();
	CheckAF(m.cpu, 0xAA, "ZNHC");			// CPL sets N and H, leaves Z and C

	m.Load({ OpScf });						// SCF
	m.cpu.pc = 0x0100;
	m.cpu.f = GbFlagZ | GbFlagN | GbFlagH;
	m.Step();
	GBA_CHECK_EQ(m.cpu.f, (u8)(GbFlagZ | GbFlagC));

	m.Load({ OpCcf });						// CCF with C set
	m.cpu.pc = 0x0100;
	m.cpu.f = GbFlagC;
	m.Step();
	GBA_CHECK_EQ(m.cpu.f, 0x00);

	// The accumulator rotates never touch Z (gbz80(7) "RLCA" and friends).
	m.Load({ 0x3E, 0x80, 0x07 });			// LD A,0x80 ; RLCA
	m.cpu.pc = 0x0100;
	m.Step();
	m.Step();
	CheckAF(m.cpu, 0x01, "---C");

	m.Load({ 0x3E, 0x00, 0x07 });			// LD A,0x00 ; RLCA
	m.cpu.pc = 0x0100;
	m.Step();
	m.Step();
	CheckAF(m.cpu, 0x00, "----");			// Z is *not* set by RLCA
}

// ---------------------------------------------------------------------------------------
// Loads, the stack and the 16-bit arithmetic
// ---------------------------------------------------------------------------------------

GBA_TEST(GbCpu, loads_and_the_hl_pointer_family)
{
	Machine m;
	m.Load({ 0x3E, 0x42, 0x21, 0x00, 0xC0, OpLdAHLI, OpLdAHLI });	// LD A,0x42 ; LD HL,0xC000 ; LD (HL+),A x2
	m.Step();
	m.Step();
	m.Step();
	m.Step();
	GBA_CHECK_EQ(m.bus.memory[0xC000], 0x42);
	GBA_CHECK_EQ(m.bus.memory[0xC001], 0x42);
	GBA_CHECK_EQ(m.cpu.HL(), 0xC002);

	m.Load({ 0x21, 0x01, 0xC0, OpLdAHLA });	// LD HL,0xC001 ; LD A,(HL+)
	m.cpu.pc = 0x0100;
	m.Step();
	m.Step();
	GBA_CHECK_EQ(m.cpu.a, 0x42);
	GBA_CHECK_EQ(m.cpu.HL(), 0xC002);

	// LD (n16),A and LD A,(n16) are the big-endian-free little-endian forms.
	m.Load({ 0x3E, 0x7E, OpLd16A, 0x34, 0x12, 0x3E, 0x00, OpLdA16, 0x34, 0x12 });
	m.cpu.pc = 0x0100;
	m.Steps(5);
	GBA_CHECK_EQ(m.bus.memory[0x1234], 0x7E);
	GBA_CHECK_EQ(m.cpu.a, 0x7E);
}

GBA_TEST(GbCpu, ldh_and_the_c_forms)
{
	Machine m;
	m.Load({ 0x3E, 0x99, OpLdhAn, 0x40 });	// LD A,0x99 ; LDH (0x40),A
	m.Step();
	m.Step();
	GBA_CHECK_EQ(m.bus.memory[0xFF40], 0x99);

	m.Load({ OpLdhA, 0x40 });				// LDH A,(0x40)
	m.cpu.pc = 0x0100;
	m.Step();
	GBA_CHECK_EQ(m.cpu.a, 0x99);

	m.Load({ 0x0E, 0x80, 0x3E, 0x11, 0xE2 });	// LD C,0x80 ; LD A,0x11 ; LD (C),A
	m.cpu.pc = 0x0100;
	m.Steps(3);
	GBA_CHECK_EQ(m.bus.memory[0xFF80], 0x11);
}

GBA_TEST(GbCpu, the_stack_and_the_call_return_family)
{
	Machine m;
	// CALL 0x0200 pushes the return address; RET pops it.
	m.Load({ OpCall, 0x00, 0x02 });
	m.bus.memory[0x0200] = OpRet;
	m.Step();
	GBA_CHECK_EQ(m.cpu.pc, 0x0200);
	GBA_CHECK_EQ(m.cpu.sp, 0xFFFC);
	GBA_CHECK_EQ(m.bus.memory[0xFFFC], 0x03);		// the low byte of the return address
	GBA_CHECK_EQ(m.bus.memory[0xFFFD], 0x01);
	m.Step();
	GBA_CHECK_EQ(m.cpu.pc, 0x0103);

	// PUSH/POP AF keeps the flags; the low nibble of F reads back as zero.
	m.Load({ OpPushAF, 0x3E, 0x00, OpPopAF });
	m.cpu.pc = 0x0100;
	m.cpu.a = 0xAB;
	m.cpu.f = 0xFF;
	m.Step();
	m.Step();
	m.Step();
	GBA_CHECK_EQ(m.cpu.a, 0xAB);
	GBA_CHECK_EQ(m.cpu.f, 0xF0);

	// RST 38h pushes and jumps into the vector table (gbz80(7) "RST vec").
	m.Load({ OpRst38 });
	m.cpu.pc = 0x0100;
	m.cpu.sp = 0xFFFE;
	m.Step();
	GBA_CHECK_EQ(m.cpu.pc, 0x0038);
	GBA_CHECK_EQ(m.cpu.sp, 0xFFFC);
}

GBA_TEST(GbCpu, conditional_branches_test_the_right_flag)
{
	// The condition field is NZ, Z, NC, C: a decoder that tests Z for the carry conditions is a
	// classic bug (it makes "JR C" behave like "JR Z"), so both halves are checked here.
	Machine m;

	m.Load({ 0x38, 0x02 });					// JR C,+2
	m.cpu.f = GbFlagC;
	m.Step();
	GBA_CHECK_EQ(m.cpu.pc, 0x0104);

	m.Load({ 0x38, 0x02 });					// JR C,+2 with C clear: not taken
	m.cpu.pc = 0x0100;
	m.cpu.f = GbFlagZ;						// Z set, C clear
	m.Step();
	GBA_CHECK_EQ(m.cpu.pc, 0x0102);

	m.Load({ 0x30, 0x02 });					// JR NC,+2 with C clear: taken
	m.cpu.pc = 0x0100;
	m.cpu.f = 0x00;
	m.Step();
	GBA_CHECK_EQ(m.cpu.pc, 0x0104);

	m.Load({ OpJrNz, 0x02 });				// JR NZ,+2 with Z set: not taken
	m.cpu.pc = 0x0100;
	m.cpu.f = GbFlagZ;
	m.Step();
	GBA_CHECK_EQ(m.cpu.pc, 0x0102);

	// JP C,n16 and CALL C,n16.
	m.Load({ 0xDA, 0x00, 0x02 });			// JP C,0x0200
	m.cpu.pc = 0x0100;
	m.cpu.f = GbFlagC;
	m.Step();
	GBA_CHECK_EQ(m.cpu.pc, 0x0200);

	m.Load({ 0xDC, 0x00, 0x02 });			// CALL C,0x0200 with C clear: not taken
	m.cpu.pc = 0x0100;
	m.cpu.sp = 0xFFFE;
	m.cpu.f = 0x00;
	m.Step();
	GBA_CHECK_EQ(m.cpu.pc, 0x0103);
	GBA_CHECK_EQ(m.cpu.sp, 0xFFFE);

	// RET C.
	m.Load({ 0xD8 });						// RET C
	m.cpu.pc = 0x0100;
	m.cpu.sp = 0xFFFC;
	m.bus.memory[0xFFFC] = 0x34;
	m.bus.memory[0xFFFD] = 0x12;
	m.cpu.f = GbFlagC;
	m.Step();
	GBA_CHECK_EQ(m.cpu.pc, 0x1234);
}

// ---------------------------------------------------------------------------------------
// The CB prefix
// ---------------------------------------------------------------------------------------

GBA_TEST(GbCpu, cb_shifts_and_rotates)
{
	Machine m;

	// RLC A: 0x80 rotates to 0x01 with the carry set and Z clear (gbz80(7) "RLC r8").
	m.Load({ 0x3E, 0x80, OpCb, 0x07 });
	m.cpu.pc = 0x0100;
	m.Step();
	m.Step();
	CheckAF(m.cpu, 0x01, "---C");

	// RLC A with 0x00 sets Z (unlike the RLCA instruction).
	m.Load({ 0x3E, 0x00, OpCb, 0x07 });
	m.cpu.pc = 0x0100;
	m.Step();
	m.Step();
	CheckAF(m.cpu, 0x00, "Z---");

	// SRA keeps bit 7: 0x80 -> 0xC0.
	m.Load({ 0x3E, 0x80, OpCb, 0x2F });
	m.cpu.pc = 0x0100;
	m.Step();
	m.Step();
	CheckAF(m.cpu, 0xC0, "----");

	// SRL shifts a zero in: 0x01 -> 0x00 with C set.
	m.Load({ 0x3E, 0x01, OpCb, 0x3F });
	m.cpu.pc = 0x0100;
	m.Step();
	m.Step();
	CheckAF(m.cpu, 0x00, "Z--C");

	// SWAP exchanges the nibbles and clears every flag but Z.
	m.Load({ 0x3E, 0xAB, OpCb, 0x37 });
	m.cpu.pc = 0x0100;
	m.cpu.f = GbFlagC;
	m.Step();
	m.Step();
	CheckAF(m.cpu, 0xBA, "----");

	// RL through the carry: 0x00 with C set becomes 0x01, and the old bit 7 goes to C.
	m.Load({ 0x3E, 0x00, OpCb, 0x17 });
	m.cpu.pc = 0x0100;
	m.cpu.f = GbFlagC;
	m.Step();
	m.Step();
	CheckAF(m.cpu, 0x01, "----");
}

GBA_TEST(GbCpu, cb_bit_res_set)
{
	Machine m;

	// BIT leaves C alone and sets H (gbz80(7) "BIT u3,r8").
	m.Load({ 0x3E, 0x00, OpCb, 0x47 });		// LD A,0x00 ; BIT 0,A
	m.cpu.pc = 0x0100;
	m.cpu.f = GbFlagC;
	m.Step();
	m.Step();
	CheckAF(m.cpu, 0x00, "Z-HC");

	m.Load({ 0x3E, 0x01, OpCb, 0x47 });		// LD A,0x01 ; BIT 0,A
	m.cpu.pc = 0x0100;
	m.cpu.f = GbFlagC;
	m.Step();
	m.Step();
	CheckAF(m.cpu, 0x01, "--HC");

	// RES and SET do not touch the flags at all.
	m.Load({ 0x3E, 0xFF, OpCb, 0x87 });		// LD A,0xFF ; RES 0,A
	m.cpu.pc = 0x0100;
	m.cpu.f = GbFlagZ | GbFlagN | GbFlagH | GbFlagC;
	m.Step();
	m.Step();
	GBA_CHECK_EQ(m.cpu.a, 0xFE);
	GBA_CHECK_EQ(m.cpu.f, (u8)(GbFlagZ | GbFlagN | GbFlagH | GbFlagC));

	m.Load({ 0x3E, 0x00, OpCb, 0xFF });		// LD A,0x00 ; SET 7,A
	m.cpu.pc = 0x0100;
	m.cpu.f = GbFlagZ;
	m.Step();
	m.Step();
	CheckAF(m.cpu, 0x80, "Z---");

	// The (HL) forms work through memory and take the extra M-cycle.
	m.Load({ 0x21, 0x00, 0xC0, OpCb, 0x86 });	// LD HL,0xC000 ; RES 0,(HL)
	m.bus.memory[0xC000] = 0xFF;
	m.cpu.pc = 0x0100;
	m.Step();
	m.Step();
	GBA_CHECK_EQ(m.bus.memory[0xC000], 0xFE);
}

// ---------------------------------------------------------------------------------------
// Interrupts, HALT and STOP
// ---------------------------------------------------------------------------------------

GBA_TEST(GbCpu, interrupt_round_trip_with_the_ei_delay)
{
	Machine m;

	// IE and IF both have VBlank set, so the interrupt is ready to be serviced.
	m.bus.memory[0xFFFF] = 0x01;
	m.bus.memory[0xFF0F] = 0x01;

	m.Load({ OpEi, OpNop, OpRet });			// EI ; NOP ; RET
	m.bus.memory[0x0040] = OpReti;			// the VBlank vector returns with IME set

	// EI's effect is delayed by one instruction (Pan Docs "Interrupts"), so the NOP runs before
	// the interrupt can be taken: after EI, IME is still clear.
	GBA_CHECK_EQ(m.Step(), 1);
	GBA_CHECK_MSG(!m.cpu.ime, "IME must still be clear right after EI");

	// The NOP runs, and only then does IME become set; the next Step() services the interrupt.
	GBA_CHECK_EQ(m.Step(), 1);
	GBA_CHECK_MSG(m.cpu.ime, "IME must be set after the instruction following EI");
	GBA_CHECK_EQ(m.Step(), 5);				// the dispatch costs five M-cycles
	GBA_CHECK_EQ(m.cpu.pc, 0x0040);
	GBA_CHECK_MSG(!m.cpu.ime, "the dispatch clears IME");
	GBA_CHECK_EQ(m.bus.memory[0xFF0F] & 0x01, 0x00);	// the IF bit is acknowledged

	// RETI returns and enables interrupts at once.
	m.Step();
	GBA_CHECK_EQ(m.cpu.pc, 0x0102);
	GBA_CHECK_MSG(m.cpu.ime, "RETI sets IME immediately");
}

GBA_TEST(GbCpu, interrupt_priority_is_bit_order)
{
	Machine m;
	m.bus.memory[0xFFFF] = 0x1F;			// every interrupt enabled
	m.bus.memory[0xFF0F] = 0x1F;			// every interrupt requested

	m.Load({ OpNop });
	m.cpu.ime = true;
	m.Step();

	// VBlank (bit 0) has the highest priority (Pan Docs "Interrupts").
	GBA_CHECK_EQ(m.cpu.pc, 0x0040);
	GBA_CHECK_EQ(m.bus.memory[0xFF0F], 0x1E);
}

GBA_TEST(GbCpu, halt_wakes_without_servicing_when_ime_is_clear)
{
	Machine m;
	m.bus.memory[0xFFFF] = 0x01;
	m.bus.memory[0xFF0F] = 0x00;

	m.Load({ OpHalt, OpNop });
	m.Step();
	GBA_CHECK_MSG(m.cpu.halted, "HALT stops the CPU");

	// With no interrupt pending the CPU does nothing and PC stays put.
	GBA_CHECK_EQ(m.Step(), 1);
	GBA_CHECK_EQ(m.cpu.pc, 0x0101);

	// An interrupt becoming pending wakes it, but with IME = 0 the handler is not called: the CPU
	// simply carries on with the instruction after the HALT.
	m.bus.memory[0xFF0F] = 0x01;
	m.Step();
	GBA_CHECK_MSG(!m.cpu.halted, "the CPU wakes when an interrupt is pending");
	GBA_CHECK_EQ(m.cpu.pc, 0x0102);
	GBA_CHECK_MSG(!m.cpu.ime, "no handler is called with IME clear");
}

GBA_TEST(GbCpu, halt_bug_reads_the_next_byte_twice)
{
	// HALT with IME = 0 and an interrupt already pending ends immediately, and PC is not advanced
	// past the HALT ("halt"): the byte after the HALT is read a second time, so the instruction
	// there executes twice.
	Machine m;
	m.bus.memory[0xFFFF] = 0x01;
	m.bus.memory[0xFF0F] = 0x01;

	// The program: HALT ; INC A ; INC A. Without the bug the two INCs would leave A = 2.
	m.Load({ OpHalt, 0x3C, 0x3C });
	m.Step();
	GBA_CHECK_MSG(!m.cpu.halted, "the bugged HALT does not stay halted");
	GBA_CHECK_EQ(m.cpu.pc, 0x0101);			// PC points at the byte after the HALT

	// The first INC A executes without PC advancing...
	m.Step();
	GBA_CHECK_EQ(m.cpu.a, 1);
	GBA_CHECK_EQ(m.cpu.pc, 0x0101);

	// ... so the same byte is fetched again and the instruction runs a second time.
	m.Step();
	GBA_CHECK_EQ(m.cpu.a, 2);
	GBA_CHECK_EQ(m.cpu.pc, 0x0102);

	// And then execution continues normally.
	m.Step();
	GBA_CHECK_EQ(m.cpu.a, 3);
	GBA_CHECK_EQ(m.cpu.pc, 0x0103);
}

GBA_TEST(GbCpu, stop_asks_the_machine_and_can_end_at_once)
{
	// STOP hands the decision to the machine (a CGB speed switch or the DMG's standby); when the
	// hook says "no switch", the CPU waits in STOP until the machine wakes it.
	static bool switchNow = false;
	static int calls = 0;
	switchNow = false;
	calls = 0;

	Machine m;
	m.cpu.OnStop = [](void*) -> bool { calls++; return switchNow; };
	m.cpu.stopUser = nullptr;

	m.Load({ OpStop, 0x00, OpNop });		// STOP (the second byte is part of the instruction)
	m.Step();
	GBA_CHECK_EQ(calls, 1);
	GBA_CHECK_MSG(m.cpu.stopped, "STOP waits when the machine does not switch");
	GBA_CHECK_EQ(m.cpu.pc, 0x0102);			// the second byte was consumed

	// Waking up (the machine does it when a button is pressed) lets the CPU run again.
	m.cpu.stopped = false;
	m.Step();
	GBA_CHECK_EQ(m.cpu.pc, 0x0103);

	// With the hook reporting a speed switch, STOP ends immediately.
	switchNow = true;
	m.Load({ OpStop, 0x00, OpNop });
	m.cpu.pc = 0x0100;
	m.cpu.stopped = false;
	m.Step();
	GBA_CHECK_MSG(!m.cpu.stopped, "a speed switch ends STOP at once");
	GBA_CHECK_EQ(m.cpu.pc, 0x0102);
}

// ---------------------------------------------------------------------------------------
// Timing and the emitter
// ---------------------------------------------------------------------------------------

GBA_TEST(GbCpu, instruction_timings_match_the_tables)
{
	// A few of the M-cycle costs from the RGBDS gbz80(7) "Cycles" column.
	Machine m;
	m.Load({ OpNop });
	GBA_CHECK_EQ(m.Step(), 1);				// NOP

	m.Load({ 0x01, 0x00, 0x00 });			// LD BC,n16
	m.cpu.pc = 0x0100;
	GBA_CHECK_EQ(m.Step(), 3);

	m.Load({ 0x21, 0x00, 0xC0, 0x7E });		// LD HL,0xC000 ; LD A,(HL)
	m.cpu.pc = 0x0100;
	m.Step();
	GBA_CHECK_EQ(m.Step(), 2);

	m.Load({ 0x21, 0x00, 0xC0, 0x36, 0x00 });	// LD HL,0xC000 ; LD (HL),n8
	m.cpu.pc = 0x0100;
	m.Step();
	GBA_CHECK_EQ(m.Step(), 3);

	m.Load({ OpCb, 0x06 });					// RLC (HL)
	m.cpu.pc = 0x0100;
	GBA_CHECK_EQ(m.Step(), 4);

	m.Load({ OpCall, 0x00, 0x02 });			// CALL n16
	m.cpu.pc = 0x0100;
	GBA_CHECK_EQ(m.Step(), 6);

	m.Load({ 0xC0 });						// RET NZ (not taken)
	m.cpu.pc = 0x0100;
	m.cpu.f = GbFlagZ;
	GBA_CHECK_EQ(m.Step(), 2);

	m.Load({ 0xC0 });						// RET NZ (taken)
	m.cpu.pc = 0x0100;
	m.cpu.f = 0x00;
	m.cpu.sp = 0xFFFC;
	m.bus.memory[0xFFFC] = 0x00;
	m.bus.memory[0xFFFD] = 0x02;
	GBA_CHECK_EQ(m.Step(), 5);
}
