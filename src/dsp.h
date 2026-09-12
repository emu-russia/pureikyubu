/*

# GAMECUBE DSP

Low-level DSP emulation module.

Includes both custom Macronix 16-bit DSPcore emulation and emulation of external DSPcore hardware "wrapping" (ARAM Accelerator and ADPCM decoder, Mailbox, DMA).

## DspCore

How does the DSP core work.

The Run method executes the Update method until it is stopped by the Suspend method or it encounters a breakpoint.

Suspend method stops DSP thread execution indifinitely.

The Step debugging method is used to unconditionally execute next DSP instruction (by interpreter).

The Update method checks the value of Gekko TBR. If its value has exceeded the limit for the execution of one DSP instruction (or segment in case of Jitc),
interpreter/Jitc Execute method is called.

DspCore uses the interpreter and recompiler at the same time, of their own free will, depending on the situation.

## DSP Decoder

This component analyzes the DSP instructions and is used by all interested systems (disassembler, interpreter and recompiler).
That is, in fact, this is a universal decoder.

I decided to divide all the DSP instructions into groups (by higher 4 bits). Decoder implemented as simple if-else.

Parallel instructions are stored into two different groups in `DecoderInfo` struct.

The simplest example of `DecoderInfo` consumption can be found in the disassembler.

The DSP instruction format is so tightly packed and has a lot of entropy, so I could make a mistake in decoding somewhere. All this then has to appear.

## GameCube DSP interpreter

The development idea is as follows - to do at least something (critical mass of code), then do some reverse engineering
of the microcodes and IROM and bring the emulation to an adequate state.

### Interpreter architecture

The interpreter is not involved in instruction decoding. It receives ready-made information from the decoder (`DecoderInfo` struct).

This is a new concept of emulation of processor systems, which I decided to try on the GameCube DSP.

(This concept was later named `UVNA` - Universal von Neumann Approach)

## Mailbox Sync

Specifics of Mailbox registers (registers consist of two halves) impose some features on their emulation.

When accessed, Dead Lock may occur when the processor hangs on the polling DSP Mailbox, and the DSP hangs on polling the CPU Mailbox.
This happens due to the almost simultaneous writing to both Mailbox from two "ends".

## DSP JDI

The debugging interface specification provided by this component can be found in jdispecs.cpp (JdiSpecs::DspJdi).

*/

#pragma once

// DSP<->PI registers

// 16-bit access
#define DSP_OUTMBOXH        0x00      // CPU->DSP mailbox
#define DSP_OUTMBOXL        0x02
#define DSP_INMBOXH         0x04      // DSP->CPU mailbox
#define DSP_INMBOXL         0x06
#define CDCR_OFF            0x0A      // CPU <-> DSP control register (CDCR)
// ARAM (auxiliary SDRAM) controller registers. Names follow the hardware documentation
// (see dsp.md 5.4 / 6.4 and aram.md 5); the OS AR driver uses its own aliases, which are
// mentioned in the comments where they differ.
#define AMCR                0x12      // ARAM memory configuration (driver: "AR_SIZE"; 0x43 = 16 MB internal, no expansion)
#define AMNF                0x16      // ARAM normal-state flag: set when the SDRAM controller is initialised and ready
#define AMCT                0x1A      // SDRAM refresh period / controller control
#define AMMAH               0x20      // ARAM-DMA main memory address, high word (bits 25:16)
#define AMMAL               0x22      // ARAM-DMA main memory address, low word (bits 15:5)
#define AMAAH               0x24      // ARAM-DMA ARAM address, high word (bits 25:16)
#define AMAAL               0x26      // ARAM-DMA ARAM address, low word (bits 15:5)
#define AMBLH               0x28      // ARAM-DMA block length, high word (bit 15 = direction: 0 RAM->ARAM, 1 ARAM->RAM)
#define AMBLL               0x2A      // ARAM-DMA block length, low word (a write to it starts the transfer)
// AI DMA
#define AID_MADRH           0x30      // DMA start address (High)
#define AID_MADRL           0x32      // DMA start address (Low)
#define AID_LEN             0x36      // DMA control/DMA length (length of audio data in 32 Byte blocks)
#define AID_CNT             0x3A      // counts down to zero showing how many 32 Byte blocks are left

// aram dma transfer type (CNT bit31)
#define RAM_TO_ARAM     0
#define ARAM_TO_RAM     1

