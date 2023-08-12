// Cui using imgui
#include "pch.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"

namespace Debug
{

	static SDL_Window* window;
	static SDL_Renderer* renderer;
	static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	static bool ShowImguiDemo = false;

#pragma region "Cui"

	Cui::Cui(std::string title, size_t width, size_t height)
	{
		conWidth = width;
		conHeight = height;

		frontBuf = new CHAR_INFO[width * height];
		memset(frontBuf, 0, sizeof(CHAR_INFO) * width * height);

		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

		// TODO: Get the size of the widest glyph and use it to set the window size

		//ImVec2 char_size = ImGui::CalcTextSize("X");
		int window_width = 1024;// char_size.x* conWidth;
		int window_height = 500;// char_size.y* conHeight;

		// Create window with SDL_Renderer graphics context
		SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
		window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, window_width, window_height, window_flags);
		renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_SOFTWARE);
		if (renderer == nullptr)
		{
			SDL_Log("Error creating SDL_Renderer!");
			return;
		}

		// Setup Dear ImGui style
		//ImGui::StyleColorsClassic();
		ImGui::StyleColorsDark();
		//ImGui::StyleColorsLight();

		// Setup Platform/Renderer backends
		ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
		ImGui_ImplSDLRenderer2_Init(renderer);

		cuiThread = EMUCreateThread(CuiThreadProc, false, this, "CuiThread");
	}

	Cui::~Cui()
	{
		EMUJoinThread(cuiThread);

		// Cleanup
		ImGui_ImplSDLRenderer2_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();

		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		
		renderer = nullptr;
		window = nullptr;

		while (!windows.empty())
		{
			CuiWindow* wnd = windows.back();
			windows.pop_back();
			delete wnd;
		}

		delete[] frontBuf;
	}

	static ImVec4 AttributeToImguiColor(uint16_t attr)
	{
		// https://github.com/ocornut/imgui/issues/1566

		CuiColor front_color = (CuiColor)(attr & 0xf);
		const float noi = 0.7f;		// no intensity bit
		const float yei = 1.0f;		// yes intensity bit

		switch (front_color)
		{
			case CuiColor::Black: return ImVec4(0, 0, 0, 1.0f);
			case CuiColor::DarkBlue: return ImVec4(0, 0, noi, 1.0f);
			case CuiColor::Green: return ImVec4(0, noi, 0, 1.0f);
			case CuiColor::Cyan: return ImVec4(0, noi, noi, 1.0f);
			case CuiColor::Red: return ImVec4(noi, 0, 0, 1.0f);
			case CuiColor::Purple: return ImVec4(noi, 0, noi, 1.0f);
			case CuiColor::Brown: return ImVec4(noi, noi, 0, 1.0f);
			case CuiColor::Normal: return ImVec4(noi, noi, noi, 1.0f);

			case CuiColor::Gray: return ImVec4(0, 0, 0, 1.0f);
			case CuiColor::Blue: return ImVec4(0, 0, yei, 1.0f);
			case CuiColor::Lime: return ImVec4(0, yei, 0, 1.0f);
			case CuiColor::BrightCyan: return ImVec4(0, yei, yei, 1.0f);
			case CuiColor::BrightRed: return ImVec4(yei, 0, 0, 1.0f);
			case CuiColor::BrightPurple: return ImVec4(yei, 0, yei, 1.0f);
			case CuiColor::Yellow: return ImVec4(yei, yei, 0, 1.0f);
			case CuiColor::White: return ImVec4(yei, yei, yei, 1.0f);

			default: return ImVec4(0, 0, 0, 1.0f);
		}
	}

	void Cui::CuiThreadProc(void* Parameter)
	{
		Cui* cui = (Cui*)Parameter;

		Thread::Sleep(10);

		// Pass key event

		SDL_Event event;
		while (SDL_PollEvent(&event)) {

			ImGui_ImplSDL2_ProcessEvent(&event);
			//if (event.type == SDL_QUIT)
			//    run_dbg_thread = false;
			//if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
			//	run_dbg_thread = false;

			// TODO: OnKeyPress
		}

		// Start the Dear ImGui frame
		ImGui_ImplSDLRenderer2_NewFrame();
		ImGui_ImplSDL2_NewFrame();
		ImGui::NewFrame();

		// Update

		for (auto it = cui->windows.begin(); it != cui->windows.end(); ++it)
		{
			CuiWindow* wnd = *it;

			if (wnd->invalidated)
			{
				wnd->OnDraw();
				cui->BlitWindow(wnd);
				wnd->invalidated = false;
			}
		}

		// Draw frontBuf

		if (!ShowImguiDemo)
		{
			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);

			if (!ImGui::Begin("title", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
			{
				ImGui::End();
				return;
			}

			if (ImGui::BeginChild("frontBuf", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar))
			{
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0)); // Tighten spacing

				for (size_t y = 0; y < cui->conHeight; y++)
				{
					for (size_t x = 0; x < cui->conWidth; x++)
					{
						CHAR_INFO* char_info = &cui->frontBuf[cui->conWidth * y + x];

						ImVec4 color = AttributeToImguiColor(char_info->Attributes);
						ImGui::SameLine();
						ImGui::TextColored(color, "%c", char_info->Char.AsciiChar);
					}
					if (y != cui->conHeight - 1)
						ImGui::Text("\n");
				}

				ImGui::PopStyleVar();
			}
			ImGui::EndChild();

			// TODO: Draw cursor

			// Auto-focus on window apparition
			ImGui::SetItemDefaultFocus();
			ImGui::End();
		}
		else
		{

			// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
			//if (show_demo_window)
			ImGui::ShowDemoWindow(nullptr);
		}

		// Rendering
		ImGui::Render();
		ImGuiIO& io = ImGui::GetIO();
		SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
		SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
		SDL_RenderClear(renderer);
		ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
		SDL_RenderPresent(renderer);
	}

	void Cui::AddWindow(CuiWindow* wnd)
	{
		windows.push_back(wnd);
	}

	void Cui::SetWindowFocus(const std::string& name)
	{
		for (auto it = windows.begin(); it != windows.end(); ++it)
		{
			CuiWindow* wnd = *it;
			wnd->SetFocus(wnd->wndName == name);
		}
	}

	void Cui::OnKeyPress(char Ascii, CuiVkey Vkey, bool shift, bool ctrl)
	{
	}

	void Cui::ShowCursor(bool show)
	{
		cursor_visible = show;
	}

	void Cui::SetCursor(int x, int y)
	{
		cursor_x = x;
		cursor_y = y;
	}

	void Cui::BlitWindow(CuiWindow* wnd)
	{
		for (size_t y = 0; y < wnd->height; y++)
		{
			size_t con_y = wnd->wndRect.top + y;
			if (con_y >= conHeight)
				break;

			for (size_t x = 0; x < wnd->width; x++)
			{
				size_t con_x = wnd->wndRect.left + x;
				if (con_x >= conWidth)
					continue;

				frontBuf[conWidth * con_y + con_x] = wnd->backBuf[wnd->width * y + x];
			}
		}
	}

	void Cui::InvalidateAll()
	{
		for (auto it = windows.begin(); it != windows.end(); ++it)
		{
			CuiWindow* wnd = *it;
			wnd->Invalidate();
		}
	}

