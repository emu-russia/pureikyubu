// ARM7TDMI (ARM v4T) interpreter.
//
// Written from the ARM Architecture Reference Manual (ARM DDI 0100E): chapter 3 "The ARM
// instruction set", chapter 4 "The Thumb instruction set" and A2.6 for the exceptions, and from
// the ARM7TDMI data sheet for the S/N/I cycle counts and for HALT. GBATEK supplies the parts the
// GBA adds around the core: there are no coprocessors (so LDC/STC/MCR/MRC/CDP and the ARMv5
// instructions are all undefined here), the SWI comment field carries the BIOS function number,
// and HALT/STOP stop the core until an interrupt is *requested*.
//
// Conventions used throughout this file:
//
//   * `currentPC` is the address of the instruction being executed, so while a decoder runs
//     Reg(15) is the pipeline value: currentPC + 8 in ARM state and currentPC + 4 in Thumb state
//     (ARM Architecture Reference Manual A3.4.1 and 4.5). Every decoder leaves currentPC on the next instruction to run:
//     the branches (and the loads into r15) overwrite it, everything else advances it by its own
//     size at the end.
//   * the value a decoder returns is the core's *own* cycle count. The bus charges the waitstates
//     of the region while it serves a fetch or a data access, so the two together are the time
//     the instruction really takes on the GBA. N, S and I cycles each take one clock on the
//     ARM7TDMI, so a cycle count is just the number of those cycles in the data sheet's table.
//   * a decoder that cannot decode what it was handed raises the undefined exception, counts it
//     in `undefinedCount` and never crashes: the GBA has no trap the host could fall into.

#include "arm7tdmi.h"
#include "gba_bus.h"

namespace GBA
{
	namespace
	{
		// -----------------------------------------------------------------------------------
		// Small helpers
		// -----------------------------------------------------------------------------------

		/// <summary>ARM Architecture Reference Manual A3.4.2: rotate right, with 0 meaning "no rotation" (not a shift by 32).</summary>
		inline u32 RotateRight(u32 value, u32 amount)
		{
			amount &= 31;
			if (amount == 0)
				return value;
			return (value >> amount) | (value << (32 - amount));
		}

		/// <summary>Sign extend the low `bits` bits of `value` (the branch and offset fields).</summary>
		inline u32 SignExtend(u32 value, int bits)
		{
			u32 sign = 1u << (bits - 1);
			return (value ^ sign) - sign;
		}

		/// <summary>The number of registers a load/store multiple list names.</summary>
		inline int CountRegisters(u32 list)
		{
			int count = 0;
			while (list != 0)
			{
				list &= list - 1;
				count++;
			}
			return count;
		}

		/// <summary>True when the CPSR's C bit is set (1 for the carry-in of ADC/SBC/RSC).</summary>
		inline u32 CarryIn(u32 cpsr)
		{
			return (cpsr & FlagC) ? 1u : 0u;
		}

		/// <summary>
		/// ARM7TDMI data sheet, "MUL/MLA": the multiplier is examined in 8-bit blocks, so a
		/// multiplier that fits in eight bits costs one internal cycle and a full 32-bit one four.
		/// </summary>
		inline int MultiplyCycles(u32 multiplier)
		{
			if ((multiplier & 0xFFFFFF00u) == 0 || (multiplier & 0xFFFFFF00u) == 0xFFFFFF00u)
				return 1;
			if ((multiplier & 0xFFFF0000u) == 0 || (multiplier & 0xFFFF0000u) == 0xFFFF0000u)
				return 2;
			if ((multiplier & 0xFF000000u) == 0 || (multiplier & 0xFF000000u) == 0xFF000000u)
				return 3;
			return 4;
		}

		/// <summary>
		/// ARM Architecture Reference Manual A2.6 ("Exception return"): an instruction that loads the CPSR from an SPSR -
		/// MOVS PC, LR; SUBS PC, LR, #4; LDM with the caret and r15; LDR with the S bit - goes
		/// back to the mode and the state the SPSR describes and branches to `address`.
		/// </summary>
		void ReturnFromException(Arm7tdmi& cpu, u32 address)
		{
			if (!cpu.InException())
			{
				// ARM Architecture Reference Manual A4.1.5: the S bit with r15 has no SPSR to restore in User or System
				// mode and is UNPREDICTABLE there; behave like a plain branch.
				GBA::Log(LogLevel::Warn, "arm7tdmi: CPSR restore from an SPSR in User/System mode");
				cpu.BranchTo(address & ~1u);
				return;
			}

			u32 spsr = cpu.ReadSPSR();
			CpuMode mode = (CpuMode)(spsr & ModeMask);

			// Bank the handler's registers out and the restored mode's in, then take the whole
			// SPSR as the CPSR (N, Z, C and V included) and branch, honouring its T bit.
			cpu.SwitchMode(mode);
			cpu.WriteCPSR(spsr);
			cpu.BranchTo((spsr & FlagT) ? (address | 1u) : (address & ~1u));
		}
	}

	// ---------------------------------------------------------------------------------------
	// Lifecycle
	// ---------------------------------------------------------------------------------------

	Arm7tdmi::Arm7tdmi(GbaBus* bus) : bus(bus)
	{
	}

	void Arm7tdmi::Reset()
	{
		// ARM Architecture Reference Manual A2.6 and the ARM7TDMI data sheet, "Exceptions": reset enters Supervisor mode
		// in ARM state with both interrupt masks set and starts fetching at the reset vector.
		// Every banked register is cleared: the ARM7TDMI has 31 registers, and the ones reset
		// does not have banked in are cleared on real hardware too.
		memset(regs, 0, sizeof(regs));
		memset(bankR13, 0, sizeof(bankR13));
		memset(bankR14, 0, sizeof(bankR14));
		memset(bankR8_12, 0, sizeof(bankR8_12));
		memset(bankSPSR, 0, sizeof(bankSPSR));

		cpsr = (u32)ModeSupervisor | FlagI | FlagF;
		currentPC = VectorReset;
		halted = false;
		retired = 0;
		undefinedCount = 0;

		LoadBank(ModeSupervisor);
	}

	int Arm7tdmi::Step()
	{
		if (halted)
		{
			// GBATEK "HALT/STOP": the clock stops until an interrupt is *requested*. The wake
			// up does not need IME or a clear CPSR.I - even a masked request restarts the core,
			// it simply does not take the exception. That is how the BIOS's Halt and IntrWait
			// let a game poll IE/IF itself.
			if ((bus->irq.ReadIE() & bus->irq.ReadIF()) == 0)
				return 2;			// stopped: the bus still sees a couple of cycles go by

			halted = false;
		}

		// ARM Architecture Reference Manual A2.6: the IRQ is taken between instructions, before the next one runs, when
		// IME is set (Irq::Pending), the cause is enabled in IE and CPSR.I is clear. currentPC
		// is then the instruction that would have run, which is also the return address.
		if (bus->irq.Pending() && (cpsr & FlagI) == 0)
		{
			Exception(VectorIrq, ModeIrq, FlagI);
			retired++;
			return 3;				// 2S + 1N: the vector fetch and the pipeline refill
		}

		retired++;
		return ThumbState() ? StepThumb() : StepArm();
	}

	// ---------------------------------------------------------------------------------------
	// Registers and modes
	// ---------------------------------------------------------------------------------------

	int Arm7tdmi::bankIndex(CpuMode mode) const
	{
		// CPSR[4:0] selects the bank. User and System mode share one set of r13/r14 and r8-r12
		// (the ARM Architecture Reference Manual calls the System bank "the User bank"), so both map to slot 0. The slots
		// are: 0 User/System, 1 FIQ, 2 IRQ, 3 Supervisor, 4 Abort, 5 Undefined.
		switch (mode)
		{
		case ModeFiq: return 1;
		case ModeIrq: return 2;
		case ModeSupervisor: return 3;
		case ModeAbort: return 4;
		case ModeUndefined: return 5;
		default: return 0;			// ModeUser, ModeSystem and any reserved encoding
		}
	}

	CpuMode Arm7tdmi::nextMode(u32 cpsrBits) const
	{
		return (CpuMode)(cpsrBits & ModeMask);
	}

	void Arm7tdmi::StoreBank()
	{
		int index = bankIndex(Mode());
		bankR13[index] = regs[13];
		bankR14[index] = regs[14];

		// r8-r12 are banked for FIQ only; every other mode uses the User bank's copy.
		int high = (index == 1) ? 1 : 0;
		for (int i = 0; i < 5; i++)
			bankR8_12[high][i] = regs[8 + i];
	}

	void Arm7tdmi::LoadBank(CpuMode mode)
	{
		int index = bankIndex(mode);
		regs[13] = bankR13[index];
		regs[14] = bankR14[index];

		int high = (index == 1) ? 1 : 0;
		for (int i = 0; i < 5; i++)
			regs[8 + i] = bankR8_12[high][i];
	}

	void Arm7tdmi::SwitchMode(CpuMode mode)
	{
		// The two halves of a mode change: the current window goes back into its bank and the
		// new mode's bank becomes the window. The rest of the CPSR is left alone here; MSR and
		// the exception entry write those bits themselves.
		StoreBank();
		cpsr = (cpsr & ~ModeMask) | ((u32)mode & ModeMask);
		LoadBank(mode);
	}

	u32 Arm7tdmi::Reg(int index) const
	{
		// ARM Architecture Reference Manual A3.4.1 and 4.5: an instruction that reads r15 sees the address of the
		// instruction being executed plus 8 (ARM state, three-stage pipeline) or plus 4
		// (Thumb state, where the instruction is two bytes wide).
		if (index == 15)
			return currentPC + (ThumbState() ? 4u : 8u);
		return regs[index & 15];
	}

