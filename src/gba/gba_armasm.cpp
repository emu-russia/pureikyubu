// The ARM/Thumb code emitter (see gba_armasm.h).
//
// The encodings below are written from the ARM Architecture Reference Manual (ARM DDI 0100E),
// chapter 5 ("ARM Instruction Set") and appendix A5 ("ARM Instruction Encodings"), and from
// chapter 5 of the same document for the 16-bit Thumb encodings (the "Format 1..19" numbering
// used in the comments is the one of that chapter). Every instruction carries the condition
// field in bits 31-28; the field values are the ones of A3.2 (Table 3-1: EQ = 0000 ... AL = 1110).
//
// Two conventions are worth writing down because the listing depends on them:
//
//   * *Filling*. `Org` moves the program counter forward and zero-fills the gap; it never fills
//     with 0xFF. TakeImage() is the only place that knows the size of the finished image and it
//     pads the tail with 0xFF (what an erased ROM reads as). That way the bytes of the image up
//     to the end of the last emitted item are exactly the bytes the listing describes - useful
//     when the image is disassembled or diffed by hand - and the "erased" look only applies to
//     the unused tail.
//
//   * *The ARM pipeline*. A branch offset is relative to the address of the branch plus 8 (two
//     words of pipeline, A5.6: "the offset is (target - (address + 8)) >> 2"), while a Thumb
//     branch is relative to the address of the branch plus 4 (one halfword of pipeline, A5.1.15
//     and A5.1.18). Both adjustments happen when the fixups are resolved in TakeImage(), so a
//     forward branch works without the caller knowing where the label will land.
//
// Anything the emitter cannot encode - an immediate that no rotation of an 8-bit value builds, a
// register number outside 0-15, a Thumb branch that reaches further than its 8/11-bit offset, an
// unresolved label - throws std::runtime_error. The boot ROM is built at start-up, so a bug in it
// has to be a hard failure instead of a warning in a log nobody reads.

#include "gba_armasm.h"

#include <stdexcept>

namespace GBA
{
	namespace ArmAsm
	{
		// ---------------------------------------------------------------------------------------
		// Text helpers. Nothing here prints: the strings end up in the listing, which the tests
		// and `--dump-bootrom` read back.
		// ---------------------------------------------------------------------------------------

		namespace
		{
			const char* const HexDigits = "0123456789ABCDEF";

			/// <summary>`value` as `digits` upper case hexadecimal characters (no prefix).</summary>
			std::string HexFixed(u32 value, int digits)
			{
				std::string text;
				for (int i = digits - 1; i >= 0; i--)
					text += HexDigits[(value >> (i * 4)) & 0xF];
				return text;
			}

			/// <summary>A constant the way an assembler writes it, with no leading zeros.</summary>
			std::string Hex(u32 value)
			{
				int digits = 1;
				while (digits < 8 && (value >> (digits * 4)) != 0)
					digits++;
				return "0x" + HexFixed(value, digits);
			}

			/// <summary>A signed offset, printed with a sign ("#-0x4", "#0x4").</summary>
			std::string Offset(s32 value)
			{
				if (value < 0)
					return "#-" + Hex((u32)(-value));
				return "#" + Hex((u32)value);
			}

			/// <summary>A register nametag. r13-r15 use the ABI names an assembly listing shows.</summary>
			std::string Reg(int index)
			{
				switch (index)
				{
					case 13: return "sp";
					case 14: return "lr";
					case 15: return "pc";
					default: return "r" + std::to_string(index);
				}
			}

			const char* const ConditionNames[16] =
			{
				"EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC",
				"HI", "LS", "GE", "LT", "GT", "LE", "", "NV",
			};

			/// <summary>The condition suffix ("" for AL, "EQ", "NE", ...).</summary>
			std::string CondText(Cond c)
			{
				return ConditionNames[(u32)c & 0xF];
			}

			/// <summary>`MNEMONIC` + the condition + the S flag, in the ARM Architecture Reference Manual's field order.</summary>
			std::string Mnemonic(const char* name, Cond c, bool setFlags)
			{
				return std::string(name) + CondText(c) + (setFlags ? "S" : "");
			}

			/// <summary>A register list the way an assembler writes it: "{r0-r3, lr}".</summary>
			std::string RegListText(u32 list)
			{
				std::string text = "{";
				bool first = true;
				int i = 0;
				while (i < 16)
				{
					if ((list & (1u << i)) == 0)
					{
						i++;
						continue;
					}
					int end = i;
					while (end + 1 < 16 && (list & (1u << (end + 1))) != 0)
						end++;
					if (!first)
						text += ", ";
					first = false;
					text += Reg(i);
					if (end > i)
						text += "-" + Reg(end);
					i = end + 1;
				}
				text += "}";
				return text;
			}

			// The data processing opcodes (A5.2, opcode field bits 24-21 / the "Op2" table).
			const u32 OpAnd = 0x0;
			const u32 OpEor = 0x1;
			const u32 OpSub = 0x2;
			const u32 OpRsb = 0x3;
			const u32 OpAdd = 0x4;
			const u32 OpOrr = 0xC;
			const u32 OpMov = 0xD;
			const u32 OpBic = 0xE;
			const u32 OpMvn = 0xF;

			/// <summary>A5.2: cond | 00 | I | opcode | S | Rn | Rd | operand2.</summary>
			u32 DataProc(Cond c, u32 opcode, bool immediate, bool setFlags, int rn, int rd, u32 operand2)
			{
				return ((u32)c << 28) | (immediate ? (1u << 25) : 0) | (opcode << 21) |
					(setFlags ? (1u << 20) : 0) | ((u32)rn << 16) | ((u32)rd << 12) | operand2;
			}

			/// <summary>A5.2: the immediate operand2 field, "rotate" in bits 11-8 and imm8 in 7-0.</summary>
			std::string ImmText(u32 value) { return "#" + Hex(value); }

			/// <summary>A5.2: the shifted register operand2 field (bits 11-4) plus Rm (bits 3-0).</summary>
			u32 ShiftedOperand(ArmAsm::Shift shift, u32 amount, int rm, bool& ok, const char* what)
			{
				ok = true;
				u32 type = (u32)shift;
				if (amount > 32)
					ok = false;
				// LSL #0 and ROR #0 are plain moves; "LSL #32" has no encoding (only LSR/ASR
				// read the zero amount as the 32 special case, A5.2 "Shift amount").
				if ((shift == ArmAsm::Shift::LSL || shift == ArmAsm::Shift::ROR) && amount == 32)
					ok = false;
				if (rm < 0 || rm > 15)
					ok = false;
				if (!ok)
					return 0;
				(void)what;
				return ((amount & 0x1F) << 7) | (type << 5) | ((u32)rm & 0xF);
			}

