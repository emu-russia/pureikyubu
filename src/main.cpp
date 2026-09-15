// Emulator controls
#include "pch.h"

// The GBA mode of the application (issue #388): its own machine and its own SDL2 frontend. The
// frontend opens a window of its own, so the headless build (GFX_NULL) does not compile it in at
// all - it is the one part of the application that cannot exist without SDL.
#ifndef GFX_NULL
#include "gba/gba_sdl.h"
#endif

using namespace Debug;

// Emulator state
Emulator emu;

// Command line options

CmdLineOptions cmdline;

// Arguments that carry their own parameters (`--bench <file> [seconds]`) need a lookahead, so the
// raw command line is first split into a token list and then walked here.
static void ParseCmdLineArgs(const std::vector<std::string>& args)
{
	for (size_t i = 0; i < args.size(); i++)
	{
		const std::string& arg = args[i];

		if (arg == "--help" || arg == "-h" || arg == "-?" || arg == "/?")
		{
			cmdline.help = true;
		}
		else if (arg == "--image")
		{
			if (i + 1 < args.size())
			{
				cmdline.image = Util::StringToWstring(args[++i]);
			}
			else
			{
				Report(Channel::Norm, "--image needs a file name\n");
			}
		}
		else if (arg == "--ipl")
		{
			cmdline.ipl = true;
		}
		else if (arg == "--no-disc")
		{
			cmdline.noDisc = true;
		}
		else if (arg == "--dspjit")
		{
			cmdline.dspJit = true;
		}
		else if (arg == "--selftest")
		{
			cmdline.selftest = true;
		}
		else if (arg == "--mcp")
		{
			cmdline.mcp = true;
		}
		else if (arg == "--gba")
		{
			// `--gba [file]`: the GBA emulator takes over. The file may follow as the next
			// argument or be given later as the bare file argument.
			cmdline.gba = true;

			if (i + 1 < args.size() && args[i + 1].size() > 1 && args[i + 1][0] != '-')
			{
				cmdline.image = Util::StringToWstring(args[++i]);
			}
		}
		else if (arg == "--gba-link")
		{
			cmdline.gba = true;
			cmdline.gbaLink = true;
		}
		else if (arg == "--gba-bios")
		{
			if (i + 1 < args.size())
			{
				cmdline.gba = true;
				cmdline.gbaBios = Util::StringToWstring(args[++i]);
			}
			else
			{
				Report(Channel::Norm, "--gba-bios needs a file name\n");
			}
		}
		else if (arg == "--no-gba-bootrom")
		{
			cmdline.gba = true;
			cmdline.gbaNoBootrom = true;
		}
		else if (arg == "--gb")
		{
			// `--gb [file]`: the Game Boy (DMG/CGB) machine, for a file whose extension does not
			// say so.
			cmdline.gba = true;
			cmdline.gb = true;

			if (i + 1 < args.size() && args[i + 1].size() > 1 && args[i + 1][0] != '-')
			{
				cmdline.image = Util::StringToWstring(args[++i]);
			}
		}
		else if (arg == "--gb-dmg")
		{
			cmdline.gba = true;
			cmdline.gb = true;
			cmdline.gbDmg = true;
		}
		else if (arg == "--bench")
		{
			if (i + 1 < args.size())
			{
				cmdline.bench = true;
				cmdline.benchFile = Util::StringToWstring(args[++i]);

				// An optional second parameter is the duration in seconds. A file name can start
				// with a digit too, so the parameter is only taken when the *whole* token is a number.
				if (i + 1 < args.size())
				{
					char* end = nullptr;
					unsigned long seconds = strtoul(args[i + 1].c_str(), &end, 0);
					if (end != nullptr && *end == 0 && seconds > 0)
					{
						cmdline.benchSeconds = (uint32_t)seconds;
						i++;
					}
				}
			}
			else
			{
				Report(Channel::Norm, "--bench needs an image file (and an optional duration in seconds)\n");
			}
		}
		else if (arg.size() > 1 && arg[0] != '-')
		{
			// A bare argument is the file to run, which is the way every command line tool takes
			// it: `pureikyubu "D:\Isos\game.gcm"`.
			cmdline.image = Util::StringToWstring(arg);
		}
		else
		{
			Report(Channel::Norm, "Unknown command line argument: %s\n", arg.c_str());
		}
	}

	// A Game Boy cartridge on the command line selects the portable emulator by itself, exactly
	// like a disk image selects the GameCube one. The Game Boy Advance and the Game Boy are two
	// machines of the same module, and the extension says which one runs the file.
#ifndef GFX_NULL
	if (!cmdline.image.empty() && GBA::IsGameBoyImage(Util::WstringToString(cmdline.image)))
	{
		cmdline.gba = true;

		if (GBA::IsDmgImage(Util::WstringToString(cmdline.image)))
		{
			cmdline.gb = true;
		}
	}
#endif
}

