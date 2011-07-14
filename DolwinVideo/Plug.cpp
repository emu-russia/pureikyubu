// plugin interface
#include "GX.h"

HINSTANCE   hPlugin;            // dll handler
HWND*       hwndMain;           // emulator main window
PluginData  *plug;

BOOL APIENTRY DllMain(
    HANDLE  hModule,
    DWORD   dwReasonForCall,
    LPVOID  lpUnknown)
{
    hPlugin = (HINSTANCE)hModule;
    return TRUE;
}

// make sure, that emulator is using correct GX plugin
void RegisterPlugin(PluginData * plugData)
{
    // save main window handler
    hwndMain = (HWND *)plugData->display;
    plug     = plugData;

    plugData->type = DOL_PLUG_GX;
    plugData->version = "GX OpenGL Plugin" " (" GX_VER ")";
}

// critical error
void GFXError(char *fmt, ...)
{
    va_list arg;
    char buf[0x1000];

    va_start(arg, fmt);
    vsprintf(buf, fmt, arg);
    va_end(arg);

    MessageBox(
        NULL, 
        buf, 
        "GFX plugin error", 
        MB_ICONEXCLAMATION | MB_OK | MB_TOPMOST
    );
}

// ---------------------------------------------------------------------------

void GXConfigure()
{
}

void GXAbout()
{
}

void GXSaveLoad(long flag, char *filename)
{
}

// ---------------------------------------------------------------------------

long GXOpen()
{
    BOOL res;

    RAM = plug->ram;
    gfx = &ogl;         // hack : select opengl as default
    res = gfx->OpenSubsystem(*hwndMain);

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

    // prepare on-screen font texture
    PerfInit();

    return res;
}

void GXClose()
{
    gfx->CloseSubsystem();

    TexFree();
}