			std::string ShiftText(ArmAsm::Shift shift, u32 amount, int rm)
			{
				const char* names[4] = { "LSL", "LSR", "ASR", "ROR" };
				// A zero amount in the LSR/ASR field means "shift by 32" (A5.2).
				u32 shown = amount;
				if (shown == 0 && (shift == ArmAsm::Shift::LSR || shift == ArmAsm::Shift::ASR))
					shown = 32;
				return std::string(names[(u32)shift & 3]) + " #" + std::to_string(shown);
			}

			[[noreturn]] void ThrowImmediate(const char* what, u32 value)
			{
				throw std::runtime_error("gba_armasm: " + std::string(what) +
					": no rotation builds the immediate " + Hex(value));
			}

			[[noreturn]] void ThrowRegister(const char* what, int reg)
			{
				throw std::runtime_error("gba_armasm: " + std::string(what) +
					": register r" + std::to_string(reg) + " is not a general purpose register");
			}

			[[noreturn]] void ThrowRange(const char* what, const std::string& detail)
			{
				throw std::runtime_error("gba_armasm: " + std::string(what) + ": " + detail);
			}
		}

		// ---------------------------------------------------------------------------------------
		// Location and data
		// ---------------------------------------------------------------------------------------

		Assembler::Assembler()
		{
			code.reserve(0x4000);
		}

		// Move the program counter. Moving *forward* zero-fills the gap (see the note at the top:
		// 0xFF is the erased-ROM value and belongs to TakeImage, not to the middle of an image).
		// Moving backwards is allowed on purpose: it is how a caller patches the words it left
		// behind earlier. The first call decides where the image starts (its origin); a later call
		// below that address cannot be represented and throws.
		void Assembler::Org(u32 address)
		{
			if (code.empty())
				origin = address;
			if (address < origin)
				throw std::runtime_error("gba_armasm: Org(" + Hex(address) +
					") is below the origin of the image (" + Hex(origin) + ")");
			u32 offset = address - origin;
			if (offset > code.size())
				code.resize(offset, 0x00);
			pc = address;
		}

		void Assembler::Label(const std::string& name)
		{
			auto existing = labels.find(name);
			if (existing != labels.end() && existing->second != pc)
				throw std::runtime_error("gba_armasm: the label '" + name + "' is already bound to " +
					Hex(existing->second));
			labels[name] = pc;
		}

		void Assembler::Reserve(u32 count)
		{
			Record(".space " + Hex(count));
			u32 offset = pc - origin;
			if (offset + count > code.size())
				code.resize(offset + count, 0x00);
			pc += count;
		}

		void Assembler::Data8(u8 value)
		{
			Record(".byte " + Hex(value));
			u32 offset = pc - origin;
			if (offset + 1 > code.size())
				code.resize(offset + 1, 0x00);
			code[offset] = value;
			pc += 1;
		}

		void Assembler::Data16(u16 value)
		{
			Record(".hword " + Hex(value));
			u32 offset = pc - origin;
			if (offset + 2 > code.size())
				code.resize(offset + 2, 0x00);
			code[offset + 0] = (u8)(value & 0xFF);
			code[offset + 1] = (u8)(value >> 8);
			pc += 2;
		}

		void Assembler::Data32(u32 value)
		{
			Record(".word " + Hex(value));
			u32 offset = pc - origin;
			if (offset + 4 > code.size())
				code.resize(offset + 4, 0x00);
			code[offset + 0] = (u8)(value & 0xFF);
			code[offset + 1] = (u8)((value >> 8) & 0xFF);
			code[offset + 2] = (u8)((value >> 16) & 0xFF);
			code[offset + 3] = (u8)((value >> 24) & 0xFF);
			pc += 4;
		}

		void Assembler::DataBytes(const void* data, size_t size)
		{
			Record(".bytes " + Hex((u32)size));
			u32 offset = pc - origin;
			if (offset + size > code.size())
				code.resize(offset + size, 0x00);
			const u8* bytes = (const u8*)data;
			for (size_t i = 0; i < size; i++)
				code[offset + i] = bytes[i];
			pc += (u32)size;
		}

		void Assembler::Align(u32 alignment)
		{
			if (alignment == 0 || (alignment & (alignment - 1)) != 0)
				throw std::runtime_error("gba_armasm: Align needs a power of two");
			while (pc % alignment != 0)
				Data8(0x00);
		}

		// ---------------------------------------------------------------------------------------
		// The two low level emitters
		// ---------------------------------------------------------------------------------------

		void Assembler::Record(const std::string& text)
		{
			listing += HexFixed(pc, 8);
			listing += ": ";
			listing += text;
			listing += '\n';
		}

		void Assembler::Emit(u32 word, const std::string& text)
		{
			Record(text);
			u32 offset = pc - origin;
			if (offset + 4 > code.size())
				code.resize(offset + 4, 0x00);
			code[offset + 0] = (u8)(word & 0xFF);
			code[offset + 1] = (u8)((word >> 8) & 0xFF);
			code[offset + 2] = (u8)((word >> 16) & 0xFF);
			code[offset + 3] = (u8)((word >> 24) & 0xFF);
			pc += 4;
		}

		void Assembler::Emit16(u16 halfword, const std::string& text)
		{
			Record(text);
			u32 offset = pc - origin;
			if (offset + 2 > code.size())
				code.resize(offset + 2, 0x00);
			code[offset + 0] = (u8)(halfword & 0xFF);
			code[offset + 1] = (u8)(halfword >> 8);
			pc += 2;
		}

		// A5.2: solve an immediate operand. The instruction computes "imm8 ROR (2*rotate)", so the
		// search rotates `value` *left* by 2*rotate and takes the first rotation whose result fits
		// in 8 bits; that is the encoding an assembler produces (rotate = 0 is preferred, so
		// "ADD r0, r1, #4" keeps its imm8 rather than being written as "1 ROR 30").
		u32 Assembler::EncodeArmImmediate(u32 value, bool& ok) const
		{
			for (u32 rotation = 0; rotation < 16; rotation++)
			{
				u32 amount = rotation * 2;
				u32 imm = (amount == 0) ? value : ((value << amount) | (value >> (32 - amount)));
				if (imm <= 0xFF)
				{
					ok = true;
					return (rotation << 8) | imm;
				}
			}
			ok = false;
			return 0;
		}

		// ---------------------------------------------------------------------------------------
		// Data processing (A5.2)
		// ---------------------------------------------------------------------------------------

