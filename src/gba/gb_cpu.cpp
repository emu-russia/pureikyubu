// The LR35902 interpreter. See gb_cpu.h for the specifications it is written from.
//
// Structure: Step() latches the EI delay and wakes the CPU from HALT, serves the highest priority
// pending interrupt if it may, and otherwise fetches and executes exactly one instruction. Every
// instruction is a direct transcription of one row of the RGBDS gbz80(7) tables / the Pan Docs
// instruction tables; the comments name the row each branch implements and call out the rules that
// are easy to get wrong (the SP-relative half carry, DAA's N/H handling, the rotates that leave Z
// alone, BIT leaving C alone, the HALT behaviour and the interrupt dispatch cost).

#include "gb_cpu.h"

#include <cstdio>

namespace GBA
{
	namespace
	{
		// ---------------------------------------------------------------------------------
		// The M-cycle cost of every opcode, transcribed from the RGBDS gbz80(7) "Cycles"
		// column. The CB-prefixed instructions have their own small table (see ExecuteCb).
		// ---------------------------------------------------------------------------------

		const u8 Cycles[256] =
		{
			1, 3, 2, 2, 1, 1, 2, 1, 5, 2, 2, 2, 1, 1, 2, 1,	// 0x00 NOP, 0x08 LD (n16),SP
			1, 3, 2, 2, 1, 1, 2, 1, 3, 2, 2, 2, 1, 1, 2, 1,	// 0x10 STOP, 0x18 JR
			3, 3, 2, 2, 1, 1, 2, 1, 3, 2, 2, 2, 1, 1, 2, 1,	// 0x20 JR cc (taken)
			3, 3, 2, 2, 3, 3, 3, 1, 3, 2, 2, 2, 1, 1, 2, 1,	// 0x30 JR cc, INC/DEC (HL), SCF
			1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,	// 0x40 LD r8,r8
			1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
			1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
			1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
			1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,	// 0x80 ADD A,r8
			1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
			1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
			1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
			5, 3, 4, 4, 6, 4, 2, 4, 5, 4, 4, 2, 6, 6, 2, 4,	// 0xC0 RET cc, POP/PUSH, JP/CALL/RST
			5, 3, 4, 1, 6, 4, 2, 4, 5, 4, 4, 1, 6, 1, 2, 4,	// 0xD0 (0xD3/0xDB/0xDD are illegal)
			3, 3, 2, 1, 1, 4, 2, 4, 4, 1, 4, 1, 1, 1, 2, 4,	// 0xE0 LDH (n8),A .. 0xEF
			3, 3, 2, 1, 1, 4, 2, 4, 3, 2, 4, 1, 1, 1, 2, 4,	// 0xF0 LDH A,(n8) .. 0xFF
		};

		/// <summary>The five interrupt vectors, in priority order (Pan Docs "Interrupts": bit 0,
		/// VBlank, has the highest priority).</summary>
		const u16 InterruptVectors[5] = { 0x0040, 0x0048, 0x0050, 0x0058, 0x0060 };

		/// <summary>
		/// The condition field of the branch family (Pan Docs "CPU Instruction Set", the cond
		/// table): 00 = NZ, 01 = Z, 10 = NC, 11 = C. The upper bit of the encoding picks whether
		/// the *carry* is tested rather than the zero flag, which is easy to get wrong.
		/// </summary>
		bool ConditionTrue(u8 condition, u8 flags)
		{
			switch (condition & 0x03)
			{
			case 0: return (flags & GbFlagZ) == 0;
			case 1: return (flags & GbFlagZ) != 0;
			case 2: return (flags & GbFlagC) == 0;
			default: return (flags & GbFlagC) != 0;
			}
		}
	}

	void GbCpu::Reset()
	{
		// The power-up state: the boot ROM starts at 0x0000 with the stack at the top of HRAM.
		a = f = b = c = d = e = h = l = 0x00;
		sp = 0xFFFE;
		pc = 0x0000;
		ime = false;
		halted = false;
		stopped = false;
		imePending = false;
	}