// enable bit in AIDLEN register
#define AID_EN              (1 << 15)

// CDCR mask
#define CDCR_RESETMOD       (1 << 11)       // 1: DSP Reset from 0x8000, 0: DSP Reset from 0x0000 (__OSInitAudioSystem)
#define CDCR_DSPDMA        (1 << 10)       // DSP dma in progress
#define CDCR_ARDMA         (1 << 9)        // ARAM dma in progress
#define CDCR_DSPINTMSK     (1 << 8)        // DSP->CPU interrupt mask (ReadWrite)
#define CDCR_DSPINT        (1 << 7)        // DSP->CPU interrupt status (ReadWrite-Clear)
#define CDCR_ARINTMSK      (1 << 6)        // ARAM DMA interrupt mask (RW)
#define CDCR_ARINT         (1 << 5)        // ARAM DMA interrupt status (RWC)
#define CDCR_AIINTMSK      (1 << 4)        // AI DMA interrupt mask (RW)
#define CDCR_AIINT         (1 << 3)        // AI DMA interrupt status (RWC)
#define CDCR_HALT          (1 << 2)        // halt DSP (stop ucoding)
#define CDCR_DINT          (1 << 1)        // CPU->DSP interrupt
#define CDCR_RES           (1 << 0)        // reset DSP (waits for 0)

#define CDCR               dsp_ai.cdcr

// GAMECUBE DSP Interface.
// In the previous version, the DSPcore implementation was mixed with the hardware binding (IFX) implementation. In this version, these entities are separated.

namespace DSP
{
	// DSP hardware registers mapping (DMEM addresses)

	enum class DspHardwareRegs
	{
		CMBH = 0xFFFE,		// CPU->DSP Mailbox H 
		CMBL = 0xFFFF,		// CPU->DSP Mailbox L 
		DMBH = 0xFFFC,		// DSP->CPU Mailbox H 
		DMBL = 0xFFFD,		// DSP->CPU Mailbox L 

		DSMAH = 0xFFCE,		// Memory address H 
		DSMAL = 0xFFCF,		// Memory address L 
		DSPA = 0xFFCD,		// DSP memory address 
		DSCR = 0xFFC9,		// DMA control 
		DSBL = 0xFFCB,		// Block size 

		ACDL = 0xFFD3,			// Accelerator data lines port (read in a read window, write in a write window)
		ACSAH = 0xFFD4,			// Accelerator start address H
		ACSAL = 0xFFD5,			// Accelerator start address L
		ACEAH = 0xFFD6,			// Accelerator end address H
		ACEAL = 0xFFD7,			// Accelerator end address L
		ACCAH = 0xFFD8,			// Accelerator current address H + accelerator direction (bit 15)
		ACCAL = 0xFFD9,			// Accelerator current address L
		AMDM = 0xFFEF,			// ARAM-DMA request mask (bit 0: 1 = ARAM dedicated to the accelerator)
		ADM = 0xFFD1,			// Audio/decoder mode: 5:4 y(n) format, 3:2 decoder mode, 1:0 read addressing mode
		ACPDS = 0xFFDA,			// PS: predictor / scale (deADPCM)
		ACYN1 = 0xFFDB,			// y[n - 1]
		ACYN2 = 0xFFDC,			// y[n - 2]
		ACYN = 0xFFDD,			// y[n] - decoder output (read only)
		ACGAN = 0xFFDE,			// GAIN: 16-bit gain (General IIR / PCM IIR)
		ACXN = 0xFFDF,			// x[n] - decoder input sample (General IIR)
		// ADPCM coef table. Coefficient selected by Adpcm Predictor
		ADPCM_A00 = 0xFFA0,		// Coef * Yn1[0]
		ADPCM_A10 = 0xFFA1,		// Coef * Yn2[0]
		ADPCM_A20 = 0xFFA2,		// Coef * Yn1[1]
		ADPCM_A30 = 0xFFA3,		// Coef * Yn2[1]
		ADPCM_A40 = 0xFFA4,		// Coef * Yn1[2]
		ADPCM_A50 = 0xFFA5,		// Coef * Yn2[2]
		ADPCM_A60 = 0xFFA6,		// Coef * Yn1[3]
		ADPCM_A70 = 0xFFA7,		// Coef * Yn2[3]
		ADPCM_A01 = 0xFFA8,		// Coef * Yn1[4]
		ADPCM_A11 = 0xFFA9,		// Coef * Yn2[4]
		ADPCM_A21 = 0xFFAA,		// Coef * Yn1[5]
		ADPCM_A31 = 0xFFAB,		// Coef * Yn2[5]
		ADPCM_A41 = 0xFFAC,		// Coef * Yn1[6]
		ADPCM_A51 = 0xFFAD,		// Coef * Yn2[6]
		ADPCM_A61 = 0xFFAE,		// Coef * Yn1[7]
		ADPCM_A71 = 0xFFAF,		// Coef * Yn2[7]
		// Unknown
		UNKNOWN_FFB0 = 0xFFB0,
		UNKNOWN_FFB1 = 0xFFB1,

