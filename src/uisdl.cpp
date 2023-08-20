// Portable UI based on SDL+imgui
#include "pch.h"
#ifdef _WINDOWS
#include <SDL_syswm.h>
#endif
#include "../thirdparty/imgui-filebrowser/imfilebrowser.h"
#include <codecvt>
#include <locale>

static bool ui_active = false;
static bool show_demo_window = true;
static SDL_Window* window;
static SDL_Window* render_target;
static SDL_Renderer* renderer;
static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
static bool debugger_enabled_check = false;
static ImGui::FileBrowser fileOpenDialog(ImGuiFileBrowserFlags_CloseOnEsc);
static ImGui::FileBrowser fileSaveDialog(ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_EnterNewFilename);
static ImGui::FileBrowser chooseDirectoryDialog(ImGuiFileBrowserFlags_SelectDirectory);

enum class FileReaction
{
	None = 0,
	OpenFile_LoadFile,
	ChooseDirectory_MountSdk,
	OpenFile_Bootrom,
	OpenFile_DROM,
	OpenFile_IROM,
	OpenFile_MemcardA,
	OpenFile_MemcardB,
};
static FileReaction file_reaction = FileReaction::None;

static uint16_t* SjisToUnicode(wchar_t* sjisText, size_t* size, size_t* chars)
{
	uint16_t* unicodeText, * ptrU, uchar, schar;
	wchar_t* ptrS;

	*size = (wcslen(sjisText) + 1) * sizeof(wchar_t);
	unicodeText = (uint16_t*)malloc(*size);
	assert(unicodeText);
	memset(unicodeText, 0, *size);

	ptrU = unicodeText;
	ptrS = sjisText;
	*chars = 0;

	schar = *ptrS;
	while (schar != 0)
	{
		uchar = SjisTable[schar];
		if (uchar == 0xFFFF)
		{
			ptrS++;
			schar = (schar << 8) | *ptrS;
			uchar = SjisTable[schar];
		}
		*ptrU = uchar;

		ptrU++;
		ptrS++;
		(*chars)++;
		schar = *ptrS;
	}
	return unicodeText;
}

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

	//UI::Error(L"Error", L"%s", Util::StringToWstring(text).c_str());

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

	//UI::Report(L"%s", Util::StringToWstring(text).c_str());

	return nullptr;
}

static Json::Value* CmdGetRenderTarget(std::vector<std::string>& args)
{
	// Return main window as RenderTarget

	// For the first time, this is a hack to check
#ifdef _WINDOWS
	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);
	SDL_GetWindowWMInfo(render_target, &wmInfo);
	HWND hwnd = wmInfo.info.win.window;

	Json::Value* value = new Json::Value();
	value->type = Json::ValueType::Int;
	value->value.AsInt = (uint64_t)hwnd;
	return value;
#endif

	// TODO

	return nullptr;
}

static void UIReflector()
{
	JdiAddCmd("UIError", CmdUIError);
	JdiAddCmd("UIReport", CmdUIReport);
	JdiAddCmd("GetRenderTarget", CmdGetRenderTarget);
}



// Statusbar

std::wstring status_parts[(int)STATUS_ENUM::StatusMax];

/* Set default values of statusbar parts */
static void ResetStatusBar()
{
	SetStatusText(STATUS_ENUM::Progress, L"Idle");
	SetStatusText(STATUS_ENUM::VIs, L"");
	SetStatusText(STATUS_ENUM::PEs, L"");
	SetStatusText(STATUS_ENUM::SystemTime, L"");
}

/* Create status bar window */
static void CreateStatusBar()
{
	/* Set default values */
	ResetStatusBar();
}

/* Change text in specified statusbar part */
void SetStatusText(STATUS_ENUM sbPart, const std::wstring& text, bool post)
{
	status_parts[(int)sbPart] = std::wstring(text);
}