	void GbCpu::LoadPostBootRegisters(u8 aValue, u8 headerChecksum, u8 fValue)
	{
		// "The Cartridge Header" / "Power Up Sequence": when the boot ROM jumps to 0x0100 the
		// registers carry these values. A is 0x01 for a DMG cartridge, 0x11 for a CGB cartridge
		// on a CGB and 0x00 for a DMG cartridge on a CGB (see GbSystem::PrepareBootState); C is
		// the header checksum (0x014D) and F is 0xB0 on a DMG for a normal cartridge.
		a = aValue;
		f = fValue;
		b = 0x00;
		c = headerChecksum;
		d = 0x00;
		e = 0xD8;
		h = 0x01;
		l = 0x4D;			// HL = 0x014D, the address of the header checksum
		sp = 0xFFFE;
		pc = 0x0100;
		ime = false;
		halted = false;
		stopped = false;
		imePending = false;
	}

	std::string GbCpu::Describe() const
	{
		char text[128];
		snprintf(text, sizeof(text), "AF=%02X%02X BC=%02X%02X DE=%02X%02X HL=%02X%02X SP=%04X PC=%04X%s%s",
			a, f, b, c, d, e, h, l, sp, pc,
			ime ? " IME" : "", halted ? " HALT" : (stopped ? " STOP" : ""));
		return text;
	}

	// ---------------------------------------------------------------------------------------
	// The stack
	// ---------------------------------------------------------------------------------------

	void GbCpu::PushByte(u8 value)
	{
		// PUSH/CALL decrement SP first, then store (gbz80(7) "PUSH r16").
		sp--;
		Write(sp, value);
	}

	void GbCpu::PushWord(u16 value)
	{
		PushByte((u8)(value >> 8));
		PushByte((u8)(value & 0xFF));
	}

	u8 GbCpu::PopByte()
	{
		u8 value = Read(sp);
		sp++;
		return value;
	}

	u16 GbCpu::PopWord()
	{
		u8 low = PopByte();
		u8 high = PopByte();
		return (u16)((high << 8) | low);
	}

	// ---------------------------------------------------------------------------------------
	// Fetching
	// ---------------------------------------------------------------------------------------

	u8 GbCpu::Fetch8()
	{
		return Read(pc++);
	}

	u16 GbCpu::Fetch16()
	{
		// The Game Boy is little-endian (Pan Docs): the low byte comes first.
		u8 low = Fetch8();
		u8 high = Fetch8();
		return (u16)((high << 8) | low);
	}

	// ---------------------------------------------------------------------------------------
	// The r8 operands
	// ---------------------------------------------------------------------------------------

	u8 GbCpu::ReadR8(int index)
	{
		switch (index)
		{
		case 0: return b;
		case 1: return c;
		case 2: return d;
		case 3: return e;
		case 4: return h;
		case 5: return l;
		case 6: return Read(HL());
		default: return a;
		}
	}

	void GbCpu::WriteR8(int index, u8 value)
	{
		switch (index)
		{
		case 0: b = value; break;
		case 1: c = value; break;
		case 2: d = value; break;
		case 3: e = value; break;
		case 4: h = value; break;
		case 5: l = value; break;
		case 6: Write(HL(), value); break;
		default: a = value; break;
		}
	}

	// ---------------------------------------------------------------------------------------
	// The instruction loop
	// ---------------------------------------------------------------------------------------

