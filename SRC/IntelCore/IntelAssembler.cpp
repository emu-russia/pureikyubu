// Intel instruction code generator.

#include "pch.h"

namespace IntelCore
{

	void IntelAssembler::Assemble16(AnalyzeInfo& info)
	{

	}

	void IntelAssembler::Assemble32(AnalyzeInfo& info)
	{

	}

	void IntelAssembler::Assemble64(AnalyzeInfo& info)
	{

	}

#pragma region "Quick helpers"

	template <> AnalyzeInfo& IntelAssembler::adc<16>()
	{
		AnalyzeInfo info;
		Assemble16(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::adc<32>()
	{
		AnalyzeInfo info;
		Assemble32(info);
		return info;
	}

	template <> AnalyzeInfo& IntelAssembler::adc<64>()
	{
		AnalyzeInfo info;
		Assemble64(info);
		return info;
	}

#pragma endregion "Quick helpers"

}
