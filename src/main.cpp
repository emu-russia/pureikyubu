// Emulator controls
#include "pch.h"

using namespace Debug;

// Emulator state
Emulator emu;

Gekko::GekkoCore *Core;

void EMUGetHwConfig(HWConfig * config)
{
	memset(config, 0, sizeof(HWConfig));

	Json::Value* renderTarget = JDI::Hub.ExecuteFast("GetRenderTarget");

	if (renderTarget == nullptr)
	{
		config->renderTarget = nullptr;
	}
	else
	{
		config->renderTarget = (void*)renderTarget->value.AsInt;
		delete renderTarget;
	}

	config->ramsize = RAMSIZE;

	config->vi_log = GetConfigBool(USER_VI_LOG, USER_HW);
	config->vi_xfb = GetConfigBool(USER_VI_XFB, USER_HW);

	config->videoEncoderFuse = 0;

	config->rswhack = GetConfigBool(USER_PI_RSWHACK, USER_HW);
	config->consoleVer = GetConfigInt(USER_CONSOLE, USER_HW);

	config->exi_log = GetConfigBool(USER_EXI_LOG, USER_HW);
	config->exi_osReport = GetConfigBool(USER_OS_REPORT, USER_HW);

	wcscpy (config->ansiFilename, GetConfigString(USER_ANSI, USER_HW));
	wcscpy (config->sjisFilename, GetConfigString(USER_SJIS, USER_HW));

	config->MemcardA_Connected = GetConfigBool(MemcardA_Connected_Key, USER_MEMCARDS);
	config->MemcardB_Connected = GetConfigBool(MemcardB_Connected_Key, USER_MEMCARDS);
	wcscpy (config->MemcardA_Filename, GetConfigString(MemcardA_Filename_Key, USER_MEMCARDS));
	wcscpy (config->MemcardB_Filename, GetConfigString(MemcardB_Filename_Key, USER_MEMCARDS));
	config->Memcard_SyncSave = GetConfigBool(Memcard_SyncSave_Key, USER_MEMCARDS);

	if (!Util::FileExists(config->MemcardA_Filename))
	{
		config->MemcardA_Connected = false;
	}

	if (!Util::FileExists(config->MemcardB_Filename))
	{
		config->MemcardB_Connected = false;
	}

	wcscpy (config->BootromFilename, GetConfigString(USER_BOOTROM, USER_HW));
	wcscpy (config->DspDromFilename, GetConfigString(USER_DSP_DROM, USER_HW));
	wcscpy (config->DspIromFilename, GetConfigString(USER_DSP_IROM, USER_HW));
}

// this function calls every time, after user loading new file
void EMUOpen(const std::wstring& filename)
{
	if (emu.loaded)
	{
		return;
	}

	Debug::Log = new Debug::EventLog();

	// open other sub-systems
	Core->Reset();
	HWConfig* hwconfig = new HWConfig;
	EMUGetHwConfig(hwconfig);
	Flipper::HW = new Flipper::Flipper(hwconfig);
	delete hwconfig;

	LoadFile(filename);   // Gekko PC will be set here
	HLEOpen();

	Debug::g_PerfCounters->ResetAllCounters();

	emu.loaded = true;
	emu.lastLoaded = filename;
}

// this function calls every time, after user stops emulation
void EMUClose()
{
	if (!emu.loaded)
	{
		return;
	}

	HLEClose();

	Core->Suspend();
	Core->Reset();

	delete Flipper::HW;
	Flipper::HW = nullptr;

	delete Debug::Log;
	Debug::Log = nullptr;

	emu.loaded = false;
}

// reset emulator
void EMUReset()
{
	bool runningBefore = Core->IsRunning();
	EMUClose();
	EMUOpen(emu.lastLoaded);
	if (runningBefore)
	{
		EMURun();
	}
}

void EMUCtor()
{
	if (emu.init)
	{
		return;
	}
	JDI::Hub.AddNode(DEBUGGER_JDI_JSON, Debug::Reflector);
	JDI::Hub.AddNode(GEKKO_CORE_JDI_JSON, Debug::gekko_init_handlers);
	Core = new Gekko::GekkoCore();
	Flipper::DSP = new DSP::Dsp16();
	Flipper::Gx = new GX::GXCore();
	JDI::Hub.AddNode(EMU_JDI_JSON, EmuReflector);
	DVD::InitSubsystem();
	HLEInit();
	Debug::g_PerfCounters = new Debug::PerfCounters();
	emu.init = true;
}

