// this is formerly 0.09 UserLoader. I was figured, that its more 
// close to Emulator, rather than UserMenu.
// supported formats are :
//      .patch      - patching files (new)
//      .dol        - GAMECUBE custom executable
//      .elf        - standard executable
//      .bin        - binary file (loaded at BINORG offset)
//      .gcm        - game master data (GC DVD images)
#include "dolphin.h"

// all loader variables are placed here
LoaderData ldat;

// ---------------------------------------------------------------------------
// DOL loader

// return DOL body size (text + data)
uint32_t DOLSize(DolHeader *dol)
{
    int32_t i;
    uint32_t totalBytes = 0;

    for(i=0; i<DOL_NUM_TEXT; i++)
    {
        if(dol->textOffset[i])
        {
            // aligned to 32 bytes
            totalBytes += (dol->textSize[i] + 31) & ~31;
        }
    }

    for(i=0; i<DOL_NUM_DATA; i++)
    {
        if(dol->dataOffset[i])
        {
            // aligned to 32 bytes
            totalBytes += (dol->dataSize[i] + 31) & ~31;
        }
    }

    return totalBytes;
}

// return DOL entrypoint, or 0 if cannot load
// we dont need to translate address, because DOL loading goes
// under control of DolphinOS, so just use simple translation mask.
uint32_t LoadDOL(char *dolname)
{
    int i;
    FILE        *dol;
    DolHeader   dh;

    // try to open file
    dol = fopen(dolname, "rb");
    if(!dol) return 0;

    // load DOL header and swap it for loader
    fread(&dh, 1, sizeof(DolHeader), dol);
    MEMSwapArea((uint32_t *)&dh, sizeof(DolHeader));

    DBReport( YEL "loading DOL (%i b).\n", 
              DOLSize(&dh) );

    // load all text (code) sections
    for(i=0; i<DOL_NUM_TEXT; i++)
    {
        if(dh.textOffset[i])    // if offset is 0, then section is empty
        {
            void *addr = &RAM[dh.textAddress[i] & RAMMASK];

            fseek(dol, dh.textOffset[i], SEEK_SET);
            fread(addr, 1, dh.textSize[i], dol);

            DBReport(
                YEL "   text section %08X->%08X, size %i b\n",
                dh.textOffset[i],
                dh.textAddress[i], dh.textSize[i]
            );
        }
    }

    // load all data sections
    for(i=0; i<DOL_NUM_DATA; i++)
    {
        if(dh.dataOffset[i])    // if offset is 0, then section is empty
        {
            void *addr = &RAM[dh.dataAddress[i] & RAMMASK];

            fseek(dol, dh.dataOffset[i], SEEK_SET);
            fread(addr, 1, dh.dataSize[i], dol);

            DBReport(
                YEL "   data section %08X->%08X, size %i b\n", 
                dh.dataOffset[i],
                dh.dataAddress[i], dh.dataSize[i]
            );
        }
    }

    BootROM(false);

    // Setup registers
    SP = 0x816ffffc;
    SDA1 = 0x81100000;      // Fake sda1

    // DO NOT CLEAR BSS !

    DBReport(YEL "   DOL entrypoint %08X\n\n", dh.entryPoint);
    fclose(dol);
    return dh.entryPoint;
}

