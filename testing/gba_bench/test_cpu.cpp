// Unit tests for the ARM7TDMI interpreter.
//
// The tests drive the core the way the debugger and the harness do: a program is written into
// EWRAM (or into a small BIOS image for the exception vectors) through GbaBus, the PC is set and
// the CPU is single stepped through the public API. Nothing here calls into the CPU to work out
// what an instruction should do: every instruction word is assembled by the little encoders
// below (plain bit twiddling, straight from the encodings in ARM DDI 0100E chapters 3 and 4) and
// every expected value and flag is written out by hand from the rules in the same document. That
// is the point of the suite - if the CPU and the test agree, they agreed because both were taken
// from the specification, not because one asked the other.
//
// The cycle counts the tests check come from the ARM7TDMI data sheet's S/N/I table (N, S and I
// cycles are one clock each there): MOV 1S, LDR 1S+1N+1I, STR 2N, LDM n regs 1S+1N+(n-1)S+1I,
// STM 2N+(n-1)S, B/BL/SWI 2S+1N, and the multiply's 1S+mI.

#include "gba_test.h"
#include "arm7tdmi.h"
#include "gba_bus.h"

#include <cstring>
#include <initializer_list>
#include <vector>

using namespace GBA;

namespace
{
	// ---------------------------------------------------------------------------------------
	// The bench
	// ---------------------------------------------------------------------------------------

	/// <summary>Where the test programs live: the bottom of EWRAM.</summary>
	const u32 CodeBase = MemEwram;

	/// <summary>Where the test data (and the stacks) live.</summary>
	const u32 DataBase = MemEwram + 0x10000;
	const u32 StackTop = DataBase + 0x1000;

	// The flag bits as they sit in a NZCV nibble, which is what the helpers below compare.
	enum : u32 { NBit = 8, ZBit = 4, CBit = 2, VBit = 1 };

	/// <summary>A bus, a CPU and the handful of helpers the tests need to poke at them.</summary>
	struct Bench
	{
		GbaBus bus;

		Bench()
		{
			// GbaBus owns the memory and the devices; the CPU is a member of the bus. Reset()
			// brings both to the power-on state (the BIOS and the cartridge are kept).
			bus.Reset();
			bus.cpu.Reset();
		}

		Arm7tdmi& Cpu() { return bus.cpu; }

		// -- loading a program ------------------------------------------------------------

		void Arm(u32 address, std::initializer_list<u32> words)
		{
			u32 at = address;
			for (u32 word : words)
			{
				bus.Write32(at, word);
				at += 4;
			}
		}

		void Thumb(u32 address, std::initializer_list<u16> halfwords)
		{
			u32 at = address;
			for (u16 halfword : halfwords)
			{
				bus.Write16(at, halfword);
				at += 2;
			}
		}

		/// <summary>Install a 16 KByte BIOS image with `word` at `offset` (a vector).</summary>
		void InstallBios(u32 offset, u32 word)
		{
			if (bios.empty())
				bios.assign(BiosSize, 0);
			memcpy(&bios[offset], &word, sizeof(word));
			bus.SetBios(bios.data(), bios.size());
		}

		std::vector<u8> bios;

		// -- running ----------------------------------------------------------------------

		void EnterArm(u32 address) { bus.cpu.BranchTo(address & ~1u); }
		void EnterThumb(u32 address) { bus.cpu.BranchTo((address & ~1u) | 1u); }

		int Step() { return bus.cpu.Step(); }

		int Steps(int count)
		{
			int cycles = 0;
			for (int i = 0; i < count; i++)
				cycles += bus.cpu.Step();
			return cycles;
		}

		/// <summary>Back to the power-on state (mode, banks, statistics, PC).</summary>
		void ResetCpu() { bus.cpu.Reset(); }

		// -- registers and flags ----------------------------------------------------------

		u32 R(int index) { return bus.cpu.Reg(index); }
		void SetR(int index, u32 value) { bus.cpu.SetReg(index, value); }

		/// <summary>The four condition flags as a nibble: N=8, Z=4, C=2, V=1.</summary>
		u32 Nzcv() { return (bus.cpu.ReadCPSR() >> 28) & 0xF; }

		void SetNzcv(u32 nzcv)
		{
			bus.cpu.WriteCPSR((bus.cpu.ReadCPSR() & 0x0FFFFFFFu) | (nzcv << 28));
		}

		// -- memory -----------------------------------------------------------------------

		u32 Peek(u32 address) { return bus.Read32(address); }
		u16 Peek16(u32 address) { return bus.Read16(address); }
		u8 Peek8(u32 address) { return bus.Read8(address); }
		void Poke(u32 address, u32 value) { bus.Write32(address, value); }
		void Poke16(u32 address, u16 value) { bus.Write16(address, value); }
		void Poke8(u32 address, u8 value) { bus.Write8(address, value); }
	};

	// ---------------------------------------------------------------------------------------
	// The encoders
	//
	// Everything below is the encoding tables of ARM DDI 0100E section 3.2 (ARM) and 4.5
	// (Thumb) written as bit arithmetic. The tests use them so that the instruction words in a
	// test read like the assembly the ARM Architecture Reference Manual would disassemble them to.
	// ---------------------------------------------------------------------------------------

	namespace Enc
	{
		// ARM Architecture Reference Manual A3.3.2, the condition field.
		enum Cond
		{
			EQ = 0x0, NE = 0x1, CS = 0x2, CC = 0x3, MI = 0x4, PL = 0x5, VS = 0x6, VC = 0x7,
			HI = 0x8, LS = 0x9, GE = 0xA, LT = 0xB, GT = 0xC, LE = 0xD, AL = 0xE,
		};

		// The opcode field of a data processing instruction (ARM Architecture Reference Manual Table 3-3).
		enum Alu
		{
			AND = 0x0, EOR = 0x1, SUB = 0x2, RSB = 0x3, ADD = 0x4, ADC = 0x5, SBC = 0x6,
			RSC = 0x7, TST = 0x8, TEQ = 0x9, CMP = 0xA, CMN = 0xB, ORR = 0xC, MOV = 0xD,
			BIC = 0xE, MVN = 0xF,
		};

		enum Shift { LSL = 0, LSR = 1, ASR = 2, ROR = 3 };

		/// <summary>Rotate left, the inverse of the ARM immediate encoding's rotate right.</summary>
		inline u32 RotateLeft(u32 value, u32 amount)
		{
			amount &= 31;
			if (amount == 0)
				return value;
			return (value << amount) | (value >> (32 - amount));
		}

		/// <summary>
		/// Data processing with an immediate operand. The helper solves the rotate field and
		/// fails the test if the value is one the encoding cannot hold.
		/// </summary>
		inline u32 DpImm(u32 cond, u32 op, bool setFlags, int rn, int rd, u32 value)
		{
			u32 rotate = 0;
			u32 imm8 = 0;
			bool found = false;
			for (u32 r = 0; r < 16 && !found; r++)
			{
				u32 candidate = RotateLeft(value, r * 2);
				if ((candidate & 0xFFFFFF00u) == 0)
				{
					rotate = r;
					imm8 = candidate & 0xFF;
					found = true;
				}
			}
			GBA_CHECK_MSG(found, std::string("the test asked for an ARM immediate the encoding cannot hold"));
			return (cond << 28) | (1u << 25) | (op << 21) | (setFlags ? (1u << 20) : 0) |
				((u32)rn << 16) | ((u32)rd << 12) | (rotate << 8) | imm8;
		}

		/// <summary>Data processing with a register operand, optionally shifted.</summary>
		inline u32 DpReg(u32 cond, u32 op, bool setFlags, int rn, int rd, int rm,
			u32 shiftType = LSL, u32 shiftAmount = 0, int shiftRegister = -1)
		{
			u32 word = (cond << 28) | (op << 21) | (setFlags ? (1u << 20) : 0) |
				((u32)rn << 16) | ((u32)rd << 12);
			word |= shiftType << 5;
			if (shiftRegister >= 0)
				word |= (1u << 4) | ((u32)shiftRegister << 8);
			else
				word |= shiftAmount << 7;
			return word | (u32)rm;
		}

		// ARM Architecture Reference Manual A4.1.3, single data transfer with a 12-bit immediate offset.
		inline u32 LdrStr(u32 cond, bool load, bool byte, int rd, int rn, u32 offset,
			bool pre = true, bool up = true, bool writeback = false)
		{
			return (cond << 28) | (1u << 26) | (pre ? (1u << 24) : 0) | (up ? (1u << 23) : 0) |
				(byte ? (1u << 22) : 0) | (writeback ? (1u << 21) : 0) | (load ? (1u << 20) : 0) |
				((u32)rn << 16) | ((u32)rd << 12) | (offset & 0xFFF);
		}

		/// <summary>The same, with a shifted register offset.</summary>
		inline u32 LdrStrReg(u32 cond, bool load, bool byte, int rd, int rn, int rm, u32 shiftType = LSL,
			u32 shiftAmount = 0, bool pre = true, bool up = true, bool writeback = false)
		{
			return (cond << 28) | (1u << 26) | (1u << 25) | (pre ? (1u << 24) : 0) |
				(up ? (1u << 23) : 0) | (byte ? (1u << 22) : 0) | (writeback ? (1u << 21) : 0) |
				(load ? (1u << 20) : 0) | ((u32)rn << 16) | ((u32)rd << 12) |
				(shiftAmount << 7) | (shiftType << 5) | (u32)rm;
		}

		/// <summary>
		/// ARM Architecture Reference Manual A4.1.8, the extra load/store instructions. H and S are the bits of the 1SH1
		/// field: (H=1, S=0) is LDRH/STRH, (H=0, S=1) LDRSB and (H=1, S=1) LDRSH.
		/// </summary>
		inline u32 HalfTransfer(u32 cond, bool load, bool half, bool sign, int rd, int rn, u32 offset,
			bool immediate = true, bool pre = true, bool up = true, bool writeback = false)
		{
			u32 word = (cond << 28) | (pre ? (1u << 24) : 0) | (up ? (1u << 23) : 0) |
				(immediate ? (1u << 22) : 0) | (writeback ? (1u << 21) : 0) |
				(load ? (1u << 20) : 0) | ((u32)rn << 16) | ((u32)rd << 12) | 0x90;
			if (half)
				word |= 0x20;
			if (sign)
				word |= 0x40;
			if (immediate)
				word |= ((offset & 0xF0) << 4) | (offset & 0xF);
			else
				word |= offset & 0xF;
			return word;
		}

		// ARM Architecture Reference Manual A4.1.6, MUL/MLA.
		inline u32 Multiply(u32 cond, bool accumulate, bool setFlags, int rd, int rm, int rs, int rn = 0)
		{
			return (cond << 28) | (accumulate ? (1u << 21) : 0) | (setFlags ? (1u << 20) : 0) |
				((u32)rd << 16) | ((u32)rn << 12) | ((u32)rs << 8) | 0x90 | (u32)rm;
		}

		// ARM Architecture Reference Manual A4.1.7, the long multiplies: RdHi in bits 19-16 and RdLo in bits 15-12.
		inline u32 LongMultiply(u32 cond, bool sign, bool accumulate, bool setFlags,
			int rdHi, int rdLo, int rm, int rs)
		{
			return (cond << 28) | (1u << 23) | (sign ? (1u << 22) : 0) |
				(accumulate ? (1u << 21) : 0) | (setFlags ? (1u << 20) : 0) |
				((u32)rdHi << 16) | ((u32)rdLo << 12) | ((u32)rs << 8) | 0x90 | (u32)rm;
		}

		// ARM Architecture Reference Manual A4.1.9, SWP/SWPB.
		inline u32 Swap(u32 cond, bool byte, int rd, int rn, int rm)
		{
			return (cond << 28) | (1u << 24) | (byte ? (1u << 22) : 0) | ((u32)rn << 16) |
				((u32)rd << 12) | 0x90 | (u32)rm;
		}

		// ARM Architecture Reference Manual A4.1.20, load/store multiple.
		inline u32 BlockTransfer(u32 cond, bool load, bool pre, bool up, bool userBank, bool writeback,
			int rn, u32 list)
		{
			return (cond << 28) | (1u << 27) | (pre ? (1u << 24) : 0) | (up ? (1u << 23) : 0) |
				(userBank ? (1u << 22) : 0) | (writeback ? (1u << 21) : 0) |
				(load ? (1u << 20) : 0) | ((u32)rn << 16) | (list & 0xFFFF);
		}

		// ARM Architecture Reference Manual A4.1.4, B/BL.
		inline u32 Branch(u32 cond, bool link, int byteOffset)
		{
			return (cond << 28) | (1u << 27) | (1u << 25) | (link ? (1u << 24) : 0) |
				(((u32)(byteOffset >> 2)) & 0xFFFFFF);
		}

		/// <summary>BX Rm (ARM Architecture Reference Manual A4.1.4).</summary>
		inline u32 Bx(u32 cond, int rm) { return (cond << 28) | 0x012FFF10u | (u32)rm; }

		/// <summary>SWI with the BIOS function number in the comment field (bits 16-23).</summary>
		inline u32 Swi(u32 cond, u32 comment) { return (cond << 28) | 0x0F000000u | ((comment & 0xFF) << 16); }

		/// <summary>MRS Rd, CPSR/SPSR (ARM Architecture Reference Manual A4.1.10).</summary>
		inline u32 Mrs(u32 cond, bool spsr, int rd)
		{
			return (cond << 28) | 0x010F0000u | (spsr ? (1u << 22) : 0) | ((u32)rd << 12);
		}

		/// <summary>MSR CPSR/SPSR_&lt;fields&gt;, Rm (ARM Architecture Reference Manual A4.1.11).</summary>
		inline u32 MsrReg(u32 cond, bool spsr, u32 fields, int rm)
		{
			return (cond << 28) | 0x0120F000u | (spsr ? (1u << 22) : 0) | ((fields & 0xF) << 16) | (u32)rm;
		}

		/// <summary>MSR CPSR/SPSR_&lt;fields&gt;, #imm8 rotated (ARM Architecture Reference Manual A4.1.11).</summary>
		inline u32 MsrImm(u32 cond, bool spsr, u32 fields, u32 rotate, u32 imm8)
		{
			return (cond << 28) | 0x0320F000u | (spsr ? (1u << 22) : 0) | ((fields & 0xF) << 16) |
				((rotate & 0xF) << 8) | (imm8 & 0xFF);
		}

		// -- Thumb (ARM Architecture Reference Manual 4.5, Table 4-1) ------------------------------------------------

		/// <summary>Format 1: LSL/LSR/ASR Rd, Rs, #imm5.</summary>
		inline u16 ThumbShift(u32 type, u32 amount, int rs, int rd)
		{
			return (u16)((type << 11) | ((amount & 0x1F) << 6) | ((u32)rs << 3) | (u32)rd);
		}

		/// <summary>Format 2: ADD/SUB Rd, Rs, Rn or #imm3.</summary>
		inline u16 ThumbAddSub(bool immediate, bool subtract, u32 field, int rs, int rd)
		{
			return (u16)(0x1800 | (immediate ? 0x0400 : 0) | (subtract ? 0x0200 : 0) |
				((field & 7) << 6) | ((u32)rs << 3) | (u32)rd);
		}

		/// <summary>Format 3: MOV/CMP/ADD/SUB Rd, #imm8 (op: 0 MOV, 1 CMP, 2 ADD, 3 SUB).</summary>
		inline u16 ThumbMovCmpAddSub(u32 op, int rd, u32 immediate)
		{
			return (u16)(0x2000 | ((op & 3) << 11) | ((u32)rd << 8) | (immediate & 0xFF));
		}

