// DSP accelerator state

#pragma once

namespace DSP
{
	union DspAccelAddress
	{
		struct
		{
			uint16_t l;
			uint16_t h;
		};
		uint32_t addr;
	};

	struct DspAccel
	{
		uint16_t Fmt;					// Sample format
		uint16_t AdpcmCoef[16];
		uint16_t AdpcmPds;				// predictor / scale combination
		uint16_t AdpcmYn1;				// y[n - 1]
		uint16_t AdpcmYn2;				// y[n - 2]
		uint16_t AdpcmGan;				// gain to be applied
		DspAccelAddress StartAddress;
		DspAccelAddress EndAddress;
		DspAccelAddress CurrAddress;
	};

}