/// <summary>
/// Run the integrated GBA emulator (issue #388) with the SDL2 frontend. The settings come from
/// build/Data/GBASettings.json, overridden by the command line.
/// </summary>
int EMURunGba()
{
#ifdef GFX_NULL
	// The portable machines are presented by a windowed SDL2 frontend of their own, which the
	// headless build does not have (and the emulation of the machine itself is not part of it).
	Report(Channel::Error, "the GBA emulator needs a window: this is the headless build\n");
	return -1;
#else
	GBA::GbaSettings settings;
	std::string usedPath;
	std::string error;

	if (!GBA::LoadSettings("", settings, usedPath, &error))
	{
		Report(Channel::Norm, "GBA: %s: %s (using the defaults)\n", usedPath.c_str(), error.c_str());
	}

	if (!cmdline.gbaBios.empty())
	{
		settings.biosPath = Util::WstringToString(cmdline.gbaBios);
		settings.useCustomBootRom = false;
	}

	if (cmdline.gbaNoBootrom)
	{
		settings.useCustomBootRom = false;
	}

	if (cmdline.gbaLink)
	{
		settings.linkEnabled = true;
	}

	Report(Channel::Norm, "GBA: settings from %s\n", usedPath.c_str());

	std::string rom = Util::WstringToString(cmdline.image);

	// The Game Boy (DMG/CGB) is the other machine of the module and has its own core; a
	// `.gb`/`.gbc`/`.sgb` cartridge (or `--gb`) selects it.
	if (cmdline.gb || (!rom.empty() && GBA::IsDmgImage(rom)))
	{
		return GBA::RunSdlFrontendGb(rom, settings, cmdline.gbDmg);
	}

	return GBA::RunSdlFrontend(rom, cmdline.gbaLink, settings);
#endif
}

// The `--help` text. It is printed to the console (when the application has one) and to the report
// log, so `EMU_LOG=<file> pureikyubu --help` also shows it.
void EMUPrintUsage()
{
	static const char* usage =
		"pureikyubu, Nintendo GameCube emulator\n"
		"\n"
		"Usage: pureikyubu [options] [file]\n"
		"\n"
		"  <file>                Load and run a file right away, without the game selector. The\n"
		"                        recognized formats are disk images (.iso, .gcm, .rvz) and\n"
		"                        executables (.dol, .elf). Quote the name when it contains spaces.\n"
		"  --image <file>        Same as the bare file argument.\n"
		"  --ipl                 Start the Bootrom (IPL) instead of waiting for the selector.\n"
		"  --no-disc             Start with the DVD lid open, so the IPL takes its \"no disk\" path.\n"
		"  --dspjit              Run the DSPcore on the experimental basic block recompiler.\n"
		"                        The default is the interpreter; see src/dspjit.h.\n"
		"  --bench <file> [sec]  Run the file unattended for the given number of seconds (30 by\n"
		"                        default) and print the throughput and the performance counters.\n"
		"  --selftest            Run the startup sequence (settings, debug interface specifications,\n"
		"                        emulated hardware, ROM and memory card files) without a window and\n"
		"                        exit with the number of failed steps as the status code.\n"
		"  --mcp                 Start the local MCP server: an MCP client (an LLM agent) starts the\n"
		"                        emulator and drives its whole debug interface over stdin/stdout, one\n"
		"                        JSON-RPC message per line. See wiki/mcp.md.\n"
		"  -h, --help            Print this text and exit.\n"
		"\n"
		"GBA emulator (issue #388):\n"
		"\n"
		"  --gba [file]          Run the integrated GBA emulator instead of the GameCube one. With a\n"
		"                        cartridge the boot ROM animation runs and the cartridge is started;\n"
		"                        with no file the link driver is started (GBA Link mode). A file\n"
		"                        whose name ends in .gba/.agb/.gb/.gbc selects this mode by itself.\n"
		"  --gba-link            Initialize the link port even when a cartridge is loaded.\n"
		"  --gba-bios <file>     Use a real 16 KByte GBA BIOS image instead of the built-in boot ROM.\n"
		"  --no-gba-bootrom      Skip the boot ROM and the BIOS: start the cartridge directly.\n"
		"\n"
		"Game Boy (DMG/CGB) emulator (the same module and frontend):\n"
		"\n"
		"  --gb [file]           Run the Game Boy machine; a .gb/.gbc/.sgb file selects it too.\n"
		"  --gb-dmg              Emulate the monochrome console instead of a CGB.\n"
		"\n"
#ifdef GFX_NULL
		"This is the headless build: there is no window and no game selector, so an image (or\n"
		"--ipl) has to be passed on the command line. Ctrl+C stops an unattended run; the reports\n"
		"go to the console and to the `EMU_LOG` file.\n";
#else
		"With no option the game selector is shown, and a file is started from there (Enter or a\n"
		"double click). File -> Reopen (F3) runs the last file again.\n";
#endif

#ifdef _WINDOWS
	// A windowed application has no console of its own; borrow the one it was started from, so
	// that the text is visible when it is run from a command prompt.
	if (AttachConsole(ATTACH_PARENT_PROCESS))
	{
		freopen("CONOUT$", "w", stdout);
	}
#endif

	printf("%s", usage);

	Report(Channel::Norm, "%s", usage);
}

