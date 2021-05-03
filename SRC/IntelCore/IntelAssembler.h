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
			Form_O = 0x4000,		// one-byte inc/dec
			Form_RMI = 0x8000,		// r, rm, imm
			Form_M_Strict = 0x1'0000,	// m8/m16/m32/m64  (INVLPG)
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
		};

		static void Invalid();
		static void OneByte(AnalyzeInfo& info, uint8_t n);
		static void TwoByte(AnalyzeInfo& info, uint8_t b1, uint8_t b2);
		static void TriByte(AnalyzeInfo& info, uint8_t b1, uint8_t b2, uint8_t b3);
		static void AddUshort(AnalyzeInfo& info, uint16_t n);
		static void AddUlong(AnalyzeInfo& info, uint32_t n);
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
		static bool IsReg(Param p);
		static bool IsReg8(Param p);
		static bool IsReg16(Param p);
		static bool IsReg32(Param p);
		static bool IsReg64(Param p);
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
		template <size_t n> static AnalyzeInfo nop();
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

	};

}
