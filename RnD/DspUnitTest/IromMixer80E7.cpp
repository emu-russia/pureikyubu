// Testing the work area of the IROM mixer code (0x80E7).
// This mixer imposes volume for the sample from the current VPB and adds the result to the final buffer in double-precision.

/*

// Reconstructed pseudo-code (single-channel)
sub_80E7(uint16_t volume, 
	uint16_t *samples, // Input PCM 16-bit samples
	uint16_t* final_samples // Output 32-bit double-precision samples. [0] - signed high part, [1] - unsigned low
)
{
	for (int i = 0; i < 32; i++)
	{
		int64_t a = (int32_t)(final_samples[i][0] << 16) | final_samples[i][1];
		a <<= 16;
		a += (int16_t)samples[i] * volume * 2;
		a >>= 16;
		final_samples[i][0] = Saturated(a);
		final_samples[i][1] = (uint16_t)a;
	}
}

*/

#include "pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace DspUnitTest
{
	TEST_CLASS(DspUnitTest)
	{
	public:

		TEST_METHOD(TestIromMixer_80E7)
		{
			char text[0x100];
			uint16_t prevFinalOutput[2];
			uint16_t finalOutput[2];
			uint16_t sample;
			uint16_t volume;

			volume = 0x7fff;

			finalOutput[0] = 0;
			finalOutput[1] = 0;

			for (int i = 0; i < 8; i++)
			{
				prevFinalOutput[0] = finalOutput[0];
				prevFinalOutput[1] = finalOutput[1];

				sample = ((int64_t)rand() * __rdtsc()) & 0xffff;
				MixOne(sample, finalOutput, volume);

				sprintf_s(text, sizeof(text), "sample: 0x%04X, final before: 0x%04X_%04X, final after: 0x%04X_%04X, volume: 0x%04X\n",
					sample, prevFinalOutput[0], prevFinalOutput[1], finalOutput[0], finalOutput[1], volume);
				Logger::WriteMessage(text);
			}
		}

		void MixOne(uint16_t sample, uint16_t* finalOutput, uint16_t volume)
		{
			int64_t a = (int32_t)(finalOutput[0] << 16) | finalOutput[1];
			a <<= 16;
			a += Mulsu(sample, volume, true);
			a >>= 16;
			finalOutput[0] = Saturate(a);
			finalOutput[1] = (uint16_t)a;
		}

		// Signed x Unsigned
		int64_t Mulsu(int16_t a, int16_t b, bool scale)
		{
			int64_t bitsPacked = (int64_t)((int64_t)(int32_t)a * (int64_t)(int32_t)(uint16_t)b);
			if (scale)
				bitsPacked <<= 1;
			return bitsPacked;
		}

		uint16_t Saturate(int64_t a)
		{
			uint16_t val = 0;
			if (a != (int32_t)a)
			{
				val = a > 0 ? 0x7fff : 0x8000;
			}
			else
			{
				val = (a >> 16) & 0xffff;
			}
			return val;
		}


	};
}


// Example:
// sample: 0x6429, final before: 0x0000_0000, final after: 0x0000_6428, volume: 0x7FFF
// sample: 0xB496, final before: 0x0000_6428, final after: 0x0000_18BE, volume: 0x7FFF
// sample: 0x68D3, final before: 0x0000_18BE, final after: 0x0000_8190, volume: 0x7FFF
// sample: 0x4EE2, final before: 0x0000_8190, final after: 0x0000_D071, volume: 0x7FFF
// sample: 0x4F67, final before: 0x0000_D071, final after: 0x0001_1FD7, volume: 0x7FFF
// sample: 0xC5E8, final before: 0x0001_1FD7, final after: 0x0000_E5BF, volume: 0x7FFF
// sample: 0x93AB, final before: 0x0000_E5BF, final after: 0x0000_796A, volume: 0x7FFF
// sample: 0x2419, final before: 0x0000_796A, final after: 0x0000_9D82, volume: 0x7FFF
