// Dolwin plugin system. See DolwinPluginSpecs.h for details.
// dont use Dolwin basic types (u32, etc), so people can
// easily use this module in their projects, without adding
// Dolwin types.
#include "dolphin.h"

//
// plugin shared data structure.
// dont forget to clear it, before "Open" call.
//

PluginData          plug;

//
// plugin API externals
//

// graphics externals
GXOPEN              GXOpen             = NULL;
GXCLOSE             GXClose            = NULL;
GXWRITEFIFO         GXWriteFifo        = NULL;
GXSETTOKENS         GXSetTokens        = NULL;
GXCONFIGURE         GXConfigure        = NULL;
GXABOUT             GXAbout            = NULL;

// audio externals
AXOPEN              AXOpen             = NULL;
AXCLOSE             AXClose            = NULL;
AXPLAYAUDIO         AXPlayAudio        = NULL;
AXSETRATE           AXSetRate          = NULL;
AXPLAYSTREAM        AXPlayStream       = NULL;
AXSETVOLUME         AXSetVolume        = NULL;
AXCONFIGURE         AXConfigure        = NULL;
AXABOUT             AXAbout            = NULL;

// pad externals
PADOPEN             PADOpen            = NULL;
PADCLOSE            PADClose           = NULL;
PADREADBUTTONS      PADReadButtons     = NULL;
PADSETRUMBLE        PADSetRumble       = NULL;
PADCONFIGURE        PADConfigure       = NULL;
PADABOUT            PADAbout           = NULL;

// dvd externals
DVDOPEN             DVDOpen            = NULL;
DVDCLOSE            DVDClose           = NULL;
DVDSETCURRENT       DVDSetCurrent      = NULL;
DVDSEEK             DVDSeek            = NULL;
DVDREAD             DVDRead            = NULL;
DVDOPENFILE         DVDOpenFile        = NULL;
DVDCONFIGURE        DVDConfigure       = NULL;
DVDABOUT            DVDAbout           = NULL;

REGISTERPLUGIN      RegisterPlugin     = NULL;

// ---------------------------------------------------------------------------

// plugin system management (controls)

static int  ps_opened = FALSE;          // TRUE, if plugins are ready

static HINSTANCE hGXInst = NULL,        // gfx plugin dll handler
                 hAXInst = NULL,        // audio
                 hPADInst = NULL,       // pad
                 hDVDInst = NULL,       // dvd
				 hNETInst = NULL;		// net