/// <summary>
/// Run the emulator's startup sequence headlessly and report what failed.
///
/// The emulator has several ways of dying before its window appears: a corrupt settings file, a
/// debug interface specification that no longer parses, a ROM or memory card file that is missing
/// or malformed. Without this check all of them look the same from the outside - the process
/// disappears. Each startup step is run in its own catch-all frame and reported, and the exit code
/// is the number of steps that failed, so a build script or a human can tell "it started" from
/// "it crashed on startup" without a debugger.
/// </summary>
int EMUSelfTest()
{
#ifdef _WINDOWS
	// A windowed application has no console of its own; borrow the one it was started from, so the
	// report is visible when the check runs from a command prompt. When stdout is already a pipe,
	// a file or anything else that was handed to us (a build script capturing the output), it is
	// left alone, otherwise the report would go to the console instead of into the capture.
	HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
	bool consoleOutput = (out == nullptr || out == INVALID_HANDLE_VALUE || GetFileType(out) == FILE_TYPE_CHAR);

	if (consoleOutput && AttachConsole(ATTACH_PARENT_PROCESS))
	{
		freopen("CONOUT$", "w", stdout);
	}
#endif

	int failures = 0;

	auto step = [&failures](const char* name, auto&& body) -> bool
	{
		printf("[..] %s\n", name);
		fflush(stdout);

		int before = failures;

		try
		{
			body();
			printf("[ok] %s\n", name);
		}
		catch (const char* text)
		{
			failures++;
			printf("[!!] %s: %s\n", name, (text != nullptr) ? text : "(no message)");
			Report(Channel::Error, "[!!] %s: %s\n", name, (text != nullptr) ? text : "(no message)");
		}
		catch (const std::exception& e)
		{
			failures++;
			printf("[!!] %s: %s\n", name, e.what());
			Report(Channel::Error, "[!!] %s: %s\n", name, e.what());
		}
		catch (...)
		{
			failures++;
			printf("[!!] %s: unknown error\n", name);
			Report(Channel::Error, "[!!] %s: unknown error\n", name);
		}

		fflush(stdout);

		return failures == before;
	};

	// The debug interface specifications are parsed here and the Gekko/DSP cores are created.
	bool coreOk = step("emulator core, debug interface specifications", [] { EMUCtor(); });

	// The settings JSON - the shipped defaults merged with the user file - is read here.
	bool settingsOk = step("settings", [] { HWConfig config{}; EMUGetHwConfig(&config); });

	// The emulated machine: the Flipper devices, the DSP ROM images and the memory cards. With a
	// file on the command line that file is loaded as well; otherwise the IPL/Bootrom path is used,
	// which is the state the emulator is in before the user picks anything.
	if (coreOk && settingsOk)
	{
		std::wstring file = cmdline.image.empty() ? std::wstring(L"Bootrom") : cmdline.image;
		step("emulated hardware, ROM and memory card files", [file] { EMUOpen(file); });
	}
	else
	{
		// Starting the hardware on top of a failed core or settings step would only produce a
		// second, less readable failure.
		printf("[--] emulated hardware skipped: the core and the settings have to work first\n");
	}

	step("shutdown", [] { EMUClose(); EMUDtor(); });

	printf("selftest: %s, %i failed step(s)\n", (failures == 0) ? "the emulator starts" : "FAILED", failures);

	return failures;
}

void EMUParseCmdLine(const char* commandLine)
{
	if (commandLine == nullptr)
	{
		return;
	}

	// Split into arguments, honouring the quotes (arguments are not allowed to contain spaces otherwise).
	// An unterminated quote only swallows the rest of the line into the last argument: the walk is
	// bounded by the NUL terminator and never looks ahead, so it can neither loop nor read past it.

	std::vector<std::string> args;
	std::string arg;
	bool quoted = false;

	for (const char* p = commandLine; *p != 0; p++)
	{
		char c = *p;

		if (c == '\"' || c == '\'')
		{
			quoted = !quoted;
		}
		else if (!quoted && (c == ' ' || c == '\t'))
		{
			if (!arg.empty())
			{
				args.push_back(arg);
				arg.clear();
			}
		}
		else
		{
			arg.push_back(c);
		}
	}

	if (!arg.empty())
	{
		args.push_back(arg);
	}

	ParseCmdLineArgs(args);
}

void EMUParseCmdLine(int argc, char** argv)
{
	std::string commandLine;

	// Skip argv[0] (the executable itself)

	for (int i = 1; i < argc; i++)
	{
		if (i > 1)
		{
			commandLine.push_back(' ');
		}
		commandLine += argv[i];
	}

	EMUParseCmdLine(commandLine.c_str());
}

Gekko::GekkoCore *Core;

std::list<Thread*> emu_threads;

/// <summary>
/// Create an emulator thread. It is used to keep statistics on threads, using the `threads` command.
/// </summary>
Thread* EMUCreateThread(ThreadProc threadProc, bool suspended, void* context, const char* name)
{
	Thread *thread = new Thread(threadProc, suspended, context, name);
	emu_threads.push_back(thread);
	return thread;
}

