// Emulator controls
#include "dolphin.h"

// emulator state
Emulator emu;

// ---------------------------------------------------------------------------

void EMUGetHwConfig(HWConfig * config)
{
    config->ramsize = RAMSIZE;
    config->hwndMain = wnd.hMainWindow;

    config->vi_log = GetConfigInt(USER_VI_LOG, USER_VI_LOG_DEFAULT) & 1;
    config->vi_xfb = GetConfigInt(USER_VI_XFB, USER_VI_XFB_DEFAULT) & 1;
    config->vi_stretch = GetConfigInt(USER_VI_STRETCH, USER_VI_STRETCH_DEFAULT) & 1;
    config->vcount = GetConfigInt(USER_VI_COUNT, USER_VI_COUNT_DEFAULT);

    config->rswhack = GetConfigInt(USER_PI_RSWHACK, USER_PI_RSWHACK_DEFAULT) & 1;
    config->consoleVer = GetConfigInt(USER_CONSOLE, USER_CONSOLE_DEFAULT);

    config->exi_log = GetConfigInt(USER_EXI_LOG, USER_EXI_LOG_DEFAULT) & 1;
    config->exi_osReport = GetConfigInt(USER_OS_REPORT, USER_OS_REPORT_DEFAULT) & 1;
    config->exi_rtc = GetConfigInt(USER_RTC, USER_RTC_DEFAULT) & 1;

    strcpy_s (config->ansiFilename, sizeof(config->ansiFilename), GetConfigString(USER_ANSI, USER_ANSI_DEFAULT));
    strcpy_s (config->sjisFilename, sizeof(config->sjisFilename), GetConfigString(USER_SJIS, USER_SJIS_DEFAULT));

    config->MemcardA_Connected = GetConfigInt(MemcardA_Connected_Key, FALSE, HKEY_MEMCARD) & 1;
    config->MemcardB_Connected = GetConfigInt(MemcardB_Connected_Key, FALSE, HKEY_MEMCARD) & 1;
    strcpy_s (config->MemcardA_Filename, sizeof(config->MemcardA_Filename), GetConfigString(MemcardA_Filename_Key, "*", HKEY_MEMCARD));
    strcpy_s (config->MemcardB_Filename, sizeof(config->MemcardB_Filename), GetConfigString(MemcardB_Filename_Key, "*", HKEY_MEMCARD));
    config->Memcard_SyncSave = GetConfigInt(Memcard_SyncSave_Key, FALSE, HKEY_MEMCARD) & 1;

    config->gxpoll = GetConfigInt(USER_GX_POLL, USER_GX_POLL_DEFAULT) & 1;

    config->one_second = cpu.one_second;

    strcpy_s(config->BootromFilename, sizeof(config->BootromFilename), GetConfigString(USER_BOOTROM, USER_BOOTROM_DEFAULT));
    strcpy_s(config->DspDromFilename, sizeof(config->DspDromFilename), GetConfigString(USER_DSP_DROM, USER_DSP_DROM_DEFAULT));
    strcpy_s(config->DspIromFilename, sizeof(config->DspIromFilename), GetConfigString(USER_DSP_IROM, USER_DSP_IROM_DEFAULT));
}

// this function is called once, during Dolwin life-time
void EMUInit()
{
    assert(!emu.loaded);
    if(emu.initok == true) return;

    MEMOpen(GetConfigInt(USER_MMU, USER_MMU_DEFAULT));
    MEMSelect(0, 0);

    emu.initok = true;
}

// this function is called last, during Dolwin life-time
void EMUDie()
{
    assert(!emu.loaded);
    if(emu.initok == false) return;

    emu.initok = false;
}

// this function calls every time, after user loading new file
void EMUOpen()
{
    if (emu.loaded)
        return;

    OnMainWindowOpened();

    // open other sub-systems
    MEMOpen(GetConfigInt(USER_MMU, USER_MMU_DEFAULT));
    MEMSelect(mem.mmu, 0);
    emu.core = new Gekko::GekkoCore;
    assert(emu.core);

    HWConfig* hwconfig = new HWConfig;
    memset(hwconfig, 0, sizeof(HWConfig));
    EMUGetHwConfig(hwconfig);
    HWOpen(hwconfig);
    delete hwconfig;

    assert(GXOpen(mi.ram, wnd.hMainWindow));
    assert(AXOpen());
    assert(PADOpen());

    ReloadFile();   // PC will be set here
    HLEOpen();

    emu.loaded = true;

    emu.core->Run();
}

// this function calls every time, after user stops emulation
void EMUClose()
{
    if (!emu.loaded)
        return;

    delete emu.core;
    emu.core = nullptr;

    // close other sub-systems
    PADClose();
    AXClose();
    GXClose();
    HLEClose();
    HWClose();
    MEMClose();

    // take care about user interface
    OnMainWindowClosed();

    emu.loaded = false;
}

// you can use EMUClose(), then EMUOpen() to reset emulator, 
// and reload last used file.