// same as LoadDOL, but DOL is mapped in memory
uint32_t LoadDOLFromMemory(DolHeader *dol, uint32_t ofs)
{
    int i;
    #define ADDPTR(p1, p2) (uint8_t *)((uint32_t)(p1)+(uint32_t)(p2))

    // swap DOL header
    MEMSwapArea((uint32_t *)dol, sizeof(DolHeader));

    DBReport( YEL "loading DOL from %08X (%i b).\n", 
              ofs, DOLSize(dol) );

    // load all text (code) sections
    for(i=0; i<DOL_NUM_TEXT; i++)
    {
        if(dol->textOffset[i])  // if offset is 0, then section is empty
        {
            uint8_t*addr = &RAM[dol->textAddress[i] & RAMMASK];
            memcpy(addr, ADDPTR(dol, dol->textOffset[i]), dol->textSize[i]);

            DBReport(
                YEL "   text section %08X->%08X, size %i b\n",
                ofs + dol->textOffset[i],
                dol->textAddress[i], dol->textSize[i]
            );
        }
    }

    // load all data sections
    for(i=0; i<DOL_NUM_DATA; i++)
    {
        if(dol->dataOffset[i])  // if offset is 0, then section is empty
        {
            uint8_t *addr = &RAM[dol->dataAddress[i] & RAMMASK];
            memcpy(addr, ADDPTR(dol, dol->dataOffset[i]), dol->dataSize[i]);

            DBReport(
                YEL "   data section %08X->%08X, size %i b\n", 
                ofs + dol->dataOffset[i],
                dol->dataAddress[i], dol->dataSize[i]
            );
        }
    }

    // DO NOT CLEAR BSS !

    DBReport(YEL "   DOL entrypoint %08X\n\n", dol->entryPoint);

    return dol->entryPoint;
}

// ---------------------------------------------------------------------------
// ELF loader

// swapping endiannes. dont use Dolwin memory swap, and keep whole
// code to be portable for other applications. and dont use Dolwin types.

static int CheckELFHeader(ElfEhdr *hdr)
{
    if(
        ( hdr->e_ident[EI_MAG0] != 0x7f ) ||
        ( hdr->e_ident[EI_MAG1] != 'E'  ) ||
        ( hdr->e_ident[EI_MAG2] != 'L'  ) ||
        ( hdr->e_ident[EI_MAG3] != 'F'  ) )
        return 0;

    if(hdr->e_ident[EI_CLASS] != ELFCLASS32)
        return 0;

    return 1;
}

static ElfAddr     (*Elf_SwapAddr)(ElfAddr);
static ElfOff      (*Elf_SwapOff)(ElfOff);
static ElfWord     (*Elf_SwapWord)(ElfWord);
static ElfHalf     (*Elf_SwapHalf)(ElfHalf);
static ElfSword    (*Elf_SwapSword)(ElfSword);

static ElfAddr     Elf_NoSwapAddr(ElfAddr data)   { return data; }
static ElfOff      Elf_NoSwapOff(ElfOff data)     { return data; }
static ElfWord     Elf_NoSwapWord(ElfWord data)   { return data; }
static ElfHalf     Elf_NoSwapHalf(ElfHalf data)   { return data; }
static ElfSword    Elf_NoSwapSword(ElfSword data) { return data; }

static ElfWord     Elf_YesSwapWord(ElfWord data)
{ 
    unsigned char 
        b1 = (unsigned char)(data      ) & 0xff,
        b2 = (unsigned char)(data >>  8) & 0xff,
        b3 = (unsigned char)(data >> 16) & 0xff,
        b4 = (unsigned char)(data >> 24) & 0xff;
    
    return 
        ((ElfWord)b1 << 24) |
        ((ElfWord)b2 << 16) |
        ((ElfWord)b3 <<  8) | b4;
}

static ElfAddr     Elf_YesSwapAddr(ElfAddr data)
{
    return (ElfAddr)Elf_YesSwapWord((ElfWord)data);
}

static ElfOff      Elf_YesSwapOff(ElfOff data)
{
    return (ElfOff)Elf_YesSwapWord((ElfWord)data);
}

static ElfHalf     Elf_YesSwapHalf(ElfHalf data)
{ 
    return ((data & 0xff) << 8) | ((data & 0xff00) >> 8);
}

static ElfSword    Elf_YesSwapSword(ElfSword data)
{
    return (ElfSword)Elf_YesSwapWord((ElfWord)data);
}