	void Arm7tdmi::SetReg(int index, u32 value)
	{
		if (index == 15)
		{
			// A write of r15 from the outside (the BIOS hooks, the debugger, a test) is a
			// branch, so it interworks the way BX does.
			BranchInternal(value);
			return;
		}
		regs[index & 15] = value;
	}

	u32 Arm7tdmi::ReadRegFor(int index) const
	{
		return Reg(index);
	}

	void Arm7tdmi::BranchInternal(u32 address)
	{
		// ARM Architecture Reference Manual A2.3.4 (interworking) and A4.1.4 (BX): bit 0 of the target is the new T bit
		// and is cleared from the address.
		currentPC = address & ~1u;
		SetFlag(FlagT, (address & 1) != 0);
	}

	u32 Arm7tdmi::ReadSPSR() const
	{
		int index = bankIndex(Mode());
		if (index == 0)
			return 0;				// User and System mode have no SPSR
		return bankSPSR[index - 1];
	}

	void Arm7tdmi::Exception(u32 vector, CpuMode mode, u32 cpsrMask)
	{
		// ARM Architecture Reference Manual A2.6 ("Exception entry") and the ARM7TDMI data sheet:
		//
		//   * the SPSR of the mode entered takes the current CPSR;
		//   * r14 of that mode takes the return address. For the interrupts (and a prefetch
		//     abort) that is the address of the instruction that would have run next plus 4,
		//     because the handler returns with SUBS PC, LR, #4; for a SWI or an undefined
		//     instruction it is the address just past the instruction, so the handler returns
		//     with MOVS PC, LR and carries on;
		//   * the mode bits change, T is cleared - exception handlers run in ARM state on the
		//     ARM7TDMI, which is why the SPSR's T bit is what brings Thumb back on the return -
		//     and the mask bits (I, plus F for FIQ and reset) are set. N, Z, C and V survive.
		//
		// The synchronous exceptions report the instruction that caused them and the interrupts
		// report the instruction that would have run: currentPC is exactly that on entry.
		u32 oldCpsr = cpsr;
		u32 returnAddress = currentPC + 4;
		if (vector == VectorSwi || vector == VectorUndefined)
			returnAddress = currentPC + (ThumbState() ? 2u : 4u);

		SwitchMode(mode);

		int index = bankIndex(mode);
		if (index != 0)
			bankSPSR[index - 1] = oldCpsr;

		regs[14] = returnAddress;
		cpsr = (oldCpsr & ~(ModeMask | FlagT)) | ((u32)mode & ModeMask) | (cpsrMask & ~ModeMask);
		currentPC = vector;
	}

	// ---------------------------------------------------------------------------------------
	// Flags and the ALU
	// ---------------------------------------------------------------------------------------

	bool Arm7tdmi::ConditionPassed(u32 condition) const
	{
		// ARM Architecture Reference Manual A3.3.2, the condition field. The flags are read as they were before the
		// instruction, which is what the local copies take care of.
		bool n = (cpsr & FlagN) != 0;
		bool z = (cpsr & FlagZ) != 0;
		bool c = (cpsr & FlagC) != 0;
		bool v = (cpsr & FlagV) != 0;

		switch (condition)
		{
		case 0x0: return z;					// EQ
		case 0x1: return !z;				// NE
		case 0x2: return c;					// CS/HS
		case 0x3: return !c;				// CC/LO
		case 0x4: return n;					// MI
		case 0x5: return !n;				// PL
		case 0x6: return v;					// VS
		case 0x7: return !v;				// VC
		case 0x8: return c && !z;			// HI
		case 0x9: return !c || z;			// LS
		case 0xA: return n == v;			// GE
		case 0xB: return n != v;			// LT
		case 0xC: return !z && (n == v);	// GT
		case 0xD: return z || (n != v);		// LE
		default: return true;				// 0xE AL; 0xF is caught as undefined earlier
		}
	}

	u32 Arm7tdmi::ShiftOperand(u32 value, u32 type, u32 amount, bool& carry)
	{
		// ARM Architecture Reference Manual A3.4.2 / A5.2 ("Shift operations"). `carry` carries the C flag in and takes
		// the shifter's carry out; an LSL #0 leaves C alone because it is a plain move. A shift
		// amount field of zero means 32 for LSR and ASR, which is how the ARM Architecture Reference Manual encodes them.
		switch (type & 3)
		{
		case 0:						// LSL
			if (amount == 0)
				return value;
			if (amount < 32)
			{
				carry = ((value >> (32 - amount)) & 1) != 0;
				return value << amount;
			}
			carry = (amount == 32) && ((value & 1) != 0);
			return 0;

		case 1:						// LSR
			if (amount == 0 || amount == 32)
			{
				carry = ((value >> 31) & 1) != 0;
				return 0;
			}
			if (amount < 32)
			{
				carry = ((value >> (amount - 1)) & 1) != 0;
				return value >> amount;
			}
			carry = false;
			return 0;

		case 2:						// ASR
			if (amount == 0 || amount >= 32)
			{
				carry = ((value >> 31) & 1) != 0;
				return (u32)((s32)value >> 31);
			}
			carry = ((value >> (amount - 1)) & 1) != 0;
			return (u32)((s32)value >> amount);

		default:					// ROR (an amount of 0 is RRX)
			if (amount == 0)
			{
				u32 carryIn = carry ? 1u : 0u;
				carry = (value & 1) != 0;
				return (value >> 1) | (carryIn << 31);
			}
			{
				u32 rotate = amount & 31;
				if (rotate == 0)
				{
					carry = ((value >> 31) & 1) != 0;
					return value;
				}
				carry = ((value >> (rotate - 1)) & 1) != 0;
				return RotateRight(value, rotate);
			}
		}
	}

	u32 Arm7tdmi::ShiftOperandByRegister(u32 value, u32 type, u32 amount, bool& carry)
	{
		// ARM Architecture Reference Manual A5.1.1 and A5.2 ("Shift operations"): a shift whose amount comes from a
		// register shifts by exactly the value in that register (its bottom byte), so an amount
		// of 0 passes the operand through unchanged and leaves the C flag alone. That is *not*
		// what the immediate encodings mean by a field of 0 - there it is a shift by 32 for LSR
		// and ASR and RRX for ROR. (gba-tests arm 166, "shift by 0 register value", checks this;
		// the Thumb ALU shifts take their amount from a register too and follow the same rule.)
		if (amount == 0)
			return value;
		return ShiftOperand(value, type, amount, carry);
	}

	u32 Arm7tdmi::AddWithCarry(u32 a, u32 b, u32 carryIn, bool& carry, bool& overflow)
	{
		// ARM Architecture Reference Manual A3.4.1: C is the carry out of bit 31 (an unsigned overflow) and V is the
		// signed overflow of the same addition. A subtraction passes ~b and a carry of 1.
		u64 sum = (u64)a + (u64)b + (u64)(carryIn & 1);
		u32 result = (u32)sum;

		carry = (sum >> 32) != 0;
		overflow = (((a ^ result) & (b ^ result)) >> 31) != 0;
		return result;
	}

	void Arm7tdmi::SetLogicFlags(u32 result, bool carry)
	{
		// N and Z come from the result, C from the shifter, and a logic operation leaves V.
		SetFlag(FlagN, (result & 0x80000000u) != 0);
		SetFlag(FlagZ, result == 0);
		SetFlag(FlagC, carry);
	}

	void Arm7tdmi::SetArithFlags(u32 result, bool carry, bool overflow)
	{
		SetFlag(FlagN, (result & 0x80000000u) != 0);
		SetFlag(FlagZ, result == 0);
		SetFlag(FlagC, carry);
		SetFlag(FlagV, overflow);
	}

	// ---------------------------------------------------------------------------------------
	// Memory helpers
	// ---------------------------------------------------------------------------------------

	u32 Arm7tdmi::LoadWord(u32 address, bool& aligned)
	{
		// The ARM7TDMI data sheet, "Unaligned loads": an LDR from an address that is not word
		// aligned loads the aligned word and rotates it right by eight times the low two bits.
		// (The bus returns the word the address selects; the rotation is the core's.)
		u32 word = bus->Read32(address & ~3u);
		u32 offset = address & 3;
		aligned = (offset == 0);
		return aligned ? word : RotateRight(word, offset * 8);
	}

	u32 Arm7tdmi::LoadHalfword(u32 address)
	{
		// The ARM7TDMI data sheet, "Misaligned halfword accesses", which gba-tests arm 408 and
		// thumb 211/219 pin down: an LDRH from an odd address returns the *aligned* halfword
		// rotated right by eight bits, so it lands in bits 31-24 of the register. (A signed
		// halfword load from an odd address is a different story: the core performs it as a
		// signed byte load, which is why the LDRSH paths do not use this helper.)
		u32 halfword = bus->Read16(address & ~1u);
		if ((address & 1) == 0)
			return halfword;
		return (halfword >> 8) | (halfword << 24);
	}

	u32 Arm7tdmi::LoadByte(u32 address)
	{
		return bus->Read8(address);
	}

	int Arm7tdmi::BlockTransferCycles(int count, bool thumb)
	{
		// ARM7TDMI data sheet: a block transfer is one S cycle for the instruction fetch, one N
		// cycle for the first register and one S cycle for each of the others, plus an internal
		// cycle. Thumb's LDMIA/STMIA and PUSH/POP are counted exactly the same way, so `thumb`
		// only documents which decoder asked.
		(void)thumb;
		return (count > 0) ? (count - 1) : 0;
	}

