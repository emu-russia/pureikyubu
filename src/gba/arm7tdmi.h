// ARM7TDMI (ARM v4T) interpreter.
//
// Written from the ARM Architecture Reference Manual (ARM DDI 0100E, chapters 3 and 4) and the
// ARM7TDMI data sheet. The core is a plain interpreter: it decodes one instruction per Step and
// charges the cycle counts the ARM7TDMI timing model describes (S/N/I cycles per memory access,
// extra cycles for the register-list and the long multiplies).
//
// What the CPU owns:
//   * the ARM and Thumb decoders and every data processing, load/store, block transfer,
//     multiply, branch and coprocessor-space instruction;
//   * the seven CPU modes with their banked r13/r14 and r8-r12 (FIQ), and both the CPSR and the
//     banked SPSRs;
//   * the exceptions (reset, undefined, SWI, prefetch/data abort, IRQ, FIQ), including the
//     return instructions that restore the CPSR from an SPSR (BX with the S bit, LDM with the
//     caret, MOVS PC, LDR with the S bit, SUBS PC, LR);
//   * HALT, which the GBA BIOS service uses (the CPU resumes when an interrupt is requested,
//     whether or not it is enabled in the CPSR).
//
// What it does not own: the memory. Every fetch and data access goes through GbaBus, which also
// counts the waitstates, so the CPU's own cycle count is the part that does not depend on the
// cartridge (the bus adds its cycles when the system advances time).

#pragma once

#include "gba_types.h"

namespace GBA
{
	class GbaBus;

	// CPSR bits
	enum : u32
	{
		FlagN = 1u << 31,
		FlagZ = 1u << 30,
		FlagC = 1u << 29,
		FlagV = 1u << 28,
		FlagI = 1u << 7,
		FlagF = 1u << 6,
		FlagT = 1u << 5,
		FlagMask = 0xF0000000u,
	};

	class Arm7tdmi
	{
	public:
		/// <summary>The result of one Step.</summary>
		struct StepResult
		{
			int cycles = 1;			// CPU cycles (the bus adds the waitstates itself)
			bool halted = false;	// the CPU is in HALT and executed nothing
		};

		explicit Arm7tdmi(GbaBus* bus);

		// -- lifecycle ---------------------------------------------------------------------

		/// <summary>Power-on reset: supervisor mode, ARM state, interrupts disabled, PC = 0.</summary>
		void Reset();

		/// <summary>Fetch and execute one instruction. Returns the cycles it took.</summary>
		int Step();

		/// <summary>Enter HALT: nothing is executed until an interrupt is requested.</summary>
		void Halt() { halted = true; }

		/// <summary>Leave HALT (an interrupt request or a reset wakes the CPU up).</summary>
		void Wake() { halted = false; }

		/// <summary>True while the core is suspended by HALTCNT (the BIOS's Halt and IntrWait): it
		/// resumes when an enabled interrupt is requested (GBATEK "HALT/STOP"). A trace uses this to
		/// show a halted stretch as one line instead of repeating the same address thousands of
		/// times, which is what makes the trace readable for a program that waits.</summary>
		bool Halted() const { return halted; }

		// -- registers ---------------------------------------------------------------------

		/// <summary>The current register file (mode-banked), index 0..15.</summary>
		u32 Reg(int index) const;

		/// <summary>Write a register of the *current* mode.</summary>
		void SetReg(int index, u32 value);

		/// <summary>The address of the instruction being executed (PC is r15 + 8 in ARM / + 4 in Thumb).</summary>
		u32 CurrentPC() const { return currentPC; }

		/// <summary>Force the PC (the low bit selects the Thumb state, as BX does).</summary>
		void BranchTo(u32 address) { BranchInternal(address); }

		u32 ReadCPSR() const { return cpsr; }
		void WriteCPSR(u32 value) { cpsr = value & 0xF00000FFu | (cpsr & 0x0FFFFF00u & ~0xFF); cpsr = value; }

		/// <summary>The SPSR of the current mode (0 when the mode has none).</summary>
		u32 ReadSPSR() const;

		bool ThumbState() const { return (cpsr & FlagT) != 0; }

		CpuMode Mode() const { return (CpuMode)(cpsr & ModeMask); }

		/// <summary>Switch the mode, banking r13/r14/r8-r12 as the hardware does.</summary>
		void SwitchMode(CpuMode mode);

		// -- exceptions --------------------------------------------------------------------

		/// <summary>Take the exception for `vector`, entering `mode` with the given CPSR mask.</summary>
		void Exception(u32 vector, CpuMode mode, u32 cpsrMask);

		/// <summary>True while the CPU is inside an exception handler that has no SPSR waiting.</summary>
		bool InException() const { return Mode() != ModeUser && Mode() != ModeSystem; }

