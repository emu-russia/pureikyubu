// Supported formats are :
//      .patch      - patching files (new)
//      .dol        - GAMECUBE custom executable
//      .elf        - standard executable
//      .bin        - binary file (loaded at BINORG offset)
//      .gcm        - game master data (GC DVD images)
#include "pch.h"

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
uint32_t LoadDOL(TCHAR *dolname)
{
    int i;
    FILE        *dol;
    DolHeader   dh;

    // try to open file
    dol = nullptr;
    _tfopen_s(&dol, dolname, _T("rb"));
    if(!dol) return 0;

    // load DOL header and swap it for loader
    fread(&dh, 1, sizeof(DolHeader), dol);
    Gekko::GekkoCore::SwapArea((uint32_t *)&dh, sizeof(DolHeader));

    DBReport2(DbgChannel::Loader, "Loading DOL %s (%i b).\n",
              Debug::Hub.TcharToString(dolname).c_str(), DOLSize(&dh) );

    // load all text (code) sections
    for(i=0; i<DOL_NUM_TEXT; i++)
    {
        if(dh.textOffset[i])    // if offset is 0, then section is empty
        {
            void *addr = &mi.ram[dh.textAddress[i] & RAMMASK];

            fseek(dol, dh.textOffset[i], SEEK_SET);
            fread(addr, 1, dh.textSize[i], dol);

            DBReport2(DbgChannel::Loader,
                "   text section %08X->%08X, size %i b\n",
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
            void *addr = &mi.ram[dh.dataAddress[i] & RAMMASK];

            fseek(dol, dh.dataOffset[i], SEEK_SET);
            fread(addr, 1, dh.dataSize[i], dol);

            DBReport2(DbgChannel::Loader,
                "   data section %08X->%08X, size %i b\n", 
                dh.dataOffset[i],
                dh.dataAddress[i], dh.dataSize[i]
            );
        }
    }

    HWConfig * config = new HWConfig;
    assert(config);
    EMUGetHwConfig(config);
    BootROM(false, false, config->consoleVer);

    // Setup registers
    Gekko::Gekko->regs.gpr[1] = 0x816ffffc;
    Gekko::Gekko->regs.gpr[13] = 0x81100000;      // Fake sda1

    // DO NOT CLEAR BSS !

    DBReport2(DbgChannel::Loader, "   DOL entrypoint %08X\n\n", dh.entryPoint);
    fclose(dol);
    return dh.entryPoint;
}

// same as LoadDOL, but DOL is mapped in memory
uint32_t LoadDOLFromMemory(DolHeader *dol, uint32_t ofs)
{
    int i;
    #define ADDPTR(p1, p2) (uint8_t *)((uint8_t*)(p1)+(uint32_t)(p2))

    // swap DOL header
    Gekko::GekkoCore::SwapArea((uint32_t *)dol, sizeof(DolHeader));

    DBReport2(DbgChannel::Loader, "Loading DOL from %08X (%i b).\n",
              ofs, DOLSize(dol) );

    // load all text (code) sections
    for(i=0; i<DOL_NUM_TEXT; i++)
    {
        if(dol->textOffset[i])  // if offset is 0, then section is empty
        {
            uint8_t*addr = &mi.ram[dol->textAddress[i] & RAMMASK];
            memcpy(addr, ADDPTR(dol, dol->textOffset[i]), dol->textSize[i]);

            DBReport2(DbgChannel::Loader,
                "   text section %08X->%08X, size %i b\n",
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
            uint8_t *addr = &mi.ram[dol->dataAddress[i] & RAMMASK];
            memcpy(addr, ADDPTR(dol, dol->dataOffset[i]), dol->dataSize[i]);

            DBReport2(DbgChannel::Loader,
                "   data section %08X->%08X, size %i b\n", 
                ofs + dol->dataOffset[i],
                dol->dataAddress[i], dol->dataSize[i]
            );
        }
    }

    // DO NOT CLEAR BSS !

    DBReport2(DbgChannel::Loader, "   DOL entrypoint %08X\n\n", dol->entryPoint);

    return dol->entryPoint;
}

// ---------------------------------------------------------------------------
// ELF loader

// swapping endiannes. dont use Dolwin memory swap, and keep whole
// code to be portable for other applications.

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
uint32_t LoadELF(TCHAR *elfname)
{
    unsigned long elf_entrypoint;
    FILE        *f;
    ElfEhdr     hdr;
    ElfPhdr     phdr;
    int         i;

    f = nullptr;
    _tfopen_s(&f, elfname, _T("rb"));
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
                fread(&mi.ram[vaddr & RAMMASK], vend - vaddr, 1, f);
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
uint32_t LoadBIN(TCHAR * binname)
{
    uint32_t org = GetConfigInt(USER_BINORG, USER_LOADER);

    // binary file loading address is above RAM
    if(org >= RAMSIZE) return 0;

    // get file size
    size_t fsize = UI::FileSize(binname);

    // try to load file
    FILE* bin = nullptr;
    _tfopen_s(&bin, binname, _T("rb"));
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
    fread(&mi.ram[org], 1, fsize, bin);
    fclose(bin);

    DBReport2(DbgChannel::Loader, "Loaded binary file at %08X (0x%08X)\n\n", org, fsize);
    org |= 0x80000000;
    return org;     // its me =:)
}

// ---------------------------------------------------------------------------
// patch loader.

// you may ignore whole patch code, since it's definitely only for me :)

// return 1, if patch loaded OK.
// "add" can be used to extend current patch table.
bool LoadPatch(TCHAR * patchname, bool add)
{
    // allowed ?
    // since "enable" flag is loaded only here, it is not important
    // to load it in standalone patch Init routine. simply if there are
    // no patch files loaded, there is nothing to apply.
    ldat.enablePatch = GetConfigBool(USER_PATCH, USER_LOADER);
    if(!ldat.enablePatch) return true;

    // count patchnum
    size_t patchNum = UI::FileSize(patchname) / sizeof(Patch);

    // try to open file
    if (!UI::FileExists(patchname))
        return false;

    // print notification in debugger
    if(add) DBReport2(DbgChannel::Loader, "Added patch: %s\n", Debug::Hub.TcharToString(patchname).c_str());
    else DBReport2(DbgChannel::Loader, "Loaded patch: %s\n", Debug::Hub.TcharToString(patchname).c_str());

    // remove current patch table (if not adding)
    if(!add)
    {
        UnloadPatch();
    }

    // extend (or allocate new) patch table, and copy data
    if(add)
    {
        if(patchNum == 0) return true;  // nothing to add

        size_t oldPatchCount = ldat.patches.size();

        Patch* patches = (Patch*)UI::FileLoad(patchname);

        for (int i = 0; i < patchNum; i++)
        {
            Patch* next = new Patch;
            *next = patches[i];

            ldat.patches.push_back(next);
        }

        free(patches);
        ApplyPatches(true, (int32_t)oldPatchCount);
    }
    else
    {
        Patch* patches = (Patch * )UI::FileLoad(patchname);

        for (int i = 0; i < patchNum; i++)
        {
            Patch* next = new Patch;
            *next = patches[i];

            ldat.patches.push_back(next);
        }

        free(patches);
        ApplyPatches(true);
    }

    return true;
}

// called after patch loading and/or every VI frame
// [a...b] - range of patches in table to apply in (including a and b)
void ApplyPatches(bool load, int32_t a, int32_t b)
{
    // allowed ?
    if(!ldat.enablePatch) return;

    // b = MAX ?
    if(b==-1) b = (int32_t)ldat.patches.size() - 1;
    
    for(int32_t i=a; i<=b; i++)     // i = [a; b]
    {
        Patch * p = ldat.patches[i];
        if(p->freeze || load)
        {
            uint32_t ea = _byteswap_ulong(p->effectiveAddress);
            uint32_t pa = (uint32_t)-1;
            if (Gekko::Gekko)
            {
                int WIMG;
                pa = Gekko::Gekko->EffectiveToPhysical(ea, Gekko::MmuAccess::Execute, WIMG);
            }
            if(pa == Gekko::BadAddress) continue;

            uint8_t * ptr = (uint8_t *)&mi.ram[pa], * data = (uint8_t *)(&(p->data));
            switch(p->dataSize)
            {
                case PATCH_SIZE_8:
                    ptr[0] = data[0];
                    DBReport2(DbgChannel::Loader, "patch: (u8)[%08X] = %02X\n", ea,
                              ptr[0] );
                    break;
                case PATCH_SIZE_16:
                    ptr[0] = data[0];
                    ptr[1] = data[1];
                    DBReport2(DbgChannel::Loader, "patch: (u16)[%08X] = %02X%02X\n", ea,
                              ptr[0], ptr[1] );
                    break;
                case PATCH_SIZE_32:
                    ptr[0] = data[0];
                    ptr[1] = data[1];
                    ptr[2] = data[2];
                    ptr[3] = data[3];
                    DBReport2(DbgChannel::Loader, "patch: (u32)[%08X] = %02X%02X%02X%02X\n", ea,
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
                    DBReport2(DbgChannel::Loader, "patch: (u64)[%08X] = %02X%02X%02X%02X%02X%02X%02X%02X\n", ea,
                              ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5], ptr[6], ptr[7] );
                    break;
            }
        }
    }
}

void UnloadPatch()
{
    while (!ldat.patches.empty())
    {
        Patch* patch = ldat.patches.back();
        ldat.patches.pop_back();
        delete patch;
    }
}

// ---------------------------------------------------------------------------
// file loader engine

static void AutoloadMap()
{
    // get map file name
    TCHAR mapname[0x1000];
    TCHAR drive[MAX_PATH], dir[MAX_PATH], name[_MAX_PATH], ext[_MAX_EXT];

    _tsplitpath_s(ldat.currentFile,
        drive, _countof(drive) - 1,
        dir, _countof(dir) - 1,
        name, _countof(name) - 1,
        ext, _countof(ext) - 1);

    // Step 1: try to load map from Data directory
    if(ldat.dvd) _stprintf_s (mapname, _countof(mapname) - 1, _T(".\\Data\\%s.map"), ldat.gameID);
    else _stprintf_s (mapname, _countof(mapname) - 1, _T(".\\Data\\%s.map"), name);
    MAP_FORMAT format = LoadMAP(mapname);
    if(format != MAP_FORMAT::BAD) return;
 
    // Step 2: try to load map from file directory
    if(ldat.dvd) _stprintf_s (mapname, _countof(mapname) - 1, _T("%s%s%s.map"), drive, dir, ldat.gameID);
    else _stprintf_s (mapname, _countof(mapname) - 1, _T("%s%s%s.map"), drive, dir, name);
    format = LoadMAP(mapname);
    if(format != MAP_FORMAT::BAD) return;

    // sorry, no maps for this DVD/executable
    DBReport2(DbgChannel::Loader, "WARNING: MAP file doesnt exist, HLE could be impossible\n\n");

    // Step 3: make new map (find symbols)
    if(GetConfigBool(USER_MAKEMAP, USER_LOADER))
    {
        if(ldat.dvd) _stprintf_s (mapname, _countof(mapname) - 1, _T(".\\Data\\%s.map"), ldat.gameID);
        else _stprintf_s (mapname, _countof(mapname) - 1, _T(".\\Data\\%s.map"), name);
        DBReport2(DbgChannel::Loader, "Making new MAP file: %s\n\n", Debug::Hub.TcharToString(mapname).c_str());
        MAPInit(mapname);
        MAPAddRange(0x80000000, 0x80000000 | RAMSIZE);  // user can wait for once :O)
        MAPFinish();
        LoadMAP(mapname);
    }
}

static void AutoloadPatch()
{
    // get patch file name
    TCHAR patch[0x1000];
    TCHAR drive[MAX_PATH], dir[MAX_PATH], name[_MAX_PATH], ext[_MAX_EXT];

    _tsplitpath_s(ldat.currentFile,
        drive, _countof(drive) - 1,
        dir, _countof(dir) - 1,
        name, _countof(name) - 1,
        ext, _countof(ext) - 1);

    // Step 1: try to load patch from Data directory
    if(ldat.dvd) _stprintf_s (patch, _countof(patch) - 1, _T(".\\Data\\%s.patch"), ldat.gameID);
    else _stprintf_s (patch, _countof (patch) - 1, _T(".\\Data\\%s.patch"), name);
    bool ok = LoadPatch(patch);
    if(ok) return;

    // Step 2: try to load patch from file directory
    if(ldat.dvd) _stprintf_s (patch, _countof (patch) - 1, _T("%s%s%s.patch"), drive, dir, ldat.gameID);
    else _stprintf_s(patch, _countof (patch) - 1, _T("%s%s%s.patch"), drive, dir, name);
    LoadPatch(patch);

    // sorry, no patches for this DVD/executable
}

static bool SetGameIDAndTitle(TCHAR *filename)
{
    // load DVD banner
    DVDBanner2 * bnr = (DVDBanner2 *)DVDLoadBanner(filename);

    // get DiskID
    char diskID[8] = { 0 };
    TCHAR diskIdTchar[8] = { 0 };
    DVD::Seek(0);
    DVD::Read(diskID, 4);
    diskIdTchar[0] = diskID[0];
    diskIdTchar[1] = diskID[1];
    diskIdTchar[2] = diskID[2];
    diskIdTchar[3] = diskID[3];
    diskIdTchar[4] = 0;

    char* ansiPtr = (char*)bnr->comments[0].longTitle;
    TCHAR* tcharPtr = ldat.currentFileName;

    while (*ansiPtr)
    {
        *tcharPtr++ = (uint8_t)*ansiPtr++;
    }
    *tcharPtr++ = 0;

    // Convert SJIS Title to Unicode

    if (DVD::RegionById(diskID) == DVD::Region::JPN)
    {
        size_t size, chars;
        uint16_t* widePtr = SjisToUnicode(ldat.currentFileName, &size, &chars);
        uint16_t* unicodePtr;

        if (widePtr)
        {
            tcharPtr = ldat.currentFileName;
            unicodePtr = widePtr;

            while (*unicodePtr)
            {
                *tcharPtr++ = *unicodePtr++;
            }
            *tcharPtr++ = 0;

            free(widePtr);
        }
    }

    // set GameID
    _stprintf_s( ldat.gameID, sizeof(ldat.gameID), _T("%.4s%02X"),
        diskIdTchar, DVDBannerChecksum((void *)bnr) );
    free(bnr);

    return true;
}

// load any Dolwin-supported file
static void DoLoadFile(TCHAR *filename)
{
    uint32_t entryPoint = 0;
    TCHAR statusText[0x1000];
    bool bootrom = false;
    ULONGLONG s_time = GetTickCount64();

    // loading progress
    _stprintf_s(statusText, _countof(statusText), _T("Loading %s"), filename);
    SetStatusText(STATUS_ENUM::Progress, statusText);

    // load file
    if (!_tcsicmp(filename, _T("Bootrom")))
    {
        entryPoint = BOOTROM_START_ADDRESS + 0x100;
        ldat.gameID[0] = 0;
        ldat.dvd = false;
        bootrom = true;
    }
    else
    {
        TCHAR * extension = _tcsrchr(filename, _T('.'));
        
        if (!_tcsicmp(extension, _T(".dol")))
        {
            entryPoint = LoadDOL(filename);
            ldat.gameID[0] = 0;
            ldat.dvd = false;
        }
        else if (!_tcsicmp(extension, _T(".elf")))
        {
            entryPoint = LoadELF(filename);
            ldat.gameID[0] = 0;
            ldat.dvd = false;
        }
        else if (!_tcsicmp(extension, _T(".bin")))
        {
            entryPoint = LoadBIN(filename);
            ldat.gameID[0] = 0;
            ldat.dvd = false;
        }
        else if (!_tcsicmp(extension, _T(".iso")))
        {
            DVD::MountFile(filename);
            ldat.dvd = SetGameIDAndTitle(filename);
        }
        else if (!_tcsicmp(extension, _T(".gcm")))
        {
            DVD::MountFile(filename);
            ldat.dvd = SetGameIDAndTitle(filename);
        }
    }

    // file load success?
    if(entryPoint == 0 && !ldat.dvd)
    {
        UI::DolwinError( _T("Cannot load file!"),
                      _T("\'%s\'\n"),
                      filename);
    }
    else
    {
        TCHAR fullPath[MAX_PATH], drive[_MAX_DRIVE + 1], dir[_MAX_DIR], name[_MAX_PATH], ext[_MAX_EXT];

        _tsplitpath_s(filename,
            drive, _countof(drive) - 1,
            dir, _countof(dir) - 1,
            name, _countof(name) - 1,
            ext, _countof(ext) - 1);

        _stprintf_s(fullPath, _countof(fullPath) - 1, _T("%s%s"), drive, dir);
        
        // Set title to loaded executables
        if (!ldat.dvd)
        {
            _tcscpy_s(ldat.currentFileName, _countof(ldat.currentFileName) - 1, name);
        }
        else
        {
            // Title set before (SetGameIDAndTitle)
        }

        if (!bootrom)
        {
            // add new recent entry
            AddRecentFile(filename);

            // add new path to selector
            AddSelectorPath(fullPath);      // all checks are there
        }
    }

    // simulate bootrom
    if (!bootrom)
    {
        HWConfig* config = new HWConfig;
        EMUGetHwConfig(config);
        BootROM(ldat.dvd, false, config->consoleVer);
        Sleep(10);
    }

    // autoload map file
    if (!bootrom)
    {
        AutoloadMap();
    }

    // autoload patch file. called after BootROM, to allow patch of OS lomem.
    UnloadPatch();
    AutoloadPatch();

    // show boot time
    ULONGLONG e_time = GetTickCount64();
    ldat.boottime = (float)(e_time - s_time) / 1000.0f;
    _stprintf_s (statusText, _countof(statusText), _T("Boot time %1.2f sec"), ldat.boottime);
    SetStatusText(STATUS_ENUM::Progress, statusText);

    // set entrypoint (for DVD, PC will set in apploader)
    if(!ldat.dvd) Gekko::Gekko->regs.pc = entryPoint;
}

// set next file to load
void LoadFile(const TCHAR *filename)
{
    _tcscpy_s(ldat.currentFile, _countof(ldat.currentFile) - 1, filename);
    SetConfigString(USER_LASTFILE, ldat.currentFile, USER_UI);
}

void LoadFile(const char* filename)
{
    char* ansiPtr = (char *)filename;
    TCHAR* tcharPtr = ldat.currentFile;
    while (*ansiPtr)
    {
        *tcharPtr++ = *ansiPtr++;
    }
    *tcharPtr++ = 0;
    SetConfigString(USER_LASTFILE, ldat.currentFile, USER_UI);
}

// reload last file
void ReloadFile()
{
    DBReport2(DbgChannel::Info,
        "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n"
        "GC File Loader.\n"
        "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n\n"
    );

    if(_tcslen(ldat.currentFile))
    {
        DBReport2(DbgChannel::Loader, "Loading file: \"%s\"\n\n", Debug::Hub.TcharToString(ldat.currentFile).c_str());
        DoLoadFile(ldat.currentFile);
    }
}
