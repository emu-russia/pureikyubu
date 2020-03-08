// GX (3D graphics) interface

#pragma once

#include <Windows.h>

// return values are always 1 for good, and 0 for bad.

// GXOpen() should be called before emulation started, to initialize
// plugin. GXClose() is called, when emulation is stopped, to shutdown plugin.
long GXOpen(uint8_t* ramPtr, HWND hwndMain);
void GXClose();

// add new data to graphics fifo. draw next primitive, if there are enough data.
// should be called, when CPU is writing any data into 0x0C008000, when CP is
// working in "linked" mode ("single-buffer" fifo). emulator should take care
// about "multi-buffer" fifo mode itself.
void GXWriteFifo(uint8_t *dataPtr, uint32_t length);

// set pointers to GX tokens.
// when fifo reaches "draw done", it will set peDrawDone to 1.
// when fifo reaches token, it will set peToken to 1, and tokenVal to token value.
// screen is updated automatically, after DrawDone or Token event.
// GX doesnt responsible for clearing token values (should be performed by emu).
void GXSetTokens(long *peDrawDone, long *peToken, unsigned short *tokenVal);
