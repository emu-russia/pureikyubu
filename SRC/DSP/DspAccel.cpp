// DSP ARAM Accelerator

#include "pch.h"

namespace DSP
{
	// Read data by accelerator and optionally decode (raw=false)
	uint16_t DspCore::AccelReadData(bool raw)
	{

		// If the current sample address exceeds the loop-end address, the streaming cache will reset itself to the loop-start position and assert an interrupt.

		return 0;
	}

	// Write RAW data to ARAM
	void DspCore::AccelWriteData(uint16_t data)
	{

	}
}
