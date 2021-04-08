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

	public:

		// Base methods.
		// Determine for which mode the code must be compiled. The `AnalyzeInfo` field values are considered according to the selected mode.

		static void Assemble16(AnalyzeInfo& info);
		static void Assemble32(AnalyzeInfo& info);
		static void Assemble64(AnalyzeInfo& info);

		// Quick helpers.
		// To select a mode, specify 16, 32 or 64 in brackets when calling a method, for example `adc<32> (...)`

		template <size_t n> static AnalyzeInfo& aaa();
		template <size_t n> static AnalyzeInfo& adc();

	};

}
