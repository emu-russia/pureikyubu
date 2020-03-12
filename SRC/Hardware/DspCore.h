// Low-level DSP core

/*

# How does the DSP core work

The Run method executes the Update method until it is stopped by the Suspend method or it encounters a breakpoint.

Suspend method stops DSP thread execution indifinitely (same as HALT instruction).

The Step debugging method is used to unconditionally execute next DSP instruction (by interpreter).

The Update method checks the value of Gekko TBR. If its value has exceeded the limit for the execution of one DSP instruction (or segment in case of Jitc),
interpreter/Jitc Execute method is called.

*/

#pragma once

#include <vector>

namespace DSP
{
	enum DspCoreType
	{
		Interpreter = 0,
		Jitc,
	};

	typedef struct _DspRegs
	{
		uint32_t clearingPaddy;		///< To use {0} on structure

		uint16_t pc;		///< Program counter
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

		// TODO: What about sample-rate/ADPCM converter mentioned in patents/sdk?
	};

	class DspCore
	{
		std::vector<uint16_t> breakpoints;		///< IMEM breakpoints
		MySpinLock::LOCK breakPointsSpinLock;

		const uint32_t GekkoTicksPerDspInstruction = 10;		///< How many Gekko ticks should pass so that we can execute one DSP instruction
		const uint32_t GekkoTicksPerDspSegment = 100;		///< How many Gekko ticks should pass so that we can execute one DSP segment (in case of Jitc)

		uint64_t savedGekkoTicks = 0;

	public:

		static const size_t IRAM_SIZE = (8 * 1024);
		static const size_t IROM_SIZE = (8 * 1024);
		static const size_t DRAM_SIZE = (8 * 1024);
		static const size_t DROM_SIZE = (4 * 1024);

		DspCoreType coreType = DspCoreType::Interpreter;

		DspRegs regs = { 0 };

		uint8_t iram[IRAM_SIZE] = { 0 };
		uint8_t irom[IROM_SIZE] = { 0 };
		uint8_t dram[DRAM_SIZE] = { 0 };
		uint8_t drom[DROM_SIZE] = { 0 };

		DspCore(HWConfig* config);
		~DspCore();

		void Reset();

		void AddBreakpoint(uint16_t imemAddress /* in halfwords slots */);
		void ClearBreakpoints();

		void Run();
		void Suspend();

		///< Execute single instruction (by interpreter)  (DEBUG)
		void Step();

		void Update();
	};
}
