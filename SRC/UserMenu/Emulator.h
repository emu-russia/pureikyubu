// Emulator includes

#pragma once

void    EMUGetHwConfig(HWConfig* config);

// emulator controls API
void    EMUInit();          // called once
void    EMUDie();           // called once
void    EMUOpen();          // [START]
void    EMUClose();         // [STOP]

// EMUReset = [STOP], [START]

// all important data is placed here
typedef struct Emulator
{
    bool    initok;         // sub-systems are ready
    bool    loaded;         // file loaded
    bool    doldebug;       // debugger active
    Gekko::GekkoCore* core;
} Emulator;

extern  Emulator emu;
