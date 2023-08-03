/*

# GCN Hardware

This component emulates everything inside the ASIC Flipper, *except* the graphics system (GX) and DSP.

![Flipper_Block_Diagram](https://github.com/ogamespec/dolwin-docs/blob/master/HW/Flipper_ASIC_Block_Diagram.png?raw=true)

A short tour into the Flipper stuff, without shocking details:
- AI: Audio DMA
- CP: Command Processor for GX FIFO
- EXI: SPI-like Macronix interface
- MI: Memory Interface
- PI: Processor Interface (interrupts, etc.)
- SI: Serial Interface (goes to GameCube controllers connectors)
- VI: Video output

Somehow also emulation of memory cards (Memcards) got here. For now, let it stay here, then we'll move it somewhere, if the emulation of more accessories and peripherals appears.

## Real revisions of Flipper

It is documented (in the Dolphin SDK) that there were at least 2 versions of Flipper hardware - `HW1` (an early debug version) and `HW2` (contained in the latest Devkits and Retail consoles).

## A small note on VI

GameCube uses a slightly alien image rendering engine:
- The scene is drawn by the GPU into the internal Flipper frame buffer (EFB)
- Then a special circuit (Copy Engine) copies this buffer to external memory on the fly converting RGB to YUV
- Buffer from external memory (XFB) is used by the video output circuit to directly send the picture to the TV

All this is wildly unoptimized in terms of emulation and is a lot of pain.

Therefore, emulator developers ignore the XFB output and display in the emulator what the GPU draws. With rare exceptions, this works in most games, but it does not work in Homebrew and the games of some perverse developers.

*/

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
}