	// ---------------------------------------------------------------------------------------
	// The ARM decoder
	// ---------------------------------------------------------------------------------------

	int Arm7tdmi::StepArm()
	{
		u32 address = currentPC;
		u32 opcode = bus->Fetch32(address);

		u32 condition = opcode >> 28;
		if (condition == 0xF)
		{
			// ARM Architecture Reference Manual A3.2: the condition field 0b1111 is reserved. ARMv4T has no unconditional
			// instruction (BLX immediate is ARMv5), so this is an undefined instruction.
			undefinedCount++;
			Exception(VectorUndefined, ModeUndefined, FlagI);
			return 3;
		}

		if (!ConditionPassed(condition))
		{
			// The fetch happened and the bus charged it; nothing else does.
			currentPC = address + 4;
			return 1;
		}

		// MSR is decoded here rather than inside the data processing decoder whose encoding
		// space it shares: it writes the PSRs, and a write that changes the mode bits has to
		// bank r13/r14/r8-r12 (ARM Architecture Reference Manual A4.1.11). Both the register form (cond 00010 R 10 ...)
		// and the immediate form (cond 00110 R 10 ...) land here.
		bool msrRegister = (opcode & 0x0FB0FFF0) == 0x0120F000;
		bool msrImmediate = (opcode & 0x0FB0F000) == 0x0320F000;
		if (msrRegister || msrImmediate)
		{
			u32 fieldMask = (opcode >> 16) & 0xF;
			u32 value = msrImmediate
				? RotateRight(opcode & 0xFF, ((opcode >> 8) & 0xF) * 2)
				: ReadRegFor((int)(opcode & 0xF));
			bool toSpsr = (opcode & (1u << 22)) != 0;

			u32 mask = 0;
			if ((fieldMask & 8) != 0) mask |= 0xFF000000u;	// f: the condition flags
			if ((fieldMask & 4) != 0) mask |= 0x00FF0000u;	// s
			if ((fieldMask & 2) != 0) mask |= 0x0000FF00u;	// x
			if ((fieldMask & 1) != 0) mask |= 0x000000FFu;	// c: control (mode, I, F, T)

			// ARM Architecture Reference Manual A4.1.11: in User mode the control field is protected and the SPSR does
			// not exist, so those bits are simply not written.
			if (Mode() == ModeUser)
				mask &= ~0x000000FFu;

			if (toSpsr)
			{
				int index = bankIndex(Mode());
				if (index == 0)
				{
					GBA::Log(LogLevel::Warn, "arm7tdmi: MSR to the SPSR in User/System mode");
				}
				else
				{
					bankSPSR[index - 1] = (bankSPSR[index - 1] & ~mask) | (value & mask);
				}
			}
			else
			{
				u32 newCpsr = (cpsr & ~mask) | (value & mask);
				if ((newCpsr & ModeMask) != (cpsr & ModeMask))
					SwitchMode(nextMode(newCpsr));		// re-bank r13/r14/r8-r12 first
				cpsr = newCpsr;
			}

			currentPC = address + 4;
			return 1;				// 1S
		}

		u32 primary = (opcode >> 25) & 7;		// bits 27-25

		if (primary == 0 || primary == 1)
		{
			// Data processing (immediate and register), MRS, multiply, swap, the extra
			// load/store instructions and BX: the densest corner of the ARM encoding.
			if (primary == 0)
			{
				u32 low = opcode & 0xF0;

				if (low == 0x90)
					return (int)ArmMultiply(opcode);
				if ((opcode & 0x90) == 0x90)
					return (int)ArmHalfwordTransfer(opcode);
				if ((opcode & 0x0FFFFFF0) == 0x012FFF10)
					return (int)ArmBranch(opcode);				// BX
				if ((opcode & 0x0FBF0FFF) == 0x010F0000)
				{
					// MRS: cond 00010 R 00 1111 Rd 0000 0000 0000 (ARM Architecture Reference Manual A4.1.10). The fixed
					// field is bits 19-16; bits 15-12 are the destination register.
					int rd = (int)((opcode >> 12) & 0xF);
					regs[rd] = ((opcode & (1u << 22)) != 0) ? ReadSPSR() : cpsr;
					currentPC = address + 4;
					return 1;		// 1S
				}
				if ((((opcode >> 23) & 0x1F) == 2 && (opcode & (1u << 20)) == 0))
				{
					// The ARMv5 additions live in the spaces the ARMv4 decoder does not use:
					// CLZ, BLX (register), BKPT, BXJ, the media multiplies (SMULxy, SMLAxy,
					// SMULWy, SMLALxy), the QADD family and the LDRD/STRD encodings. ARM DDI
					// 0100E calls all of them UNDEFINED for an ARMv4T core, and so does the
					// GBA: none of them must ever reach the host as a crash. Everything with
					// bits 27-23 = 00010 and S = 0 is in that space; the ARMv4 instructions that
					// share those bits (TST, TEQ, CMP, CMN in the register form) always have S
					// set, and MRS, MSR and SWP were decoded above.
					undefinedCount++;
					Exception(VectorUndefined, ModeUndefined, FlagI);
					return 3;
				}
			}

			return (int)ArmDataProcessing(opcode);
		}

		switch (primary)
		{
		case 2:						// single data transfer, immediate offset
		case 3:						// single data transfer, register offset
			return (int)ArmLoadStore(opcode);

		case 4:						// load/store multiple
			return (int)ArmBlockTransfer(opcode);

		case 5:						// branch and branch with link
			return (int)ArmBranch(opcode);

		case 6:						// LDC/STC: the GBA has no coprocessor
			return (int)ArmCoprocessor(opcode);

		default:					// 7: coprocessor data/register transfers or SWI
			if ((opcode & (1u << 24)) != 0)
				return (int)ArmSwi(opcode);
			return (int)ArmCoprocessor(opcode);
		}
	}

	u32 Arm7tdmi::ArmDataProcessing(u32 opcode)
	{
		// ARM Architecture Reference Manual A4.1.5 (data processing). The immediate bit selects the operand form; both
		// forms share the opcode field in bits 24-21.
		u32 aluOp = (opcode >> 21) & 0xF;
		bool immediate = (opcode & (1u << 25)) != 0;
		bool setFlags = (opcode & (1u << 20)) != 0;
		bool registerShift = !immediate && (opcode & (1u << 4)) != 0;
		int rn = (int)((opcode >> 16) & 0xF);
		int rd = (int)((opcode >> 12) & 0xF);
		u32 address = currentPC;

		// ARM Architecture Reference Manual A3.4.1: r15 reads as the address of the instruction plus 8. On the ARM7TDMI a
		// register-specified shift delays the instruction by one internal cycle, and r15 then
		// reads as the address plus 12 instead (gba-tests arm 224/225, "PC as shifted register"
		// and "PC as operand 1 with shifted register"). Both operands see the later value.
		u32 pipelinePC = address + (registerShift ? 12u : 8u);
		auto ReadOperand = [&](int index) -> u32
		{
			return (index == 15) ? pipelinePC : regs[index];
		};

		bool carry = (cpsr & FlagC) != 0;
		u32 operand2;
		int cycles = 1;				// 1S for the fetch

		if (immediate)
		{
			// ARM Architecture Reference Manual A3.4.3: the operand is an 8-bit value rotated right by twice the rotate
			// field, and that rotation is the shifter's shift - so when the instruction sets the
			// flags, C takes the carry out of the rotation, which is the last bit rotated out of
			// the eight-bit value (bit 2*rotate-1). A rotate field of 0 is no rotation at all and
			// leaves C alone, unlike the register shifts where an amount of 0 does the same but
			// for a different reason. gba-tests arm 217 ("update carry for rotated immediate").
			u32 rotate = ((opcode >> 8) & 0xF) * 2;
			operand2 = RotateRight(opcode & 0xFF, rotate);
			if (rotate != 0)
				carry = ((opcode >> (rotate - 1)) & 1) != 0;
		}
		else
		{
			int rm = (int)(opcode & 0xF);
			u32 type = (opcode >> 5) & 3;
			u32 amount = (opcode >> 7) & 0x1F;
			u32 value = ReadOperand(rm);

			if (registerShift)
			{
				// ARM Architecture Reference Manual A3.4.1: a register-specified shift takes the amount from the bottom
				// byte of the register named by bits 11-8 and costs one internal cycle. A zero
				// there is a shift by nothing, not the immediate encodings' "32"/RRX.
				amount = ReadOperand((int)((opcode >> 8) & 0xF)) & 0xFF;
				cycles += 1;
				operand2 = ShiftOperandByRegister(value, type, amount, carry);
			}
			else
			{
				operand2 = ShiftOperand(value, type, amount, carry);
			}
		}

		bool isTest = (aluOp >= 8 && aluOp <= 11);		// TST, TEQ, CMP, CMN
		bool arithmetic = (aluOp >= 2 && aluOp <= 7) || aluOp == 10 || aluOp == 11;

		u32 a = ReadOperand(rn);
		u32 result = 0;
		bool carryOut = carry;
		bool overflow = false;

		switch (aluOp)
		{
		case 0x0: result = a & operand2; break;											// AND
		case 0x1: result = a ^ operand2; break;											// EOR
		case 0x2: result = AddWithCarry(a, ~operand2, 1, carryOut, overflow); break;		// SUB
		case 0x3: result = AddWithCarry(~a, operand2, 1, carryOut, overflow); break;		// RSB
		case 0x4: result = AddWithCarry(a, operand2, 0, carryOut, overflow); break;		// ADD
		case 0x5: result = AddWithCarry(a, operand2, CarryIn(cpsr), carryOut, overflow); break;	// ADC
		case 0x6: result = AddWithCarry(a, ~operand2, CarryIn(cpsr), carryOut, overflow); break;	// SBC
		case 0x7: result = AddWithCarry(~a, operand2, CarryIn(cpsr), carryOut, overflow); break;	// RSC
		case 0x8: result = a & operand2; break;											// TST
		case 0x9: result = a ^ operand2; break;											// TEQ
		case 0xA: result = AddWithCarry(a, ~operand2, 1, carryOut, overflow); break;		// CMP
		case 0xB: result = AddWithCarry(a, operand2, 0, carryOut, overflow); break;		// CMN
		case 0xC: result = a | operand2; break;											// ORR
		case 0xD: result = operand2; break;												// MOV
		case 0xE: result = a & ~operand2; break;											// BIC
		default:  result = ~operand2; break;											// MVN
		}

		// TST, TEQ, CMP and CMN always set the flags; the others do it when S is set.
		if (setFlags || isTest)
		{
			if (arithmetic)
				SetArithFlags(result, carryOut, overflow);
			else
				SetLogicFlags(result, carryOut);
		}

		if (!isTest)
		{
			if (rd == 15)
			{
				if (setFlags)
				{
					// ARM Architecture Reference Manual A4.1.5: with S set and r15 as the destination the CPSR is loaded
					// from the SPSR as well as branching - MOVS PC, LR and SUBS PC, LR, #4.
					ReturnFromException(*this, result);
					return cycles + 2;		// 2S + 1N for the pipeline refill
				}

				// A plain write of r15 is a branch. ARMv4T only interworks through BX, so the
				// state does not change here (ARM Architecture Reference Manual A4.1.5).
				currentPC = result & ~1u;
				return cycles + 2;			// 2S + 1N
			}

			regs[rd] = result;
		}
		else if (rd == 15 && setFlags && InException())
		{
			// The ARM7TDMI decodes TST/TEQ/CMP/CMN with S set and r15 in the destination field
			// as the CPSR-restoring form of a data processing instruction, so the SPSR becomes
			// the CPSR and the register bank switches - but the instruction has no destination
			// register, so the PC is not written and execution simply carries on in the restored
			// mode (gba-tests arm 234, "bad CMP/CMN/TST/TEQ change the mode"). In User and
			// System mode there is no SPSR to restore, which is what arm 235 checks.
			u32 spsr = ReadSPSR();
			SwitchMode((CpuMode)(spsr & ModeMask));
			cpsr = spsr;
		}

		currentPC = address + 4;
		return cycles;
	}

