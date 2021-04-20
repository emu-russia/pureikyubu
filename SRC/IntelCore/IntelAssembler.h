// An instruction code generator based on information from the AnalyzeInfo structure. 

// All assembly errors are based on throwing exceptions.
// Therefore, if you need to process them, you need to enclose the call to the class methods in a try/catch block. 

#pragma once

namespace IntelCore
{
	class IntelAssembler
	{

		static void Invalid();
		static void OneByte(AnalyzeInfo& info, uint8_t n);
		static void TwoByte(AnalyzeInfo& info, uint8_t b1, uint8_t b2);
		static void TriByte(AnalyzeInfo& info, uint8_t b1, uint8_t b2, uint8_t b3);
		static void OneByteImm8(AnalyzeInfo& info, uint8_t n);
		static void AddImmParam(AnalyzeInfo& info, uint8_t n);
		static void AddPrefix(AnalyzeInfo& info, Prefix pre);
		static void AddPrefixByte(AnalyzeInfo& info, uint8_t pre);

		// Methods for determining the category of the parameter. 

		static bool IsImm(Param p);
		static bool IsReg(Param p);
		static bool IsMem(Param p);
		static bool IsSib(Param p);
		static bool IsMemDisp8(Param p);
		static bool IsMemDisp16(Param p);
		static bool IsMemDisp32(Param p);

		static bool AssemblePrefixes(AnalyzeInfo& info);

		// These methods decompose the parameter into its constituent parts, which are included in the ModRM/SIB byte fields.

		static void GetReg(Param p, size_t& reg);
		static void GetMod(Param p, size_t& mod);
		static void GetRm(Param p, size_t& rm);
		static void GetSS(Param p, size_t& scale);
		static void GetIndex(Param p, size_t& index);
		static void GetBase(Param p, size_t& base);

		static void ModRegRm(AnalyzeInfo& info);

	public:

		// Base methods.
		// Determine for which mode the code must be compiled. The `AnalyzeInfo` field values are considered according to the selected mode.

		static void Assemble16(AnalyzeInfo& info);
		static void Assemble32(AnalyzeInfo& info);
		static void Assemble64(AnalyzeInfo& info);

		// Quick helpers.
		// To select a mode, specify 16, 32 or 64 in brackets when calling a method, for example `adc<32> (...)`

		template <size_t n> static AnalyzeInfo& adc(Param to, Param from, uint64_t disp = 0, int32_t imm = 0, Prefix sr = Prefix::NoPrefix, Prefix lock = Prefix::NoPrefix);

		template <size_t n> static AnalyzeInfo& aaa();
		template <size_t n> static AnalyzeInfo& aad();
		template <size_t n> static AnalyzeInfo& aad(uint8_t v);
		template <size_t n> static AnalyzeInfo& aam();
		template <size_t n> static AnalyzeInfo& aam(uint8_t v);
		template <size_t n> static AnalyzeInfo& aas();
		template <size_t n> static AnalyzeInfo& cbw();
		template <size_t n> static AnalyzeInfo& cwde();
		template <size_t n> static AnalyzeInfo& cdqe();
		template <size_t n> static AnalyzeInfo& cwd();
		template <size_t n> static AnalyzeInfo& cdq();
		template <size_t n> static AnalyzeInfo& cqo();
		template <size_t n> static AnalyzeInfo& clc();
		template <size_t n> static AnalyzeInfo& cld();
		template <size_t n> static AnalyzeInfo& cli();
		template <size_t n> static AnalyzeInfo& clts();
		template <size_t n> static AnalyzeInfo& cmc();
		template <size_t n> static AnalyzeInfo& stc();
		template <size_t n> static AnalyzeInfo& std();
		template <size_t n> static AnalyzeInfo& sti();
		template <size_t n> static AnalyzeInfo& cpuid();
		template <size_t n> static AnalyzeInfo& daa();
		template <size_t n> static AnalyzeInfo& das();
		template <size_t n> static AnalyzeInfo& hlt();
		template <size_t n> static AnalyzeInfo& int3();
		template <size_t n> static AnalyzeInfo& into();
		template <size_t n> static AnalyzeInfo& int1();
		template <size_t n> static AnalyzeInfo& invd();
		template <size_t n> static AnalyzeInfo& iret();
		template <size_t n> static AnalyzeInfo& iretd();
		template <size_t n> static AnalyzeInfo& iretq();
		template <size_t n> static AnalyzeInfo& lahf();
		template <size_t n> static AnalyzeInfo& sahf();
		template <size_t n> static AnalyzeInfo& leave();
		template <size_t n> static AnalyzeInfo& nop();
		template <size_t n> static AnalyzeInfo& rdmsr();
		template <size_t n> static AnalyzeInfo& rdpmc();
		template <size_t n> static AnalyzeInfo& rdtsc();
		template <size_t n> static AnalyzeInfo& rdtscp();
		template <size_t n> static AnalyzeInfo& rsm();
		template <size_t n> static AnalyzeInfo& swapgs();
		template <size_t n> static AnalyzeInfo& syscall();
		template <size_t n> static AnalyzeInfo& sysret();
		template <size_t n> static AnalyzeInfo& sysretq();
		template <size_t n> static AnalyzeInfo& ud2();
		template <size_t n> static AnalyzeInfo& wait();
		template <size_t n> static AnalyzeInfo& fwait();
		template <size_t n> static AnalyzeInfo& wbinvd();
		template <size_t n> static AnalyzeInfo& wrmsr();
		template <size_t n> static AnalyzeInfo& xlatb();
		template <size_t n> static AnalyzeInfo& popa();
		template <size_t n> static AnalyzeInfo& popad();
		template <size_t n> static AnalyzeInfo& popf();
		template <size_t n> static AnalyzeInfo& popfd();
		template <size_t n> static AnalyzeInfo& popfq();
		template <size_t n> static AnalyzeInfo& pusha();
		template <size_t n> static AnalyzeInfo& pushad();
		template <size_t n> static AnalyzeInfo& pushf();
		template <size_t n> static AnalyzeInfo& pushfd();
		template <size_t n> static AnalyzeInfo& pushfq();
		template <size_t n> static AnalyzeInfo& cmpsb(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo& cmpsw(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo& cmpsd(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo& cmpsq(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo& lodsb(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo& lodsw(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo& lodsd(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo& lodsq(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo& movsb(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo& movsw(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo& movsd(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo& movsq(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo& scasb(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo& scasw(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo& scasd(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo& scasq(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo& stosb(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo& stosw(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo& stosd(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo& stosq(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo& insb(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo& insw(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo& insd(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo& outsb(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo& outsw(Prefix pre = Prefix::NoPrefix);
		template <size_t n> static AnalyzeInfo& outsd(Prefix pre = Prefix::NoPrefix);

	};

}