/*

# Performance Counters

Interesting to track :
- The number of emulated Gekko instructions (million per second, mips)
- Number of recompiled and executed GekkoCore recompiler segments.
- Number of DSP instructions emulated (million per second, mips)
- Number of VI interrupts (frames per second)
- Number of draw operations (PE DrawDone / second)
- Show formatted value of TBR register (OSSystemTime)

*/

namespace UI
{

	// Global instance of the utility, which is controlled in the ui.cpp module
	PerfMetrics* g_perfMetrics = nullptr;


	void PerfMetrics::PerfThreadProc(void* param)
	{
		PerfMetrics* perf = (PerfMetrics*)param;

		// Get and reset counters

		int64_t gekkoMips = perf->GetGekkoInstructionsCounter();
		perf->ResetGekkoInstructionsCounter();

		int64_t compiledSegs = perf->GetGekkoCompiledSegments();
		perf->ResetGekkoCompiledSegments();

		int64_t executedSegs = perf->GetGekkoExecutedSegments();
		perf->ResetGekkoExecutedSegments();

		int64_t dspMips = perf->GetDspInstructionsCounter();
		perf->ResetDspInstructionsCounter();

		// If the number of executed segments is zero, then most likely the emulator is running in interpreter mode
		// or is in debug mode (emulation is temporarily stopped), so there is no point in displaying JITC statistics.

		char str[0x100];
		if (executedSegs != 0)
		{
			sprintf(str, "gekko: %.02f mips (jitc %lld/%.02fM), dsp: %.02f mips",
				(float)gekkoMips / 1000000.f, compiledSegs, (float)executedSegs / 1000000.f, (float)dspMips / 1000000.f);
		}
		else
		{
			sprintf(str, "gekko: %.02f mips, dsp: %.02f mips",
				(float)gekkoMips / 1000000.f, (float)dspMips / 1000000.f);
		}

		int32_t vis = perf->GetVICounter();
		perf->ResetVICounter();

		int32_t pes = perf->GetPECounter();
		perf->ResetPECounter();

		// Display information in the status bar

		SetStatusText(STATUS_ENUM::Progress, Util::StringToWstring(str));
		SetStatusText(STATUS_ENUM::VIs, std::to_wstring(vis) + L" VI/s");
		SetStatusText(STATUS_ENUM::PEs, std::to_wstring(pes) + L" PE/s");
		SetStatusText(STATUS_ENUM::SystemTime, Util::StringToWstring(perf->GetSystemTime()));

		Thread::Sleep(perf->metricsInterval);
	}

	PerfMetrics::PerfMetrics()
	{
		perfThread = EMUCreateThread(PerfThreadProc, false, this, "PerfThread");
	}

	PerfMetrics::~PerfMetrics()
	{
		EMUJoinThread(perfThread);
	}

	int64_t PerfMetrics::GetGekkoInstructionsCounter()
	{
		return Jdi->GetPerformanceCounter(0);
	}

	void PerfMetrics::ResetGekkoInstructionsCounter()
	{
		Jdi->ResetPerformanceCounter(0);
	}

	int64_t PerfMetrics::GetGekkoCompiledSegments()
	{
		return Jdi->GetPerformanceCounter(4);
	}

	void PerfMetrics::ResetGekkoCompiledSegments()
	{
		Jdi->ResetPerformanceCounter(4);
	}

	int64_t PerfMetrics::GetGekkoExecutedSegments()
	{
		return Jdi->GetPerformanceCounter(5);
	}

	void PerfMetrics::ResetGekkoExecutedSegments()
	{
		Jdi->ResetPerformanceCounter(5);
	}

	int64_t PerfMetrics::GetDspInstructionsCounter()
	{
		return Jdi->GetPerformanceCounter(1);
	}

	void PerfMetrics::ResetDspInstructionsCounter()
	{
		Jdi->ResetPerformanceCounter(1);
	}

	int32_t PerfMetrics::GetVICounter()
	{
		return (int32_t)Jdi->GetPerformanceCounter(2);
	}

	void PerfMetrics::ResetVICounter()
	{
		Jdi->ResetPerformanceCounter(2);
	}

