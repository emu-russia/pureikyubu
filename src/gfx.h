// Flipper GFX Engine
#pragma once

#include "cp.h"
#include "xf.h"
#include "su.h"
#include "ras.h"
#include "tev.h"
#include "tx.h"
#include "pe.h"

namespace GX
{

	// Defines the state of the entire graphics system
	struct State
	{
		PERegs peregs;
		CPHostRegs cpregs;		// Mapped command processor registers
		CPState cp;
		XFState xf;

		// PI FIFO
		volatile uint32_t    pi_cp_base;
		volatile uint32_t    pi_cp_top;
		volatile uint32_t    pi_cp_wrptr;          // also WRAP bit

		// Command Processor
		Thread* cp_thread;     // CP FIFO thread
		size_t	tickPerFifo;
		int64_t	updateTbrValue;

		// Stats
		size_t cpLoads;
		size_t xfLoads;
		size_t bpLoads;
	};

}


namespace GX
{

	class GXCore
	{
		State state;

		// TODO: Refactoring hacks
		void DONE_INT();
		void TOKEN_INT();

		void CP_BREAK();
		void CP_OVF();
		void CP_UVF();

		static void CPThread(void* Param);

		FifoProcessor * fifo;		// Internal CP FIFO

	public:
		GXCore();
		~GXCore();

		void Open();
		void Close();

		// Debug
		void DumpPIFIFO();
		void DumpCPFIFO();

#pragma region "Interface to Flipper"

		// CP Registers
		uint16_t CpReadReg(CPMappedRegister id);
		void CpWriteReg(CPMappedRegister id, uint16_t value);

		// Pixel Engine
		uint16_t PeReadReg(PEMappedRegister id);
		void PeWriteReg(PEMappedRegister id, uint16_t value);
		uint32_t EfbPeek(uint32_t addr);
		void EfbPoke(uint32_t addr, uint32_t value);

		// PI->CP Registers
		uint32_t PiCpReadReg(PI_CPMappedRegister id);
		void PiCpWriteReg(PI_CPMappedRegister id, uint32_t value);

		// Streaming FIFO (32-byte burst-only)
		void FifoWriteBurst(uint8_t data[32]);

		// TODO: Refactoring hacks
		void CPDrawDoneCallback();
		void CPDrawTokenCallback(uint16_t tokenValue);

#pragma endregion "Interface to Flipper"


	};

}
