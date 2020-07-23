#pragma once

// Config

#include <tchar.h>

struct HWConfig
{
    // MI
    size_t      ramsize;

    // VI
    void* renderTarget;
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
