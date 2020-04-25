// External interface
#include "pch.h"

uint8_t* RAM;
HINSTANCE   hPlugin;
HWND hwndMain;

static bool gxOpened = false;

long GXOpen(uint8_t * ramPtr, HWND _hwndMain)
{
    if (gxOpened)
        return 1;

    BOOL res;

    hPlugin = GetModuleHandle(NULL);
    hwndMain = _hwndMain;

    RAM = ramPtr;

    res = GL_LazyOpenSubsystem(_hwndMain);
    assert(res);

    // vertex programs extension
    //SetupVertexShaders();
    //ReloadVertexShaders();

    // reset pipeline
    FifoReconfigure(VTX_MAX_ATTR, 0, 0, 0, 0, 0);
    accptr = accum;
    acclen = 0;
    cmdidle=1;
    frame_done=1;

    // flush texture cache
    TexInit();

    gxOpened = true;

    return true;
}

void GXClose()
{
    if (!gxOpened)
        return;

    GL_CloseSubsystem();

    TexFree();

    PerfClose();

    gxOpened = false;
}
