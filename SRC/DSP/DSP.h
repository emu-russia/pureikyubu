// GAMECUBE DSP Interface.
// In the previous version, the DSPcore implementation was mixed with the hardware binding (IFX) implementation. In this version, these entities are separated.

#pragma once

namespace DSP
{
	typedef uint32_t DspAddress;		// in halfwords slots 
}

#include "DspDma.h"
#include "DspAccel.h"
#include "DspAdpcm.h"
#include "DspStack.h"
#include "DspAlu.h"
#include "DspCore.h"
#include "DspAnalyzer.h"
#include "DspInterpreter.h"

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

	class Dsp16
	{
		friend DspCore;
		friend DspInterpreter;

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
