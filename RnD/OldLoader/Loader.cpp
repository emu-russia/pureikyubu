/* Supported formats are :                                      */
/*      .dol        - GAMECUBE custom executable                */
/*      .elf        - standard executable                       */
/*      .bin        - binary file (loaded at BINORG offset)     */
/*      .gcm        - game master data (GC DVD images)          */
#include "pch.h"

using namespace Debug;

/* All loader variables are placed here */
LoaderData ldat;

/* ---------------------------------------------------------------------------  */
/* DOL loader                                                                   */

/* Return DOL body size (text + data) */
uint32_t DOLSize(DolHeader *dol)
{
    uint32_t totalBytes = 0;

    for (int i = 0; i < DOL_NUM_TEXT; i++)
    {
        if (dol->textOffset[i])
        {
            /* Aligned to 32 bytes */
            totalBytes += (dol->textSize[i] + 31) & ~31;
        }
    }

    for (int i = 0; i < DOL_NUM_DATA; i++)
    {
        if (dol->dataOffset[i])
        {
            /* Aligned to 32 bytes */
            totalBytes += (dol->dataSize[i] + 31) & ~31;
        }
    }

    return totalBytes;
}

/* Return DOL entrypoint, or 0 if cannot load                           */
/* we dont need to translate address, because DOL loading goes          */
/* under control of DolphinOS, so just use simple translation mask.     */
uint32_t LoadDOL(std::wstring& dolname)
{
    DolHeader   dh;

    /* Try to open file. */
    auto dol = std::ifstream(dolname, std::ifstream::binary);
    if (!dol.is_open())
    {
        return 0;
    }

    /* Load DOL header and swap it for loader. */
    dol.read((char*)&dh, sizeof(DolHeader));
    Gekko::GekkoCore::SwapArea((uint32_t*)&dh, sizeof(DolHeader));

    Report(Channel::Loader, "Loading DOL %s (%i b).\n", dolname.data(), DOLSize(&dh));

    /* Load all text (code) sections. */
    for(int i = 0; i < DOL_NUM_TEXT; i++)
    {
        if(dh.textOffset[i])    /* If offset is 0, then section is empty */
        {
            char* addr = (char*)&mi.ram[dh.textAddress[i] & RAMMASK];

            dol.seekg(dh.textOffset[i]);
            dol.read(addr, dh.textSize[i]);

            Report(Channel::Loader,
                "   text section %08X->%08X, size %i b\n",
                dh.textOffset[i],
                dh.textAddress[i], dh.textSize[i]
            );
        }
    }

    /* Load all data sections */
    for (int i = 0; i < DOL_NUM_DATA; i++)
    {
        if (dh.dataOffset[i])    /* If offset is 0, then section is empty */
        {
            char* addr = (char*)&mi.ram[dh.dataAddress[i] & RAMMASK];

            dol.seekg(dh.dataOffset[i]);
            dol.read(addr, dh.dataSize[i]);

            Report(Channel::Loader,
                "   data section %08X->%08X, size %i b\n", 
                dh.dataOffset[i],
                dh.dataAddress[i], dh.dataSize[i]
            );
        }
    }

    HWConfig* config = new HWConfig;
    assert(config);
    EMUGetHwConfig(config);
    BootROM(false, false, config->consoleVer);

    /* Setup registers. */
    Gekko::Gekko->regs.gpr[1] = 0x816ffffc;
    Gekko::Gekko->regs.gpr[13] = 0x81100000;      // Fake sda1

    // DO NOT CLEAR BSS !

    Report(Channel::Loader, "   DOL entrypoint %08X\n\n", dh.entryPoint);
    dol.close();
    return dh.entryPoint;
}

