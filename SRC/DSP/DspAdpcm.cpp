// DSP ADPCM Decoder

#include "pch.h"

namespace DSP
{
	uint16_t DspCore::DecodeAdpcm(uint8_t nibble)
	{
		// Just noise for now
		return rand () & 0x3ff;
	}
}