		DIRQ = 0xFFFB,		// IRQ request
	};

	/// <summary>
	/// GAMECUBE DSP Interface.
	/// </summary>
	class Dsp16
	{
		friend DspCore;
		friend DspInterpreter;
		friend DspUnitTest::DspUnitTest;

		Thread* dspThread = nullptr;
		static void DspThreadProc(void* Parameter);
		uint64_t savedGekkoTicks = 0;

		DspDmaRegs DmaRegs{};

		DspAccel Accel{};

		// Each mailbox is a 32-bit message made of two 16-bit halves that are written and
		// read one at a time. A single lock per mailbox serialises the whole pair, and the
		// low word is snapshotted together with the high word so that a receiver which
		// reads the high word and then the low word can never pick up a mixture of two
		// messages (the sender may post the next message between those two reads).

		volatile uint16_t DspToCpuMailbox[2]{};		// DMBH, DMBL
		SpinLock DspToCpuLock;
		volatile uint16_t DspToCpuSnapshot = 0;		// DMBL latched with the last valid DMBH read
		volatile bool DspToCpuSnapshotValid = false;

		//! The CPU->DSP interrupt request (CDCR bit 1), latched until the core takes it.
		bool intdspRequested = false;

		volatile uint16_t CpuToDspMailbox[2]{};		// CMBH, CMBL
		SpinLock CpuToDspLock;
		volatile uint16_t CpuToDspSnapshot = 0;		// CMBL latched with the last valid CMBH read
		volatile bool CpuToDspSnapshotValid = false;

		void ResetIfx();
		void DoDma();
		uint16_t AccelReadData(bool raw);
		uint16_t AccelFetch();
		void AccelWriteData(uint16_t data);
		uint16_t DecodeAdpcm(uint16_t nibble);

		// Logging control
		bool logMailbox = false;
		bool logInsaneMailbox = false;
		bool logDspControlBits = false;
		bool logDspInterrupts = false;
		bool logNonconditionalCallJmp = false;
		bool logDspDma = false;
		bool logAccel = false;
		bool logAdpcm = false;
		bool dumpUcode = false;

		bool haltOnUnmappedMemAccess = false;

	public:

		DspCore* core = nullptr;

		Dsp16();
		~Dsp16();

		void Run();
		bool IsRunning() { return dspThread->IsRunning(); }
		void Suspend();

		// Memory engine

		uint16_t ReadDMem(DspAddress addr);
		void WriteDMem(DspAddress addr, uint16_t value);

		// Debug

		void DumpIfx();

#pragma region "Flipper interface"

		// CDCR bits
		void SetResetBit(bool val);
		bool GetResetBit();
		void SetIntBit(bool val);
		bool GetIntBit();
		void SetHaltBit(bool val);
		bool GetHaltBit();

		// CPU->DSP Mailbox
		void CpuToDspWriteHi(uint16_t value);
		void CpuToDspWriteLo(uint16_t value);
		uint16_t CpuToDspReadHi(bool ReadByDsp);
		uint16_t CpuToDspReadLo(bool ReadByDsp);

		//! The latched CPU->DSP interrupt request (see SetIntBit).
		bool CpuIntRequested() const;
		void ClearCpuIntRequest();

		// DSP->CPU Mailbox
		void DspToCpuWriteHi(uint16_t value);
		void DspToCpuWriteLo(uint16_t value);
		uint16_t DspToCpuReadHi(bool ReadByDsp);
		uint16_t DspToCpuReadLo(bool ReadByDsp);

		// ARAM DMA has a special mode for copying data to IRAM (used exclusively in OSInitAudioSystem)
		void SpecialAramImemDma(uint8_t* ptr, size_t byteCount);

#pragma endregion "Flipper interface"

	};

}