void EMUDtor()
{
	if (!emu.init)
	{
		return;
	}
	JDI::Hub.RemoveNode(EMU_JDI_JSON);
	DVD::Unmount();
	DVD::ShutdownSubsystem();
	delete Core;
	Core = nullptr;
	delete Flipper::DSP;
	Flipper::DSP = nullptr;
	delete Flipper::Gx;
	Flipper::Gx = nullptr;
	HLEShutdown();
	JDI::Hub.RemoveNode(GEKKO_CORE_JDI_JSON);
	JDI::Hub.RemoveNode(DEBUGGER_JDI_JSON);
	delete Debug::g_PerfCounters;
	emu.init = false;
}

/// <summary>
/// Run Gekko
/// </summary>
void EMURun()
{
	if (!emu.loaded)
	{
		return;
	}

	if (!Core->IsRunning())
	{
		Core->Run();
	}
}

/// <summary>
/// Stop Gekko
/// </summary>
void EMUStop()
{
	if (!emu.loaded)
	{
		return;
	}

	if (Core->IsRunning())
	{
		Core->Suspend();
	}
}

// Emu commands

static Json::Value* EmuFileLoad(std::vector<std::string>& args)
{
	FILE* f;

	f = fopen(args[1].c_str(), "rb");
	if (!f)
	{
		Report(Channel::Error, "Failed to open: %s\n", args[1].c_str());
		return nullptr;
	}

	Json::Value* output = new Json::Value();
	output->type = Json::ValueType::Array;

	while (!feof(f))
	{
		uint8_t AsByte = 0;
		fread(&AsByte, 1, 1, f);
		output->AddInt(nullptr, AsByte);
	}

	fclose(f);
	Report(Channel::Norm, "Loaded: %s (%zi bytes)\n", args[1].c_str(), output->children.size());

	return output;
}

static Json::Value* EmuFileSave(std::vector<std::string>& args)
{
	std::vector<std::string> cmdArgs;

	cmdArgs.insert(cmdArgs.begin(), args.begin() + 2, args.end());

	Json::Value* input = JDI::Hub.Execute(cmdArgs);
	if (input)
	{
		if (input->type != Json::ValueType::Array)
		{
			Report(Channel::Error, "Command returned invalid output (must be Array)\n");
			delete input;
			return nullptr;
		}

		FILE* f;

		f = fopen(args[1].c_str(), "wb");
		if (!f)
		{
			Report(Channel::Error, "Failed to create file: %s\n", args[1].c_str());
		}

		for (auto it = input->children.begin(); it != input->children.end(); ++it)
		{
			Json::Value* child = *it;

			if (child->type == Json::ValueType::Int)
			{
				uint8_t AsByte = (uint8_t)child->value.AsInt;
				fwrite(&AsByte, 1, 1, f);
			}
			else if (child->type == Json::ValueType::String)
			{
				size_t size = wcslen(child->value.AsString);
				fwrite(child->value.AsString, sizeof(wchar_t), size, f);
			}

			// Skip other types for now
		}

		fclose(f);
		Report(Channel::Norm, "Saved as: %s\n", args[1].c_str());
	}

	return nullptr;
}

// Sleep specified number of milliseconds
static Json::Value* CmdSleep(std::vector<std::string>& args)
{
	Thread::Sleep(atoi(args[1].c_str()));
	return nullptr;
}

// Exit
static Json::Value* CmdExit(std::vector<std::string>& args)
{
	Report(Channel::Norm, ": exiting...\n");
	EMUClose();
	EMUDtor();
	exit(0);
}

static Json::Value* CmdLoad(std::vector<std::string>& args)
{
	if (args[1] != "Bootrom")
	{
		if (!Util::FileExists(args[1]))
		{
			Report(Channel::Norm, "file not exist! filepath=%s\n", args[1].c_str());
			return nullptr;
		}
	}

	EMUClose();
	EMUOpen(Util::StringToWstring(args[1]));

	return nullptr;
}

