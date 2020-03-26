// Emulator controls
#include "dolphin.h"

// emulator state
Emulator emu;

// ---------------------------------------------------------------------------

void EMUGetHwConfig(HWConfig * config)
{
    config->ramsize = RAMSIZE;
    config->hwndMain = wnd.hMainWindow;

    config->vi_log = GetConfigInt(USER_VI_LOG, USER_HW) & 1;
    config->vi_xfb = GetConfigInt(USER_VI_XFB, USER_HW) & 1;
    config->vcount = GetConfigInt(USER_VI_COUNT, USER_HW);

    // There is Fuse on the motherboard, which determines the video encoder mode. 
    // Some games test it in VIConfigure and try to set the mode according to Fuse. But the program code does not allow this (example - Zelda PAL Version)
    // https://www.ifixit.com/Guide/Nintendo+GameCube+Regional+Modification+Selector+Switch/35482
    config->videoEncoderFuse = ldat.gameID[3] == 'P' ? 0 : 1;

    config->rswhack = GetConfigInt(USER_PI_RSWHACK, USER_HW) & 1;
    config->consoleVer = GetConfigInt(USER_CONSOLE, USER_HW);

    config->exi_log = GetConfigInt(USER_EXI_LOG, USER_HW) & 1;
    config->exi_osReport = GetConfigInt(USER_OS_REPORT, USER_HW) & 1;

    strcpy_s (config->ansiFilename, sizeof(config->ansiFilename), GetConfigString(USER_ANSI, USER_HW));
    strcpy_s (config->sjisFilename, sizeof(config->sjisFilename), GetConfigString(USER_SJIS, USER_HW));

    config->MemcardA_Connected = false;// GetConfigInt(MemcardA_Connected_Key, FALSE, HKEY_MEMCARD) & 1;
    config->MemcardB_Connected = false;// GetConfigInt(MemcardB_Connected_Key, FALSE, HKEY_MEMCARD) & 1;
    strcpy_s (config->MemcardA_Filename, sizeof(config->MemcardA_Filename), GetConfigString(MemcardA_Filename_Key, "*", HKEY_MEMCARD));
    strcpy_s (config->MemcardB_Filename, sizeof(config->MemcardB_Filename), GetConfigString(MemcardB_Filename_Key, "*", HKEY_MEMCARD));
    config->Memcard_SyncSave = GetConfigInt(Memcard_SyncSave_Key, FALSE, HKEY_MEMCARD) & 1;

    config->one_second = cpu.one_second;

    strcpy_s(config->BootromFilename, sizeof(config->BootromFilename), GetConfigString(USER_BOOTROM, USER_HW));
    strcpy_s(config->DspDromFilename, sizeof(config->DspDromFilename), GetConfigString(USER_DSP_DROM, USER_HW));
    strcpy_s(config->DspIromFilename, sizeof(config->DspIromFilename), GetConfigString(USER_DSP_IROM, USER_HW));
}

// this function calls every time, after user loading new file
void EMUOpen()
{
    if (emu.loaded)
        return;

    // open other sub-systems
    emu.core = new Gekko::GekkoCore;
    assert(emu.core);

    HWConfig* hwconfig = new HWConfig;
    memset(hwconfig, 0, sizeof(HWConfig));
    EMUGetHwConfig(hwconfig);
    emu.hw = new Flipper::Flipper(hwconfig);
    assert(emu.hw);
    delete hwconfig;

    ReloadFile();   // PC will be set here
    HLEOpen();

    OnMainWindowOpened();

    emu.loaded = true;

    if (!emu.doldebug)
    {
        emu.core->Run();
    }
}

// this function calls every time, after user stops emulation
void EMUClose()
{
    if (!emu.loaded)
        return;

    HLEClose();

    delete emu.core;
    emu.core = nullptr;

    delete emu.hw;
    emu.hw = nullptr;

    // take care about user interface
    OnMainWindowClosed();

    emu.loaded = false;
}

// reset emulator
void EMUReset()
{
    EMUClose();
    EMUOpen();
}
