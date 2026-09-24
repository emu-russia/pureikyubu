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

		// Save state
		//
		// This is the DSP-side AI DMA (the audio DMA that lives in the DSP block) and the CDCR word.
		// CDCR is the one register that carries state for the whole DSP block (the DSP->CPU
		// interrupt cause and its mask, the ARAM and AI causes, the reset-vector select and the
		// request to the core's interrupt gate), and the Flipper rebuilds its low three bits from
		// the DSP block on every read (see read_cdcr), so writing it here is writing the whole word
		// the guest sees rather than a shadow of it.
		//
		// The rest of the block is the running transfer: the address and length registers, the block
		// counter, the current address the mixer is reading from and the tick at which the next
		// block is due. `dmaTime` is `(uint64_t)-1` when no transfer is in progress (AIStopDMA parks
		// it there), so it is carried as the unsigned value it is and the comparison in AITickSync
		// reads the same sentinel after a load. The transfer is driven off the Gekko time base, so a
		// state has to carry where it was in the stream; put back, the DMA resumes where it was
		// interrupted.
		//
		// `zeroes` goes with the transfer because it is what the DMA hands to the mixer when the
		// guest has no buffer left to play: the block is a constant of the machine, and writing it
		// out keeps a state self-contained instead of depending on the initialisation path.
		// `one_second` (a host constant of the tick rate) and `log` (a host switch) stay out.
		//
		// The pair is defined here rather than in dspai.cpp, unlike every other block of the
		// machine: the unit test project compiles the DSP core without this file (it doubles the
		// block and its host-side functions, which reach for the audio mixer), and the pair is pure
		// serialization with no host dependency of its own - so it travels with the declaration and
		// both builds see the same one.

		/// <summary>Write the AI-DMA registers and the running transfer.</summary>
		void SaveState(SaveStates::StateWriter& writer) const
		{
			writer.Fields(cdcr, madr_hi, madr_lo, len, dcnt);
			writer.Fields(currentDmaAddr, dmaRate, dmaTime);
			writer.Array(zeroes);
		}

		/// <summary>Read them back.</summary>
		void LoadState(SaveStates::StateReader& reader)
		{
			reader.Fields(cdcr, madr_hi, madr_lo, len, dcnt);
			reader.Fields(currentDmaAddr, dmaRate, dmaTime);
			reader.Array(zeroes);
		}

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