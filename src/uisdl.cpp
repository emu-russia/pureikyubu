// Portable UI based on SDL+imgui
#include "pch.h"
#ifdef _WINDOWS
#include <SDL_syswm.h>
#endif

static bool ui_active = false;
static bool show_demo_window = true;
static SDL_Window* window;
static SDL_Renderer* renderer;
static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
static bool debugger_enabled_check = false;

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
	SDL_GetWindowWMInfo(window, &wmInfo);
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

// emulation stop in progress
void OnMainWindowClosed()
{
	//if (UI::g_perfMetrics != nullptr) {
	//	delete UI::g_perfMetrics;
	//	UI::g_perfMetrics = nullptr;
	//}

	// set to Idle
	auto win_name = fmt::format("{:s} - {:s} ({:s})", APPNAME_A, Util::WstringToString(APPDESC), UI::Jdi->GetVersion());
	SDL_SetWindowTitle(window, win_name.c_str());
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
		// Menu Bar
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				ImGui::MenuItem("Open", NULL);
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
					//OnMainWindowOpened(L"Bootrom");
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
