// Defines the state of the entire graphics system

#pragma once

namespace GX
{

	// PE registers mapped to CPU
	struct PERegs
	{
		uint16_t     sr;         // status register
		uint16_t     token;      // last token
	};

	struct State
	{
		PERegs pe;
	};

}
