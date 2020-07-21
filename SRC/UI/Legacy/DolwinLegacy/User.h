// main include file for User-friendly stuff :)

#pragma once

// version info
constexpr auto APPNAME = L"Dolwin";
constexpr auto APPDESC = L"Nintendo Gamecube Emulator for Windows";
constexpr auto APPVER  = L"0.130";

#include "UserMain.h"           // application entrypoint
#include "UserFile.h"           // various file utilities
#include "UserConfig.h"         // Dolwin configuration
#include "UserSelector.h"       // file selector
#include "UserWindow.h"         // main window controls
#include "UserProfiler.h"       // emu profiler
#include "UserSettings.h"       // settings dialog
#include "UserFonts.h"          // bootrom fonts dialog
#include "UserMemcards.h"       // memcards dialog
#include "UserAbout.h"          // about dialog
#include "DVDBanner.h"			// banner utilities for selector
