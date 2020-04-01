// Emulator controls
#include "dolphin.h"

// emulator state
Emulator emu;

void EMUGetHwConfig(HWConfig * config)
{
    config->ramsize = RAMSIZE;
    config->hwndMain = wnd.hMainWindow;

    config->vi_log = GetConfigBool(USER_VI_LOG, USER_HW);
    config->vi_xfb = GetConfigBool(USER_VI_XFB, USER_HW);
    config->vcount = GetConfigInt(USER_VI_COUNT, USER_HW);

    // There is Fuse on the motherboard, which determines the video encoder mode. 
    // Some games test it in VIConfigure and try to set the mode according to Fuse. But the program code does not allow this (example - Zelda PAL Version)
    // https://www.ifixit.com/Guide/Nintendo+GameCube+Regional+Modification+Selector+Switch/35482
    config->videoEncoderFuse = ldat.gameID[3] == 'P' ? 0 : 1;

    config->rswhack = GetConfigBool(USER_PI_RSWHACK, USER_HW);
    config->consoleVer = GetConfigInt(USER_CONSOLE, USER_HW);

    config->exi_log = GetConfigBool(USER_EXI_LOG, USER_HW);
    config->exi_osReport = GetConfigBool(USER_OS_REPORT, USER_HW);

    _tcscpy_s (config->ansiFilename, _countof(config->ansiFilename) - 1, GetConfigString(USER_ANSI, USER_HW));
    _tcscpy_s (config->sjisFilename, _countof(config->sjisFilename) - 1, GetConfigString(USER_SJIS, USER_HW));

    config->MemcardA_Connected = GetConfigBool(MemcardA_Connected_Key, USER_MEMCARDS);
    config->MemcardB_Connected = GetConfigBool(MemcardB_Connected_Key, USER_MEMCARDS);
    _tcscpy_s (config->MemcardA_Filename, _countof(config->MemcardA_Filename) - 1, GetConfigString(MemcardA_Filename_Key, USER_MEMCARDS));
    _tcscpy_s (config->MemcardB_Filename, _countof(config->MemcardB_Filename) - 1, GetConfigString(MemcardB_Filename_Key, USER_MEMCARDS));
    config->Memcard_SyncSave = GetConfigBool(Memcard_SyncSave_Key, USER_MEMCARDS);

    config->one_second = cpu.one_second;

    _tcscpy_s (config->BootromFilename, _countof(config->BootromFilename) - 1, GetConfigString(USER_BOOTROM, USER_HW));
    _tcscpy_s (config->DspDromFilename, _countof(config->DspDromFilename) - 1, GetConfigString(USER_DSP_DROM, USER_HW));
    _tcscpy_s (config->DspIromFilename, _countof(config->DspIromFilename) - 1, GetConfigString(USER_DSP_IROM, USER_HW));
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

void EMUCtor()
{
    Debug::Hub.AddNode(EMU_JDI_JSON, EmuReflector);
    DSP::DspCore::InitSubsystem();
    DVD::InitSubsystem();
}

void EMUDtor()
{
    Debug::Hub.RemoveNode(EMU_JDI_JSON);
    DSP::DspCore::ShutdownSubsystem();
    DVD::ShutdownSubsystem();
}
