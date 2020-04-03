
#pragma once

#include "../Common/Thread.h"

// hardware registers base (physical address)
#define HW_BASE         0x0C000000

extern	DSP::DspCore* dspCore;      // instance of dsp core

namespace Flipper
{
	class Flipper
	{
		static void HwUpdateThread(void* Parameter);

		int64_t hwUpdateTbrValue = 0;

		Thread* hwUpdateThread = nullptr;

	public:
		AudioMixer* Mixer = nullptr;

		Flipper(HWConfig* config);
		~Flipper();

		void Update();
	};

	extern Flipper* HW;
}
