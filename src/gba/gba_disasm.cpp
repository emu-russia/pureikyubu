// The instruction disassembler of the GBA module. See gba_disasm.h for what it is for and which
// documents it is written from.

#include "gba_disasm.h"

#include <cstdio>
#include <cstring>

namespace GBA
{
	ImageMemory::ImageMemory(const u8* data, size_t size, u32 base)
		: data(data), size(size), base(base)
	{
	}

	u16 ImageMemory::Read16(u32 address) const
	{
		if (address < base || address >= base + size)
			return 0xFFFF;					// outside the image: the open bus, as the bus reports it

		// Any address may be asked for: the Game Boy's instruction stream is made of single bytes
		// at every alignment, and the ARM and Thumb words are little endian.
		u8 low = data[address - base];
		u8 high = (address + 1 < base + size) ? data[address + 1 - base] : 0xFF;
		return (u16)(low | (high << 8));
	}

	namespace
	{
		const char* const ArmRegisters[16] =
		{
			"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
			"r8", "r9", "r10", "r11", "r12", "sp", "lr", "pc",
		};

		const char* const ThumbRegisters[8] = { "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7" };

		/// <summary>The condition suffixes of the ARM condition field (A3.2).</summary>
		const char* const ConditionNames[16] =
		{
			"eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
			"hi", "ls", "ge", "lt", "gt", "le", "", "nv",
		};

		const char* const ShiftNames[4] = { "lsl", "lsr", "asr", "ror" };

		std::string Hex(u32 value)
		{
			char text[16];
			snprintf(text, sizeof text, "0x%X", value);
			return text;
		}

		std::string HexPadded(u32 value, int digits)
		{
			char text[16];
			snprintf(text, sizeof text, "0x%0*X", digits, value);
			return text;
		}

		std::string Dec(u32 value)
		{
			char text[16];
			snprintf(text, sizeof text, "%u", value);
			return text;
		}

		std::string Signed(s32 value)
		{
			char text[16];
			snprintf(text, sizeof text, "%d", value);
			return text;
		}

		u32 RotateRight(u32 value, u32 amount)
		{
			amount &= 31;
			if (amount == 0)
				return value;
			return (value >> amount) | (value << (32 - amount));
		}

		std::string Condition(u32 instruction)
		{
			// The "always" condition (1110) has no suffix; 1111 is the unconditional escape and is
			// never printed as a suffix.
			u32 condition = instruction >> 28;
			if (condition >= 14)
				return "";
			return ConditionNames[condition];
		}

		/// <summary>The operand of a data processing instruction that is a register, with the
		/// barrel shifter applied (A5.1.5): "r3", "r3, lsl #2", "r3, rrx", "r3, lsr r4".</summary>
		std::string ShiftedOperand(u32 instruction)
		{
			u32 reg = instruction & 0xF;
			u32 type = (instruction >> 5) & 3;
			std::string base = ArmRegisters[reg];

			if ((instruction & 0x10) != 0)
			{
				// The shift amount is the bottom byte of a register (A5.1.5).
				return base + ", " + ShiftNames[type] + " " + ArmRegisters[(instruction >> 8) & 0xF];
			}

			u32 amount = (instruction >> 7) & 0x1F;
			if (amount == 0)
			{
				if (type == 0)
					return base;						// lsl #0 is the register itself
				if (type == 1)
					return base + ", lsr #32";
				if (type == 2)
					return base + ", asr #32";
				return base + ", rrx";
			}

			return base + ", " + ShiftNames[type] + " #" + Dec(amount);
		}

		/// <summary>The 32-bit value of a data processing immediate, i.e. the eight bits rotated
		/// right by an even amount (A5.1.4). The rotation is printed when it is not zero.</summary>
		std::string ImmediateValue(u32 instruction)
		{
			u32 rotation = ((instruction >> 8) & 0xF) * 2;
			u32 value = RotateRight(instruction & 0xFF, rotation);
			if (rotation == 0)
				return "#" + Hex(value);
			return "#" + Hex(value) + "\t; " + Dec(instruction & 0xFF) + " ror " + Dec(rotation);
		}

