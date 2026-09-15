/*

# Headless UI (uinull)

The front end of the headless build. It is the counterpart of the Win32 UI (ui.cpp) and the SDL one
(uisdl.cpp): the same emulator, with no window, no ImGui and no user input at all.

What it exists for:

  * an unattended run on a machine that has no display (a build server, a CI box, a WSL session
    without an X server, a DolphinSDK demo sweep - issue #385). There is nothing to click on, so
    the entry point just loads what it was told to load and runs it;
  * `--bench <file> [seconds]`, the measurement tool (bench.cpp). The windowed build can run it
    too, but it does not need a window to be useful: in the headless build the benchmark is the
    primary way to run an image;
  * `--selftest`, which already ran without a window and now does not need the SDL/ImGui parts of
    the windowed front end to be linked either;
  * `--mcp`, the local MCP server (issue #383): the client that started the emulator drives its
    whole debug interface over stdin/stdout, and the front end only waits for it. stdout belongs
    to the protocol then, so the text of this file goes to stderr.

Everything the emulator core asks of a UI goes through the Json Debug Interface, so the headless
front end is small:

  * the `UI_JDI_JSON` node (`UIReflector`) answers `UIError`, `UIReport` and `GetRenderTarget`;
  * `GetRenderTarget` returns 0, because there is no window to render into. The GFX backend accepts
    that: the headless build uses the null renderer (gfxnull.cpp), and the video output uses
    videonull.cpp, so the `RenderTarget` is never dereferenced;
  * the reports (`Debug::Report`) are echoed to the console, so an unattended run leaves a readable
    log on stdout as well as in `EMU_LOG`.

*/

#include "pch.h"

#include "bench.h"

#include <csignal>

using namespace Debug;

// -------------------------------------------------------------------------------------------
// The UI Json Debug Interface node
// -------------------------------------------------------------------------------------------

static Json::Value* CmdUIError(std::vector<std::string>& args)
{
	std::string text = "";

	if (args.size() < 2)
	{
		return nullptr;
	}

	for (size_t i = 1; i < args.size(); i++)
	{
		text += args[i] + " ";
	}

	printf("UIError: %s\n", text.c_str());

	return nullptr;
}

static Json::Value* CmdUIReport(std::vector<std::string>& args)
{
	std::string text = "";

	if (args.size() < 2)
	{
		return nullptr;
	}

	for (size_t i = 1; i < args.size(); i++)
	{
		text += args[i] + " ";
	}

	printf("UIReport: %s\n", text.c_str());

	return nullptr;
}

static Json::Value* CmdGetRenderTarget(std::vector<std::string>& args)
{
	// There is no window: the caller gets a null render target, and the null GFX/video backends
	// never look at it.

	Json::Value* value = new Json::Value();
	value->type = Json::ValueType::Int;
	value->value.AsInt = 0;
	return value;
}

void UIReflector()
{
	JdiAddCmd("UIError", CmdUIError);
	JdiAddCmd("UIReport", CmdUIReport);
	JdiAddCmd("GetRenderTarget", CmdGetRenderTarget);
}

// -------------------------------------------------------------------------------------------
// Stopping an unattended run
// -------------------------------------------------------------------------------------------

// A headless run has no window to close, so the interrupt (Ctrl+C) is the way to stop it. The
// handler only sets a flag: the run loop notices it and returns, and the emulator is then shut
// down in the normal order rather than from inside the signal handler.
static volatile std::sig_atomic_t interrupted = 0;

static void OnInterrupt(int)
{
	interrupted = 1;
}

static bool RunPump()
{
	if (interrupted)
	{
		printf("\ninterrupted, stopping the emulation...\n");
		return false;
	}

	return true;
}

// -------------------------------------------------------------------------------------------
// Running an image
// -------------------------------------------------------------------------------------------

// The front end talks to the console it was started from. While the MCP server owns the streams
// (`--mcp`), stdout carries the JSON-RPC messages of the client and nothing else, so the text of
// the front end goes to stderr - which is where the MCP specification expects a server's log.
static void Say(const char* format, ...)
{
	va_list arg;
	va_start(arg, format);

	FILE* out = cmdline.mcp ? stderr : stdout;
	vfprintf(out, format, arg);
	fflush(out);

	va_end(arg);
}

