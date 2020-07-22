// Emulator includes

#pragma once

#define EMU_VERSION _T("0.131")

void    EMUGetHwConfig(HWConfig* config);

// emulator controls API
void    EMUCtor();
void    EMUDtor();
void    EMUOpen(bool run);  // [START]
void    EMUClose();         // [STOP]
void    EMUReset();         // Reset

// all important data is placed here
typedef struct Emulator
{
    bool    loaded;         // file loaded
} Emulator;

extern  Emulator emu;

#include "EmuCommands.h"
