// Plugin API for emulator
#include "DVD.h"

DVD dvd;

// ---------------------------------------------------------------------------
// run-time initializations

// dll entrypoint
BOOL APIENTRY DllMain(HANDLE hModule, DWORD dwReasonForCall, LPVOID lpUnknown)
{
    if(dwReasonForCall == DLL_PROCESS_ATTACH)
    {
        dvd.inst = (HINSTANCE)hModule;
    }
    return TRUE;
}

// make sure, that emulator is using correct DVD plugin
void RegisterPlugin(PluginData * plugData)
{
    // save main window handler
    dvd.hwndMain = (HWND *)plugData->display;

    plugData->type = DOL_PLUG_DVD;
    plugData->version = "GC DVD Plugin (GCM, GMP)" " (" DVD_VER ")";
}

// ---------------------------------------------------------------------------
// called when emulation started/stopped (dvd controls)

long DVDOpen()
{
    // nothing to do
    return TRUE;    // ok
}

void DVDClose()
{
    // nothing to do
}

// ---------------------------------------------------------------------------
// select current dvd 

// return one of DVD_FMT*
static int get_file_format(char *filename, FILE *f)
{
    if(f == NULL) return DVD_FMT_GCM;

    // check for GMP
    char sign[16];
    fread(sign, 16, 1, f);
    fseek(f, 0, SEEK_SET);
    if(!strcmp(sign, GMP_ID)) return DVD_FMT_GCMP;

    // check for GCM
    // only GCM files can be checked by extension
    // no lamer ISOs
    char drive[_MAX_DRIVE+1],
         dir[_MAX_DIR+1],
         name[_MAX_FNAME+1],
         ext[_MAX_EXT+1];
    _splitpath(filename, drive, dir, name, ext);
    if(!stricmp(ext, ".gcm")) return DVD_FMT_GCM;

    // unknown / unsupported DVD-image format
    return 0;
}

long DVDSetCurrent(char *file)
{
    // close previously selected file
    if(dvd.close) dvd.close();

    // try to open file
    FILE *f = fopen(file, "rb");
    if(!f) return FALSE;

    // select format
    dvd.format = get_file_format(file, f);
    fclose(f);

    // set callbacks
    switch(dvd.format)
    {
        case DVD_FMT_GCM:       // GCM
            dvd.select = GCMSelectFile;
            dvd.seek   = GCMSeek;
            dvd.read   = GCMRead;
            dvd.close  = GCMClose;
            break;

        case DVD_FMT_GCMP:      // Dolwin GMP
            dvd.select = GCMPSelectFile;
            dvd.seek   = GCMPSeek;
            dvd.read   = GCMPRead;
            dvd.close  = GCMPClose;
            break;

        // add your formats here
        // ..

        // unsupported
        default:
            return FALSE;
    }

    // select current DVD
    BOOL res = dvd.select(file);

    // init filesystem
    if(res)
    {
        res = dvd_fs_init();
    }

    if(!res) dvd.format = 0;    // failed
    return res;
}

// return TRUE, if DVD image is compressed
long DVDIsCompressed(char *file)
{
    FILE * f = fopen(file, "rb");
    int fmt = get_file_format(file, f);
    if(f) fclose(f);

    if(fmt > DVD_FMT_GCM) return TRUE;
    else return FALSE;
}

// ---------------------------------------------------------------------------
// dvd operations on current dvd

void DVDSeek(s32 position)
{
    // DVD is not selected
    if(dvd.format == 0) return; 

    dvd.seek(position);
}

void DVDRead(void *buffer, s32 length)
{
    // DVD is not selected
    if(dvd.format == 0) return; 

    dvd.read((u8 *)buffer, length);
}

long DVDOpenFile(char *dvdfile)
{
    // DVD is not selected
    if(dvd.format == 0) return 0; 

    // call DVD filesystem open
    return dvd_open(dvdfile);
}

// ---------------------------------------------------------------------------
// save/load (currently there is nothing to save)

void DVDSaveLoad(long flag, char *filename)
{
    // empty
}
