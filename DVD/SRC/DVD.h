// common include project header

// plugin version
#define DVD_VER         "0.1"

// compiler and Windows API includes
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

// length of DVD image
#define DVD_SIZE            0x57058000  // 1.4 GB

// Dolwin plugin specifications. we need only DVD.
#include "DolwinPluginSpecs.h"

// other include files
#include "filesystem.h"     // DVD file system, based on hotquik's code from Dolwin 0.09
#include "GCM.h"            // very simple GCM reading (for .gcm files)

// all important data is placed here
typedef struct
{
    HINSTANCE   inst;       // plugin dll handler
    HWND*       hwndMain;   // emulator's main window

    // callbacks for selected format
    BOOL        (*select)(char *);
    void        (*seek)(int);
    void        (*read)(uint8_t *, int);
    void        (*close)();

    // DVD plugin settings
    int         frdm;       // file read mode,
                            // 0 : fread(1, size)
                            // 1 : fread(size, 1) <- default

    bool        selected;
} DVD;

extern DVD dvd;             // share with other modules
