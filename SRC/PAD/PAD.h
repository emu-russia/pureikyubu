// common include project header

// compiler and Windows API includes
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "../../RES/resource.h"

#include "../Hardware/PAD.h"

// other include files
#include "Configure.h"      // PAD configure dialog

// all important data is placed here
typedef struct
{
    int         padToConfigure;
    PADCONF     config[4];
} PAD;

extern PAD pad;             // share with other modules
