// GameCube DSP interpreter

#pragma once

namespace DSP
{
	class DspInterpreter
	{
		DspCore* core;

		// Instructions

		void LRI(AnalyzeInfo& info);

		void M2(AnalyzeInfo& info);
		void M0(AnalyzeInfo& info);
		void CLR15(AnalyzeInfo& info);
		void SET15(AnalyzeInfo& info);
		void CLR40(AnalyzeInfo& info);
		void SET40(AnalyzeInfo& info);

		void SBSET(AnalyzeInfo& info);

		// Packed instructions

		void Dispatch(AnalyzeInfo& info);

	public:
		DspInterpreter(DspCore * parent);
		~DspInterpreter();

		void ExecuteInstr();

	};
}
