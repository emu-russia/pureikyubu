
#pragma once

// Config
struct HWConfig
{
    // MI
    size_t      ramsize;

    // VI
    void* renderTarget;
    bool        vi_log;
    bool        vi_xfb;
    int         videoEncoderFuse;       // 1 - PAL, 0 - NTSC

    // PI
    uint32_t    consoleVer;
    bool        rswhack;

    // EI
    bool        exi_log;
    bool        exi_osReport;
    wchar_t     ansiFilename[0x1000];
    wchar_t     sjisFilename[0x1000];

    // MC
    bool        MemcardA_Connected;
    bool        MemcardB_Connected;
    wchar_t     MemcardA_Filename[0x1000];
    wchar_t     MemcardB_Filename[0x1000];
    bool        Memcard_SyncSave;

    wchar_t     BootromFilename[0x1000];
    wchar_t     DspDromFilename[0x1000];
    wchar_t     DspIromFilename[0x1000];

};


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
