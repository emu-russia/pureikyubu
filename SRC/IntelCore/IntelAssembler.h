// An instruction code generator based on information from the DecoderInfo structure. 

// All assembly errors are based on throwing exceptions.
// Therefore, if you need to process them, you need to enclose the call to the class methods in a try/catch block. 

#pragma once

namespace IntelAssemblerUnitTest
{
	// https://github.com/ogamespec/dolwin-rnd/tree/main/IntelAssemblerUnitTest
	class IntelAssemblerUnitTest;
}

namespace IntelCore
{
	class IntelAssembler
	{
		friend IntelAssemblerUnitTest::IntelAssemblerUnitTest;

		/// <summary>
		/// A bitmask enumeration to determine which forms of encoding the instruction supports. 
		/// </summary>
		enum InstrForm : uint32_t
		{
			None = 0,
			Form_I = 0x1,			// Simplified version, when the destination is the al/ax/eax/rax register, and the source is imm
			Form_MI = 0x2,			// rm, imm
			Form_MI8 = 0x4,			// rm, imm	(imm8 only)
			Form_MR = 0x8,			// rm, r  (It will also be used when rm is a register)
			Form_MR16 = 0x10,		// rm, r  (It will also be used when rm is a register)  (16-bit only)
			Form_RM = 0x20,			// r, rm
			Form_RM16 = 0x40,		// r, rm (16-bit only)
			Form_RM32 = 0x80,		// r, rm (32-bit only)
			Form_M = 0x100,			// rm8/rm16/rm32/rm64
			Form_Rel8 = 0x200,		// rel8
			Form_Rel16 = 0x400,		// rel16
			Form_Rel32 = 0x800,		// rel32
			Form_Far16 = 0x1000,	// farptr16
			Form_Far32 = 0x2000,	// farptr32
			Form_O = 0x4000,		// one-byte inc/dec, pop
			Form_RMI = 0x8000,		// r, rm, imm
			Form_M_Strict = 0x1'0000,	// m8/m16/m32/m64  (INVLPG)
			Form_FD = 0x2'0000,		// e.g. al, moffs8
			Form_TD = 0x4'0000,		// e.g. moffs8, al
			Form_OI = 0x8'0000,		// r, imm
			Form_MSr = 0x10'0000,	// rm16/64, Sreg
			Form_SrM = 0x20'0000,	// Sreg, rm16/64
			Form_RMX = 0x40'0000,	// r, r/m  (MOVSX)
			Form_RotSh = 0x80'0000,	// Various options for Rotate/Shift instructions
			Form_Shd = 0x100'0000,	// Various options for SHLD/SHRD (MRI8, MRC)
		};

		/// <summary>
		/// If the instruction does not support some mode, specify this opcode
		/// </summary>
		static constexpr auto UnusedOpcode = 0x0F;