		/// <summary>A register list as "{r0-r3, r5, lr}". An empty list is "{}"; a pair of adjacent
		/// registers stays a pair ("{r0, r1}") because that is how an assembler writes it.</summary>
		std::string RegisterList(u16 list, const char* const* names, int count)
		{
			std::string text = "{";
			int written = 0;

			for (int reg = 0; reg < count; )
			{
				if ((list & (1u << reg)) == 0)
				{
					reg++;
					continue;
				}

				int end = reg;
				while (end + 1 < count && (list & (1u << (end + 1))) != 0)
					end++;

				if (written++ != 0)
					text += ", ";
				text += names[reg];
				if (end >= reg + 2)
					text += "-" + std::string(names[end]);
				else if (end == reg + 1)
					text += ", " + std::string(names[end]);
				reg = end + 1;
			}

			return text + "}";
		}

		/// <summary>The addressing mode of an LDM/STM (A5.2): P selects pre- or post-indexing and U
		/// the direction, so P=1/U=1 is increment-before, P=1/U=0 decrement-before, and so on.</summary>
		const char* BlockMode(u32 instruction)
		{
			bool pre = (instruction & 0x01000000) != 0;
			bool up = (instruction & 0x00800000) != 0;
			if (pre && up) return "ib";
			if (!pre && up) return "ia";
			if (pre && !up) return "db";
			return "da";
		}

		/// <summary>The addressing mode of a single data transfer: "[r1, #4]", "[r1, r2, lsl #2]!",
		/// "[r1], #4", "[r1], r2".</summary>
		std::string TransferAddress(u32 instruction)
		{
			u32 base = (instruction >> 16) & 0xF;
			bool pre = (instruction & 0x01000000) != 0;
			bool up = (instruction & 0x00800000) != 0;
			bool writeback = (instruction & 0x00200000) != 0;
			bool immediate = (instruction & 0x02000000) == 0;

			std::string offset;
			if (immediate)
			{
				u32 value = instruction & 0xFFF;
				if (value == 0)
					offset = "";
				else
					offset = (up ? ", #" : ", -#") + Dec(value);
			}
			else
			{
				std::string operand = ShiftedOperand(instruction);
				offset = (up ? ", " : ", -") + operand;
			}

			std::string text = "[" + std::string(ArmRegisters[base]);
			if (pre)
			{
				text += offset;
				if (writeback)
					text += "!";
				text += "]";
			}
			else
			{
				text += "]";
				if (offset.empty())
					text += ", #0";
				else
					text += offset;
			}

			return text;
		}

		/// <summary>The GBA BIOS calls the SWI numbers name (GBATEK "BIOS Functions"). Only the
		/// documented entries with a stable number are named; anything else stays a bare number.</summary>
		const char* SwiName(u32 number)
		{
			switch (number)
			{
			case 0x000000: return "SoftReset";
			case 0x010000: return "RegisterRamReset";
			case 0x020000: return "Halt";
			case 0x030000: return "Stop";
			case 0x040000: return "IntrWait";
			case 0x050000: return "VBlankIntrWait";
			case 0x060000: return "Div";
			case 0x070000: return "DivArm";
			case 0x080000: return "Sqrt";
			case 0x090000: return "ArcTan";
			case 0x0A0000: return "ArcTan2";
			case 0x0B0000: return "CpuSet";
			case 0x0C0000: return "CpuFastSet";
			case 0x0E0000: return "BgAffineSet";
			case 0x0F0000: return "ObjAffineSet";
			case 0x100000: return "BitUnPack";
			case 0x110000: return "LZ77UnCompWram";
			case 0x120000: return "LZ77UnCompVram";
			case 0x130000: return "HuffUnComp";
			case 0x140000: return "RlUnCompWram";
			case 0x150000: return "RlUnCompVram";
			default: return nullptr;
			}
		}

		// -------------------------------------------------------------------------------------
		// ARM
		// -------------------------------------------------------------------------------------

		std::string DecodeDataProcessing(u32 instruction, bool immediate)
		{
			static const char* const Operations[16] =
			{
				"and", "eor", "sub", "rsb", "add", "adc", "sbc", "rsc",
				"tst", "teq", "cmp", "cmn", "orr", "mov", "bic", "mvn",
			};

			u32 operation = (instruction >> 21) & 0xF;
			bool setFlags = (instruction & 0x00100000) != 0;
			std::string mnemonic = Operations[operation];

			// TST/TEQ/CMP/CMN always write the flags and take no destination register (A3.4.1).
			bool compares = operation >= 8 && operation <= 11;
			if (setFlags && !compares)
				mnemonic += "s";
			mnemonic += Condition(instruction);

			std::string text = mnemonic;

			if (compares)
			{
				// TST/TEQ/CMP/CMN take the first operand from bits 19-16 and write no register.
				text += " " + std::string(ArmRegisters[(instruction >> 16) & 0xF]) + ", ";
			}
			else
			{
				text += " " + std::string(ArmRegisters[(instruction >> 12) & 0xF]);
				if (operation == 13 || operation == 15)
					text += ", ";						// MOV and MVN take only the operand
				else
					text += ", " + std::string(ArmRegisters[(instruction >> 16) & 0xF]) + ", ";
			}

			text += immediate ? ImmediateValue(instruction) : ShiftedOperand(instruction);
			return text;
		}

