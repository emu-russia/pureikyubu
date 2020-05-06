// Low-level DSP core

#pragma once

#include <vector>
#include <map>
#include <string>
#include <atomic>
#include "../Common/Thread.h"

namespace DSP
{
	typedef uint32_t DspAddress;		// in halfwords slots 

	#pragma warning (push)
	#pragma warning (disable: 4201)

	#pragma pack (push, 1)

	typedef union _DspLongAccumulator
	{
		struct
		{
			uint16_t	l;
			union
			{
				struct
				{
					uint16_t	m;
					uint16_t	h;
				};
				uint32_t hm;
				int32_t shm;
			};
		};
		uint64_t	bits;
		int64_t		sbits;
	} DspLongAccumulator;

	typedef union _DspShortAccumulator
	{
		struct
		{
			uint16_t	l;
			uint16_t	h;
		};
		uint32_t	bits;
		int32_t		sbits;
	} DspShortAccumulator;

	typedef struct _DspProduct
	{
		struct
		{
			uint16_t l;
			uint16_t m1;
			uint16_t h;
			uint16_t m2;
		};
		uint64_t bitsPacked;
	} DspProduct;

	typedef union _DspStatus
	{
		struct
		{
			unsigned c : 1;			// Carry
			unsigned o : 1;			// Overﬂow 
			unsigned z : 1;			// Arithmetic zero 
			unsigned s : 1;		// Sign
			unsigned as : 1;	// Above s32 
			unsigned tt : 1;	// Top two bits are equal 
			unsigned ok : 1;	// 1: Bit test OK, 0: Bit test not OK
			unsigned os : 1;	// Overflow (sticky)
			unsigned hwz : 1;	// Hardwired to 0? 
			unsigned ie : 1;	// Interrupt enable 
			unsigned unk10 : 1;
			unsigned eie : 1;		// External interrupt enable 
			unsigned unk12 : 1;
			// Not actually status, but ALU control
			unsigned am : 1;		// Product multiply result by 2 (when AM = 0)  (0 = M2, 1 = M0)
			unsigned sxm : 1;	// Sign extension mode for loading in Middle regs (0 = clr40, 1 = set40) 
			unsigned su : 1;	// Operands are signed (1 = unsigned)
		};

		uint16_t bits;

	} DspStatus;

	#pragma pack (pop)

	enum class DspRegister
	{
		ar0 = 0,		// Addressing register 0 
		ar1,			// Addressing register 1 
		ar2,			// Addressing register 2 
		ar3,			// Addressing register 3 
		indexRegs,
		ix0 = indexRegs,	// Indexing register 0 
		ix1,			// Indexing register 1
		ix2,			// Indexing register 2
		ix3,			// Indexing register 3
		limitRegs,
		lm0 = limitRegs,
		lm1,
		lm2,
		lm3,
		stackRegs,
		st0 = stackRegs,	// Call stack register 
		st1,			// Data stack register 
		st2,			// Loop address stack register 
		st3,			// Loop counter register 
		ac0h,			// 40-bit Accumulator 0 (high) 
		ac1h,			// 40-bit Accumulator 1 (high) 
		bank,			// Bank register (LRS/SRS)
		sr,				// Status register 
		prodl,			// Product register (low) 
		prodm1,			// Product register (mid 1) 
		prodh,			// Product register (high) 
		prodm2,			// Product register (mid 2) 
		ax0l,			// 32-bit Accumulator 0 (low) 
		ax0h,			// 32-bit Accumulator 0 (high) 
		ax1l,			// 32-bit Accumulator 1 (low) 
		ax1h,			// 32-bit Accumulator 1 (high
		ac0l,			// 40-bit Accumulator 0 (low) 
		ac1l,			// 40-bit Accumulator 1 (low) 
		ac0m,			// 40-bit Accumulator 0 (mid)
		ac1m,			// 40-bit Accumulator 1 (mid)
	};

