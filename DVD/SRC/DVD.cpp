// Plugin API for emulator
#include "DVD.h"

DVD dvd;

// ---------------------------------------------------------------------------
// select current dvd 

long DVDSetCurrent(char *file)
{
    // close previously selected file
    if(dvd.close) dvd.close();

    // try to open file
    FILE *f = fopen(file, "rb");
    if(!f) return FALSE;
    fclose(f);

    dvd.select = GCMSelectFile;
    dvd.seek   = GCMSeek;
    dvd.read   = GCMRead;
    dvd.close  = GCMClose;

    // select current DVD
    BOOL res = dvd.select(file);

    // init filesystem
    if(res)
    {
        res = dvd_fs_init();
    }

    if (!res)
        return res;

    dvd.selected = true;
    return res;
}

// ---------------------------------------------------------------------------
// dvd operations on current dvd

void DVDSeek(int position)
{
    // DVD is not selected
    if (!dvd.selected) return;

    dvd.seek(position);
}

void DVDRead(void *buffer, int length)
{
    // DVD is not selected
    if(!dvd.selected) return;

    dvd.read((uint8_t *)buffer, length);
}

long DVDOpenFile(char *dvdfile)
{
    // DVD is not selected
    if(!dvd.selected) return 0;

    // call DVD filesystem open
    return dvd_open(dvdfile);
}
