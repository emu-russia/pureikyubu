// GX (3D graphics) interface

#pragma once

#include <Windows.h>

// return values are always 1 for good, and 0 for bad.

// GXOpen() should be called before emulation started, to initialize graphics backend.
// GXClose() is called, when emulation is stopped, to shutdown graphics backend.
long GXOpen(uint8_t* ramPtr, HWND hwndMain);
void GXClose();

// add new data to graphics fifo. draw next primitive, if there are enough data.
void GXWriteFifo(uint8_t dataPtr[32]);

typedef void (*GXDrawDoneCallback)();
typedef void (*GXDrawTokenCallback)(uint16_t tokenValue);

// when fifo reaches "draw done", it will issue GXDrawDoneCallback.
// when fifo reaches token, it will issue GXDrawTokenCallback.
void GXSetDrawCallbacks(GXDrawDoneCallback drawDoneCb, GXDrawTokenCallback drawTokenCb);
