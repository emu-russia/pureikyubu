
#pragma once

namespace Flipper
{
	class AudioMixer;

	class Flipper
	{
		static void HwUpdateThread(void* Parameter);

		int64_t hwUpdateTbrValue = 0;

		Thread* hwUpdateThread = nullptr;
		static const size_t ticksToHwUpdate = 100;

	public:
		AudioMixer* Mixer = nullptr;

		Flipper(HWConfig* config);
		~Flipper();

		void Update();
	};

	extern Flipper* HW;
	extern DSP::Dsp16* DSP;
}
