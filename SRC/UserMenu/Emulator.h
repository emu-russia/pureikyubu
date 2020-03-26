// Emulator includes

#pragma once

void    EMUGetHwConfig(HWConfig* config);

// emulator controls API
void    EMUOpen();          // [START]
void    EMUClose();         // [STOP]
void    EMUReset();         // Reset

// all important data is placed here
typedef struct Emulator
{
    bool    loaded;         // file loaded
    bool    doldebug;       // debugger active
    Gekko::GekkoCore* core;
    Flipper::Flipper* hw;
} Emulator;

extern  Emulator emu;