static Json::Value* CmdUnload(std::vector<std::string>& args)
{
	if (emu.loaded)
	{
		EMUClose();
	}
	else Report(Channel::Norm, "not loaded.\n");
	return nullptr;
}

static Json::Value* CmdReset(std::vector<std::string>& args)
{
	EMUReset();
	return nullptr;
}

// Return true if emulation state is `Loaded`
static Json::Value* CmdIsLoadedInternal(std::vector<std::string>& args)
{
	Json::Value* output = new Json::Value();
	output->type = Json::ValueType::Bool;

	output->value.AsBool = emu.loaded;
	
	return output;
}

static Json::Value* CmdGetLoadedInternal(std::vector<std::string>& args)
{
	if (!emu.loaded)
		return nullptr;

	Json::Value* output = new Json::Value();
	output->type = Json::ValueType::Object;

	output->AddString("loaded", emu.lastLoaded.c_str());

	return output;
}

// Get emulator version
static Json::Value* CmdGetVersionInternal(std::vector<std::string>& args)
{
	Json::Value* output = new Json::Value();
	output->type = Json::ValueType::Array;

	output->AddString(nullptr, EMU_VERSION);

	return output;
}

static Json::Value* CmdGetConfig(std::vector<std::string>& args)
{
	Report(Channel::Norm, "%s = %s\n", USER_ANSI, Util::WstringToString(GetConfigString(USER_ANSI, USER_HW)).c_str());
	Report(Channel::Norm, "%s = %s\n", USER_SJIS, Util::WstringToString(GetConfigString(USER_SJIS, USER_HW)).c_str());
	Report(Channel::Norm, "%s = 0x%08X\n", USER_CONSOLE, GetConfigInt(USER_CONSOLE, USER_HW));
	Report(Channel::Norm, "%s = %i\n", USER_OS_REPORT, GetConfigBool(USER_OS_REPORT, USER_HW));
	Report(Channel::Norm, "%s = %i\n", USER_PI_RSWHACK, GetConfigBool(USER_PI_RSWHACK, USER_HW));
	Report(Channel::Norm, "%s = %i\n", USER_VI_XFB, GetConfigBool(USER_VI_XFB, USER_HW));

	Report(Channel::Norm, "%s = %s\n", USER_BOOTROM, Util::WstringToString(GetConfigString(USER_BOOTROM, USER_HW)).c_str());
	Report(Channel::Norm, "%s = %s\n", USER_DSP_DROM, Util::WstringToString(GetConfigString(USER_DSP_DROM, USER_HW)).c_str());
	Report(Channel::Norm, "%s = %s\n", USER_DSP_IROM, Util::WstringToString(GetConfigString(USER_DSP_IROM, USER_HW)).c_str());

	Report(Channel::Norm, "%s = %i\n", USER_EXI_LOG, GetConfigBool(USER_EXI_LOG, USER_HW));
	Report(Channel::Norm, "%s = %i\n", USER_VI_LOG, GetConfigBool(USER_VI_LOG, USER_HW));

	return nullptr;
}

static Json::Value* CmdGetConfigString(std::vector<std::string>& args)
{
	wchar_t* param = GetConfigString(args[2].c_str(), args[1].c_str());

	Json::Value* output = new Json::Value();
	output->type = Json::ValueType::Array;

	output->AddString(nullptr, param);

	return output;
}

static Json::Value* CmdSetConfigString(std::vector<std::string>& args)
{
	SetConfigString(args[2].c_str(), Util::StringToWstring(args[3]).c_str(), args[1].c_str());
	return nullptr;
}

static Json::Value* CmdGetConfigInt(std::vector<std::string>& args)
{
	int param = GetConfigInt(args[2].c_str(), args[1].c_str());

	Json::Value* output = new Json::Value();
	output->type = Json::ValueType::Array;

	output->AddInt(nullptr, param);

	return output;
}

static Json::Value* CmdSetConfigInt(std::vector<std::string>& args)
{
	SetConfigInt(args[2].c_str(), atoi(args[3].c_str()), args[1].c_str());
	return nullptr;
}

static Json::Value* CmdGetConfigBool(std::vector<std::string>& args)
{
	bool param = GetConfigBool(args[2].c_str(), args[1].c_str());

	Json::Value* output = new Json::Value();
	output->type = Json::ValueType::Array;

	output->AddBool(nullptr, param);

	return output;
}