static void Elf_SwapInit(int is_little)
{
    if(is_little)
    {
        Elf_SwapAddr = Elf_NoSwapAddr;
        Elf_SwapOff  = Elf_NoSwapOff;
        Elf_SwapWord = Elf_NoSwapWord;
        Elf_SwapHalf = Elf_NoSwapHalf;
        Elf_SwapSword= Elf_NoSwapSword;
    }
    else
    {
        Elf_SwapAddr = Elf_YesSwapAddr;
        Elf_SwapOff  = Elf_YesSwapOff;
        Elf_SwapWord = Elf_YesSwapWord;
        Elf_SwapHalf = Elf_YesSwapHalf;
        Elf_SwapSword= Elf_YesSwapSword;
    }
}

// return ELF entrypoint, or 0 if cannot load
// we dont need to translate address, because DOL loading goes
// under control of DolphinOS, so just use simple translation mask.
uint32_t LoadELF(char *elfname)
{
    unsigned long elf_entrypoint;
    FILE        *f;
    ElfEhdr     hdr;
    ElfPhdr     phdr;
    int         i;

    f = fopen(elfname, "rb");
    if(!f) return 0;

    // check header
    fread(&hdr, 1, sizeof(ElfEhdr), f);
    if(CheckELFHeader(&hdr) == 0)
    {
        fclose(f);
        return 0;
    }

    Elf_SwapInit( (hdr.e_ident[EI_DATA] == ELFDATA2LSB) ? (1) : (0) );

    // check file type (must be exec)
    if(Elf_SwapHalf(hdr.e_type) != ET_EXEC)
    {
        fclose(f);
        return 0;
    }

    elf_entrypoint = Elf_SwapAddr(hdr.e_entry);

    //
    // load all segments
    //

    fseek(f, Elf_SwapOff(hdr.e_phoff), SEEK_SET);

    for(i=0; i<Elf_SwapHalf(hdr.e_phnum); i++)
    {
        long old;

        fread(&phdr, 1, sizeof(ElfPhdr), f);
        old = ftell(f);

        // load one segment
        {
            unsigned long vend, vaddr;
            long size;

            if(Elf_SwapWord(phdr.p_type) == PT_LOAD)
            {
                vaddr = Elf_SwapAddr(phdr.p_vaddr);
                
                size = Elf_SwapWord(phdr.p_filesz);
                if(size == 0) continue;

                vend = vaddr + size;

                fseek(f, Elf_SwapOff(phdr.p_offset), SEEK_SET);
                fread(&RAM[vaddr & RAMMASK], vend - vaddr, 1, f);
            }
        }

        fseek(f, old, SEEK_SET);
    }

    fclose(f);
    return elf_entrypoint;
}

// ---------------------------------------------------------------------------
// BIN loader

// return BINORG offset, or 0 if cannot load.
// use physical addressing!
uint32_t LoadBIN(char *binname)
{
    uint32_t org = GetConfigInt(USER_BINORG, USER_BINORG_DEFAULT);

    // binary file loading address is above RAM
    if(org >= RAMSIZE) return 0;

    // get file size
    int fsize = FileSize(binname);

    // try to load file
    FILE * bin = fopen(binname, "rb");
    if(bin == NULL) return 0;

    // nothing to load ?
    if(fsize == 0)
    {
        fclose(bin);
        return 0;
    }

    // limit by RAMSIZE
    if((org + fsize) > RAMSIZE)
    {
        fsize = RAMSIZE - org;
    }

    // load
    fread(&RAM[org], 1, fsize, bin);
    fclose(bin);

    DBReport(YEL "loaded binary file at %08X (%s)\n\n", org, FileSmartSize(fsize));
    org |= 0x80000000;
    return org;     // its me =:)
}

// ---------------------------------------------------------------------------
// patch loader. may be used for game cheating, but I dont like cheaters.
// so there is no cheat support in Dolwin. instead of that, patches are 
// using to erase some unimplemented hardware code.

// you may ignore whole patch code, since it's definitely only for org :)

