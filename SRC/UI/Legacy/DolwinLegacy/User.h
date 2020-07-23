// main include file for User-friendly stuff :)

#pragma once

// version info
#define APPNAME _T("Dolwin")
#define APPDESC _T("Nintendo Gamecube Emulator for Windows")

// Will be derived from JDI
//#define APPVER  _T("0.131")

#include "UserMain.h"           // application entrypoint
#include "UserJdiClient.h"
#include "UserConfig.h"
#include "UserFile.h"           // various file utilities
#include "UserSelector.h"       // file selector
#include "UserWindow.h"         // main window controls
#include "UserSettings.h"       // settings dialog
#include "UserFonts.h"          // bootrom fonts dialog
#include "UserMemcards.h"       // memcards dialog
#include "UserPadConfigure.h"
#include "UserAbout.h"          // about dialog
#include "DVDBanner.h"			// banner utilities for selector
#include "UserDebugStubs.h"
