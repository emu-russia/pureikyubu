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
	enum : uint32_t
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
		uint32_t Reg(int index) const;

		/// <summary>Write a register of the *current* mode.</summary>
		void SetReg(int index, uint32_t value);

		/// <summary>The address of the instruction being executed (PC is r15 + 8 in ARM / + 4 in Thumb).</summary>
		uint32_t CurrentPC() const { return currentPC; }

		/// <summary>Force the PC (the low bit selects the Thumb state, as BX does).</summary>
		void BranchTo(uint32_t address) { BranchInternal(address); }

		uint32_t ReadCPSR() const { return cpsr; }
		void WriteCPSR(uint32_t value) { cpsr = value & 0xF00000FFu | (cpsr & 0x0FFFFF00u & ~0xFF); cpsr = value; }

		/// <summary>The SPSR of the current mode (0 when the mode has none).</summary>
		uint32_t ReadSPSR() const;

		bool ThumbState() const { return (cpsr & FlagT) != 0; }

		CpuMode Mode() const { return (CpuMode)(cpsr & ModeMask); }

		/// <summary>Switch the mode, banking r13/r14/r8-r12 as the hardware does.</summary>
		void SwitchMode(CpuMode mode);

		// -- exceptions --------------------------------------------------------------------

		/// <summary>Take the exception for `vector`, entering `mode` with the given CPSR mask.</summary>
		void Exception(uint32_t vector, CpuMode mode, uint32_t cpsrMask);

		/// <summary>True while the CPU is inside an exception handler that has no SPSR waiting.</summary>
		bool InException() const { return Mode() != ModeUser && Mode() != ModeSystem; }

		// -- statistics (used by the tests and the harness) --------------------------------

		uint64_t RetiredInstructions() const { return retired; }
		void ResetStatistics() { retired = 0; }

		/// <summary>Number of instructions the decoder did not recognize as valid (they take the
		/// undefined exception, so a non-zero count usually means a bug in the ROM or in us).</summary>
		uint64_t UndefinedInstructions() const { return undefinedCount; }

	private:
		GbaBus* bus = nullptr;

		// Mode-banked registers. Index 0..6 are the seven modes of CPSR[4:0] in the order
		// User/System, FIQ, IRQ, Supervisor, Abort, Undefined, ... The ARM7TDMI has 31 registers:
		// r0-r7 and r15 are shared, the rest are banked.
		//
		// bank[0] is the User/System set, which every mode uses for r8-r12 except FIQ.
		uint32_t regs[16]{};			// the current window (a copy of the active bank)
		uint32_t bankR13[7]{};			// r13 (SP) per mode
		uint32_t bankR14[7]{};			// r14 (LR) per mode
		uint32_t bankR8_12[2][5]{};		// r8-r12: [0] = normal, [1] = FIQ
		uint32_t bankSPSR[6]{};			// SPSR per exception mode
		uint32_t cpsr = 0;
		uint32_t currentPC = 0;

		bool halted = false;
		uint64_t retired = 0;
		uint64_t undefinedCount = 0;

		int bankIndex(CpuMode mode) const;
		CpuMode nextMode(uint32_t cpsrBits) const;

		// Bank the registers out of `regs` into the arrays of the current mode and back in for
		// `mode` (the two halves of SwitchMode).
		void StoreBank();
		void LoadBank(CpuMode mode);

		// Pipeline helpers: the PC value an instruction sees.
		uint32_t ReadRegFor(int index) const;
		void BranchInternal(uint32_t address);

		void SetFlag(uint32_t flag, bool set) { if (set) cpsr |= flag; else cpsr &= ~flag; }

		// -- decoders ----------------------------------------------------------------------

		int StepArm();
		int StepThumb();

		// Data processing (ARM and Thumb share the ALU core).
		uint32_t ShiftOperand(uint32_t value, uint32_t type, uint32_t amount, bool& carry);
		// The same, for a shift whose amount comes from a register: an amount of 0 there is a
		// shift by nothing that leaves C alone, where the immediate encodings mean "32" (LSR and
		// ASR) or RRX (ROR). See arm7tdmi.cpp and ARM Architecture Reference Manual A5.1.1.
		uint32_t ShiftOperandByRegister(uint32_t value, uint32_t type, uint32_t amount, bool& carry);
		bool ConditionPassed(uint32_t condition) const;
		uint32_t AddWithCarry(uint32_t a, uint32_t b, uint32_t carryIn, bool& carry, bool& overflow);
		void SetLogicFlags(uint32_t result, bool carry);
		void SetArithFlags(uint32_t result, bool carry, bool overflow);

		uint32_t ArmDataProcessing(uint32_t opcode);
		uint32_t ArmLoadStore(uint32_t opcode);
		uint32_t ArmBlockTransfer(uint32_t opcode);
		uint32_t ArmBranch(uint32_t opcode);
		uint32_t ArmMultiply(uint32_t opcode);
		uint32_t ArmHalfwordTransfer(uint32_t opcode);
		uint32_t ArmSwi(uint32_t opcode);
		uint32_t ArmCoprocessor(uint32_t opcode);

		uint32_t ThumbShiftImmediate(uint16_t opcode);
		uint32_t ThumbAddSubtract(uint16_t opcode);
		uint32_t ThumbMovCmpAddSub(uint16_t opcode);
		uint32_t ThumbAluOperation(uint16_t opcode);
		uint32_t ThumbHiRegister(uint16_t opcode);
		uint32_t ThumbPcRelativeLoad(uint16_t opcode);
		uint32_t ThumbLoadStoreReg(uint16_t opcode);
		uint32_t ThumbLoadStoreSignExtend(uint16_t opcode);
		uint32_t ThumbLoadStoreImmediate(uint16_t opcode);
		uint32_t ThumbLoadStoreHalfword(uint16_t opcode);
		uint32_t ThumbLoadStoreSpRelative(uint16_t opcode);
		uint32_t ThumbLoadAddress(uint16_t opcode);
		uint32_t ThumbAddOffsetToSp(uint16_t opcode);
		uint32_t ThumbPushPop(uint16_t opcode);
		uint32_t ThumbMultipleTransfer(uint16_t opcode);
		uint32_t ThumbConditionalBranch(uint16_t opcode);
		uint32_t ThumbSwi(uint16_t opcode);
		uint32_t ThumbUnconditionalBranch(uint16_t opcode);
		uint32_t ThumbLongBranchWithLink(uint16_t opcode);

		// Shared memory helpers with the ARM rotation and alignment rules.
		uint32_t LoadWord(uint32_t address, bool& aligned);
		uint32_t LoadHalfword(uint32_t address);
		uint32_t LoadByte(uint32_t address);

		// The number of S cycles a register list transfer takes (ARM7TDMI timing model).
		int BlockTransferCycles(int count, bool thumb);
	};
}
