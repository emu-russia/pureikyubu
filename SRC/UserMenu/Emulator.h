// Emulator includes

#pragma once

// emulator controls API
void    EMUInit();          // called once
void    EMUDie();           // called once
void    EMUOpen(int bailout, int delay, int counterFactor); // [START]
void    EMUClose();         // [STOP]

// EMUReset = [STOP], [START]

// all important data is placed here
typedef struct Emulator
{
    bool    initok;         // sub-systems are ready
    bool    running;        // running game (not Idle)
    bool    doldebug;       // debugger active
} Emulator;

extern  Emulator emu;
