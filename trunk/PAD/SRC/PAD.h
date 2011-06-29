// common include project header

// plugin version
#define PAD_VER         "0.7"

// compiler and Windows API includes
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "resource.h"

// GC data types.
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

// Dolwin plugin specifications. we need only PAD.
#include "DolwinPluginSpecs.h"

// other include files
#include "Configure.h"      // PAD configure dialog

// all important data is placed here
typedef struct
{
    HINSTANCE   inst;       // plugin dll handler
    HWND*       hwndParent; // main window

    u32         rumbleFlag[4];
    int         padToConfigure;
    PADCONF     config[4];
} PAD;

extern PAD pad;             // share with other modules
