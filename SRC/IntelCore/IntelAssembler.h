// An instruction code generator based on information from the AnalyzeInfo structure. 

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
		static void OneByte(AnalyzeInfo& info, uint8_t n);
		static void TwoByte(AnalyzeInfo& info, uint8_t b1, uint8_t b2);
		static void TriByte(AnalyzeInfo& info, uint8_t b1, uint8_t b2, uint8_t b3);
		static void AddUshort(AnalyzeInfo& info, uint16_t n);
		static void AddUlong(AnalyzeInfo& info, uint32_t n);
		static void AddQword(AnalyzeInfo& info, uint64_t n);
		static void OneByteImm8(AnalyzeInfo& info, uint8_t n);
		static void AddImmParam(AnalyzeInfo& info, uint8_t n);
		static void AddPrefix(AnalyzeInfo& info, Prefix pre);
		static void AddPrefixByte(AnalyzeInfo& info, uint8_t pre);

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

		static void AssemblePrefixes(AnalyzeInfo& info);

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

		static void ProcessGpInstr(AnalyzeInfo& info, size_t bits, InstrFeatures& feature);
		static void HandleModRm(AnalyzeInfo& info, size_t bits, uint8_t opcode8, uint8_t opcode16_64, uint8_t opcodeReg, uint8_t extendedOpcode = 0x00);
		static void HandleModRegRm(AnalyzeInfo& info, size_t bits, size_t regParam, size_t rmParam, uint8_t opcode8, uint8_t opcode16_64, uint8_t extendedOpcode = 0x00, uint8_t extendedOpcode2 = 0x00);
		static void HandleModRmImm(AnalyzeInfo& info, size_t bits, uint8_t opcode8, uint8_t opcode16_64, uint8_t opcodeSimm8, uint8_t opcodeReg, uint8_t extendedOpcode = 0x00);
		static void HandleModRegRmImm(AnalyzeInfo& info, size_t bits, size_t regParam, size_t rmParam, uint8_t opcode8, uint8_t opcode16_64, uint8_t extendedOpcode = 0x00);
		static void HandleMoffs(AnalyzeInfo& info, size_t bits, size_t regParam, size_t moffsParam, uint8_t opcode8, uint8_t opcode16_64);
		static void HandleModSregRm(AnalyzeInfo& info, size_t bits, size_t sregParam, size_t rmParam, uint8_t opcode);
		static void HandleModRegRmx(AnalyzeInfo& info, size_t bits, uint8_t opcode8, uint8_t opcode16_64, uint8_t extendedOpcode = 0x00);
		static void HandleModRmRotSh(AnalyzeInfo& info, size_t bits, InstrFeatures& feature);
		static void HandleInOut(AnalyzeInfo& info, size_t bits, bool in);
		static void HandleJcc(AnalyzeInfo& info, size_t bits, uint8_t opcode8, uint8_t opcode16_32);
		static void HandleMovSpecial(AnalyzeInfo& info, size_t bits, uint8_t opcode);

		static bool IsFpuInstr(Instruction instr);
		static void ProcessFpuInstr(AnalyzeInfo& info, size_t bits, FpuInstrFeatures& feature);
		static void FpuAssemble(size_t bits, AnalyzeInfo& info);

	public:

		// Base methods.
		// Determine for which mode the code must be compiled. The `AnalyzeInfo` field values are considered according to the selected mode.

		/// <summary>
		/// Generate an instruction using the AnalyzeInfo information. Use 16-bit architecture. 
		/// </summary>
		static void Assemble16(AnalyzeInfo& info);

		/// <summary>
		/// Generate an instruction using the AnalyzeInfo information. Use 32-bit architecture. 
		/// </summary>
		static void Assemble32(AnalyzeInfo& info);

		/// <summary>
		/// Generate an instruction using the AnalyzeInfo information. Use 64-bit architecture. 
		/// </summary>
		static void Assemble64(AnalyzeInfo& info);

		// Quick helpers.
		// To select a mode, specify 16, 32 or 64 in brackets when calling a method, for example `adc<32> (...)`

		template <size_t n> static AnalyzeInfo adc(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo add(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo _and(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo arpl(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo bound(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo bsf(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo bsr(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo bt(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo btc(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo btr(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo bts(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo call(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo callf(Param p, uint16_t seg = 0, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo cmp(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo cmpxchg(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo dec(Param p, PtrHint ptrHint=PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo div(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo idiv(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo imul(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo imul(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo imul(Param to, Param from, Param i, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo inc(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo invlpg(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo invpcid(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo jmp(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo jmpf(Param p, uint16_t seg = 0, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo lar(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo lds(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo lea(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo les(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo lfs(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo lgdt(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo lgs(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo lidt(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo lldt(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo lmsw(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo lsl(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo lss(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo ltr(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo mov(Param to, Param from, uint64_t disp = 0, int64_t imm = 0, PtrHint ptrHint = PtrHint::NoHint, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo movbe(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo movsx(Param to, Param from, uint64_t disp = 0, PtrHint ptrHint = PtrHint::BytePtr, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo movsxd(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo movzx(Param to, Param from, uint64_t disp = 0, PtrHint ptrHint = PtrHint::BytePtr, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo mul(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo nop(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo _not(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo _or(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo pop(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo push(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo rcl(Param to, Param from, uint64_t disp = 0, int64_t imm = 0, PtrHint ptrHint = PtrHint::NoHint, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo rcr(Param to, Param from, uint64_t disp = 0, int64_t imm = 0, PtrHint ptrHint = PtrHint::NoHint, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo rol(Param to, Param from, uint64_t disp = 0, int64_t imm = 0, PtrHint ptrHint = PtrHint::NoHint, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo ror(Param to, Param from, uint64_t disp = 0, int64_t imm = 0, PtrHint ptrHint = PtrHint::NoHint, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo sal(Param to, Param from, uint64_t disp = 0, int64_t imm = 0, PtrHint ptrHint = PtrHint::NoHint, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo sar(Param to, Param from, uint64_t disp = 0, int64_t imm = 0, PtrHint ptrHint = PtrHint::NoHint, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo sbb(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo seta(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setae(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setb(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setbe(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setc(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo sete(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setg(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setge(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setl(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setle(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setna(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setnae(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setnb(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setnbe(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setnc(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setne(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setng(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setnge(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setnl(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setnle(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setno(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setnp(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setns(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setnz(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo seto(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setp(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setpe(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setpo(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo sets(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo setz(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo sgdt(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo shl(Param to, Param from, uint64_t disp = 0, int64_t imm = 0, PtrHint ptrHint = PtrHint::NoHint, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo shld(Param to, Param from, Param c, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo shr(Param to, Param from, uint64_t disp = 0, int64_t imm = 0, PtrHint ptrHint = PtrHint::NoHint, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo shrd(Param to, Param from, Param c, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo sidt(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo sldt(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo smsw(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo str(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo sub(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo test(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo ud0(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo ud1(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo verr(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo verw(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo xadd(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo xchg(Param to, Param from, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo _xor(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);

		template <size_t n> static AnalyzeInfo bswap(Param p);
		template <size_t n> static AnalyzeInfo in(Param to, Param from, uint8_t imm = 0);
		template <size_t n> static AnalyzeInfo _int(uint8_t imm);
		template <size_t n> static AnalyzeInfo jo(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jno(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jb(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jc(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jnae(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jae(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jnb(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jnc(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo je(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jz(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jne(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jnz(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jbe(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jna(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo ja(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jnbe(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo js(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jns(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jp(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jpe(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jpo(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jnp(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jl(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jnge(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jge(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jnl(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jle(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jng(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jg(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jnle(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jcxz(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jecxz(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo jrcxz(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo loop(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo loope(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo loopz(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo loopne(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo loopnz(Param p, int32_t imm);
		template <size_t n> static AnalyzeInfo out(Param to, Param from, uint8_t imm = 0);
		template <size_t n> static AnalyzeInfo ret(uint16_t imm = 0);
		template <size_t n> static AnalyzeInfo retf(uint16_t imm = 0);

		template <size_t n> static AnalyzeInfo aaa();
		template <size_t n> static AnalyzeInfo aad();
		template <size_t n> static AnalyzeInfo aad(uint8_t v);
		template <size_t n> static AnalyzeInfo aam();
		template <size_t n> static AnalyzeInfo aam(uint8_t v);
		template <size_t n> static AnalyzeInfo aas();
		template <size_t n> static AnalyzeInfo cbw();
		template <size_t n> static AnalyzeInfo cwde();
		template <size_t n> static AnalyzeInfo cdqe();
		template <size_t n> static AnalyzeInfo cwd();
		template <size_t n> static AnalyzeInfo cdq();
		template <size_t n> static AnalyzeInfo cqo();
		template <size_t n> static AnalyzeInfo clc();
		template <size_t n> static AnalyzeInfo cld();
		template <size_t n> static AnalyzeInfo cli();
		template <size_t n> static AnalyzeInfo clts();
		template <size_t n> static AnalyzeInfo cmc();
		template <size_t n> static AnalyzeInfo stc();
		template <size_t n> static AnalyzeInfo std();
		template <size_t n> static AnalyzeInfo sti();
		template <size_t n> static AnalyzeInfo cpuid();
		template <size_t n> static AnalyzeInfo daa();
		template <size_t n> static AnalyzeInfo das();
		template <size_t n> static AnalyzeInfo hlt();
		template <size_t n> static AnalyzeInfo int3();
		template <size_t n> static AnalyzeInfo into();
		template <size_t n> static AnalyzeInfo int1();
		template <size_t n> static AnalyzeInfo invd();
		template <size_t n> static AnalyzeInfo iret();
		template <size_t n> static AnalyzeInfo iretd();
		template <size_t n> static AnalyzeInfo iretq();
		template <size_t n> static AnalyzeInfo lahf();
		template <size_t n> static AnalyzeInfo sahf();
		template <size_t n> static AnalyzeInfo leave();
		template <size_t n> static AnalyzeInfo nop(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo rdmsr();
		template <size_t n> static AnalyzeInfo rdpmc();
		template <size_t n> static AnalyzeInfo rdtsc();
		template <size_t n> static AnalyzeInfo rdtscp();
		template <size_t n> static AnalyzeInfo rsm();
		template <size_t n> static AnalyzeInfo swapgs();
		template <size_t n> static AnalyzeInfo syscall();
		template <size_t n> static AnalyzeInfo sysret();
		template <size_t n> static AnalyzeInfo sysretq();
		template <size_t n> static AnalyzeInfo ud2();
		template <size_t n> static AnalyzeInfo wait();
		template <size_t n> static AnalyzeInfo fwait();
		template <size_t n> static AnalyzeInfo wbinvd();
		template <size_t n> static AnalyzeInfo wrmsr();
		template <size_t n> static AnalyzeInfo xlatb();
		template <size_t n> static AnalyzeInfo popa();
		template <size_t n> static AnalyzeInfo popad();
		template <size_t n> static AnalyzeInfo popf();
		template <size_t n> static AnalyzeInfo popfd();
		template <size_t n> static AnalyzeInfo popfq();
		template <size_t n> static AnalyzeInfo pusha();
		template <size_t n> static AnalyzeInfo pushad();
		template <size_t n> static AnalyzeInfo pushf();
		template <size_t n> static AnalyzeInfo pushfd();
		template <size_t n> static AnalyzeInfo pushfq();
		template <size_t n> static AnalyzeInfo cmpsb(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo cmpsw(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo cmpsd(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo cmpsq(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo lodsb(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo lodsw(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo lodsd(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo lodsq(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo movsb(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo movsw(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo movsd(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo movsq(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo scasb(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo scasw(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo scasd(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo scasq(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo stosb(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo stosw(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo stosd(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo stosq(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo insb(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo insw(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo insd(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo outsb(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo outsw(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo outsd(Prefix pre = Prefix::NoPrefix);

		template <size_t n> static AnalyzeInfo fld(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fst(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fstp(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fild(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fist(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fistp(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fbld(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fbstp(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fxch(Param p = Param::st1);
		template <size_t n> static AnalyzeInfo fcmovb(Param to, Param from);
		template <size_t n> static AnalyzeInfo fcmove(Param to, Param from);
		template <size_t n> static AnalyzeInfo fcmovbe(Param to, Param from);
		template <size_t n> static AnalyzeInfo fcmovu(Param to, Param from);
		template <size_t n> static AnalyzeInfo fcmovnb(Param to, Param from);
		template <size_t n> static AnalyzeInfo fcmovne(Param to, Param from);
		template <size_t n> static AnalyzeInfo fcmovnbe(Param to, Param from);
		template <size_t n> static AnalyzeInfo fcmovnu(Param to, Param from);
		template <size_t n> static AnalyzeInfo fadd(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fadd(Param to, Param from);
		template <size_t n> static AnalyzeInfo faddp(Param to = Param::st1, Param from = Param::st0);
		template <size_t n> static AnalyzeInfo fiadd(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fsub(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fsub(Param to, Param from);
		template <size_t n> static AnalyzeInfo fsubp(Param to = Param::st1, Param from = Param::st0);
		template <size_t n> static AnalyzeInfo fisub(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fsubr(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fsubr(Param to, Param from);
		template <size_t n> static AnalyzeInfo fsubrp(Param to = Param::st1, Param from = Param::st0);
		template <size_t n> static AnalyzeInfo fisubr(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fmul(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fmul(Param to, Param from);
		template <size_t n> static AnalyzeInfo fmulp(Param to = Param::st1, Param from = Param::st0);
		template <size_t n> static AnalyzeInfo fimul(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fdiv(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fdiv(Param to, Param from);
		template <size_t n> static AnalyzeInfo fdivp(Param to = Param::st1, Param from = Param::st0);
		template <size_t n> static AnalyzeInfo fidiv(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fdivr(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fdivr(Param to, Param from);
		template <size_t n> static AnalyzeInfo fdivrp(Param to = Param::st1, Param from = Param::st0);
		template <size_t n> static AnalyzeInfo fidivr(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fprem();
		template <size_t n> static AnalyzeInfo fprem1();
		template <size_t n> static AnalyzeInfo fabs();
		template <size_t n> static AnalyzeInfo fchs();
		template <size_t n> static AnalyzeInfo frndint();
		template <size_t n> static AnalyzeInfo fscale();
		template <size_t n> static AnalyzeInfo fsqrt();
		template <size_t n> static AnalyzeInfo fxtract();
		template <size_t n> static AnalyzeInfo fcom(Param p = Param::st1, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fcomp(Param p = Param::st1, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fcompp();
		template <size_t n> static AnalyzeInfo fucom(Param p = Param::st1);
		template <size_t n> static AnalyzeInfo fucomp(Param p = Param::st1);
		template <size_t n> static AnalyzeInfo fucompp();
		template <size_t n> static AnalyzeInfo ficom(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo ficomp(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fcomi(Param to, Param from);
		template <size_t n> static AnalyzeInfo fcomip(Param to, Param from);
		template <size_t n> static AnalyzeInfo fucomi(Param to, Param from);
		template <size_t n> static AnalyzeInfo fucomip(Param to, Param from);
		template <size_t n> static AnalyzeInfo ftst();
		template <size_t n> static AnalyzeInfo fxam();
		template <size_t n> static AnalyzeInfo fsin();
		template <size_t n> static AnalyzeInfo fcos();
		template <size_t n> static AnalyzeInfo fsincos();
		template <size_t n> static AnalyzeInfo fptan();
		template <size_t n> static AnalyzeInfo fpatan();
		template <size_t n> static AnalyzeInfo f2xm1();
		template <size_t n> static AnalyzeInfo fyl2x();
		template <size_t n> static AnalyzeInfo fyl2xp1();
		template <size_t n> static AnalyzeInfo fld1();
		template <size_t n> static AnalyzeInfo fldl2t();
		template <size_t n> static AnalyzeInfo fldl2e();
		template <size_t n> static AnalyzeInfo fldpi();
		template <size_t n> static AnalyzeInfo fldlg2();
		template <size_t n> static AnalyzeInfo fldln2();
		template <size_t n> static AnalyzeInfo fldz();
		template <size_t n> static AnalyzeInfo fincstp();
		template <size_t n> static AnalyzeInfo fdecstp();
		template <size_t n> static AnalyzeInfo ffree(Param p);
		template <size_t n> static AnalyzeInfo finit();
		template <size_t n> static AnalyzeInfo fninit();
		template <size_t n> static AnalyzeInfo fclex();
		template <size_t n> static AnalyzeInfo fnclex();
		template <size_t n> static AnalyzeInfo fstcw(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fnstcw(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fldcw(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fstenv(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fnstenv(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fldenv(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fsave(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fnsave(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo frstor(Param p, PtrHint ptrHint = PtrHint::NoHint, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fstsw(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fnstsw(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fnop();
		template <size_t n> static AnalyzeInfo fxsave(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fxsave64(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fxrstor(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo fxrstor64(Param p, uint64_t disp = 0, Prefix sr = Prefix::NoPrefix);

	};

}
