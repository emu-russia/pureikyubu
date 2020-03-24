// DVD API for emulator
#include "pch.h"

DVD dvd;

// ---------------------------------------------------------------------------
// select current dvd 

bool DVDSetCurrent(char *file)
{
    // try to open file
    FILE* f = nullptr;
    fopen_s(&f, file, "rb");
    if(!f) return false;
    fclose(f);

    // select current DVD
    bool res = GCMSelectFile(file);
    if (!res)
        return res;

    // init filesystem
    res = dvd_fs_init();
    if (!res)
    {
        GCMSelectFile(nullptr);
        return res;
    }

    return true;
}

// ---------------------------------------------------------------------------
// dvd operations on current dvd

void DVDSeek(int position)
{
    // DVD is not selected
    if (!dvd.selected) return;

    GCMSeek(position);
}

void DVDRead(void *buffer, int length)
{
    // DVD is not selected
    if (!dvd.selected)
    {
        memset(buffer, 0, length);        // fill by zeroes
        return;
    }

    GCMRead((uint8_t *)buffer, length);
}

long DVDOpenFile(const char *dvdfile)
{
    // DVD is not selected
    if(!dvd.selected) return 0;

    // call DVD filesystem open
    return dvd_open(dvdfile);
}
