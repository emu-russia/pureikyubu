// GC hardware includes

#pragma once

// Config from Ui

#include <Windows.h>

typedef struct _HWConfig
{
    // MI
    size_t      ramsize;

    // VI
	HWND	    hwndMain;
    bool        vi_log;
    bool        vi_xfb;
    uint32_t    vcount;
    int         videoEncoderFuse;       // 1 - PAL, 0 - NTSC

    // PI
    uint32_t    consoleVer;
    bool        rswhack;

    // EI
    bool        exi_log;
    bool        exi_osReport;
    TCHAR       ansiFilename[0x1000];
    TCHAR       sjisFilename[0x1000];

    // MC
    bool        MemcardA_Connected;
    bool        MemcardB_Connected;
    TCHAR       MemcardA_Filename[0x1000];
    TCHAR       MemcardB_Filename[0x1000];
    bool        Memcard_SyncSave;

    TCHAR       BootromFilename[0x1000];
    TCHAR       DspDromFilename[0x1000];
    TCHAR       DspIromFilename[0x1000];

} HWConfig;

// external interfaces (previously plugins)
#include "AX.h"
#include "GX.h"
#include "PAD.h"
#include "../DVD/DVD.h"

#include "../DSP/DspCore.h"

// hardware controls
#include "HW.h"

// GC hardware set (in register addressing order, see MI.h)
#include "EFB.h"
#include "AI.h"
#include "GDI.h"
#include "CP.h"
#include "VI.h"
#include "PI.h"
#include "MI.h"
#include "AR.h"
#include "DI.h"
#include "SI.h"
#include "EI.h"
#include "MC.h"
#include "IPLDescrambler.h"
