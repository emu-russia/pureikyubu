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
    bool        vi_stretch;
    uint32_t    vcount;

    // PI
    uint32_t    consoleVer;
    bool        rswhack;

    // EI
    bool        exi_log;
    bool        exi_osReport;
    bool        exi_rtc;
    char        ansiFilename[0x1000];
    char        sjisFilename[0x1000];

    // MC
    bool        MemcardA_Connected;
    bool        MemcardB_Connected;
    char        MemcardA_Filename[0x1000];
    char        MemcardB_Filename[0x1000];
    bool        Memcard_SyncSave;

    // CP
    bool        gxpoll;         // 1: poll controllers after GX draw done

    int64_t     one_second;         // one CPU second in timer ticks

    char        BootromFilename[0x1000];
    char        DspDromFilename[0x1000];
    char        DspIromFilename[0x1000];

} HWConfig;

// external interfaces (previously plugins)
#include "AX.h"
#include "GX.h"
#include "PAD.h"
#include "DVD.h"

#include "DspCore.h"

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
#include "DspAnalyzer.h"
#include "DspInterpreter.h"