	u32 Arm7tdmi::ArmMultiply(u32 opcode)
	{
		// ARM Architecture Reference Manual A4.1.6 (MUL/MLA), A4.1.7 (the long multiplies) and A4.1.9 (SWP/SWPB). The
		// space is selected by bits 27-23 and by the 1SH1 field in bits 7-4.
		u32 address = currentPC;
		u32 space = (opcode >> 23) & 0x1F;
		int rd = (int)((opcode >> 16) & 0xF);
		int rn = (int)((opcode >> 12) & 0xF);
		int rs = (int)((opcode >> 8) & 0xF);
		int rm = (int)(opcode & 0xF);
		bool accumulate = (opcode & (1u << 21)) != 0;

		if (space == 0)
		{
			// MUL/MLA: Rd = Rm * Rs (+ Rn).
			if (rd == 15 || rm == 15 || rs == 15 || (rn == 15 && accumulate))
			{
				undefinedCount++;
				Exception(VectorUndefined, ModeUndefined, FlagI);
				return 3;
			}

			u32 value = ReadRegFor(rm);
			u32 multiplier = ReadRegFor(rs);
			u32 result = value * multiplier;
			if (accumulate)
				result += ReadRegFor(rn);

			regs[rd] = result;
			// N and Z come from the 32-bit result. C is UNPREDICTABLE for MUL/MLA on ARMv4
			// (the shifter's carry is what remains) and V is not touched by a multiply.
			SetFlag(FlagN, (result & 0x80000000u) != 0);
			SetFlag(FlagZ, result == 0);

			currentPC = address + 4;
			return 1 + MultiplyCycles(multiplier) + (accumulate ? 1 : 0);
		}

		if (space == 1)
		{
			// The long multiplies: RdHi:RdLo = Rm * Rs (+ RdHi:RdLo).
			if (rd == 15 || rn == 15 || rm == 15 || rs == 15)
			{
				undefinedCount++;
				Exception(VectorUndefined, ModeUndefined, FlagI);
				return 3;
			}

			bool signedMultiply = (opcode & (1u << 22)) != 0;
			u32 value = ReadRegFor(rm);
			u32 multiplier = ReadRegFor(rs);

			u64 result = signedMultiply
				? (u64)((s64)(s32)value * (s64)(s32)multiplier)
				: (u64)((u64)value * (u64)multiplier);

			if (accumulate)
				result += ((u64)regs[rd] << 32) | (u64)regs[rn];

			// ARM Architecture Reference Manual A4.1.7: RdHi holds the high half and RdLo the low half, and N and Z come
			// from the 64-bit result (bit 63 is N). C is unpredictable and V is unchanged.
			regs[rd] = (u32)(result >> 32);
			regs[rn] = (u32)result;
			SetFlag(FlagN, (result & 0x8000000000000000ull) != 0);
			SetFlag(FlagZ, result == 0);

			currentPC = address + 4;
			return 1 + MultiplyCycles(multiplier) + 1 + (accumulate ? 1 : 0);
		}

		if (space == 2)
		{
			// SWP/SWPB: read, then write, both to the same address. The only atomic primitive
			// the core has; it costs two non-sequential accesses and an internal cycle.
			//
			// The encoding is cond 00010 B 00 Rn Rd 0000 1001 Rm, so the address register is in
			// bits 19-16 and the destination in bits 15-12 - the opposite way round from the
			// multiply layouts above, which is why the two roles are reread here.
			int swapRn = (int)((opcode >> 16) & 0xF);
			int swapRd = (int)((opcode >> 12) & 0xF);
			int swapRm = (int)(opcode & 0xF);

			if (swapRd == 15 || swapRm == 15 || swapRn == 15)
			{
				undefinedCount++;
				Exception(VectorUndefined, ModeUndefined, FlagI);
				return 3;
			}

			bool byte = (opcode & (1u << 22)) != 0;
			u32 swapAddress = ReadRegFor(swapRn);
			u32 value = ReadRegFor(swapRm);

			if (byte)
			{
				regs[swapRd] = bus->Read8(swapAddress);
				bus->Write8(swapAddress, (u8)value);
			}
			else
			{
				bool aligned;
				regs[swapRd] = LoadWord(swapAddress, aligned);
				bus->Write32(swapAddress & ~3u, value);
			}

			currentPC = address + 4;
			return 4;				// 1S + 2N + 1I
		}

		undefinedCount++;
		Exception(VectorUndefined, ModeUndefined, FlagI);
		return 3;
	}

	u32 Arm7tdmi::ArmLoadStore(u32 opcode)
	{
		// ARM Architecture Reference Manual A4.1.3: single data transfer.
		//   cond 01 I P U B W L Rn Rd offset
		bool registerOffset = (opcode & (1u << 25)) != 0;
		bool preIndexed = (opcode & (1u << 24)) != 0;
		bool add = (opcode & (1u << 23)) != 0;
		bool byte = (opcode & (1u << 22)) != 0;
		bool writeback = (opcode & (1u << 21)) != 0;
		bool load = (opcode & (1u << 20)) != 0;
		int rn = (int)((opcode >> 16) & 0xF);
		int rd = (int)((opcode >> 12) & 0xF);
		u32 address = currentPC;

		if (!preIndexed && writeback)
		{
			// ARM Architecture Reference Manual A4.1.3: P = 0 with W = 1 is one of the undefined encodings.
			undefinedCount++;
			Exception(VectorUndefined, ModeUndefined, FlagI);
			return 3;
		}
		if (registerOffset && (opcode & 0x10) != 0)
		{
			// The register offset form has no bit 4; ARMv5 put the media instructions there.
			undefinedCount++;
			Exception(VectorUndefined, ModeUndefined, FlagI);
			return 3;
		}

		// With P = 0 the base is always written back (W has to be 0 there); with P = 1 only when
		// W asks for it. ARM Architecture Reference Manual A4.1.3.
		bool updateBase = writeback || !preIndexed;

		u32 base = ReadRegFor(rn);
		u32 offset;
		int cycles = load ? 3 : 2;	// LDR: 1S + 1N + 1I; STR: 2N

		if (registerOffset)
		{
			bool carry = (cpsr & FlagC) != 0;
			offset = ShiftOperand(ReadRegFor((int)(opcode & 0xF)),
				(opcode >> 5) & 3, (opcode >> 7) & 0x1F, carry);
			cycles += 1;			// the shifter's internal cycle
		}
		else
		{
			offset = opcode & 0xFFF;
		}

		u32 offsetAddress = add ? base + offset : base - offset;
		u32 accessAddress = preIndexed ? offsetAddress : base;

		if (load)
		{
			u32 value;
			if (byte)
			{
				value = LoadByte(accessAddress);
			}
			else
			{
				bool aligned;
				value = LoadWord(accessAddress, aligned);
				if (!aligned)
					GBA::Log(LogLevel::Debug, "arm7tdmi: unaligned LDR at %08X", accessAddress);
			}

			// ARM Architecture Reference Manual A4.1.3: the write-back happens before the load writes Rd, so an LDR with
			// Rd = Rn leaves the loaded value in the register.
			if (updateBase && rn != 15)
				regs[rn] = offsetAddress;

			if (rd == 15)
			{
				// LDR into r15 is a branch. ARMv4T does not interwork here: bit 0 is dropped
				// and the state does not change (only BX switches state).
				cycles += 1;		// the pipeline refill
				currentPC = value & ~1u;
			}
			else
			{
				regs[rd] = value;
			}
		}
		else
		{
			// ARM Architecture Reference Manual A3.4.1: a store puts the address of the instruction plus 12 on the bus when
			// r15 is the source, because the data is read one pipeline stage later than an operand
			// (gba-tests arm 510, "store PC + 4", does the same check for STM).
			u32 value = (rd == 15)
				? (address + 12)
				: (byte ? (ReadRegFor(rd) & 0xFFu) : ReadRegFor(rd));

			if (updateBase && rn != 15)
				regs[rn] = offsetAddress;

			if (byte)
				bus->Write8(accessAddress, (u8)value);
			else
				bus->Write32(accessAddress & ~3u, value);
		}

		if (!(load && rd == 15))
			currentPC = address + 4;
		return cycles;
	}

