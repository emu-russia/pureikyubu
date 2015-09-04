/*++

Copyright (c)

Module Name:

    dolphin.h

Abstract:

    General include file for the whole project. Must be included first.

--*/

#pragma once

#define XP_THEME

#ifdef XP_THEME
#pragma comment(linker, "\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif // XP_THEME

#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NON_CONFORMING_SWPRINTFS

//
// Compiler and SDK specific include files.
//

#include <Windows.h>
#include <Commctrl.h>
#include <tchar.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

//
// Compile-time triggers
//

// Version info, used everywhere in emu.
#define APPNAME     _T("Dolwin")
#define APPDESC     _T("Nintendo Gamecube Emulator for Windows")
#define APPVER      _T("0.11")

//
// Dolwin Subsystems
//

#include "Utils.h"
#include "Gui.h"
#include "Debugger.h"