#define LoadAPI(lib, cast, name)                            \
{                                                           \
    ##name## = (cast)GetProcAddress(h##lib##Inst, #name);   \
    ASSERT(##name## == NULL, #lib"::"#name" is missing!");  \
}

static void GXPluginInit(char *name, int warn)
{
    unsigned long type = 0;

    hGXInst = LoadLibrary(name);
    if(hGXInst == NULL)
    {
        if(warn)
        {
            MessageBox(
                NULL, 
                "Video plugin is not assigned (3D graphics unavailable).\n"
                "Please, select graphics plugin in options.", 
                "Dolwin Plugin System", 
                MB_OK | MB_TOPMOST | MB_ICONEXCLAMATION
            );
        }
        return;
    }

    // check plugin type
    RegisterPlugin = (REGISTERPLUGIN)GetProcAddress(hGXInst, "RegisterPlugin");
    ASSERT((FARPROC)RegisterPlugin == NULL, "Plugin registration failed!");
    RegisterPlugin(&plug);
    ASSERT(!IS_DOL_PLUG_GX(plug.type), "Illegal plugin type!");
    DBReport(WHITE "GX : %s\n", plug.version);

    LoadAPI(GX, GXOPEN, GXOpen);
    LoadAPI(GX, GXCLOSE, GXClose);
    LoadAPI(GX, GXWRITEFIFO, GXWriteFifo);
    LoadAPI(GX, GXSETTOKENS, GXSetTokens);
    LoadAPI(GX, GXCONFIGURE, GXConfigure);
    LoadAPI(GX, GXABOUT, GXAbout);

    // set GX tokens
    GXSetTokens(&fifo.drawdone, &fifo.token, &fifo.pe.token);
}

static void AXPluginInit(char *name, int warn)
{
    unsigned long type = 0;

    hAXInst = LoadLibrary(name);
    if(hAXInst == NULL)
    {
        if(warn)
        {
            MessageBox(
                NULL, 
                "Audio plugin is not assigned (no sound support).\n"
                "Please, select audio plugin in options.", 
                "Dolwin Plugin System", 
                MB_OK | MB_TOPMOST | MB_ICONEXCLAMATION
            );
        }
        return;
    }

    // check plugin type
    RegisterPlugin = (REGISTERPLUGIN)GetProcAddress(hAXInst, "RegisterPlugin");
    ASSERT((FARPROC)RegisterPlugin == NULL, "Plugin registration failed!");
    RegisterPlugin(&plug);
    ASSERT(!IS_DOL_PLUG_AX(plug.type), "Illegal plugin type!");
    DBReport(WHITE "AX : %s\n", plug.version);

	LoadAPI(AX, AXOPEN, AXOpen);
    LoadAPI(AX, AXCLOSE, AXClose);
    LoadAPI(AX, AXPLAYAUDIO, AXPlayAudio);
    LoadAPI(AX, AXSETRATE, AXSetRate);
    LoadAPI(AX, AXPLAYSTREAM, AXPlayStream);
    LoadAPI(AX, AXSETVOLUME, AXSetVolume);
    LoadAPI(AX, AXCONFIGURE, AXConfigure);
    LoadAPI(AX, AXABOUT, AXAbout);
}

static void PADPluginInit(char *name, int warn)
{
    unsigned long type = 0;

    hPADInst = LoadLibrary(name);
    if(hPADInst == NULL)
    {
        if(warn)
        {
            MessageBox(
                NULL, 
                "Input plugin is not assigned (no PAD support).\n"
                "Please, select input plugin in options.", 
                "Dolwin Plugin System", 
                MB_OK | MB_TOPMOST | MB_ICONEXCLAMATION
            );
        }
        return;
    }

    // check plugin type
    RegisterPlugin = (REGISTERPLUGIN)GetProcAddress(hPADInst, "RegisterPlugin");
    ASSERT((FARPROC)RegisterPlugin == NULL, "Plugin registration failed!");
    RegisterPlugin(&plug);
    ASSERT(!IS_DOL_PLUG_PAD(plug.type), "Illegal plugin type!");
    DBReport(WHITE "PAD: %s\n", plug.version);

    LoadAPI(PAD, PADOPEN, PADOpen);
    LoadAPI(PAD, PADCLOSE, PADClose);
    LoadAPI(PAD, PADREADBUTTONS, PADReadButtons);
    LoadAPI(PAD, PADSETRUMBLE, PADSetRumble);
    LoadAPI(PAD, PADCONFIGURE, PADConfigure);
    LoadAPI(PAD, PADABOUT, PADAbout);
}

static void DVDPluginInit(char *name, int warn)
{
    unsigned long type = 0;

    hDVDInst = LoadLibrary(name);
    if(hDVDInst == NULL)
    {
        if(warn)
        {
            MessageBox(
                NULL, 
                "DVD plugin is not assigned (no DVD read support).\n"
                "Please, select DVD plugin in options.", 
                "Dolwin Plugin System", 
                MB_OK | MB_TOPMOST | MB_ICONEXCLAMATION
            );
        }
        return;
    }

    // check plugin type
    RegisterPlugin = (REGISTERPLUGIN)GetProcAddress(hDVDInst, "RegisterPlugin");
    ASSERT((FARPROC)RegisterPlugin == NULL, "Plugin registration failed!");
    RegisterPlugin(&plug);
    ASSERT(!IS_DOL_PLUG_DVD(plug.type), "Illegal plugin type!");
    DBReport(WHITE "DVD: %s\n", plug.version);

    LoadAPI(DVD, DVDOPEN, DVDOpen);
    LoadAPI(DVD, DVDCLOSE, DVDClose);
    LoadAPI(DVD, DVDSETCURRENT, DVDSetCurrent);
    LoadAPI(DVD, DVDSEEK, DVDSeek);
    LoadAPI(DVD, DVDREAD, DVDRead);
    LoadAPI(DVD, DVDOPENFILE, DVDOpenFile);
    LoadAPI(DVD, DVDCONFIGURE, DVDConfigure);
    LoadAPI(DVD, DVDABOUT, DVDAbout);
}

// ---------------------------------------------------------------------------

void PSOpen()
{
    if(ps_opened == TRUE) return;

    // prepare plugin data structure
    memset(&plug, 0, sizeof(PluginData));
    plug.ram = RAM;
    plug.display = (void *)&wnd.hMainWindow;

    GXPluginInit (GetConfigString(USER_GX , USER_GX_DEFAULT) , 0);
    AXPluginInit (GetConfigString(USER_AX , USER_AX_DEFAULT) , 0);
    PADPluginInit(GetConfigString(USER_PAD, USER_PAD_DEFAULT), 0);
    DVDPluginInit(GetConfigString(USER_DVD, USER_DVD_DEFAULT), 0);

    if(GXOpen)  ASSERT(GXOpen()  == 0, "Cannot start graphics!");
    if(AXOpen)  ASSERT(AXOpen()  == 0, "Cannot start audio!");
    if(PADOpen) ASSERT(PADOpen() == 0, "Cannot start joypad!");
    if(DVDOpen) ASSERT(DVDOpen() == 0, "Cannot start DVD!");
    DBReport("\n");

    ps_opened = TRUE;
}

void PSClose()
{
    if(ps_opened == FALSE) return;

    if(PADClose) PADClose();    // first user losing focus
    if(AXClose)  AXClose();     // then sound
    if(DVDClose) DVDClose();    // then disk
    if(GXClose)  GXClose();     // and then, at last the picture

    if(hGXInst)  FreeLibrary(hGXInst);
    if(hAXInst)  FreeLibrary(hAXInst);
    if(hPADInst) FreeLibrary(hPADInst);
    if(hDVDInst) FreeLibrary(hDVDInst);

    hGXInst = hAXInst = hPADInst = hDVDInst = hNETInst = NULL;

    ps_opened = FALSE;
}

void PSInit()
{
    int warn = GetConfigInt(USER_PLUG_WARN, USER_PLUG_WARN_DEFAULT);

    // prepare plugin data structure
    memset(&plug, 0, sizeof(PluginData));
    plug.ram = RAM;
    plug.display = (void *)&wnd.hMainWindow;

    GXPluginInit (GetConfigString(USER_GX , USER_GX_DEFAULT) , warn);
    AXPluginInit (GetConfigString(USER_AX , USER_AX_DEFAULT) , warn);
    PADPluginInit(GetConfigString(USER_PAD, USER_PAD_DEFAULT), warn);
    DVDPluginInit(GetConfigString(USER_DVD, USER_DVD_DEFAULT), warn);
}

void PSShutdown()
{
    if(hGXInst)  FreeLibrary(hGXInst);
    if(hAXInst)  FreeLibrary(hAXInst);
    if(hPADInst) FreeLibrary(hPADInst);
    if(hDVDInst) FreeLibrary(hDVDInst);

    hGXInst = hAXInst = hPADInst = hDVDInst = hNETInst = NULL;
}