// return 1, if patch loaded OK.
// "add" can be used to extend current patch table.
BOOL LoadPatch(char * patchname, BOOL add)
{
    // allowed ?
    // since "enable" flag is loaded only here, it is not important
    // to load it in standalone patch Init routine. simply if there are
    // no patch files loaded, there is nothing to apply.
    ldat.enablePatch = GetConfigInt(USER_PATCH, USER_PATCH_DEFAULT);
    if(!ldat.enablePatch) return TRUE;

    // count patchnum
    int patchNum = FileSize(patchname) / sizeof(Patch);

    // try to open file
    FILE *f = fopen(patchname, "rb");
    if(f == NULL) return FALSE;

    // print notification in debugger
    if(add) DBReport(YEL "added patch : %s\n", patchname);
    else DBReport(YEL "loaded patch : %s\n", patchname);

    // remove current patch table (if not adding)
    if(!add)
    {
        UnloadPatch();
    }

    // extend (or allocate new) patch table, and copy data
    if(add)
    {
        if(patchNum == 0) return TRUE;  // nothing to add

        ldat.patches = (Patch *)realloc(
            ldat.patches, 
            (patchNum + ldat.patchNum) * sizeof(Patch)
        );
        if(ldat.patches == NULL) return FALSE;

        fread(&ldat.patches[ldat.patchNum], sizeof(Patch), patchNum, f);
        ldat.patchNum += patchNum;
        ApplyPatches(1, ldat.patchNum - patchNum);
    }
    else
    {
        ldat.patches = (Patch *)malloc(patchNum * sizeof(Patch));
        if(ldat.patches == NULL) return FALSE;

        fread(ldat.patches, sizeof(Patch), patchNum, f);
        ldat.patchNum = patchNum;
        ApplyPatches(1);
    }

    fclose(f);
    return TRUE;
}

// called after patch loading and/or every VI frame
// [a...b] - range of patches in table to apply in (including a and b)
void ApplyPatches(BOOL load, int32_t a, int32_t b)
{
    // allowed ?
    if(!ldat.enablePatch) return;

    // b = MAX ?
    if(b==-1) b = ldat.patchNum - 1;
    
    for(int32_t i=a; i<=b; i++)     // i = [a; b]
    {
        Patch * p = &ldat.patches[i];
        if(p->freeze || load)
        {
            uint32_t ea = MEMSwap(p->effectiveAddress);
            uint32_t pa = MEMEffectiveToPhysical(ea, 0);
            if(pa == -1) continue;

            uint8_t * ptr = (uint8_t *)&RAM[pa], * data = (uint8_t *)(&(p->data));
            switch(p->dataSize)
            {
                case PATCH_SIZE_8:
                    ptr[0] = data[0];
                    DBReport( YEL "patch : (u8)[%08X] = %02X\n", ea,
                              ptr[0] );
                    break;
                case PATCH_SIZE_16:
                    ptr[0] = data[0];
                    ptr[1] = data[1];
                    DBReport( YEL "patch : (u16)[%08X] = %02X%02X\n", ea,
                              ptr[0], ptr[1] );
                    break;
                case PATCH_SIZE_32:
                    ptr[0] = data[0];
                    ptr[1] = data[1];
                    ptr[2] = data[2];
                    ptr[3] = data[3];
                    DBReport( YEL "patch : (u32)[%08X] = %02X%02X%02X%02X\n", ea,
                              ptr[0], ptr[1], ptr[2], ptr[3] );
                    break;
                case PATCH_SIZE_64:
                    ptr[0] = data[0];
                    ptr[1] = data[1];
                    ptr[2] = data[2];
                    ptr[3] = data[3];
                    ptr[4] = data[4];
                    ptr[5] = data[5];
                    ptr[6] = data[6];
                    ptr[7] = data[7];
                    DBReport( YEL "patch : (u64)[%08X] = %02X%02X%02X%02X%02X%02X%02X%02X\n", ea,
                              ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7] );
                    break;
            }
        }
    }
}