	typedef struct _DspRegs
	{
		uint32_t clearingPaddy;		// To use {0} on structure
		uint16_t ar[4];		// Addressing registers
		uint16_t ix[4];		// Indexing registers
		uint16_t lm[4];	// Limit registers
		std::vector<DspAddress> st[4];	// Stack registers
		DspLongAccumulator ac[2];		// 40-bit Accumulators
		DspShortAccumulator ax[2];		// 32-bit Accumulators
		DspProduct prod;		// Product register
		// https://github.com/dolphin-emu/dolphin/wiki/Zelda-Microcode#unknown-registers
		uint16_t bank;		// bank (lrs/srs)
		DspStatus sr;		// status
		DspAddress pc;		// Program counter
	} DspRegs;

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
		ACCAH = 0xFFD8,		// Accelerator current address H 
		ACCAL = 0xFFD9,		// Accelerator current address L 
		ACDAT = 0xFFDD,		// Decoded Accelerator data (Read)
		AMDM = 0xFFEF,		// ARAM DMA Request Mask
		// From https://github.com/devkitPro/gamecube-tools/blob/master/gdopcode/disassemble.cpp
		ACFMT = 0xFFD1,			// sample format used
		ACPDS = 0xFFDA,			// predictor / scale combination
		ACYN1 = 0xFFDB,			// y[n - 1]
		ACYN2 = 0xFFDC,			// y[n - 2]
		ACGAN = 0xFFDE,			// gain to be applied (0 for ADPCM, 0x0800 for PCM8/16)
		// ADPCM coef table
		ADPCM_A00 = 0xFFA0,
		ADPCM_A10 = 0xFFA1,
		ADPCM_A20 = 0xFFA2,
		ADPCM_A30 = 0xFFA3,
		ADPCM_A40 = 0xFFA4,
		ADPCM_A50 = 0xFFA5,
		ADPCM_A60 = 0xFFA6,
		ADPCM_A70 = 0xFFA7,
		ADPCM_A01 = 0xFFA8,
		ADPCM_A11 = 0xFFA9,
		ADPCM_A21 = 0xFFAA,
		ADPCM_A31 = 0xFFAB,
		ADPCM_A41 = 0xFFAC,
		ADPCM_A51 = 0xFFAD,
		ADPCM_A61 = 0xFFAE,
		ADPCM_A71 = 0xFFAF,
		// Unknown
		UNKNOWN_FFB0 = 0xFFB0,
		UNKNOWN_FFB1 = 0xFFB1,

