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

namespace DSP
{
	typedef uint32_t DspAddress;		///< in halfwords slots 

	typedef struct _DspRegs
	{
		uint32_t clearingPaddy;		///< To use {0} on structure
		uint16_t ar[4];		///< Addressing registers
		uint16_t ix[4];		///< Indexing registers
		uint16_t gpr[4];	///< General purpose (r8-r11)
		DspAddress st[4];	///< Stack registers
		uint64_t ac[2];		///< 40-bit Accumulators
		uint64_t ax[2];		///< 32-bit Accumulators
		uint64_t prod;		///< Product register
		uint16_t cr;		///< config
		uint16_t sr;		///< status
		DspAddress pc;		///< Program counter
	} DspRegs;

	enum class DspHardwareRegs
	{
		CMBH = 0xFFFE,		///< CPU Mailbox H 
		CMBL = 0xFFFF,		///< CPU Mailbox L 
		DMBH = 0xFFFC,		///< DSP Mailbox H 
		DMBL = 0xFFFD,		///< DSP Mailbox L 

		DSMAH = 0xFFCE,		///< Memory address H 
		DSMAL = 0xFFCF,		///< Memory address L 
		DSPA = 0xFFCD,		///< DSP memory address 
		DSCR = 0xFFC9,		///< DMA control 
		DSBL = 0xFFCB,		///< Block size 

		ACSAH = 0xFFD4,		///< Accelerator start address H 
		ACSAL = 0xFFD5,		///< Accelerator start address L 
		ACEAH = 0xFFD6,		///< Accelerator end address H 
		ACEAL = 0xFFD7,		///< Accelerator end address L 
		ACCAH = 0xFFD8,		///< Accelerator current address H 
		ACCAL = 0xFFD9,		///< Accelerator current address L 
		ACDAT = 0xFFDD,		///< Accelerator data

		DIRQ = 0xFFFB,		///< IRQ request

		// From https://github.com/devkitPro/gamecube-tools/blob/master/gdopcode/disassemble.cpp

		ACFMT = 0xFFD1,
		ACPDS = 0xFFDA,
		ACYN1 = 0xFFDB,
		ACYN2 = 0xFFDC,
		ACGAN = 0xFFDE,
		AMDM = 0xFFEF,		///< ARAM DMA Request Mask

		// TODO: What about sample-rate/ADPCM converter mentioned in patents/sdk?
	};

	class DspInterpreter;

	class DspCore
	{
		std::vector<DspAddress> breakpoints;		///< IMEM breakpoints
		MySpinLock::LOCK breakPointsSpinLock;

		const uint32_t GekkoTicksPerDspInstruction = 10;		///< How many Gekko ticks should pass so that we can execute one DSP instruction
		const uint32_t GekkoTicksPerDspSegment = 100;		///< How many Gekko ticks should pass so that we can execute one DSP segment (in case of Jitc)

		uint64_t savedGekkoTicks = 0;

		bool running = false;

		HANDLE threadHandle;
		DWORD threadId;

		static DWORD WINAPI DspThreadProc(LPVOID lpParameter);

		DspInterpreter* interp;

	public:

		static const size_t MaxInstructionSizeInBytes = 4;		///< max instruction size

		static const size_t IRAM_SIZE = (8 * 1024);
		static const size_t IROM_SIZE = (8 * 1024);
		static const size_t DRAM_SIZE = (8 * 1024);
		static const size_t DROM_SIZE = (4 * 1024);

		DspRegs regs = { 0 };

		uint8_t iram[IRAM_SIZE] = { 0 };
		uint8_t irom[IROM_SIZE] = { 0 };
		uint8_t dram[DRAM_SIZE] = { 0 };
		uint8_t drom[DROM_SIZE] = { 0 };

		DspCore(HWConfig* config);
		~DspCore();

		void Reset();

		void Run();
		bool IsRunning() { return running; }
		void Suspend();

		void Update();

		// Debug methods

		void AddBreakpoint(DspAddress imemAddress);
		void ListBreakpoints();
		void ClearBreakpoints();
		void Step();
		void DumpRegs(DspRegs *prevState);

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

		void DSPWriteOutMailboxHi(uint16_t value);
		void DSPWriteOutMailboxLo(uint16_t value);
		uint16_t DSPReadOutMailboxHi();
		uint16_t DSPReadOutMailboxLo();
		uint16_t DSPReadInMailboxHi();
		uint16_t DSPReadInMailboxLo();

	};
}