	int GbCpu::Step()
	{
		if (bus == nullptr)
			return 1;

		// EI's effect is delayed by one instruction ("Interrupts": "The effect of ei is delayed
		// by one instruction"), so the flag the previous instruction set becomes IME here and the
		// interrupt may only be taken after the instruction that follows EI has run.
		bool imeJustEnabled = false;
		if (imePending)
		{
			imePending = false;
			ime = true;
			imeJustEnabled = true;
		}

		u16 pending = (u16)(Read(0xFFFF) & Read(0xFF0F) & 0x1F);

		// Waking up: with IME = 0 the CPU resumes regular execution as soon as an interrupt
		// becomes pending, but the handler is not called ("halt").
		if (halted && pending != 0)
			halted = false;

		// An interrupt is serviced before the next instruction, when IME and IE both allow it.
		if (ime && pending != 0 && !imeJustEnabled)
			return ServiceInterrupt((u8)pending);

		if (stopped)
		{
			// STOP mode: the CPU sits idle until the machine says a button was pressed (see
			// GbBus::WakeFromStop). The machine keeps ticking the rest of the system.
			return 1;
		}

		if (halted)
		{
			// HALT with no pending interrupt: the CPU does nothing until one appears. The machine
			// is responsible for making time pass (GbSystem::RunCycles).
			return 1;
		}

		u8 opcode;
		if (haltBug)
		{
			// The halt bug: this one opcode fetch does not advance PC, so the byte after the HALT
			// is executed twice ("halt"). Only the fetch is affected; the instruction's operand
			// fetches behave normally.
			haltBug = false;
			opcode = Read(pc);
		}
		else
		{
			opcode = Fetch8();
		}

		int cycles = Cycles[opcode];
		Execute(opcode, cycles);

		if (halted && !stopped)
		{
			// HALT was just executed (the opcode fetch already moved PC past it). With IME = 0 and
			// an interrupt already pending the "halt bug" fires ("halt"): the instruction ends
			// immediately and the *next* opcode fetch does not advance PC, so the byte after the
			// HALT is read a second time and its instruction runs twice. With IME = 1, or with no
			// interrupt pending, execution continues normally at the byte after the HALT.
			if (!ime && (u16)(Read(0xFFFF) & Read(0xFF0F) & 0x1F) != 0)
			{
				halted = false;
				haltBug = true;
			}
		}

		return cycles;
	}

	int GbCpu::ServiceInterrupt(u8 pending)
	{
		// "Interrupt handling": reset the IF bit and IME, push PC and jump to the vector. The
		// whole dispatch lasts 5 M-cycles (two wait states, the two byte push, the jump).
		for (int bit = 0; bit < 5; bit++)
		{
			if ((pending & (1 << bit)) == 0)
				continue;

			// Acknowledge the interrupt by clearing its IF bit.
			Write(0xFF0F, (u8)(Read(0xFF0F) & ~(1 << bit)));

			ime = false;
			imePending = false;
			halted = false;
			stopped = false;
			haltBug = false;

			PushWord(pc);
			pc = InterruptVectors[bit];
			return 5;
		}

		return 1;
	}

	// ---------------------------------------------------------------------------------------
	// The flag rules
	// ---------------------------------------------------------------------------------------

	void GbCpu::SetZFromResult(u8 result)
	{
		f = 0;
		if (result == 0)
			f |= GbFlagZ;
	}

	void GbCpu::AddA(u8 value, bool withCarry)
	{
		// ADD/ADC A,r8 (gbz80(7)): Z from the result, N = 0, H from bit 3, C from bit 7.
		int carry = (withCarry && (f & GbFlagC)) ? 1 : 0;
		int result = a + value + carry;
		f = 0;
		if ((result & 0xFF) == 0)
			f |= GbFlagZ;
		if (((a & 0x0F) + (value & 0x0F) + carry) > 0x0F)
			f |= GbFlagH;
		if (result > 0xFF)
			f |= GbFlagC;
		a = (u8)result;
	}

	void GbCpu::SubA(u8 value, bool withCarry, bool store)
	{
		// SUB/SBC A,r8/CP A,r8 (gbz80(7)): Z from the result, N = 1, H from bit 4, C from
		// bit 7. CP is the same arithmetic without storing the result.
		int carry = (withCarry && (f & GbFlagC)) ? 1 : 0;
		int result = a - value - carry;
		f = 0;
		if ((result & 0xFF) == 0)
			f |= GbFlagZ;
		f |= GbFlagN;
		if (((a & 0x0F) - (value & 0x0F) - carry) < 0)
			f |= GbFlagH;
		if (result < 0)
			f |= GbFlagC;
		if (store)
			a = (u8)result;
	}

	void GbCpu::AndA(u8 value)
	{
		// AND A,r8: N = 0, H = 1, C = 0.
		a &= value;
		f = 0;
		if (a == 0)
			f |= GbFlagZ;
		f |= GbFlagH;
	}