	int32_t PerfMetrics::GetPECounter()
	{
		return (int32_t)Jdi->GetPerformanceCounter(3);
	}

	void PerfMetrics::ResetPECounter()
	{
		Jdi->ResetPerformanceCounter(3);
	}

	std::string PerfMetrics::GetSystemTime()
	{
		return Jdi->GetSystemTime();
	}

}



// emulation has started - do proper actions
void OnMainWindowOpened(const wchar_t* currentFileName)
{
	std::wstring newTitle, gameTitle;
	wchar_t drive[_MAX_DRIVE + 1] = { 0, }, dir[_MAX_DIR] = { 0, }, name[_MAX_PATH] = { 0, }, ext[_MAX_EXT] = { 0, };
	bool dvd = false;
	bool bootrom = !wcscmp(currentFileName, L"Bootrom");

	if (!bootrom)
	{
		wchar_t* extension = wcsrchr((wchar_t*)currentFileName, L'.');

		if (!_wcsicmp(extension, L".dol"))
		{
			dvd = false;
		}
		else if (!_wcsicmp(extension, L".elf"))
		{
			dvd = false;
		}
		else if (!_wcsicmp(extension, L".iso"))
		{
			dvd = true;
		}
		else if (!_wcsicmp(extension, L".gcm"))
		{
			dvd = true;
		}

		_wsplitpath_s(currentFileName,
			drive, _countof(drive) - 1,
			dir, _countof(dir) - 1,
			name, _countof(name) - 1,
			ext, _countof(ext) - 1);
	}

	// set new title for main window

	if (dvd)
	{
		UI::Jdi->DvdMount(Util::WstringToString(currentFileName));

		// get DiskID
		std::vector<uint8_t> diskID;
		diskID.resize(4);
		UI::Jdi->DvdSeek(0);
		UI::Jdi->DvdRead(diskID);

		// Get title from banner

		std::vector<uint8_t> bnrRaw = DVDLoadBanner(currentFileName);

		DVDBanner2* bnr = (DVDBanner2*)bnrRaw.data();

		wchar_t longTitle[0x200];

		char* ansiPtr = (char*)bnr->comments[0].longTitle;
		wchar_t* wcharPtr = longTitle;

		while (*ansiPtr)
		{
			*wcharPtr++ = (uint8_t)*ansiPtr++;
		}
		*wcharPtr++ = 0;

		// Convert SJIS Title to Unicode

		if (UI::Jdi->DvdRegionById((char*)diskID.data()) == "JPN")
		{
			size_t size, chars;
			uint16_t* widePtr = SjisToUnicode(longTitle, &size, &chars);
			uint16_t* unicodePtr;

			if (widePtr)
			{
				wcharPtr = longTitle;
				unicodePtr = widePtr;

				while (*unicodePtr)
				{
					*wcharPtr++ = *unicodePtr++;
				}
				*wcharPtr++ = 0;

				free(widePtr);
			}
		}

		// Update recent files list and add selector path

		wchar_t fullPath[MAX_PATH];

		swprintf_s(fullPath, _countof(fullPath) - 1, L"%s%s", drive, dir);

		gameTitle = longTitle;
		newTitle = fmt::format(L"{:s} - Running {:s}", APPNAME, gameTitle);
	}
	else
	{
		if (bootrom)
		{
			gameTitle = currentFileName;
		}
		else
		{
			gameTitle = fmt::format(L"{:s} demo", name);
		}

		newTitle = fmt::format(L"{:s} - Running {:s}", APPNAME, gameTitle);
	}

	std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_conv;
	SDL_SetWindowTitle(window, utf8_conv.to_bytes(newTitle).c_str());

	UI::g_perfMetrics = new UI::PerfMetrics();
}

