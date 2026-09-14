// The Game Boy's instruction disassembler: the SM83 (the LR35902 the DMG and CGB run).
//
// It is the Game Boy half of the module's debugging tool (see gba_disasm.h for the ARM half): the
// harness lists a cartridge's code with it and can trace what the machine executed. It is written
// from the SM83 opcode map the Pan Docs reproduce ("CPU Instruction Set" and "CPU Opcode Table"),
// which is what the emulator itself is written from - undocumented opcodes are printed as "db", not
// guessed at.

#pragma once

#include "gba_disasm.h"

#include <string>

namespace GBA
{
	/// <summary>One SM83 instruction, in the assembler syntax of the Pan Docs tables. `size`
	/// (when given) receives 1, 2 or 3 bytes; a DB-prefixed instruction is one byte.</summary>
	std::string GbDisassemble(const DisasmMemory& memory, u16 address, int* size = nullptr);

	/// <summary>The raw bytes of one instruction, as hexadecimal ("3E 01" reads as "3E01").</summary>
	std::string GbInstructionBytes(const DisasmMemory& memory, u16 address, int size);
}