	void GbCpu::XorA(u8 value)
	{
		// XOR A,r8: all four flags are cleared but Z, which comes from the result.
		a ^= value;
		f = 0;
		if (a == 0)
			f |= GbFlagZ;
	}

	void GbCpu::OrA(u8 value)
	{
		// OR A,r8: N = H = C = 0, Z from the result.
		a |= value;
		f = 0;
		if (a == 0)
			f |= GbFlagZ;
	}

	void GbCpu::IncR8(int index)
	{
		// INC r8 (gbz80(7)): Z from the result, N = 0, H from bit 3. C is not affected.
		u8 value = ReadR8(index);
		u8 result = (u8)(value + 1);
		f = (u8)(f & GbFlagC);
		if (result == 0)
			f |= GbFlagZ;
		if ((value & 0x0F) == 0x0F)
			f |= GbFlagH;
		WriteR8(index, result);
	}

	void GbCpu::DecR8(int index)
	{
		// DEC r8 (gbz80(7)): Z from the result, N = 1, H from bit 4. C is not affected.
		u8 value = ReadR8(index);
		u8 result = (u8)(value - 1);
		f = (u8)((f & GbFlagC) | GbFlagN);
		if (result == 0)
			f |= GbFlagZ;
		if ((value & 0x0F) == 0x00)
			f |= GbFlagH;
		WriteR8(index, result);
	}

	void GbCpu::AddHLR16(u16 value)
	{
		// ADD HL,r16 (gbz80(7)): N = 0, H from bit 11, C from bit 15. Z is not affected.
		u16 current = HL();
		u32 result = (u32)current + value;
		f = (u8)(f & GbFlagZ);
		if (((current & 0x0FFF) + (value & 0x0FFF)) > 0x0FFF)
			f |= GbFlagH;
		if (result > 0xFFFF)
			f |= GbFlagC;
		SetHL((u16)result);
	}

	void GbCpu::AddSPe8(s8 offset)
	{
		// ADD SP,e8 (gbz80(7)): H it set if the addition overflows from bit 3 *of the low byte*
		// and C if it overflows from bit 7 of it, i.e. the flags describe the 8-bit addition of
		// the offset to the low byte of SP, not the 16-bit result. Z and N are cleared. This is
		// one of the two SP-relative flag quirks.
		u8 low = (u8)(sp & 0xFF);
		u8 value = (u8)offset;
		f = 0;
		if (((low & 0x0F) + (value & 0x0F)) > 0x0F)
			f |= GbFlagH;
		if ((int)low + (int)value > 0xFF)
			f |= GbFlagC;
		sp = (u16)(sp + offset);
	}

	void GbCpu::LdHLSPe8(s8 offset)
	{
		// LD HL,SP+e8 (gbz80(7)): the same flag rule as ADD SP,e8.
		u8 low = (u8)(sp & 0xFF);
		u8 value = (u8)offset;
		f = 0;
		if (((low & 0x0F) + (value & 0x0F)) > 0x0F)
			f |= GbFlagH;
		if ((int)low + (int)value > 0xFF)
			f |= GbFlagC;
		SetHL((u16)(sp + offset));
	}

	void GbCpu::Daa()
	{
		// DAA (Pan Docs "CPU Registers and Flags" and gbz80(7)): correct A to a BCD value using
		// the N, H and C flags. After an addition (N = 0) a digit above 9 or a half carry adds 6
		// to the low digit; the same test on the whole byte adds 0x60 to the high digit. After a
		// subtraction (N = 1) the correction is subtracted instead. H is always cleared and C is
		// left set once it was set (or set by the high digit correction).
		int correction = 0;
		bool carry = (f & GbFlagC) != 0;

		if ((f & GbFlagN) == 0)
		{
			if ((f & GbFlagH) || (a & 0x0F) > 0x09)
				correction |= 0x06;
			if (carry || a > 0x99)
			{
				correction |= 0x60;
				carry = true;
			}
		}
		else
		{
			if (f & GbFlagH)
				correction |= 0x06;
			if (carry)
				correction |= 0x60;
		}

		a = (u8)(a + ((f & GbFlagN) ? -correction : correction));

		// DAA sets Z from the result, clears H, decides C from the correction and leaves N alone
		// (its own N is what tells a later DAA which way to correct).
		f = (u8)(f & GbFlagN);
		if (a == 0)
			f |= GbFlagZ;
		if (carry)
			f |= GbFlagC;
	}

