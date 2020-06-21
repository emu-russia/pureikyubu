// Emulator controls
#include "pch.h"

// emulator state
Emulator emu;

void EMUGetHwConfig(HWConfig * config)
{
    config->ramsize = RAMSIZE;
    config->hwndMain = wnd.hMainWindow;

    config->vi_log = GetConfigBool(USER_VI_LOG, USER_HW);
    config->vi_xfb = GetConfigBool(USER_VI_XFB, USER_HW);

    config->videoEncoderFuse = 0;

    config->rswhack = GetConfigBool(USER_PI_RSWHACK, USER_HW);
    config->consoleVer = GetConfigInt(USER_CONSOLE, USER_HW);

    config->exi_log = GetConfigBool(USER_EXI_LOG, USER_HW);
    config->exi_osReport = GetConfigBool(USER_OS_REPORT, USER_HW);

    _tcscpy_s (config->ansiFilename, _countof(config->ansiFilename) - 1, GetConfigString(USER_ANSI, USER_HW).data());
    _tcscpy_s (config->sjisFilename, _countof(config->sjisFilename) - 1, GetConfigString(USER_SJIS, USER_HW).data());

    config->MemcardA_Connected = GetConfigBool(MemcardA_Connected_Key, USER_MEMCARDS);
    config->MemcardB_Connected = GetConfigBool(MemcardB_Connected_Key, USER_MEMCARDS);
    _tcscpy_s (config->MemcardA_Filename, _countof(config->MemcardA_Filename) - 1, GetConfigString(MemcardA_Filename_Key, USER_MEMCARDS).data());
    _tcscpy_s (config->MemcardB_Filename, _countof(config->MemcardB_Filename) - 1, GetConfigString(MemcardB_Filename_Key, USER_MEMCARDS).data());
    config->Memcard_SyncSave = GetConfigBool(Memcard_SyncSave_Key, USER_MEMCARDS);

    _tcscpy_s (config->BootromFilename, _countof(config->BootromFilename) - 1, GetConfigString(USER_BOOTROM, USER_HW).data());
    _tcscpy_s (config->DspDromFilename, _countof(config->DspDromFilename) - 1, GetConfigString(USER_DSP_DROM, USER_HW).data());
    _tcscpy_s (config->DspIromFilename, _countof(config->DspIromFilename) - 1, GetConfigString(USER_DSP_IROM, USER_HW).data());
}

// this function calls every time, after user loading new file
void EMUOpen()
{
    if (emu.loaded)
        return;

    // open other sub-systems
    Gekko::Gekko->Reset();
    HWConfig* hwconfig = new HWConfig;
    memset(hwconfig, 0, sizeof(HWConfig));
    EMUGetHwConfig(hwconfig);
    Flipper::HW = new Flipper::Flipper(hwconfig);
    assert(Flipper::HW);
    delete hwconfig;

    ReloadFile();   // PC will be set here
    HLEOpen();

    // There is Fuse on the motherboard, which determines the video encoder mode. 
    // Some games test it in VIConfigure and try to set the mode according to Fuse. But the program code does not allow this (example - Zelda PAL Version)
    // https://www.ifixit.com/Guide/Nintendo+GameCube+Regional+Modification+Selector+Switch/35482
    if (ldat.dvd)
    {
        char id[4] = { 0 };

        id[0] = (char)ldat.gameID[0];
        id[1] = (char)ldat.gameID[1];
        id[2] = (char)ldat.gameID[2];
        id[3] = (char)ldat.gameID[3];

        DVD::Region region = DVD::RegionById(id);
        VISetEncoderFuse(DVD::IsNtsc(region) ? 0 : 1);
    }

    OnMainWindowOpened();

    emu.loaded = true;

    if (!emu.doldebug)
    {
        Gekko::Gekko->Run();
    }
}

// this function calls every time, after user stops emulation
void EMUClose()
{
    if (!emu.loaded)
        return;

    HLEClose();

    Gekko::Gekko->Suspend();

    delete Flipper::HW;
    Flipper::HW = nullptr;

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
    Gekko::Gekko = new Gekko::GekkoCore;
    assert(Gekko::Gekko);
    Debug::Hub.AddNode(EMU_JDI_JSON, EmuReflector);
    DSP::DspCore::InitSubsystem();
    DVD::InitSubsystem();
    HLEInit();
}

void EMUDtor()
{
    Debug::Hub.RemoveNode(EMU_JDI_JSON);
    DSP::DspCore::ShutdownSubsystem();
    DVD::ShutdownSubsystem();
    delete Gekko::Gekko;
    Gekko::Gekko = nullptr;
    HLEShutdown();
}