		void Assembler::Mov(int rd, u32 imm, Cond c, bool setFlags)
		{
			bool ok = false;
			u32 operand = EncodeArmImmediate(imm, ok);
			if (!ok)
				ThrowImmediate("MOV", imm);
			Emit(DataProc(c, OpMov, true, setFlags, 0, rd, operand),
				Mnemonic("MOV", c, setFlags) + " " + Reg(rd) + ", " + ImmText(imm));
		}

		void Assembler::MovReg(int rd, int rm, Cond c, bool setFlags, Shift shift, u32 amount)
		{
			bool ok = false;
			u32 operand = ShiftedOperand(shift, amount, rm, ok, "MOV");
			if (!ok)
				ThrowRange("MOV", "the shifted register operand cannot be encoded");
			std::string text = Mnemonic("MOV", c, setFlags) + " " + Reg(rd) + ", " + Reg(rm);
			if (amount != 0 || shift != Shift::LSL)
				text += ", " + ShiftText(shift, amount, rm);
			Emit(DataProc(c, OpMov, false, setFlags, 0, rd, operand), text);
		}

		// A "free form" shifted register: MOV is the only data processing opcode whose operand is
		// free of a second source register, so this is exactly MovReg with the MOV opcode.
		void Assembler::ShiftReg(int rd, int rm, Shift shift, u32 amount, Cond c, bool setFlags)
		{
			MovReg(rd, rm, c, setFlags, shift, amount);
		}

		void Assembler::Add(int rd, int rn, u32 imm, Cond c, bool setFlags)
		{
			bool ok = false;
			u32 operand = EncodeArmImmediate(imm, ok);
			if (!ok)
				ThrowImmediate("ADD", imm);
			Emit(DataProc(c, OpAdd, true, setFlags, rn, rd, operand),
				Mnemonic("ADD", c, setFlags) + " " + Reg(rd) + ", " + Reg(rn) + ", " + ImmText(imm));
		}

		void Assembler::AddReg(int rd, int rn, int rm, Cond c, bool setFlags)
		{
			Emit(DataProc(c, OpAdd, false, setFlags, rn, rd, (u32)rm & 0xF),
				Mnemonic("ADD", c, setFlags) + " " + Reg(rd) + ", " + Reg(rn) + ", " + Reg(rm));
		}

		void Assembler::Sub(int rd, int rn, u32 imm, Cond c, bool setFlags)
		{
			bool ok = false;
			u32 operand = EncodeArmImmediate(imm, ok);
			if (!ok)
				ThrowImmediate("SUB", imm);
			Emit(DataProc(c, OpSub, true, setFlags, rn, rd, operand),
				Mnemonic("SUB", c, setFlags) + " " + Reg(rd) + ", " + Reg(rn) + ", " + ImmText(imm));
		}

		void Assembler::SubReg(int rd, int rn, int rm, Cond c, bool setFlags)
		{
			Emit(DataProc(c, OpSub, false, setFlags, rn, rd, (u32)rm & 0xF),
				Mnemonic("SUB", c, setFlags) + " " + Reg(rd) + ", " + Reg(rn) + ", " + Reg(rm));
		}

		void Assembler::Rsb(int rd, int rn, u32 imm, Cond c, bool setFlags)
		{
			bool ok = false;
			u32 operand = EncodeArmImmediate(imm, ok);
			if (!ok)
				ThrowImmediate("RSB", imm);
			Emit(DataProc(c, OpRsb, true, setFlags, rn, rd, operand),
				Mnemonic("RSB", c, setFlags) + " " + Reg(rd) + ", " + Reg(rn) + ", " + ImmText(imm));
		}

		void Assembler::And(int rd, int rn, u32 imm, Cond c, bool setFlags)
		{
			bool ok = false;
			u32 operand = EncodeArmImmediate(imm, ok);
			if (!ok)
				ThrowImmediate("AND", imm);
			Emit(DataProc(c, OpAnd, true, setFlags, rn, rd, operand),
				Mnemonic("AND", c, setFlags) + " " + Reg(rd) + ", " + Reg(rn) + ", " + ImmText(imm));
		}

		void Assembler::AndReg(int rd, int rn, int rm, Cond c, bool setFlags)
		{
			Emit(DataProc(c, OpAnd, false, setFlags, rn, rd, (u32)rm & 0xF),
				Mnemonic("AND", c, setFlags) + " " + Reg(rd) + ", " + Reg(rn) + ", " + Reg(rm));
		}

		void Assembler::Orr(int rd, int rn, u32 imm, Cond c, bool setFlags)
		{
			bool ok = false;
			u32 operand = EncodeArmImmediate(imm, ok);
			if (!ok)
				ThrowImmediate("ORR", imm);
			Emit(DataProc(c, OpOrr, true, setFlags, rn, rd, operand),
				Mnemonic("ORR", c, setFlags) + " " + Reg(rd) + ", " + Reg(rn) + ", " + ImmText(imm));
		}

		void Assembler::OrrReg(int rd, int rn, int rm, Cond c, bool setFlags)
		{
			Emit(DataProc(c, OpOrr, false, setFlags, rn, rd, (u32)rm & 0xF),
				Mnemonic("ORR", c, setFlags) + " " + Reg(rd) + ", " + Reg(rn) + ", " + Reg(rm));
		}

		void Assembler::Eor(int rd, int rn, u32 imm, Cond c, bool setFlags)
		{
			bool ok = false;
			u32 operand = EncodeArmImmediate(imm, ok);
			if (!ok)
				ThrowImmediate("EOR", imm);
			Emit(DataProc(c, OpEor, true, setFlags, rn, rd, operand),
				Mnemonic("EOR", c, setFlags) + " " + Reg(rd) + ", " + Reg(rn) + ", " + ImmText(imm));
		}

		void Assembler::EorReg(int rd, int rn, int rm, Cond c, bool setFlags)
		{
			Emit(DataProc(c, OpEor, false, setFlags, rn, rd, (u32)rm & 0xF),
				Mnemonic("EOR", c, setFlags) + " " + Reg(rd) + ", " + Reg(rn) + ", " + Reg(rm));
		}

		void Assembler::Bic(int rd, int rn, u32 imm, Cond c, bool setFlags)
		{
			bool ok = false;
			u32 operand = EncodeArmImmediate(imm, ok);
			if (!ok)
				ThrowImmediate("BIC", imm);
			Emit(DataProc(c, OpBic, true, setFlags, rn, rd, operand),
				Mnemonic("BIC", c, setFlags) + " " + Reg(rd) + ", " + Reg(rn) + ", " + ImmText(imm));
		}