	// ---------------------------------------------------------------------------------------
	// The CB prefix: shifts, rotates, swaps and the bit operations
	// ---------------------------------------------------------------------------------------

	u8 GbCpu::ShiftOp(int operation, u8 value)
	{
		// The CB family (gbz80(7)): every operation here sets Z from its result and clears N and
		// H; BIT (which the caller handles) leaves C alone.
		switch (operation)
		{
		case 0:		// RLC r8:   C <- [7 <- 0] <- [7]
		{
			u8 carry = (u8)(value >> 7);
			u8 result = (u8)((value << 1) | carry);
			SetZFromResult(result);
			if (carry)
				f |= GbFlagC;
			return result;
		}
		case 1:		// RRC r8:   [0] -> [7 -> 0] -> C
		{
			u8 carry = (u8)(value & 1);
			u8 result = (u8)((value >> 1) | (carry << 7));
			SetZFromResult(result);
			if (carry)
				f |= GbFlagC;
			return result;
		}
		case 2:		// RL r8:    C <- [7 <- 0] <- C
		{
			u8 carry = (u8)(value >> 7);
			u8 result = (u8)((value << 1) | ((f & GbFlagC) ? 1 : 0));
			SetZFromResult(result);
			if (carry)
				f |= GbFlagC;
			return result;
		}
		case 3:		// RR r8:    C -> [7 -> 0] -> C
		{
			u8 carry = (u8)(value & 1);
			u8 result = (u8)((value >> 1) | ((f & GbFlagC) ? 0x80 : 0));
			SetZFromResult(result);
			if (carry)
				f |= GbFlagC;
			return result;
		}
		case 4:		// SLA r8:   C <- [7 <- 0] <- 0
		{
			u8 carry = (u8)(value >> 7);
			u8 result = (u8)(value << 1);
			SetZFromResult(result);
			if (carry)
				f |= GbFlagC;
			return result;
		}
		case 5:		// SRA r8:   [7] -> [7 -> 0] -> C (bit 7 is preserved)
		{
			u8 carry = (u8)(value & 1);
			u8 result = (u8)((value >> 1) | (value & 0x80));
			SetZFromResult(result);
			if (carry)
				f |= GbFlagC;
			return result;
		}
		case 6:		// SWAP r8:  the nibbles exchange, every flag cleared but Z
		{
			u8 result = (u8)((value << 4) | (value >> 4));
			SetZFromResult(result);
			return result;
		}
		default:	// SRL r8:   0 -> [7 -> 0] -> C
		{
			u8 carry = (u8)(value & 1);
			u8 result = (u8)(value >> 1);
			SetZFromResult(result);
			if (carry)
				f |= GbFlagC;
			return result;
		}
		}
	}

	void GbCpu::ExecuteCb(int& cycles)
	{
		u8 opcode = Fetch8();
		int index = opcode & 0x07;
		int group = opcode >> 6;
		int operation = (opcode >> 3) & 0x07;

		if (group == 0)
		{
			// rlc/rrc/rl/rr/sla/sra/swap/srl r8: 2 M-cycles for a register, 4 for (HL).
			u8 value = ReadR8(index);
			WriteR8(index, ShiftOp(operation, value));
			cycles = (index == 6) ? 4 : 2;
			return;
		}

		if (group == 1)
		{
			// BIT u3,r8: Z is set when the tested bit is zero; N is cleared, H is set and C is
			// left alone, which is why this does not go through SetZFromResult.
			u8 value = ReadR8(index);
			f = (u8)((f & GbFlagC) | GbFlagH);
			if ((value & (1 << operation)) == 0)
				f |= GbFlagZ;
			cycles = (index == 6) ? 3 : 2;
			return;
		}

		if (group == 2)
		{
			// RES u3,r8: the flags are untouched.
			WriteR8(index, (u8)(ReadR8(index) & ~(1 << operation)));
			cycles = (index == 6) ? 4 : 2;
			return;
		}

		// SET u3,r8: the flags are untouched.
		WriteR8(index, (u8)(ReadR8(index) | (1 << operation)));
		cycles = (index == 6) ? 4 : 2;
	}

