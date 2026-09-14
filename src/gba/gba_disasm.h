// The GBA's instruction disassembler: ARM (A32) and Thumb (T16) for the ARM7TDMI.
//
// It exists for the same reason the emulator itself exists: to read what a program does from the
// manufacturer's documentation rather than from somebody else's source. It is the debugging tool
// the board brings with it - the harness lists a BIOS or a cartridge, and the trace mode prints the
// instructions a machine actually executed - so it is written from the ARM Architecture Reference
// Manual (ARM DDI 0100E), sections A3 (ARM), A4 (Thumb) and the ARM7TDMI data sheet's instruction
// summary.
//
// The decoder is a tree over the encoding bits: every instruction is one entry point below, and the
// operand formatting helpers (register lists, shifts, addresses) are shared. An encoding the ARM ARM
// marks as undefined comes back as "undef", and one this build does not decode as "db <bytes>", so
// nothing is ever silently wrong.

#pragma once

#include "gba_types.h"

#include <string>

namespace GBA
{
	/// <summary>The instruction stream a disassembly reads from: a live machine (the bus) or a
	/// flat image (a BIOS or a cartridge file). The disassembler itself never touches memory.</summary>
	class DisasmMemory
	{
	public:
		virtual ~DisasmMemory() = default;

		/// <summary>The halfword at `address`. Both ARM and Thumb are made of halfwords, so one
		/// accessor is enough; an unmapped address should read as the open bus, not throw.</summary>
		virtual u16 Read16(u32 address) const = 0;

		/// <summary>The byte at `address`, which is what the Game Boy's instruction stream is made
		/// of. The default reads it out of the halfword, so an implementation only has to provide
		/// the halfword form.</summary>
		virtual u8 Read8(u32 address) const { return (u8)(Read16(address) & 0xFF); }
	};

	/// <summary>A flat image with a base address, e.g. a BIOS (base 0) or a cartridge (base
	/// 0x08000000).</summary>
	class ImageMemory : public DisasmMemory
	{
	public:
		ImageMemory(const u8* data, size_t size, u32 base);
		u16 Read16(u32 address) const override;

	private:
		const u8* data;
		size_t size;
		u32 base;
	};

	/// <summary>One ARM instruction: the mnemonic with its operands, in the ARM assembler syntax
	/// the manual uses. `size` (when given) receives 4 for an ARM instruction.</summary>
	std::string ArmDisassemble(const DisasmMemory& memory, u32 address, int* size = nullptr);

	/// <summary>One Thumb instruction; `size` receives 2 or 4.</summary>
	std::string ThumbDisassemble(const DisasmMemory& memory, u32 address, int* size = nullptr);

	/// <summary>Whichever of the two the CPU state asks for.</summary>
	std::string Disassemble(const DisasmMemory& memory, u32 address, bool thumb, int* size = nullptr);

	/// <summary>The raw bytes of one instruction, as hexadecimal ("E1A09002" or "2001"), which the
	/// listings print in front of the mnemonic.</summary>
	std::string InstructionBytes(const DisasmMemory& memory, u32 address, int size);

	/// <summary>The textual form of the CPSR's condition flags for a trace line.</summary>
	std::string ConditionFlags(u32 cpsr);
}
