// common include project header

// plugin version
#define DVD_VER         "0.1"

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

// length of DVD image
#define DVD_SIZE            0x57058000  // 1.4 GB

// Dolwin plugin specifications. we need only DVD.
#include "DolwinPluginSpecs.h"

// other include files
#include "Configure.h"      // DVD configure dialog
#include "filesystem.h"     // DVD file system, based on hotquik's code from Dolwin 0.09
#include "GCM.h"            // very simple GCM reading (for .gcm files)
#include "GMP.h"            // LZ compressed GCMs, used by Dolwin (.gmp files)

// list of supported formats for DVD images
enum
{
    DVD_FMT_GCM = 1,        // usual .gcm
    DVD_FMT_GCMP            // Dolwin compressed format
};

// all important data is placed here
typedef struct
{
    HINSTANCE   inst;       // plugin dll handler
    HWND*       hwndMain;   // emulator's main window

    int         format;     // see DVD_FMT* (0 = not selected)

    // callbacks for selected format
    BOOL        (*select)(char *);
    void        (*seek)(long);
    void        (*read)(u8 *, long);
    void        (*close)();

    // DVD plugin settings
    int         frdm;       // file read mode,
                            // 0 : fread(1, size)
                            // 1 : fread(size, 1) <- default
} DVD;

extern DVD dvd;             // share with other modules
