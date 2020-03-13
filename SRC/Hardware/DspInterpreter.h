// GameCube DSP interpreter

#pragma once

namespace DSP
{
	class DspInterpreter
	{
		DspCore* core;

		void Dispatch(AnalyzeInfo& info);

	public:
		DspInterpreter(DspCore * parent);
		~DspInterpreter();

		void ExecuteInstr();

	};
}