// same as LoadDOL, but DOL is mapped in memory
uint32_t LoadDOLFromMemory(DolHeader *dol, uint32_t ofs)
{
    int i;
    #define ADDPTR(p1, p2) (uint8_t *)((uint8_t*)(p1)+(uint32_t)(p2))

    // swap DOL header
    Gekko::GekkoCore::SwapArea((uint32_t *)dol, sizeof(DolHeader));

    Report(Channel::Loader, "Loading DOL from %08X (%i b).\n",
           ofs, DOLSize(dol) );

    // load all text (code) sections
    for(i=0; i<DOL_NUM_TEXT; i++)
    {
        if(dol->textOffset[i])  // if offset is 0, then section is empty
        {
            uint8_t*addr = &mi.ram[dol->textAddress[i] & RAMMASK];
            memcpy(addr, ADDPTR(dol, dol->textOffset[i]), dol->textSize[i]);

            Report(Channel::Loader,
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

            Report(Channel::Loader,
                "   data section %08X->%08X, size %i b\n", 
                ofs + dol->dataOffset[i],
                dol->dataAddress[i], dol->dataSize[i]
            );
        }
    }

    // DO NOT CLEAR BSS !

    Report(Channel::Loader, "   DOL entrypoint %08X\n\n", dol->entryPoint);

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
uint32_t LoadELF(std::wstring& elfname)
{
    unsigned long elf_entrypoint;
    ElfEhdr     hdr;
    ElfPhdr     phdr;

    auto file = std::ifstream(elfname, std::ifstream::binary);
    if (!file.is_open())
    {
        return 0;
    }

    // check header
    file.read((char*)&hdr, sizeof(ElfEhdr));
    if(CheckELFHeader(&hdr) == 0)
    {
        file.close();
        return 0;
    }

    Elf_SwapInit((hdr.e_ident[EI_DATA] == ELFDATA2LSB ? 1 : 0));

    // check file type (must be exec)
    if(Elf_SwapHalf(hdr.e_type) != ET_EXEC)
    {
        file.close();
        return 0;
    }

    elf_entrypoint = Elf_SwapAddr(hdr.e_entry);

    //
    // load all segments
    //

    file.seekg(Elf_SwapOff(hdr.e_phoff));
    for(int i = 0; i < Elf_SwapHalf(hdr.e_phnum); i++)
    {
        long old;

        file.read((char*)&phdr, sizeof(ElfPhdr));
        old = file.tellg();

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

                file.seekg(Elf_SwapOff(phdr.p_offset));
                file.read((char*)&mi.ram[vaddr & RAMMASK], vend - vaddr);
                //fread(&mi.ram[vaddr & RAMMASK], vend - vaddr, 1, f);
            }
        }

        file.seekg(old);
    }

    file.close();
    return elf_entrypoint;
}

// ---------------------------------------------------------------------------
// BIN loader

// return BINORG offset, or 0 if cannot load.
// use physical addressing!
uint32_t LoadBIN(std::wstring& binname)
{
    uint32_t org = GetConfigInt(USER_BINORG, USER_LOADER);

    /* Binary file loading address is above RAM. */
    if (org >= RAMSIZE)
    {
        return 0;
    }

    /* Try to load file. */
    auto file = std::ifstream(binname, std::ifstream::binary | std::ifstream::ate);
    
    /* Nothing to load? */
    if (!file.is_open())
    {
        return 0;
    }
    
    /* Get the size of the file. */
    auto fsize = file.tellg();
    file.seekg(std::ifstream::beg);

    /* Limit by RAMSIZE. */
    if((org + fsize) > RAMSIZE)
    {
        fsize = RAMSIZE - org;
    }

    file.read((char*)&mi.ram[org], fsize);
    file.close();

    Report(Channel::Loader, "Loaded binary file at %08X (0x%08X)\n\n", org, fsize);
    org |= 0x80000000;
    return org;     // its me =:)
}

/* ---------------------------------------------------------------------------  */
/* File loader engine                                                           */

static void AutoloadMap()
{
    // get map file name
    auto mapname = std::wstring();
    TCHAR drive[MAX_PATH], dir[MAX_PATH], name[_MAX_PATH], ext[_MAX_EXT];

    _tsplitpath_s(ldat.currentFile.data(),
        drive, _countof(drive) - 1,
        dir, _countof(dir) - 1,
        name, _countof(name) - 1,
        ext, _countof(ext) - 1);

    // Step 1: try to load map from Data directory
    if (ldat.dvd)
    {
        mapname = fmt::format(L".\\Data\\{:s}.map", ldat.gameID);
    }
    else
    {
        mapname = fmt::format(L".\\Data\\{:s}.map", name);
    }
    
    MAP_FORMAT format = LoadMAP(mapname.data());
    if (format != MAP_FORMAT::BAD) return;
 
    // Step 2: try to load map from file directory
    if (ldat.dvd)
    {
        mapname = fmt::format(L"{:s}{:s}{:s}.map", drive, dir, ldat.gameID);
    }
    else
    {
        mapname = fmt::format(L"{:s}{:s}{:s}.map", drive, dir, name);
    }

    format = LoadMAP(mapname.data());
    if (format != MAP_FORMAT::BAD) return;

    // sorry, no maps for this DVD/executable
    Report(Channel::Loader, "WARNING: MAP file doesnt exist, HLE could be impossible\n\n");

    // Step 3: make new map (find symbols)
    if(GetConfigBool(USER_MAKEMAP, USER_LOADER))
    {
        if (ldat.dvd)
        {
            mapname = fmt::format(L".\\Data\\{:s}.map", ldat.gameID);
        }
        else
        {
            mapname = fmt::format(L".\\Data\\{:s}.map", name);
        }
        
        Report(Channel::Loader, "Making new MAP file: %s\n\n", Util::WstringToString(mapname).c_str());
        MAPInit(mapname.data());
        MAPAddRange(0x80000000, 0x80000000 | RAMSIZE);  // user can wait for once :O)
        MAPFinish();
        LoadMAP(mapname.data());
    }
}

