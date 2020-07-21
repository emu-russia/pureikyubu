// Centralized hub for all debug messages

#pragma once

namespace Debug
{
	// Debug messages output channel

	enum class Channel
	{
		Void = 0,

		Norm,
		Info,
		Error,
		Header,

		CP,
		PE,
		VI,
		GP,
		PI,
		CPU,
		MI,
		DSP,
		DI,
		AR,
		AI,
		AIS,
		SI,
		EXI,
		MC,
		DVD,
		AX,

		Loader,
		HLE,
	};

	// always breaks emulation
	void Halt (const char* text, ...);

	// do debugger output
	void Report (Channel chan, const char* text, ...);

}
