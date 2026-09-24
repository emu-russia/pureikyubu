#pragma once

// TODO: Drag the AID+AIS mixer from the audio backend (audiosdl.cpp) here and make the backend play a simple buffer. This way it will be more similar to real HW.


// AI Streaming registers

// 32-bit access usually
#define AIS_CR              0x00		// AI control register 
#define AIS_VR              0x04		// AI volume register
#define AIS_SCNT            0x08		// AI sample counter
#define AIS_IT              0x0C		// AI interrupt timing
#define AIS_REG_MAX			0x10

// Audio Interface Control Register mask
#define AICR_DFR            (1 << 6)        // AI DSP DMA sample rate (HW2 only). 0 - 48000, 1 - 32000
#define AICR_SCRESET        (1 << 5)        // reset sample counter
#define AICR_AIINTVLD       (1 << 4)        // This bit controls whether AIINT is affected by the AIIT register matching (0 - match affects, 1 - not)
#define AICR_AIINT          (1 << 3)        // AI Streaming interrupt status
#define AICR_AIINTMSK       (1 << 2)        // AI Streaming interrupt mask
#define AICR_AFR            (1 << 1)        // AI Streaming sample rate. 0 - 32000, 1 - 48000
#define AICR_PSTAT          (1 << 0)        // This bit enables the DDU AISLR clock

// ---------------------------------------------------------------------------
// hardware API

namespace Flipper
{
	class StateWriter;
	class StateReader;

	// AI state (registers and other data)
	struct AIState
	{
		// AI Streaming
		volatile uint32_t    cr;             // AI Streaming control reg
		volatile uint32_t    vr;             // AI Streaming volume
		volatile uint32_t    scnt;           // Streaming sample counter
		volatile uint32_t    it;             // Streaming sample counter trigger

		uint8_t     streamFifo[32];
		size_t      streamFifoPtr;

		bool        log;            // Enable AI Streaming log
	};

	class AudioInterface
	{
		AIState ai{};		//!< AI state (registers and other data)

		void MixerSetDvdAudioSampleRate(AudioSampleRate rate);
		void AISINT();
		void AIControl();
		static void AIReadReg(uint32_t addr, uint32_t* reg, void* ctx);
		static void AIWriteReg(uint32_t addr, uint32_t data, void* ctx);
		uint16_t AdjustVolume(uint16_t sampleValue, int volume);
		static void AIStreamCallback(uint16_t l, uint16_t r, void* ctx);

	public:
		AudioInterface(Flipper *flipper, HWConfig* config);
		~AudioInterface();

		// -- save states -------------------------------------------------------------------

		/// <summary>
		/// Write the audio interface into the save state section: the four streaming registers
		/// and the 32-byte FIFO the DVD audio decoder fills, with the position inside it. The
		/// FIFO is fed and drained outside the guest's register writes (the decoder callback
		/// appends a sample pair every time the drive decodes one), so the half-filled buffer
		/// is state the guest cannot rebuild by writing a register.
		/// </summary>
		void SaveState(SaveStates::StateWriter& writer) const;

		/// <summary>
		/// Read it back and re-derive what the control register implies for the machinery around
		/// this block. The four registers are decoded state and go back as they are; the stream
		/// clock, the mixer's DVD audio channel and the two sample rates are not registers, so
		/// the load ends with AIControl() - the same refresh a write to the control register
		/// does - after taking the restored FIFO position and sample counter out of its way,
		/// because that refresh restarts the streaming state from the beginning rather than
		/// resuming it. Nothing has to be called after the load.
		/// </summary>
		void LoadState(SaveStates::StateReader& reader);

		//! The AI state, for the debug interface (`airegs` and the debugui2 "Audio" panel).
		const AIState& State() const { return ai; }
	};
}