	u32 Arm7tdmi::ArmHalfwordTransfer(u32 opcode)
	{
		// ARM Architecture Reference Manual A4.1.8: the extra load/store instructions (halfword and signed byte).
		//   cond 000 P U I W L Rn Rd offsetHigh 1 S H 1 offsetLow
		bool preIndexed = (opcode & (1u << 24)) != 0;
		bool add = (opcode & (1u << 23)) != 0;
		bool immediate = (opcode & (1u << 22)) != 0;
		bool writeback = (opcode & (1u << 21)) != 0;
		bool load = (opcode & (1u << 20)) != 0;
		bool signedLoad = (opcode & (1u << 6)) != 0;
		bool halfword = (opcode & (1u << 5)) != 0;
		int rn = (int)((opcode >> 16) & 0xF);
		int rd = (int)((opcode >> 12) & 0xF);
		u32 address = currentPC;

		if (!preIndexed && writeback)
		{
			undefinedCount++;
			Exception(VectorUndefined, ModeUndefined, FlagI);
			return 3;
		}
		if (signedLoad && !load)
		{
			// LDRD/STRD (ARMv5) hide in this encoding; ARMv4T calls it undefined.
			undefinedCount++;
			Exception(VectorUndefined, ModeUndefined, FlagI);
			return 3;
		}
		if (!immediate && (opcode & 0xF00) != 0)
		{
			undefinedCount++;
			Exception(VectorUndefined, ModeUndefined, FlagI);
			return 3;
		}
		if ((load || signedLoad) && rd == 15)
		{
			// A halfword or signed load into r15 has no defined meaning on this core.
			undefinedCount++;
			Exception(VectorUndefined, ModeUndefined, FlagI);
			return 3;
		}

		u32 offset = immediate
			? (((opcode >> 8) & 0xF) << 4) | (opcode & 0xF)
			: ReadRegFor((int)(opcode & 0xF));
		u32 base = ReadRegFor(rn);
		u32 offsetAddress = add ? base + offset : base - offset;
		u32 accessAddress = preIndexed ? offsetAddress : base;
		bool updateBase = writeback || !preIndexed;		// P = 0 always writes the base back

		if (load)
		{
			u32 value;
			if (!signedLoad)
			{
				value = LoadHalfword(accessAddress);						// LDRH (rotates if odd)
			}
			else if (!halfword)
			{
				value = (u32)(s32)(s8)bus->Read8(accessAddress);			// LDRSB
			}
			else if ((accessAddress & 1) != 0)
			{
				// ARM7TDMI: a signed halfword load from an odd address is done as a signed byte
				// load (gba-tests arm 409, "misaligned load signed halfword").
				value = (u32)(s32)(s8)bus->Read8(accessAddress);
			}
			else
			{
				value = (u32)(s32)(s16)bus->Read16(accessAddress);			// LDRSH
			}

			if (updateBase && rn != 15)
				regs[rn] = offsetAddress;
			regs[rd] = value;
		}
		else
		{
			// ARM Architecture Reference Manual A3.4.1: a store of r15 puts the address of the instruction plus 12 on the
			// bus; only an operand read uses plus 8.
			u32 value = (rd == 15) ? (address + 12) : (ReadRegFor(rd) & 0xFFFFu);

			if (updateBase && rn != 15)
				regs[rn] = offsetAddress;
			bus->Write16(accessAddress & ~1u, (u16)value);
		}

		currentPC = address + 4;
		return load ? 3 : 2;
	}

	u32 Arm7tdmi::ArmBlockTransfer(u32 opcode)
	{
		// ARM Architecture Reference Manual A4.1.20: load/store multiple.
		//   cond 100 P U S W L Rn register_list
		bool preIndexed = (opcode & (1u << 24)) != 0;
		bool add = (opcode & (1u << 23)) != 0;
		bool userBank = (opcode & (1u << 22)) != 0;
		bool writeback = (opcode & (1u << 21)) != 0;
		bool load = (opcode & (1u << 20)) != 0;
		int rn = (int)((opcode >> 16) & 0xF);
		u32 list = opcode & 0xFFFF;
		u32 address = currentPC;

		if (rn == 15)
		{
			// r15 as the base of a block transfer is UNPREDICTABLE; treat it as undefined rather
			// than inventing an address.
			undefinedCount++;
			Exception(VectorUndefined, ModeUndefined, FlagI);
			return 3;
		}

		// The ARM7TDMI's documented oddity for an empty register list (gba-tests arm 513-515 and
		// 530-532, thumb 227-229): the address arithmetic and the write-back behave as if all
		// sixteen registers were in the list - the base moves by 0x40 - but only r15 is actually
		// transferred. A store puts the PC (the instruction address plus 12) at the first address
		// of that full transfer; a load takes r15 from it, which branches.
		bool emptyList = (list == 0);
		if (emptyList)
			list = 0x8000;

		u32 base = ReadRegFor(rn);
		int count = emptyList ? 16 : CountRegisters(list);
		u32 writebackValue = add ? base + 4u * count : base - 4u * count;

		// The transfer walks the addresses in increasing order, and the four addressing modes put
		// the first of them here (ARM Architecture Reference Manual A4.1.20):
		//   IA: base, base+4, ...          IB: base+4, base+8, ...
		//   DA: base-4(n-1), ..., base     DB: base-4n, ..., base-4
		u32 transferAddress = add
			? (preIndexed ? base + 4 : base)
			: (preIndexed ? base - 4u * count : base - 4u * (count - 1));

		// The caret: with S set and r15 *not* in the list the transfer uses the User bank's
		// registers whatever mode the CPU is in - that is how a handler reaches the stack of
		// the task it interrupted. With r15 in the list it is the exception return instead and
		// the transfer stays in the current bank (ARM Architecture Reference Manual A4.1.20).
		bool useUserBank = userBank && !(load && (list & 0x8000) != 0);

		auto ReadRegister = [&](int index) -> u32
		{
			if (!useUserBank)
				return regs[index];
			if (index >= 8 && index <= 12) return bankR8_12[0][index - 8];
			if (index == 13) return bankR13[0];
			if (index == 14) return bankR14[0];
			return regs[index];			// r0-r7 are shared by every mode
		};

		auto WriteRegister = [&](int index, u32 value)
		{
			if (!useUserBank)
			{
				regs[index] = value;
				return;
			}
			if (index >= 8 && index <= 12)
			{
				bankR8_12[0][index - 8] = value;
				if (bankIndex(Mode()) != 1)		// FIQ keeps its own r8-r12 in the window
					regs[index] = value;
			}
			else if (index == 13)
			{
				bankR13[0] = value;
				if (bankIndex(Mode()) == 0)
					regs[13] = value;
			}
			else if (index == 14)
			{
				bankR14[0] = value;
				if (bankIndex(Mode()) == 0)
					regs[14] = value;
			}
			else
			{
				regs[index] = value;
			}
		};

		if (load)
		{
			// The write-back is applied first so that a loaded base register wins (ARM Architecture Reference Manual
			// A4.1.20: the base is written back and then the registers are loaded). An empty list
			// moves the base too, write-back bit or not, because that is how the ARM7TDMI's
			// oddity is described.
			if (writeback || emptyList)
			{
				if (useUserBank)
					WriteRegister(rn, writebackValue);
				else
					regs[rn] = writebackValue;
			}

			u32 loadedPC = 0;
			bool loadedPCPresent = false;

			for (int i = 0; i < 16; i++)
			{
				if ((list & (1u << i)) == 0)
					continue;

				// A block transfer ignores the low two bits of the address: unlike an LDR there
				// is no rotation of the loaded word (ARM Architecture Reference Manual A4.1.20, and gba-tests arm 508
				// transfers from a base that is deliberately misaligned).
				u32 value = bus->Read32(transferAddress & ~3u);
				transferAddress += 4;

				if (i == 15)
				{
					loadedPC = value;
					loadedPCPresent = true;
				}
				else
				{
					WriteRegister(i, value);
				}
			}

			if (loadedPCPresent)
			{
				if (userBank)
				{
					// LDM with the caret and r15: the SPSR becomes the CPSR.
					ReturnFromException(*this, loadedPC);
					return 2 + BlockTransferCycles(count, false) + 2;
				}

				// A plain load of r15 branches; ARMv4T does not change state here.
				currentPC = loadedPC & ~1u;
				return 2 + BlockTransferCycles(count, false) + 2;
			}
		}
		else
		{
			// ARM Architecture Reference Manual A4.1.20 / the ARM7TDMI (gba-tests arm 516-529): with the write-back, the
			// base register's own slot holds the write-back value - the new base - unless the
			// base is the *lowest*-numbered register of the list, in which case the original base
			// value is what goes to memory. Without a write-back the base never moves, so the
			// original value is stored either way.
			int lowest = 15;
			for (int i = 0; i < 16; i++)
			{
				if ((list & (1u << i)) != 0)
				{
					lowest = i;
					break;
				}
			}

			for (int i = 0; i < 16; i++)
			{
				if ((list & (1u << i)) == 0)
					continue;

				u32 value;
				if (i == 15)
				{
					// A store of r15 puts the *next* pipeline stage's PC on the bus: the pipeline
					// value (the instruction address plus one instruction) plus one more
					// instruction - so plus 12 in ARM state and plus 6 in Thumb state
					// (gba-tests arm 510 and thumb 229).
					value = address + (ThumbState() ? 6u : 12u);
				}
				else if (i == rn && writeback && i != lowest)
					value = writebackValue;
				else
					value = ReadRegister(i);

				bus->Write32(transferAddress & ~3u, value);
				transferAddress += 4;
			}

			if (writeback || emptyList)
			{
				if (useUserBank)
					WriteRegister(rn, writebackValue);
				else
					regs[rn] = writebackValue;
			}
		}

		currentPC = address + 4;

		// ARM7TDMI data sheet: LDM is 1S + 1N + (n-1)S + 1I and STM is 2N + (n-1)S.
		return (load ? 3 : 2) + BlockTransferCycles(count, false);
	}