		/// <summary>Format 4: the sixteen ALU operations, encoded as op Rd, Rs.</summary>
		inline u16 ThumbAlu(u32 op, int rs, int rd)
		{
			return (u16)(0x4000 | ((op & 0xF) << 6) | ((u32)rs << 3) | (u32)rd);
		}

		/// <summary>Format 5: the hi register operations and BX (op: 0 ADD, 1 CMP, 2 MOV, 3 BX).</summary>
		inline u16 ThumbHi(u32 op, int rs, int rd)
		{
			return (u16)(0x4400 | ((op & 3) << 8) | ((rs & 8) ? 0x40 : 0) | ((rd & 8) ? 0x80 : 0) |
				((u32)(rs & 7) << 3) | (u32)(rd & 7));
		}

		/// <summary>Format 6: LDR Rd, [PC, #word8*4].</summary>
		inline u16 ThumbPcLoad(int rd, u32 word8) { return (u16)(0x4800 | ((u32)rd << 8) | (word8 & 0xFF)); }

		/// <summary>Format 7: STR/LDR/STRB/LDRB Rd, [Rb, Ro].</summary>
		inline u16 ThumbLdrStrReg(bool load, bool byte, int ro, int rb, int rd)
		{
			return (u16)(0x5000 | (load ? 0x0800 : 0) | (byte ? 0x0400 : 0) |
				((u32)ro << 6) | ((u32)rb << 3) | (u32)rd);
		}

		/// <summary>Format 8: STRH/LDRH/LDRSB/LDRSH Rd, [Rb, Ro].</summary>
		inline u16 ThumbLdrStrSign(bool half, bool sign, int ro, int rb, int rd)
		{
			return (u16)(0x5200 | (half ? 0x0800 : 0) | (sign ? 0x0400 : 0) |
				((u32)ro << 6) | ((u32)rb << 3) | (u32)rd);
		}

		/// <summary>Format 9: STR/LDR/STRB/LDRB Rd, [Rb, #imm5].</summary>
		inline u16 ThumbLdrStrImm(bool byte, bool load, u32 offset5, int rb, int rd)
		{
			return (u16)(0x6000 | (byte ? 0x1000 : 0) | (load ? 0x0800 : 0) |
				((offset5 & 0x1F) << 6) | ((u32)rb << 3) | (u32)rd);
		}

		/// <summary>Format 10: STRH/LDRH Rd, [Rb, #imm5*2].</summary>
		inline u16 ThumbLdrStrHalf(bool load, u32 offset5, int rb, int rd)
		{
			return (u16)(0x8000 | (load ? 0x0800 : 0) | ((offset5 & 0x1F) << 6) |
				((u32)rb << 3) | (u32)rd);
		}

		/// <summary>Format 11: STR/LDR Rd, [SP, #word8*4].</summary>
		inline u16 ThumbSpRel(bool load, int rd, u32 word8)
		{
			return (u16)(0x9000 | (load ? 0x0800 : 0) | ((u32)rd << 8) | (word8 & 0xFF));
		}

		/// <summary>Format 12: ADD Rd, PC/SP, #word8*4.</summary>
		inline u16 ThumbLoadAddress(bool fromStack, int rd, u32 word8)
		{
			return (u16)(0xA000 | (fromStack ? 0x0800 : 0) | ((u32)rd << 8) | (word8 & 0xFF));
		}

		/// <summary>Format 13: ADD/SUB SP, #word7*4.</summary>
		inline u16 ThumbAddSp(bool subtract, u32 word7)
		{
			return (u16)(0xB000 | (subtract ? 0x0080 : 0) | (word7 & 0x7F));
		}

		/// <summary>Format 14: PUSH/POP.</summary>
		inline u16 ThumbPushPop(bool pop, bool extra, u32 list)
		{
			return (u16)(0xB400 | (pop ? 0x0800 : 0) | (extra ? 0x0100 : 0) | (list & 0xFF));
		}

		/// <summary>Format 15: STMIA/LDMIA Rb!, {Rlist}.</summary>
		inline u16 ThumbMultiple(bool load, int rb, u32 list)
		{
			return (u16)(0xC000 | (load ? 0x0800 : 0) | ((u32)rb << 8) | (list & 0xFF));
		}

		/// <summary>Format 16: B&lt;cond&gt; label (a signed byte offset in halfwords).</summary>
		inline u16 ThumbCondBranch(u32 cond, int halfwordOffset)
		{
			return (u16)(0xD000 | ((cond & 0xF) << 8) | ((u32)halfwordOffset & 0xFF));
		}

		/// <summary>Format 17: SWI with the comment in the low byte.</summary>
		inline u16 ThumbSwiCall(u32 comment) { return (u16)(0xDF00 | (comment & 0xFF)); }

		/// <summary>Format 18: B label (an unsigned 11-bit halfword offset).</summary>
		inline u16 ThumbBranch(int halfwordOffset)
		{
			return (u16)(0xE000 | ((u32)halfwordOffset & 0x7FF));
		}

		/// <summary>Format 19, first halfword: the high part of a BL offset (bits 22-12).</summary>
		inline u16 ThumbBlFirst(u32 offset11) { return (u16)(0xF000 | (offset11 & 0x7FF)); }

		/// <summary>Format 19, second halfword: the low part and the branch.</summary>
		inline u16 ThumbBlSecond(u32 offset11) { return (u16)(0xF800 | (offset11 & 0x7FF)); }
	}

	// ---------------------------------------------------------------------------------------
	// Reset and the pipeline
	// ---------------------------------------------------------------------------------------

	GBA_TEST(Cpu, ResetState)
	{
		Bench b;
		Arm7tdmi& cpu = b.Cpu();
		cpu.Reset();

		// Supervisor mode, ARM state, both interrupts masked, fetching at 0.
		GBA_CHECK(cpu.Mode() == ModeSupervisor);
		GBA_CHECK(!cpu.ThumbState());
		GBA_CHECK((cpu.ReadCPSR() & FlagI) != 0);
		GBA_CHECK((cpu.ReadCPSR() & FlagF) != 0);
		GBA_CHECK_EQ(cpu.CurrentPC(), 0u);

		// The banked registers are cleared, including the ones the reset mode does not use.
		for (int i = 0; i <= 14; i++)
			GBA_CHECK_EQ(cpu.Reg(i), 0u);
		GBA_CHECK_EQ(cpu.ReadSPSR(), 0u);
		GBA_CHECK_EQ(cpu.RetiredInstructions(), 0ull);
		GBA_CHECK_EQ(cpu.UndefinedInstructions(), 0ull);
	}

	GBA_TEST(Cpu, PcReadsAsPipeline)
	{
		// ARM Architecture Reference Manual A3.4.1: r15 reads as the address of the instruction plus 8 in ARM state...
		Bench b;
		b.Arm(CodeBase, { Enc::DpReg(Enc::AL, Enc::MOV, false, 0, 0, 15) });	// MOV r0, r15
		b.EnterArm(CodeBase);
		b.Step();
		GBA_CHECK_EQ(b.R(0), CodeBase + 8);

		// ... and plus 4 in Thumb state (ARM Architecture Reference Manual 4.5, the Thumb pipeline).
		Bench t;
		t.Thumb(CodeBase, { Enc::ThumbHi(2, 15, 0) });						// MOV r0, r15
		t.EnterThumb(CodeBase);
		t.Step();
		GBA_CHECK_EQ(t.R(0), CodeBase + 4);
		GBA_CHECK(t.Cpu().ThumbState());
	}

	// ---------------------------------------------------------------------------------------
	// Conditions
	// ---------------------------------------------------------------------------------------

	GBA_TEST(Cpu, ConditionCodes)
	{
		// Every condition against every combination of the four flags. The expected table is
		// ARM Architecture Reference Manual Table 3-2 written out (N=8, Z=4, C=2, V=1).
		for (u32 cond = 0; cond <= 0xE; cond++)
		{
			Bench b;
			b.Arm(CodeBase, { Enc::DpImm(cond, Enc::MOV, false, 0, 0, 0x55) });
			b.EnterArm(CodeBase);

			for (u32 nzcv = 0; nzcv < 16; nzcv++)
			{
				b.SetR(0, 0);
				b.SetNzcv(nzcv);
				b.EnterArm(CodeBase);			// each run starts at the same instruction
				b.Step();

				bool n = (nzcv & NBit) != 0;
				bool z = (nzcv & ZBit) != 0;
				bool c = (nzcv & CBit) != 0;
				bool v = (nzcv & VBit) != 0;
				bool expected = false;

				switch (cond)
				{
				case Enc::EQ: expected = z; break;
				case Enc::NE: expected = !z; break;
				case Enc::CS: expected = c; break;
				case Enc::CC: expected = !c; break;
				case Enc::MI: expected = n; break;
				case Enc::PL: expected = !n; break;
				case Enc::VS: expected = v; break;
				case Enc::VC: expected = !v; break;
				case Enc::HI: expected = c && !z; break;
				case Enc::LS: expected = !c || z; break;
				case Enc::GE: expected = n == v; break;
				case Enc::LT: expected = n != v; break;
				case Enc::GT: expected = !z && (n == v); break;
				case Enc::LE: expected = z || (n != v); break;
				default: expected = true; break;
				}

				GBA_CHECK_EQ(b.R(0), expected ? 0x55u : 0u);
			}
		}
	}

	// ---------------------------------------------------------------------------------------
	// Data processing
	// ---------------------------------------------------------------------------------------

	GBA_TEST(Cpu, DataProcessingLogical)
	{
		Bench b;
		b.EnterArm(CodeBase);
		b.SetR(0, 0x0000FFFF);

		u32 at = CodeBase;
		// AND r1, r0, #0x0F00 -> 0x00000F00
		b.Arm(at, { Enc::DpImm(Enc::AL, Enc::AND, true, 0, 1, 0x0F00) });
		b.Step();
		GBA_CHECK_EQ(b.R(1), 0x00000F00u);
		GBA_CHECK_EQ(b.Nzcv(), 0u);

		// EOR r2, r0, #0xFF -> 0x0000FF00
		at += 4;
		b.Arm(at, { Enc::DpImm(Enc::AL, Enc::EOR, true, 0, 2, 0xFF) });
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(2), 0x0000FF00u);

