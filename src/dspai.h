#pragma once

// TODO: This module implements AI DMA, which is located inside the DSP, but also contains hooks for the DSP Mailbox registers. It needs to be made prettier.

namespace DSP
{
	struct DspAIControl
	{
		// AID
		volatile uint16_t dcr;			// AI/DSP control register
		volatile uint16_t madr_hi;		// DMA start address hi
		volatile uint16_t madr_lo;		// DMA start address lo
		volatile uint16_t len;			// DMA control/DMA length (length of audio data)
		volatile uint16_t dcnt;			// DMA count-down

		// helpers
		uint32_t    currentDmaAddr; // current DMA address
		int32_t     dmaRate;        // copy of DFR value (32000/48000)
		uint64_t    dmaTime;        // audio DMA update time 

		uint8_t     zeroes[32];

		int64_t     one_second;     // one CPU second in timer ticks
		bool        log;            // Enable AI DMA log

		Thread* audioThread;    // The main AI thread that receives samples from AI DMA FIFO.
		// When FIFOs overflow - AudioThread Feed Mixer.
	};

	extern  DspAIControl dsp_ai;

	void    DspAIOpen(HWConfig* config);
	void    DspAIClose();

	// Used by DspCore

	void    DSPAssertInt();
	bool    DSPGetInterruptStatus();
	bool    DSPGetResetModifier();

	void	DspSetAiDmaSampleRate(int32_t rate);
}