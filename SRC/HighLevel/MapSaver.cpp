// MAP saver is moved to stand-alone module, because MAP saving operation
// is not easy, like you may think. We should watch for MAP file formats
// and try to append symbols into alredy present MAP.
#include "pch.h"

using namespace Debug;

#define DEFAULT_MAP L"Data\\default.map"
//#define HEX "0x"
#define HEX

static MAP_FORMAT mapFormat;
static char *mapName;
static FILE* mapFile = NULL;
static bool appendStarted;
static int itemsUpdated;

static void AppendMAPBySymbol(uint32_t address, char *symbol)
{
    mapFile = fopen(mapName, "a");
    if(!mapFile) return;

    // linefeed
    if(!appendStarted)
    {
        appendStarted = 1;
        fprintf(mapFile, "\n");
    }

    if(mapFormat == MAP_FORMAT::RAW)
    {
        //80002300 Symbol
        // * or * (dont care)
        //0x80002300 Symbol
        fprintf(mapFile, HEX "%08X %s\n", address, symbol);
    }
    else if(mapFormat == MAP_FORMAT::CW)
    {
        //00000000 000000f0 80003100 0 __start
        // ignore size (set to 4)
        fprintf(mapFile, "00000000 00000004 %08X 0 %s\n", address, symbol);
    }
    else if(mapFormat == MAP_FORMAT::GCC)
    {
        //0x8000ab00 Symbol
        // its not clear for me, because its hotquik's stuff :o)
        fprintf(mapFile, "%08x %s\n", address, symbol);        
    }

    Report(Channel::HLE, "New map entry: %08X %s\n", address, symbol);
    itemsUpdated ++;
    fclose (mapFile);
}

// save whole symbolic information in map.
// there can be two cases of this call : save map into specified file and update current map
// if there is not map loaded, all new symbols will go in default.map
// saved map is appended (mean no file overwrite, and add new symbols to the end)
static void SaveMAP2(const wchar_t *mapname)
{
    static SYMControl temp;     // STATIC !
    SYMControl *thisSet = &sym, *mapSet = &temp;

    if(!mapname)
    {
        if(hle.mapfile[0] == 0)
        {
            Report(Channel::Error, "No map file loaded! Symbols will be saved to default map\n");
            mapname = DEFAULT_MAP;
        }
        else mapname = hle.mapfile;
    }

    Report(Channel::HLE, "Saving/updating map: %s ...\n\n", mapname);

    // load MAP symbols
    SYMSetWorkspace(mapSet);
    mapFormat = LoadMAP(mapname);
    if(mapFormat == MAP_FORMAT::BAD) return;  // :(

    // find new map entries to append file
    mapName = (char *)mapname;
    appendStarted = itemsUpdated = 0;
    SYMCompareWorkspaces(thisSet, mapSet, AppendMAPBySymbol);
    if ( itemsUpdated == 0 ) Report (Channel::HLE, "Nothing to update\n");

    // restore old workspace
    SYMKill();
    SYMSetWorkspace(thisSet);
}

void SaveMAP(const char* mapname)
{
    if (!mapname)
    {
        SaveMAP2(nullptr);
        return;
    }

    wchar_t wcharStr[0x1000] = { 0, };
    char* ansiPtr = (char*)mapname;
    wchar_t* wcharPtr = wcharStr;
    while (*ansiPtr)
    {
        *wcharPtr++ = *ansiPtr++;
    }
    *wcharPtr++ = 0;
    SaveMAP2(wcharStr);
}
