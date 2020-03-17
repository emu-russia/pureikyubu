// Dolwin debugger interface

#pragma once

#include "console.h"
#include "GekkoDisasm.h"    // Gekko disassembler
#include "DspDisasm.h"		// DSP disassembler

// hardware messages output channel
enum class DbgChannel
{
	Void = 0,

	Norm,
	Info,
	Error,

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