		/// <summary>
		/// Contains information about which forms the instruction supports and which opcodes are used for the specified forms.
		/// This structure is an attempt to somehow generalize the chaos of the x86/x64 instruction format.
		/// This structure is used only for internal implementation, so it looks hacky. If you are not going to understand the assembler internals, you can just skip the definition.
		/// </summary>
		struct InstrFeatures
		{
			uint32_t forms;
			uint8_t Extended_Opcode;			// Specify a nonzero value if you want an additional opcode 1 before the main one. 
			uint8_t Extended_Opcode_RMOnly;		// Additional opcode 1 but only for the RM instruction form
			uint8_t Extended_Opcode2;			// Specify a nonzero value if you want an additional opcode 2 before the main one. 
			uint8_t Form_RegOpcode;				// The part of the opcode that is contained in the `reg` field of the ModRM byte 
			uint8_t Form_I_Opcode8;				// e.g. ADC AL, imm8
			uint8_t Form_I_Opcode16_64;			// e.g. ADC AX, imm16
			uint8_t Form_MI_Opcode8;			// e.g. ADC r/m8, imm8		(If the instruction does not support 8-bit mode, specify UnusedOpcode)
			uint8_t Form_MI_Opcode16_64;		// e.g. ADC r/m32, imm32	(If the instruction does not support 16/32/64-bit mode, specify UnusedOpcode)
			uint8_t Form_MI_Opcode_SImm8;		// e.g. ADC r/m32, simm8
			uint8_t Form_MR_Opcode8;			// e.g. ADC r/m8, r8		(If the instruction does not support 8-bit mode, specify UnusedOpcode)
			uint8_t Form_MR_Opcode16_64;		// e.g. ADC r/m16, r16		
			uint8_t Form_RM_Opcode8;			// e.g. ADC r8, r/m8		(If the instruction does not support 8-bit mode, specify UnusedOpcode)
			uint8_t Form_RM_Opcode16_64;		// e.g. ADC ADC r64, r/m64
			uint8_t Form_MR_Opcode;				// e.g. ARPL r/m16, r16
			uint8_t Form_RM_Opcode;				// e.g. BOUND r16, r/m16
			uint8_t Form_M_Opcode8;				// e.g. DEC r/m8			(If the instruction does not support 8-bit mode, specify UnusedOpcode)
			uint8_t Form_M_Opcode16_64;			// e.g. CALL r/m32
			uint8_t Form_Rel_Opcode8;			// e.g. JMP rel8
			uint8_t Form_Rel_Opcode16_32;		// e.g. CALL rel16
			uint8_t Form_FarPtr_Opcode;			// e.g. CALL far 0x1234:0x1234
			uint8_t Form_O_Opcode;				// e.g. DEC r16
			uint8_t Form_RMI_Opcode8;			// e.g. IMUL r16, r/m16, imm8		(If the instruction does not support 8-bit mode, specify UnusedOpcode)
			uint8_t Form_RMI_Opcode16_64;		// e.g. IMUL r16, r/m16, imm16
			uint8_t Form_FD_Opcode8;			// e.g. MOV AL, moffs8
			uint8_t Form_FD_Opcode16_64;		// e.g. MOV AX, moffs16
			uint8_t Form_TD_Opcode8;			// e.g. MOV moffs8, AL
			uint8_t Form_TD_Opcode16_64;		// e.g. MOV moffs16, AX
			uint8_t Form_OI_Opcode8;			// e.g. MOV r8, imm8
			uint8_t Form_OI_Opcode16_64;		// e.g. MOV r64, imm64
			uint8_t Form_MSr_Opcode;			// Opcode for MOV instructions when Sreg is used
			uint8_t Form_SrM_Opcode;			// Opcode for MOV instructions when Sreg is used
			uint8_t Form_M1_Opcode8;			// e.g. RCL r/m8, 1
			uint8_t Form_M1_Opcode16_64;		// e.g. RCL r/m32, 1
			uint8_t Form_MC_Opcode8;			// e.g. RCL r/m8, CL
			uint8_t Form_MC_Opcode16_64;		// e.g. RCL r/m32, CL
			uint8_t Form_MRI_Opcode;			// e.g. SHLD r/m16, r16, imm8
			uint8_t Form_MRC_Opcode;			// e.g. SHLD r/m16, r16, CL
		};

		static void Invalid();
		static void OneByte(DecoderInfo& info, uint8_t n);
		static void TwoByte(DecoderInfo& info, uint8_t b1, uint8_t b2);
		static void TriByte(DecoderInfo& info, uint8_t b1, uint8_t b2, uint8_t b3);
		static void AddUshort(DecoderInfo& info, uint16_t n);
		static void AddUlong(DecoderInfo& info, uint32_t n);
		static void AddQword(DecoderInfo& info, uint64_t n);
		static void OneByteImm8(DecoderInfo& info, uint8_t n);
		static void AddImmParam(DecoderInfo& info, uint8_t n);
		static void AddPrefix(DecoderInfo& info, Prefix pre);
		static void AddPrefixByte(DecoderInfo& info, uint8_t pre);

		// Methods for determining the category of the parameter. 

		static bool IsSpecial(Param p);
		static bool IsImm(Param p);
		static bool IsSImm(Param p);
		static bool IsRel(Param p);
		static bool IsFarPtr(Param p);
		static bool IsMoffs(Param p);
		static bool IsReg(Param p);
		static bool IsReg8(Param p);
		static bool IsReg16(Param p);
		static bool IsReg32(Param p);
		static bool IsReg64(Param p);
		static bool IsSreg(Param p);
		static bool IsCR(Param p);
		static bool IsDR(Param p);
		static bool IsTR(Param p);
		static bool IsFpuReg(Param p);
		static bool IsMem(Param p);
		static bool IsMem16(Param p);
		static bool IsMem32(Param p);
		static bool IsMem64(Param p);
		static bool IsSib(Param p);
		static bool IsMemDisp8(Param p);
		static bool IsMemDisp16(Param p);
		static bool IsMemDisp32(Param p);
		static bool IsMemDisp(Param p);

