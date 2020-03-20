// Low-level DSP core

/*

# How does the DSP core work

The Run method executes the Update method until it is stopped by the Suspend method or it encounters a breakpoint.

Suspend method stops DSP thread execution indifinitely (same as HALT instruction).

The Step debugging method is used to unconditionally execute next DSP instruction (by interpreter).

The Update method checks the value of Gekko TBR. If its value has exceeded the limit for the execution of one DSP instruction (or segment in case of Jitc),
interpreter/Jitc Execute method is called.

DspCore uses the interpreter and recompiler at the same time, of their own free will, depending on the situation.

*/

#pragma once

#include <vector>
#include <map>
#include <string>
#include <atomic>

namespace DSP
{
	typedef uint32_t DspAddress;		///< in halfwords slots 

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

	typedef union _DspProduct
	{
		struct
		{
			uint16_t l;
			uint16_t m1;
			uint16_t m2;
			uint16_t h;
		};
		uint64_t bitsUnpacked;
	} DspProduct;

	typedef union _DspStatus
	{
		struct
		{
			unsigned c : 1;			///< Carry
			unsigned o : 1;			///< Overﬂow 
			unsigned z : 1;			///< Arithmetic zero 
			unsigned s : 1;		///< Sign
			unsigned as : 1;	///< Above s32 
			unsigned tt : 1;	///< Top two bits are equal 
			unsigned ok : 1;	///< 1: Bit test OK, 0: Bit test not OK
			unsigned os : 1;	///< Overflow (sticky)
			unsigned hwz : 1;	///< Hardwired to 0? 
			unsigned ie : 1;	///< Interrupt enable 
			unsigned unk10 : 1;
			unsigned eie : 1;		///< External interrupt enable 
			unsigned unk12 : 1;
			// Not actually status, but ALU control
			unsigned am : 1;		///< Product multiply result by 2 (when AM = 0)  (0 = M2, 1 = M0)
			unsigned sxm : 1;	///< Sign extension mode (0 = clr40, 1 = set40)
			unsigned su : 1;	///< Operands are signed (1 = unsigned) 
		};

		uint16_t bits;

	} DspStatus;

	#pragma pack (pop)

	enum class DspRegister
	{
		ar0 = 0,		///< Addressing register 0 
		ar1,			///< Addressing register 1 
		ar2,			///< Addressing register 2 
		ar3,			///< Addressing register 3 
		indexRegs,
		ix0 = indexRegs,	///< Indexing register 0 
		ix1,			///< Indexing register 1
		ix2,			///< Indexing register 2
		ix3,			///< Indexing register 3
		gprs,
		r8 = gprs,
		r9,
		r10,
		r11,
		stackRegs,
		st0 = stackRegs,	///< Call stack register 
		st1,			///< Data stack register 
		st2,			///< Loop address stack register 
		st3,			///< Loop counter register 
		ac0h,			///< 40-bit Accumulator 0 (high) 
		ac1h,			///< 40-bit Accumulator 1 (high) 
		config,			///< Config register 
		sr,				///< Status register 
		prodl,			///< Product register (low) 
		prodm1,			///< Product register (mid 1) 
		prodh,			///< Product register (high) 
		prodm2,			///< Product register (mid 2) 
		ax0l,			///< 32-bit Accumulator 0 (low) 
		ax0h,			///< 32-bit Accumulator 0 (high) 
		ax1l,			///< 32-bit Accumulator 1 (low) 
		ax1h,			///< 32-bit Accumulator 1 (high
		ac0l,			///< 40-bit Accumulator 0 (low) 
		ac1l,			///< 40-bit Accumulator 1 (low) 
		ac0m,			///< 40-bit Accumulator 0 (mid)
		ac1m,			///< 40-bit Accumulator 1 (mid)
	};

	typedef struct _DspRegs
	{
		uint32_t clearingPaddy;		///< To use {0} on structure
		uint16_t ar[4];		///< Addressing registers
		uint16_t ix[4];		///< Indexing registers
		uint16_t gpr[4];	///< General purpose (r8-r11)
		std::vector<DspAddress> st[4];	///< Stack registers
		DspLongAccumulator ac[2];		///< 40-bit Accumulators
		DspShortAccumulator ax[2];		///< 32-bit Accumulators
		DspProduct prod;		///< Product register
		uint16_t cr;		///< config
		DspStatus sr;		///< status
		DspAddress pc;		///< Program counter
	} DspRegs;

	enum class DspHardwareRegs
	{
		CMBH = 0xFFFE,		///< CPU->DSP Mailbox H 
		CMBL = 0xFFFF,		///< CPU->DSP Mailbox L 
		DMBH = 0xFFFC,		///< DSP->CPU Mailbox H 
		DMBL = 0xFFFD,		///< DSP->CPU Mailbox L 

