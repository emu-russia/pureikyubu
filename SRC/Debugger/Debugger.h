// Dolwin debugger interface.
// Note: every debugger call must include if(emu.doldebug) check!

#pragma once

#include "console.h"
#include "GekkoDisasm.h"    // Gekko disassembler
#include "DspDisasm.h"		// DSP disassembler

// hardware messages output prefix (GC hardware)
#define CP      CYAN "CP : "
#define PE      CYAN "PE : "
#define VI      CYAN "VI : "
#define GP      CYAN "GP : "
#define PI      CYAN "PI : "
#define CPU     CYAN "CPU: "
#define MI      CYAN "MI : "
#define _DSP    CYAN "DSP: "
#define DI      CYAN "DI : "
#define AR      CYAN "AR : "
#define AI      CYAN "AI : "
#define AIS     CYAN "AIS: "
#define _SI     CYAN "SI : "
#define EXI     CYAN "EXI: "
#define MC      CYAN "MC : "

// '\n' is supported (no auto-linefeed!)
extern  void (*DBHalt)(const char *text, ...);    // always breaks emulation
extern  void (*DBReport)(const char *text, ...);  // do debugger output

void    DBOpen();                           // open debugger window ("integrate")
void    DBClose();                          // close debugger completely
void    DBRedraw();                         // redraw console, GUI, what else
void    DBStart();                          // start debugger loop, from PC