#pragma endregion "Cui"


#pragma region "CuiWindow"

	CuiWindow::CuiWindow(CuiRect& rect, std::string name, Cui* parent)
	{
		wndRect = rect;
		wndName = name;
		cui = parent;

		width = (size_t)rect.right - (size_t)rect.left + 1;
		height = (size_t)rect.bottom - (size_t)rect.top + 1;

		backBuf = new CHAR_INFO[width * height];

		memset(backBuf, 0, width * height * sizeof(CHAR_INFO));
	}

	CuiWindow::~CuiWindow()
	{
		delete[] backBuf;
	}

	void CuiWindow::PutChar(CuiColor back, CuiColor front, int x, int y, char c)
	{
		if (x < 0 || x >= width)
			return;
		if (y < 0 || y >= height)
			return;

		CHAR_INFO* info = &backBuf[y * width + x];

		info->Attributes = ((int)back << 4) | (int)front;
		info->Char.AsciiChar = c;
	}

	void CuiWindow::Print(CuiColor back, CuiColor front, int x, int y, std::string text)
	{
		for (auto it = text.begin(); it != text.end(); ++it)
		{
			PutChar(back, front, x++, y, *it >= ' ' ? *it : ' ');
		}
	}

	void CuiWindow::Print(CuiColor front, int x, int y, std::string text)
	{
		for (auto it = text.begin(); it != text.end(); ++it)
		{
			PutChar(CuiColor::Black, front, x++, y, *it >= ' ' ? *it : ' ');
		}
	}

	void CuiWindow::Print(CuiColor back, CuiColor front, int x, int y, const char* fmt, ...)
	{
		char    buf[0x1000];
		va_list arg;

		va_start(arg, fmt);
		vsprintf_s(buf, sizeof(buf) - 1, fmt, arg);
		va_end(arg);

		std::string text = buf;
		Print(back, front, x, y, text);
	}

	void CuiWindow::Print(CuiColor front, int x, int y, const char* fmt, ...)
	{
		char    buf[0x1000];
		va_list arg;

		va_start(arg, fmt);
		vsprintf_s(buf, sizeof(buf) - 1, fmt, arg);
		va_end(arg);

		std::string text = buf;
		Print(CuiColor::Black, front, x, y, text);
	}

	void CuiWindow::Fill(CuiColor back, CuiColor front, char c)
	{
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				PutChar(back, front, x, y, c);
			}
		}
	}

	void CuiWindow::FillLine(CuiColor back, CuiColor front, int y, char c)
	{
		for (int x = 0; x < width; x++)
		{
			PutChar(back, front, x, y, c);
		}
	}

	void CuiWindow::SetCursor(int x, int y)
	{
		cui->SetCursor(wndRect.left + x, wndRect.top + y);
	}

#pragma endregion "CuiWindow"

}
