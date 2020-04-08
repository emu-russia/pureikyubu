// Dolwin debugger interface

#pragma once

// Debug messages output channel
enum class DbgChannel
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

// '\n' is supported (no auto-linefeed)

// always breaks emulation
extern  void (*DBHalt)(const char *text, ...);

// do debugger output
extern  void (*DBReport)(const char* text, ...);
extern  void (*DBReport2)(DbgChannel chan, const char *text, ...);

void    DBOpen();                           // open debugger window in its own thread
void    DBClose();                          // close debugger completely

#include "Jdi.h"
