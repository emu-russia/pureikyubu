// common include project header

// compiler and Windows API includes
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "resource.h"

#include "Hardware/PAD.h"

// other include files
#include "Configure.h"      // PAD configure dialog

// all important data is placed here
typedef struct
{
    HINSTANCE   inst;       // plugin dll handler
    HWND*       hwndParent; // main window

    int         padToConfigure;
    PADCONF     config[4];
} PAD;

extern PAD pad;             // share with other modules
