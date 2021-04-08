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
		static void OneByteImm8(AnalyzeInfo& info, uint8_t n);
		static void AddImmParam(AnalyzeInfo& info, uint8_t n);

	public:

		// Base methods.
		// Determine for which mode the code must be compiled. The `AnalyzeInfo` field values are considered according to the selected mode.

		static void Assemble16(AnalyzeInfo& info);
		static void Assemble32(AnalyzeInfo& info);
		static void Assemble64(AnalyzeInfo& info);

		// Quick helpers.
		// To select a mode, specify 16, 32 or 64 in brackets when calling a method, for example `adc<32> (...)`

		template <size_t n> static AnalyzeInfo& adc();

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

	};

}
