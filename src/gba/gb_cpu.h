// The LR35902 (Sharp SM83) interpreter: the CPU of the Game Boy and the Game Boy Color.
//
// Written from the Pan Docs (gbdev.io/pandocs: "CPU Instruction Set", "CPU Registers and Flags",
// "Interrupts", "halt", "Reducing Power Consumption") and the RGBDS gbz80(7) opcode reference,
// which spells out the flag rules of every instruction including the odd ones (ADD SP,e8 and
// LD HL,SP+e8 set H and C from the low byte, DAA's N/H behaviour, the rotates that leave Z alone,
// BIT leaving C alone, and the opcodes the table does not give a mnemonic to).
//
// The CPU is deliberately bus-agnostic: it owns its registers and asks a Memory interface for the
// bus. That keeps the instruction decoding testable on its own (the unit tests hand it a flat
// memory) while the real machine plugs the address decoder in behind it.
//
// Timing is counted in M-cycles: one M-cycle is four of the Game Boy's 4.194304 MHz clocks, and
// every instruction takes a whole number of them (1 M-cycle for a register operation, 5 for the
// interrupt dispatch). Step() returns the number of M-cycles the instruction took; the machine
// multiplies it by four (two in CGB double speed) to get the clock cycles the rest of the system
// advances by.
//
// One deliberate deviation: the eleven opcodes the Pan Docs call invalid (0xD3, 0xDB, 0xDD,
// 0xE3, 0xE4, 0xEB, 0xEC, 0xED, 0xF4, 0xFC, 0xFD), which hard-lock a real CPU, are treated as
// one M-cycle no-ops with a warning instead of hanging the emulator. Legal software never
// executes them.

#pragma once

#include "gba_types.h"

#include <string>

namespace GBA
{
	// ---------------------------------------------------------------------------------------
	// The memory interface the CPU drives. The bus implements it; the tests implement it with a
	// flat 64 KByte array, which is enough to check every instruction without a PPU.
	// ---------------------------------------------------------------------------------------

	class GbCpuBus
	{
	public:
		virtual ~GbCpuBus() = default;

		virtual u8 ReadByte(u16 address) = 0;
		virtual void WriteByte(u16 address, u8 value) = 0;

		/// <summary>True when an enabled interrupt is pending (IE &amp; IF != 0).</summary>
		virtual bool InterruptsPending() const = 0;
	};

	// ---------------------------------------------------------------------------------------
	// The flag register (the low byte of AF). Pan Docs "CPU Registers and Flags".
	//
	// The names carry the Gb prefix because both cores of this module (the GBA's ARM7TDMI and
	// this one) live in namespace GBA, and the ARM core already has FlagZ/FlagN/FlagC.
	// ---------------------------------------------------------------------------------------

	enum GbFlagBits : u8
	{
		GbFlagZ = 0x80,			// zero
		GbFlagN = 0x40,			// subtraction (BCD)
		GbFlagH = 0x20,			// half carry (BCD)
		GbFlagC = 0x10,			// carry
		GbFlagMask = 0xF0,		// the low nibble of F always reads back as zero
	};

	class GbCpu
	{
	public:
		// -- state -------------------------------------------------------------------------

		u8 a = 0x00, f = 0x00;
		u8 b = 0x00, c = 0x00, d = 0x00, e = 0x00, h = 0x00, l = 0x00;
		u16 sp = 0xFFFE;
		u16 pc = 0x0100;

		/// <summary>The interrupt master enable (not readable by the program).</summary>
		bool ime = false;

		/// <summary>True while the CPU is in HALT.</summary>
		bool halted = false;

		/// <summary>True while the CPU is in STOP.</summary>
		bool stopped = false;

		/// <summary>
		/// Set when HALT executed with IME = 0 while an interrupt was already pending (Pan Docs
		/// "halt"): the next opcode fetch does not advance PC, so the byte after the HALT is read
		/// a second time and the instruction there runs twice.
		/// </summary>
		bool haltBug = false;

		/// <summary>Set by EI and RETI: IME becomes true after the *next* instruction.</summary>
		bool imePending = false;

		/// <summary>The CGB double-speed mode (KEY1 bit 7). The machine keeps the CPU's own
		/// M-cycle count meaningful by halving the cycles it hands to the rest of the system.</summary>
		bool doubleSpeed = false;