static Json::Value* CmdSetConfigBool(std::vector<std::string>& args)
{
	SetConfigBool(args[2].c_str(), args[3] == "true" ? true : false, args[1].c_str());
	return nullptr;
}

void EmuReflector()
{
	JDI::Hub.AddCmd("FileLoad", EmuFileLoad);
	JDI::Hub.AddCmd("FileSave", EmuFileSave);
	JDI::Hub.AddCmd("sleep", CmdSleep);
	JDI::Hub.AddCmd("exit", CmdExit);
	JDI::Hub.AddCmd("quit", CmdExit);
	JDI::Hub.AddCmd("x", CmdExit);
	JDI::Hub.AddCmd("q", CmdExit);
	JDI::Hub.AddCmd("load", CmdLoad);
	JDI::Hub.AddCmd("unload", CmdUnload);
	JDI::Hub.AddCmd("reset", CmdReset);
	JDI::Hub.AddCmd("IsLoaded", CmdIsLoadedInternal);
	JDI::Hub.AddCmd("GetLoaded", CmdGetLoadedInternal);
	JDI::Hub.AddCmd("GetVersion", CmdGetVersionInternal);

	JDI::Hub.AddCmd("GetConfig", CmdGetConfig);
	JDI::Hub.AddCmd("GetConfigString", CmdGetConfigString);
	JDI::Hub.AddCmd("SetConfigString", CmdSetConfigString);
	JDI::Hub.AddCmd("GetConfigInt", CmdGetConfigInt);
	JDI::Hub.AddCmd("SetConfigInt", CmdSetConfigInt);
	JDI::Hub.AddCmd("GetConfigBool", CmdGetConfigBool);
	JDI::Hub.AddCmd("SetConfigBool", CmdSetConfigBool);
}