		std::string DecodeMultiply(u32 instruction)
		{
			// MUL/MLA: A3.4.4. The long forms are recognised by their own pattern before this.
			bool accumulate = (instruction & 0x00200000) != 0;
			bool setFlags = (instruction & 0x00100000) != 0;
			std::string mnemonic = (accumulate ? "mla" : "mul") + std::string(setFlags ? "s" : "") +
				Condition(instruction);

			std::string text = mnemonic + " " + ArmRegisters[(instruction >> 16) & 0xF] + ", " +
				ArmRegisters[instruction & 0xF] + ", " + ArmRegisters[(instruction >> 8) & 0xF];
			if (accumulate)
				text += ", " + std::string(ArmRegisters[(instruction >> 12) & 0xF]);
			return text;
		}

		std::string DecodeLongMultiply(u32 instruction)
		{
			bool sign = (instruction & 0x00400000) != 0;
			bool accumulate = (instruction & 0x00200000) != 0;
			bool setFlags = (instruction & 0x00100000) != 0;

			std::string mnemonic = sign ? "s" : "u";
			mnemonic += accumulate ? "mlal" : "mull";
			if (setFlags)
				mnemonic += "s";
			mnemonic += Condition(instruction);

			return mnemonic + " " + ArmRegisters[(instruction >> 12) & 0xF] + ", " +
				ArmRegisters[(instruction >> 16) & 0xF] + ", " + ArmRegisters[instruction & 0xF] + ", " +
				ArmRegisters[(instruction >> 8) & 0xF];
		}

		std::string DecodeHalfwordTransfer(u32 instruction)
		{
			u32 type = (instruction >> 5) & 3;			// 1 = H, 2 = SB, 3 = SH
			bool load = (instruction & 0x00100000) != 0;
			bool pre = (instruction & 0x01000000) != 0;
			bool up = (instruction & 0x00800000) != 0;
			bool writeback = (instruction & 0x00200000) != 0;

			// Bit 22 selects a register offset: "offsetH ... 1 S H 1 offsetL" is either the high
			// nibble of an immediate or the register (A3.4.3).
			bool registerOffset = (instruction & 0x00400000) != 0;

			if (!load && type != 1)
				return "undef";							// only STRH has a store form

			std::string mnemonic = load ? "ldr" : "str";
			if (type == 2) mnemonic += "sb";
			if (type == 3) mnemonic += "sh";
			if (type == 1 && load) mnemonic += "h";
			if (type == 1 && !load) mnemonic += "h";
			mnemonic += Condition(instruction);

			u32 base = (instruction >> 16) & 0xF;
			u32 target = (instruction >> 12) & 0xF;

			std::string offset;
			if (registerOffset)
			{
				offset = (up ? ", " : ", -") + std::string(ArmRegisters[instruction & 0xF]);
			}
			else
			{
				u32 value = ((instruction >> 4) & 0xF0) | (instruction & 0xF);
				if (value == 0)
					offset = "";
				else
					offset = (up ? ", #" : ", -#") + Dec(value);
			}

			std::string address = "[" + std::string(ArmRegisters[base]);
			if (pre)
			{
				address += offset;
				if (writeback)
					address += "!";
				address += "]";
			}
			else
			{
				address += "]";
				address += offset.empty() ? ", #0" : offset;
			}

			return mnemonic + " " + ArmRegisters[target] + ", " + address;
		}

