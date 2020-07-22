// GC hardware includes

#pragma once

// Config from Ui

#include <Windows.h>

struct HWConfig
{
    // MI
    size_t      ramsize;

    // VI
	HWND	    hwndMain;           // TODO: Redesign the video system so that all rendering is done only in memory (algorithmically) and get rid of the platform (Windows)
    bool        vi_log;
    bool        vi_xfb;
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

};

// Backends
#include "GX.h"
#include "../Backends/PadSimpleWin32/PAD.h"
#include "../Backends/VideoGdi/GDI.h"

#include "../DVD/DVD.h"
#include "../DSP/DspCore.h"
#include "HW.h"
#include "EFB.h"
#include "AI.h"
#include "CP.h"
#include "VI.h"
#include "PI.h"
#include "MI.h"
#include "AR.h"
#include "DI.h"
#include "SI.h"
#include "EXI.h"
#include "MC.h"
#include "IPLDescrambler.h"
