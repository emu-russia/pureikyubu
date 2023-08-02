// DSP DMA registers

#pragma once

namespace DSP
{

	struct DspDmaRegs
	{
		union
		{
			struct
			{
				uint16_t	l;
				uint16_t	h;
			};
			uint32_t	bits;
		} mmemAddr;
		DspAddress  dspAddr;
		uint16_t	blockSize;
		union
		{
			struct
			{
				unsigned Dsp2Mmem : 1;		// 0: MMEM -> DSP, 1: DSP -> MMEM
				unsigned Imem : 1;			// 0: DMEM, 1: IMEM
			};
			uint16_t	bits;
		} control;
	};

}
