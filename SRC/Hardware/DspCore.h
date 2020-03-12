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

	class DspCore
	{
		std::vector<uint16_t> breakpoints;		///< IMEM breakpoints
		MySpinLock::LOCK breakPointsSpinLock;

		const uint32_t GekkoTicksPerDspInstruction = 10;		///< How many Gekko ticks should pass so that we can execute one DSP instruction
		const uint32_t GekkoTicksPerDspSegment = 100;		///< How many Gekko ticks should pass so that we can execute one DSP segment (in case of Jitc)

		uint64_t savedGekkoTicks = 0;

	public:

		DspCoreType coreType = DspCoreType::Interpreter;

		DspRegs regs = { 0 };

		uint8_t dmem[0x10000 * 2] = { 0 };		///< DSP DRAM+DROM
		uint8_t imem[0x10000 * 2] = { 0 };		///< DSP IRAM+IROM

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