		// ORR r3, r0, #0x10000 -> 0x0001FFFF
		at += 4;
		b.Arm(at, { Enc::DpImm(Enc::AL, Enc::ORR, true, 0, 3, 0x10000) });
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(3), 0x0001FFFFu);

		// BIC r4, r0, #0xFF -> 0x0000FF00
		at += 4;
		b.Arm(at, { Enc::DpImm(Enc::AL, Enc::BIC, true, 0, 4, 0xFF) });
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(4), 0x0000FF00u);

		// MVN r5, #0 -> 0xFFFFFFFF, N set
		at += 4;
		b.Arm(at, { Enc::DpImm(Enc::AL, Enc::MVN, true, 0, 5, 0) });
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(5), 0xFFFFFFFFu);
		GBA_CHECK_EQ(b.Nzcv(), NBit);

		// MOVS r6, #0 -> 0, Z set and C unchanged (it was clear)
		at += 4;
		b.Arm(at, { Enc::DpImm(Enc::AL, Enc::MOV, true, 0, 6, 0) });
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(6), 0u);
		GBA_CHECK_EQ(b.Nzcv(), ZBit);
	}

	GBA_TEST(Cpu, DataProcessingArithmetic)
	{
		// ADD/SUB/RSB and the C (borrow) and V (signed overflow) rules of ARM Architecture Reference Manual A3.4.1.
		Bench b;
		b.EnterArm(CodeBase);
		u32 at = CodeBase;

		// ADDS r0, r1, r2 with 0x7FFFFFFF + 1 -> 0x80000000, N and V set, no carry out.
		b.Arm(at, { Enc::DpReg(Enc::AL, Enc::ADD, true, 1, 0, 2) });
		b.SetR(1, 0x7FFFFFFF);
		b.SetR(2, 1);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0x80000000u);
		GBA_CHECK_EQ(b.Nzcv(), NBit | VBit);

		// ADDS r0, r1, r2 with 0xFFFFFFFF + 1 -> 0, Z and C set.
		at += 4;
		b.Arm(at, { Enc::DpReg(Enc::AL, Enc::ADD, true, 1, 0, 2) });
		b.SetR(1, 0xFFFFFFFF);
		b.SetR(2, 1);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0u);
		GBA_CHECK_EQ(b.Nzcv(), ZBit | CBit);

		// SUBS r0, r1, r2 with 0 - 1 -> 0xFFFFFFFF: borrow, so C is clear, N set.
		at += 4;
		b.Arm(at, { Enc::DpReg(Enc::AL, Enc::SUB, true, 1, 0, 2) });
		b.SetR(1, 0);
		b.SetR(2, 1);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0xFFFFFFFFu);
		GBA_CHECK_EQ(b.Nzcv(), NBit);

		// SUBS r0, r1, r2 with 0x80000000 - 1 -> 0x7FFFFFFF: signed overflow, C set.
		at += 4;
		b.Arm(at, { Enc::DpReg(Enc::AL, Enc::SUB, true, 1, 0, 2) });
		b.SetR(1, 0x80000000);
		b.SetR(2, 1);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0x7FFFFFFFu);
		GBA_CHECK_EQ(b.Nzcv(), CBit | VBit);

		// RSBS r0, r1, #0 with 0 - 5 -> 0xFFFFFFFB, N set, borrow.
		at += 4;
		b.Arm(at, { Enc::DpImm(Enc::AL, Enc::RSB, true, 1, 0, 0) });
		b.SetR(1, 5);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0xFFFFFFFBu);
		GBA_CHECK_EQ(b.Nzcv(), NBit);

		// RSBS r0, r1, #0 with 0 - 0 -> 0, Z and C set.
		at += 4;
		b.Arm(at, { Enc::DpImm(Enc::AL, Enc::RSB, true, 1, 0, 0) });
		b.SetR(1, 0);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0u);
		GBA_CHECK_EQ(b.Nzcv(), ZBit | CBit);

		// A register operand that is shifted by an immediate: ADD r0, r1, r2, LSL #4.
		at += 4;
		b.Arm(at, { Enc::DpReg(Enc::AL, Enc::ADD, false, 1, 0, 2, Enc::LSL, 4) });
		b.SetR(1, 0x10);
		b.SetR(2, 3);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0x40u);
	}

	GBA_TEST(Cpu, DataProcessingAdcSbcRsc)
	{
		// The three instructions that consume the carry flag: ADC = Rn + Rm + C,
		// SBC = Rn - Rm - 1 + C and RSC = Rm - Rn - 1 + C (ARM Architecture Reference Manual A3.4.1).
		Bench b;
		b.EnterArm(CodeBase);
		u32 at = CodeBase;

		// ADC with C set: 0xFFFFFFFF + 0 + 1 -> 0 with carry out, Z and C set.
		b.Arm(at, { Enc::DpReg(Enc::AL, Enc::ADC, true, 1, 0, 2) });
		b.SetR(1, 0xFFFFFFFF);
		b.SetR(2, 0);
		b.SetNzcv(CBit);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0u);
		GBA_CHECK_EQ(b.Nzcv(), ZBit | CBit);

		// ADC with C clear: 0xFFFFFFFF + 0 + 0 -> 0xFFFFFFFF, N set, no carry.
		at += 4;
		b.Arm(at, { Enc::DpReg(Enc::AL, Enc::ADC, true, 1, 0, 2) });
		b.SetR(1, 0xFFFFFFFF);
		b.SetR(2, 0);
		b.SetNzcv(0);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0xFFFFFFFFu);
		GBA_CHECK_EQ(b.Nzcv(), NBit);

		// ADC signed overflow: 0x7FFFFFFF + 0 + 1 -> 0x80000000, N and V set, C clear.
		at += 4;
		b.Arm(at, { Enc::DpReg(Enc::AL, Enc::ADC, true, 1, 0, 2) });
		b.SetR(1, 0x7FFFFFFF);
		b.SetR(2, 0);
		b.SetNzcv(CBit);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0x80000000u);
		GBA_CHECK_EQ(b.Nzcv(), NBit | VBit);

		// SBC with C set: 0 - 1 - 1 + 1 = 0xFFFFFFFF (borrow).
		at += 4;
		b.Arm(at, { Enc::DpReg(Enc::AL, Enc::SBC, true, 1, 0, 2) });
		b.SetR(1, 0);
		b.SetR(2, 1);
		b.SetNzcv(CBit);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0xFFFFFFFFu);
		GBA_CHECK_EQ(b.Nzcv(), NBit);

		// SBC with C clear: 0 - 1 - 1 + 0 = 0xFFFFFFFE (borrow).
		at += 4;
		b.Arm(at, { Enc::DpReg(Enc::AL, Enc::SBC, true, 1, 0, 2) });
		b.SetR(1, 0);
		b.SetR(2, 1);
		b.SetNzcv(0);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0xFFFFFFFEu);
		GBA_CHECK_EQ(b.Nzcv(), NBit);

		// SBC with C set: 5 - 3 - 1 + 1 = 2, no borrow so C is set again.
		at += 4;
		b.Arm(at, { Enc::DpReg(Enc::AL, Enc::SBC, true, 1, 0, 2) });
		b.SetR(1, 5);
		b.SetR(2, 3);
		b.SetNzcv(CBit);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 2u);
		GBA_CHECK_EQ(b.Nzcv(), CBit);

		// RSC with C clear: Rm - Rn - 1 + C = 1 - 0 - 1 + 0 = 0 with a carry out.
		at += 4;
		b.Arm(at, { Enc::DpReg(Enc::AL, Enc::RSC, true, 1, 0, 2) });
		b.SetR(1, 0);			// Rn
		b.SetR(2, 1);			// Rm
		b.SetNzcv(0);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0u);
		GBA_CHECK_EQ(b.Nzcv(), ZBit | CBit);

		// RSC with C clear: 0 - 1 - 1 + 0 = 0xFFFFFFFE, borrow.
		at += 4;
		b.Arm(at, { Enc::DpReg(Enc::AL, Enc::RSC, true, 1, 0, 2) });
		b.SetR(1, 1);			// Rn
		b.SetR(2, 0);			// Rm
		b.SetNzcv(0);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0xFFFFFFFEu);
		GBA_CHECK_EQ(b.Nzcv(), NBit);
	}

	GBA_TEST(Cpu, DataProcessingTestOps)
	{
		// TST, TEQ, CMP and CMN never write a register and always set the flags.
		Bench b;
		b.EnterArm(CodeBase);
		b.SetR(3, 0xDEADBEEF);

		// TST r3, #0x0F -> non-zero result, so Z clear; N comes from the result.
		u32 at = CodeBase;
		b.Arm(at, { Enc::DpImm(Enc::AL, Enc::TST, true, 3, 0, 0x0F) });
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0u);
		GBA_CHECK_EQ(b.Nzcv(), 0u);

		// TST r3, #0xF0000000 -> bit 31 set in the AND result (N), and the rotation that builds
		// the immediate (0x0F rotated right by 4) has 1 as its last bit out, so C is set too.
		at += 4;
		b.Arm(at, { Enc::DpImm(Enc::AL, Enc::TST, true, 3, 0, 0xF0000000) });
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.Nzcv(), NBit | CBit);

		// TEQ r3, r3 -> zero, Z set; the carry comes from the shifter (an LSL #0 leaves C).
		at += 4;
		b.Arm(at, { Enc::DpReg(Enc::AL, Enc::TEQ, true, 3, 0, 3) });
		b.SetNzcv(CBit);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.Nzcv(), ZBit | CBit);

		// CMP r3, r3 -> zero, Z and C set (no borrow).
		at += 4;
		b.Arm(at, { Enc::DpReg(Enc::AL, Enc::CMP, true, 3, 0, 3) });
		b.SetNzcv(0);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.Nzcv(), ZBit | CBit);

		// CMN r3, r3 with 0xDEADBEEF + itself: carry out (bit 31 lost), N set.
		at += 4;
		b.Arm(at, { Enc::DpReg(Enc::AL, Enc::CMN, true, 3, 0, 3) });
		b.SetNzcv(0);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.Nzcv(), NBit | CBit);
	}

	// ---------------------------------------------------------------------------------------
	// The barrel shifter
	// ---------------------------------------------------------------------------------------

	GBA_TEST(Cpu, BarrelShifterImmediate)
	{
		// ARM Architecture Reference Manual A3.4.2, all the awkward amounts: 0 (which means 32 for LSR and ASR and "keep
		// C" for LSL), 1, 32 and more than 32 through the register form as well.
		Bench b;
		b.EnterArm(CodeBase);
		b.SetR(1, 0x80000001);
		u32 at = CodeBase;

		// MOVS r0, r1, LSL #1 -> 0x00000002, carry is bit 31 of the operand.
		b.Arm(at, { Enc::DpReg(Enc::AL, Enc::MOV, true, 0, 0, 1, Enc::LSL, 1) });
		b.SetNzcv(0);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0x00000002u);
		GBA_CHECK_EQ(b.Nzcv(), CBit);

		// MOVS r0, r1, LSL #0 -> unchanged, C is left alone.
		at += 4;
		b.Arm(at, { Enc::DpReg(Enc::AL, Enc::MOV, true, 0, 0, 1, Enc::LSL, 0) });
		b.SetNzcv(CBit);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0x80000001u);
		GBA_CHECK_EQ(b.Nzcv(), NBit | CBit);

		// MOVS r0, r1, LSR #1 -> 0x40000000, carry is bit 0 of the operand.
		at += 4;
		b.Arm(at, { Enc::DpReg(Enc::AL, Enc::MOV, true, 0, 0, 1, Enc::LSR, 1) });
		b.SetNzcv(0);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0x40000000u);
		GBA_CHECK_EQ(b.Nzcv(), CBit);

		// MOVS r0, r1, LSR #0 means LSR #32 -> 0, carry is bit 31.
		at += 4;
		b.Arm(at, { Enc::DpReg(Enc::AL, Enc::MOV, true, 0, 0, 1, Enc::LSR, 0) });
		b.SetNzcv(0);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0u);
		GBA_CHECK_EQ(b.Nzcv(), ZBit | CBit);

		// MOVS r0, r1, ASR #1 -> 0xC0000000, carry is bit 0.
		at += 4;
		b.Arm(at, { Enc::DpReg(Enc::AL, Enc::MOV, true, 0, 0, 1, Enc::ASR, 1) });
		b.SetNzcv(0);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0xC0000000u);
		GBA_CHECK_EQ(b.Nzcv(), NBit | CBit);

		// MOVS r0, r1, ASR #0 means ASR #32 -> all sign bits, carry is bit 31.
		at += 4;
		b.Arm(at, { Enc::DpReg(Enc::AL, Enc::MOV, true, 0, 0, 1, Enc::ASR, 0) });
		b.SetNzcv(0);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0xFFFFFFFFu);
		GBA_CHECK_EQ(b.Nzcv(), NBit | CBit);

		// MOVS r0, r1, ROR #1 -> 0xC0000000, carry is bit 0 of the operand.
		at += 4;
		b.Arm(at, { Enc::DpReg(Enc::AL, Enc::MOV, true, 0, 0, 1, Enc::ROR, 1) });
		b.SetNzcv(0);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0xC0000000u);
		GBA_CHECK_EQ(b.Nzcv(), NBit | CBit);

		// MOVS r0, r1, ROR #0 is RRX: the carry goes in at bit 31 and bit 0 comes out.
		at += 4;
		b.Arm(at, { Enc::DpReg(Enc::AL, Enc::MOV, true, 0, 0, 1, Enc::ROR, 0) });
		b.SetNzcv(0);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0x40000000u);
		GBA_CHECK_EQ(b.Nzcv(), CBit);				// the C that went in at bit 31 is not N

		// An immediate rotate of 32 cannot be encoded (the five-bit field would read as 0, which
		// is RRX), so ROR by a full 32 is covered by the register-shift test above.
	}

	GBA_TEST(Cpu, BarrelShifterRegister)
	{
		// A shift by a register costs an extra internal cycle and masks the amount to its
		// bottom byte (ARM Architecture Reference Manual A3.4.1).
		Bench b;
		b.EnterArm(CodeBase);
		u32 at = CodeBase;

		struct Case { u32 type; u32 amount; u32 expected; u32 carry; };
		const Case cases[] =
		{
			{ Enc::LSL, 32, 0x00000000u, 1 },	// bit 0 goes into C
			{ Enc::LSL, 33, 0x00000000u, 0 },
			{ Enc::LSL, 31, 0x80000000u, 0 },	// bit 0 ends up in bit 31; carry is bit (32-31)
			{ Enc::LSR, 32, 0x00000000u, 1 },
			{ Enc::LSR, 33, 0x00000000u, 0 },
			{ Enc::ASR, 33, 0xFFFFFFFFu, 1 },
			{ Enc::ROR, 32, 0x80000001u, 1 },
			{ Enc::ROR, 33, 0xC0000000u, 1 },
		};

		for (const Case& c : cases)
		{
			b.Arm(at, { Enc::DpReg(Enc::AL, Enc::MOV, true, 0, 0, 1, (u32)c.type, 0, 2) });
			b.SetR(1, 0x80000001);
			b.SetR(2, c.amount);
			b.SetNzcv(0);
			b.EnterArm(at);
			int cycles = b.Step();
			GBA_CHECK_EQ(b.R(0), c.expected);
			GBA_CHECK_EQ(b.Nzcv() & CBit, c.carry ? CBit : 0u);
			GBA_CHECK_EQ(cycles, 2);		// 1S + 1I
			at += 4;
		}
	}

	GBA_TEST(Cpu, ArmRegisterShiftByZero)
	{
		// ARM Architecture Reference Manual A5.1.1 / A5.2, and gba-tests arm 166 ("shift by 0 register value"): a shift
		// whose amount comes from a register shifts by exactly the value in that register, so an
		// amount of 0 passes the operand through AND leaves C alone. The immediate encodings are
		// the ones where a field of 0 means "32" (LSR, ASR) or RRX (ROR), and a register amount
		// above 32 behaves as the ARM Architecture Reference Manual describes (0 for LSL/LSR, the sign for ASR, amount & 31
		// for ROR, with C from the last bit moved).
		Bench b;
		b.EnterArm(CodeBase);

		struct Case { u32 type; u32 amount; u32 carryIn; u32 expected; u32 carryOut; };
		const Case cases[] =
		{
			// amount 0: nothing moves and C keeps whatever it had
			{ Enc::LSL, 0, 1, 0x80000001u, 1 },
			{ Enc::LSR, 0, 1, 0x80000001u, 1 },
			{ Enc::ASR, 0, 1, 0x80000001u, 1 },
			{ Enc::ROR, 0, 1, 0x80000001u, 1 },
			{ Enc::LSR, 0, 0, 0x80000001u, 0 },
			{ Enc::ROR, 0, 0, 0x80000001u, 0 },
			// 32
			{ Enc::LSL, 32, 1, 0x00000000u, 1 },	// C is bit 0 of the operand
			{ Enc::LSR, 32, 0, 0x00000000u, 1 },	// C is bit 31
			{ Enc::ASR, 32, 0, 0xFFFFFFFFu, 1 },	// the sign, C is bit 31
			{ Enc::ROR, 32, 0, 0x80000001u, 1 },	// a multiple of 32 rotates by nothing, C is bit 31
			// more than 32
			{ Enc::LSL, 33, 1, 0x00000000u, 0 },
			{ Enc::LSR, 33, 1, 0x00000000u, 0 },
			{ Enc::ASR, 33, 0, 0xFFFFFFFFu, 1 },
			{ Enc::ROR, 33, 0, 0xC0000000u, 1 },
		};

		u32 at = CodeBase;
		for (const Case& c : cases)
		{
			b.Arm(at, { Enc::DpReg(Enc::AL, Enc::MOV, true, 0, 0, 1, c.type, 0, 2) });
			b.SetR(1, 0x80000001);
			b.SetR(2, c.amount);
			b.SetNzcv(c.carryIn ? CBit : 0);
			b.EnterArm(at);
			b.Step();
			GBA_CHECK_EQ(b.R(0), c.expected);
			GBA_CHECK_EQ((b.Nzcv() & CBit) ? 1u : 0u, c.carryOut);
			at += 4;
		}
	}

	GBA_TEST(Cpu, ThumbRegisterShiftByZero)
	{
		// The same rule for the Thumb format 4 shifts, whose amount is the low byte of Rd (that
		// is why the test gives r0 the value and r1 the amount): gba-tests thumb 068.
		Bench b;
		b.EnterThumb(CodeBase);

		struct Case { u32 op; u32 amount; u32 carryIn; u32 expected; u32 carryOut; };
		const Case cases[] =
		{
			{ 0x2, 0, 1, 0x80000001u, 1 },		// LSL r0, r1
			{ 0x3, 0, 1, 0x80000001u, 1 },		// LSR r0, r1
			{ 0x4, 0, 1, 0x80000001u, 1 },		// ASR r0, r1
			{ 0x7, 0, 1, 0x80000001u, 1 },		// ROR r0, r1
			{ 0x3, 0, 0, 0x80000001u, 0 },
			{ 0x7, 0, 0, 0x80000001u, 0 },
			{ 0x2, 32, 1, 0x00000000u, 1 },
			{ 0x3, 32, 0, 0x00000000u, 1 },
			{ 0x4, 32, 0, 0xFFFFFFFFu, 1 },
			{ 0x7, 32, 0, 0x80000001u, 1 },
			{ 0x2, 33, 1, 0x00000000u, 0 },
			{ 0x3, 33, 1, 0x00000000u, 0 },
			{ 0x4, 33, 0, 0xFFFFFFFFu, 1 },
			{ 0x7, 33, 0, 0xC0000000u, 1 },
		};

		u32 at = CodeBase;
		for (const Case& c : cases)
		{
			b.Thumb(at, { Enc::ThumbAlu(c.op, 1, 0) });		// op r0, r1: r0 is the value, r1 the amount
			b.SetR(0, 0x80000001);
			b.SetR(1, c.amount);
			b.SetNzcv(c.carryIn ? CBit : 0);
			b.EnterThumb(at);
			b.Step();
			GBA_CHECK_EQ(b.R(0), c.expected);
			GBA_CHECK_EQ((b.Nzcv() & CBit) ? 1u : 0u, c.carryOut);
			at += 2;
		}
	}

	GBA_TEST(Cpu, RotatedImmediateCarry)
	{
		// ARM Architecture Reference Manual A3.4.3: a rotated immediate is built by the shifter, so with S set C takes the
		// carry out of that rotation - the last bit rotated out of the eight-bit value, which is
		// bit 2*rotate-1 - while a rotate field of 0 leaves C alone. gba-tests arm 217.
		Bench b;
		b.EnterArm(CodeBase);
		u32 at = CodeBase;

		// MOVS r0, #0xF000000F is 0xFF rotated right by 4: the bit that leaves is bit 3 of 0xFF.
		b.Arm(at, { Enc::DpImm(Enc::AL, Enc::MOV, true, 0, 0, 0xF000000F) });
		b.SetNzcv(0);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0xF000000Fu);
		GBA_CHECK_EQ(b.Nzcv(), NBit | CBit);

		// MOVS r0, #0x0FF00000 is the same 0xFF rotated right by 12, so the bit that leaves is 0
		// and C is cleared even though it was set before.
		at += 4;
		b.Arm(at, { Enc::DpImm(Enc::AL, Enc::MOV, true, 0, 0, 0x0FF00000) });
		b.SetNzcv(CBit);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0x0FF00000u);
		GBA_CHECK_EQ(b.Nzcv(), 0u);

		// A rotate field of 0 is no rotation: C keeps its value.
		at += 4;
		b.Arm(at, { Enc::DpImm(Enc::AL, Enc::MOV, true, 0, 0, 0xFF) });
		b.SetNzcv(CBit);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0xFFu);
		GBA_CHECK_EQ(b.Nzcv(), CBit);

		// The carry reaches a logical operation the same way: ANDS with 0xF0000000 (0x0F rotated
		// right by 4) leaves 1 in C.
		at += 4;
		b.Arm(at, { Enc::DpImm(Enc::AL, Enc::AND, true, 0, 0, 0xF0000000) });
		b.SetR(0, 0xFFFFFFFF);
		b.SetNzcv(0);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0xF0000000u);
		GBA_CHECK_EQ(b.Nzcv(), NBit | CBit);

		// An arithmetic operation takes C from the addition instead, not from the rotation.
		at += 4;
		b.Arm(at, { Enc::DpImm(Enc::AL, Enc::ADD, true, 1, 0, 0xF0000000) });
		b.SetR(1, 0x10000000);
		b.SetNzcv(0);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0x00000000u);
		GBA_CHECK_EQ(b.Nzcv(), ZBit | CBit);		// 0x10000000 + 0xF0000000 carries out
	}

	// ---------------------------------------------------------------------------------------
	// Multiply
	// ---------------------------------------------------------------------------------------

	GBA_TEST(Cpu, Multiply)
	{
		Bench b;
		b.EnterArm(CodeBase);
		u32 at = CodeBase;

		// MULS r0, r1, r2 with 3 * 5 = 15: N and Z from the 32-bit result.
		b.Arm(at, { Enc::Multiply(Enc::AL, false, true, 0, 1, 2) });
		b.SetR(1, 3);
		b.SetR(2, 5);
		b.EnterArm(at);
		int cycles = b.Step();
		GBA_CHECK_EQ(b.R(0), 15u);
		GBA_CHECK_EQ(b.Nzcv() & (NBit | ZBit), 0u);
		GBA_CHECK_EQ(cycles, 2);			// 1S + mI with a multiplier that fits in 8 bits

		// MULS r0, r1, r2 with 0x80000000 * 2 = 0 (the low 32 bits): Z set.
		at += 4;
		b.Arm(at, { Enc::Multiply(Enc::AL, false, true, 0, 1, 2) });
		b.SetR(1, 0x80000000);
		b.SetR(2, 2);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0u);
		GBA_CHECK_EQ(b.Nzcv(), ZBit);

		// MLA r0, r1, r2, r3 with 3 * 5 + 1 = 16.
		at += 4;
		b.Arm(at, { Enc::Multiply(Enc::AL, true, false, 0, 1, 2, 3) });
		b.SetR(1, 3);
		b.SetR(2, 5);
		b.SetR(3, 1);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 16u);
	}

	GBA_TEST(Cpu, LongMultiply)
	{
		Bench b;
		b.EnterArm(CodeBase);
		u32 at = CodeBase;

		// UMULL r0, r1, r2, r3: RdLo = r0, RdHi = r1; 0xFFFFFFFF * 2 = 0x1FFFFFFFE.
		b.Arm(at, { Enc::LongMultiply(Enc::AL, false, false, true, 1, 0, 2, 3) });
		b.SetR(2, 0xFFFFFFFF);
		b.SetR(3, 2);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0xFFFFFFFEu);
		GBA_CHECK_EQ(b.R(1), 1u);
		GBA_CHECK_EQ(b.Nzcv() & (NBit | ZBit), 0u);

		// SMULL r0, r1, r2, r3 with -1 * 2 = -2 = 0xFFFFFFFFFFFFFFFE: N is bit 63.
		at += 4;
		b.Arm(at, { Enc::LongMultiply(Enc::AL, true, false, true, 1, 0, 2, 3) });
		b.SetR(2, 0xFFFFFFFF);
		b.SetR(3, 2);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0xFFFFFFFEu);
		GBA_CHECK_EQ(b.R(1), 0xFFFFFFFFu);
		GBA_CHECK_EQ(b.Nzcv(), NBit);

		// UMLAL r0, r1, r2, r3 with (1 << 32 | 1) + 0xFFFFFFFF * 1 = 0x2_0000_0000: the high
		// half is 2, the low half wrapped to zero and the 64-bit result is not zero.
		at += 4;
		b.Arm(at, { Enc::LongMultiply(Enc::AL, false, true, false, 1, 0, 2, 3) });
		b.SetR(0, 1);
		b.SetR(1, 1);
		b.SetR(2, 0xFFFFFFFF);
		b.SetR(3, 1);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0u);
		GBA_CHECK_EQ(b.R(1), 2u);
		GBA_CHECK_EQ(b.Nzcv(), 0u);

		// SMLAL r0, r1, r2, r3 with (0 << 32 | 5) + (-1 * 3) = 2.
		at += 4;
		b.Arm(at, { Enc::LongMultiply(Enc::AL, true, true, true, 1, 0, 2, 3) });
		b.SetR(0, 5);
		b.SetR(1, 0);
		b.SetR(2, 0xFFFFFFFF);
		b.SetR(3, 3);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 2u);
		GBA_CHECK_EQ(b.R(1), 0u);
		GBA_CHECK_EQ(b.Nzcv(), 0u);
	}

	// ---------------------------------------------------------------------------------------
	// Load/store
	// ---------------------------------------------------------------------------------------

	GBA_TEST(Cpu, LoadStoreWord)
	{
		Bench b;
		b.Poke(DataBase + 0, 0x11223344);
		b.Poke(DataBase + 4, 0x55667788);
		b.Poke(DataBase + 8, 0x99AABBCC);

		u32 at = CodeBase;
		b.EnterArm(CodeBase);

		// LDR r0, [r1] -> the first word.
		b.Arm(at, { Enc::LdrStr(Enc::AL, true, false, 0, 1, 0) });
		b.SetR(1, DataBase);
		b.EnterArm(at);
		GBA_CHECK_EQ(b.Step(), 3);			// 1S + 1N + 1I
		GBA_CHECK_EQ(b.R(0), 0x11223344u);

		// LDR r2, [r1, #4].
		at += 4;
		b.Arm(at, { Enc::LdrStr(Enc::AL, true, false, 2, 1, 4) });
		b.SetR(1, DataBase);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(2), 0x55667788u);

		// LDR r3, [r1, #-4] with the base at +4 -> the first word again.
		at += 4;
		b.Arm(at, { Enc::LdrStr(Enc::AL, true, false, 3, 1, 4, true, false) });
		b.SetR(1, DataBase + 4);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(3), 0x11223344u);

		// Post-indexed LDR r4, [r1], #4: the base moves on after the access.
		at += 4;
		b.Arm(at, { Enc::LdrStr(Enc::AL, true, false, 4, 1, 4, false) });
		b.SetR(1, DataBase);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(4), 0x11223344u);
		GBA_CHECK_EQ(b.R(1), DataBase + 4);

		// LDR r5, [r1, #4]! : write-back with a pre-indexed offset.
		at += 4;
		b.Arm(at, { Enc::LdrStr(Enc::AL, true, false, 5, 1, 4, true, true, true) });
		b.SetR(1, DataBase);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(5), 0x55667788u);
		GBA_CHECK_EQ(b.R(1), DataBase + 4);

		// STR r2, [r1, #8] and read it back with the memory helpers.
		at += 4;
		b.Arm(at, { Enc::LdrStr(Enc::AL, false, false, 2, 1, 8) });
		b.SetR(1, DataBase);
		b.SetR(2, 0xCAFEBABE);
		b.EnterArm(at);
		GBA_CHECK_EQ(b.Step(), 2);			// 2N
		GBA_CHECK_EQ(b.Peek(DataBase + 8), 0xCAFEBABEu);

		// An LDR with the same register as the base and the destination: the loaded value wins.
		at += 4;
		b.Arm(at, { Enc::LdrStr(Enc::AL, true, false, 1, 1, 0, true, true, true) });
		b.SetR(1, DataBase);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(1), 0x11223344u);
	}

	GBA_TEST(Cpu, LoadStoreByteAndHalfword)
	{
		Bench b;
		b.Poke8(DataBase + 0, 0xAB);
		b.Poke8(DataBase + 1, 0xCD);
		b.Poke16(DataBase + 4, 0x8001);

		u32 at = CodeBase;
		b.EnterArm(CodeBase);

		// LDRB r0, [r1].
		b.Arm(at, { Enc::LdrStr(Enc::AL, true, true, 0, 1, 0) });
		b.SetR(1, DataBase);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0xABu);

		// LDRB r2, [r1, #1].
		at += 4;
		b.Arm(at, { Enc::LdrStr(Enc::AL, true, true, 2, 1, 1) });
		b.SetR(1, DataBase);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(2), 0xCDu);

		// STRB r3, [r1, #2].
		at += 4;
		b.Arm(at, { Enc::LdrStr(Enc::AL, false, true, 3, 1, 2) });
		b.SetR(1, DataBase);
		b.SetR(3, 0x1234EF56);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.Peek8(DataBase + 2), 0x56u);
		GBA_CHECK_EQ(b.Peek16(DataBase), 0xCDABu);		// the two bytes the LDRBs read

		// LDRSB r4, [r1] -> 0xAB sign extends to 0xFFFFFFAB.
		at += 4;
		b.Arm(at, { Enc::HalfTransfer(Enc::AL, true, false, true, 4, 1, 0) });
		b.SetR(1, DataBase);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(4), 0xFFFFFFABu);

		// LDRH r5, [r1, #4] -> 0x8001 (no sign extension).
		at += 4;
		b.Arm(at, { Enc::HalfTransfer(Enc::AL, true, true, false, 5, 1, 4) });
		b.SetR(1, DataBase);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(5), 0x8001u);

		// LDRSH r6, [r1, #4] -> 0xFFFF8001.
		at += 4;
		b.Arm(at, { Enc::HalfTransfer(Enc::AL, true, true, true, 6, 1, 4) });
		b.SetR(1, DataBase);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(6), 0xFFFF8001u);

		// STRH r7, [r1, #6] writes both bytes.
		at += 4;
		b.Arm(at, { Enc::HalfTransfer(Enc::AL, false, true, false, 7, 1, 6) });
		b.SetR(1, DataBase);
		b.SetR(7, 0x00001234);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.Peek16(DataBase + 6), 0x1234u);
	}

	GBA_TEST(Cpu, LoadStoreRotatedRegisterOffset)
	{
		// A register offset goes through the barrel shifter: LDR r0, [r1, r2, LSL #2] with
		// r2 = 1 must read the word at r1 + 4.
		Bench b;
		b.Poke(DataBase + 4, 0xFEEDFACE);
		b.Arm(CodeBase, { Enc::LdrStrReg(Enc::AL, true, false, 0, 1, 2, Enc::LSL, 2) });
		b.SetR(1, DataBase);
		b.SetR(2, 1);
		b.EnterArm(CodeBase);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0xFEEDFACEu);

		// The same offset as an ROR: 2 rotated right by 31 is 4.
		Bench r;
		r.Poke(DataBase + 4, 0xFEEDFACE);
		r.Arm(CodeBase, { Enc::LdrStrReg(Enc::AL, true, false, 0, 1, 2, Enc::ROR, 31) });
		r.SetR(1, DataBase);
		r.SetR(2, 2);
		r.EnterArm(CodeBase);
		r.Step();
		GBA_CHECK_EQ(r.R(0), 0xFEEDFACEu);
	}

	GBA_TEST(Cpu, UnalignedLoads)
	{
		// ARM7TDMI data sheet: an unaligned LDR rotates the aligned word right by 8*(addr&3),
		// and an unaligned LDRH drops bit 0 and returns the aligned halfword.
		Bench b;
		b.Poke(DataBase, 0x11223344);
		b.EnterArm(CodeBase);
		u32 at = CodeBase;

		b.Arm(at, { Enc::LdrStr(Enc::AL, true, false, 0, 1, 1) });
		b.SetR(1, DataBase);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0x44112233u);

		at += 4;
		b.Arm(at, { Enc::LdrStr(Enc::AL, true, false, 2, 1, 2) });
		b.SetR(1, DataBase);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(2), 0x33441122u);

		// LDRH from an odd address: the aligned halfword 0x3344 rotated right by eight bits, which
		// is the ARM7TDMI's misaligned halfword rule (gba-tests arm 408 / thumb 211).
		at += 4;
		b.Arm(at, { Enc::HalfTransfer(Enc::AL, true, true, false, 3, 1, 1) });
		b.SetR(1, DataBase);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(3), 0x44000033u);

		// A signed halfword load from an odd address is done as a signed *byte* load instead
		// (gba-tests arm 409): with the halfword 0xFF00 at DataBase + 4, an LDRSH from
		// DataBase + 5 reads the byte 0xFF and sign extends it.
		b.Poke16(DataBase + 4, 0xFF00);
		at += 4;
		b.Arm(at, { Enc::HalfTransfer(Enc::AL, true, true, true, 4, 1, 5) });
		b.SetR(1, DataBase);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(4), 0xFFFFFFFFu);

		// The Thumb format 8 loads follow the same two rules: an odd LDRH rotates the aligned
		// halfword, and an odd LDRSH is a signed byte load.
		Bench t;
		t.Poke16(DataBase, 0x00FF);
		t.Thumb(CodeBase, { Enc::ThumbLdrStrSign(true, false, 1, 0, 2) });	// LDRH r2, [r0, r1]
		t.EnterThumb(CodeBase);
		t.SetR(0, DataBase);
		t.SetR(1, 1);
		t.Step();
		GBA_CHECK_EQ(t.R(2), 0xFF000000u);

		Bench s;
		s.Poke16(DataBase, 0xFF00);			// bytes 00 FF: the byte at +1 is 0xFF
		s.Thumb(CodeBase, { Enc::ThumbLdrStrSign(true, true, 1, 0, 3) });	// LDRSH r3, [r0, r1]
		s.EnterThumb(CodeBase);
		s.SetR(0, DataBase);
		s.SetR(1, 1);
		s.Step();
		GBA_CHECK_EQ(s.R(3), 0xFFFFFFFFu);	// not 0xFFFFFF00: the byte path, not the halfword
	}

	GBA_TEST(Cpu, LoadStoreMultiple)
	{
		Bench b;
		b.Poke(DataBase + 0, 0x00000001);
		b.Poke(DataBase + 4, 0x00000002);
		b.Poke(DataBase + 8, 0x00000003);

		u32 at = CodeBase;
		b.EnterArm(CodeBase);

		// LDMIA r0!, {r1, r2, r3}: the base moves past the three words.
		b.Arm(at, { Enc::BlockTransfer(Enc::AL, true, false, true, false, true, 0, 0x0E) });
		b.SetR(0, DataBase);
		b.EnterArm(at);
		GBA_CHECK_EQ(b.Step(), 5);			// 1S + 1N + 2S + 1I
		GBA_CHECK_EQ(b.R(1), 1u);
		GBA_CHECK_EQ(b.R(2), 2u);
		GBA_CHECK_EQ(b.R(3), 3u);
		GBA_CHECK_EQ(b.R(0), DataBase + 12);

		// STMDB sp!, {r1, r2} with the stack at StackTop: r2 lands on top.
		at += 4;
		b.Arm(at, { Enc::BlockTransfer(Enc::AL, false, true, false, false, true, 13, 0x06) });
		b.SetR(13, StackTop);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.Peek(StackTop - 8), 1u);
		GBA_CHECK_EQ(b.Peek(StackTop - 4), 2u);
		GBA_CHECK_EQ(b.R(13), StackTop - 8);

		// The same transfer back: LDMIA sp!, {r4, r5}.
		at += 4;
		b.Arm(at, { Enc::BlockTransfer(Enc::AL, true, false, true, false, true, 13, 0x30) });
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(4), 1u);
		GBA_CHECK_EQ(b.R(5), 2u);
		GBA_CHECK_EQ(b.R(13), StackTop);
	}

	GBA_TEST(Cpu, LoadStoreMultipleBaseInList)
	{
		// ARM Architecture Reference Manual A4.1.20: with the base in the list and write-back the ARM7TDMI stores the
		// original base when it is the lowest register of the list and the write-back value
		// when it is the highest.
		Bench b;
		b.EnterArm(CodeBase);

		// STMIA r0!, {r0, r1}: r0 is the lowest, so the original base goes to memory.
		u32 at = CodeBase;
		b.Arm(at, { Enc::BlockTransfer(Enc::AL, false, false, true, false, true, 0, 0x03) });
		b.SetR(0, DataBase);
		b.SetR(1, 0x11111111);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.Peek(DataBase), DataBase);
		GBA_CHECK_EQ(b.Peek(DataBase + 4), 0x11111111u);
		GBA_CHECK_EQ(b.R(0), DataBase + 8);

		// STMIA r2!, {r0, r2}: the base r2 is the highest register of the list, so its own slot
		// gets the write-back value - the ARM7TDMI has already moved the base on by then.
		at += 4;
		b.Arm(at, { Enc::BlockTransfer(Enc::AL, false, false, true, false, true, 2, 0x0005) });
		b.SetR(0, 0x11111111);
		b.SetR(2, DataBase + 0x20);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.Peek(DataBase + 0x20), 0x11111111u);
		GBA_CHECK_EQ(b.Peek(DataBase + 0x24), DataBase + 0x28);
		GBA_CHECK_EQ(b.R(2), DataBase + 0x28);
	}

	GBA_TEST(Cpu, LoadStoreMultipleUserBank)
	{
		// The caret (ARM Architecture Reference Manual A4.1.20): with S set and r15 out of the list the transfer uses the
		// User bank's r8-r14 whatever mode the CPU is in.
		Bench b;
		b.ResetCpu();

		// Fill the User bank through System mode.
		b.Cpu().SwitchMode(ModeSystem);
		for (int i = 0; i < 5; i++)
			b.SetR(8 + i, 0x88888888u + (u32)i);
		b.SetR(13, 0x03008000);
		b.SetR(14, 0xEEEEEEEE);

		// Now run in IRQ mode, whose r8-r14 are a different set.
		b.Cpu().SwitchMode(ModeIrq);
		for (int i = 0; i < 5; i++)
			b.SetR(8 + i, 0x11111111u + (u32)i);

		b.SetR(0, DataBase);
		b.Arm(CodeBase, { Enc::BlockTransfer(Enc::AL, false, false, true, true, false, 0, 0x1F00) });
		b.EnterArm(CodeBase);
		b.Step();

		// STM with the caret stored the User bank, not the IRQ bank.
		for (int i = 0; i < 5; i++)
			GBA_CHECK_EQ(b.Peek(DataBase + 4 * i), 0x88888888u + (u32)i);

		// LDM with the caret writes the User bank. Only FIQ has its own r8-r12, so the IRQ
		// window is the User bank here and the loaded values are visible in both places.
		for (int i = 0; i < 5; i++)
			b.Poke(DataBase + 4 * i, 0xAAAA0000u + (u32)i);

		b.Cpu().SwitchMode(ModeIrq);
		b.SetR(0, DataBase);
		b.Arm(CodeBase, { Enc::BlockTransfer(Enc::AL, true, false, true, true, false, 0, 0x1F00) });
		b.EnterArm(CodeBase);
		b.Step();
		for (int i = 0; i < 5; i++)
			GBA_CHECK_EQ(b.R(8 + i), 0xAAAA0000u + (u32)i);

		b.Cpu().SwitchMode(ModeSystem);
		for (int i = 0; i < 5; i++)
			GBA_CHECK_EQ(b.R(8 + i), 0xAAAA0000u + (u32)i);

		// The IRQ bank's r13/r14 stayed where they were: those *are* banked per mode.
		GBA_CHECK_EQ(b.R(13), 0x03008000u);
		GBA_CHECK_EQ(b.R(14), 0xEEEEEEEEu);
	}

	GBA_TEST(Cpu, LoadStoreMultipleCaretReturn)
	{
		// An LDM with the caret and r15 is the exception return: the SPSR becomes the CPSR.
		Bench b;
		b.ResetCpu();
		b.Poke(StackTop - 8, 0x00000042);
		b.Poke(StackTop - 4, CodeBase + 0x40);

		// Enter IRQ mode through a real exception so that SPSR_irq holds a known CPSR.
		b.EnterArm(CodeBase);
		b.SetNzcv(NBit);
		u32 interrupted = b.Cpu().ReadCPSR();
		b.Cpu().Exception(VectorIrq, ModeIrq, FlagI);

		// The handler runs the return instruction at the vector. LDMIA sp! reads upwards from
		// the stack pointer, so it has to point at the first saved word.
		b.InstallBios(VectorIrq, Enc::BlockTransfer(Enc::AL, true, false, true, true, true, 13, 0x8001));
		b.SetR(13, StackTop - 8);
		b.EnterArm(VectorIrq);
		b.Step();

		GBA_CHECK_EQ(b.R(0), 0x42u);
		GBA_CHECK(b.Cpu().Mode() == ModeSupervisor);
		GBA_CHECK_EQ(b.Cpu().ReadCPSR(), interrupted);
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), CodeBase + 0x40);
	}

	GBA_TEST(Cpu, Swap)
	{
		// ARM Architecture Reference Manual A4.1.9: SWP reads the addressed word and writes the other one back, both in
		// the same instruction; SWPB does it a byte at a time.
		Bench b;
		b.Poke(DataBase, 0x11223344);
		b.Poke8(DataBase + 8, 0x5A);

		u32 at = CodeBase;
		b.EnterArm(CodeBase);

		b.Arm(at, { Enc::Swap(Enc::AL, false, 0, 2, 1) });		// SWP r0, r1, [r2]
		b.SetR(1, 0xAABBCCDD);
		b.SetR(2, DataBase);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0x11223344u);
		GBA_CHECK_EQ(b.Peek(DataBase), 0xAABBCCDDu);

		at += 4;
		b.Arm(at, { Enc::Swap(Enc::AL, true, 3, 5, 4) });		// SWPB r3, r4, [r5]
		b.SetR(4, 0x00001234);
		b.SetR(5, DataBase + 8);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(3), 0x5Au);
		GBA_CHECK_EQ(b.Peek8(DataBase + 8), 0x34u);
	}

	GBA_TEST(Cpu, MrsFromSpsrInException)
	{
		// MRS Rd, SPSR is only readable in an exception mode; it is how a handler finds out what
		// state it interrupted.
		Bench b;
		b.ResetCpu();
		b.EnterArm(CodeBase);
		b.SetNzcv(NBit | ZBit);
		u32 interrupted = b.Cpu().ReadCPSR();
		b.Cpu().Exception(VectorIrq, ModeIrq, FlagI);

		b.Arm(CodeBase, { Enc::Mrs(Enc::AL, true, 0) });		// MRS r0, SPSR
		b.EnterArm(CodeBase);
		b.Step();
		GBA_CHECK_EQ(b.R(0), interrupted);
		GBA_CHECK(b.Cpu().Mode() == ModeIrq);

		// MSR SPSR_cxsf, r1 then MRS it back: the handler can edit what it will restore, and the
		// live CPSR is not touched by either.
		b.Arm(CodeBase, {
			Enc::MsrReg(Enc::AL, true, 0xF, 1),
			Enc::Mrs(Enc::AL, true, 2),
		});
		b.SetR(1, 0xF0000000);
		b.EnterArm(CodeBase);
		u32 live = b.Cpu().ReadCPSR();
		b.Step();
		b.Step();
		GBA_CHECK_EQ(b.R(2), 0xF0000000u);
		GBA_CHECK_EQ(b.Cpu().ReadCPSR(), live);
	}

	GBA_TEST(Cpu, PcWithRegisterShift)
	{
		// The ARM7TDMI's pipeline quirk behind gba-tests arm 224/225: an instruction whose
		// operand is shifted by a register takes one extra internal cycle, and r15 then reads as
		// the address of the instruction plus 12 instead of plus 8 - for every r15 operand of
		// that instruction, and for the shift amount register too.
		Bench b;
		b.EnterArm(CodeBase);

		// MOVS r0, r15, LSL r1 with r1 = 0.
		b.Arm(CodeBase, { Enc::DpReg(Enc::AL, Enc::MOV, false, 0, 0, 15, Enc::LSL, 0, 1) });
		b.SetR(1, 0);
		b.EnterArm(CodeBase);
		b.Step();
		GBA_CHECK_EQ(b.R(0), CodeBase + 12);

		// ADD r0, r15, r0, LSL r1: operand 1 sees the same later value.
		b.Arm(CodeBase, { Enc::DpReg(Enc::AL, Enc::ADD, false, 15, 0, 0, Enc::LSL, 0, 1) });
		b.SetR(0, 0);
		b.SetR(1, 0);
		b.EnterArm(CodeBase);
		b.Step();
		GBA_CHECK_EQ(b.R(0), CodeBase + 12);

		// An immediate shift keeps the ordinary plus 8, even when the amount is 0 (gba-tests
		// arm 226 relies on this).
		b.Arm(CodeBase, { Enc::DpReg(Enc::AL, Enc::ADD, false, 15, 0, 0, Enc::LSL, 0) });
		b.SetR(0, 0);
		b.EnterArm(CodeBase);
		b.Step();
		GBA_CHECK_EQ(b.R(0), CodeBase + 8);

		// A plain operand read is plus 8 as well.
		b.Arm(CodeBase, { Enc::DpReg(Enc::AL, Enc::MOV, false, 0, 0, 15) });
		b.EnterArm(CodeBase);
		b.Step();
		GBA_CHECK_EQ(b.R(0), CodeBase + 8);
	}

	GBA_TEST(Cpu, EmptyRegisterListQuirk)
	{
		// The ARM7TDMI transfers r15 and moves the base by 0x40 when a block transfer names no
		// registers at all (gba-tests arm 513-515/530-532, thumb 227-229).
		Bench b;
		b.EnterArm(CodeBase);

		// STMIA r0!, {} stores the PC (the instruction address plus 12) at the base and leaves
		// the base 0x40 further on.
		b.Arm(CodeBase, { 0xE8A00000u });		// stmia r0!, {}
		b.SetR(0, DataBase);
		b.EnterArm(CodeBase);
		b.Step();
		GBA_CHECK_EQ(b.Peek(DataBase), CodeBase + 12);
		GBA_CHECK_EQ(b.R(0), DataBase + 0x40);

		// STMDB r2!, {} stores downwards: first address at base - 0x40.
		b.Arm(CodeBase + 4, { 0xE9220000u });	// stmdb r2!, {}
		b.SetR(2, DataBase + 0x100);
		b.EnterArm(CodeBase + 4);
		b.Step();
		GBA_CHECK_EQ(b.Peek(DataBase + 0xC0), CodeBase + 4 + 12);
		GBA_CHECK_EQ(b.R(2), DataBase + 0xC0);

		// LDMIA r0!, {} loads r15 from the base (a branch) and moves the base on by 0x40.
		b.Poke(DataBase + 0x200, CodeBase + 0x80);
		b.Arm(CodeBase + 8, { 0xE8B00000u });	// ldmia r0!, {}
		b.SetR(0, DataBase + 0x200);
		b.EnterArm(CodeBase + 8);
		b.Step();
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), CodeBase + 0x80);
		GBA_CHECK_EQ(b.R(0), DataBase + 0x240);

		// The same oddity in Thumb state stores the *Thumb* PC: the instruction address plus six,
		// because the pipeline advances by a halfword per stage there (gba-tests thumb 229).
		Bench t;
		t.Thumb(CodeBase, { 0xC000 });			// stmia r0!, {}
		t.EnterThumb(CodeBase);
		t.SetR(0, DataBase);
		t.Step();
		GBA_CHECK_EQ(t.Peek(DataBase), CodeBase + 6);
		GBA_CHECK_EQ(t.R(0), DataBase + 0x40);
	}

	GBA_TEST(Cpu, BlockTransferAddressingModes)
	{
		// The four addressing modes of ARM Architecture Reference Manual A4.1.20 and the misaligned base rule
		// (gba-tests arm 500-508): the transfer ignores the low two bits of the base while the
		// write-back keeps them, and an LDM does not rotate its loaded words.
		Bench b;
		b.EnterArm(CodeBase);
		b.SetR(0, 0x11111111);
		b.SetR(1, 0x22222222);

		// STMDA (decrement after) r2!, {r0, r1}: base-4 and base, write-back base-8.
		b.Arm(CodeBase, { Enc::BlockTransfer(Enc::AL, false, false, false, false, true, 2, 0x03) });
		b.SetR(2, DataBase + 0x40);
		b.EnterArm(CodeBase);
		b.Step();
		GBA_CHECK_EQ(b.Peek(DataBase + 0x3C), 0x11111111u);
		GBA_CHECK_EQ(b.Peek(DataBase + 0x40), 0x22222222u);
		GBA_CHECK_EQ(b.R(2), DataBase + 0x38);

		// LDMDA r2!, {r3, r4} reads them back from the same base and unwinds it.
		b.Arm(CodeBase + 4, { Enc::BlockTransfer(Enc::AL, true, false, false, false, true, 2, 0x18) });
		b.SetR(2, DataBase + 0x40);
		b.EnterArm(CodeBase + 4);
		b.Step();
		GBA_CHECK_EQ(b.R(3), 0x11111111u);
		GBA_CHECK_EQ(b.R(4), 0x22222222u);
		GBA_CHECK_EQ(b.R(2), DataBase + 0x38);

		// STMIB (increment before) r5!, {r0, r1}: base+4 and base+8.
		b.Arm(CodeBase + 8, { Enc::BlockTransfer(Enc::AL, false, true, true, false, true, 5, 0x03) });
		b.SetR(5, DataBase + 0x80);
		b.EnterArm(CodeBase + 8);
		b.Step();
		GBA_CHECK_EQ(b.Peek(DataBase + 0x84), 0x11111111u);
		GBA_CHECK_EQ(b.Peek(DataBase + 0x88), 0x22222222u);
		GBA_CHECK_EQ(b.R(5), DataBase + 0x88);

		// STMDB from an unaligned base (base + 3) with two registers: the words land at
		// base - 8 and base - 4, the base moves on by eight without being aligned.
		b.Arm(CodeBase + 12, { Enc::BlockTransfer(Enc::AL, false, true, false, false, true, 6, 0x03) });
		b.SetR(6, DataBase + 0x103);
		b.EnterArm(CodeBase + 12);
		b.Step();
		GBA_CHECK_EQ(b.Peek(DataBase + 0xF8), 0x11111111u);
		GBA_CHECK_EQ(b.Peek(DataBase + 0xFC), 0x22222222u);
		GBA_CHECK_EQ(b.R(6), DataBase + 0xFB);

		// LDMIA from the unaligned base - 5 reads the same words and, unlike LDR, does not
		// rotate them (gba-tests arm 508).
		b.Arm(CodeBase + 16, { Enc::BlockTransfer(Enc::AL, true, false, true, false, false, 7, 0x18) });
		b.SetR(7, DataBase + 0xFB);
		b.EnterArm(CodeBase + 16);
		b.Step();
		GBA_CHECK_EQ(b.R(3), 0x11111111u);
		GBA_CHECK_EQ(b.R(4), 0x22222222u);
	}

	GBA_TEST(Cpu, BadTestOpRestoresCpsr)
	{
		// The ARM7TDMI decodes TST/TEQ/CMP/CMN with S set and r15 in the (unused) destination
		// field as the CPSR-restoring form of a data processing instruction: the SPSR becomes the
		// CPSR and the bank switches, but no PC is written, so execution carries on in the
		// restored mode (gba-tests arm 234). With no SPSR to restore - System or User mode -
		// nothing happens (arm 235).
		Bench b;
		b.ResetCpu();
		b.EnterArm(CodeBase);
		b.SetR(8, 0x32);						// the User/System bank's r8
		b.Cpu().SwitchMode(ModeFiq);
		b.SetR(8, 0x64);						// FIQ has a copy of its own

		// SPSR_fiq = System mode with Z set, so that the restore is visible in the flags too.
		b.Arm(CodeBase, { Enc::MsrReg(Enc::AL, true, 0xF, 0) });
		b.SetR(0, (u32)ModeSystem | FlagZ);
		b.EnterArm(CodeBase);
		b.Step();
		GBA_CHECK(b.Cpu().Mode() == ModeFiq);

		// 0xE15FF000 is CMP r15, r0 with r15 in the destination field.
		b.Arm(CodeBase + 4, { 0xE15FF000u });
		b.SetR(0, 0x80000000);
		b.SetNzcv(0);
		b.EnterArm(CodeBase + 4);
		b.Step();
		GBA_CHECK(b.Cpu().Mode() == ModeSystem);			// the SPSR was restored
		GBA_CHECK_EQ(b.R(8), 0x32u);						// ... with the User bank
		GBA_CHECK_EQ(b.Nzcv(), ZBit);						// the flags came from the SPSR
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), CodeBase + 8);	// and the PC was not written

		// In System mode there is no SPSR, so the "bad CMP" is harmless.
		b.Arm(CodeBase + 8, { 0xE15FF000u });
		b.EnterArm(CodeBase + 8);
		b.Step();
		GBA_CHECK(b.Cpu().Mode() == ModeSystem);
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), CodeBase + 12);
	}

	// ---------------------------------------------------------------------------------------
	// Branches
	// ---------------------------------------------------------------------------------------

	GBA_TEST(Cpu, BranchAndLink)
	{
		Bench b;
		b.EnterArm(CodeBase);
		u32 at = CodeBase;

		// B to CodeBase + 0x20: the offset is relative to the instruction plus 8, so the raw
		// byte offset is 0x18.
		b.Arm(at, { Enc::Branch(Enc::AL, false, 0x18) });
		b.EnterArm(at);
		GBA_CHECK_EQ(b.Step(), 3);			// 2S + 1N
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), CodeBase + 0x20);

		// BL backwards: from CodeBase + 0x20 back to CodeBase, with LR left on the instruction
		// after the BL. The offset is (target - (address + 8)) = -0x28.
		at = CodeBase + 0x20;
		b.Arm(at, { Enc::Branch(Enc::AL, true, -0x28) });
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), CodeBase);
		GBA_CHECK_EQ(b.R(14), CodeBase + 0x24);

		// A conditional branch that is not taken does nothing but the fetch.
		at = CodeBase + 0x40;
		b.Arm(at, { Enc::Branch(Enc::EQ, false, 0x10) });
		b.EnterArm(at);
		b.SetNzcv(0);						// Z clear, so EQ fails
		GBA_CHECK_EQ(b.Step(), 1);
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), CodeBase + 0x44);
	}

	GBA_TEST(Cpu, BranchExchange)
	{
		// BX is the ARMv4T interworking instruction: bit 0 of the target is the new T bit.
		Bench b;
		b.Thumb(CodeBase + 8, { Enc::ThumbMovCmpAddSub(0, 0, 0x42) });	// MOV r0, #0x42
		b.Arm(CodeBase, { Enc::Bx(Enc::AL, 0) });
		b.SetR(0, (CodeBase + 8) | 1u);
		b.EnterArm(CodeBase);
		GBA_CHECK_EQ(b.Step(), 3);
		GBA_CHECK(b.Cpu().ThumbState());
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), CodeBase + 8);
		GBA_CHECK_EQ(b.R(15), CodeBase + 12);		// the Thumb pipeline value

		b.Step();
		GBA_CHECK_EQ(b.R(0), 0x42u);

		// ... and back to ARM state.
		b.Thumb(CodeBase + 16, { Enc::ThumbHi(3, 1, 0) });		// BX r1
		b.EnterThumb(CodeBase + 16);
		b.SetR(1, CodeBase + 0x80);
		b.Step();
		GBA_CHECK(!b.Cpu().ThumbState());
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), CodeBase + 0x80);
	}

	// ---------------------------------------------------------------------------------------
	// PSR transfers and the modes
	// ---------------------------------------------------------------------------------------

	GBA_TEST(Cpu, MrsMsr)
	{
		Bench b;
		b.EnterArm(CodeBase);
		u32 at = CodeBase;

		// MRS r0, CPSR returns the live CPSR.
		b.Arm(at, { Enc::Mrs(Enc::AL, false, 0) });
		b.SetNzcv(NBit | ZBit | CBit | VBit);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), b.Cpu().ReadCPSR());

		// MSR CPSR_f, r1 replaces the condition flags only.
		at += 4;
		b.Arm(at, { Enc::MsrReg(Enc::AL, false, 0x8, 1) });
		b.SetR(1, 0x50000000);				// Z and V
		b.SetNzcv(NBit);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK_EQ(b.Nzcv(), ZBit | VBit);
		GBA_CHECK(b.Cpu().Mode() == ModeSupervisor);

		// MSR CPSR_c, r1 changes the mode, and the banked registers change with it.
		at += 4;
		b.Arm(at, { Enc::MsrReg(Enc::AL, false, 0x1, 1) });
		b.SetR(13, 0x11111111);				// the Supervisor stack pointer
		b.SetR(1, (u32)ModeIrq);
		b.EnterArm(at);
		b.Step();
		GBA_CHECK(b.Cpu().Mode() == ModeIrq);
		GBA_CHECK_EQ(b.R(13), 0u);			// the IRQ bank's stack is still the reset value
		b.SetR(13, 0x22222222);
		b.Cpu().SwitchMode(ModeSupervisor);
		GBA_CHECK_EQ(b.R(13), 0x11111111u);
	}

	GBA_TEST(Cpu, UndefinedInstruction)
	{
		Bench b;
		b.InstallBios(VectorUndefined, Enc::DpReg(Enc::AL, Enc::MOV, true, 0, 15, 14));	// MOVS PC, LR
		b.Arm(CodeBase, { 0xE7F000F0 });	// LDR/STR register space with bit 4 set: undefined
		b.EnterArm(CodeBase);

		u32 before = b.Cpu().ReadCPSR();
		b.Step();

		GBA_CHECK(b.Cpu().Mode() == ModeUndefined);
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), VectorUndefined);
		GBA_CHECK_EQ(b.Cpu().Reg(14), CodeBase + 4);	// LR_und points past the instruction
		GBA_CHECK_EQ(b.Cpu().ReadSPSR(), before);
		GBA_CHECK((b.Cpu().ReadCPSR() & FlagI) != 0);
		GBA_CHECK_EQ(b.Cpu().UndefinedInstructions(), 1ull);

		// MOVS PC, LR returns and puts the CPSR back.
		b.Step();
		GBA_CHECK(b.Cpu().Mode() == ModeSupervisor);
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), CodeBase + 4);
		GBA_CHECK_EQ(b.Cpu().ReadCPSR(), before);
	}

	GBA_TEST(Cpu, CoprocessorUndefined)
	{
		// The GBA has no coprocessor: LDC/STC, CDP and MCR/MRC all take the undefined vector.
		const u32 words[] = { 0xEC000000u /*LDC*/, 0xEE000000u /*CDP*/, 0xEE010010u /*MCR*/ };

		for (u32 word : words)
		{
			Bench b;
			b.Arm(CodeBase, { word });
			b.EnterArm(CodeBase);
			b.Step();
			GBA_CHECK(b.Cpu().Mode() == ModeUndefined);
			GBA_CHECK_EQ(b.Cpu().CurrentPC(), VectorUndefined);
			GBA_CHECK_EQ(b.Cpu().UndefinedInstructions(), 1ull);
		}
	}

	GBA_TEST(Cpu, UndefinedEncodings)
	{
		// The ARMv5-only instructions and the Thumb encodings ARMv4T leaves undefined must all
		// end in the undefined vector, counted, and never in a crash or a wrong execution.
		//   * 0xE16F0F10 is CLZ (ARMv5), which sits in the data processing "miscellaneous" space
		//     with S = 0 - the encodings ARMv4T does not define there.
		//   * 0xBE00 is the Thumb BKPT (ARMv5).
		//   * 0xDE00 is Thumb format 16 with condition 1110, which the ARM Architecture Reference Manual leaves undefined.
		Bench arm;
		arm.Arm(CodeBase, { 0xE16F0F10 });
		arm.EnterArm(CodeBase);
		arm.Step();
		GBA_CHECK(arm.Cpu().Mode() == ModeUndefined);
		GBA_CHECK_EQ(arm.Cpu().CurrentPC(), VectorUndefined);
		GBA_CHECK_EQ(arm.Cpu().Reg(14), CodeBase + 4);
		GBA_CHECK_EQ(arm.Cpu().UndefinedInstructions(), 1ull);

		const u16 thumbWords[] = { 0xBE00u, 0xDE00u };
		for (u16 word : thumbWords)
		{
			Bench t;
			t.Thumb(CodeBase, { word });
			t.EnterThumb(CodeBase);
			t.Step();
			GBA_CHECK(t.Cpu().Mode() == ModeUndefined);
			GBA_CHECK_EQ(t.Cpu().CurrentPC(), VectorUndefined);
			GBA_CHECK_EQ(t.Cpu().Reg(14), CodeBase + 2);		// just past the halfword
			GBA_CHECK_EQ(t.Cpu().UndefinedInstructions(), 1ull);
		}
	}

	// ---------------------------------------------------------------------------------------
	// Exceptions
	// ---------------------------------------------------------------------------------------

	GBA_TEST(Cpu, IrqRoundTrip)
	{
		Bench b;
		// The handler at the IRQ vector returns with SUBS PC, LR, #4 (ARM Architecture Reference Manual A2.6).
		b.InstallBios(VectorIrq, Enc::DpImm(Enc::AL, Enc::SUB, true, 14, 15, 4));
		b.Arm(CodeBase, { Enc::DpImm(Enc::AL, Enc::MOV, false, 0, 0, 0x11),
			Enc::DpImm(Enc::AL, Enc::MOV, false, 0, 1, 0x22) });
		b.EnterArm(CodeBase);

		b.bus.irq.WriteIE(INT_VBLANK);
		b.bus.irq.Raise(INT_VBLANK);			// writing IF would acknowledge it instead
		b.bus.irq.WriteIME(true);
		b.Cpu().WriteCPSR(b.Cpu().ReadCPSR() & ~FlagI);		// let the IRQ in

		u32 before = b.Cpu().ReadCPSR();

		// The interrupt is taken before the first instruction runs.
		GBA_CHECK_EQ(b.Step(), 3);
		GBA_CHECK(b.Cpu().Mode() == ModeIrq);
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), VectorIrq);
		GBA_CHECK_EQ(b.Cpu().Reg(14), CodeBase + 4);		// the instruction that would have run, plus 4
		GBA_CHECK_EQ(b.Cpu().ReadSPSR(), before);
		GBA_CHECK((b.Cpu().ReadCPSR() & FlagI) != 0);
		GBA_CHECK_EQ(b.R(0), 0u);							// nothing was executed

		// SUBS PC, LR, #4 resumes where the interrupt happened and restores the CPSR.
		b.Step();
		GBA_CHECK(b.Cpu().Mode() == ModeSupervisor);
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), CodeBase);
		GBA_CHECK_EQ(b.Cpu().ReadCPSR(), before);

		// The acknowledge drops the request, so the re-enabled IRQ does not fire again.
		b.bus.irq.WriteIF(INT_VBLANK);
		GBA_CHECK_EQ(b.Step(), 1);
		GBA_CHECK_EQ(b.R(0), 0x11u);
	}

	GBA_TEST(Cpu, IrqWithImeClearDoesNotFire)
	{
		Bench b;
		b.Arm(CodeBase, { Enc::DpImm(Enc::AL, Enc::MOV, false, 0, 0, 0x11) });
		b.EnterArm(CodeBase);
		b.bus.irq.WriteIE(INT_VBLANK);
		b.bus.irq.Raise(INT_VBLANK);
		b.bus.irq.WriteIME(false);							// IME clear: the IRQ is not taken
		b.Cpu().WriteCPSR(b.Cpu().ReadCPSR() & ~FlagI);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0x11u);
		GBA_CHECK(b.Cpu().Mode() == ModeSupervisor);
	}

	GBA_TEST(Cpu, FiqException)
	{
		// FIQ is implemented although the GBA never raises it (GBATEK: the core has it, the
		// board does not use it). It banks r8-r12 as well as r13/r14.
		Bench b;
		b.ResetCpu();
		b.EnterArm(CodeBase);
		b.SetR(8, 0x11111111);
		b.SetR(13, 0x03008000);
		b.Cpu().WriteCPSR(b.Cpu().ReadCPSR() & ~FlagF);
		u32 before = b.Cpu().ReadCPSR();

		b.Cpu().Exception(VectorFiq, ModeFiq, FlagI | FlagF);

		GBA_CHECK(b.Cpu().Mode() == ModeFiq);
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), VectorFiq);
		GBA_CHECK_EQ(b.Cpu().Reg(14), CodeBase + 4);
		GBA_CHECK_EQ(b.Cpu().ReadSPSR(), before);
		GBA_CHECK((b.Cpu().ReadCPSR() & FlagI) != 0);
		GBA_CHECK((b.Cpu().ReadCPSR() & FlagF) != 0);
		GBA_CHECK_EQ(b.R(8), 0u);				// the FIQ bank is a fresh set
		b.SetR(8, 0x22222222);
		b.Cpu().SwitchMode(ModeSupervisor);
		GBA_CHECK_EQ(b.R(8), 0x11111111u);		// the shared bank is untouched
	}

	GBA_TEST(Cpu, SwiException)
	{
		// With HLE off a SWI is the real exception: the handler runs in Supervisor mode with
		// LR_svc just past the SWI and returns with MOVS PC, LR (ARM Architecture Reference Manual A2.6).
		Bench b;
		b.bus.HleBiosEnabled = false;
		b.InstallBios(VectorSwi, Enc::DpReg(Enc::AL, Enc::MOV, true, 0, 15, 14));	// MOVS PC, LR
		b.Arm(CodeBase, { Enc::Swi(Enc::AL, 0x06) });
		b.EnterArm(CodeBase);

		u32 before = b.Cpu().ReadCPSR();
		b.Step();

		GBA_CHECK(b.Cpu().Mode() == ModeSupervisor);
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), VectorSwi);
		GBA_CHECK_EQ(b.Cpu().Reg(14), CodeBase + 4);
		GBA_CHECK_EQ(b.Cpu().ReadSPSR(), before);
		GBA_CHECK((b.Cpu().ReadCPSR() & FlagI) != 0);

		b.Step();
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), CodeBase + 4);
		GBA_CHECK_EQ(b.Cpu().ReadCPSR(), before);
	}

	GBA_TEST(Cpu, SwiHleContract)
	{
		// With HLE on the host handles the call. The contract: the CPU leaves the PC on the
		// return address and leaves the caller's r14 alone (a real BIOS preserves it across the
		// SWI, and a leaf "swi N; bx lr" thunk depends on it), does not bank the mode and does
		// not touch the CPSR; the handler finishes the call itself (here: Div, r0/r1 in, r0/r1
		// out, then BranchTo(PC)). This exercises gba_hlebios.cpp through the real bus, because
		// GbaBus::Swi is not virtual.
		Bench b;
		b.Arm(CodeBase, { Enc::Swi(Enc::AL, 0x06) });		// SWI 0x060000: the BIOS Div call
		b.EnterArm(CodeBase);
		b.SetR(0, 100);
		b.SetR(1, 7);

		u32 before = b.Cpu().ReadCPSR();
		u32 lrBefore = b.R(14);
		b.Step();

		GBA_CHECK_EQ(b.R(0), 14u);					// 100 / 7
		GBA_CHECK_EQ(b.R(1), 2u);					// 100 % 7
		GBA_CHECK_EQ(b.R(14), lrBefore);			// the caller's LR is preserved
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), CodeBase + 4);
		GBA_CHECK_EQ(b.Cpu().ReadCPSR(), before);	// no mode change, no CPSR change
		GBA_CHECK(b.Cpu().Mode() == ModeSupervisor);
	}

	GBA_TEST(Cpu, HaltWakeWithoutIme)
	{
		// GBATEK: HALT stops the clock until IE & IF is non-zero, even when the interrupt is
		// masked - the CPU then just carries on at the PC.
		Bench b;
		b.Arm(CodeBase, { Enc::DpImm(Enc::AL, Enc::MOV, false, 0, 0, 0x11) });
		b.EnterArm(CodeBase);
		b.Cpu().Halt();
		GBA_CHECK(b.Cpu().Halted());

		GBA_CHECK_EQ(b.Step(), 2);					// stopped
		GBA_CHECK(b.Cpu().Halted());
		GBA_CHECK_EQ(b.R(0), 0u);

		b.bus.irq.WriteIE(INT_VBLANK);
		b.bus.irq.Raise(INT_VBLANK);
		b.bus.irq.WriteIME(false);					// masked: the CPU wakes without the exception
		GBA_CHECK_EQ(b.Step(), 1);
		GBA_CHECK(!b.Cpu().Halted());
		GBA_CHECK_EQ(b.R(0), 0x11u);
		GBA_CHECK(b.Cpu().Mode() == ModeSupervisor);
	}

	GBA_TEST(Cpu, HaltWakeTakesIrq)
	{
		// If IME is set and CPSR.I is clear the wake-up takes the interrupt.
		Bench b;
		b.InstallBios(VectorIrq, Enc::DpImm(Enc::AL, Enc::SUB, true, 14, 15, 4));
		b.Arm(CodeBase, { Enc::DpImm(Enc::AL, Enc::MOV, false, 0, 0, 0x11) });
		b.EnterArm(CodeBase);
		b.bus.irq.WriteIE(INT_VBLANK);
		b.bus.irq.Raise(INT_VBLANK);
		b.bus.irq.WriteIME(true);
		b.Cpu().WriteCPSR(b.Cpu().ReadCPSR() & ~FlagI);
		b.Cpu().Halt();

		GBA_CHECK_EQ(b.Step(), 3);
		GBA_CHECK(!b.Cpu().Halted());
		GBA_CHECK(b.Cpu().Mode() == ModeIrq);
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), VectorIrq);
		GBA_CHECK_EQ(b.Cpu().Reg(14), CodeBase + 4);
	}

	// ---------------------------------------------------------------------------------------
	// Thumb: format 1 and 2
	// ---------------------------------------------------------------------------------------

	GBA_TEST(Cpu, ThumbShiftImmediate)
	{
		Bench b;
		b.Thumb(CodeBase, {
			Enc::ThumbShift(Enc::LSL, 1, 0, 1),		// LSL r1, r0, #1
			Enc::ThumbShift(Enc::LSR, 1, 0, 2),		// LSR r2, r0, #1
			Enc::ThumbShift(Enc::ASR, 1, 0, 3),		// ASR r3, r0, #1
		});
		b.EnterThumb(CodeBase);
		b.SetR(0, 0x80000001);

		b.SetNzcv(0);
		b.Step();
		GBA_CHECK_EQ(b.R(1), 0x00000002u);
		GBA_CHECK_EQ(b.Nzcv(), CBit);				// bit 31 was shifted out

		b.Step();
		GBA_CHECK_EQ(b.R(2), 0x40000000u);
		GBA_CHECK_EQ(b.Nzcv(), CBit);				// bit 0 was shifted out

		b.Step();
		GBA_CHECK_EQ(b.R(3), 0xC0000000u);
		GBA_CHECK_EQ(b.Nzcv(), NBit | CBit);
	}

	GBA_TEST(Cpu, ThumbAddSubtract)
	{
		Bench b;
		b.Thumb(CodeBase, {
			Enc::ThumbAddSub(false, false, 1, 0, 2),	// ADD r2, r0, r1
			Enc::ThumbAddSub(true, true, 1, 0, 3),		// SUB r3, r0, #1
			Enc::ThumbAddSub(false, true, 1, 0, 4),		// SUB r4, r0, r1
		});
		b.EnterThumb(CodeBase);
		b.SetR(0, 5);
		b.SetR(1, 3);

		b.SetNzcv(NBit);
		b.Step();
		GBA_CHECK_EQ(b.R(2), 8u);
		GBA_CHECK_EQ(b.Nzcv(), 0u);

		b.Step();
		GBA_CHECK_EQ(b.R(3), 4u);
		GBA_CHECK_EQ(b.Nzcv(), CBit);				// no borrow

		b.Step();
		GBA_CHECK_EQ(b.R(4), 2u);
		GBA_CHECK_EQ(b.Nzcv(), CBit);
	}

	// ---------------------------------------------------------------------------------------
	// Thumb: format 3, 4 and 5
	// ---------------------------------------------------------------------------------------

	GBA_TEST(Cpu, ThumbMovCmpAddSub)
	{
		Bench b;
		b.Thumb(CodeBase, {
			Enc::ThumbMovCmpAddSub(0, 0, 0x42),		// MOV r0, #0x42
			Enc::ThumbMovCmpAddSub(1, 0, 0x42),		// CMP r0, #0x42
			Enc::ThumbMovCmpAddSub(2, 1, 0x10),		// ADD r1, #0x10
			Enc::ThumbMovCmpAddSub(3, 1, 0x08),		// SUB r1, #0x08
		});
		b.EnterThumb(CodeBase);
		b.SetR(1, 1);
		b.SetNzcv(NBit | CBit);

		b.Step();
		GBA_CHECK_EQ(b.R(0), 0x42u);
		GBA_CHECK_EQ(b.Nzcv(), CBit);				// MOV leaves C and V alone

		b.Step();
		GBA_CHECK_EQ(b.Nzcv(), ZBit | CBit);		// 0x42 - 0x42: equal, no borrow

		b.Step();
		GBA_CHECK_EQ(b.R(1), 0x11u);

		b.Step();
		GBA_CHECK_EQ(b.R(1), 9u);
		GBA_CHECK_EQ(b.Nzcv(), CBit);
	}

	GBA_TEST(Cpu, ThumbAluOperations)
	{
		// All sixteen operations of format 4, encoded as "op Rd, Rs" (Rd is the destination and
		// also the first operand; for the shifts its low byte is the amount).
		Bench b;
		b.EnterThumb(CodeBase);
		u32 at = CodeBase;

		struct Case { u32 op; int rs; int rd; u32 r0; u32 r1; u32 r2; u32 initial; u32 expected; u32 nzcv; };
		const Case cases[] =
		{
			// AND/EOR/ORR/BIC/MVN leave C alone (the LSL #0 shifter carry), so an initial C of 0
			// comes back out as 0.
			{ 0x0, 0, 1, 0xFF00FF00u, 0x0F0F0F0Fu, 0, 0, 0x0F000F00u, 0 },		// AND r1, r0
			{ 0x1, 0, 1, 0xFF00FF00u, 0x0F0F0F0Fu, 0, 0, 0xF00FF00Fu, NBit },	// EOR r1, r0
			{ 0x2, 0, 1, 0x00000004u, 0x00000001u, 0, 0, 0x00000010u, 0 },		// LSL r1, r0
			{ 0x3, 0, 1, 0x00000004u, 0x80000000u, 0, 0, 0x08000000u, 0 },		// LSR r1, r0
			{ 0x4, 0, 1, 0x00000004u, 0x80000000u, 0, 0, 0xF8000000u, NBit },	// ASR r1, r0
			{ 0x5, 0, 1, 0x00000000u, 0x00000000u, 0, CBit, 0x00000001u, 0 },	// ADC with C set
			{ 0x6, 0, 1, 0x00000001u, 0x00000000u, 0, CBit, 0xFFFFFFFFu, NBit },// SBC with C set
			{ 0x7, 0, 1, 0x00000004u, 0x80000001u, 0, 0, 0x18000000u, 0 },		// ROR r1, r0
			{ 0x9, 0, 1, 0x00000005u, 0x00000000u, 0, 0, 0xFFFFFFFBu, NBit },	// NEG r1, r0
			{ 0xC, 0, 1, 0xFF00FF00u, 0x000000FFu, 0, 0, 0xFF00FFFFu, NBit },	// ORR r1, r0
			{ 0xD, 0, 2, 0x00000003u, 0x00000000u, 0x00000005u, 0, 0x0000000Fu, 0 },	// MUL r2, r1
			{ 0xE, 0, 1, 0xFF00FF00u, 0x0F0F0F0Fu, 0, 0, 0x000F000Fu, 0 },		// BIC r1, r0
			{ 0xF, 0, 1, 0xFF00FF00u, 0x00000000u, 0, 0, 0x00FF00FFu, 0 },		// MVN r1, r0
		};

		for (const Case& c : cases)
		{
			b.Thumb(at, { Enc::ThumbAlu(c.op, c.rs, c.rd) });
			b.SetR(0, c.r0);
			b.SetR(1, c.r1);
			b.SetR(2, c.r2);
			b.SetNzcv(c.initial);
			b.EnterThumb(at);
			b.Step();
			GBA_CHECK_EQ(b.R(c.rd), c.expected);
			GBA_CHECK_EQ(b.Nzcv(), c.nzcv);
			at += 2;
		}

		// The test operations: TST and CMP set the flags and write nothing.
		b.Thumb(at, { Enc::ThumbAlu(0x8, 0, 1) });					// TST r1, r0
		b.SetR(0, 0x0F);
		b.SetR(1, 0xF0);
		b.SetNzcv(CBit);
		b.EnterThumb(at);
		b.Step();
		GBA_CHECK_EQ(b.R(1), 0xF0u);
		GBA_CHECK_EQ(b.Nzcv(), ZBit | CBit);						// 0xF0 & 0x0F == 0

		at += 2;
		b.Thumb(at, { Enc::ThumbAlu(0xA, 0, 1) });					// CMP r1, r0
		b.SetR(0, 1);
		b.SetR(1, 0);
		b.SetNzcv(0);
		b.EnterThumb(at);
		b.Step();
		GBA_CHECK_EQ(b.R(1), 0u);
		GBA_CHECK_EQ(b.Nzcv(), NBit);								// 0 - 1 borrows

		at += 2;
		b.Thumb(at, { Enc::ThumbAlu(0xB, 0, 1) });					// CMN r1, r0
		b.SetR(0, 1);
		b.SetR(1, 0xFFFFFFFF);
		b.SetNzcv(0);
		b.EnterThumb(at);
		b.Step();
		GBA_CHECK_EQ(b.Nzcv(), ZBit | CBit);						// wraps to zero with a carry
	}

	GBA_TEST(Cpu, ThumbHiRegisterAndBx)
	{
		Bench b;
		b.EnterThumb(CodeBase);
		u32 at = CodeBase;

		// ADD r8, r0 (no flags).
		b.Thumb(at, { Enc::ThumbHi(0, 0, 8) });
		b.SetR(0, 5);
		b.SetR(8, 0x10);
		b.SetNzcv(NBit);
		b.EnterThumb(at);
		b.Step();
		GBA_CHECK_EQ(b.R(8), 0x15u);
		GBA_CHECK_EQ(b.Nzcv(), NBit);

		// CMP r8, r9 sets all four flags.
		at += 2;
		b.Thumb(at, { Enc::ThumbHi(1, 9, 8) });
		b.SetR(8, 0x10);
		b.SetR(9, 0x10);
		b.SetNzcv(0);
		b.EnterThumb(at);
		b.Step();
		GBA_CHECK_EQ(b.Nzcv(), ZBit | CBit);
		GBA_CHECK_EQ(b.R(8), 0x10u);

		// MOV r0, r9 copies a high register into a low one.
		at += 2;
		b.Thumb(at, { Enc::ThumbHi(2, 9, 0) });
		b.SetR(9, 0xABCDEF01);
		b.EnterThumb(at);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0xABCDEF01u);

		// BX r9 switches to ARM state (the target is even).
		at += 2;
		b.Thumb(at, { Enc::ThumbHi(3, 9, 0) });
		b.SetR(9, CodeBase + 0x100);
		b.EnterThumb(at);
		b.Step();
		GBA_CHECK(!b.Cpu().ThumbState());
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), CodeBase + 0x100);
	}

	// ---------------------------------------------------------------------------------------
	// Thumb: the load/store formats
	// ---------------------------------------------------------------------------------------

	GBA_TEST(Cpu, ThumbPcRelativeLoad)
	{
		Bench b;
		b.Poke(CodeBase + 8, 0x12345678);
		b.Thumb(CodeBase, { Enc::ThumbPcLoad(0, 1) });		// LDR r0, [PC, #4]
		b.EnterThumb(CodeBase);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0x12345678u);
	}

	GBA_TEST(Cpu, ThumbLoadStoreRegister)
	{
		Bench b;
		b.Poke(DataBase, 0xAABBCCDD);
		b.Poke8(DataBase + 4, 0x7E);
		b.Thumb(CodeBase, {
			Enc::ThumbLdrStrReg(true, false, 1, 0, 2),		// LDR r2, [r0, r1]
			Enc::ThumbLdrStrReg(true, true, 1, 0, 3),		// LDRB r3, [r0, r1] (r1 = 4)
			Enc::ThumbLdrStrReg(false, true, 1, 0, 4),		// STRB r4, [r0, r1]
		});
		b.EnterThumb(CodeBase);
		b.SetR(0, DataBase);
		b.SetR(1, 0);
		b.Step();
		GBA_CHECK_EQ(b.R(2), 0xAABBCCDDu);

		b.SetR(1, 4);
		b.Step();
		GBA_CHECK_EQ(b.R(3), 0x7Eu);

		b.SetR(4, 0x1234);
		b.Step();
		GBA_CHECK_EQ(b.Peek8(DataBase + 4), 0x34u);
	}

	GBA_TEST(Cpu, ThumbLoadStoreSignExtend)
	{
		Bench b;
		b.Poke16(DataBase, 0x8001);
		b.Poke8(DataBase + 4, 0x80);
		b.Thumb(CodeBase, {
			Enc::ThumbLdrStrSign(true, false, 1, 0, 2),		// LDRH r2, [r0, r1]
			Enc::ThumbLdrStrSign(true, true, 1, 0, 3),		// LDRSH r3, [r0, r1]
			Enc::ThumbLdrStrSign(false, true, 1, 0, 4),		// LDRSB r4, [r0, r1]
			Enc::ThumbLdrStrSign(false, false, 1, 0, 5),	// STRH r5, [r0, r1]
		});
		b.EnterThumb(CodeBase);
		b.SetR(0, DataBase);
		b.SetR(1, 0);

		b.Step();
		GBA_CHECK_EQ(b.R(2), 0x8001u);

		b.Step();
		GBA_CHECK_EQ(b.R(3), 0xFFFF8001u);

		b.SetR(1, 4);
		b.Step();
		GBA_CHECK_EQ(b.R(4), 0xFFFFFF80u);

		b.SetR(5, 0x0000BEEF);
		b.Step();
		GBA_CHECK_EQ(b.Peek16(DataBase + 4), 0xBEEFu);
	}

	GBA_TEST(Cpu, ThumbLoadStoreImmediate)
	{
		Bench b;
		b.Poke(DataBase + 8, 0x55667788);
		b.Poke8(DataBase + 2, 0x99);
		b.Thumb(CodeBase, {
			Enc::ThumbLdrStrImm(false, true, 2, 0, 1),		// LDR r1, [r0, #8]
			Enc::ThumbLdrStrImm(true, true, 2, 0, 2),		// LDRB r2, [r0, #2]
			Enc::ThumbLdrStrImm(true, false, 3, 0, 3),		// STRB r3, [r0, #3]
		});
		b.EnterThumb(CodeBase);
		b.SetR(0, DataBase);

		b.Step();
		GBA_CHECK_EQ(b.R(1), 0x55667788u);

		b.Step();
		GBA_CHECK_EQ(b.R(2), 0x99u);

		b.SetR(3, 0x1234);
		b.Step();
		GBA_CHECK_EQ(b.Peek8(DataBase + 3), 0x34u);
	}

	GBA_TEST(Cpu, ThumbLoadStoreHalfword)
	{
		Bench b;
		b.Poke16(DataBase + 6, 0xABCD);
		b.Thumb(CodeBase, {
			Enc::ThumbLdrStrHalf(true, 3, 0, 1),		// LDRH r1, [r0, #6]
			Enc::ThumbLdrStrHalf(false, 4, 0, 2),		// STRH r2, [r0, #8]
		});
		b.EnterThumb(CodeBase);
		b.SetR(0, DataBase);
		b.Step();
		GBA_CHECK_EQ(b.R(1), 0xABCDu);

		b.SetR(2, 0x1234);
		b.Step();
		GBA_CHECK_EQ(b.Peek16(DataBase + 8), 0x1234u);
	}

	GBA_TEST(Cpu, ThumbStackRelativeAndLoadAddress)
	{
		Bench b;
		b.Thumb(CodeBase, {
			Enc::ThumbSpRel(false, 0, 1),			// STR r0, [SP, #4]
			Enc::ThumbSpRel(true, 1, 1),			// LDR r1, [SP, #4]
			Enc::ThumbLoadAddress(false, 2, 2),		// ADD r2, PC, #8
			Enc::ThumbLoadAddress(true, 3, 1),		// ADD r3, SP, #4
		});
		b.EnterThumb(CodeBase);
		b.SetR(0, 0x12345678);
		b.SetR(13, StackTop);

		b.Step();
		GBA_CHECK_EQ(b.Peek(StackTop + 4), 0x12345678u);

		b.Step();
		GBA_CHECK_EQ(b.R(1), 0x12345678u);

		b.Step();
		GBA_CHECK_EQ(b.R(2), CodeBase + 0x10);		// ADD r2, PC, #8 with the instruction at +4

		b.Step();
		GBA_CHECK_EQ(b.R(3), StackTop + 4);
	}

	GBA_TEST(Cpu, ThumbAddOffsetToSp)
	{
		Bench b;
		b.Thumb(CodeBase, {
			Enc::ThumbAddSp(true, 2),				// SUB SP, #8
			Enc::ThumbAddSp(false, 1),				// ADD SP, #4
		});
		b.EnterThumb(CodeBase);
		b.SetR(13, StackTop);
		b.Step();
		GBA_CHECK_EQ(b.R(13), StackTop - 8);
		b.Step();
		GBA_CHECK_EQ(b.R(13), StackTop - 4);
	}

	GBA_TEST(Cpu, ThumbPushPopAndMultipleTransfer)
	{
		Bench b;
		b.Thumb(CodeBase, {
			Enc::ThumbPushPop(false, true, 0x05),	// PUSH {r0, r2, LR}
			Enc::ThumbPushPop(true, true, 0x05),	// POP {r0, r2, PC}
		});
		b.EnterThumb(CodeBase);
		b.SetR(13, StackTop);
		b.SetR(0, 0x11111111);
		b.SetR(2, 0x22222222);
		b.SetR(14, (CodeBase + 0x40) | 1u);

		b.Step();
		GBA_CHECK_EQ(b.Peek(StackTop - 12), 0x11111111u);
		GBA_CHECK_EQ(b.Peek(StackTop - 8), 0x22222222u);
		GBA_CHECK_EQ(b.Peek(StackTop - 4), (CodeBase + 0x40) | 1u);
		GBA_CHECK_EQ(b.R(13), StackTop - 12);

		b.SetR(0, 0);
		b.SetR(2, 0);
		b.Step();
		GBA_CHECK_EQ(b.R(0), 0x11111111u);
		GBA_CHECK_EQ(b.R(2), 0x22222222u);
		GBA_CHECK_EQ(b.R(13), StackTop);
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), CodeBase + 0x40);	// POP {PC} branched (staying in Thumb)
		GBA_CHECK(b.Cpu().ThumbState());

		// LDMIA/STMIA with a base register.
		Bench m;
		m.Poke(DataBase, 0x01010101);
		m.Poke(DataBase + 4, 0x02020202);
		m.Thumb(CodeBase, {
			Enc::ThumbMultiple(true, 0, 0x06),		// LDMIA r0!, {r1, r2}
			Enc::ThumbMultiple(false, 0, 0x18),		// STMIA r0!, {r3, r4}
		});
		m.EnterThumb(CodeBase);
		m.SetR(0, DataBase);
		m.Step();
		GBA_CHECK_EQ(m.R(1), 0x01010101u);
		GBA_CHECK_EQ(m.R(2), 0x02020202u);
		GBA_CHECK_EQ(m.R(0), DataBase + 8);
		m.SetR(3, 0x33333333);
		m.SetR(4, 0x44444444);
		m.Step();
		GBA_CHECK_EQ(m.Peek(DataBase + 8), 0x33333333u);
		GBA_CHECK_EQ(m.Peek(DataBase + 12), 0x44444444u);
		GBA_CHECK_EQ(m.R(0), DataBase + 16);
	}

	// ---------------------------------------------------------------------------------------
	// Thumb: the branches and SWI
	// ---------------------------------------------------------------------------------------

	GBA_TEST(Cpu, ThumbConditionalBranch)
	{
		Bench b;
		// BEQ +4 halfwords: from CodeBase the target is CodeBase + 4 + 8.
		b.Thumb(CodeBase, { Enc::ThumbCondBranch(Enc::EQ, 4) });
		b.EnterThumb(CodeBase);

		b.SetNzcv(ZBit);
		GBA_CHECK_EQ(b.Step(), 3);
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), CodeBase + 12);

		b.SetNzcv(0);
		b.EnterThumb(CodeBase);
		GBA_CHECK_EQ(b.Step(), 1);
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), CodeBase + 2);

		// A backwards branch: BNE -6 halfwords from CodeBase + 8 lands back on CodeBase.
		b.Thumb(CodeBase + 8, { Enc::ThumbCondBranch(Enc::NE, -6) });
		b.EnterThumb(CodeBase + 8);
		b.SetNzcv(0);
		b.Step();
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), CodeBase);
	}

	GBA_TEST(Cpu, ThumbUnconditionalBranch)
	{
		Bench b;
		// B +8 halfwords from CodeBase + 4 + 16.
		b.Thumb(CodeBase, { Enc::ThumbBranch(8) });
		b.EnterThumb(CodeBase);
		GBA_CHECK_EQ(b.Step(), 3);
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), CodeBase + 20);
	}

	GBA_TEST(Cpu, ThumbLongBranchWithLink)
	{
		// BL is two halfwords. The ARM7TDMI writes (address of the BL + 4) | 1 to LR - the
		// address just past the pair - which is what "bx lr" needs to return.
		Bench b;
		b.Thumb(CodeBase, { Enc::ThumbBlFirst(0), Enc::ThumbBlSecond(0x0E) });	// BL +0x1C
		b.EnterThumb(CodeBase);

		b.Step();								// the first halfword
		GBA_CHECK_EQ(b.R(14), CodeBase + 4);
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), CodeBase + 2);

		b.Step();								// the second halfword
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), CodeBase + 0x20);
		GBA_CHECK_EQ(b.R(14), (CodeBase + 4) | 1u);
		GBA_CHECK(b.Cpu().ThumbState());

		// The return: BX LR jumps back to the instruction after the BL.
		b.Thumb(CodeBase + 0x20, { Enc::ThumbHi(3, 14, 0) });	// BX LR
		b.EnterThumb(CodeBase + 0x20);
		b.Step();
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), CodeBase + 4);
		GBA_CHECK(b.Cpu().ThumbState());
	}

	GBA_TEST(Cpu, ThumbSwiException)
	{
		// With HLE off the Thumb SWI takes the real exception: LR_svc points at the next
		// halfword (ARM Architecture Reference Manual A2.6) and the handler runs in ARM state.
		Bench b;
		b.bus.HleBiosEnabled = false;
		b.Thumb(CodeBase, { Enc::ThumbSwiCall(0x06) });
		b.EnterThumb(CodeBase);

		u32 before = b.Cpu().ReadCPSR();
		b.Step();

		GBA_CHECK(b.Cpu().Mode() == ModeSupervisor);
		GBA_CHECK(!b.Cpu().ThumbState());					// T is cleared on entry
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), VectorSwi);
		GBA_CHECK_EQ(b.Cpu().Reg(14), CodeBase + 2);		// just past the two-byte SWI
		GBA_CHECK_EQ(b.Cpu().ReadSPSR(), before);			// ... and the SPSR remembers Thumb
		GBA_CHECK((b.Cpu().ReadSPSR() & FlagT) != 0);
	}

	GBA_TEST(Cpu, ThumbSwiHle)
	{
		// A Thumb BIOS call handled in the host: the comment is the SWI's own byte, the call
		// finishes in the caller's mode, the CPU stays in Thumb state and the caller's LR is
		// preserved (a real BIOS leaves it alone).
		Bench b;
		b.Thumb(CodeBase, { Enc::ThumbSwiCall(0x06) });		// swi 6: the same BIOS Div call
		b.EnterThumb(CodeBase);
		b.SetR(0, 100);
		b.SetR(1, 7);

		u32 before = b.Cpu().ReadCPSR();
		u32 lrBefore = b.R(14);
		b.Step();

		GBA_CHECK_EQ(b.R(0), 14u);
		GBA_CHECK_EQ(b.R(1), 2u);
		GBA_CHECK_EQ(b.R(14), lrBefore);
		GBA_CHECK_EQ(b.Cpu().CurrentPC(), CodeBase + 2);
		GBA_CHECK(b.Cpu().ThumbState());
		GBA_CHECK_EQ(b.Cpu().ReadCPSR(), before);
	}

	// ---------------------------------------------------------------------------------------
	// Timing
	// ---------------------------------------------------------------------------------------

	GBA_TEST(Cpu, CycleCounts)
	{
		// The ARM7TDMI data sheet's S/N/I table, one clock per cycle.
		Bench b;
		b.Poke(DataBase, 0x00000000);
		b.EnterArm(CodeBase);
		u32 at = CodeBase;

		// MOV: 1S.
		b.Arm(at, { Enc::DpImm(Enc::AL, Enc::MOV, false, 0, 0, 1) });
		b.EnterArm(at);
		GBA_CHECK_EQ(b.Step(), 1);

		// A data processing instruction shifted by a register: 1S + 1I.
		at += 4;
		b.Arm(at, { Enc::DpReg(Enc::AL, Enc::ADD, false, 0, 0, 1, Enc::LSL, 0, 2) });
		b.EnterArm(at);
		GBA_CHECK_EQ(b.Step(), 2);

		// STR: 2N; LDR: 1S + 1N + 1I.
		at += 4;
		b.Arm(at, { Enc::LdrStr(Enc::AL, false, false, 0, 1, 0) });
		b.SetR(1, DataBase);
		b.EnterArm(at);
		GBA_CHECK_EQ(b.Step(), 2);

		at += 4;
		b.Arm(at, { Enc::LdrStr(Enc::AL, true, false, 0, 1, 0) });
		b.SetR(1, DataBase);
		b.EnterArm(at);
		GBA_CHECK_EQ(b.Step(), 3);

		// LDM of three registers: 1S + 1N + 2S + 1I = 5; STM of three: 2N + 2S = 4.
		at += 4;
		b.Arm(at, { Enc::BlockTransfer(Enc::AL, true, false, true, false, false, 1, 0x0E) });
		b.SetR(1, DataBase);
		b.EnterArm(at);
		GBA_CHECK_EQ(b.Step(), 5);

		at += 4;
		b.Arm(at, { Enc::BlockTransfer(Enc::AL, false, false, true, false, false, 1, 0x0E) });
		b.SetR(1, DataBase);
		b.EnterArm(at);
		GBA_CHECK_EQ(b.Step(), 4);

		// B/BL/SWI: 2S + 1N.
		at += 4;
		b.Arm(at, { Enc::Branch(Enc::AL, false, 8) });
		b.EnterArm(at);
		GBA_CHECK_EQ(b.Step(), 3);

		// MUL with a multiplier that needs four blocks costs 1S + 4I.
		at += 4;
		b.Arm(at, { Enc::Multiply(Enc::AL, false, false, 0, 1, 2) });
		b.SetR(1, 0x12345678);
		b.SetR(2, 0x9ABCDEF0);
		b.EnterArm(at);
		GBA_CHECK_EQ(b.Step(), 5);

		// A Thumb instruction is one sequential fetch.
		Bench t;
		t.Thumb(CodeBase, { Enc::ThumbMovCmpAddSub(0, 0, 1) });
		t.EnterThumb(CodeBase);
		GBA_CHECK_EQ(t.Step(), 1);
	}
}
