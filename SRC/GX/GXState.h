// Defines the state of the entire graphics system

#pragma once

namespace GX
{

	struct State
	{
		PERegs peregs;
		CPRegs cpregs;		// Mapped command processor registers

		// PI FIFO
		volatile uint32_t    pi_cp_base;
		volatile uint32_t    pi_cp_top;
		volatile uint32_t    pi_cp_wrptr;          // also WRAP bit

		// Command Processor
		Thread* cp_thread;     // CP FIFO thread
		size_t	tickPerFifo;
		int64_t	updateTbrValue;
	};

}