// emulation stop in progress
void OnMainWindowClosed()
{
	if (UI::g_perfMetrics != nullptr) {
		delete UI::g_perfMetrics;
		UI::g_perfMetrics = nullptr;
	}

	// set to Idle
	auto win_name = fmt::format(L"{:s} - {:s} ({:s})", APPNAME, APPDESC, Util::StringToWstring(UI::Jdi->GetVersion()));
	std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_conv;
	SDL_SetWindowTitle(window, utf8_conv.to_bytes(win_name).c_str());
}

static void ui_main_menu()
{
	// Menu Bar
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Open", NULL)) {
				file_reaction = FileReaction::OpenFile_LoadFile;
				fileOpenDialog.Open();
			}
			ImGui::MenuItem("Reopen", NULL);
			if (ImGui::MenuItem("Close", NULL)) {		// Unload file (STOP)
				UI::Jdi->Stop();
				Thread::Sleep(100);
				UI::Jdi->Unload();
				OnMainWindowClosed();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Run Bootrom", NULL)) {		// Load bootrom
				UI::Jdi->LoadFile("Bootrom");
				OnMainWindowOpened(L"Bootrom");
				if (Debug::debugger == nullptr)
				{
					UI::Jdi->Run();
				}
				else
				{
					Debug::debugger->SetDisasmCursor(0xfff0'0100);
					UI::Jdi->ExecuteCommand("echo \"Bootrom is started in Suspended state for debugging purposes. Press F5 to continue.\"");
				}
			}
			if (ImGui::BeginMenu("Swap Disk"))
			{
				ImGui::MenuItem("Open Cover", NULL);
				ImGui::MenuItem("Change DVD...", NULL);
				ImGui::EndMenu();
			}
			ImGui::Separator();
			ImGui::MenuItem("Refresh View", NULL);
			ImGui::Separator();
			if (ImGui::MenuItem("Exit", NULL)) {
				ui_active = false;
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Debug"))
		{
			if (ImGui::MenuItem("Debug Console", NULL, debugger_enabled_check)) {	// Open/close system-wide debugger
				if (Debug::debugger == nullptr)
				{   // open
					debugger_enabled_check = true;
					Debug::debugger = new Debug::Debugger();
					UI::Jdi->SetConfigBool(USER_DOLDEBUG, true, USER_UI);
					//SetStatusText(STATUS_ENUM::Progress, L"Debugger opened");
				}
				else
				{   // close
					debugger_enabled_check = false;
					delete Debug::debugger;
					Debug::debugger = nullptr;
					UI::Jdi->SetConfigBool(USER_DOLDEBUG, false, USER_UI);
					//SetStatusText(STATUS_ENUM::Progress, L"Debugger closed");
				}
			}
			ImGui::MenuItem("Mount DolphinSDK as DVD...", NULL);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Options"))
		{
			ImGui::MenuItem("Settings...", NULL);
			ImGui::MenuItem("View", NULL);
			ImGui::Separator();
			if (ImGui::BeginMenu("Controllers"))
			{
				ImGui::MenuItem("Port 1", NULL);
				ImGui::MenuItem("Port 2", NULL);
				ImGui::MenuItem("Port 3", NULL);
				ImGui::MenuItem("Port 4", NULL);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Memcards"))
			{
				ImGui::MenuItem("Slot A", NULL);
				ImGui::MenuItem("Slot B", NULL);
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help"))
		{
			ImGui::MenuItem("About...", NULL);
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}
}

static void ui_selector()
{
	const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	if (ImGui::BeginChild("selector", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar))
	{

	}
	ImGui::EndChild();
	ImGui::Separator();
}

static void ui_status_bar()
{
	for (int i = 0; i < (int)STATUS_ENUM::StatusMax; i++)
	{
		ImGui::TextWrapped(Util::WstringToString(status_parts[i]).c_str());
		ImGui::SameLine();
	}
}

static void ui_main_window()
{
	static bool use_work_area = true;
	static ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(use_work_area ? viewport->WorkPos : viewport->Pos);
	ImGui::SetNextWindowSize(use_work_area ? viewport->WorkSize : viewport->Size);

	if (ImGui::Begin("main_window", nullptr, flags))
	{
		ui_main_menu();
		ui_selector();
		ui_status_bar();
	}
	ImGui::End();
}

static int ui_main()
{
	EMUCtor();

	// debugger enabled ?
	debugger_enabled_check = UI::Jdi->GetConfigBool(USER_DOLDEBUG, USER_UI);
	if (debugger_enabled_check)
	{
		Debug::debugger = new Debug::Debugger();
	}

	// Create an interface for communicating with the emulator core
	UI::Jdi = new UI::JdiClient;

	// Add UI methods
	JdiAddNode(UI_JDI_JSON, UIReflector);
	JdiAddNode(DEBUG_UI_JDI_JSON, Debug::DebugUIReflector);

	// Start the user interface

	fileOpenDialog.SetTitle("Open File");
	fileOpenDialog.SetTypeFilters({ ".dol", ".elf", ".gcm", ".iso", ".map", ".json", ".bin" });
	
	fileSaveDialog.SetTitle("Save File");
	fileSaveDialog.SetTypeFilters({ ".dol", ".elf", ".gcm", ".iso", ".map", ".json", ".bin" });

	chooseDirectoryDialog.SetTitle("Choose Directory");

	CreateStatusBar();

	// From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
	SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

	// Create window with SDL_Renderer graphics context
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	window = SDL_CreateWindow(APPNAME_A, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, window_flags);
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_SOFTWARE);
	if (renderer == nullptr) {
		SDL_Log("Error creating SDL_Renderer!");
		return -1;
	}

	// simulate close operation, like we just stopped emu
	OnMainWindowClosed();

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	//ImGui::StyleColorsClassic();
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer2_Init(renderer);

	ui_active = true;

	// Main loop

	while (ui_active) {

		SDL_Event event;
		while (SDL_PollEvent(&event)) {

			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_QUIT)
				ui_active = false;
			if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
				ui_active = false;
		}

		// Start the Dear ImGui frame
		ImGui_ImplSDLRenderer2_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
		ui_main_window();
		if (show_demo_window) {
			ImGui::ShowDemoWindow(&show_demo_window);
		}
		
		fileOpenDialog.Display();
		if (fileOpenDialog.HasSelected())
		{
			auto name = fileOpenDialog.GetSelected().string();
			fileOpenDialog.ClearSelected();

			switch (file_reaction)
			{
				case FileReaction::OpenFile_LoadFile:
					if (!name.empty())
					{
						UI::Jdi->LoadFile(name);
						if (Debug::debugger)
						{
							Debug::debugger->InvalidateAll();
						}
						OnMainWindowOpened(Util::StringToWstring(name).c_str());
						UI::Jdi->Run();
					}
					break;
			}

			file_reaction = FileReaction::None;
		}

		fileSaveDialog.Display();
		if (fileSaveDialog.HasSelected())
		{
			fileSaveDialog.ClearSelected();
		}

		chooseDirectoryDialog.Display();
		if (chooseDirectoryDialog.HasSelected())
		{
			chooseDirectoryDialog.ClearSelected();
		}

		// Rendering
		ImGui::Render();
		ImGuiIO& io = ImGui::GetIO();
		SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
		SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
		SDL_RenderClear(renderer);
		ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
		SDL_RenderPresent(renderer);

		SDL_Delay(10);
	}

	// Main window closed

	// Cleanup
	ImGui_ImplSDLRenderer2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	UI::Jdi->Unload();

	JdiRemoveNode(UI_JDI_JSON);
	JdiRemoveNode(DEBUG_UI_JDI_JSON);

	if (Debug::debugger) {
		delete Debug::debugger;
	}

	EMUDtor();
	UI::Jdi->ExecuteCommand("exit");

	return 0;
}

#ifdef _WINDOWS

// Entry point for Win32 applications, quickly going straight to SDL

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	return ui_main();
}

#else

int main(int argc, char** argv)
{
	return ui_main();
}

#endif