		// -- the machine's hooks --------------------------------------------------------------

		/// <summary>The bus. Never null once Reset() has run.</summary>
		GbCpuBus* bus = nullptr;

		/// <summary>Called when the CPU executes STOP: the machine decides between the CGB
		/// speed switch and the DMG's low-power standby. Returns true when STOP ended
		/// immediately (a speed switch), false when the CPU should wait in STOP mode.</summary>
		bool (*OnStop)(void* user) = nullptr;
		void* stopUser = nullptr;

		// -- operation ---------------------------------------------------------------------

		void Reset();

		/// <summary>Run one instruction (or serve one interrupt) and return the M-cycles it took.</summary>
		int Step();

		/// <summary>True when the CPU is doing nothing and cannot make progress (HALT or STOP).</summary>
		bool Sleeping() const { return halted || stopped; }

		/// <summary>Push a byte/word onto the stack, as PUSH and CALL do (SP decrements first).</summary>
		void PushByte(u8 value);
		void PushWord(u16 value);
		u8 PopByte();
		u16 PopWord();

		/// <summary>The 16-bit register pairs, for the tests and the boot ROM hand-off.</summary>
		u16 AF() const { return (u16)((a << 8) | f); }
		u16 BC() const { return (u16)((b << 8) | c); }
		u16 DE() const { return (u16)((d << 8) | e); }
		u16 HL() const { return (u16)((h << 8) | l); }
		void SetAF(u16 value) { a = (u8)(value >> 8); f = (u8)(value & GbFlagMask); }
		void SetBC(u16 value) { b = (u8)(value >> 8); c = (u8)value; }
		void SetDE(u16 value) { d = (u8)(value >> 8); e = (u8)value; }
		void SetHL(u16 value) { h = (u8)(value >> 8); l = (u8)value; }

		/// <summary>The register state the boot ROM hands to the cartridge (Pan Docs): A holds the
		/// console/cartridge kind, F the flags, B/C/D/E the header bytes, HL the header checksum.
		/// `fValue` is 0xB0 on a DMG for a normal cartridge (Z=1, N=0, H and C set because the
		/// header checksum is non-zero) and 0x80 on a CGB (Z=1 and nothing else).</summary>
		void LoadPostBootRegisters(u8 aValue, u8 headerChecksum, u8 fValue = 0xB0);

		/// <summary>A one line dump of the registers (used by the failure messages).</summary>
		std::string Describe() const;

		// -- flag helpers, for the tests -----------------------------------------------------

		bool Flag(u8 flag) const { return (f & flag) != 0; }
		void SetFlag(u8 flag, bool set) { f = (u8)(set ? (f | flag) : (f & ~flag)); f &= GbFlagMask; }

	private:
		// -- fetching and the opcode dispatch ------------------------------------------------

		u8 Fetch8();
		u16 Fetch16();

		u8 Read(u16 address) { return bus->ReadByte(address); }
		void Write(u16 address, u8 value) { bus->WriteByte(address, value); }

		/// <summary>The eight r8 operands, addressed by their encoding (6 is (HL)).</summary>
		u8 ReadR8(int index);
		void WriteR8(int index, u8 value);

		/// <summary>Serve the highest priority pending interrupt. Returns the M-cycles (5).</summary>
		int ServiceInterrupt(u8 pending);

		void Execute(u8 opcode, int& cycles);
		void ExecuteCb(int& cycles);

		// -- the arithmetic, each following one row of the gbz80(7) tables -------------------

		void AddA(u8 value, bool withCarry);
		void SubA(u8 value, bool withCarry, bool store);
		void AndA(u8 value);
		void XorA(u8 value);
		void OrA(u8 value);
		void IncR8(int index);
		void DecR8(int index);
		void AddHLR16(u16 value);
		void AddSPe8(s8 offset);
		void LdHLSPe8(s8 offset);
		void Daa();

		/// <summary>The CB-prefixed shift/rotate/swap family, one operation per case.</summary>
		u8 ShiftOp(int operation, u8 value);

		/// <summary>Set the Z flag from a result and clear N/H; the shape of every CB result.</summary>
		void SetZFromResult(u8 result);
	};
}
