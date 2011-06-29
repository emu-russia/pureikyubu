// ---------------------------------------------------------------------------
// DOL format definitions

#define DOL_NUM_TEXT    7
#define DOL_NUM_DATA    11

typedef struct DolHeader
{
    u32     textOffset[DOL_NUM_TEXT];
    u32     dataOffset[DOL_NUM_DATA];

    u32     textAddress[DOL_NUM_TEXT];
    u32     dataAddress[DOL_NUM_DATA];

    u32     textSize[DOL_NUM_TEXT];
    u32     dataSize[DOL_NUM_DATA];

    u32     bssAddress;
    u32     bssSize;
    u32     entryPoint;
    u32     padd[7];
} DolHeader;

u32     DOLSize(DolHeader *dol);
u32     LoadDOL(char *dolname);
u32     LoadDOLFromMemory(DolHeader *dol, u32 ofs);

// ---------------------------------------------------------------------------
// ELF format definitions

typedef unsigned long       ElfAddr;
typedef unsigned long       ElfOff;
typedef unsigned short      ElfHalf;
typedef unsigned long       ElfWord;
typedef long                ElfSword;

typedef struct ElfEhdr
{
    unsigned char   e_ident[16];

    ElfHalf         e_type;
    ElfHalf         e_machine;
    ElfWord         e_version;
    ElfAddr         e_entry;
    ElfOff          e_phoff;

    ElfOff          e_shoff;
    ElfWord         e_flags;
    ElfHalf         e_ehsize;
    ElfHalf         e_phentsize;
    ElfHalf         e_phnum;    
    ElfHalf         e_shentsize;

    ElfHalf         e_shnum;
    ElfHalf         e_shstrndx;
} ElfEhdr;

enum ELF_IDENT
{
    EI_MAG0 = 0,
    EI_MAG1,
    EI_MAG2,
    EI_MAG3,
    EI_CLASS,
    EI_DATA,
    EI_VERSION,
    EI_OSABI,
    EI_ABIVERSION,
    EI_PAD,
    EI_NIDENT = 16,
};

#define ELFCLASS32  1
#define ELFCLASS64  2

#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

#define ET_NONE     0
#define ET_REL      1
#define ET_EXEC     2
#define ET_DYN      3
#define ET_CORE     4
#define ET_LOOS     0xfe00
#define ET_HIOS     0xfeff
#define ET_LOPROC   0xff00
#define ET_HIPROC   0xffff

typedef struct ElfPhdr
{
    ElfWord         p_type;
    ElfOff          p_offset;
    ElfAddr         p_vaddr;
    ElfAddr         p_paddr;
    ElfWord         p_filesz;
    ElfWord         p_memsz;
    ElfWord         p_flags;
    ElfWord         p_align;
} ElfPhdr;

#define PT_NULL     0
#define PT_LOAD     1
#define PT_DYNAMIC  2
#define PT_INTERP   3
#define PT_NOTE     4
#define PT_SHLIB    5
#define PT_PHDR     6
#define PT_LOOS     0x60000000
#define PT_HIOS     0x6fffffff
#define PT_LOPROC   0x70000000
#define PT_HIPROC   0x7fffffff

#define PF_X        0x1
#define PF_W        0x2
#define PF_R        0x4
#define PF_MASKOS   0x00ff0000
#define PF_MASKPROC 0xff000000

u32     LoadELF(char *elfname);

// ---------------------------------------------------------------------------
// load binary file

u32     LoadBIN(char *binname);

// ---------------------------------------------------------------------------
// patch file definitions (*.patch is new one, not old *.ppf)
// you may ignore whole patch code, since it's definitely only for org :)

// patch file is an array of following structures.
// number of patches in *.patch file = FileSize(file) / sizeof(Patch)

// size of patch data (simply we use unswapped 'dataSize' value)
#define PATCH_SIZE_8    0x0100
#define PATCH_SIZE_16   0x0200
#define PATCH_SIZE_32   0x0400
#define PATCH_SIZE_64   0x0800

// data in patch is in big-endian PPC format
#pragma pack(1)
typedef struct Patch
{
    u32     effectiveAddress;       // CPU effective address to apply patch there
    u16     freeze;                 // apply patch after every VI frame, if 1
    u16     dataSize;               // size of data to write at address (in bytes)
    u64     data;                   // patch data (size : 1, 2, 4 or 8 bytes)
                                    // data bytes are started right from this offset,
                                    // not matter what size of data is applied.
                                    // unused bytes of data can be used as small comment

    // example patch : put 'blr' opcode at 0x8000B494 (for FreeLoader OSInitAudioSystem).
    // 00000: 80 00 B4 94 00 00 00 04 4E 80 00 20 00 00 00 00
    //        |address  |freeze|size |blr opcode |dummy data|
} Patch;
#pragma pack()

BOOL    LoadPatch(char * patchname, BOOL add=FALSE);
void    ApplyPatches(BOOL load=0, s32 a=0, s32 b=-1);
void    UnloadPatch();

// ---------------------------------------------------------------------------
// loader

// loader API
void    LoadFile(char *filename);
void    ReloadFile();

// all loader variables are placed here
typedef struct LoaderData
{
    BOOL    cmdline;                // loaded from command line ?
    char    cwd[1024];              // current working directory
    u32     binOffset;              // binary file loading offset
    BOOL    dvd;                    // 1, if loaded file is DVD image
    char    gameID[16];             // GameID, for dvd
    char    currentFile[MAX_PATH];  // next file to be loaded or re-loaded
    char    currentFileName[256];   // name of loaded file (without extension)
    f32     boottime;               // in seconds

    BOOL    enablePatch;            // allow patches, if 1
    u32     patchNum;               // number of patches in table
    Patch*  patches;                // patch table

    // identify game for hacks
    BOOL    freeLoader;
    BOOL    actionReplay;
} LoaderData;

extern  LoaderData ldat;
