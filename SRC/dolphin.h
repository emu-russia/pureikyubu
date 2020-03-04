// general include file for whole project. must be included first.

#ifndef __DOLPHIN_H__
#define __DOLPHIN_H__

// ---------------------------------------------------------------------------
// compiler and SDK include files.

#include <stdint.h>
#include <direct.h>
#include <setjmp.h>
#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>
#include <commctrl.h>
#include "resource.h"

// ---------------------------------------------------------------------------
// compile-time triggers.

// version info, used everywhere in emu.
#define APPNAME     "Dolwin"
#define APPDESC     "Nintendo Gamecube Emulator for Windows"
#define APPVER      "0.11"

// ---------------------------------------------------------------------------
// Dolwin includes, from higher to lower levels.

#include "User.h"
#include "HighLevel.h"
#include "Hardware.h"
#include "Emulator.h"

#endif  // __DOLPHIN_H__
