// common include project header

// size of DVD image
#define DVD_SIZE            0x57058000  // 1.4 GB

#include "../Hardware/DVD.h"

// other include files
#include "filesystem.h"     // DVD file system, based on hotquik's code from Dolwin 0.09

// all important data is placed here
typedef struct
{
    // DVD plugin settings
    int         frdm;       // file read mode,
                            // 0 : fread(1, size)
                            // 1 : fread(size, 1) <- default

    bool        selected;
} DVD;

extern DVD dvd;             // share with other modules
