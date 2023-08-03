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

The debugging interface specification provided by this component can be found in Data/Json/DspJdi.json.

*/

#pragma once

#define ARAMSIZE        (16 * 1024 * 1024)  // 16 mb
#define ARAM            aram.mem

// aram dma transfer type (CNT bit31)
#define RAM_TO_ARAM     0
#define ARAM_TO_RAM     1

// known ARAM controller registers
#define AR_SIZE             0x0C005012      // 16 bit regs
#define AR_MODE             0x0C005016
#define AR_REFRESH          0x0C00501A
#define AR_DMA_MMADDR_H     0x0C005020
#define AR_DMA_MMADDR_L     0x0C005022
#define AR_DMA_ARADDR_H     0x0C005024
#define AR_DMA_ARADDR_L     0x0C005026
#define AR_DMA_CNT_H        0x0C005028
#define AR_DMA_CNT_L        0x0C00502A

#define AR_DMA_MMADDR       0x0C005020      // 32 bit regs
#define AR_DMA_ARADDR       0x0C005024
#define AR_DMA_CNT          0x0C005028

// not sure about AR_SIZE, AR_MODE and AR_REFRESH.

// ARAM state (registers and other data)
struct ARControl
{
    uint8_t* mem;                // aux. memory buffer (size is ARAMSIZE)
    volatile uint32_t    mmaddr, araddr;     // DMA address
    volatile uint32_t    cnt;                // count + transfer type (bit31)
    uint16_t    size;               // "AR_SIZE" (0x5012) register
    Thread* dmaThread;
    int64_t gekkoTicks;
    size_t gekkoTicksPerSlice;
    bool dspRunningBeforeAramDma;
    bool log;
};

void    AROpen();
void    ARClose();

extern  ARControl aram;


// GAMECUBE DSP Interface.
// In the previous version, the DSPcore implementation was mixed with the hardware binding (IFX) implementation. In this version, these entities are separated.

namespace DSP
{
	union DspAccelAddress
	{
		struct
		{
			uint16_t l;
			uint16_t h;
		};
		uint32_t addr;
	};

	struct DspAccel
	{
		uint16_t Fmt;					// Sample format
		uint16_t AdpcmCoef[16];
		uint16_t AdpcmPds;				// predictor / scale combination
		uint16_t AdpcmYn1;				// y[n - 1]
		uint16_t AdpcmYn2;				// y[n - 2]
		uint16_t AdpcmGan;				// gain to be applied
		DspAccelAddress StartAddress;
		DspAccelAddress EndAddress;
		DspAccelAddress CurrAddress;
	};

}


namespace DSP
{

	// Accelerator sample format

	enum class AccelFormat
	{
		RawByte = 0x0005,		// Seen in IROM
		RawUInt16 = 0x0006,		// 
		Pcm16 = 0x000A,			// Signed 16 bit PCM mono
		Pcm8 = 0x0019,			// Signed 8 bit PCM mono
		Adpcm = 0x0000,			// ADPCM encoded (both standard & extended)
	};

}


// DSP DMA registers

namespace DSP
{

	struct DspDmaRegs
	{
		union
		{
			struct
			{
				uint16_t	l;
				uint16_t	h;
			};
			uint32_t	bits;
		} mmemAddr;
		DspAddress  dspAddr;
		uint16_t	blockSize;
		union
		{
			struct
			{
				unsigned Dsp2Mmem : 1;		// 0: MMEM -> DSP, 1: DSP -> MMEM
				unsigned Imem : 1;			// 0: DMEM, 1: IMEM
			};
			uint16_t	bits;
		} control;
	};

}


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

		ACDAT2 = 0xFFD3,	// RAW accelerator data (R/W)
		ACSAH = 0xFFD4,		// Accelerator start address H 
		ACSAL = 0xFFD5,		// Accelerator start address L 
		ACEAH = 0xFFD6,		// Accelerator end address H 
		ACEAL = 0xFFD7,		// Accelerator end address L 
		ACCAH = 0xFFD8,		// Accelerator current address H  +  Acc Direction
		ACCAL = 0xFFD9,		// Accelerator current address L 
		AMDM = 0xFFEF,		// ARAM DMA Request Mask
		ACFMT = 0xFFD1,			// sample format used
		ACPDS = 0xFFDA,			// predictor / scale combination
		ACYN1 = 0xFFDB,			// y[n - 1]
		ACYN2 = 0xFFDC,			// y[n - 2]
		ACDAT = 0xFFDD,		// Decoded Adpcm data (Read)  y[n]  (Read only)
		ACGAN = 0xFFDE,			// gain to be applied (PCM mode only)
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

		static const size_t IRAM_SIZE = (8 * 1024);
		static const size_t IROM_SIZE = (8 * 1024);
		static const size_t DRAM_SIZE = (8 * 1024);
		static const size_t DROM_SIZE = (4 * 1024);

		static const size_t IROM_START_ADDRESS = 0x8000;
		static const size_t DROM_START_ADDRESS = 0x1000;
		static const size_t IFX_START_ADDRESS = 0xFF00;		// Internal dsp "hardware"

		uint8_t iram[IRAM_SIZE] = { 0 };
		uint8_t irom[IROM_SIZE] = { 0 };
		uint8_t dram[DRAM_SIZE] = { 0 };
		uint8_t drom[DROM_SIZE] = { 0 };

		Thread* dspThread = nullptr;
		static void DspThreadProc(void* Parameter);
		uint64_t savedGekkoTicks = 0;

		DspDmaRegs DmaRegs;

		DspAccel Accel;

		volatile uint16_t DspToCpuMailbox[2];		// DMBH, DMBL
		SpinLock DspToCpuLock[2];

		volatile uint16_t CpuToDspMailbox[2];		// CMBH, CMBL
		SpinLock CpuToDspLock[2];

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

		bool LoadIrom(std::vector<uint8_t>& iromImage);
		bool LoadDrom(std::vector<uint8_t>& dromImage);

		void Run();
		bool IsRunning() { return dspThread->IsRunning(); }
		void Suspend();

		// Memory engine

		uint8_t* TranslateIMem(DspAddress addr);
		uint8_t* TranslateDMem(DspAddress addr);
		uint16_t ReadIMem(DspAddress addr);
		uint16_t ReadDMem(DspAddress addr);
		void WriteDMem(DspAddress addr, uint16_t value);

		// Debug

		void DumpIfx();

#pragma region "Flipper interface"

		// AIDCR bits
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