	u32 Arm7tdmi::ArmBranch(u32 opcode)
	{
		u32 address = currentPC;
		bool link = (opcode & (1u << 24)) != 0;

		if ((opcode & 0x0FFFFFF0) == 0x012FFF10)
		{
			// BX: cond 0001 0010 1111 1111 1111 0001 Rm. The only ARMv4T instruction that
			// switches between ARM and Thumb state (ARM Architecture Reference Manual A4.1.4).
			BranchInternal(ReadRegFor((int)(opcode & 0xF)));
			return 3;				// 2S + 1N
		}

		if ((opcode & 0x0FFFFFF0) == 0x012FFF30)
		{
			// BLX (register) is ARMv5; the GBA's core does not have it.
			undefinedCount++;
			Exception(VectorUndefined, ModeUndefined, FlagI);
			return 3;
		}

		// B/BL: cond 101 L offset(24). The offset is sign-extended, shifted left by two and
		// added to the pipeline PC (the address of the instruction plus 8).
		u32 offset = SignExtend(opcode & 0xFFFFFF, 24) << 2;
		if (link)
			regs[14] = address + 4;		// the return address: the instruction after the BL
		currentPC = (address + 8 + offset) & ~1u;
		return 3;						// 2S + 1N
	}

	u32 Arm7tdmi::ArmSwi(u32 opcode)
	{
		// GBATEK "SWI": the comment field is bits 16-23 of the instruction and selects the BIOS
		// function. The return address is the instruction after the SWI.
		u32 address = currentPC;
		u32 comment = (opcode >> 16) & 0xFF;
		u32 returnAddress = address + 4;

		bool wasThumb = ThumbState();

		if (bus->HleBiosEnabled)
		{
			// The HLE contract: the host handler is called from the caller's mode and finishes the
			// call itself (it returns through the PC, which this leaves on the return address, or
			// leaves the CPU halted for Halt/IntrWait). r14 of the caller's mode is *not* touched:
			// a real BIOS preserves it across the SWI, and a leaf thunk ("swi N; bx lr") depends on
			// it. The CPU must not bank the mode, not touch the CPSR or the SPSR and not
			// double-return to the caller. Leaving currentPC on the return address makes the
			// instruction complete normally if a handler chooses not to move the PC at all.
			currentPC = returnAddress;
		}

		if (bus->Swi(comment))
		{
			// A SWI never changes the instruction state: the real handler returns through the
			// SPSR and so comes back in the state it was called from. A host handler moves the
			// PC with BranchTo(), which interworks on bit 0 and would clear T for a Thumb
			// caller, so the state is put back here.
			SetFlag(FlagT, wasThumb);
		}
		else
		{
			// No host handler (a real BIOS image is installed, or the call is not implemented):
			// this is the real exception, with LR_svc just past the SWI and SPSR_svc holding the
			// caller's CPSR - the caller's own r14 is left untouched. Exception() derives LR from
			// the instruction address, so put the PC back first.
			currentPC = address;
			Exception(VectorSwi, ModeSupervisor, FlagI);
		}

		return 3;						// 2S + 1N
	}

	u32 Arm7tdmi::ArmCoprocessor(u32 opcode)
	{
		// GBATEK "GBA CPU": the GBA has no coprocessor, so every coprocessor instruction -
		// LDC/STC (cond 110), CDP and MCR/MRC (cond 1110) - is UNDEFINED and takes the
		// undefined instruction vector (ARM Architecture Reference Manual A3.2 and A4.1.13 to A4.1.15).
		(void)opcode;
		undefinedCount++;
		Exception(VectorUndefined, ModeUndefined, FlagI);
		return 3;
	}

	// ---------------------------------------------------------------------------------------
	// The Thumb decoder
	// ---------------------------------------------------------------------------------------

	int Arm7tdmi::StepThumb()
	{
		u32 address = currentPC;
		u16 opcode = bus->Fetch16(address);

		// ARM Architecture Reference Manual 4.5, Table 4-1: bits 15-12 (with a few special cases inside each group) pick
		// one of the nineteen Thumb formats. Every decoder moves the PC on itself.
		switch (opcode & 0xF000)
		{
		case 0x0000:
		case 0x1000:
			// 1 (move shifted register) or 2 (add/subtract): bits 12-11 = 11 is the add/subtract
			// format, which is why the two groups share the top nibbles 0000 and 0001.
			return (int)(((opcode & 0x1800) == 0x1800)
				? ThumbAddSubtract(opcode)
				: ThumbShiftImmediate(opcode));

		case 0x2000:
		case 0x3000:
			return (int)ThumbMovCmpAddSub(opcode);				// 3

		case 0x4000:
			if ((opcode & 0xF800) == 0x4800)
				return (int)ThumbPcRelativeLoad(opcode);		// 6 (01001 Rd word8)
			if ((opcode & 0xFC00) == 0x4000)
				return (int)ThumbAluOperation(opcode);			// 4
			if ((opcode & 0xFC00) == 0x4400)
				return (int)ThumbHiRegister(opcode);			// 5
			break;

		case 0x5000:
			// 7 (register offset) and 8 (sign-extended) differ in bit 9.
			return (int)(((opcode & 0x0200) != 0)
				? ThumbLoadStoreSignExtend(opcode)
				: ThumbLoadStoreReg(opcode));

		case 0x6000:
		case 0x7000:
			return (int)ThumbLoadStoreImmediate(opcode);		// 9

		case 0x8000:
			return (int)ThumbLoadStoreHalfword(opcode);			// 10

		case 0x9000:
			return (int)ThumbLoadStoreSpRelative(opcode);		// 11

		case 0xA000:
			return (int)ThumbLoadAddress(opcode);				// 12

		case 0xB000:
			if ((opcode & 0xF600) == 0xB400)
				return (int)ThumbPushPop(opcode);				// 14
			if ((opcode & 0xFF00) == 0xB000)
				return (int)ThumbAddOffsetToSp(opcode);			// 13
			break;												// 0xBE00 is BKPT in ARMv5

		case 0xC000:
			return (int)ThumbMultipleTransfer(opcode);			// 15

		case 0xD000:
			if ((opcode & 0xFF00) == 0xDF00)
				return (int)ThumbSwi(opcode);					// 17
			return (int)ThumbConditionalBranch(opcode);			// 16

		case 0xE000:
			return (int)ThumbUnconditionalBranch(opcode);		// 18

		default:
			return (int)ThumbLongBranchWithLink(opcode);		// 19
		}

		// Nothing in the table matched: an undefined instruction.
		undefinedCount++;
		Exception(VectorUndefined, ModeUndefined, FlagI);
		return 3;
	}

	u32 Arm7tdmi::ThumbShiftImmediate(u16 opcode)
	{
		// ARM Architecture Reference Manual 4.5.1: LSL/LSR/ASR Rd, Rs, #imm5. Always sets N, Z and C.
		u32 address = currentPC;
		u32 type = (opcode >> 11) & 3;
		u32 amount = (opcode >> 6) & 0x1F;
		int rs = (opcode >> 3) & 7;
		int rd = opcode & 7;

		bool carry = (cpsr & FlagC) != 0;
		u32 result = ShiftOperand(regs[rs], type, amount, carry);
		SetLogicFlags(result, carry);
		regs[rd] = result;

		currentPC = address + 2;
		return 1;
	}

	u32 Arm7tdmi::ThumbAddSubtract(u16 opcode)
	{
		// ARM Architecture Reference Manual 4.5.2: ADD/SUB Rd, Rs, Rn or ADD/SUB Rd, Rs, #imm3. Sets all four flags.
		u32 address = currentPC;
		bool immediate = (opcode & 0x0400) != 0;
		bool subtract = (opcode & 0x0200) != 0;
		u32 field = (opcode >> 6) & 7;
		int rs = (opcode >> 3) & 7;
		int rd = opcode & 7;

		u32 operand = immediate ? field : regs[field];
		bool carry = false;
		bool overflow = false;
		u32 result = subtract
			? AddWithCarry(regs[rs], ~operand, 1, carry, overflow)
			: AddWithCarry(regs[rs], operand, 0, carry, overflow);

		SetArithFlags(result, carry, overflow);
		regs[rd] = result;

		currentPC = address + 2;
		return 1;
	}