		// -- statistics (used by the tests and the harness) --------------------------------

		u64 RetiredInstructions() const { return retired; }
		void ResetStatistics() { retired = 0; }

		/// <summary>Number of instructions the decoder did not recognize as valid (they take the
		/// undefined exception, so a non-zero count usually means a bug in the ROM or in us).</summary>
		u64 UndefinedInstructions() const { return undefinedCount; }

	private:
		GbaBus* bus = nullptr;

		// Mode-banked registers. Index 0..6 are the seven modes of CPSR[4:0] in the order
		// User/System, FIQ, IRQ, Supervisor, Abort, Undefined, ... The ARM7TDMI has 31 registers:
		// r0-r7 and r15 are shared, the rest are banked.
		//
		// bank[0] is the User/System set, which every mode uses for r8-r12 except FIQ.
		u32 regs[16]{};				// the current window (a copy of the active bank)
		u32 bankR13[7]{};			// r13 (SP) per mode
		u32 bankR14[7]{};			// r14 (LR) per mode
		u32 bankR8_12[2][5]{};		// r8-r12: [0] = normal, [1] = FIQ
		u32 bankSPSR[6]{};			// SPSR per exception mode
		u32 cpsr = 0;
		u32 currentPC = 0;

		bool halted = false;
		u64 retired = 0;
		u64 undefinedCount = 0;

		int bankIndex(CpuMode mode) const;
		CpuMode nextMode(u32 cpsrBits) const;

		// Bank the registers out of `regs` into the arrays of the current mode and back in for
		// `mode` (the two halves of SwitchMode).
		void StoreBank();
		void LoadBank(CpuMode mode);

		// Pipeline helpers: the PC value an instruction sees.
		u32 ReadRegFor(int index) const;
		void BranchInternal(u32 address);

		void SetFlag(u32 flag, bool set) { if (set) cpsr |= flag; else cpsr &= ~flag; }

		// -- decoders ----------------------------------------------------------------------

		int StepArm();
		int StepThumb();

		// Data processing (ARM and Thumb share the ALU core).
		u32 ShiftOperand(u32 value, u32 type, u32 amount, bool& carry);
		// The same, for a shift whose amount comes from a register: an amount of 0 there is a
		// shift by nothing that leaves C alone, where the immediate encodings mean "32" (LSR and
		// ASR) or RRX (ROR). See arm7tdmi.cpp and ARM Architecture Reference Manual A5.1.1.
		u32 ShiftOperandByRegister(u32 value, u32 type, u32 amount, bool& carry);
		bool ConditionPassed(u32 condition) const;
		u32 AddWithCarry(u32 a, u32 b, u32 carryIn, bool& carry, bool& overflow);
		void SetLogicFlags(u32 result, bool carry);
		void SetArithFlags(u32 result, bool carry, bool overflow);

		u32 ArmDataProcessing(u32 opcode);
		u32 ArmLoadStore(u32 opcode);
		u32 ArmBlockTransfer(u32 opcode);
		u32 ArmBranch(u32 opcode);
		u32 ArmMultiply(u32 opcode);
		u32 ArmHalfwordTransfer(u32 opcode);
		u32 ArmSwi(u32 opcode);
		u32 ArmCoprocessor(u32 opcode);

		u32 ThumbShiftImmediate(u16 opcode);
		u32 ThumbAddSubtract(u16 opcode);
		u32 ThumbMovCmpAddSub(u16 opcode);
		u32 ThumbAluOperation(u16 opcode);
		u32 ThumbHiRegister(u16 opcode);
		u32 ThumbPcRelativeLoad(u16 opcode);
		u32 ThumbLoadStoreReg(u16 opcode);
		u32 ThumbLoadStoreSignExtend(u16 opcode);
		u32 ThumbLoadStoreImmediate(u16 opcode);
		u32 ThumbLoadStoreHalfword(u16 opcode);
		u32 ThumbLoadStoreSpRelative(u16 opcode);
		u32 ThumbLoadAddress(u16 opcode);
		u32 ThumbAddOffsetToSp(u16 opcode);
		u32 ThumbPushPop(u16 opcode);
		u32 ThumbMultipleTransfer(u16 opcode);
		u32 ThumbConditionalBranch(u16 opcode);
		u32 ThumbSwi(u16 opcode);
		u32 ThumbUnconditionalBranch(u16 opcode);
		u32 ThumbLongBranchWithLink(u16 opcode);

		// Shared memory helpers with the ARM rotation and alignment rules.
		u32 LoadWord(u32 address, bool& aligned);
		u32 LoadHalfword(u32 address);
		u32 LoadByte(u32 address);

		// The number of S cycles a register list transfer takes (ARM7TDMI timing model).
		int BlockTransferCycles(int count, bool thumb);
	};
}