		void Assembler::Mvn(int rd, u32 imm, Cond c, bool setFlags)
		{
			bool ok = false;
			u32 operand = EncodeArmImmediate(imm, ok);
			if (!ok)
				ThrowImmediate("MVN", imm);
			Emit(DataProc(c, OpMvn, true, setFlags, 0, rd, operand),
				Mnemonic("MVN", c, setFlags) + " " + Reg(rd) + ", " + ImmText(imm));
		}

		// CMP/TST always update the flags and write no register (Rd is 0 in the encoding, A5.2).
		void Assembler::Cmp(int rn, u32 imm, Cond c)
		{
			bool ok = false;
			u32 operand = EncodeArmImmediate(imm, ok);
			if (!ok)
				ThrowImmediate("CMP", imm);
			Emit(DataProc(c, 0xA, true, true, rn, 0, operand),
				Mnemonic("CMP", c, false) + " " + Reg(rn) + ", " + ImmText(imm));
		}

		void Assembler::CmpReg(int rn, int rm, Cond c, Shift shift, u32 amount)
		{
			bool ok = false;
			u32 operand = ShiftedOperand(shift, amount, rm, ok, "CMP");
			if (!ok)
				ThrowRange("CMP", "the shifted register operand cannot be encoded");
			std::string text = Mnemonic("CMP", c, false) + " " + Reg(rn) + ", " + Reg(rm);
			if (amount != 0 || shift != Shift::LSL)
				text += ", " + ShiftText(shift, amount, rm);
			Emit(DataProc(c, 0xA, false, true, rn, 0, operand), text);
		}

		void Assembler::Tst(int rn, u32 imm, Cond c)
		{
			bool ok = false;
			u32 operand = EncodeArmImmediate(imm, ok);
			if (!ok)
				ThrowImmediate("TST", imm);
			Emit(DataProc(c, 0x8, true, true, rn, 0, operand),
				Mnemonic("TST", c, false) + " " + Reg(rn) + ", " + ImmText(imm));
		}

		void Assembler::TstReg(int rn, int rm, Cond c)
		{
			Emit(DataProc(c, 0x8, false, true, rn, 0, (u32)rm & 0xF),
				Mnemonic("TST", c, false) + " " + Reg(rn) + ", " + Reg(rm));
		}

		// A5.3: MUL/MLA. cond | 000000 | A | S | Rd | Rn | Rs | 1001 | Rm. MUL is the A = 0 form,
		// so the Rn field (bits 15-12) is zero.
		void Assembler::Mul(int rd, int rm, int rs, Cond c, bool setFlags)
		{
			if (rd == 15 || rm == 15 || rs == 15)
				ThrowRegister("MUL", 15);
			u32 word = ((u32)c << 28) | (setFlags ? (1u << 20) : 0) | ((u32)rd << 16) |
				((u32)rs << 8) | 0x90 | ((u32)rm & 0xF);
			Emit(word, Mnemonic("MUL", c, setFlags) + " " + Reg(rd) + ", " + Reg(rm) + ", " + Reg(rs));
		}

		// A5.3: UMULL. cond | 00001 | U | A | S | RdHi | RdLo | Rs | 1001 | Rm, with U = 0, A = 0.
		void Assembler::Umull(int rdLo, int rdHi, int rm, int rs, Cond c, bool setFlags)
		{
			if (rdLo == 15 || rdHi == 15 || rm == 15 || rs == 15)
				ThrowRegister("UMULL", 15);
			if (rdLo == rdHi)
				ThrowRange("UMULL", "the low and the high destination must differ");
			u32 word = ((u32)c << 28) | (1u << 23) | (setFlags ? (1u << 20) : 0) | ((u32)rdHi << 16) |
				((u32)rdLo << 12) | ((u32)rs << 8) | 0x90 | ((u32)rm & 0xF);
			Emit(word, Mnemonic("UMULL", c, setFlags) + " " + Reg(rdLo) + ", " + Reg(rdHi) + ", " +
				Reg(rm) + ", " + Reg(rs));
		}

		// ---------------------------------------------------------------------------------------
		// Loads and stores (A5.4 single data transfer, A5.5 halfword and signed transfer)
		// ---------------------------------------------------------------------------------------

		// A5.4: cond | 01 | I | P | U | B | W | L | Rn | Rd | offset12. Pre-indexed without
		// writeback (P = 1, W = 0): "LDR rd, [rn, #+/-offset]".
		void Assembler::Ldr(int rd, int rn, int offset, Cond c, bool byte)
		{
			u32 magnitude = (offset < 0) ? (u32)(-offset) : (u32)offset;
			if (magnitude > 0xFFF)
				ThrowRange(byte ? "LDRB" : "LDR", "the offset does not fit in 12 bits");
			u32 word = ((u32)c << 28) | 0x04000000 | (1u << 24) | ((offset < 0) ? 0 : (1u << 23)) |
				(byte ? (1u << 22) : 0) | (1u << 20) | ((u32)rn << 16) | ((u32)rd << 12) | magnitude;
			Emit(word, Mnemonic(byte ? "LDRB" : "LDR", c, false) + " " + Reg(rd) + ", [" + Reg(rn) +
				", " + Offset(offset) + "]");
		}

		void Assembler::Str(int rd, int rn, int offset, Cond c, bool byte)
		{
			u32 magnitude = (offset < 0) ? (u32)(-offset) : (u32)offset;
			if (magnitude > 0xFFF)
				ThrowRange(byte ? "STRB" : "STR", "the offset does not fit in 12 bits");
			u32 word = ((u32)c << 28) | 0x04000000 | (1u << 24) | ((offset < 0) ? 0 : (1u << 23)) |
				(byte ? (1u << 22) : 0) | ((u32)rn << 16) | ((u32)rd << 12) | magnitude;
			Emit(word, Mnemonic(byte ? "STRB" : "STR", c, false) + " " + Reg(rd) + ", [" + Reg(rn) +
				", " + Offset(offset) + "]");
		}

		// A5.4: the same, with I = 1 and the offset in Rm (no shift: bits 11-4 are zero).
		void Assembler::LdrReg(int rd, int rn, int rm, Cond c, bool byte)
		{
			u32 word = ((u32)c << 28) | 0x04000000 | (1u << 25) | (1u << 24) | (1u << 23) |
				(byte ? (1u << 22) : 0) | (1u << 20) | ((u32)rn << 16) | ((u32)rd << 12) | ((u32)rm & 0xF);
			Emit(word, Mnemonic(byte ? "LDRB" : "LDR", c, false) + " " + Reg(rd) + ", [" + Reg(rn) +
				", " + Reg(rm) + "]");
		}

