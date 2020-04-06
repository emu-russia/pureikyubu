
#pragma once

#include "../Common/Thread.h"

// hardware registers base (physical address)
#define HW_BASE         0x0C000000

namespace Flipper
{
	class AudioMixer;

	class Flipper
	{
		static void HwUpdateThread(void* Parameter);

		int64_t hwUpdateTbrValue = 0;

		Thread* hwUpdateThread = nullptr;

	public:
		DSP::DspCore* DSP;      // instance of dsp core
		AudioMixer* Mixer = nullptr;

		Flipper(HWConfig* config);
		~Flipper();

		void Update();
	};

	extern Flipper* HW;
}
