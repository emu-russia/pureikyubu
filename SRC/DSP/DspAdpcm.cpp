// DSP ADPCM Decoder

#include "pch.h"

namespace DSP
{
	uint16_t DspCore::DecodeAdpcm(uint8_t nibble)
	{
		int pred = (Accel.AdpcmPds >> 4) & 7;
		int scale = Accel.AdpcmPds & 0xf;
		int mode = (Accel.Fmt >> 4) & 3;

		int16_t xn = nibble << 11;
		if (xn & 0x4000)
			xn |= 0x8000;

		int64_t yn = (int64_t)(int32_t)Accel.AdpcmYn1 * (int64_t)(int32_t)Accel.AdpcmCoef[2 * pred + 0]
			+ (int64_t)(int32_t)Accel.AdpcmYn2 * (int64_t)(int32_t)Accel.AdpcmCoef[2 * pred + 1] +
			(int64_t)(int32_t)xn;

		int64_t out = 0;

		switch (mode)
		{
			case 0:
				out = yn >> 11;
				out = max(-0x8000, min(out, 0x7FFF));
				break;
			case 1:
				out = yn & 0xffff;
				break;
			case 2:
				out = (yn >> 16);
				break;
		}

		Accel.AdpcmYn2 = Accel.AdpcmYn1;
		Accel.AdpcmYn1 = (uint16_t)out;

		return (uint16_t)out;
	}
}