void UnloadPatch()
{
    if(ldat.patches)
    {
        free(ldat.patches);
        ldat.patches = NULL;
        ldat.patchNum = 0;
    }
}

// ---------------------------------------------------------------------------
// file loader engine

static void LoadIniSettings()
{
    // overwrite CPU timing by INI settings
    char *str = GetIniVar(ldat.gameID, "cf");
    if(str)
    {
        cpu.cf = atoi(str);
        if(cpu.cf <= 0) cpu.cf = 1;
    }
    str = GetIniVar(ldat.gameID, "delay");
    if(str)
    {
        cpu.delay = atoi(str);
        if(cpu.delay <= 0) cpu.delay = 1;
    }
    str = GetIniVar(ldat.gameID, "bailout");
    if(str)
    {
        cpu.bailout = atoi(str);
        if(cpu.bailout <= 0) cpu.bailout = 1;
    }

    // show CPU timing setup
    char buf[64];
    sprintf(buf, "%i - %i - %i", cpu.cf, cpu.delay, cpu.bailout);
    SetStatusText(STATUS_TIMING, buf);
}

static void AutoloadMap()
{
    // get map file name
    char mapname[4*1024];
    char drive[1024], dir[1024], name[1024], ext[1024];
    _splitpath(ldat.currentFile, drive, dir, name, ext);

    // Step 1: try to load map from Data directory
    if(ldat.dvd) sprintf(mapname, ".\\Data\\%s.map", ldat.gameID);
    else sprintf(mapname, ".\\Data\\%s.map", name);
    int ok = LoadMAP(mapname);
    if(ok) return;
 
    // Step 2: try to load map from file directory
    if(ldat.dvd) sprintf(mapname, "%s%s%s.map", drive, dir, ldat.gameID);
    else sprintf(mapname, "%s%s%s.map", drive, dir, name);
    ok = LoadMAP(mapname);
    if(ok) return;

    // sorry, no maps for this DVD/executable
    DBReport( YEL "WARNING: MAP file doesnt exist, HLE could be impossible\n\n");

    // Step 3: make new map (find symbols)
    if(GetConfigInt(USER_MAKEMAP, USER_MAKEMAP_DEFAULT))
    {
        if(ldat.dvd) sprintf(mapname, ".\\Data\\%s.map", ldat.gameID);
        else sprintf(mapname, ".\\Data\\%s.map", name);
        DBReport( YEL "Making new MAP file : %s\n\n", mapname);
        MAPInit(mapname);
        MAPAddRange(0x80000000, 0x80000000 | RAMSIZE);  // user can wait for once :O)
        MAPFinish();
        LoadMAP(mapname);
    }
}

static void AutoloadPatch()
{
    // get patch file name
    char patch[4*1024];
    char drive[1024], dir[1024], name[1024], ext[1024];
    _splitpath(ldat.currentFile, drive, dir, name, ext);

    // Step 1: try to load patch from Data directory
    if(ldat.dvd) sprintf(patch, ".\\Data\\%s.patch", ldat.gameID);
    else sprintf(patch, ".\\Data\\%s.patch", name);
    BOOL ok = LoadPatch(patch);
    if(ok) return;

    // Step 2: try to load patch from file directory
    if(ldat.dvd) sprintf(patch, "%s%s%s.patch", drive, dir, ldat.gameID);
    else sprintf(patch, "%s%s%s.patch", drive, dir, name);
    LoadPatch(patch);

    // sorry, no patches for this DVD/executable
}

static BOOL SetGameID(char *filename)
{
    // try to set current DVD
    if(DVDSetCurrent(filename) == FALSE) return FALSE;

    // load DVD banner
    DVDBanner2 * bnr = (DVDBanner2 *)DVDLoadBanner(filename);

    // get DiskID
    char diskID[8];
    DVDSeek(0);
    DVDRead(diskID, 4);
    diskID[4] = 0;

    // set GameID
    sprintf( ldat.gameID, "%.4s%02X",
             diskID, DVDBannerChecksum((void *)bnr) );
    free(bnr);
    return TRUE;
}

