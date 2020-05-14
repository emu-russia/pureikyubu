// DSP ADPCM Decoder

#include "pch.h"

namespace DSP
{
	uint16_t DspCore::DecodeAdpcm(uint8_t nibble)
	{
		int pred = (Accel.AdpcmPds >> 4) & 7;
		int scale = Accel.AdpcmPds & 0xf;
		int outputMode = (Accel.Fmt >> 4) & 3;
		int16_t gain = 1 << scale;

		int16_t xn = nibble << 11;
		if (xn & 0x4000)
			xn |= 0x8000;

		int64_t yn = (int64_t)(int32_t)(int16_t)Accel.AdpcmYn1 * (int64_t)(int32_t)(int16_t)Accel.AdpcmCoef[pred]
			+ (int64_t)(int32_t)(int16_t)Accel.AdpcmYn2 * (int64_t)(int32_t)(int16_t)Accel.AdpcmCoef[8 + pred] +
			(int64_t)(int32_t)xn * gain;

		int64_t out = 0;

		Accel.AdpcmYn2 = Accel.AdpcmYn1;
		Accel.AdpcmYn1 = (uint16_t)(yn >> 11);

		switch (outputMode)
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

		// Here you can sniff the sound from the decoder, but for verification it is desirable that a single-channel sound is produced.

#if 0
		FILE* f;
		fopen_s(&f, "adpcmOut.bin", "ab+");
		fwrite(&out, 1, sizeof(uint16_t), f);
		fclose(f);
#endif

		return (uint16_t)out;
	}
}