	u32 Arm7tdmi::ThumbMovCmpAddSub(u16 opcode)
	{
		// ARM Architecture Reference Manual 4.5.3: MOV/CMP/ADD/SUB Rd, #imm8. MOV sets only N and Z; the other three set
		// all four flags.
		u32 address = currentPC;
		u32 operation = (opcode >> 11) & 3;
		int rd = (opcode >> 8) & 7;
		u32 immediate = opcode & 0xFF;

		bool carry = false;
		bool overflow = false;

		switch (operation)
		{
		case 0:						// MOV: C and V are unchanged
			regs[rd] = immediate;
			SetLogicFlags(immediate, (cpsr & FlagC) != 0);
			break;
		case 1:						// CMP
		{
			u32 result = AddWithCarry(regs[rd], ~immediate, 1, carry, overflow);
			SetArithFlags(result, carry, overflow);
			break;
		}
		case 2:						// ADD
		{
			u32 result = AddWithCarry(regs[rd], immediate, 0, carry, overflow);
			SetArithFlags(result, carry, overflow);
			regs[rd] = result;
			break;
		}
		default:					// SUB
		{
			u32 result = AddWithCarry(regs[rd], ~immediate, 1, carry, overflow);
			SetArithFlags(result, carry, overflow);
			regs[rd] = result;
			break;
		}
		}

		currentPC = address + 2;
		return 1;
	}

	u32 Arm7tdmi::ThumbAluOperation(u16 opcode)
	{
		// ARM Architecture Reference Manual 4.5.4: the sixteen ALU operations of format 4. For the four shifts the amount
		// is the low byte of Rd, which is also the destination - that is the encoding, and it
		// makes those shifts register-specified ones, so an amount of 0 leaves the value and C
		// alone (ShiftOperandByRegister; gba-tests thumb 068).
		u32 address = currentPC;
		u32 operation = (opcode >> 6) & 0xF;
		int rs = (opcode >> 3) & 7;
		int rd = opcode & 7;
		u32 value = regs[rd];
		u32 operand = regs[rs];

		bool carry = (cpsr & FlagC) != 0;
		bool overflow = false;
		int cycles = 1;

		switch (operation)
		{
		case 0x0: regs[rd] = value & operand; SetLogicFlags(regs[rd], carry); break;	// AND
		case 0x1: regs[rd] = value ^ operand; SetLogicFlags(regs[rd], carry); break;	// EOR
		case 0x2:														// LSL
			regs[rd] = ShiftOperandByRegister(value, 0, operand & 0xFF, carry);
			SetLogicFlags(regs[rd], carry);
			cycles += 1;
			break;
		case 0x3:														// LSR
			regs[rd] = ShiftOperandByRegister(value, 1, operand & 0xFF, carry);
			SetLogicFlags(regs[rd], carry);
			cycles += 1;
			break;
		case 0x4:														// ASR
			regs[rd] = ShiftOperandByRegister(value, 2, operand & 0xFF, carry);
			SetLogicFlags(regs[rd], carry);
			cycles += 1;
			break;
		case 0x5:														// ADC
			regs[rd] = AddWithCarry(value, operand, CarryIn(cpsr), carry, overflow);
			SetArithFlags(regs[rd], carry, overflow);
			break;
		case 0x6:														// SBC
			regs[rd] = AddWithCarry(value, ~operand, CarryIn(cpsr), carry, overflow);
			SetArithFlags(regs[rd], carry, overflow);
			break;
		case 0x7:														// ROR
			regs[rd] = ShiftOperandByRegister(value, 3, operand & 0xFF, carry);
			SetLogicFlags(regs[rd], carry);
			cycles += 1;
			break;
		case 0x8:														// TST
			SetLogicFlags(value & operand, carry);
			break;
		case 0x9:														// NEG
			regs[rd] = AddWithCarry(0, ~operand, 1, carry, overflow);
			SetArithFlags(regs[rd], carry, overflow);
			break;
		case 0xA:														// CMP
		{
			u32 result = AddWithCarry(value, ~operand, 1, carry, overflow);
			SetArithFlags(result, carry, overflow);
			break;
		}
		case 0xB:														// CMN
		{
			u32 result = AddWithCarry(value, operand, 0, carry, overflow);
			SetArithFlags(result, carry, overflow);
			break;
		}
		case 0xC: regs[rd] = value | operand; SetLogicFlags(regs[rd], carry); break;	// ORR
		case 0xD:														// MUL
			regs[rd] = value * operand;
			SetFlag(FlagN, (regs[rd] & 0x80000000u) != 0);
			SetFlag(FlagZ, regs[rd] == 0);
			cycles = 1 + MultiplyCycles(operand);
			break;
		case 0xE: regs[rd] = value & ~operand; SetLogicFlags(regs[rd], carry); break;	// BIC
		default:  regs[rd] = ~operand; SetLogicFlags(regs[rd], carry); break;			// MVN
		}

		currentPC = address + 2;
		return (u32)cycles;
	}

	u32 Arm7tdmi::ThumbHiRegister(u16 opcode)
	{
		// ARM Architecture Reference Manual 4.5.5: ADD/CMP/MOV/BX with the high registers. ADD and MOV do not set the
		// flags, CMP sets all four, and BX is the interworking branch. A write of r15 here is a
		// branch; ARMv4T does not switch state on it.
		u32 address = currentPC;
		u32 operation = (opcode >> 8) & 3;
		int rs = ((opcode >> 3) & 7) | ((opcode & 0x40) ? 8 : 0);
		int rd = (opcode & 7) | ((opcode & 0x80) ? 8 : 0);

		bool carry = false;
		bool overflow = false;

		switch (operation)
		{
		case 0:						// ADD (no flags)
		{
			u32 result = ReadRegFor(rd) + ReadRegFor(rs);
			if (rd == 15)
			{
				currentPC = result & ~1u;
				return 3;
			}
			regs[rd] = result;
			break;
		}
		case 1:						// CMP (all flags)
		{
			u32 result = AddWithCarry(ReadRegFor(rd), ~ReadRegFor(rs), 1, carry, overflow);
			SetArithFlags(result, carry, overflow);
			break;
		}
		case 2:						// MOV (no flags)
		{
			u32 result = ReadRegFor(rs);
			if (rd == 15)
			{
				currentPC = result & ~1u;
				return 3;
			}
			regs[rd] = result;
			break;
		}
		default:					// BX
			BranchInternal(ReadRegFor(rs));
			return 3;
		}

		currentPC = address + 2;
		return 1;
	}

	u32 Arm7tdmi::ThumbPcRelativeLoad(u16 opcode)
	{
		// ARM Architecture Reference Manual 4.5.6: LDR Rd, [PC, #imm8*4]. The address is word aligned.
		u32 address = currentPC;
		int rd = (opcode >> 8) & 7;
		u32 offset = (opcode & 0xFF) * 4;

		regs[rd] = bus->Read32((ReadRegFor(15) & ~3u) + offset);

		currentPC = address + 2;
		return 3;					// 1S + 1N + 1I
	}

	u32 Arm7tdmi::ThumbLoadStoreReg(u16 opcode)
	{
		// ARM Architecture Reference Manual 4.5.7: STR/LDR/STRB/LDRB Rd, [Rb, Ro].
		u32 address = currentPC;
		bool load = (opcode & 0x0800) != 0;
		bool byte = (opcode & 0x0400) != 0;
		int ro = (opcode >> 6) & 7;
		int rb = (opcode >> 3) & 7;
		int rd = opcode & 7;
		u32 accessAddress = regs[rb] + regs[ro];

		if (load)
		{
			if (byte)
			{
				regs[rd] = LoadByte(accessAddress);
			}
			else
			{
				bool aligned;
				regs[rd] = LoadWord(accessAddress, aligned);
			}
		}
		else if (byte)
		{
			bus->Write8(accessAddress, (u8)regs[rd]);
		}
		else
		{
			bus->Write32(accessAddress & ~3u, regs[rd]);
		}

		currentPC = address + 2;
		return load ? 3 : 2;
	}

	u32 Arm7tdmi::ThumbLoadStoreSignExtend(u16 opcode)
	{
		// ARM Architecture Reference Manual 4.5.8: STRH/LDRH/LDRSB/LDRSH Rd, [Rb, Ro]. Unlike the ARM encoding there is
		// no separate L bit: H says halfword, S says sign-extend, and only the H = 0, S = 0
		// combination is a store - all four combinations are defined in Thumb.
		u32 address = currentPC;
		bool halfwordOperand = (opcode & 0x0800) != 0;
		bool signedOperand = (opcode & 0x0400) != 0;
		int ro = (opcode >> 6) & 7;
		int rb = (opcode >> 3) & 7;
		int rd = opcode & 7;
		u32 accessAddress = regs[rb] + regs[ro];
		bool load = halfwordOperand || signedOperand;

		if (signedOperand)
		{
			if (halfwordOperand && (accessAddress & 1) == 0)
				regs[rd] = (u32)(s32)(s16)bus->Read16(accessAddress);		// LDRSH
			else
				regs[rd] = (u32)(s32)(s8)bus->Read8(accessAddress);			// LDRSB, and an odd LDRSH
		}
		else if (halfwordOperand)
		{
			regs[rd] = LoadHalfword(accessAddress);							// LDRH
		}
		else
		{
			bus->Write16(accessAddress & ~1u, (u16)regs[rd]);				// STRH
		}

		currentPC = address + 2;
		return load ? 3 : 2;
	}