		void Assembler::StrReg(int rd, int rn, int rm, Cond c, bool byte)
		{
			u32 word = ((u32)c << 28) | 0x04000000 | (1u << 25) | (1u << 24) | (1u << 23) |
				(byte ? (1u << 22) : 0) | ((u32)rn << 16) | ((u32)rd << 12) | ((u32)rm & 0xF);
			Emit(word, Mnemonic(byte ? "STRB" : "STR", c, false) + " " + Reg(rd) + ", [" + Reg(rn) +
				", " + Reg(rm) + "]");
		}

		// A5.5: cond | 000 | P | U | I | W | L | Rn | Rd | offsetHi | 1 S H 1 | offsetLo. P = 1 and
		// W = 0 again; the immediate form (I = 1) splits the 8-bit offset around the SH bits. The
		// transfer size is picked by S/H: 01 = halfword, 10 = signed byte, 11 = signed halfword.
		void Assembler::Ldrh(int rd, int rn, int offset, Cond c)
		{
			u32 magnitude = (offset < 0) ? (u32)(-offset) : (u32)offset;
			if (magnitude > 0xFF)
				ThrowRange("LDRH", "the offset does not fit in 8 bits");
			u32 word = ((u32)c << 28) | (1u << 24) | ((offset < 0) ? 0 : (1u << 23)) |
				(1u << 22) | (1u << 20) | ((u32)rn << 16) | ((u32)rd << 12) |
				((magnitude & 0xF0) << 4) | 0xB0 | (magnitude & 0xF);
			Emit(word, Mnemonic("LDRH", c, false) + " " + Reg(rd) + ", [" + Reg(rn) + ", " +
				Offset(offset) + "]");
		}

		void Assembler::Strh(int rd, int rn, int offset, Cond c)
		{
			u32 magnitude = (offset < 0) ? (u32)(-offset) : (u32)offset;
			if (magnitude > 0xFF)
				ThrowRange("STRH", "the offset does not fit in 8 bits");
			u32 word = ((u32)c << 28) | (1u << 24) | ((offset < 0) ? 0 : (1u << 23)) |
				(1u << 22) | ((u32)rn << 16) | ((u32)rd << 12) |
				((magnitude & 0xF0) << 4) | 0xB0 | (magnitude & 0xF);
			Emit(word, Mnemonic("STRH", c, false) + " " + Reg(rd) + ", [" + Reg(rn) + ", " +
				Offset(offset) + "]");
		}

		void Assembler::Ldrsb(int rd, int rn, int offset, Cond c)
		{
			u32 magnitude = (offset < 0) ? (u32)(-offset) : (u32)offset;
			if (magnitude > 0xFF)
				ThrowRange("LDRSB", "the offset does not fit in 8 bits");
			u32 word = ((u32)c << 28) | (1u << 24) | ((offset < 0) ? 0 : (1u << 23)) |
				(1u << 22) | (1u << 20) | ((u32)rn << 16) | ((u32)rd << 12) |
				((magnitude & 0xF0) << 4) | 0xD0 | (magnitude & 0xF);
			Emit(word, Mnemonic("LDRSB", c, false) + " " + Reg(rd) + ", [" + Reg(rn) + ", " +
				Offset(offset) + "]");
		}

		void Assembler::Ldrsh(int rd, int rn, int offset, Cond c)
		{
			u32 magnitude = (offset < 0) ? (u32)(-offset) : (u32)offset;
			if (magnitude > 0xFF)
				ThrowRange("LDRSH", "the offset does not fit in 8 bits");
			u32 word = ((u32)c << 28) | (1u << 24) | ((offset < 0) ? 0 : (1u << 23)) |
				(1u << 22) | (1u << 20) | ((u32)rn << 16) | ((u32)rd << 12) |
				((magnitude & 0xF0) << 4) | 0xF0 | (magnitude & 0xF);
			Emit(word, Mnemonic("LDRSH", c, false) + " " + Reg(rd) + ", [" + Reg(rn) + ", " +
				Offset(offset) + "]");
		}

		// A5.6: cond | 100 | P | U | S | W | L | Rn | register_list. LDMIA/STMIA is P = 0, U = 1.
		// The S bit (bit 22, the "^" of "LDMIA rn!, {...}^") is not reachable through the frozen
		// interface of gba_armasm.h, which only has the writeback flag; it is never set here.
		// The boot ROM's exception return uses "SUBS pc, lr, #4" instead of an LDM with the caret.
		void Assembler::Ldmia(int rn, u32 regList, Cond c, bool writeBack)
		{
			u32 word = ((u32)c << 28) | 0x08000000 | (1u << 23) | (writeBack ? (1u << 21) : 0) |
				(1u << 20) | ((u32)rn << 16) | (regList & 0xFFFF);
			Emit(word, Mnemonic("LDMIA", c, false) + " " + Reg(rn) + (writeBack ? "!" : "") + ", " +
				RegListText(regList));
		}

		void Assembler::Stmia(int rn, u32 regList, Cond c, bool writeBack)
		{
			u32 word = ((u32)c << 28) | 0x08000000 | (1u << 23) | (writeBack ? (1u << 21) : 0) |
				((u32)rn << 16) | (regList & 0xFFFF);
			Emit(word, Mnemonic("STMIA", c, false) + " " + Reg(rn) + (writeBack ? "!" : "") + ", " +
				RegListText(regList));
		}

		// ---------------------------------------------------------------------------------------
		// Branches (A5.7 - A5.10)
		// ---------------------------------------------------------------------------------------

		void Assembler::B(const std::string& label, Cond c)
		{
			Fixup fixup;
			fixup.address = pc;
			fixup.label = label;
			fixup.thumb = false;
			fixup.link = false;
			fixup.conditional = (c != Cond::AL);
			fixup.condition = (u32)c;
			fixup.blx = false;
			fixups.push_back(fixup);
			Emit(((u32)c << 28) | 0x0A000000, Mnemonic("B", c, false) + " " + label);
		}

		void Assembler::Bl(const std::string& label, Cond c)
		{
			Fixup fixup;
			fixup.address = pc;
			fixup.label = label;
			fixup.thumb = false;
			fixup.link = true;
			fixup.conditional = (c != Cond::AL);
			fixup.condition = (u32)c;
			fixup.blx = false;
			fixups.push_back(fixup);
			Emit(((u32)c << 28) | 0x0B000000, Mnemonic("BL", c, false) + " " + label);
		}

