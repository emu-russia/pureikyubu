// main include file for User-friendly stuff :)
// seriously, User Interface is just "front-end" for user variables.

#pragma once

#include "UserSpinLock.h"		// Spinlocks for multithreaded environment
#include "UserMain.h"           // application entrypoint
#include "UserFile.h"           // various file utilities
#include "UserConfig.h"         // Dolwin configuration, GAMES.ini
#include "UserSelector.h"       // file selector
#include "UserWindow.h"         // main window controls
#include "UserProfiler.h"       // emu profiler
#include "UserSettings.h"       // settings dialog
#include "UserFonts.h"          // bootrom fonts dialog
#include "UserMemcards.h"       // memcards dialog
#include "UserAbout.h"          // about dialog
#include "DVDBanner.h"			// banner utilities for selector
#include "Loader.h"				// GC file loader
#include "Emulator.h"			// emu control
