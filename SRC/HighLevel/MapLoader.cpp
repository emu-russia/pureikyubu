// MAP files loader. currently there are support for three MAP file formats : 
// Dolwin custom ("RAW"), CodeWarrior and GCC-like.
#include "pch.h"

// load CodeWarrior-generated map file
// thanks Dolphin team for idea
static int LoadMapCW(char *mapname)
{
    BOOL    started = FALSE;
    char    buf[1024], token1[256];
    FILE    *map;
    
    // symbol information
    uint32_t  moduleOffset, procSize, procAddr;
    int     flags;
    char    procName[512];

    map = fopen(mapname, "r");
    if(!map) return MAP_FORMAT_BAD;

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

    DBReport(YEL "CW format map loaded : %s\n\n", mapname);
    return MAP_FORMAT_CW;
}

// load GCC-generated map file
static int LoadMapGCC(char *mapname)
{
    BOOL    started = FALSE;
    char    buf[1024];
    FILE    *map;
    
    // symbol information
    uint32_t     procAddr;
    char    par1[512];
    char    par2[512];

    map = fopen(mapname, "r");
    if(!map) return MAP_FORMAT_BAD;

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

    DBReport(YEL "GCC format map loaded : %s\n\n", mapname);
    return MAP_FORMAT_GCC;
}

// load Dolwin format map-file
static int LoadMapRAW(char *mapname)
{
    int i;
    int size = FileSize(mapname);
    FILE *f = fopen(mapname, "rt");

    // allocate memory
    char *mapbuf = (char *)malloc(size + 1);
    if(mapbuf == NULL)
    {
        DBHalt(
            "Not enough memory to load MAP.\n"
            "file name : %s\n"
            "file size : %ib\n\n",
            mapname, size
        );
        return MAP_FORMAT_BAD;
    }

    // load from file
    fread(mapbuf, size, 1, f);
    fclose(f);
    mapbuf[size] = 0;

    // remove all garbage, like tabs
    for(i=0; i<size; i++)
    {
        if(mapbuf[i] < ' ') mapbuf[i] = '\n';
    }

    char *ptr = mapbuf;
    while(*ptr)
    {
        // some maps has really *huge* symbols
        char line[1000];
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
    free(mapbuf);

    DBReport(YEL "RAW format map loaded : %s\n\n", mapname);
    return MAP_FORMAT_RAW;
}

// wrapper for all map formats. FALSE is returned, if cannot load map file.
int LoadMAP(char *mapname, bool add)
{
    FILE *f;
    char sign[256];

    // delete previous MAP symbols ?
    if(!add)
    {
        SYMKill();
    }

    // copy name for MAP saver (with SaveMAP "this" parameter)
    strncpy(hle.mapfile, mapname, sizeof(hle.mapfile));

    // try to open
    f = fopen(mapname, "r");
    if(!f)
    {
        DBReport(YEL "cannot %s MAP : %s\n", (add) ? "add" : "load", mapname);
        hle.mapfile[0] = 0;
        return MAP_FORMAT_BAD;
    }

    // recognize map format
    fread(sign, 1, 256, f);
    fclose(f);

    int format;
    if(!strncmp(sign, "Link map", 8)) format = LoadMapCW(mapname);
    else if(!strncmp(sign, "Archive member", 14)) format = LoadMapGCC(mapname);
    else format = LoadMapRAW(mapname);

    if(format == MAP_FORMAT_BAD)
    {
        hle.mapfile[0] = 0;
    }
    return format;
}
