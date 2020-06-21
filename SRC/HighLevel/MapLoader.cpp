// MAP files loader. currently there are support for three MAP file formats: 
// Dolwin custom ("RAW"), CodeWarrior and GCC-like.
#include "pch.h"

// load CodeWarrior-generated map file
// thanks Dolphin team for idea
static MAP_FORMAT LoadMapCW(const TCHAR *mapname)
{
    BOOL    started = FALSE;
    char    buf[1024], token1[256];
    FILE    *map;
    
    // symbol information
    uint32_t  moduleOffset, procSize, procAddr;
    int     flags;
    char    procName[512];

    _tfopen_s(&map, mapname, _T("r"));
    if(!map) return MAP_FORMAT::BAD;

    while(!feof(map))
    {
        fgets(buf, 1024, map);
        sscanf(buf, "%s", token1);

        // check section type (we need only code sections)
        if(!strcmp(buf, ".init section layout\n")) { started = TRUE; continue; }
        if(!strcmp(buf, ".text section layout\n")) { started = TRUE; continue; }
        if(!strcmp(buf, ".data section layout\n")) break;

        // check first token
        #define IFIS(str) if(!strcmp(token1, #str)) continue;
        IFIS(Starting);
        IFIS(address);
        IFIS(-----------------------);
        IFIS(UNUSED);

        if(token1[strlen(token1) - 1] == ']') continue;
        if(started == FALSE) continue;

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

    DBReport2(DbgChannel::HLE, "CodeWarrior format map loaded: %s\n\n", Debug::Hub.TcharToString((TCHAR*)mapname).c_str());
    return MAP_FORMAT::CW;
}

// load GCC-generated map file
static MAP_FORMAT LoadMapGCC(const TCHAR *mapname)
{
    BOOL    started = FALSE;
    char    buf[1024];
    FILE    *map;
    
    // symbol information
    uint32_t     procAddr;
    char    par1[512];
    char    par2[512];

    _tfopen_s(&map, mapname, _T("r"));
    if(!map) return MAP_FORMAT::BAD;

    while(!feof(map))
    {
        fgets(buf, 1024, map);

        // parse symbols
        if(sscanf(buf, "%s %s", par1, par2) != 2) continue;

        if(strcmp(par1, ".init") == 0) { started = TRUE; continue; }
        if(strcmp(par1, ".text") == 0) { started = TRUE; continue; }
        if(par1[0] == '.')  { started = FALSE; continue; }

        if(started)
        {
            if (par1[0] == '0' && par1[1] == 'x') {
                sscanf(&par1[2], "%08x", &procAddr);
                SYMAddNew(procAddr, par2);
            }
        }
    }

    fclose(map);

    DBReport2(DbgChannel::HLE, "GCC format map loaded: %s\n\n", Debug::Hub.TcharToString((TCHAR*)mapname).c_str());
    return MAP_FORMAT::GCC;
}

// load Dolwin format map-file
static MAP_FORMAT LoadMapRAW(const TCHAR *mapname)
{
    int i;
    auto mapbuf = UI::FileLoad(mapname);

    // remove all garbage, like tabs
    for(i = 0; i < mapbuf.size(); i++)
    {
        char c = mapbuf[i];
        if (c < ' ') 
        { 
            c = '\n'; 
        }
    }

    char *ptr = (char*)mapbuf.data();
    while(*ptr)
    {
        // some maps has really *huge* symbols
        char line[0x1000];
        line[i = 0] = 0;

        // cut string
        while(*ptr == '\n') ptr++;
        if(!*ptr) break;
        while(*ptr != '\n') line[i++] = *ptr++;
        line[i++] = 0;

        // remove comments
        char *p = line;
        while(*p)
        {
            if(p[0] == '/' && p[1] == '/')
            {
                *p = 0;
                break;
            }
            p++;
        }

        // remove spaces at the end
        p = &line[strlen(line) - 1];
        while(*p <= ' ') p--;
        if(*p) p[1] = 0;

        // remove spaces at the beginning
        p = line;
        while(*p <= ' ' && *p) p++;

        // empty string ?
        if(!*p) continue;

        // add symbol
        char *name;
        uint32_t addr = strtoul(p, &name, 16);
        while(*name <= ' ') name++;
        SYMAddNew(addr, name);
    }

    DBReport2(DbgChannel::HLE, "RAW format map loaded: %s\n\n", Debug::Hub.TcharToString((TCHAR*)mapname).c_str());
    return MAP_FORMAT::RAW;
}

// wrapper for all map formats.
MAP_FORMAT LoadMAP(const TCHAR *mapname, bool add)
{
    FILE *f;
    char sign[256];

    // delete previous MAP symbols?
    if(!add)
    {
        SYMKill();
    }

    // copy name for MAP saver (with SaveMAP "this" parameter)
    _tcscpy(hle.mapfile, mapname);

    // try to open
    _tfopen_s(&f, mapname, _T("r"));
    if(!f)
    {
        DBReport2(DbgChannel::HLE, "Cannot %s MAP: %s\n", (add) ? "add" : "load", Debug::Hub.TcharToString((TCHAR*)mapname).c_str());
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
    TCHAR tcharStr[0x1000] = { 0, };
    TCHAR* tcharPtr = tcharStr;
    char* charPtr = (char*)mapname;

    while (*charPtr)
    {
        *tcharPtr++ = *charPtr++;
    }
    *tcharPtr++ = 0;

    return LoadMAP(tcharStr, add);
}
