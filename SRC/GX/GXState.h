// Defines the state of the entire graphics system

#pragma once

namespace GX
{

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