		std::string DecodeArm(u32 instruction, u32 address)
		{
			u32 condition = instruction >> 28;
			u32 space = (instruction >> 25) & 7;

			// The unconditional encodings (cond = 1111) are BLX (ARMv5) and the coprocessor/SWI
			// space; on an ARM7TDMI only the SWI form exists and the rest is undefined.
			if (condition == 0xF)
			{
				if (space == 7)
					return "swi " + HexPadded(instruction & 0xFFFFFF, 6);
				return "undef";
			}

			switch (space)
			{
			case 0:
			{
				u32 bits7to4 = (instruction >> 4) & 0xF;

				if (bits7to4 == 9)
				{
					bool swap = (instruction & 0x01000000) != 0;
					if (swap)
					{
						// SWP/SWPB (A3.4.6).
						std::string mnemonic = (instruction & 0x00400000) ? "swpb" : "swp";
						return mnemonic + Condition(instruction) + " " +
							ArmRegisters[(instruction >> 12) & 0xF] + ", " +
							ArmRegisters[instruction & 0xF] + ", [" +
							ArmRegisters[(instruction >> 16) & 0xF] + "]";
					}

					// MUL (bit 22 clear, bit 23 clear) or a long multiply (bit 23 set).
					if ((instruction & 0x00800000) == 0)
						return DecodeMultiply(instruction);
					return DecodeLongMultiply(instruction);
				}

				if (bits7to4 == 0xB || bits7to4 == 0xD || bits7to4 == 0xF)
					return DecodeHalfwordTransfer(instruction);

				// The miscellaneous space: MRS, MSR (register), BX, BLX (register), CLZ.
				if ((instruction & 0x0FBF0FFF) == 0x010F0000)
					return "mrs" + Condition(instruction) + " " +
						ArmRegisters[(instruction >> 12) & 0xF] + ", cpsr";
				if ((instruction & 0x0FBF0FFF) == 0x014F0000)
					return "mrs" + Condition(instruction) + " " +
						ArmRegisters[(instruction >> 12) & 0xF] + ", spsr";
				if ((instruction & 0x0FB0FFF0) == 0x0120F000)
				{
					// MSR: the field mask is in bits 19-16.
					static const char* const Fields[4] = { "c", "x", "s", "f" };
					std::string fields;
					for (int i = 3; i >= 0; i--)
						if ((instruction & (1u << (16 + i))) != 0)
							fields += Fields[i];

					return "msr" + Condition(instruction) + " cpsr_" + fields + ", " +
						ArmRegisters[instruction & 0xF];
				}
				if ((instruction & 0x0FFFFFF0) == 0x012FFF10)
					return "bx" + Condition(instruction) + " " + ArmRegisters[instruction & 0xF];
				if ((instruction & 0x0FFFFFF0) == 0x012FFF30)
					return "blx" + Condition(instruction) + " " + ArmRegisters[instruction & 0xF];
				if ((instruction & 0x0FFF0FF0) == 0x016F0F10)
					return "clz" + Condition(instruction) + " " +
						ArmRegisters[(instruction >> 12) & 0xF] + ", " + ArmRegisters[instruction & 0xF];

				// Everything else in this space is a data processing instruction. Bit 4 selects
				// whether the shifter's amount is an immediate or the bottom byte of a register,
				// which ShiftedOperand handles; it is not a format discriminator.
				return DecodeDataProcessing(instruction, false);
			}

			case 1:
			{
				// MSR with an immediate (A3.4.5) uses the same field mask as the register form.
				if ((instruction & 0x0FB00000) == 0x03200000)
				{
					static const char* const Fields[4] = { "c", "x", "s", "f" };
					std::string fields;
					for (int i = 3; i >= 0; i--)
						if ((instruction & (1u << (16 + i))) != 0)
							fields += Fields[i];

					return "msr" + Condition(instruction) + " cpsr_" + fields + ", " +
						ImmediateValue(instruction);
				}

				return DecodeDataProcessing(instruction, true);
			}

			case 2:
			case 3:
			{
				bool load = (instruction & 0x00100000) != 0;
				bool byte = (instruction & 0x00400000) != 0;
				bool postIndex = (instruction & 0x01000000) == 0;
				bool writeback = (instruction & 0x00200000) != 0;

				std::string mnemonic = load ? "ldr" : "str";
				if (byte)
					mnemonic += "b";
				if (postIndex && writeback)
					mnemonic += "t";					// the user-mode variant (A3.4.2)
				mnemonic += Condition(instruction);

				return mnemonic + " " + ArmRegisters[(instruction >> 12) & 0xF] + ", " +
					TransferAddress(instruction);
			}

			case 4:
			{
				bool load = (instruction & 0x00100000) != 0;
				bool writeback = (instruction & 0x00200000) != 0;
				bool userMode = (instruction & 0x00400000) != 0;

				// The manual's syntax is LDM{<cond>}<addressing mode>: the condition comes before
				// the mode, as in "ldmltia".
				std::string mnemonic = (load ? "ldm" : "stm") + std::string(Condition(instruction)) +
					BlockMode(instruction);
				std::string text = mnemonic + " " + ArmRegisters[(instruction >> 16) & 0xF];
				if (writeback)
					text += "!";
				text += ", " + RegisterList((u16)instruction, ArmRegisters, 16);
				if (userMode)
					text += "^";
				return text;
			}

			case 5:
			{
				bool link = (instruction & 0x01000000) != 0;
				s32 offset = (s32)(instruction & 0xFFFFFF);
				if ((offset & 0x800000) != 0)
					offset |= (s32)0xFF000000;
				u32 target = address + 8 + (u32)(offset << 2);

				return std::string(link ? "bl" : "b") + Condition(instruction) + " " + Hex(target);
			}

			case 7:
			{
				// SWI: the GBA BIOS numbers its service functions in the top byte, so the call is
				// named when the number is one of the documented ones.
				u32 number = instruction & 0xFFFFFF;
				std::string text = "swi" + Condition(instruction) + " " + HexPadded(number, 6);
				const char* name = SwiName(number);
				if (name != nullptr)
					text += "\t; " + std::string(name);
				return text;
			}

			default:
				// The coprocessor space (110 and 111 with a condition): the GBA has no
				// coprocessor, and so does not execute these.
				return "undef";
			}
		}