/* Supported formats are :                                      */
/*      .dol        - GAMECUBE custom executable                */
/*      .elf        - standard executable                       */
/*      .bin        - binary file (loaded at BINORG offset)     */
/*      .gcm        - game master data (GC DVD images)          */

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
uint32_t LoadDOL(const std::wstring& dolname)
{
	DolHeader   dh;

	/* Try to open file. */
	auto dol = std::ifstream( Util::WstringToString(dolname).c_str(), std::ifstream::binary);
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
	EMUGetHwConfig(config);
	BootROM(false, false, config->consoleVer);

	/* Setup registers. */
	Core->regs.gpr[1] = 0x816ffffc;
	Core->regs.gpr[13] = 0x81100000;      // Fake sda1

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

// swapping endiannes.

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
uint32_t LoadELF(const std::wstring& elfname)
{
	unsigned long elf_entrypoint;
	ElfEhdr     hdr;
	ElfPhdr     phdr;

	auto file = std::ifstream(Util::WstringToString(elfname).c_str(), std::ifstream::binary);
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
		std::streampos old;

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
uint32_t LoadBIN(const std::wstring& binname)
{
	uint32_t org = GetConfigInt(USER_BINORG, USER_LOADER);

	/* Binary file loading address is above RAM. */
	if (org >= RAMSIZE)
	{
		return 0;
	}

	/* Try to load file. */
	auto file = std::ifstream(Util::WstringToString(binname).c_str(), std::ifstream::binary | std::ifstream::ate);
	
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

static void AutoloadMap(const std::wstring & filename, bool dvd, std::wstring & diskId)
{
	// get map file name
	auto mapname = std::wstring();
	char drive[0x100], dir[0x1000], name[0x100], ext[0x100];

	Util::SplitPath(Util::WstringToString(filename).c_str(),
		drive, 
		dir, 
		name,
		ext);

	// Step 1: try to load map from Data directory
	if (dvd)
	{
		mapname = fmt::format(L"./Data/{:s}.map", diskId);
	}
	else
	{
		mapname = fmt::format(L"./Data/{:s}.map", Util::StringToWstring(name));
	}
	
	MAP_FORMAT format = LoadMAP(mapname.data());
	if (format != MAP_FORMAT::BAD) return;
 
	// Step 2: try to load map from file directory
	if (dvd)
	{
		mapname = fmt::format(L"{:s}{:s}{:s}.map", Util::StringToWstring(drive), Util::StringToWstring(dir), diskId);
	}
	else
	{
		mapname = fmt::format(L"{:s}{:s}{:s}.map", Util::StringToWstring(drive), Util::StringToWstring(dir), Util::StringToWstring(name));
	}

	format = LoadMAP(mapname.data());
	if (format != MAP_FORMAT::BAD) return;

	// sorry, no maps for this DVD/executable
	Report(Channel::Loader, "WARNING: MAP file doesnt exist, HLE could be impossible\n\n");

	// Step 3: make new map (find symbols)
	if(GetConfigBool(USER_MAKEMAP, USER_LOADER))
	{
		if (dvd)
		{
			mapname = fmt::format(L"./Data/{:s}.map", diskId);
		}
		else
		{
			mapname = fmt::format(L"./Data/{:s}.map", Util::StringToWstring(name));
		}
		
		Report(Channel::Loader, "Making new MAP file: %s\n\n", Util::WstringToString(mapname).c_str());
		MAPInit(mapname.data());
		MAPAddRange(0x80000000, 0x80000000 | RAMSIZE);  // user can wait for once :O)
		MAPFinish();
		LoadMAP(mapname.data());
	}
}

/* Get DiskID. */
void GetDiskId(std::wstring& diskId)
{
	char diskID[8] = { 0 };
	wchar_t diskIdWchar[8] = { 0 };
	DVD::Seek(0);
	DVD::Read(diskID, 4);

	diskIdWchar[0] = diskID[0];
	diskIdWchar[1] = diskID[1];
	diskIdWchar[2] = diskID[2];
	diskIdWchar[3] = diskID[3];
	diskIdWchar[4] = 0;

	diskId = fmt::sprintf(L"%.4s", diskIdWchar);
}

/* Load any supported file */
void LoadFile(const std::wstring& filename)
{
	uint32_t entryPoint = 0;
	bool bootrom = false;
	bool dvd = false;
	std::wstring diskId;

	// load file
	if (filename == L"Bootrom")
	{
		entryPoint = BOOTROM_START_ADDRESS + 0x100;
		dvd = false;
		bootrom = true;
	}
	else
	{
		wchar_t* extension = wcsrchr((wchar_t*)filename.c_str(), L'.');
		
		if (!_wcsicmp(extension, L".dol"))
		{
			entryPoint = LoadDOL(filename);
			dvd = false;
		}
		else if (!_wcsicmp(extension, L".elf"))
		{
			entryPoint = LoadELF(filename);
			dvd = false;
		}
		else if (!_wcsicmp(extension, L".bin"))
		{
			entryPoint = LoadBIN(filename);
			dvd = false;
		}
		else if (!_wcsicmp(extension, L".iso"))
		{
			DVD::MountFile(filename);
			GetDiskId(diskId);
			dvd = true;
		}
		else if (!_wcsicmp(extension, L".gcm"))
		{
			DVD::MountFile(filename);
			GetDiskId(diskId);
			dvd = true;
		}
	}

	/* File load success? */
	if(entryPoint == 0 && !dvd)
	{
		throw "Cannot load file!";
	}

	// simulate bootrom
	if (!bootrom)
	{
		HWConfig* config = new HWConfig;
		EMUGetHwConfig(config);
		BootROM(dvd, false, config->consoleVer);
		delete config;
		Thread::Sleep(10);
	}

	// autoload map file
	if (!bootrom)
	{
		AutoloadMap(filename, dvd, diskId);
	}

	// set entrypoint (for DVD, PC will set in apploader)
	if (!dvd)
	{
		Core->regs.pc = entryPoint;
	}

	// There is Fuse on the motherboard, which determines the video encoder mode. 
	// Some games test it in VIConfigure and try to set the mode according to Fuse. But the program code does not allow this (example - Zelda PAL Version)
	// https://www.ifixit.com/Guide/Nintendo+GameCube+Regional+Modification+Selector+Switch/35482
	if (dvd)
	{
		char id[4] = { 0 };

		DVD::Seek(0);
		DVD::Read(id, 4);

		DVD::Region region = DVD::RegionById(id);
		VISetEncoderFuse(DVD::IsNtsc(region) ? 0 : 1);
	}
}
