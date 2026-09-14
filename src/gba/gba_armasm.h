// A very small ARM/Thumb code emitter, used to build the emulator's own boot ROM (and the test
// ROMs the harness assembles).
//
// Why not an external assembler: the boot ROM is part of the emulator's source, so it has to be
// buildable by the same compiler on every platform the emulator builds on, with no toolchain
// besides the C++ compiler. The emitter covers the instruction subset a boot ROM needs: data
// processing with immediates and registers, loads and stores with the addressing modes that reach
// the I/O registers, block transfers, multiplies, the branch family with symbolic labels, and SWI.
//
// Labels are resolved when TakeImage() is called, so forward branches work; a branch to a label
// that was never bound throws std::runtime_error. Every emitted instruction is also recorded in a
// listing (mnemonic + address), which the boot ROM tests and the documentation print.

#pragma once

#include "gba_types.h"

#include <map>

namespace GBA
{
	namespace ArmAsm
	{
		/// <summary>The condition field of a conditional instruction.</summary>
		enum class Cond : u32
		{
			EQ = 0x0, NE = 0x1, CS = 0x2, CC = 0x3, MI = 0x4, PL = 0x5, VS = 0x6, VC = 0x7,
			HI = 0x8, LS = 0x9, GE = 0xA, LT = 0xB, GT = 0xC, LE = 0xD, AL = 0xE,
		};

		/// <summary>The shift types of a register operand.</summary>
		enum class Shift : u32
		{
			LSL = 0, LSR = 1, ASR = 2, ROR = 3,
		};

		class Assembler
		{
		public:
			Assembler();

			// -- location -------------------------------------------------------------------

			/// <summary>Move the program counter (the image is zero-filled up to it).</summary>
			void Org(u32 address);

			u32 PC() const { return pc; }

			/// <summary>Bind a label to the current address.</summary>
			void Label(const std::string& name);

			/// <summary>Reserve `count` zero bytes (a data area a label points at).</summary>
			void Reserve(u32 count);

			// -- data -----------------------------------------------------------------------

			void Data8(u8 value);
			void Data16(u16 value);
			void Data32(u32 value);
			void DataBytes(const void* data, size_t size);
			void Align(u32 alignment);

			// -- data processing ------------------------------------------------------------

			/// <summary>MOV{S} rd, #imm (the rotation is solved for `imm`; throws when it cannot be).</summary>
			void Mov(int rd, u32 imm, Cond c = Cond::AL, bool setFlags = false);

			/// <summary>MOV{S} rd, rm (optionally shifted).</summary>
			void MovReg(int rd, int rm, Cond c = Cond::AL, bool setFlags = false,
				Shift shift = Shift::LSL, u32 amount = 0);

			void Add(int rd, int rn, u32 imm, Cond c = Cond::AL, bool setFlags = false);
			void AddReg(int rd, int rn, int rm, Cond c = Cond::AL, bool setFlags = false);
			void Sub(int rd, int rn, u32 imm, Cond c = Cond::AL, bool setFlags = false);
			void SubReg(int rd, int rn, int rm, Cond c = Cond::AL, bool setFlags = false);
			void Rsb(int rd, int rn, u32 imm, Cond c = Cond::AL, bool setFlags = false);
			void And(int rd, int rn, u32 imm, Cond c = Cond::AL, bool setFlags = false);
			void AndReg(int rd, int rn, int rm, Cond c = Cond::AL, bool setFlags = false);
			void Orr(int rd, int rn, u32 imm, Cond c = Cond::AL, bool setFlags = false);
			void OrrReg(int rd, int rn, int rm, Cond c = Cond::AL, bool setFlags = false);
			void Eor(int rd, int rn, u32 imm, Cond c = Cond::AL, bool setFlags = false);
			void EorReg(int rd, int rn, int rm, Cond c = Cond::AL, bool setFlags = false);
			void Bic(int rd, int rn, u32 imm, Cond c = Cond::AL, bool setFlags = false);
			void Mvn(int rd, u32 imm, Cond c = Cond::AL, bool setFlags = false);
			void Cmp(int rn, u32 imm, Cond c = Cond::AL);
			void CmpReg(int rn, int rm, Cond c = Cond::AL, Shift shift = Shift::LSL, u32 amount = 0);
			void Tst(int rn, u32 imm, Cond c = Cond::AL);
			void TstReg(int rn, int rm, Cond c = Cond::AL);
			void Mul(int rd, int rm, int rs, Cond c = Cond::AL, bool setFlags = false);
			void Umull(int rdLo, int rdHi, int rm, int rs, Cond c = Cond::AL, bool setFlags = false);

			/// <summary>A free-form shifted-register operand for the calls above (LSL/LSR/ASR/ROR).</summary>
			void ShiftReg(int rd, int rm, Shift shift, u32 amount, Cond c = Cond::AL, bool setFlags = false);

			// -- loads and stores -----------------------------------------------------------