/// <summary>
/// Stop the thread and delete from history.
/// </summary>
void EMUJoinThread(Thread* thread)
{
	emu_threads.remove(thread);
	delete thread;
}

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

	config->ramsize = 24*1024*1024;			// TODO: Make it configurable

	config->vi_log = GetConfigBool(USER_VI_LOG, USER_HW);
	config->vi_xfb = GetConfigBool(USER_VI_XFB, USER_HW);

	config->gfxPipeline = GetConfigInt(USER_GFX_PIPELINE, USER_HW);

	config->videoEncoderFuse = 0;

	config->consoleVer = GetConfigInt(USER_CONSOLE, USER_HW);
	config->pi_log = GetConfigBool(USER_PI_LOG, USER_HW);

	config->exi_log = GetConfigBool(USER_EXI_LOG, USER_HW);
	config->exi_osReport = GetConfigBool(USER_OS_REPORT, USER_HW);

	wcscpy (config->ansiFilename, GetConfigString(USER_ANSI, USER_HW));
	wcscpy (config->sjisFilename, GetConfigString(USER_SJIS, USER_HW));

	config->MemcardA_Connected = GetConfigBool(MemcardA_Connected_Key, USER_MEMCARDS);
	config->MemcardB_Connected = GetConfigBool(MemcardB_Connected_Key, USER_MEMCARDS);
	wcscpy (config->MemcardA_Filename, GetConfigString(MemcardA_Filename_Key, USER_MEMCARDS));
	wcscpy (config->MemcardB_Filename, GetConfigString(MemcardB_Filename_Key, USER_MEMCARDS));
	config->Memcard_SyncSave = GetConfigBool(Memcard_SyncSave_Key, USER_MEMCARDS);

	config->di_log = GetConfigBool(USER_DI_LOG, USER_HW);
	config->si_log = GetConfigBool(USER_SI_LOG, USER_HW);
	config->ai_log = GetConfigBool(USER_AI_LOG, USER_HW);
	config->mi_log = GetConfigBool(USER_MI_LOG, USER_HW);
	config->cp_log = GetConfigBool(USER_CP_LOG, USER_HW);

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

// Free the emulated machine without touching the file-loaded state. EMUClose uses it on the normal
// path; EMUOpen uses it to undo a partial startup. It is safe to call when nothing was created.
static void EMUReleaseHardware()
{
	if (Flipper::HW) {
		delete Flipper::HW;
		Flipper::HW = nullptr;
	}

	if (Debug::Log) {
		delete Debug::Log;
		Debug::Log = nullptr;
	}
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
	static HWConfig hwconfig{};
	EMUGetHwConfig(&hwconfig);
	Flipper::HW = new Flipper::Flipper(&hwconfig);

	// A file that cannot be loaded (a damaged image, an executable with a broken header, or a
	// script that refuses to run) must leave the emulator in the state it was in before the
	// attempt. Without this the half-built Flipper object stayed allocated, and the next shutdown
	// walked into freed or never-initialised state - a crash on the way out of a startup failure.
	try
	{
		CallJdi("script autoexec.cmd");
		LoadFile(filename);   // Gekko PC will be set here
		HLEOpen();
	}
	catch (...)
	{
		Report(Channel::Error, "Failed to open: %s\n", Util::WstringToString(filename).c_str());
		EMUReleaseHardware();
		throw;
	}

	Debug::g_PerfCounters->ResetAllCounters();

	// The HW interface profiler (issue #394) counts the traffic of the machine that was just
	// built, so the counters of the previous one (and the window measured over them) go away.
	Debug::HwProfile::Reset();

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

	EMUReleaseHardware();

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
	JDI::Hub.AddNode(L"DEBUGGER_JDI_JSON", JdiSpecs::DebuggerJdi, Debug::Reflector);
	JDI::Hub.AddNode(L"GEKKO_CORE_JDI_JSON", JdiSpecs::GekkoCoreJdi, Debug::gekko_init_handlers);
	Core = new Gekko::GekkoCore();
	Flipper::DSP = new DSP::Dsp16();

	// The DSPcore recompiler is an experimental feature and off by default; `--dspjit` swaps the
	// interpreter for it, so two runs of the same binary differ only in the execution engine.
	Flipper::DSP->core->JitEnabled = cmdline.dspJit;
	if (cmdline.dspJit)
	{
		DSP::Jit* dspJit = Flipper::DSP->core->GetJit();
		Report(Channel::DSP, "DSPcore: experimental recompiler requested (%s)\n",
			(dspJit != nullptr && dspJit->IsSupported()) ? "on" : "not available on this host, staying on the interpreter");
	}
	JDI::Hub.AddNode(L"EMU_JDI_JSON", JdiSpecs::EmuJdi, EmuReflector);

	// The MCP server (issue #383) publishes the commands of every node as its tools, so its own
	// node is registered here with the rest of them. Its local transport is started by the front
	// end (`--mcp`), which is the one that owns stdin/stdout.
	JDI::Hub.AddNode(L"MCP_JDI_JSON", JdiSpecs::McpJdi, Mcp::Reflector);

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

	// The server is shut down before the nodes it publishes go away, so that a client that is
	// still connected cannot call into a half-destroyed debug interface.
	Mcp::StopTransport();

	JDI::Hub.RemoveNode(L"MCP_JDI_JSON");
	JDI::Hub.RemoveNode(L"EMU_JDI_JSON");
	DVD::Unmount();
	DVD::ShutdownSubsystem();
	delete Core;
	Core = nullptr;
	delete Flipper::DSP;
	Flipper::DSP = nullptr;
	HLEShutdown();
	JDI::Hub.RemoveNode(L"GEKKO_CORE_JDI_JSON");
	JDI::Hub.RemoveNode(L"DEBUGGER_JDI_JSON");
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
	if (args.size() < 2)
	{
		Report(Channel::Error, "FileLoad: file name expected\n");
		return nullptr;
	}

	FILE* f;

	f = Util::FileOpen(Util::StringToWstring(args[1]), "rb");
	if (!f)
	{
		Report(Channel::Error, "Failed to open: %s\n", args[1].c_str());
		return nullptr;
	}

	// The answer holds one Json value per byte, so this command is for small data files (fonts, FST
	// dumps) and a bigger file is refused instead of being turned into gigabytes of Json values.
	const size_t MaxFileLoadSize = 16 * 1024 * 1024;

	size_t size = Util::FileSize(args[1]);
	if (size == 0 || size > MaxFileLoadSize)
	{
		Report(Channel::Error, "FileLoad: refusing %s (%zi bytes)\n", args[1].c_str(), size);
		fclose(f);
		return nullptr;
	}

	std::vector<uint8_t> data(size);

	// feof() only becomes true after a read has already failed, so the old loop appended one bogus
	// element at the end; read the exact size and insist on getting it.
	size_t bytesRead = fread(data.data(), 1, size, f);
	fclose(f);

	if (bytesRead != size)
	{
		Report(Channel::Error, "FileLoad: short read on %s (%zi of %zi bytes)\n", args[1].c_str(), bytesRead, size);
		return nullptr;
	}

	Json::Value* output = new Json::Value();
	output->type = Json::ValueType::Array;

	for (size_t i = 0; i < size; i++)
	{
		output->AddInt(nullptr, data[i]);
	}

	Report(Channel::Norm, "Loaded: %s (%zi bytes)\n", args[1].c_str(), size);

	return output;
}