#define IFEXT(ext) if(!stricmp(ext, strrchr(filename, '.')))

// load any Dolwin-supported file
static void DoLoadFile(char *filename)
{
    uint32_t entryPoint = 0;
    char statusText[256];
    uint32_t s_time = GetTickCount();

    // loading progress
    sprintf(statusText, "Loading %s", filename);
    SetStatusText(STATUS_PROGRESS, statusText);

    // load file
    IFEXT(".dol")
    {
        entryPoint = LoadDOL(filename);
        ldat.gameID[0] = 0;
        ldat.dvd = FALSE;
    }
    else IFEXT(".elf")
    {
        entryPoint = LoadELF(filename);
        ldat.gameID[0] = 0;
        ldat.dvd = FALSE;
    }
    else IFEXT(".bin")
    {
        entryPoint = LoadBIN(filename);
        ldat.gameID[0] = 0;
        ldat.dvd = FALSE;
    }
    else IFEXT(".gcm")
    {
        DVDSetCurrent(filename);
        ldat.dvd = SetGameID(filename);
    }

    // file load success ?
    if(entryPoint == 0 && !ldat.dvd)
    {
        DolwinError( "Cannot load file!",
                      "\'%s\'\n",
                      filename);
    }
    else
    {
        char fullPath[MAX_PATH], drive[_MAX_DRIVE + 1], dir[_MAX_DIR], name[_MAX_PATH], ext[_MAX_EXT];
        _splitpath(filename, drive, dir, name, ext);
        sprintf(fullPath, "%s%s", drive, dir);
        strcpy(ldat.currentFileName, name);

        // add new recent entry
        AddRecentFile(filename);

        // add new path to selector
        AddSelectorPath(fullPath);      // all checks are there
    }

    // set hack identifiers
    {
        ldat.freeLoader   =             // clear all
        ldat.actionReplay = FALSE;
        if(!stricmp(ldat.gameID, "GNHEE9")) ldat.freeLoader = TRUE;
        if(!stricmp(ldat.gameID, "GNHE8C")) ldat.actionReplay = TRUE;
        if(!stricmp(ldat.currentFileName, "ar")) ldat.actionReplay = TRUE;

        // show all
        if(ldat.freeLoader) DBReport(GREEN "Freeloader" YEL " hacks are enabled\n");
        if(ldat.actionReplay) DBReport(GREEN "Action Replay" YEL " hacks are enabled\n");
    }

    // simulate bootrom
    BootROM(ldat.dvd);
    Sleep(10);

    // autoload map file
    AutoloadMap();

    // autoload patch file. called after BootROM, to allow patch of OS lomem.
    UnloadPatch();
    AutoloadPatch();

    // show boot time
    uint32_t e_time = GetTickCount();
    ldat.boottime = (float)(e_time - s_time) / 1000.0f;
    sprintf(statusText, "Boot time %1.2f sec", ldat.boottime);
    SetStatusText(STATUS_PROGRESS, statusText);

    // set entrypoint (for DVD, PC will set in apploader)
    if(!ldat.dvd) PC = entryPoint;

    // now onverwrite by INI settings
    LoadIniSettings();
}

// set next file to load
void LoadFile(char *filename)
{
    strcpy(ldat.currentFile, filename);
    SetConfigString(USER_LASTFILE, ldat.currentFile);
}

// reload last file
void ReloadFile()
{
    DBReport(
        GREEN "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n"
        GREEN "GC File Loader.\n"
        GREEN "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\n"
    );

    if(strlen(ldat.currentFile))
    {
        DBReport(YEL "loading file : \"%s\"\n\n", ldat.currentFile);
        DoLoadFile(ldat.currentFile);
    }
}