		// A5.7: BX. cond | 0001 0010 1111 1111 1111 0001 | Rm.
		void Assembler::Bx(int rm, Cond c)
		{
			if (rm < 0 || rm > 15)
				ThrowRegister("BX", rm);
			Emit(((u32)c << 28) | 0x012FFF10 | ((u32)rm & 0xF),
				Mnemonic("BX", c, false) + " " + Reg(rm));
		}

		// BLX (register) is the ARMv5T encoding cond | 0001 0010 1111 1111 1111 0011 | Rm. The
		// emulated core is an ARM7TDMI (ARMv4T, which has BX but not BLX) and the boot ROM never
		// uses it; it is here because gba_armasm.h declares it.
		void Assembler::Blx(int rm, Cond c)
		{
			if (rm < 0 || rm > 15)
				ThrowRegister("BLX", rm);
			Emit(((u32)c << 28) | 0x012FFF30 | ((u32)rm & 0xF),
				Mnemonic("BLX", c, false) + " " + Reg(rm));
		}

		// A5.8: SWI. cond | 1111 | comment24. The GBA BIOS's SWI dispatcher reads the function
		// number from the byte at [lr - 2], which is bits 23-16 of the instruction (GBATEK "BIOS
		// Functions": the SWI handler does "LDRB r0, [lr, #-2]"), so the comment - the BIOS
		// function number, 00h..2Fh - is placed in that byte. That is the encoding GBA ROMs use:
		// "SWI 05h" is EF050000h.
		void Assembler::Swi(u32 comment, Cond c)
		{
			if (comment > 0xFF)
				ThrowRange("SWI", "the GBA BIOS function number is one byte");
			Emit(((u32)c << 28) | 0x0F000000 | ((comment & 0xFF) << 16),
				Mnemonic("SWI", c, false) + " #" + Hex(comment & 0xFF));
		}

		// A5.9: BKPT. The encoding is 0xE1200070 (cond = 1110, which is why the condition field is
		// not settable) with the 16-bit comment split as imm12 in bits 19-8 and imm4 in bits 3-0.
		// The harness uses the immediate as a small identifier (the semihost hook), so the low
		// nibble is where it goes; anything above 15 does not fit that field and throws.
		void Assembler::Bkpt(u32 immediate)
		{
			if (immediate > 0xF)
				ThrowRange("BKPT", "the immediate does not fit the 4-bit field of A5.9");
			Emit(0xE1200070 | (immediate & 0xF), "BKPT #" + Hex(immediate & 0xF));
		}

		u32 Assembler::LiteralPool(const std::vector<u32>& words)
		{
			u32 address = pc;
			for (u32 word : words)
				Data32(word);
			return address;
		}

		// ---------------------------------------------------------------------------------------
		// Thumb (ARM DDI 0100E chapter 5, the "Format n" encodings)
		// ---------------------------------------------------------------------------------------

		void Assembler::ThumbMov(int rd, u32 imm8)
		{
			if (rd < 0 || rd > 7 || imm8 > 0xFF)
				ThrowRange("Thumb MOV", "format 3 (A5.1.3) needs Rd 0-7 and an 8-bit immediate");
			Emit16((u16)(0x2000 | (rd << 8) | imm8), "MOV " + Reg(rd) + ", #" + Hex(imm8));
		}

		// Format 5 (A5.1.5): the high register operations. Bit 7 is the high bit of Rd, bit 6 the
		// high bit of Rs, so MOV can reach r8-r15.
		void Assembler::ThumbMovReg(int rd, int rm)
		{
			if (rd < 0 || rd > 15 || rm < 0 || rm > 15)
				ThrowRange("Thumb MOV", "format 5 (A5.1.5) needs registers 0-15");
			Emit16((u16)(0x4600 | ((rd >> 3) << 7) | ((rm >> 3) << 6) | ((rm & 7) << 3) | (rd & 7)),
				"MOV " + Reg(rd) + ", " + Reg(rm));
		}

		// Format 2 (A5.1.2): ADD Rd, Rn, Rm. The register form is op = 0 with bit 10 (I) = 0.
		void Assembler::ThumbAdd(int rd, int rn, int rm)
		{
			if (rd < 0 || rd > 7 || rn < 0 || rn > 7 || rm < 0 || rm > 7)
				ThrowRange("Thumb ADD", "format 2 (A5.1.2) needs registers 0-7");
			Emit16((u16)(0x1800 | (rn << 6) | (rm << 3) | rd),
				"ADD " + Reg(rd) + ", " + Reg(rn) + ", " + Reg(rm));
		}

		// Format 3 (A5.1.3): ADD Rd, #imm8 (opcode 10).
		void Assembler::ThumbAddImm(int rd, u32 imm8)
		{
			if (rd < 0 || rd > 7 || imm8 > 0xFF)
				ThrowRange("Thumb ADD", "format 3 (A5.1.3) needs Rd 0-7 and an 8-bit immediate");
			Emit16((u16)(0x3000 | (rd << 8) | imm8), "ADD " + Reg(rd) + ", #" + Hex(imm8));
		}

		void Assembler::ThumbSubImm(int rd, u32 imm8)
		{
			if (rd < 0 || rd > 7 || imm8 > 0xFF)
				ThrowRange("Thumb SUB", "format 3 (A5.1.3) needs Rd 0-7 and an 8-bit immediate");
			Emit16((u16)(0x3800 | (rd << 8) | imm8), "SUB " + Reg(rd) + ", #" + Hex(imm8));
		}

		void Assembler::ThumbCmpImm(int rd, u32 imm8)
		{
			if (rd < 0 || rd > 7 || imm8 > 0xFF)
				ThrowRange("Thumb CMP", "format 3 (A5.1.3) needs Rd 0-7 and an 8-bit immediate");
			Emit16((u16)(0x2800 | (rd << 8) | imm8), "CMP " + Reg(rd) + ", #" + Hex(imm8));
		}

		// Format 1 (A5.1.1): the move shifted register group. "LSL #0" is a plain move and
		// "LSL #32" has no encoding, while LSR/ASR read a zero amount as the 32 special case.
		void Assembler::ThumbLsl(int rd, int rm, u32 amount)
		{
			if (rd < 0 || rd > 7 || rm < 0 || rm > 7 || amount > 31)
				ThrowRange("Thumb LSL", "format 1 (A5.1.1) needs registers 0-7 and an amount 0-31");
			Emit16((u16)(0x0000 | (amount << 6) | (rm << 3) | rd),
				"LSL " + Reg(rd) + ", " + Reg(rm) + ", #" + std::to_string(amount));
		}

