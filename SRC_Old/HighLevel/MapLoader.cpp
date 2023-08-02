// MAP files loader. currently there are support for three MAP file formats: 
// Dolwin custom ("RAW"), CodeWarrior and GCC-like.
#include "pch.h"

using namespace Debug;

// load CodeWarrior-generated map file
// thanks Dolphin team for idea
static MAP_FORMAT LoadMapCW(const wchar_t *mapname)
{
    bool    started = false;
    char    buf[1024], token1[256];
    FILE    *map;
    
    // symbol information
    uint32_t  moduleOffset, procSize, procAddr;
    int     flags;
    char    procName[512];

    map = fopen ( Util::WstringToString(mapname).c_str(), "r");
    if(!map) return MAP_FORMAT::BAD;

    while(!feof(map))
    {
        fgets(buf, 1024, map);
        sscanf(buf, "%s", token1);

        // check section type (we need only code sections)
        if(!strcmp(buf, ".init section layout\n")) { started = true; continue; }
        if(!strcmp(buf, ".text section layout\n")) { started = true; continue; }
        if(!strcmp(buf, ".data section layout\n")) break;

        // check first token
        #define IFIS(str) if(!strcmp(token1, #str)) continue;
        IFIS(Starting);
        IFIS(address);
        IFIS(-----------------------);
        IFIS(UNUSED);

        if(token1[strlen(token1) - 1] == ']') continue;
        if(started == false) continue;

        // parse symbols
        if(sscanf(buf, "%08x %08x %08x %i %s", 
            &moduleOffset, &procSize, &procAddr,
            &flags,
            procName) != 5) continue;

        if(flags != 1)
        {
            SYMAddNew(procAddr, procName);
        }
    }

    fclose(map);

    Report(Channel::HLE, "CodeWarrior format map loaded: %s\n\n", Util::WstringToString(mapname).c_str());
    return MAP_FORMAT::CW;
}

// load GCC-generated map file
static MAP_FORMAT LoadMapGCC(const wchar_t *mapname)
{
    bool    started = false;
    char    buf[1024];
    FILE    *map;
    
    // symbol information
    uint32_t     procAddr;
    char    par1[512];
    char    par2[512];

    map = fopen ( Util::WstringToString(mapname).c_str(), "r");
    if(!map) return MAP_FORMAT::BAD;

    while(!feof(map))
    {
        fgets(buf, 1024, map);

        // parse symbols
        if(sscanf(buf, "%s %s", par1, par2) != 2) continue;

        if(strcmp(par1, ".init") == 0) { started = true; continue; }
        if(strcmp(par1, ".text") == 0) { started = true; continue; }
        if(par1[0] == '.')  { started = false; continue; }

        if(started)
        {
            if (par1[0] == '0' && par1[1] == 'x') {
                sscanf(&par1[2], "%08x", &procAddr);
                SYMAddNew(procAddr, par2);
            }
        }
    }

    fclose(map);

    Report(Channel::HLE, "GCC format map loaded: %s\n\n", Util::WstringToString(mapname).c_str());
    return MAP_FORMAT::GCC;
}

// load Dolwin format map-file
static MAP_FORMAT LoadMapRAW(const wchar_t *mapname)
{
    /* Open the map file. */
    auto file = std::ifstream( Util::WstringToString(mapname).c_str());
    if (!file.is_open())
    {
        throw std::exception();
    }

    /* Read all the symbols with their addresses. */
    auto address = 0U;
    auto symbol = std::string();
    while (file >> std::hex >> address >> symbol)
    {
        SYMAddNew(address, symbol.c_str());
    }

    Report(Channel::HLE, "RAW format map loaded: %s\n\n", Util::WstringToString(mapname).c_str());
    return MAP_FORMAT::RAW;
}

// wrapper for all map formats.
MAP_FORMAT LoadMAP(const wchar_t *mapname, bool add)
{
    FILE *f;
    char sign[256];

    // delete previous MAP symbols?
    if(!add)
    {
        SYMKill();
    }

    // copy name for MAP saver (with SaveMAP "this" parameter)
    wcscpy(hle.mapfile, mapname);

    // try to open
    f = fopen ( Util::WstringToString(mapname).c_str(), "r");
    if(!f)
    {
        Report(Channel::HLE, "Cannot %s MAP: %s\n", (add) ? "add" : "load", Util::WstringToString(mapname).c_str());
        hle.mapfile[0] = 0;
        return MAP_FORMAT::BAD;
    }

    // recognize map format
    fread(sign, 1, 256, f);
    fclose(f);

    MAP_FORMAT format;
    if(!strncmp(sign, "Link map", 8)) format = LoadMapCW(mapname);
    else if(!strncmp(sign, "Archive member", 14)) format = LoadMapGCC(mapname);
    else format = LoadMapRAW(mapname);

    if(format == MAP_FORMAT::BAD)
    {
        hle.mapfile[0] = 0;
    }
    return format;
}

MAP_FORMAT LoadMAP(const char* mapname, bool add)
{
    wchar_t wcharStr[0x1000] = { 0, };
    wchar_t* wcharPtr = wcharStr;
    char* charPtr = (char*)mapname;

    while (*charPtr)
    {
        *wcharPtr++ = *charPtr++;
    }
    *wcharPtr++ = 0;

    return LoadMAP(wcharStr, add);
}