static bool SetGameIDAndTitle(std::wstring& filename)
{
    /* Load DVD banner. */
    auto bnr_ptr = DVDLoadBanner(filename);
    auto bnr = (DVDBannerPAL*)bnr_ptr.get();

    /* Get DiskID. */
    char diskID[8] = { 0 };
    TCHAR diskIdTchar[8] = { 0 };
    DVD::Seek(0);
    DVD::Read(diskID, 4);
    
    diskIdTchar[0] = diskID[0];
    diskIdTchar[1] = diskID[1];
    diskIdTchar[2] = diskID[2];
    diskIdTchar[3] = diskID[3];
    diskIdTchar[4] = 0;

    auto title = std::string_view((char*)bnr->comments[0].longTitle);
    ldat.currentFileName = Util::convert<wchar_t, char>(title);

    /* Convert SJIS Title to Unicode */

    if (DVD::RegionById(diskID) == DVD::Region::JPN)
    {
        size_t size, chars;
        uint16_t* widePtr = SjisToUnicode(ldat.currentFileName.data(), &size, &chars);
        uint16_t* unicodePtr;

        if (widePtr)
        {
            auto tcharPtr = ldat.currentFileName.data();
            unicodePtr = widePtr;

            while (*unicodePtr)
            {
                *tcharPtr++ = *unicodePtr++;
            }
            *tcharPtr++ = 0;

            free(widePtr);
        }
    }

    /* Set GameID. */
    ldat.gameID = fmt::sprintf(L"%.4s%02X", diskIdTchar, DVDBannerChecksum(bnr));
    return true;
}

/* Load any Dolwin-supported file */
static void DoLoadFile(std::wstring& filename)
{
    uint32_t entryPoint = 0;
    bool bootrom = false;
    ULONGLONG s_time = GetTickCount64();

    /* Loading progress */
    auto statusText = fmt::format(L"Loading {:s}", filename);
    SetStatusText(STATUS_ENUM::Progress, statusText);

    // load file
    if (filename == L"Bootrom")
    {
        entryPoint = BOOTROM_START_ADDRESS + 0x100;
        ldat.gameID[0] = 0;
        ldat.dvd = false;
        bootrom = true;
    }
    else
    {
        TCHAR * extension = _tcsrchr((wchar_t*)filename.data(), _T('.'));
        
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

    /* File load success? */
    if(entryPoint == 0 && !ldat.dvd)
    {
        UI::DolwinError( _T("Cannot load file!"),
                      _T("\'%s\'\n"),
                      filename);
    }
    else
    {
        TCHAR fullPath[MAX_PATH], drive[_MAX_DRIVE + 1], dir[_MAX_DIR], name[_MAX_PATH], ext[_MAX_EXT];

        _tsplitpath_s(filename.data(),
            drive, _countof(drive) - 1,
            dir, _countof(dir) - 1,
            name, _countof(name) - 1,
            ext, _countof(ext) - 1);

        _stprintf_s(fullPath, _countof(fullPath) - 1, _T("%s%s"), drive, dir);
        
        // Set title to loaded executables
        if (!ldat.dvd)
        {
            ldat.currentFileName = name;
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

    // show boot time
    ULONGLONG e_time = GetTickCount64();
    ldat.boottime = (float)(e_time - s_time) / 1000.0f;
    statusText = fmt::format(L"Boot time {:.2f} sec", ldat.boottime);
    
    SetStatusText(STATUS_ENUM::Progress, statusText);

    // set entrypoint (for DVD, PC will set in apploader)
    if(!ldat.dvd) Gekko::Gekko->regs.pc = entryPoint;
}

// set next file to load
void LoadFile(std::wstring& filename)
{
    ldat.currentFile = filename;
    SetConfigString(USER_LASTFILE, ldat.currentFile, USER_UI);
}

void LoadFile(std::string& filename)
{
    char* ansiPtr = (char*)filename.data();
    TCHAR* tcharPtr = ldat.currentFile.data();
    while (*ansiPtr)
    {
        *tcharPtr++ = *ansiPtr++;
    }
    *tcharPtr++ = 0;
    
    SetConfigString(USER_LASTFILE, ldat.currentFile, USER_UI);
}