		void Assembler::ThumbLsr(int rd, int rm, u32 amount)
		{
			if (rd < 0 || rd > 7 || rm < 0 || rm > 7 || amount > 32)
				ThrowRange("Thumb LSR", "format 1 (A5.1.1) needs registers 0-7 and an amount 0-32");
			Emit16((u16)(0x0800 | ((amount & 0x1F) << 6) | (rm << 3) | rd),
				"LSR " + Reg(rd) + ", " + Reg(rm) + ", #" + std::to_string(amount));
		}

		void Assembler::ThumbAsr(int rd, int rm, u32 amount)
		{
			if (rd < 0 || rd > 7 || rm < 0 || rm > 7 || amount > 32)
				ThrowRange("Thumb ASR", "format 1 (A5.1.1) needs registers 0-7 and an amount 0-32");
			Emit16((u16)(0x1000 | ((amount & 0x1F) << 6) | (rm << 3) | rd),
				"ASR " + Reg(rd) + ", " + Reg(rm) + ", #" + std::to_string(amount));
		}

		// Format 4 (A5.1.4): the ALU operations, "010000 | opcode | Rm | Rd".
		void Assembler::ThumbAnd(int rd, int rm)
		{
			if (rd < 0 || rd > 7 || rm < 0 || rm > 7)
				ThrowRange("Thumb AND", "format 4 (A5.1.4) needs registers 0-7");
			Emit16((u16)(0x4000 | (rm << 3) | rd), "AND " + Reg(rd) + ", " + Reg(rm));
		}

		void Assembler::ThumbOrr(int rd, int rm)
		{
			if (rd < 0 || rd > 7 || rm < 0 || rm > 7)
				ThrowRange("Thumb ORR", "format 4 (A5.1.4) needs registers 0-7");
			Emit16((u16)(0x4300 | (rm << 3) | rd), "ORR " + Reg(rd) + ", " + Reg(rm));
		}

		void Assembler::ThumbEor(int rd, int rm)
		{
			if (rd < 0 || rd > 7 || rm < 0 || rm > 7)
				ThrowRange("Thumb EOR", "format 4 (A5.1.4) needs registers 0-7");
			Emit16((u16)(0x4040 | (rm << 3) | rd), "EOR " + Reg(rd) + ", " + Reg(rm));
		}

		void Assembler::ThumbMvn(int rd, int rm)
		{
			if (rd < 0 || rd > 7 || rm < 0 || rm > 7)
				ThrowRange("Thumb MVN", "format 4 (A5.1.4) needs registers 0-7");
			Emit16((u16)(0x43C0 | (rm << 3) | rd), "MVN " + Reg(rd) + ", " + Reg(rm));
		}

		// Format 8 (A5.1.8): "011 | B | L | offset5 | Rb | Rd". The offset is a *word* offset, so
		// the byte offset the caller passes has to be a multiple of 4 and at most 124.
		void Assembler::ThumbLdrImm(int rd, int rn, u32 offset)
		{
			if (rd < 0 || rd > 7 || rn < 0 || rn > 7 || offset > 124 || (offset & 3) != 0)
				ThrowRange("Thumb LDR", "format 8 (A5.1.8) needs registers 0-7 and a byte offset of 0-124 in steps of 4");
			Emit16((u16)(0x6800 | ((offset >> 2) << 6) | (rn << 3) | rd),
				"LDR " + Reg(rd) + ", [" + Reg(rn) + ", #" + Hex(offset) + "]");
		}

		void Assembler::ThumbStrImm(int rd, int rn, u32 offset)
		{
			if (rd < 0 || rd > 7 || rn < 0 || rn > 7 || offset > 124 || (offset & 3) != 0)
				ThrowRange("Thumb STR", "format 8 (A5.1.8) needs registers 0-7 and a byte offset of 0-124 in steps of 4");
			Emit16((u16)(0x6000 | ((offset >> 2) << 6) | (rn << 3) | rd),
				"STR " + Reg(rd) + ", [" + Reg(rn) + ", #" + Hex(offset) + "]");
		}

		// Format 9 (A5.1.9): "1000 | L | offset5 | Rb | Rd", a halfword offset of 0-62 in steps of 2.
		void Assembler::ThumbLdrhImm(int rd, int rn, u32 offset)
		{
			if (rd < 0 || rd > 7 || rn < 0 || rn > 7 || offset > 62 || (offset & 1) != 0)
				ThrowRange("Thumb LDRH", "format 9 (A5.1.9) needs registers 0-7 and a byte offset of 0-62 in steps of 2");
			Emit16((u16)(0x8800 | ((offset >> 1) << 6) | (rn << 3) | rd),
				"LDRH " + Reg(rd) + ", [" + Reg(rn) + ", #" + Hex(offset) + "]");
		}

		void Assembler::ThumbStrhImm(int rd, int rn, u32 offset)
		{
			if (rd < 0 || rd > 7 || rn < 0 || rn > 7 || offset > 62 || (offset & 1) != 0)
				ThrowRange("Thumb STRH", "format 9 (A5.1.9) needs registers 0-7 and a byte offset of 0-62 in steps of 2");
			Emit16((u16)(0x8000 | ((offset >> 1) << 6) | (rn << 3) | rd),
				"STRH " + Reg(rd) + ", [" + Reg(rn) + ", #" + Hex(offset) + "]");
		}

		// Format 7/8 (A5.1.7 and A5.1.8): the byte forms of the immediate load/store.
		void Assembler::ThumbLdrbImm(int rd, int rn, u32 offset)
		{
			if (rd < 0 || rd > 7 || rn < 0 || rn > 7 || offset > 31)
				ThrowRange("Thumb LDRB", "format 7 (A5.1.7) needs registers 0-7 and a byte offset of 0-31");
			Emit16((u16)(0x7800 | (offset << 6) | (rn << 3) | rd),
				"LDRB " + Reg(rd) + ", [" + Reg(rn) + ", #" + Hex(offset) + "]");
		}

		void Assembler::ThumbStrbImm(int rd, int rn, u32 offset)
		{
			if (rd < 0 || rd > 7 || rn < 0 || rn > 7 || offset > 31)
				ThrowRange("Thumb STRB", "format 7 (A5.1.7) needs registers 0-7 and a byte offset of 0-31");
			Emit16((u16)(0x7000 | (offset << 6) | (rn << 3) | rd),
				"STRB " + Reg(rd) + ", [" + Reg(rn) + ", #" + Hex(offset) + "]");
		}

