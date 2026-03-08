#pragma once

// TODO: Drag the AID+AIS mixer from audio.cpp here and make audio.cpp play a simple buffer. This way it will be more similar to real HW.


// AI Streaming registers

// 32-bit access
#define AIS_CR              0x0C006C00      // AIS control register 
#define AIS_VR              0x0C006C04      // AIS volume register
#define AIS_SCNT            0x0C006C08      // AIS sample counter
#define AIS_IT              0x0C006C0C      // AIS interrupt timing
#define AIS_UNUSED_4        0x0C006C10
#define AIS_UNUSED_5        0x0C006C14
#define AIS_UNUSED_6        0x0C006C18
#define AIS_UNUSED_7        0x0C006C1C

// Audio Interface Control Register mask
#define AICR_DFR            (1 << 6)        // AID sample rate (HW2 only). 0 - 48000, 1 - 32000
#define AICR_SCRESET        (1 << 5)        // reset sample counter
#define AICR_AIINTVLD       (1 << 4)        // This bit controls whether AIINT is affected by the AIIT register matching (0 - match affects, 1 - not)
#define AICR_AIINT          (1 << 3)        // AIS interrupt status
#define AICR_AIINTMSK       (1 << 2)        // AIS interrupt mask
#define AICR_AFR            (1 << 1)        // AIS sample rate. 0 - 32000, 1 - 48000
#define AICR_PSTAT          (1 << 0)        // This bit enables the DDU AISLR clock

// ---------------------------------------------------------------------------
// hardware API

namespace Flipper
{
	class AudioMixer;

	// AI state (registers and other data)
	struct AIControl
	{
		// AIS
		volatile uint32_t    cr;             // AIS control reg
		volatile uint32_t    vr;             // AIS volume
		volatile uint32_t    scnt;           // sample counter
		volatile uint32_t    it;             // sample counter trigger

		uint8_t     streamFifo[32];
		size_t      streamFifoPtr;

		bool        log;            // Enable AI log

		AudioMixer* Mixer = nullptr;
	};

	extern  AIControl ai;

	void AIOpen(HWConfig* config);
	void AIClose();
}