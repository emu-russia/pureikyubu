// Emulator includes

#ifndef __EMULATOR_H__
#define __EMULATOR_H__

// emulator sub-systems
#include "Memory.h"         // memory engine for CPU
#include "Gekko.h"          // CPU controls
#include "DisasmPPC.h"      // PowerPC disassembler
#include "DisasmX86.h"      // x86-series disassembler
#include "Debugger.h"       // debugger interface
#include "Interpreter.h"    // Gekko interpreter
#include "Recompiler.h"     // Gekko recompiler
#include "Loader.h"         // GC file loader
#include "SaveLoad.h"       // save-state operations
#include "Compare.h"        // CPU compare engine

// emulator controls API
void    EMUInit();          // called once
void    EMUDie();           // called once
void    EMUOpen();          // [START]
void    EMUClose();         // [STOP]

// EMUReset = [STOP], [START]

// all important data is placed here
typedef struct Emulator
{
    BOOL    initok;         // sub-systems are ready
    BOOL    running;        // running game (not Idle)
    BOOL    doldebug;       // debugger active
} Emulator;

extern  Emulator emu;

#endif  // __EMULATOR_H__
