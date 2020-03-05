// Emulator includes

#pragma once

// emulator sub-systems
#include "Memory.h"         // memory engine for CPU
#include "Gekko.h"          // CPU controls
#include "Interpreter.h"    // Gekko interpreter
#include "Loader.h"         // GC file loader

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
