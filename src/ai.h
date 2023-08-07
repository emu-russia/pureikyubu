#pragma once

// TODO: Drag the AID+AIS mixer from audio.cpp here and make audio.cpp play a simple buffer. This way it will be more similar to real HW.
// TODO: Drag CDCR register handling into dsp.cpp to be close to real HW


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

// enable bit in AIDLEN register
#define AID_EN              (1 << 15)

// Audio Interface Control Register mask
#define AICR_DFR            (1 << 6)        // AID sample rate (HW2 only). 0 - 48000, 1 - 32000
#define AICR_SCRESET        (1 << 5)        // reset sample counter
#define AICR_AIINTVLD       (1 << 4)        // This bit controls whether AIINT is affected by the AIIT register matching (0 - match affects, 1 - not)
#define AICR_AIINT          (1 << 3)        // AIS interrupt status
#define AICR_AIINTMSK       (1 << 2)        // AIS interrupt mask
#define AICR_AFR            (1 << 1)        // AIS sample rate. 0 - 32000, 1 - 48000
#define AICR_PSTAT          (1 << 0)        // This bit enables the DDU AISLR clock

#define AIDCR               ai.dcr

// ---------------------------------------------------------------------------
// hardware API

namespace Flipper
{
	class AudioMixer;

	// AI state (registers and other data)
	struct AIControl
	{
		// AID
		volatile uint16_t dcr;			// AI/DSP control register
		volatile uint16_t madr_hi;		// DMA start address hi
		volatile uint16_t madr_lo;		// DMA start address lo
		volatile uint16_t len;			// DMA control/DMA length (length of audio data)
		volatile uint16_t dcnt;			// DMA count-down

		// AIS
		volatile uint32_t    cr;             // AIS control reg
		volatile uint32_t    vr;             // AIS volume
		volatile uint32_t    scnt;           // sample counter
		volatile uint32_t    it;             // sample counter trigger

		// helpers
		uint32_t    currentDmaAddr; // current DMA address
		int32_t     dmaRate;        // copy of DFR value (32000/48000)
		uint64_t    dmaTime;        // audio DMA update time 

		Thread* audioThread;    // The main AI thread that receives samples from AI DMA FIFO and DVD Audio (which accumulate in AIS FIFO).
		// When FIFOs overflow - AudioThread Feed Mixer.

		uint8_t     streamFifo[32];
		size_t      streamFifoPtr;

		int64_t     one_second;     // one CPU second in timer ticks
		bool        log;            // Enable AI log

		uint8_t     zeroes[32];

		AudioMixer* Mixer = nullptr;
	};

	extern  AIControl ai;

	void    AIOpen(HWConfig* config);
	void    AIClose();

	// Used by DspCore

	void    DSPAssertInt();
	bool    DSPGetInterruptStatus();
	bool    DSPGetResetModifier();
}
