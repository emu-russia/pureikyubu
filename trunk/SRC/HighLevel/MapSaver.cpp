// MAP saver is moved to stand-alone module, because MAP saving operation
// is not easy, like you may think. We should watch for MAP file formats
// and try to append symbols into alredy present MAP.
// Simple MAP overwrite is stupid.
#include "dolphin.h"

#define DEFAULT_MAP "Data\\default.map"
//#define HEX "0x"
#define HEX

static int mapFormat;
static char *mapName;
static FILE* mapFile;
static BOOL appendStarted;

static void AppendMAPBySymbol(u32 address, char *symbol)
{
    mapFile = fopen(mapName, "a");
    if(!mapFile) return;

    // linefeed
    if(!appendStarted)
    {
        appendStarted = 1;
        fprintf(mapFile, "\n");
    }

    if(mapFormat == MAP_FORMAT_RAW)
    {
        //80002300 Symbol
        // * or * (dont care)
        //0x80002300 Symbol
        fprintf(mapFile, HEX "%08X %s\n", address, symbol);
    }
    else if(mapFormat == MAP_FORMAT_CW)
    {
        //00000000 000000f0 80003100 0 __start
        // ignore size (set to 4)
        fprintf(mapFile, "00000000 00000004 %08X 0 %s\n", address, symbol);
    }
    else if(mapFormat == MAP_FORMAT_GCC)
    {
        //0x8000ab00 Symbol
        // its not clear for me, because its hotquik's stuff :o)
        fprintf(mapFile, "%08x %s\n", address, symbol);        
    }

    DBReport(YEL "New map entry: %08X %s\n", address, symbol);
}

// save whole symbolic information in map.
// there can be two cases of this call : save map into specified file and update current map
// if there is not map loaded, all new symbols will go in default.map
// saved map is appended (mean no file overwrite, and add new symbols to the end)
void SaveMAP(char *mapname /*="this"*/)
{
    static SYMControl temp;     // STATIC !
    SYMControl *thisSet = &sym, *mapSet = &temp;

    if(!stricmp(mapname, "this"))
    {
        if(hle.mapfile[0] == 0)
        {
            DBReport(BRED "No map file loaded! Symbols will be saved to " DEFAULT_MAP "\n");
            mapname = DEFAULT_MAP;
        }
        else mapname = hle.mapfile;
    }

    DBReport(YEL "saving/updating map : %s ...\n\n", mapname);

    // load MAP symbols
    SYMSetWorkspace(mapSet);
    mapFormat = LoadMAP(mapname);
    if(mapFormat == 0) return;  // :(

    // find new map entries to append file
    mapName = mapname;
    appendStarted = 0;
    SYMCompareWorkspaces(thisSet, mapSet, AppendMAPBySymbol);
    fflush(mapFile);

    // restore old workspace
    SYMKill();
    SYMSetWorkspace(thisSet);
}