	u32 Arm7tdmi::ThumbLoadStoreImmediate(u16 opcode)
	{
		// ARM Architecture Reference Manual 4.5.9: STR/LDR/STRB/LDRB Rd, [Rb, #imm5] (word or byte scaled).
		u32 address = currentPC;
		bool byte = (opcode & 0x1000) != 0;
		bool load = (opcode & 0x0800) != 0;
		u32 offset = (opcode >> 6) & 0x1F;
		int rb = (opcode >> 3) & 7;
		int rd = opcode & 7;
		u32 accessAddress = regs[rb] + (byte ? offset : offset * 4);

		if (load)
		{
			if (byte)
			{
				regs[rd] = LoadByte(accessAddress);
			}
			else
			{
				bool aligned;
				regs[rd] = LoadWord(accessAddress, aligned);
			}
		}
		else if (byte)
		{
			bus->Write8(accessAddress, (u8)regs[rd]);
		}
		else
		{
			bus->Write32(accessAddress & ~3u, regs[rd]);
		}

		currentPC = address + 2;
		return load ? 3 : 2;
	}

	u32 Arm7tdmi::ThumbLoadStoreHalfword(u16 opcode)
	{
		// ARM Architecture Reference Manual 4.5.10: STRH/LDRH Rd, [Rb, #imm5*2].
		u32 address = currentPC;
		bool load = (opcode & 0x0800) != 0;
		u32 offset = ((opcode >> 6) & 0x1F) * 2;
		int rb = (opcode >> 3) & 7;
		int rd = opcode & 7;
		u32 accessAddress = regs[rb] + offset;

		if (load)
			regs[rd] = LoadHalfword(accessAddress);
		else
			bus->Write16(accessAddress & ~1u, (u16)regs[rd]);

		currentPC = address + 2;
		return load ? 3 : 2;
	}

	u32 Arm7tdmi::ThumbLoadStoreSpRelative(u16 opcode)
	{
		// ARM Architecture Reference Manual 4.5.11: STR/LDR Rd, [SP, #imm8*4].
		u32 address = currentPC;
		bool load = (opcode & 0x0800) != 0;
		int rd = (opcode >> 8) & 7;
		u32 accessAddress = regs[13] + (opcode & 0xFF) * 4;

		if (load)
		{
			bool aligned;
			regs[rd] = LoadWord(accessAddress, aligned);
		}
		else
		{
			bus->Write32(accessAddress & ~3u, regs[rd]);
		}

		currentPC = address + 2;
		return load ? 3 : 2;
	}

	u32 Arm7tdmi::ThumbLoadAddress(u16 opcode)
	{
		// ARM Architecture Reference Manual 4.5.12: ADD Rd, PC/SP, #imm8*4. No flags.
		u32 address = currentPC;
		bool fromStack = (opcode & 0x0800) != 0;
		int rd = (opcode >> 8) & 7;
		u32 offset = (opcode & 0xFF) * 4;

		regs[rd] = (fromStack ? regs[13] : (ReadRegFor(15) & ~3u)) + offset;

		currentPC = address + 2;
		return 1;
	}

	u32 Arm7tdmi::ThumbAddOffsetToSp(u16 opcode)
	{
		// ARM Architecture Reference Manual 4.5.13: ADD/SUB SP, #imm7*4. No flags.
		u32 address = currentPC;
		u32 offset = (opcode & 0x7F) * 4;

		if ((opcode & 0x0080) != 0)
			regs[13] -= offset;
		else
			regs[13] += offset;

		currentPC = address + 2;
		return 1;
	}

	u32 Arm7tdmi::ThumbPushPop(u16 opcode)
	{
		// ARM Architecture Reference Manual 4.5.14: PUSH {Rlist, LR} is STMDB SP!, {...} and POP {Rlist, PC} is
		// LDMIA SP!, {...} - exactly the ARM form of the same transfer. Building that word and
		// running the block transfer keeps the two decoders from drifting apart.
		u32 address = currentPC;
		bool pop = (opcode & 0x0800) != 0;
		bool extra = (opcode & 0x0100) != 0;
		u32 list = opcode & 0xFF;

		if (extra)
			list |= pop ? (1u << 15) : (1u << 14);

		u32 armOpcode = pop
			? (0xE8BD0000u | list)		// LDMIA sp!, {...}
			: (0xE92D0000u | list);		// STMDB sp!, {...}

		u32 cycles = ArmBlockTransfer(armOpcode);

		// The shared decoder moves the PC on by four bytes (an ARM instruction); a Thumb
		// instruction is two bytes long. A POP that loaded r15 branched and has already moved it
		// - including the empty-list oddity, which transfers r15 as well.
		bool branched = pop && (list == 0 || (list & 0x8000) != 0);
		if (!branched)
			currentPC = address + 2;
		return cycles;
	}

	u32 Arm7tdmi::ThumbMultipleTransfer(u16 opcode)
	{
		// ARM Architecture Reference Manual 4.5.15: STMIA/LDMIA Rb!, {Rlist}: the ARM post-indexed block transfer.
		u32 address = currentPC;
		bool load = (opcode & 0x0800) != 0;
		int rb = (opcode >> 8) & 7;
		u32 list = opcode & 0xFF;

		u32 armOpcode = (load ? 0xE8B00000u : 0xE8A00000u) | ((u32)rb << 16) | list;
		u32 cycles = ArmBlockTransfer(armOpcode);

		// Same as PUSH/POP: two bytes, not the four the ARM decoder charges, unless the load
		// brought r15 in (an empty list does that through the ARM7TDMI's oddity).
		bool branched = load && (list == 0 || (list & 0x80) != 0);
		if (!branched)
			currentPC = address + 2;
		return cycles;
	}

	u32 Arm7tdmi::ThumbConditionalBranch(u16 opcode)
	{
		// ARM Architecture Reference Manual 4.5.16: B<cond> label, a signed 8-bit offset counted in halfwords from the
		// pipeline PC. Condition 1110 is undefined; 1111 is the SWI, which the table sends
		// somewhere else.
		u32 address = currentPC;
		u32 condition = (opcode >> 8) & 0xF;

		if (condition == 0xE)
		{
			undefinedCount++;
			Exception(VectorUndefined, ModeUndefined, FlagI);
			return 3;
		}

		if (ConditionPassed(condition))
		{
			u32 offset = SignExtend(opcode & 0xFF, 8) << 1;
			currentPC = (address + 4 + offset) & ~1u;
			return 3;				// 2S + 1N
		}

		currentPC = address + 2;
		return 1;
	}

	u32 Arm7tdmi::ThumbSwi(u16 opcode)
	{
		// GBATEK "SWI": a Thumb SWI's comment is its own 8-bit immediate (the ARM form puts the
		// same number in bits 16-23, which is why Thumb assemblers accept "swi n<<16"). The HLE
		// contract is the one ArmSwi documents: the host returns through the PC (left on the next
		// halfword), and the caller's LR is left untouched.
		u32 address = currentPC;
		u32 comment = opcode & 0xFF;
		u32 returnAddress = address + 2;

		if (bus->HleBiosEnabled)
		{
			currentPC = returnAddress;
		}

		if (bus->Swi(comment))
		{
			// Same as the ARM form: a Thumb BIOS call comes back in Thumb state, whatever the
			// host handler did with the PC.
			SetFlag(FlagT, true);
		}
		else
		{
			// A real BIOS image: the SWI is a real exception and the caller's r14 stays put.
			currentPC = address;
			Exception(VectorSwi, ModeSupervisor, FlagI);
		}

		return 3;
	}

	u32 Arm7tdmi::ThumbUnconditionalBranch(u16 opcode)
	{
		// ARM Architecture Reference Manual 4.5.18: B label, an 11-bit signed offset counted in halfwords.
		u32 address = currentPC;
		u32 offset = SignExtend(opcode & 0x7FF, 11) << 1;

		currentPC = (address + 4 + offset) & ~1u;
		return 3;					// 2S + 1N
	}

	u32 Arm7tdmi::ThumbLongBranchWithLink(u16 opcode)
	{
		// ARM Architecture Reference Manual 4.5.23: BL, two halfwords. The first puts the high part of the offset into LR
		// and the second adds the low part and branches.
		//
		// The return address is the documented ARM7TDMI quirk. the ARM Architecture Reference Manual's pseudo-code adds
		// two to the pipeline PC for the second halfword, which would put LR two bytes past the
		// end of the BL; the ARM7TDMI does not add that 2 and writes (address of the BL + 4) | 1,
		// i.e. the address just past the two halfwords. That is the value every Thumb compiler
		// and every GBA game expects from "bl" - it is what makes "bx lr" return to the
		// instruction after the call - so that is the behaviour implemented here.
		u32 address = currentPC;
		bool second = (opcode & 0x0800) != 0;
		u32 offset = opcode & 0x7FF;

		if (!second)
		{
			// The first halfword runs with the Thumb pipeline PC in r15.
			regs[14] = ReadRegFor(15) + (SignExtend(offset, 11) << 12);
			currentPC = address + 2;
			return 1;
		}

		u32 target = regs[14] + (offset << 1);
		regs[14] = (address + 2) | 1u;		// = (address of the BL + 4) | 1
		currentPC = target & ~1u;			// the branch stays in Thumb

		return 3;					// 2S + 1N
	}
}
