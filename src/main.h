// Emulator main deck control
#pragma once

#define EMU_VERSION L"1.8"

void    EMUGetHwConfig(HWConfig* config);

// emulator controls API
void    EMUCtor();
void    EMUDtor();
void    EMUOpen(const std::wstring& filename);    // Power up system
void    EMUClose();         // Power down system
void    EMUReset();         // Reset
void    EMURun();           // Run Gekko
void    EMUStop();          // Stop Gekko

Thread* EMUCreateThread(ThreadProc threadProc, bool suspended, void* context, const char* name);
void EMUJoinThread(Thread* thread);

// all important data is placed here
struct Emulator
{
	bool    init;
	bool    loaded;         // file loaded
	std::wstring lastLoaded;
	bool	bootrom;		// The emulator is running in Bootrom runtime mode
};

extern  Emulator emu;

// Command line options (parsed once, at the very start of the application)

struct CmdLineOptions
{
	bool    help = false;       // `--help`: print the accepted options and exit
	bool    ipl = false;        // `--ipl`: start the Bootrom (IPL) immediately, without going through the UI
	bool    noDisc = false;     // `--no-disc`: start with the DVD lid open (no disk), so that the IPL takes its "no disk" path

	// `--image <file>` (or a bare file name): load and run that file right away instead of waiting
	// for the game selector. The file is a disk image (`.iso`, `.gcm`, `.rvz`) or an executable
	// (`.dol`, `.elf`).
	std::wstring image;

	// `--bench <file> [seconds]`: load the file, run it for the given number of seconds (30 by
	// default) without a user interface and print the measured emulation throughput and the
	// performance counters. This is the measurement tool for the "find the bottleneck" work:
	// the counters say where the emulated CPU time actually goes.
	bool    bench = false;
	std::wstring benchFile;
	uint32_t    benchSeconds = 30;
};

extern  CmdLineOptions cmdline;

/// <summary>
/// Print the accepted command line options (the `--help` text) to the console, if there is one.
/// </summary>
void EMUPrintUsage();

/// <summary>
/// Parse the raw command line of the application (`lpCmdLine` on Windows, `argv` joined by spaces elsewhere).
/// </summary>
/// <param name="commandLine">Raw command line, without the executable name. May be nullptr.</param>
void EMUParseCmdLine(const char* commandLine);

/// <summary>
/// Same as EMUParseCmdLine, but for the `argv`/`argc` pair of the portable entry point.
/// </summary>
void EMUParseCmdLine(int argc, char** argv);

// Emu debug commands

void EmuReflector();


// The format of executable files used in GAMECUBE

// DOL format definitions

#define DOL_NUM_TEXT 7
#define DOL_NUM_DATA 11

struct DolHeader
{
	uint32_t textOffset[DOL_NUM_TEXT];
	uint32_t dataOffset[DOL_NUM_DATA];

	uint32_t textAddress[DOL_NUM_TEXT];
	uint32_t dataAddress[DOL_NUM_DATA];

	uint32_t textSize[DOL_NUM_TEXT];
	uint32_t dataSize[DOL_NUM_DATA];

	uint32_t bssAddress;
	uint32_t bssSize;
	uint32_t entryPoint;
	uint32_t padd[7];
};

// ELF format definitions (sufficient to load files of this format)

using ElfAddr = unsigned long;
using ElfOff = unsigned long;
using ElfHalf = unsigned short;
using ElfWord = unsigned long;
using ElfSword = long;

struct ElfEhdr
{
	uint8_t e_ident[16];

	ElfHalf e_type;
	ElfHalf e_machine;
	ElfWord e_version;
	ElfAddr e_entry;
	ElfOff  e_phoff;

	ElfOff  e_shoff;
	ElfWord e_flags;
	ElfHalf e_ehsize;
	ElfHalf e_phentsize;
	ElfHalf e_phnum;
	ElfHalf e_shentsize;

	ElfHalf e_shnum;
	ElfHalf e_shstrndx;
};

struct ElfPhdr
{
	ElfWord p_type;
	ElfOff  p_offset;
	ElfAddr p_vaddr;
	ElfAddr p_paddr;
	ElfWord p_filesz;
	ElfWord p_memsz;
	ElfWord p_flags;
	ElfWord p_align;
};

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


// Loader API

/// <summary>
/// Load any supported file
/// </summary>
/// <param name="filename"></param>
void LoadFile(const std::wstring& filename);