	// ---------------------------------------------------------------------------------------
	// The opcode dispatch
	// ---------------------------------------------------------------------------------------

	void GbCpu::Execute(u8 opcode, int& cycles)
	{
		switch (opcode)
		{
		case 0x00:		// NOP
			break;

		case 0x10:		// STOP: the second byte is part of the instruction. On a CGB a pending
		{				// speed switch (KEY1 bit 0) is performed instead of entering standby; the
			(void)Fetch8();	// machine's hook decides which ("Reducing Power Consumption").
			if (OnStop == nullptr || !OnStop(stopUser))
				stopped = true;
			break;
		}

		case 0x76:		// HALT. Step() finishes it (it needs the pending-interrupt state).
			halted = true;
			break;

		case 0x27: Daa(); break;						// DAA
		case 0x2F:										// CPL: N = H = 1, Z and C untouched
			a = (u8)~a;
			f = (u8)((f & (GbFlagZ | GbFlagC)) | GbFlagN | GbFlagH);
			break;
		case 0x37:										// SCF: C = 1, N = H = 0
			f = (u8)((f & GbFlagZ) | GbFlagC);
			break;
		case 0x3F:										// CCF: C inverted, N = H = 0
			f = (u8)((f & GbFlagZ) | ((f & GbFlagC) ? 0 : GbFlagC));
			break;

		case 0x07:		// RLCA: a plain rotate, unlike CB RLC it always clears Z
		{
			u8 carry = (u8)(a >> 7);
			a = (u8)((a << 1) | carry);
			f = carry ? GbFlagC : 0;
			break;
		}
		case 0x0F:		// RRCA
		{
			u8 carry = (u8)(a & 1);
			a = (u8)((a >> 1) | (carry << 7));
			f = carry ? GbFlagC : 0;
			break;
		}
		case 0x17:		// RLA
		{
			u8 carry = (u8)(a >> 7);
			a = (u8)((a << 1) | ((f & GbFlagC) ? 1 : 0));
			f = carry ? GbFlagC : 0;
			break;
		}
		case 0x1F:		// RRA
		{
			u8 carry = (u8)(a & 1);
			a = (u8)((a >> 1) | ((f & GbFlagC) ? 0x80 : 0));
			f = carry ? GbFlagC : 0;
			break;
		}

		case 0xF3: ime = false; imePending = false; break;	// DI
		case 0xFB: imePending = true; break;				// EI (delayed by one instruction)

		case 0xCB: ExecuteCb(cycles); break;

		case 0x08:		// LD (n16),SP - the documented 16-bit store
		{
			u16 address = Fetch16();
			Write(address, (u8)(sp & 0xFF));
			Write((u16)(address + 1), (u8)(sp >> 8));
			break;
		}

		case 0xE8: AddSPe8((s8)Fetch8()); break;			// ADD SP,e8
		case 0xF8: LdHLSPe8((s8)Fetch8()); break;			// LD HL,SP+e8
		case 0xF9: sp = HL(); break;						// LD SP,HL
		case 0xE9: pc = HL(); break;						// JP HL

		case 0xE0: Write((u16)(0xFF00 | Fetch8()), a); break;	// LDH (n8),A
		case 0xF0: a = Read((u16)(0xFF00 | Fetch8())); break;	// LDH A,(n8)
		case 0xE2: Write((u16)(0xFF00 | c), a); break;			// LD (C),A
		case 0xF2: a = Read((u16)(0xFF00 | c)); break;			// LD A,(C)
		case 0xEA: Write(Fetch16(), a); break;					// LD (n16),A
		case 0xFA: a = Read(Fetch16()); break;					// LD A,(n16)

		case 0x02: Write(BC(), a); break;						// LD (BC),A
		case 0x12: Write(DE(), a); break;						// LD (DE),A
		case 0x0A: a = Read(BC()); break;						// LD A,(BC)
		case 0x1A: a = Read(DE()); break;						// LD A,(DE)
		case 0x22: Write(HL(), a); SetHL((u16)(HL() + 1)); break;	// LD (HL+),A
		case 0x32: Write(HL(), a); SetHL((u16)(HL() - 1)); break;	// LD (HL-),A
		case 0x2A: a = Read(HL()); SetHL((u16)(HL() + 1)); break;	// LD A,(HL+)
		case 0x3A: a = Read(HL()); SetHL((u16)(HL() - 1)); break;	// LD A,(HL-)

		case 0xC3: pc = Fetch16(); break;						// JP n16
		case 0xCD:												// CALL n16
		{
			u16 address = Fetch16();
			PushWord(pc);
			pc = address;
			break;
		}
		case 0xC9: pc = PopWord(); break;						// RET
		case 0xD9:												// RETI: IME is set at once
			pc = PopWord();
			ime = true;
			imePending = false;
			break;

		case 0x18:												// JR e8
		{
			s8 offset = (s8)Fetch8();
			pc = (u16)(pc + offset);
			break;
		}

		case 0xC7: case 0xCF: case 0xD7: case 0xDF:				// RST vec
		case 0xE7: case 0xEF: case 0xF7: case 0xFF:
			PushWord(pc);
			pc = (u16)(opcode & 0x38);
			break;

		case 0xC0: case 0xC8: case 0xD0: case 0xD8:				// RET cc
		{
			bool take = ConditionTrue((u8)((opcode >> 3) & 0x03), f);
			if (take)
			{
				pc = PopWord();
				cycles = 5;
			}
			else
			{
				cycles = 2;
			}
			break;
		}

		case 0xC2: case 0xCA: case 0xD2: case 0xDA:				// JP cc,n16
		{
			bool take = ConditionTrue((u8)((opcode >> 3) & 0x03), f);
			u16 address = Fetch16();
			if (take)
				pc = address;
			else
				cycles = 3;
			break;
		}

		case 0xC4: case 0xCC: case 0xD4: case 0xDC:				// CALL cc,n16
		{
			bool take = ConditionTrue((u8)((opcode >> 3) & 0x03), f);
			u16 address = Fetch16();
			if (take)
			{
				PushWord(pc);
				pc = address;
			}
			else
			{
				cycles = 3;
			}
			break;
		}

		case 0x20: case 0x28: case 0x30: case 0x38:				// JR cc,e8
		{
			bool take = ConditionTrue((u8)((opcode >> 3) & 0x03), f);
			s8 offset = (s8)Fetch8();
			if (take)
			{
				pc = (u16)(pc + offset);
			}
			else
			{
				cycles = 2;
			}
			break;
		}

		case 0xC1:												// POP r16stk
		case 0xD1:
		case 0xE1:
		case 0xF1:
			switch ((opcode >> 4) & 0x03)
			{
			case 0: SetBC(PopWord()); break;
			case 1: SetDE(PopWord()); break;
			case 2: SetHL(PopWord()); break;
			default:
				// POP AF: the low nibble of F always reads back as zero (Pan Docs).
				SetAF((u16)(PopWord() & 0xFFF0));
				break;
			}
			break;

		case 0xC5:												// PUSH r16stk
		case 0xD5:
		case 0xE5:
		case 0xF5:
			switch ((opcode >> 4) & 0x03)
			{
			case 0: PushWord(BC()); break;
			case 1: PushWord(DE()); break;
			case 2: PushWord(HL()); break;
			default: PushWord(AF()); break;
			}
			break;

		case 0x01: case 0x11: case 0x21: case 0x31:				// LD r16,n16
		{
			u16 value = Fetch16();
			switch ((opcode >> 4) & 0x03)
			{
			case 0: SetBC(value); break;
			case 1: SetDE(value); break;
			case 2: SetHL(value); break;
			default: sp = value; break;
			}
			break;
		}

		case 0x03: case 0x13: case 0x23: case 0x33:				// INC r16 (no flags)
			switch ((opcode >> 4) & 0x03)
			{
			case 0: SetBC((u16)(BC() + 1)); break;
			case 1: SetDE((u16)(DE() + 1)); break;
			case 2: SetHL((u16)(HL() + 1)); break;
			default: sp++; break;
			}
			break;

		case 0x0B: case 0x1B: case 0x2B: case 0x3B:				// DEC r16 (no flags)
			switch ((opcode >> 4) & 0x03)
			{
			case 0: SetBC((u16)(BC() - 1)); break;
			case 1: SetDE((u16)(DE() - 1)); break;
			case 2: SetHL((u16)(HL() - 1)); break;
			default: sp--; break;
			}
			break;

		case 0x09: case 0x19: case 0x29: case 0x39:				// ADD HL,r16
			switch ((opcode >> 4) & 0x03)
			{
			case 0: AddHLR16(BC()); break;
			case 1: AddHLR16(DE()); break;
			case 2: AddHLR16(HL()); break;
			default: AddHLR16(sp); break;
			}
			break;

		default:
			// The four blocks the table above does not name explicitly: the register-to-register
			// loads, the (HL) and immediate arithmetic, INC/DEC r8 and LD r8,n8.
			if (opcode >= 0x40 && opcode <= 0x7F)
			{
				int dest = (opcode >> 3) & 0x07;
				int source = opcode & 0x07;
				WriteR8(dest, ReadR8(source));
				break;
			}

			if (opcode >= 0x80 && opcode <= 0xBF)
			{
				// ADD/ADC/SUB/SBC/AND/XOR/OR/CP A,r8: the top three bits select the operation
				// and the low three the operand.
				u8 value = ReadR8(opcode & 0x07);
				switch ((opcode >> 3) & 0x07)
				{
				case 0: AddA(value, false); break;
				case 1: AddA(value, true); break;
				case 2: SubA(value, false, true); break;
				case 3: SubA(value, true, true); break;
				case 4: AndA(value); break;
				case 5: XorA(value); break;
				case 6: OrA(value); break;
				default: SubA(value, false, false); break;
				}
				break;
			}

			if (opcode >= 0x04 && opcode <= 0x3D && (opcode & 0x07) >= 0x04 && (opcode & 0x07) <= 0x05)
			{
				// INC/DEC r8: 00 ddd 100 / 00 ddd 101.
				if ((opcode & 0x07) == 0x04)
					IncR8((opcode >> 3) & 0x07);
				else
					DecR8((opcode >> 3) & 0x07);
				break;
			}

			if (opcode < 0x40 && (opcode & 0x07) == 0x06)
			{
				// LD r8,n8: 00 ddd 110.
				WriteR8((opcode >> 3) & 0x07, Fetch8());
				break;
			}

			switch (opcode)
			{
			case 0xC6: AddA(Fetch8(), false); break;			// ADD A,n8
			case 0xCE: AddA(Fetch8(), true); break;				// ADC A,n8
			case 0xD6: SubA(Fetch8(), false, true); break;		// SUB n8
			case 0xDE: SubA(Fetch8(), true, true); break;		// SBC A,n8
			case 0xE6: AndA(Fetch8()); break;					// AND n8
			case 0xEE: XorA(Fetch8()); break;					// XOR n8
			case 0xF6: OrA(Fetch8()); break;					// OR n8
			case 0xFE: SubA(Fetch8(), false, false); break;		// CP n8
			default:
			{
				// The eleven opcodes the Pan Docs call invalid (0xD3, 0xDB, 0xDD, 0xE3, 0xE4,
				// 0xEB, 0xEC, 0xED, 0xF4, 0xFC, 0xFD) lock a real CPU until it is powered off.
				// This emulator treats them as one M-cycle no-ops and reports them, because
				// hanging the whole frontend on a bad byte is not useful behaviour for a test
				// harness; legal software never executes them.
				GBA::Log(LogLevel::Warn, "gb cpu: illegal opcode %02X at %04X ignored", opcode, (u16)(pc - 1));
				break;
			}
			}
			break;
		}
	}
}
