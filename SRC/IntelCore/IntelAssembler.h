// An instruction code generator based on information from the AnalyzeInfo structure. 

#pragma once

namespace IntelCore
{
	class IntelAssembler
	{

	public:

		// Base methods

		static void Assemble16(AnalyzeInfo& info);
		static void Assemble32(AnalyzeInfo& info);
		static void Assemble64(AnalyzeInfo& info);

		// Quick helpers

		template <size_t n> static AnalyzeInfo& adc();

	};

}
