#pragma once

// TODO: This module implements AI DMA, which is located inside the DSP, but also contains hooks for the DSP Mailbox registers. It needs to be made prettier.

namespace DSP
{
	struct DspAIControl
	{
		// AI DMA regs
		volatile uint16_t cdcr;			// AI/DSP control register
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

		/// <summary>
		/// Put the AI DMA state back to its power-on value. This exists because the block used to be
		/// cleared with a plain memset, which also wiped the Event above (a wait on a zeroed handle
		/// returns immediately, so the AI thread silently went back to spinning).
		/// </summary>
		void Reset();
	};

	extern  DspAIControl dsp_ai;

	void    DspAIOpen(Flipper::Flipper *flipper, HWConfig* config);
	void    DspAIClose();

	// Used by DspCore

	/// <summary>
	/// Re-evaluate the aggregate Processor Interface line (PI_INTERRUPT_DSP) from the three internal
	/// CDCR causes and their masks. Must be called whenever a cause or a mask changes - raising a
	/// cause, or the guest acknowledging one by writing CDCR.
	/// </summary>
	void    DSPUpdateInt();

	void    DSPAssertInt();
	bool    DSPGetInterruptStatus();
	bool    DSPGetResetModifier();

	void	DspSetAiDmaSampleRate(int32_t rate);

	/// <summary>
	/// Called by the CPU thread (through Flipper::Update) every Flipper tick step, so that the AI
	/// thread wakes up when the next DMA block is due.
	/// </summary>
	void	AITickSync(int64_t ticks);
}