		static void AssemblePrefixes(DecoderInfo& info);

		// These methods decompose the parameter into its constituent parts, which are included in the ModRM/SIB byte fields.

		static void GetReg(Param p, size_t& reg);
		static void GetSreg(Param p, size_t& sreg);
		static void GetSpecReg(Param p, size_t& reg);
		static void GetFpuReg(Param p, size_t& reg);
		static void GetMod(Param p, size_t& mod);
		static void GetRm(Param p, size_t& rm);
		static void GetSS(Param p, size_t& scale);
		static void GetIndex(Param p, size_t& index);
		static void GetBase(Param p, size_t& base);

		static void ProcessGpInstr(DecoderInfo& info, size_t bits, InstrFeatures& feature);
		static void HandleModRm(DecoderInfo& info, size_t bits, uint8_t opcode8, uint8_t opcode16_64, uint8_t opcodeReg, uint8_t extendedOpcode = 0x00);
		static void HandleModRegRm(DecoderInfo& info, size_t bits, size_t regParam, size_t rmParam, uint8_t opcode8, uint8_t opcode16_64, uint8_t extendedOpcode = 0x00, uint8_t extendedOpcode2 = 0x00);
		static void HandleModRmImm(DecoderInfo& info, size_t bits, uint8_t opcode8, uint8_t opcode16_64, uint8_t opcodeSimm8, uint8_t opcodeReg, uint8_t extendedOpcode = 0x00);
		static void HandleModRegRmImm(DecoderInfo& info, size_t bits, size_t regParam, size_t rmParam, uint8_t opcode8, uint8_t opcode16_64, uint8_t extendedOpcode = 0x00);
		static void HandleMoffs(DecoderInfo& info, size_t bits, size_t regParam, size_t moffsParam, uint8_t opcode8, uint8_t opcode16_64);
		static void HandleModSregRm(DecoderInfo& info, size_t bits, size_t sregParam, size_t rmParam, uint8_t opcode);
		static void HandleModRegRmx(DecoderInfo& info, size_t bits, uint8_t opcode8, uint8_t opcode16_64, uint8_t extendedOpcode = 0x00);
		static void HandleModRmRotSh(DecoderInfo& info, size_t bits, InstrFeatures& feature);
		static void HandleInOut(DecoderInfo& info, size_t bits, bool in);
		static void HandleJcc(DecoderInfo& info, size_t bits, uint8_t opcode8, uint8_t opcode16_32);
		static void HandleMovSpecial(DecoderInfo& info, size_t bits, uint8_t opcode);

		static bool IsFpuInstr(Instruction instr);
		static void HandleModRmFpu(DecoderInfo& info, size_t bits, uint8_t opcode, uint8_t opcodeReg, uint8_t extendedOpcode);
		static void ProcessFpuInstr(DecoderInfo& info, size_t bits, FpuInstrFeatures& feature);
		static void FpuAssemble(size_t bits, DecoderInfo& info);

	public:

		// Base methods.
		// Determine for which mode the code must be compiled. The `DecoderInfo` field values are considered according to the selected mode.

		/// <summary>
		/// Generate an instruction using the DecoderInfo information. Use 16-bit architecture. 
		/// </summary>
		static void Assemble16(DecoderInfo& info);

		/// <summary>
		/// Generate an instruction using the DecoderInfo information. Use 32-bit architecture. 
		/// </summary>
		static void Assemble32(DecoderInfo& info);

		/// <summary>
		/// Generate an instruction using the DecoderInfo information. Use 64-bit architecture. 
		/// </summary>
		static void Assemble64(DecoderInfo& info);

		// Quick helpers.
		// To select a mode, specify 16, 32 or 64 in brackets when calling a method, for example `adc<32> (...)`