		DIRQ = 0xFFFB,		// IRQ request
	};

	// Known DSP exceptions

	enum class DspException
	{
		RESET = 0,
		STOVF,			// Stack underflow/overflow
		Unknown2,
		Unknown3,
		Unknown4,
		ACCOV,			// Accelerator address overflow
		Unknown6,		// Trap?
		INT,			// External interrupt (from CPU)
	};

	// Accelerator sample format

	enum class AccelFormat
	{
		RawByte = 0x0005,		// Seen in IROM
		RawUInt16 = 0x0006,		// 
		Pcm16 = 0x000A,			// Signed 16 bit PCM mono
		Pcm8 = 0x0019,			// Signed 8 bit PCM mono
		Adpcm = 0x0000,			// ADPCM encoded (both standard & extended)
	};

	class DspInterpreter;

	class DspCore
	{
		std::vector<DspAddress> breakpoints;		// IMEM breakpoints
		SpinLock breakPointsSpinLock;

		std::map<DspAddress, std::string> canaries;		// When the PC is equal to the canary address, a debug message is displayed
		SpinLock canariesSpinLock;

		const uint32_t GekkoTicksPerDspInstruction = 100;		// How many Gekko ticks should pass so that we can execute one DSP instruction
		const uint32_t GekkoTicksPerDspSegment = 500;		// How many Gekko ticks should pass so that we can execute one DSP segment (in case of Jitc)

		uint64_t savedGekkoTicks = 0;

		Thread* dspThread = nullptr;
		static void DspThreadProc(void* Parameter);

		DspInterpreter* interp;

		uint16_t DspToCpuMailbox[2];		// DMBH, DMBL
		uint16_t DspToCpuMailboxShadow[2];
		SpinLock DspToCpuLock;

		uint16_t CpuToDspMailbox[2];		// CMBH, CMBL
		uint16_t CpuToDspMailboxShadow[2];
		SpinLock CpuToDspLock;

		bool haltOnUnmappedMemAccess = false;
		bool log = false;

		struct
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
		} DmaRegs;

		struct
		{
			uint16_t Fmt;					// Sample format
			uint16_t AdpcmCoef[16];			
			uint16_t AdpcmPds;				// predictor / scale combination
			uint16_t AdpcmYn1;				// y[n - 1]
			uint16_t AdpcmYn2;				// y[n - 2]
			uint16_t AdpcmGan;				// gain to be applied
			struct
			{
				uint16_t l;
				uint16_t h;
			} StartAddress;
			struct
			{
				uint16_t l;
				uint16_t h;
			} EndAddress;
			struct
			{
				uint16_t l;
				uint16_t h;
			} CurrAddress;
		} Accel;

		void ResetIfx();
		void DoDma();
		uint16_t AccelReadData(bool raw);
		void AccelWriteData(uint16_t data);

	public:

		static const size_t MaxInstructionSizeInBytes = 4;		// max instruction size

		static const size_t IRAM_SIZE = (8 * 1024);
		static const size_t IROM_SIZE = (8 * 1024);
		static const size_t DRAM_SIZE = (8 * 1024);
		static const size_t DROM_SIZE = (4 * 1024);

		static const size_t IROM_START_ADDRESS = 0x8000;
		static const size_t DROM_START_ADDRESS = 0x1000;
		static const size_t IFX_START_ADDRESS = 0xFF00;		// Internal dsp "hardware"

		DspRegs regs;

		uint8_t iram[IRAM_SIZE] = { 0 };
		uint8_t irom[IROM_SIZE] = { 0 };
		uint8_t dram[DRAM_SIZE] = { 0 };
		uint8_t drom[DROM_SIZE] = { 0 };

		DspCore(HWConfig* config);
		~DspCore();

		void Exception(DspException id);
		void ReturnFromException();
		void HardReset();

		void Run();
		bool IsRunning() { return dspThread->IsRunning(); }
		void Suspend();

		void Update();

		// Debug methods

		void AddBreakpoint(DspAddress imemAddress);
		void ListBreakpoints();
		void ClearBreakpoints();
		bool TestBreakpoint(DspAddress imemAddress);
		void AddCanary(DspAddress imemAddress, std::string text);
		void ListCanaries();
		void ClearCanaries();
		bool TestCanary(DspAddress imemAddress);
		void Step();
		void DumpRegs(DspRegs *prevState);
		void DumpIfx();

		// Register access

		void MoveToReg(int reg, uint16_t val);
		uint16_t MoveFromReg(int reg);

		// Memory engine

		uint8_t* TranslateIMem(DspAddress addr);
		uint8_t* TranslateDMem(DspAddress addr);
		uint16_t ReadIMem(DspAddress addr);
		uint16_t ReadDMem(DspAddress addr);
		void WriteDMem(DspAddress addr, uint16_t value);

		// Flipper interface

		void DSPSetResetBit(bool val);
		bool DSPGetResetBit();
		void DSPSetIntBit(bool val);
		bool DSPGetIntBit();
		void DSPSetHaltBit(bool val);
		bool DSPGetHaltBit();

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

		// Multiplier
		
		static void PackProd(DspProduct &prod);
		static void UnpackProd(DspProduct& prod);
		static DspProduct Muls(uint16_t a, uint16_t b);
		static DspProduct Mulu(uint16_t a, uint16_t b);

		static void InitSubsystem();
		static void ShutdownSubsystem();
	};

	#pragma warning (pop)		// warning C4201: nonstandard extension used: nameless struct/union

}
