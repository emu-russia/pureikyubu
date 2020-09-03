/* Emulator controls */
#include "pch.h"

/* Emulator state */
Emulator emu;

void EMUGetHwConfig(HWConfig * config)
{
    memset(config, 0, sizeof(HWConfig));

    Json::Value* renderTarget = JDI::Hub.ExecuteFast("GetRenderTarget");

    if (renderTarget == nullptr)
    {
        throw "GetRenderTarget failed!";
    }

    config->renderTarget = (void *)renderTarget->value.AsInt;
    delete renderTarget;

    config->ramsize = RAMSIZE;

    config->vi_log = GetConfigBool(USER_VI_LOG, USER_HW);
    config->vi_xfb = GetConfigBool(USER_VI_XFB, USER_HW);

    config->videoEncoderFuse = 0;

    config->rswhack = GetConfigBool(USER_PI_RSWHACK, USER_HW);
    config->consoleVer = GetConfigInt(USER_CONSOLE, USER_HW);

    config->exi_log = GetConfigBool(USER_EXI_LOG, USER_HW);
    config->exi_osReport = GetConfigBool(USER_OS_REPORT, USER_HW);

    wcscpy (config->ansiFilename, GetConfigString(USER_ANSI, USER_HW));
    wcscpy (config->sjisFilename, GetConfigString(USER_SJIS, USER_HW));

    config->MemcardA_Connected = GetConfigBool(MemcardA_Connected_Key, USER_MEMCARDS);
    config->MemcardB_Connected = GetConfigBool(MemcardB_Connected_Key, USER_MEMCARDS);
    wcscpy (config->MemcardA_Filename, GetConfigString(MemcardA_Filename_Key, USER_MEMCARDS));
    wcscpy (config->MemcardB_Filename, GetConfigString(MemcardB_Filename_Key, USER_MEMCARDS));
    config->Memcard_SyncSave = GetConfigBool(Memcard_SyncSave_Key, USER_MEMCARDS);

    if (!Util::FileExists(config->MemcardA_Filename))
    {
        config->MemcardA_Connected = false;
    }

    if (!Util::FileExists(config->MemcardB_Filename))
    {
        config->MemcardB_Connected = false;
    }

    wcscpy (config->BootromFilename, GetConfigString(USER_BOOTROM, USER_HW));
    wcscpy (config->DspDromFilename, GetConfigString(USER_DSP_DROM, USER_HW));
    wcscpy (config->DspIromFilename, GetConfigString(USER_DSP_IROM, USER_HW));
}

// this function calls every time, after user loading new file
void EMUOpen(const std::wstring& filename)
{
    if (emu.loaded)
    {
        return;
    }

    Debug::Log = new Debug::EventLog();

    // open other sub-systems
    Gekko::Gekko->Reset();
    HWConfig* hwconfig = new HWConfig;
    EMUGetHwConfig(hwconfig);
    Flipper::HW = new Flipper::Flipper(hwconfig);
    delete hwconfig;

    LoadFile(filename);   // Gekko PC will be set here
    HLEOpen();

    Debug::g_PerfCounters->ResetAllCounters();

    emu.loaded = true;
    emu.lastLoaded = filename;
}

// this function calls every time, after user stops emulation
void EMUClose()
{
    if (!emu.loaded)
    {
        return;
    }

    HLEClose();

    Gekko::Gekko->Suspend();
	Gekko::Gekko->Reset();

    delete Flipper::HW;
    Flipper::HW = nullptr;

    delete Debug::Log;
    Debug::Log = nullptr;

	DVD::Unmount();

    emu.loaded = false;
}

// reset emulator
void EMUReset()
{
    bool runningBefore = Gekko::Gekko->IsRunning();
    EMUClose();
    EMUOpen(emu.lastLoaded);
    if (runningBefore)
    {
        EMURun();
    }
}

void EMUCtor()
{
    if (emu.init)
    {
        return;
    }
    JDI::Hub.AddNode(DEBUGGER_JDI_JSON, Debug::Reflector);
    Gekko::Gekko = new Gekko::GekkoCore();
    Flipper::DSP = new DSP::Dsp16();
    Flipper::Gx = new GX::GXCore();
    JDI::Hub.AddNode(EMU_JDI_JSON, EmuReflector);
    DVD::InitSubsystem();
    HLEInit();
    Debug::g_PerfCounters = new Debug::PerfCounters();
    emu.init = true;
}

void EMUDtor()
{
    if (!emu.init)
    {
        return;
    }
    JDI::Hub.RemoveNode(EMU_JDI_JSON);
    DVD::ShutdownSubsystem();
    delete Gekko::Gekko;
    Gekko::Gekko = nullptr;
    delete Flipper::DSP;
    Flipper::DSP = nullptr;
    delete Flipper::Gx;
    Flipper::Gx = nullptr;
    HLEShutdown();
    JDI::Hub.RemoveNode(DEBUGGER_JDI_JSON);
    delete Debug::g_PerfCounters;
    emu.init = false;
}

/// <summary>
/// Run Gekko
/// </summary>
void EMURun()
{
    if (!emu.loaded)
    {
        return;
    }

    if (!Gekko::Gekko->IsRunning())
    {
        Gekko::Gekko->Run();
    }
}

/// <summary>
/// Stop Gekko
/// </summary>
void EMUStop()
{
    if (!emu.loaded)
    {
        return;
    }

    if (Gekko::Gekko->IsRunning())
    {
        Gekko::Gekko->Suspend();
    }
}
