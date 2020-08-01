
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
		DSP::Dsp16* DSP = nullptr;      // instance of dsp core
		AudioMixer* Mixer = nullptr;

		Flipper(HWConfig* config);
		~Flipper();

		void Update();
	};

	extern Flipper* HW;
}
