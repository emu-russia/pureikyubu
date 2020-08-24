
#pragma once

namespace Flipper
{
	// Global class for driving Flipper ASIC.

	class Flipper
	{
		static void HwUpdateThread(void* Parameter);

		int64_t hwUpdateTbrValue = 0;

		Thread* hwUpdateThread = nullptr;
		static const size_t ticksToHwUpdate = 100;

	public:
		Flipper(HWConfig* config);
		~Flipper();

		void Update();
	};

	extern Flipper* HW;

	// TODO: I do not like these lonely definitions, which, moreover, have to be created far away in the emulation module (Emulator.cpp).
	// Need to make one single class for the Flipper ASIC and move them there.
	extern DSP::Dsp16* DSP;
	extern GX::GXCore* Gx;
}
