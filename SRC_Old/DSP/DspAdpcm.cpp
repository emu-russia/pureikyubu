// DSP PCM/ADPCM Decoder

#include "pch.h"

using namespace Debug;

#define my_max(a,b) (((a) > (b)) ? (a) : (b))
#define my_min(a,b) (((a) < (b)) ? (a) : (b))

namespace DSP
{
	uint16_t Dsp16::DecodeAdpcm(uint16_t in)
	{
		int64_t yn = 0;
		int64_t out = 0;
		int outputMode = (Accel.Fmt >> 4) & 3;

		switch ((Accel.Fmt >> 2) & 3)
		{
			case 0:
			{
				int pred = (Accel.AdpcmPds >> 4) & 7;
				int scale = Accel.AdpcmPds & 0xf;
				int16_t gain = 1 << scale;

				if (scale > 0xc)
					scale = 0xc;

				int16_t xn = in << 11;
				if (xn & 0x4000)
					xn |= 0x8000;

				yn = (int64_t)(int32_t)(int16_t)Accel.AdpcmYn1 * (int64_t)(int32_t)(int16_t)Accel.AdpcmCoef[2 * pred]
					+ (int64_t)(int32_t)(int16_t)Accel.AdpcmYn2 * (int64_t)(int32_t)(int16_t)Accel.AdpcmCoef[2 * pred + 1] +
					(int64_t)(int32_t)xn * gain;

				Accel.AdpcmYn2 = Accel.AdpcmYn1;
				Accel.AdpcmYn1 = (uint16_t)(yn >> 11);
				break;
			}

			case 1:
				yn = (int64_t)(int32_t)((int16_t)(in << 8)) * Accel.AdpcmGan;
				break;

			case 2:
				yn = (int64_t)(int32_t)(int16_t)in * Accel.AdpcmGan;
				break;
		}

		switch (outputMode)
		{
			case 0:
				out = yn >> 11;
				out = my_max(-0x8000, my_min(out, 0x7FFF));
				break;
			case 1:
				out = yn & 0xffff;
				break;
			case 2:
				out = (yn >> 16);
				break;
			case 3:
				Halt("DSP: Unsupported Decoder output mode\n");
				break;
		}

		//Report(Channel::DSP, "0x%08X = 0x%04X\n", (Accel.CurrAddress.addr & 0x07FF'FFFF) - 1, (uint16_t)out);

		return (uint16_t)out;
	}
}