		template <size_t n> static DecoderInfo adc(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo add(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo _and(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo arpl(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo bound(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo bsf(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo bsr(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo bt(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo btc(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo btr(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo bts(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo call(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo callf(Param p, uint16_t seg = 0, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo cmp(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo cmpxchg(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo dec(Param p, PtrHint ptrHint=PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo div(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo idiv(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo imul(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo imul(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo imul(Param to, Param from, Param i, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo inc(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo invlpg(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo invpcid(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo jmp(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo jmpf(Param p, uint16_t seg = 0, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo lar(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo lds(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo lea(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo les(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo lfs(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo lgdt(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo lgs(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo lidt(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo lldt(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo lmsw(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo lsl(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo lss(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo ltr(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo mov(Param to, Param from, uint64_t disp = 0, int64_t imm = 0, PtrHint ptrHint = PtrHint::NoHint, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo movbe(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo movsx(Param to, Param from, uint64_t disp = 0, PtrHint ptrHint = PtrHint::BytePtr, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo movsxd(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo movzx(Param to, Param from, uint64_t disp = 0, PtrHint ptrHint = PtrHint::BytePtr, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo mul(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo nop(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo _not(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo _or(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo pop(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo push(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo rcl(Param to, Param from, uint64_t disp = 0, int64_t imm = 0, PtrHint ptrHint = PtrHint::NoHint, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo rcr(Param to, Param from, uint64_t disp = 0, int64_t imm = 0, PtrHint ptrHint = PtrHint::NoHint, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo rol(Param to, Param from, uint64_t disp = 0, int64_t imm = 0, PtrHint ptrHint = PtrHint::NoHint, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo ror(Param to, Param from, uint64_t disp = 0, int64_t imm = 0, PtrHint ptrHint = PtrHint::NoHint, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo sal(Param to, Param from, uint64_t disp = 0, int64_t imm = 0, PtrHint ptrHint = PtrHint::NoHint, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo sar(Param to, Param from, uint64_t disp = 0, int64_t imm = 0, PtrHint ptrHint = PtrHint::NoHint, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo sbb(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo seta(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setae(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setb(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setbe(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setc(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo sete(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setg(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setge(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setl(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setle(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setna(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setnae(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setnb(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setnbe(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setnc(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setne(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setng(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setnge(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setnl(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setnle(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setno(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setnp(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setns(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setnz(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo seto(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setp(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setpe(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setpo(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo sets(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo setz(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo sgdt(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo shl(Param to, Param from, uint64_t disp = 0, int64_t imm = 0, PtrHint ptrHint = PtrHint::NoHint, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo shld(Param to, Param from, Param c, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo shr(Param to, Param from, uint64_t disp = 0, int64_t imm = 0, PtrHint ptrHint = PtrHint::NoHint, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo shrd(Param to, Param from, Param c, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo sidt(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo sldt(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo smsw(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo str(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo sub(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo test(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo ud0(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo ud1(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo verr(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo verw(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo xadd(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo xchg(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo _xor(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);

		template <size_t n> static DecoderInfo bswap(Param p);
		template <size_t n> static DecoderInfo in(Param to, Param from, uint8_t imm = 0);
		template <size_t n> static DecoderInfo _int(uint8_t imm);
		template <size_t n> static DecoderInfo jo(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jno(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jb(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jc(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jnae(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jae(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jnb(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jnc(Param p, int32_t imm);
		template <size_t n> static DecoderInfo je(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jz(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jne(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jnz(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jbe(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jna(Param p, int32_t imm);
		template <size_t n> static DecoderInfo ja(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jnbe(Param p, int32_t imm);
		template <size_t n> static DecoderInfo js(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jns(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jp(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jpe(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jpo(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jnp(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jl(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jnge(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jge(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jnl(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jle(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jng(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jg(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jnle(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jcxz(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jecxz(Param p, int32_t imm);
		template <size_t n> static DecoderInfo jrcxz(Param p, int32_t imm);
		template <size_t n> static DecoderInfo loop(Param p, int32_t imm);
		template <size_t n> static DecoderInfo loope(Param p, int32_t imm);
		template <size_t n> static DecoderInfo loopz(Param p, int32_t imm);
		template <size_t n> static DecoderInfo loopne(Param p, int32_t imm);
		template <size_t n> static DecoderInfo loopnz(Param p, int32_t imm);
		template <size_t n> static DecoderInfo out(Param to, Param from, uint8_t imm = 0);
		template <size_t n> static DecoderInfo ret(uint16_t imm = 0);
		template <size_t n> static DecoderInfo retf(uint16_t imm = 0);

		template <size_t n> static DecoderInfo aaa();
		template <size_t n> static DecoderInfo aad();
		template <size_t n> static DecoderInfo aad(uint8_t v);
		template <size_t n> static DecoderInfo aam();
		template <size_t n> static DecoderInfo aam(uint8_t v);
		template <size_t n> static DecoderInfo aas();
		template <size_t n> static DecoderInfo cbw();
		template <size_t n> static DecoderInfo cwde();
		template <size_t n> static DecoderInfo cdqe();
		template <size_t n> static DecoderInfo cwd();
		template <size_t n> static DecoderInfo cdq();
		template <size_t n> static DecoderInfo cqo();
		template <size_t n> static DecoderInfo clc();
		template <size_t n> static DecoderInfo cld();
		template <size_t n> static DecoderInfo cli();
		template <size_t n> static DecoderInfo clts();
		template <size_t n> static DecoderInfo cmc();
		template <size_t n> static DecoderInfo stc();
		template <size_t n> static DecoderInfo std();
		template <size_t n> static DecoderInfo sti();
		template <size_t n> static DecoderInfo cpuid();
		template <size_t n> static DecoderInfo daa();
		template <size_t n> static DecoderInfo das();
		template <size_t n> static DecoderInfo hlt();
		template <size_t n> static DecoderInfo int3();
		template <size_t n> static DecoderInfo into();
		template <size_t n> static DecoderInfo int1();
		template <size_t n> static DecoderInfo invd();
		template <size_t n> static DecoderInfo iret();
		template <size_t n> static DecoderInfo iretd();
		template <size_t n> static DecoderInfo iretq();
		template <size_t n> static DecoderInfo lahf();
		template <size_t n> static DecoderInfo sahf();
		template <size_t n> static DecoderInfo leave();
		template <size_t n> static DecoderInfo nop(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo rdmsr();
		template <size_t n> static DecoderInfo rdpmc();
		template <size_t n> static DecoderInfo rdtsc();
		template <size_t n> static DecoderInfo rdtscp();
		template <size_t n> static DecoderInfo rsm();
		template <size_t n> static DecoderInfo swapgs();
		template <size_t n> static DecoderInfo syscall();
		template <size_t n> static DecoderInfo sysret();
		template <size_t n> static DecoderInfo sysretq();
		template <size_t n> static DecoderInfo ud2();
		template <size_t n> static DecoderInfo wait();
		template <size_t n> static DecoderInfo fwait();
		template <size_t n> static DecoderInfo wbinvd();
		template <size_t n> static DecoderInfo wrmsr();
		template <size_t n> static DecoderInfo xlatb();
		template <size_t n> static DecoderInfo popa();
		template <size_t n> static DecoderInfo popad();
		template <size_t n> static DecoderInfo popf();
		template <size_t n> static DecoderInfo popfd();
		template <size_t n> static DecoderInfo popfq();
		template <size_t n> static DecoderInfo pusha();
		template <size_t n> static DecoderInfo pushad();
		template <size_t n> static DecoderInfo pushf();
		template <size_t n> static DecoderInfo pushfd();
		template <size_t n> static DecoderInfo pushfq();
		template <size_t n> static DecoderInfo cmpsb(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo cmpsw(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo cmpsd(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo cmpsq(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo lodsb(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo lodsw(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo lodsd(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo lodsq(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo movsb(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo movsw(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo movsd(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo movsq(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo scasb(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo scasw(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo scasd(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo scasq(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo stosb(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo stosw(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo stosd(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo stosq(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo insb(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo insw(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo insd(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo outsb(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo outsw(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo outsd(Prefix pre = Prefix::NoPrefix);

		template <size_t n> static DecoderInfo fld(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fst(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fstp(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fild(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fist(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fistp(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fbld(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fbstp(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fxch(Param p = Param::st1);
		template <size_t n> static DecoderInfo fcmovb(Param to, Param from);
		template <size_t n> static DecoderInfo fcmove(Param to, Param from);
		template <size_t n> static DecoderInfo fcmovbe(Param to, Param from);
		template <size_t n> static DecoderInfo fcmovu(Param to, Param from);
		template <size_t n> static DecoderInfo fcmovnb(Param to, Param from);
		template <size_t n> static DecoderInfo fcmovne(Param to, Param from);
		template <size_t n> static DecoderInfo fcmovnbe(Param to, Param from);
		template <size_t n> static DecoderInfo fcmovnu(Param to, Param from);
		template <size_t n> static DecoderInfo fadd(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fadd(Param to, Param from);
		template <size_t n> static DecoderInfo faddp(Param to = Param::st1, Param from = Param::st0);
		template <size_t n> static DecoderInfo fiadd(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fsub(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fsub(Param to, Param from);
		template <size_t n> static DecoderInfo fsubp(Param to = Param::st1, Param from = Param::st0);
		template <size_t n> static DecoderInfo fisub(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fsubr(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fsubr(Param to, Param from);
		template <size_t n> static DecoderInfo fsubrp(Param to = Param::st1, Param from = Param::st0);
		template <size_t n> static DecoderInfo fisubr(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fmul(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fmul(Param to, Param from);
		template <size_t n> static DecoderInfo fmulp(Param to = Param::st1, Param from = Param::st0);
		template <size_t n> static DecoderInfo fimul(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fdiv(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fdiv(Param to, Param from);
		template <size_t n> static DecoderInfo fdivp(Param to = Param::st1, Param from = Param::st0);
		template <size_t n> static DecoderInfo fidiv(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fdivr(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fdivr(Param to, Param from);
		template <size_t n> static DecoderInfo fdivrp(Param to = Param::st1, Param from = Param::st0);
		template <size_t n> static DecoderInfo fidivr(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fprem();
		template <size_t n> static DecoderInfo fprem1();
		template <size_t n> static DecoderInfo fabs();
		template <size_t n> static DecoderInfo fchs();
		template <size_t n> static DecoderInfo frndint();
		template <size_t n> static DecoderInfo fscale();
		template <size_t n> static DecoderInfo fsqrt();
		template <size_t n> static DecoderInfo fxtract();
		template <size_t n> static DecoderInfo fcom(Param p = Param::st1, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fcomp(Param p = Param::st1, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fcompp();
		template <size_t n> static DecoderInfo fucom(Param p = Param::st1);
		template <size_t n> static DecoderInfo fucomp(Param p = Param::st1);
		template <size_t n> static DecoderInfo fucompp();
		template <size_t n> static DecoderInfo ficom(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo ficomp(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fcomi(Param to, Param from);
		template <size_t n> static DecoderInfo fcomip(Param to, Param from);
		template <size_t n> static DecoderInfo fucomi(Param to, Param from);
		template <size_t n> static DecoderInfo fucomip(Param to, Param from);
		template <size_t n> static DecoderInfo ftst();
		template <size_t n> static DecoderInfo fxam();
		template <size_t n> static DecoderInfo fsin();
		template <size_t n> static DecoderInfo fcos();
		template <size_t n> static DecoderInfo fsincos();
		template <size_t n> static DecoderInfo fptan();
		template <size_t n> static DecoderInfo fpatan();
		template <size_t n> static DecoderInfo f2xm1();
		template <size_t n> static DecoderInfo fyl2x();
		template <size_t n> static DecoderInfo fyl2xp1();
		template <size_t n> static DecoderInfo fld1();
		template <size_t n> static DecoderInfo fldl2t();
		template <size_t n> static DecoderInfo fldl2e();
		template <size_t n> static DecoderInfo fldpi();
		template <size_t n> static DecoderInfo fldlg2();
		template <size_t n> static DecoderInfo fldln2();
		template <size_t n> static DecoderInfo fldz();
		template <size_t n> static DecoderInfo fincstp();
		template <size_t n> static DecoderInfo fdecstp();
		template <size_t n> static DecoderInfo ffree(Param p);
		template <size_t n> static DecoderInfo finit();
		template <size_t n> static DecoderInfo fninit();
		template <size_t n> static DecoderInfo fclex();
		template <size_t n> static DecoderInfo fnclex();
		template <size_t n> static DecoderInfo fstcw(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fnstcw(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fldcw(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fstenv(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fnstenv(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fldenv(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fsave(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fnsave(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo frstor(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fstsw(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fnstsw(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fnop();
		template <size_t n> static DecoderInfo fxsave(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fxsave64(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fxrstor(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static DecoderInfo fxrstor64(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);

	};

}