			/// <summary>LDR rd, [rn, #+/-offset]</summary>
			void Ldr(int rd, int rn, int offset, Cond c = Cond::AL, bool byte = false);
			void Str(int rd, int rn, int offset, Cond c = Cond::AL, bool byte = false);
			void LdrReg(int rd, int rn, int rm, Cond c = Cond::AL, bool byte = false);
			void StrReg(int rd, int rn, int rm, Cond c = Cond::AL, bool byte = false);
			void Ldrh(int rd, int rn, int offset, Cond c = Cond::AL);
			void Strh(int rd, int rn, int offset, Cond c = Cond::AL);
			void Ldrsb(int rd, int rn, int offset, Cond c = Cond::AL);
			void Ldrsh(int rd, int rn, int offset, Cond c = Cond::AL);

			/// <summary>LDMIA/STMIA with a register list (bit i = register i).</summary>
			void Ldmia(int rn, u32 regList, Cond c = Cond::AL, bool writeBack = true);
			void Stmia(int rn, u32 regList, Cond c = Cond::AL, bool writeBack = true);
			void Push(u32 regList) { Stmia(13, regList, Cond::AL, true); }
			void Pop(u32 regList) { Ldmia(13, regList, Cond::AL, true); }

			// -- branches -------------------------------------------------------------------

			void B(const std::string& label, Cond c = Cond::AL);
			void Bl(const std::string& label, Cond c = Cond::AL);
			void Bx(int rm, Cond c = Cond::AL);
			void Blx(int rm, Cond c = Cond::AL);

			/// <summary>SWI with the given comment field (the BIOS function number).</summary>
			void Swi(u32 comment, Cond c = Cond::AL);

			/// <summary>BKPT (a debugger breakpoint; also the "semihost" hook the harness uses).</summary>
			void Bkpt(u32 immediate);

			/// <summary>A literal pool: emit the words and return the address of the first one.</summary>
			u32 LiteralPool(const std::vector<u32>& words);

			// -- Thumb ----------------------------------------------------------------------

			/// <summary>Switch the emitter to Thumb. Thumb code must be reached with a BX whose
			/// low bit is set, so the caller decides where the switch happens.</summary>
			void UseThumb(bool thumb) { thumbMode = thumb; }
			bool Thumb() const { return thumbMode; }

			void ThumbMov(int rd, u32 imm8);
			void ThumbMovReg(int rd, int rm);
			void ThumbAdd(int rd, int rn, int rm);
			void ThumbAddImm(int rd, u32 imm8);
			void ThumbSubImm(int rd, u32 imm8);
			void ThumbCmpImm(int rd, u32 imm8);
			void ThumbLsl(int rd, int rm, u32 amount);
			void ThumbLsr(int rd, int rm, u32 amount);
			void ThumbAsr(int rd, int rm, u32 amount);
			void ThumbAnd(int rd, int rm);
			void ThumbOrr(int rd, int rm);
			void ThumbEor(int rd, int rm);
			void ThumbMvn(int rd, int rm);
			void ThumbLdrImm(int rd, int rn, u32 offset);
			void ThumbStrImm(int rd, int rn, u32 offset);
			void ThumbLdrhImm(int rd, int rn, u32 offset);
			void ThumbStrhImm(int rd, int rn, u32 offset);
			void ThumbLdrbImm(int rd, int rn, u32 offset);
			void ThumbStrbImm(int rd, int rn, u32 offset);
			void ThumbPush(u32 regList);
			void ThumbPop(u32 regList);
			void ThumbB(const std::string& label, Cond c = Cond::AL);
			void ThumbBx(int rm);
			void ThumbSwi(u32 comment);

			// -- output ---------------------------------------------------------------------

			/// <summary>
			/// Resolve the labels, pad the image to `size` bytes with 0xFF (the value an erased
			/// ROM reads as) and return it. Throws std::runtime_error when a label is undefined.
			/// </summary>
			std::vector<u8> TakeImage(u32 size = 0);

			/// <summary>The listing of everything emitted so far ("address: mnemonic").</summary>
			std::string Listing() const { return listing; }

		private:
			u32 pc = 0;
			// The address the image starts at: the first Org() decides it, and everything the
			// emitter stores is indexed by (pc - origin), so a program assembled at 0x08000000
			// produces a short image instead of a 128 MByte one with a hole in front of it. The
			// listing still prints the real addresses.
			u32 origin = 0;
			bool thumbMode = false;
			std::vector<u8> code;
			std::string listing;

			struct Fixup
			{
				u32 address;			// where the branch instruction is
				std::string label;
				bool thumb;
				bool link;
				bool conditional;
				u32 condition;
				bool blx;
			};
			std::vector<Fixup> fixups;
			std::map<std::string, u32> labels;

			void Emit(u32 word, const std::string& text);
			void Emit16(u16 halfword, const std::string& text);
			u32 EncodeArmImmediate(u32 value, bool& ok) const;
			void Record(const std::string& text);
		};
	}
}