static Json::Value* EmuFileSave(std::vector<std::string>& args)
{
	if (args.size() < 2)
	{
		Report(Channel::Error, "FileSave: file name expected\n");
		return nullptr;
	}

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

		f = Util::FileOpen(Util::StringToWstring(args[1]), "wb");
		if (!f)
		{
			Report(Channel::Error, "Failed to create file: %s\n", args[1].c_str());
			return nullptr;
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
	if (args.size() < 2)
	{
		Report(Channel::Error, "sleep: milliseconds expected\n");
		return nullptr;
	}

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
	if (args.size() < 2)
	{
		Report(Channel::Error, "load: file name expected\n");
		return nullptr;
	}

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
	Report(Channel::Norm, "%s = %i\n", USER_VI_XFB, GetConfigBool(USER_VI_XFB, USER_HW));
	Report(Channel::Norm, "%s = %i\n", USER_GFX_PIPELINE, GetConfigInt(USER_GFX_PIPELINE, USER_HW));

	Report(Channel::Norm, "%s = %s\n", USER_BOOTROM, Util::WstringToString(GetConfigString(USER_BOOTROM, USER_HW)).c_str());
	Report(Channel::Norm, "%s = %s\n", USER_DSP_DROM, Util::WstringToString(GetConfigString(USER_DSP_DROM, USER_HW)).c_str());
	Report(Channel::Norm, "%s = %s\n", USER_DSP_IROM, Util::WstringToString(GetConfigString(USER_DSP_IROM, USER_HW)).c_str());

	Report(Channel::Norm, "%s = %i\n", USER_PI_LOG, GetConfigBool(USER_PI_LOG, USER_HW));
	Report(Channel::Norm, "%s = %i\n", USER_EXI_LOG, GetConfigBool(USER_EXI_LOG, USER_HW));
	Report(Channel::Norm, "%s = %i\n", USER_VI_LOG, GetConfigBool(USER_VI_LOG, USER_HW));
	Report(Channel::Norm, "%s = %i\n", USER_DI_LOG, GetConfigBool(USER_DI_LOG, USER_HW));
	Report(Channel::Norm, "%s = %i\n", USER_SI_LOG, GetConfigBool(USER_SI_LOG, USER_HW));
	Report(Channel::Norm, "%s = %i\n", USER_AI_LOG, GetConfigBool(USER_AI_LOG, USER_HW));
	Report(Channel::Norm, "%s = %i\n", USER_MI_LOG, GetConfigBool(USER_MI_LOG, USER_HW));
	Report(Channel::Norm, "%s = %i\n", USER_CP_LOG, GetConfigBool(USER_CP_LOG, USER_HW));

	return nullptr;
}

static Json::Value* CmdGetConfigString(std::vector<std::string>& args)
{
	if (args.size() < 3)
	{
		Report(Channel::Error, "GetConfigString: section and parameter expected\n");
		return nullptr;
	}

	wchar_t* param = GetConfigString(args[2].c_str(), args[1].c_str());

	Json::Value* output = new Json::Value();
	output->type = Json::ValueType::Array;

	output->AddString(nullptr, param);

	return output;
}

static Json::Value* CmdSetConfigString(std::vector<std::string>& args)
{
	if (args.size() < 4)
	{
		Report(Channel::Error, "SetConfigString: section, parameter and value expected\n");
		return nullptr;
	}

	SetConfigString(args[2].c_str(), Util::StringToWstring(args[3]).c_str(), args[1].c_str());
	return nullptr;
}

static Json::Value* CmdGetConfigInt(std::vector<std::string>& args)
{
	if (args.size() < 3)
	{
		Report(Channel::Error, "GetConfigInt: section and parameter expected\n");
		return nullptr;
	}

	int param = GetConfigInt(args[2].c_str(), args[1].c_str());

	Json::Value* output = new Json::Value();
	output->type = Json::ValueType::Array;

	output->AddInt(nullptr, param);

	return output;
}

static Json::Value* CmdSetConfigInt(std::vector<std::string>& args)
{
	if (args.size() < 4)
	{
		Report(Channel::Error, "SetConfigInt: section, parameter and value expected\n");
		return nullptr;
	}

	SetConfigInt(args[2].c_str(), atoi(args[3].c_str()), args[1].c_str());
	return nullptr;
}

static Json::Value* CmdGetConfigBool(std::vector<std::string>& args)
{
	if (args.size() < 3)
	{
		Report(Channel::Error, "GetConfigBool: section and parameter expected\n");
		return nullptr;
	}

	bool param = GetConfigBool(args[2].c_str(), args[1].c_str());

	Json::Value* output = new Json::Value();
	output->type = Json::ValueType::Array;

	output->AddBool(nullptr, param);

	return output;
}

static Json::Value* CmdSetConfigBool(std::vector<std::string>& args)
{
	if (args.size() < 4)
	{
		Report(Channel::Error, "SetConfigBool: section, parameter and value expected\n");
		return nullptr;
	}

	SetConfigBool(args[2].c_str(), args[3] == "true" ? true : false, args[1].c_str());
	return nullptr;
}

static Json::Value* CmdThreads(std::vector<std::string>& args)
{
	Report(Channel::Norm, "Emulator threads:\n");
	for (auto it = emu_threads.begin(); it != emu_threads.end(); ++it) {
		Thread* thread = *it;
		Report(Channel::Norm, "- %s: %s\n", thread->GetName(), thread->IsRunning() ? "Running" : "Suspended");
	}
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

	JDI::Hub.AddCmd("threads", CmdThreads);
}


/* Supported formats are :                                      */
/*      .dol        - GAMECUBE custom executable                */
/*      .elf        - standard executable                       */
/*      .gcm        - game master data (GC DVD images)          */
/*      .iso        - raw GC DVD image                          */
/*      .rvz        - compressed GC DVD image (Dolphin RVZ)     */

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
	DolHeader dh{};

	/* Try to open file. */
	auto dol = std::ifstream( Util::WstringToString(dolname).c_str(), std::ifstream::binary);
	if (!dol.is_open())
	{
		return 0;
	}

	/* Load DOL header and swap it for loader. */
	if (!dol.read((char*)&dh, sizeof(DolHeader)))
	{
		Report(Channel::Error, "Cannot read DOL header: %s\n", Util::WstringToString(dolname).c_str());
		return 0;
	}
	Gekko::GekkoCore::SwapArea((uint32_t*)&dh, sizeof(DolHeader));

	// A section has two ends that have to be checked separately: its window in the file (the size
	// the stream can really deliver) and its window in main memory (the RAM the MI allocated, which
	// is not the 64 MB the address mask allows). Neither a null destination nor an over-long read
	// may be reached from a crafted header.
	size_t fileSize = Util::FileSize(dolname);
	size_t ramSize = Flipper::HW->mem->MIGetMemorySize();

	Report(Channel::Loader, "Loading DOL %s (%i b).\n", dolname.data(), DOLSize(&dh));

	/* Load all text (code) sections. */
	for(int i = 0; i < DOL_NUM_TEXT; i++)
	{
		if(dh.textOffset[i])    /* If offset is 0, then section is empty */
		{
			if (!Verify::ImageSection(fileSize, dh.textOffset[i], dh.textSize[i], dh.textAddress[i], ramSize))
			{
				Report(Channel::Error, "DOL text section %i is out of range: file offset %08X, size %i b, address %08X\n",
					i, dh.textOffset[i], dh.textSize[i], dh.textAddress[i]);
				return 0;
			}

			uint8_t* ptr = (uint8_t*)Flipper::HW->mem->MIGetMemoryPointerForDebug(dh.textAddress[i], dh.textSize[i]);
			if (ptr == nullptr)
			{
				Report(Channel::Error, "DOL text section %i has no memory at %08X\n", i, dh.textAddress[i]);
				return 0;
			}
			char* addr = (char*)ptr;

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
			if (!Verify::ImageSection(fileSize, dh.dataOffset[i], dh.dataSize[i], dh.dataAddress[i], ramSize))
			{
				Report(Channel::Error, "DOL data section %i is out of range: file offset %08X, size %i b, address %08X\n",
					i, dh.dataOffset[i], dh.dataSize[i], dh.dataAddress[i]);
				return 0;
			}

			uint8_t* ptr = (uint8_t*)Flipper::HW->mem->MIGetMemoryPointerForDebug(dh.dataAddress[i], dh.dataSize[i]);
			if (ptr == nullptr)
			{
				Report(Channel::Error, "DOL data section %i has no memory at %08X\n", i, dh.dataAddress[i]);
				return 0;
			}
			char* addr = (char*)ptr;

			dol.seekg(dh.dataOffset[i]);
			dol.read(addr, dh.dataSize[i]);

			Report(Channel::Loader,
				"   data section %08X->%08X, size %i b\n", 
				dh.dataOffset[i],
				dh.dataAddress[i], dh.dataSize[i]
			);
		}
	}

	static HWConfig config{};
	EMUGetHwConfig(&config);
	BootROM(&config, false, false);

	/* Setup registers. */
	Core->regs.gpr[1] = 0x816ffffc;
	Core->regs.gpr[13] = 0x81100000;      // Fake sda1

	// DO NOT CLEAR BSS !

	Report(Channel::Loader, "   DOL entrypoint %08X\n\n", dh.entryPoint);
	dol.close();
	return dh.entryPoint;
}