		// Format 14 (A5.1.14): "1011 | L | 10 | R | register_list". Bit 8 is the extra register
		// (LR for PUSH, PC for POP); Push/Pop are Stmia/Ldmia of r13 in Thumb form.
		void Assembler::ThumbPush(u32 regList)
		{
			u32 list = regList & 0xFFFF;
			if ((list & ~0x40FFu) != 0)
				ThrowRange("Thumb PUSH", "format 14 (A5.1.14) takes r0-r7 plus lr");
			u16 halfword = (u16)(0xB400 | ((list & 0x4000) ? 0x100 : 0x000) | (list & 0xFF));
			Emit16(halfword, "PUSH " + RegListText((list & 0xFF) | ((list & 0x4000) ? (1u << 14) : 0)));
		}

		void Assembler::ThumbPop(u32 regList)
		{
			u32 list = regList & 0xFFFF;
			if ((list & ~0x80FFu) != 0)
				ThrowRange("Thumb POP", "format 14 (A5.1.14) takes r0-r7 plus pc");
			u16 halfword = (u16)(0xBC00 | ((list & 0x8000) ? 0x100 : 0x000) | (list & 0xFF));
			Emit16(halfword, "POP " + RegListText((list & 0xFF) | ((list & 0x8000) ? (1u << 15) : 0)));
		}

		void Assembler::ThumbB(const std::string& label, Cond c)
		{
			bool conditional = (c != Cond::AL);
			Fixup fixup;
			fixup.address = pc;
			fixup.label = label;
			fixup.thumb = true;
			fixup.link = false;
			fixup.conditional = conditional;
			fixup.condition = (u32)c;
			fixup.blx = false;
			fixups.push_back(fixup);
			if (conditional)
			{
				// Format 15 (A5.1.15): "1101 | cond | soffset8", cond must not be AL or NV
				// (1110 is the SWI format and 1111 is undefined).
				if ((u32)c == 0xE || (u32)c == 0xF)
					ThrowRange("Thumb B", "format 15 has no AL/NV condition");
				Emit16((u16)(0xD000 | ((u32)c << 8)), Mnemonic("B", c, false) + " " + label);
			}
			else
			{
				// Format 17 (A5.1.17): "11100 | offset11".
				Emit16((u16)0xE000, "B " + label);
			}
		}

		// Format 5 (A5.1.5), opcode 11: BX.
		void Assembler::ThumbBx(int rm)
		{
			if (rm < 0 || rm > 15)
				ThrowRange("Thumb BX", "format 5 (A5.1.5) needs a register 0-15");
			Emit16((u16)(0x4700 | ((rm >> 3) << 6) | ((rm & 7) << 3)),
				"BX " + Reg(rm));
		}

		// Format 16 (A5.1.16): "11011111 | imm8".
		void Assembler::ThumbSwi(u32 comment)
		{
			if (comment > 0xFF)
				ThrowRange("Thumb SWI", "format 16 (A5.1.16) takes an 8-bit comment");
			Emit16((u16)(0xDF00 | comment), "SWI #" + Hex(comment));
		}

		// ---------------------------------------------------------------------------------------
		// Output
		// ---------------------------------------------------------------------------------------

		std::vector<u8> Assembler::TakeImage(u32 size)
		{
			for (const Fixup& fixup : fixups)
			{
				auto found = labels.find(fixup.label);
				if (found == labels.end())
					throw std::runtime_error("gba_armasm: the label '" + fixup.label + "' is never bound");

				u32 target = found->second;
				u32 address = fixup.address;

				if (!fixup.thumb)
				{
					// A5.6: the ARM branch offset is (target - (address + 8)) >> 2, a signed 24-bit
					// word offset (the pipeline is two words ahead of the executing instruction).
					s32 delta = (s32)target - (s32)(address + 8);
					if ((delta & 3) != 0)
						throw std::runtime_error("gba_armasm: the branch to '" + fixup.label +
							"' is not word aligned");
					s32 words = delta >> 2;
					if (words < -(1 << 23) || words >= (1 << 23))
						throw std::runtime_error("gba_armasm: the branch to '" + fixup.label +
							"' does not reach");
					u32 word = ((fixup.condition & 0xF) << 28) |
						(fixup.link ? 0x0B000000 : 0x0A000000) | ((u32)words & 0xFFFFFF);
					if (address + 4 > origin + code.size())
						throw std::runtime_error("gba_armasm: a branch landed outside the image");
					u32 offset = address - origin;
					code[offset + 0] = (u8)(word & 0xFF);
					code[offset + 1] = (u8)((word >> 8) & 0xFF);
					code[offset + 2] = (u8)((word >> 16) & 0xFF);
					code[offset + 3] = (u8)((word >> 24) & 0xFF);
				}
				else
				{
					// The Thumb branch is relative to the address of the branch plus 4 (one
					// halfword of pipeline): format 15 has a signed 8-bit and format 17 a signed
					// 11-bit *halfword* offset (A5.1.15, A5.1.17).
					s32 delta = (s32)target - (s32)(address + 4);
					if ((delta & 1) != 0)
						throw std::runtime_error("gba_armasm: the Thumb branch to '" + fixup.label +
							"' is not halfword aligned");
					s32 halfwords = delta >> 1;
					if (address + 2 > origin + code.size())
						throw std::runtime_error("gba_armasm: a branch landed outside the image");
					u32 offset = address - origin;
					if (fixup.conditional)
					{
						if (halfwords < -128 || halfwords > 127)
							throw std::runtime_error("gba_armasm: the Thumb branch to '" + fixup.label +
								"' does not reach (format 15)");
						u16 halfword = (u16)(0xD000 | ((fixup.condition & 0xF) << 8) |
							((u32)halfwords & 0xFF));
						code[offset + 0] = (u8)(halfword & 0xFF);
						code[offset + 1] = (u8)(halfword >> 8);
					}
					else
					{
						if (halfwords < -1024 || halfwords > 1023)
							throw std::runtime_error("gba_armasm: the Thumb branch to '" + fixup.label +
								"' does not reach (format 17)");
						u16 halfword = (u16)(0xE000 | ((u32)halfwords & 0x7FF));
						code[offset + 0] = (u8)(halfword & 0xFF);
						code[offset + 1] = (u8)(halfword >> 8);
					}
				}
			}

			if (size != 0)
			{
				// `size` counts the bytes of the *image*, not addresses: an image at
				// 0x08000000 is the ROM's contents, not 128 MByte of leading hole. A caller that
				// assembled a longer program than it asked to pad to gets an error instead of a
				// silently truncated image.
				if (code.size() > size)
					throw std::runtime_error("gba_armasm: the image is " + Hex((u32)code.size()) +
						" bytes and does not fit in " + Hex(size) + " bytes");
				// 0xFF is what an erased ROM (and an unprogrammed flash) reads as.
				code.resize(size, 0xFF);
			}

			return code;
		}
	}
}
