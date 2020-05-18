// Code for checking AX Ucode Volume Envelope mixer.

/*


// Apply volume to sample-rate-converted samples (#0x0E48 - volume envelope per 32 samples, $0x0E43 - pointer to sample rate converted samples)

0355 8F 00       	set40
0356 00 80 0E 48 	lri  	ar0, #0x0E48 						// Volume envelope buffer for 32 samples
0358 00 C1 0E 43 	lr   	ar1, $0x0E43 						// 0xCE0 ...
035A 1C 61       	mrr  	ar3, ar1 							// ar3 = ar1 (New values replaces old)
035B 19 3A       	lrri 	ax0.h, @ar1
035C 19 18       	lrri 	ax0.l, @ar0
035D 90 59       	mul  	ax0.l, ax0.h    	l    	ax1.h, @ar1
035E 19 19       	lrri 	ax1.l, @ar0
035F 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
0360 80 80       	nx   	                	ls   	ax0.l, ac0.m
0361 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
0362 80 91       	nx   	                	ls   	ax1.l, ac1.m
0363 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
0364 80 80       	nx   	                	ls   	ax0.l, ac0.m
0365 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
0366 80 91       	nx   	                	ls   	ax1.l, ac1.m
0367 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
0368 80 80       	nx   	                	ls   	ax0.l, ac0.m
0369 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
036A 80 91       	nx   	                	ls   	ax1.l, ac1.m
036B 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
036C 80 80       	nx   	                	ls   	ax0.l, ac0.m
036D 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
036E 80 91       	nx   	                	ls   	ax1.l, ac1.m
036F 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
0370 80 80       	nx   	                	ls   	ax0.l, ac0.m
0371 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
0372 80 91       	nx   	                	ls   	ax1.l, ac1.m
0373 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
0374 80 80       	nx   	                	ls   	ax0.l, ac0.m
0375 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
0376 80 91       	nx   	                	ls   	ax1.l, ac1.m
0377 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
0378 80 80       	nx   	                	ls   	ax0.l, ac0.m
0379 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
037A 80 91       	nx   	                	ls   	ax1.l, ac1.m
037B 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
037C 80 80       	nx   	                	ls   	ax0.l, ac0.m
037D 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
037E 80 91       	nx   	                	ls   	ax1.l, ac1.m
037F 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
0380 80 80       	nx   	                	ls   	ax0.l, ac0.m
0381 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
0382 80 91       	nx   	                	ls   	ax1.l, ac1.m
0383 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
0384 80 80       	nx   	                	ls   	ax0.l, ac0.m
0385 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
0386 80 91       	nx   	                	ls   	ax1.l, ac1.m
0387 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
0388 80 80       	nx   	                	ls   	ax0.l, ac0.m
0389 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
038A 80 91       	nx   	                	ls   	ax1.l, ac1.m
038B 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
038C 80 80       	nx   	                	ls   	ax0.l, ac0.m
038D 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
038E 80 91       	nx   	                	ls   	ax1.l, ac1.m
038F 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
0390 80 80       	nx   	                	ls   	ax0.l, ac0.m
0391 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
0392 80 91       	nx   	                	ls   	ax1.l, ac1.m
0393 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
0394 80 80       	nx   	                	ls   	ax0.l, ac0.m
0395 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
0396 80 91       	nx   	                	ls   	ax1.l, ac1.m
0397 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
0398 80 80       	nx   	                	ls   	ax0.l, ac0.m
0399 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
039A 80 91       	nx   	                	ls   	ax1.l, ac1.m
039B 9E 00       	mulmv	ax1.l, ax1.h, ac0
039C 6F 33       	movp 	ac1             	s    	@ar3, ac0.m
039D 1B 7F       	srri 	@ar3, ac1.m
039E 02 9F 04 02 	j    	$0x0402

{
	MulMode(false); 	// Multiply result by 2
	Mode40(true); 		// Special processing for acX.m registers enabled
	ar0 = Temp; 		// #0xE48  - Volume envelope buffer
	ar1 = *(uint16_t)(0xE43); 		// Next 32 samples after SRC  (Usually initialized by #0xCE0, incremented by 32 after each SRC step)
	ar3 = ar1;

	for (int i=0; i<32; i++)
	{
		ax0l = *ar0++;
		ax0h = *ar1++;
		Prod = ax0l * ax0h * 2; 	// *2 comes from SR.AM bit
		ac0 = Prod;
		*ar3++ = Saturate(ac0m);	// Controlled by SR.XM bit
	}
}


*/

#include "pch.h"
#include "CppUnitTest.h"
#include <intrin.h>
#include <algorithm>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace DspUnitTest
{
	TEST_CLASS(DspUnitTest)
	{
	public:

		TEST_METHOD(TestAxUcodeVolumeEnvelope)
		{
			uint16_t volumeEnvelope[32];
			uint16_t samples[32];

			for (int i = 0; i < 32; i++)
			{
				volumeEnvelope[i] = 0x7fff;
			}

			for (int i = 0; i < 32; i++)
			{

				if (i == 0)
					samples[i] = 0xffff;
				else if (i == 1)
					samples[i] = 2;
				else
					samples[i] = ((int64_t)rand() * __rdtsc()) & 0xffff;
			}

			for (int i = 0; i < 32; i++)
			{
				char text[0x100];
				int64_t prod = Muls (samples[i], volumeEnvelope[i], true);

				uint16_t saturated = Saturate(prod);

				sprintf_s(text, sizeof(text), "sample[%i] = 0x%04X, prod: 0x%llX, saturated: 0x%04X\n", i, samples[i], prod, saturated);
				Logger::WriteMessage(text);
			}

		}

		int64_t Muls(int16_t a, int16_t b, bool scale)
		{
			int64_t bitsPacked = (int64_t)((int64_t)(int32_t)a * (int64_t)(int32_t)b);
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