// same as LoadDOL, but DOL is mapped in memory at `dol`. The image is not necessarily a file, so
// the caller has to pass the number of readable bytes that start at `dol` (`imageSize`); every
// section is validated against that window and against the allocated RAM before it is copied.
// NOTE: there is no caller of this function in the tree right now; a new one must pass the real
// size of its buffer (for a mounted image, `Util::FileSize` of the image file).
uint32_t LoadDOLFromMemory(DolHeader *dol, uint32_t ofs, uint32_t imageSize)
{
	int i;
	#define ADDPTR(p1, p2) (uint8_t *)((uint8_t*)(p1)+(uint32_t)(p2))

	// swap DOL header
	Gekko::GekkoCore::SwapArea((uint32_t *)dol, sizeof(DolHeader));

	// The MI allocation, not the 64 MB the address mask spans.
	size_t ramSize = Flipper::HW->mem->MIGetMemorySize();

	Report(Channel::Loader, "Loading DOL from %08X (%i b).\n",
		   ofs, DOLSize(dol) );

	// load all text (code) sections
	for(i=0; i<DOL_NUM_TEXT; i++)
	{
		if(dol->textOffset[i])  // if offset is 0, then section is empty
		{
			if (!Verify::ImageSection(imageSize, dol->textOffset[i], dol->textSize[i], dol->textAddress[i], ramSize))
			{
				Report(Channel::Error, "DOL text section %i is out of range: image offset %08X, size %i b, address %08X\n",
					i, dol->textOffset[i], dol->textSize[i], dol->textAddress[i]);
				return 0;
			}

			uint8_t* ptr = (uint8_t*)Flipper::HW->mem->MIGetMemoryPointerForDebug(dol->textAddress[i], dol->textSize[i]);
			if (ptr == nullptr)
			{
				Report(Channel::Error, "DOL text section %i has no memory at %08X\n", i, dol->textAddress[i]);
				return 0;
			}

			memcpy(ptr, ADDPTR(dol, dol->textOffset[i]), dol->textSize[i]);

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
			if (!Verify::ImageSection(imageSize, dol->dataOffset[i], dol->dataSize[i], dol->dataAddress[i], ramSize))
			{
				Report(Channel::Error, "DOL data section %i is out of range: image offset %08X, size %i b, address %08X\n",
					i, dol->dataOffset[i], dol->dataSize[i], dol->dataAddress[i]);
				return 0;
			}

			uint8_t* ptr = (uint8_t*)Flipper::HW->mem->MIGetMemoryPointerForDebug(dol->dataAddress[i], dol->dataSize[i]);
			if (ptr == nullptr)
			{
				Report(Channel::Error, "DOL data section %i has no memory at %08X\n", i, dol->dataAddress[i]);
				return 0;
			}

			memcpy(ptr, ADDPTR(dol, dol->dataOffset[i]), dol->dataSize[i]);

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
	if (!file.read((char*)&hdr, sizeof(ElfEhdr)))
	{
		Report(Channel::Error, "Cannot read ELF header: %s\n", Util::WstringToString(elfname).c_str());
		file.close();
		return 0;
	}
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

	// The program header table has to be inside the file before any header is read out of it:
	// e_phoff and e_phnum come straight from the file and e_phnum was previously only the loop
	// bound.
	size_t fileSize = Util::FileSize(elfname);
	size_t ramSize = Flipper::HW->mem->MIGetMemorySize();
	uint64_t phoff = Elf_SwapOff(hdr.e_phoff);
	uint64_t phnum = Elf_SwapHalf(hdr.e_phnum);

	if (!Verify::Range(phoff, phnum * sizeof(ElfPhdr), fileSize))
	{
		Report(Channel::Error, "ELF program header table is out of range: offset %llu, count %llu, file size %zi\n",
			(unsigned long long)phoff, (unsigned long long)phnum, fileSize);
		file.close();
		return 0;
	}

	file.seekg((std::streamoff)phoff);
	for(uint64_t i = 0; i < phnum; i++)
	{
		std::streampos old;

		if (!file.read((char*)&phdr, sizeof(ElfPhdr)))
		{
			Report(Channel::Error, "Cannot read ELF program header %llu\n", (unsigned long long)i);
			file.close();
			return 0;
		}
		old = file.tellg();

		// load one segment
		{
			uint64_t vaddr;
			uint32_t size;

			if(Elf_SwapWord(phdr.p_type) == PT_LOAD)
			{
				vaddr = Elf_SwapAddr(phdr.p_vaddr);

				// p_filesz is a 32-bit field. Held in a signed long (as the old code did) a segment
				// of 0x80000000 bytes or more became a negative length.
				size = Elf_SwapWord(phdr.p_filesz);
				if(size == 0) continue;

				// The segment has to be inside the file *and* inside main memory; a segment that
				// is not is a malformed image, not something to half-load.
				if (!Verify::ImageSection(fileSize, Elf_SwapOff(phdr.p_offset), size, (uint32_t)vaddr, ramSize))
				{
					Report(Channel::Error, "ELF segment %llu is out of range: file offset %08X, size %08X, address %08X\n",
						(unsigned long long)i, (uint32_t)Elf_SwapOff(phdr.p_offset), size, (uint32_t)vaddr);
					file.close();
					return 0;
				}

				file.seekg((std::streamoff)Elf_SwapOff(phdr.p_offset));
				uint8_t* ptr = (uint8_t*)Flipper::HW->mem->MIGetMemoryPointerForDebug((uint32_t)vaddr, size);
				if (ptr == nullptr)
				{
					Report(Channel::Error, "ELF segment %llu has no memory at %08X\n", (unsigned long long)i, (uint32_t)vaddr);
					file.close();
					return 0;
				}
				file.read((char*)ptr, size);
			}
		}

		file.seekg(old);
	}

	file.close();
	return elf_entrypoint;
}

/* ---------------------------------------------------------------------------  */
/* File loader engine                                                           */

static void AutoloadMap(HWConfig* config, const std::wstring & filename, bool dvd, std::wstring & diskId)
{
	// get map file name
	std::wstring mapname{};
	char drive[0x100], dir[0x1000], name[0x100], ext[0x100];

	// Every component is copied with its destination size: a name with an over-long component (or a
	// UNC prefix) used to overflow these stack buffers, and an empty split would invent a map path.
	if (!Util::SplitPath(Util::WstringToString(filename).c_str(),
		drive, sizeof(drive),
		dir, sizeof(dir),
		name, sizeof(name),
		ext, sizeof(ext)))
	{
		Report(Channel::Error, "Cannot split the path of %s, map autoload skipped\n", Util::WstringToString(filename).c_str());
		return;
	}

	// Step 1: try to load map from Data directory
	if (dvd)
	{
		mapname = std::wstring(L"./Data/") + diskId + std::wstring(L".map");
	}
	else
	{
		mapname = std::wstring(L"./Data/") + Util::StringToWstring(name) + std::wstring(L".map");
	}
	
	MAP_FORMAT format = LoadMAP(mapname.c_str());
	if (format != MAP_FORMAT::BAD) return;
 
	// Step 2: try to load map from file directory
	if (dvd)
	{
		mapname = Util::StringToWstring(drive) + Util::StringToWstring(dir) + diskId + std::wstring(L".map");
	}
	else
	{
		mapname = Util::StringToWstring(drive) + Util::StringToWstring(dir) + Util::StringToWstring(name) + std::wstring(L".map");
	}

	format = LoadMAP(mapname.c_str());
	if (format != MAP_FORMAT::BAD) return;

	// sorry, no maps for this DVD/executable
	Report(Channel::Loader, "WARNING: MAP file doesnt exist, HLE could be impossible\n\n");

	// Step 3: make new map (find symbols)
	if(GetConfigBool(USER_MAKEMAP, USER_LOADER))
	{
		if (dvd)
		{
			mapname = std::wstring(L"./Data/") + diskId + std::wstring(L".map");
		}
		else
		{
			mapname = std::wstring(L"./Data/") + Util::StringToWstring(name) + std::wstring(L".map");
		}
		
		Report(Channel::Loader, "Making new MAP file: %s\n\n", Util::WstringToString(mapname).c_str());
		MAPInit(mapname.c_str());
		MAPAddRange(0x80000000, 0x80000000 | (uint32_t)config->ramsize);  // user can wait for once :O)
		MAPFinish();
		LoadMAP(mapname.c_str());
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

	diskId = std::wstring(diskIdWchar);
}

void LoadFile(const std::wstring& filename)
{
	uint32_t entryPoint = 0;
	bool dvd = false;
	std::wstring diskId;

	// load file
	if (filename == L"Bootrom")
	{
		entryPoint = PI_MEMSPACE_BOOTROM + 0x100;
		dvd = false;
		emu.bootrom = true;
	}
	else
	{
		// wcsrchr returns NULL for a name without a dot; _wcsicmp(NULL, ...) aborts (MSVC) or
		// segfaults (Linux, where it maps to wcscasecmp), so the extension has to be tested first.
		const wchar_t* extension = wcsrchr(filename.c_str(), L'.');

		if (extension == nullptr)
		{
			Report(Channel::Error, "Unknown file type: %s\n", Util::WstringToString(filename).c_str());
		}
		else if (!_wcsicmp(extension, L".dol"))
		{
			entryPoint = LoadDOL(filename);
			dvd = false;
		}
		else if (!_wcsicmp(extension, L".elf"))
		{
			entryPoint = LoadELF(filename);
			dvd = false;
		}
		else if (!_wcsicmp(extension, L".iso") || !_wcsicmp(extension, L".gcm") || !_wcsicmp(extension, L".rvz"))
		{
			// A disk image that cannot be mounted (a truncated or renamed file whose boot info
			// does not check out) must fail the load. Ignoring the result used to leave the DVD
			// layer with no image mounted and the boot sequence reading zeroes from it forever.
			if (!DVD::MountFile(filename))
			{
				Report(Channel::Error, "Failed to mount the disk image: %s\n", Util::WstringToString(filename).c_str());
				throw "Cannot load file!";
			}

			GetDiskId(diskId);
			dvd = true;
		}
		else
		{
			Report(Channel::Error, "Unknown file type: %s\n", Util::WstringToString(filename).c_str());
		}
	}

	/* File load success? */
	if(entryPoint == 0 && !dvd)
	{
		throw "Cannot load file!";
	}

	static HWConfig config{};
	EMUGetHwConfig(&config);

	// simulate bootrom
	if (!emu.bootrom)
	{
		BootROM(&config, dvd, false);
		Thread::Sleep(10);
	}

	// autoload map file
	if (!emu.bootrom)
	{
		AutoloadMap(&config, filename, dvd, diskId);
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
		Flipper::HW->vi->VISetEncoderFuse(DVD::IsNtsc(region) ? 0 : 1);
	}

	// Do the same for the bootstrap.
	if (emu.bootrom) {

		Flipper::HW->vi->VISetEncoderFuse(IsBootromPALRevision() ? 1 : 0);
	}
}