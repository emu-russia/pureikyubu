// general include file for whole project. must be included first.

#ifndef __DOLPHIN_H__
#define __DOLPHIN_H__

// ---------------------------------------------------------------------------
// compiler and SDK include files.

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
#define APPVER      "0.10"

// ---------------------------------------------------------------------------
// Dolwin data types. please, try not to use various DWORDs, ulongs etc.,
// except Windows defined BOOL. let the source to be compatible with Nintendo's ;)

typedef signed   char       s8;
typedef signed   short      s16;
typedef signed   long       s32;
typedef signed   __int64    s64;
typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned long       u32;
typedef unsigned __int64    u64;
typedef float               f32;
typedef double              f64;

// just to show, that BOOL is common for use
#ifndef WIN32
typedef enum BOOL { FALSE, TRUE = !FALSE } BOOL;
#endif

// note : do not use C++ classes in Dolwin, because "this" pointer is
// making some pressure on x86 registers.

// do not use "inline" calls, they may be buggy in __fastcall functions.

// ---------------------------------------------------------------------------
// Dolwin includes, from higher to lower levels.

#include "User.h"
#include "HighLevel.h"
#include "Hardware.h"
#include "Emulator.h"

#endif  // __DOLPHIN_H__