		// -------------------------------------------------------------------------------------
		// Thumb
		// -------------------------------------------------------------------------------------

		/// <summary>A sign-extended branch offset, as the manual writes it.</summary>
		s32 SignExtend(u32 value, int bits)
		{
			if ((value & (1u << (bits - 1))) != 0)
				return (s32)(value | ~((1u << bits) - 1));
			return (s32)value;
		}

		/// <summary>Thumb: the branch target of a conditional or unconditional branch, which is the
		/// instruction address plus four plus the doubled offset (A4.3).</summary>
		std::string DecodeThumb(u32 instruction, u32 address, const DisasmMemory& memory)
		{
			// The order of the tests is the order of the format table in A4.2, and each format is
			// recognised by the bit pattern the manual gives for it.
			// Format 1: shift by immediate (LSL/LSR/ASR Rd, Rm, #imm5). Bits 12-11 are the
			// operation, and 11 is also the marker bit of format 2 ("00011"), so the two formats
			// share the 000 prefix and format 2's pattern has to be excluded here.
			if ((instruction & 0xE000) == 0x0000 && (instruction & 0x1800) != 0x1800)
			{
				static const char* const Names[3] = { "lsl", "lsr", "asr" };
				u32 operation = (instruction >> 11) & 3;
				if (operation == 3)
					return "undef";					// bits 12-11 = 11 without I is not an encoding
				u32 amount = (instruction >> 6) & 0x1F;
				u32 source = (instruction >> 3) & 7;
				u32 target = instruction & 7;
				if (amount == 0 && operation != 0)
					amount = 32;					// LSR/ASR #0 mean #32, LSL #0 is the register
				return std::string(Names[operation]) + " " + ThumbRegisters[target] + ", " +
					ThumbRegisters[source] + ", #" + Dec(amount);
			}

			// Format 2: add/subtract (register or three-bit immediate).
			if ((instruction & 0xF800) == 0x1800)
			{
				bool immediate = (instruction & 0x0400) != 0;
				bool subtract = (instruction & 0x0200) != 0;
				u32 operand = (instruction >> 6) & 7;
				u32 source = (instruction >> 3) & 7;
				u32 target = instruction & 7;

				std::string text = subtract ? "sub" : "add";
				text += " " + std::string(ThumbRegisters[target]);
				text += ", " + std::string(ThumbRegisters[source]);
				text += ", ";
				text += immediate ? ("#" + Hex(operand)) : std::string(ThumbRegisters[operand]);
				return text;
			}

			// Format 3: MOV/CMP/ADD/SUB with an eight-bit immediate.
			if ((instruction & 0xE000) == 0x2000)
			{
				static const char* const Names[4] = { "mov", "cmp", "add", "sub" };
				u32 operation = (instruction >> 11) & 3;
				return std::string(Names[operation]) + " " + ThumbRegisters[(instruction >> 8) & 7] +
					", #" + Hex(instruction & 0xFF);
			}

			// Format 4: ALU operations on two low registers.
			if ((instruction & 0xFC00) == 0x4000)
			{
				static const char* const Names[16] =
				{
					"and", "eor", "lsl", "lsr", "asr", "adc", "sbc", "ror",
					"tst", "neg", "cmp", "cmn", "orr", "mul", "bic", "mvn",
				};

				u32 operation = (instruction >> 6) & 0xF;
				return std::string(Names[operation]) + " " + ThumbRegisters[instruction & 7] + ", " +
					ThumbRegisters[(instruction >> 3) & 7];
			}

			// Format 5: the high register operations and BX.
			if ((instruction & 0xFC00) == 0x4400)
			{
				static const char* const Names[3] = { "add", "cmp", "mov" };
				u32 operation = (instruction >> 8) & 3;
				u32 target = (instruction & 7) | ((instruction >> 4) & 8);
				u32 source = (instruction >> 3) & 0xF;
				if (operation == 3)
					return "bx " + std::string(ArmRegisters[source]);
				return std::string(Names[operation]) + " " + ArmRegisters[target] + ", " +
					ArmRegisters[source];
			}

			// Format 6: PC-relative load.
			if ((instruction & 0xF800) == 0x4800)
			{
				u32 offset = (instruction & 0xFF) * 4;
				u32 target = (instruction >> 8) & 7;
				return "ldr " + std::string(ThumbRegisters[target]) + ", [pc, #" + Dec(offset) +
					"]\t; = " + Hex(((address + 4) & ~3u) + offset);
			}

			// Format 7: register offset transfers, including the sign-extended byte/halfword loads.
			if ((instruction & 0xF000) == 0x5000)
			{
				static const char* const Names[8] =
				{
					"str", "strh", "strb", "ldrsb", "ldr", "ldrh", "ldrb", "ldrsh",
				};

				u32 operation = (instruction >> 9) & 7;
				u32 offset = (instruction >> 6) & 7;
				u32 base = (instruction >> 3) & 7;
				u32 target = instruction & 7;
				return std::string(Names[operation]) + " " + ThumbRegisters[target] + ", [" +
					ThumbRegisters[base] + ", " + ThumbRegisters[offset] + "]";
			}

			// Format 8: immediate offset transfers (word and byte).
			if ((instruction & 0xE000) == 0x6000)
			{
				bool byte = (instruction & 0x1000) != 0;
				bool load = (instruction & 0x0800) != 0;
				u32 offset = (instruction >> 6) & 0x1F;
				u32 base = (instruction >> 3) & 7;
				u32 target = instruction & 7;
				std::string mnemonic = load ? "ldr" : "str";
				if (byte)
					mnemonic += "b";
				return mnemonic + " " + ThumbRegisters[target] + ", [" + ThumbRegisters[base] + ", #" +
					Dec(offset * (byte ? 1 : 4)) + "]";
			}

			// Format 9: halfword immediate offset transfers.
			if ((instruction & 0xF000) == 0x8000)
			{
				bool load = (instruction & 0x0800) != 0;
				u32 offset = (instruction >> 6) & 0x1F;
				u32 base = (instruction >> 3) & 7;
				u32 target = instruction & 7;
				return std::string(load ? "ldrh" : "strh") + " " + ThumbRegisters[target] + ", [" +
					ThumbRegisters[base] + ", #" + Dec(offset * 2) + "]";
			}

			// Format 10: SP-relative word transfers.
			if ((instruction & 0xF000) == 0x9000)
			{
				bool load = (instruction & 0x0800) != 0;
				u32 offset = (instruction & 0xFF) * 4;
				return std::string(load ? "ldr" : "str") + " " + ThumbRegisters[(instruction >> 8) & 7] +
					", [sp, #" + Dec(offset) + "]";
			}

			// Format 11: load the address of a PC- or SP-relative word.
			if ((instruction & 0xF000) == 0xA000)
			{
				bool stack = (instruction & 0x0800) != 0;
				u32 offset = (instruction & 0xFF) * 4;
				u32 base = stack ? 0 : ((address + 4) & ~3u);
				return "add " + std::string(ThumbRegisters[(instruction >> 8) & 7]) + ", " +
					(stack ? "sp" : "pc") + ", #" + Dec(offset) + "\t; = " + Hex(base + offset);
			}

			// Format 12: add/subtract to SP.
			if ((instruction & 0xFF00) == 0xB000)
			{
				bool subtract = (instruction & 0x0080) != 0;
				u32 offset = (instruction & 0x7F) * 4;
				return std::string(subtract ? "sub" : "add") + " sp, #" + Dec(offset);
			}

			// Format 13: push and pop. The R bit is the "extra" register: LR for PUSH, PC for POP,
			// and the manual's syntax writes it inside the list.
			if ((instruction & 0xF600) == 0xB400)
			{
				bool pop = (instruction & 0x0800) != 0;
				u16 list = (u16)(instruction & 0xFF);
				if ((instruction & 0x0100) != 0)
					list |= pop ? 0x8000 : 0x4000;		// bit 15 is pc, bit 14 is lr
				return std::string(pop ? "pop" : "push") + " " +
					RegisterList(list, ArmRegisters, pop ? 16 : 15);
			}

			// Format 14: multiple load and store.
			if ((instruction & 0xF000) == 0xC000)
			{
				bool load = (instruction & 0x0800) != 0;
				return std::string(load ? "ldmia" : "stmia") + " " +
					ThumbRegisters[(instruction >> 8) & 7] + "!, " +
					RegisterList((u16)(instruction & 0xFF), ThumbRegisters, 8);
			}

			// Format 15 and 16: the conditional branch and the software interrupt.
			if ((instruction & 0xF000) == 0xD000)
			{
				u32 condition = (instruction >> 8) & 0xF;
				if (condition == 0xF)
					return "swi " + Dec(instruction & 0xFF);
				return "b" + std::string(ConditionNames[condition]) + " " +
					Hex(address + 4 + (u32)(SignExtend(instruction & 0xFF, 8) * 2));
			}

			// Format 17: the unconditional branch.
			if ((instruction & 0xF800) == 0xE000)
			{
				return "b " + Hex(address + 4 + (u32)(SignExtend(instruction & 0x7FF, 11) * 2));
			}

			// Format 18: the long branch with link, in two halfwords.
			if ((instruction & 0xF800) == 0xF000)
			{
				s32 high = SignExtend(instruction & 0x7FF, 11);
				u32 low = memory.Read16(address + 2);
				if ((low & 0xF800) != 0xF800)
					return "bl\t; the second halfword is missing";
				u32 target = address + 4 + (u32)(high * 4096) + ((low & 0x7FF) * 2);
				return "bl " + Hex(target);
			}

			return "undef";
		}
	}