//! Load `file`, run it and measure it. `seconds` is the benchmark duration; 0 means "until the user
//! interrupts it" (the plain headless run).
static void RunImage(const std::wstring& file, uint32_t seconds)
{
	Say("Loading %s...\n", Util::WstringToString(file).c_str());

	UI::Jdi->LoadFile(Util::WstringToString(file));
	UI::Jdi->Run();

	Bench::Measure(file, seconds, RunPump);

	UI::Jdi->Stop();
	Thread::Sleep(200);
	UI::Jdi->Unload();
}

static int HeadlessMain()
{
	if (cmdline.help)
	{
		EMUPrintUsage();
		return 0;
	}

	if (cmdline.selftest)
	{
		return EMUSelfTest();
	}

	// The GBA and the Game Boy are presented by a windowed SDL2 frontend of their own, which is not
	// part of this build (see EMURunGba).
	if (cmdline.gba)
	{
		Say("headless: the GBA and Game Boy machines need a window.\n");
		return -1;
	}

	// From this point on the reports are also written to the console - except when the MCP server
	// owns stdout, where anything but a JSON-RPC message would break the protocol.
	ConsoleEcho = !cmdline.mcp;

	// An MCP session needs no image of its own: the client loads what it wants with a tool call.
	if (!cmdline.bench && cmdline.image.empty() && !cmdline.ipl && !cmdline.mcp)
	{
		// There is no selector to show, so say what is missing instead of waiting for input that
		// can never arrive.
		Say("headless: nothing to run.\n");
		Say("Pass an image, or use --image <file>, --ipl, --bench <file> [sec] or --mcp. `--help` lists the options.\n");
		return -1;
	}

	signal(SIGINT, OnInterrupt);

	EMUCtor();

	// Create an interface for communicating with the emulator core
	UI::Jdi = new UI::JdiClient;

	// Add the UI methods
	JdiAddNode("UI_JDI_JSON", JdiSpecs::UiJdi, UIReflector);

	Say("pureikyubu (headless), Nintendo GameCube emulator version %s\n", UI::Jdi->GetVersion().c_str());

	int status = 0;

	try
	{
		if (cmdline.bench)
		{
			Say("Benchmark run: %u second(s).\n", cmdline.benchSeconds);
			RunImage(cmdline.benchFile, cmdline.benchSeconds);
		}
		else if (cmdline.mcp)
		{
			// The local MCP server (issue #383): the client that started the emulator drives it
			// through the debug interface, so the front end steps aside and only waits for it. An
			// image on the command line is started right away, which is how a client attaches to a
			// running game.
			if (!cmdline.image.empty())
			{
				Say("Loading %s...\n", Util::WstringToString(cmdline.image).c_str());
				UI::Jdi->LoadFile(Util::WstringToString(cmdline.image));
				UI::Jdi->Run();
			}

			Mcp::StartTransport();
			Say("MCP server: the messages are read from stdin. Close the stream to stop the emulator.\n");

			while (Mcp::TransportRunning() && !interrupted)
			{
				Thread::Sleep(20);
			}

			Say("MCP server: the client is gone, stopping the emulation.\n");

			UI::Jdi->Stop();
			Thread::Sleep(200);
			UI::Jdi->Unload();
		}
		else
		{
			// The Bootrom runs by itself, so `--ipl` is a complete request.
			std::wstring file = cmdline.image.empty() ? std::wstring(L"Bootrom") : cmdline.image;
			Say("Press Ctrl+C to stop the emulation.\n");
			RunImage(file, 0);
		}
	}
	catch (const char* text)
	{
		Report(Channel::Error, "the emulation stopped: %s\n", (text != nullptr) ? text : "(no message)");
		status = 1;
	}
	catch (const std::exception& e)
	{
		Report(Channel::Error, "the emulation stopped: %s\n", e.what());
		status = 1;
	}
	catch (...)
	{
		Report(Channel::Error, "the emulation stopped with an unknown error\n");
		status = 1;
	}

	// Unload

	Mcp::StopTransport();
	JdiRemoveNode("UI_JDI_JSON");
	delete UI::Jdi;
	UI::Jdi = nullptr;
	EMUDtor();

	Say("\nThank you for flying pureikyubu airlines!\n");
	return status;
}

// The headless build is a console application (there is no window to attach to), so `main` is the
// entry point on every platform.
int main(int argc, char** argv)
{
	EMUParseCmdLine(argc, argv);

	return HeadlessMain();
}
