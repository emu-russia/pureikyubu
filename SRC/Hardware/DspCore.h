// Low-level DSP core

#pragma once

namespace DSP
{
	class DspCore
	{
	public:
		DspCore(HWConfig* config);
		~DspCore();

		void Update();
	};
}
