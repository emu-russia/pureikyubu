// External interface
#include "pch.h"

uint8_t* RAM;
HINSTANCE   hPlugin;
HWND hwndMain;

static bool gxOpened = false;

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