	std::string ArmDisassemble(const DisasmMemory& memory, u32 address, int* size)
	{
		if (size != nullptr)
			*size = 4;

		u32 instruction = memory.Read16(address) | ((u32)memory.Read16(address + 2) << 16);
		return DecodeArm(instruction, address);
	}

	std::string ThumbDisassemble(const DisasmMemory& memory, u32 address, int* size)
	{
		u32 instruction = memory.Read16(address);
		bool longBranch = (instruction & 0xF800) == 0xF000;
		if (size != nullptr)
			*size = longBranch ? 4 : 2;
		return DecodeThumb(instruction, address, memory);
	}

	std::string Disassemble(const DisasmMemory& memory, u32 address, bool thumb, int* size)
	{
		return thumb ? ThumbDisassemble(memory, address, size) : ArmDisassemble(memory, address, size);
	}

	std::string InstructionBytes(const DisasmMemory& memory, u32 address, int size)
	{
		char text[32];
		if (size == 4)
			snprintf(text, sizeof text, "%04X%04X", memory.Read16(address + 2), memory.Read16(address));
		else
			snprintf(text, sizeof text, "%04X", memory.Read16(address));
		return text;
	}

	std::string ConditionFlags(u32 cpsr)
	{
		std::string text;
		text += (cpsr & 0x80000000u) ? 'N' : 'n';
		text += (cpsr & 0x40000000u) ? 'Z' : 'z';
		text += (cpsr & 0x20000000u) ? 'C' : 'c';
		text += (cpsr & 0x10000000u) ? 'V' : 'v';
		text += (cpsr & 0x00000080u) ? 'I' : 'i';
		text += (cpsr & 0x00000040u) ? 'F' : 'f';
		return text;
	}
}

