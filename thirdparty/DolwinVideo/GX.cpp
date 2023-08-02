// External interface
#include "pch.h"

using namespace Debug;

uint8_t* RAM;
HINSTANCE   hPlugin;
HWND hwndMain;

static bool gxOpened = false;
extern int     VtxSize[8];

long GXOpen(HWConfig* config, uint8_t * ramPtr)
{
    if (gxOpened)
        return 1;

    BOOL res;

    hPlugin = GetModuleHandle(NULL);
    hwndMain = (HWND)config->renderTarget;

    RAM = ramPtr;

    res = GL_LazyOpenSubsystem(hwndMain);
    assert(res);

    // vertex programs extension
    //SetupVertexShaders();
    //ReloadVertexShaders();

    // reset pipeline
    frame_done = true;

    // flush texture cache
    TexInit();

    GxFifo.Reset();

    gxOpened = true;

    return true;
}

void GXClose()
{
    if (!gxOpened)
        return;

    GL_CloseSubsystem();

    TexFree();

    gxOpened = false;
}