		DSMAH = 0xFFCE,		///< Memory address H 
		DSMAL = 0xFFCF,		///< Memory address L 
		DSPA = 0xFFCD,		///< DSP memory address 
		DSCR = 0xFFC9,		///< DMA control 
		DSBL = 0xFFCB,		///< Block size 

		ACDAT2 = 0xFFD3,	///< Another accelerator data (R/W)
		ACSAH = 0xFFD4,		///< Accelerator start address H 
		ACSAL = 0xFFD5,		///< Accelerator start address L 
		ACEAH = 0xFFD6,		///< Accelerator end address H 
		ACEAL = 0xFFD7,		///< Accelerator end address L 
		ACCAH = 0xFFD8,		///< Accelerator current address H 
		ACCAL = 0xFFD9,		///< Accelerator current address L 
		ACDAT = 0xFFDD,		///< Accelerator data
		AMDM = 0xFFEF,		///< ARAM DMA Request Mask

		DIRQ = 0xFFFB,		///< IRQ request

		// From https://github.com/devkitPro/gamecube-tools/blob/master/gdopcode/disassemble.cpp

		ACFMT = 0xFFD1,
		ACPDS = 0xFFDA,
		ACYN1 = 0xFFDB,
		ACYN2 = 0xFFDC,
		ACGAN = 0xFFDE,

		// Unknown (FIR Filters?)

		UNKNOWN_FFA0,
		UNKNOWN_FFA1,
		UNKNOWN_FFA2,
		UNKNOWN_FFA3,
		UNKNOWN_FFA4,
		UNKNOWN_FFA5,
		UNKNOWN_FFA6,
		UNKNOWN_FFA7,
		UNKNOWN_FFA8,
		UNKNOWN_FFA9,
		UNKNOWN_FFAA,
		UNKNOWN_FFAB,
		UNKNOWN_FFAC,
		UNKNOWN_FFAD,
		UNKNOWN_FFAE,
		UNKNOWN_FFAF,

		// Unknown

		UNKNOWN_FFB0,
		UNKNOWN_FFB1,

		// TODO: What about sample-rate/ADPCM converter mentioned in patents/sdk?
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
		Unknown6,
		INT,			// External interrupt (from CPU)
	};

	class DspInterpreter;

	class DspCore
	{
		std::vector<DspAddress> breakpoints;		///< IMEM breakpoints
		MySpinLock::LOCK breakPointsSpinLock;

		std::map<DspAddress, std::string> canaries;		///< When the PC is equal to the canary address, a debug message is displayed
		MySpinLock::LOCK canariesSpinLock;

		const uint32_t GekkoTicksPerDspInstruction = 5;		///< How many Gekko ticks should pass so that we can execute one DSP instruction
		const uint32_t GekkoTicksPerDspSegment = 50;		///< How many Gekko ticks should pass so that we can execute one DSP segment (in case of Jitc)

		uint64_t savedGekkoTicks = 0;

		bool running = false;

		HANDLE threadHandle;
		DWORD threadId;

		static DWORD WINAPI DspThreadProc(LPVOID lpParameter);

		DspInterpreter* interp;

		std::atomic<uint16_t> DspToCpuMailbox[2];		///< DMBH, DMBL
		uint16_t DspToCpuMailboxShadow[2];

		std::atomic<uint16_t> CpuToDspMailbox[2];		///< CMBH, CMBL
		uint16_t CpuToDspMailboxShadow[2];

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
					unsigned Dsp2Mmem : 1;		/// 0: MMEM -> DSP, 1: DSP -> MMEM
					unsigned Imem : 1;			/// 0: DMEM, 1: IMEM
				};
				uint16_t	bits;
			} control;
		} DmaRegs;

		void ResetIfx();
		void DoDma();

	public:

		static const size_t MaxInstructionSizeInBytes = 4;		///< max instruction size

		static const size_t IRAM_SIZE = (8 * 1024);
		static const size_t IROM_SIZE = (8 * 1024);
		static const size_t DRAM_SIZE = (8 * 1024);
		static const size_t DROM_SIZE = (4 * 1024);

		static const size_t IROM_START_ADDRESS = 0x8000;
		static const size_t DROM_START_ADDRESS = 0x1000;
		static const size_t IFX_START_ADDRESS = 0xFF00;		///< Internal dsp "hardware"

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
		bool IsRunning() { return running; }
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
		uint16_t CpuToDspReadLo();
		// DSP->CPU Mailbox
		void DspToCpuWriteHi(uint16_t value);
		void DspToCpuWriteLo(uint16_t value);
		uint16_t DspToCpuReadHi(bool ReadByDsp);
		uint16_t DspToCpuReadLo();

	};
}
