#pragma once

// Config

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
    wchar_t     ansiFilename[0x1000];
    wchar_t     sjisFilename[0x1000];

    // MC
    bool        MemcardA_Connected;
    bool        MemcardB_Connected;
    wchar_t     MemcardA_Filename[0x1000];
    wchar_t     MemcardB_Filename[0x1000];
    bool        Memcard_SyncSave;

    wchar_t     BootromFilename[0x1000];
    wchar_t     DspDromFilename[0x1000];
    wchar_t     DspIromFilename[0x1000];

};
