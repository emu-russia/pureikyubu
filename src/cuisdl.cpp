// Cui using imgui
#include "pch.h"

// TODO: Get the size of the widest glyph and use it to set the window size
// TODO: F5 not working x_x
// TODO: All the @ # * and stuff (input).


namespace Debug
{

	static SDL_Window* window;
	static SDL_Renderer* renderer;
	static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	static bool ShowImguiDemo = false;
	static int cursor_blink_counter = 0;
	static const int cui_thread_delay = 10;		// ms
	static int char_width = 8;
	static int char_height = 8;

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
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

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

	static ImU32 CuiColorToImguiColor(CuiColor color)
	{
		switch (color)
		{
			case CuiColor::Black: return 0x000000ff;
			case CuiColor::DarkBlue: return 0x0000aaff;
			case CuiColor::Green: return 0x00aa00ff;
			case CuiColor::Cyan: return 0x00aaaaff;
			case CuiColor::Red: return 0xaa0000ff;
			case CuiColor::Purple: return 0xaa00aaff;
			case CuiColor::Brown: return 0xaa5500ff;
			case CuiColor::Normal: return 0xaaaaaaff;

			case CuiColor::Gray: return 0x555555ff;
			case CuiColor::Blue: return 0x5555ffff;
			case CuiColor::Lime: return 0x55ff55ff;
			case CuiColor::BrightCyan: return 0x55ffffff;
			case CuiColor::BrightRed: return 0xff5555ff;
			case CuiColor::BrightPurple: return 0xff55ffff;
			case CuiColor::Yellow: return 0xffff55ff;
			case CuiColor::White: return 0xffffffff;

			default: return 0xff000000;
		}
	}

	static bool SdlEventToCuiKeypress(SDL_Event& event, char& Ascii, CuiVkey& Vkey, bool& shift, bool& ctrl)
	{
		if (event.type == SDL_KEYDOWN) {

			shift = (event.key.keysym.mod & KMOD_LSHIFT) || (event.key.keysym.mod & KMOD_RSHIFT);
			ctrl = (event.key.keysym.mod & KMOD_LCTRL) || (event.key.keysym.mod & KMOD_RCTRL);

			if (event.key.keysym.sym >= ' ' && event.key.keysym.sym < 127) {

				// TODO: All the @ # * and stuff.

				Ascii = event.key.keysym.sym;
				if (shift)
					Ascii = toupper(Ascii);
			}

			else switch (event.key.keysym.scancode)
			{
				case SDL_Scancode::SDL_SCANCODE_UP: Vkey = CuiVkey::Up; break;
				case SDL_Scancode::SDL_SCANCODE_DOWN: Vkey = CuiVkey::Down; break;
				case SDL_Scancode::SDL_SCANCODE_LEFT: Vkey = CuiVkey::Left; break;
				case SDL_Scancode::SDL_SCANCODE_RIGHT: Vkey = CuiVkey::Right; break;
				case SDL_Scancode::SDL_SCANCODE_PAGEUP: Vkey = CuiVkey::PageUp; break;
				case SDL_Scancode::SDL_SCANCODE_PAGEDOWN: Vkey = CuiVkey::PageDown; break;
				case SDL_Scancode::SDL_SCANCODE_HOME: Vkey = CuiVkey::Home; break;
				case SDL_Scancode::SDL_SCANCODE_END: Vkey = CuiVkey::End; break;
				case SDL_Scancode::SDL_SCANCODE_ESCAPE: Vkey = CuiVkey::Escape; break;
				case SDL_Scancode::SDL_SCANCODE_RETURN: Vkey = CuiVkey::Enter; break;
				case SDL_Scancode::SDL_SCANCODE_BACKSPACE: Vkey = CuiVkey::Backspace; break;
				case SDL_Scancode::SDL_SCANCODE_DELETE: Vkey = CuiVkey::Delete; break;

				case SDL_Scancode::SDL_SCANCODE_F1: Vkey = CuiVkey::F1; break;
				case SDL_Scancode::SDL_SCANCODE_F2: Vkey = CuiVkey::F2; break;
				case SDL_Scancode::SDL_SCANCODE_F3: Vkey = CuiVkey::F3; break;
				case SDL_Scancode::SDL_SCANCODE_F4: Vkey = CuiVkey::F4; break;
				case SDL_Scancode::SDL_SCANCODE_F5: Vkey = CuiVkey::F5; break;
				case SDL_Scancode::SDL_SCANCODE_F6: Vkey = CuiVkey::F6; break;
				case SDL_Scancode::SDL_SCANCODE_F7: Vkey = CuiVkey::F7; break;
				case SDL_Scancode::SDL_SCANCODE_F8: Vkey = CuiVkey::F8; break;
				case SDL_Scancode::SDL_SCANCODE_F9: Vkey = CuiVkey::F9; break;
				case SDL_Scancode::SDL_SCANCODE_F10: Vkey = CuiVkey::F10; break;
				case SDL_Scancode::SDL_SCANCODE_F11: Vkey = CuiVkey::F11; break;
				case SDL_Scancode::SDL_SCANCODE_F12: Vkey = CuiVkey::F12; break;

				default:
					return false;
			}

			return true;
		}

		return false;
	}

	void Cui::CuiThreadProc(void* Parameter)
	{
		Cui* cui = (Cui*)Parameter;

		Thread::Sleep(cui_thread_delay);

		// Pass key event

		SDL_Event event;
		while (SDL_PollEvent(&event)) {

			if (event.type != SDL_KEYDOWN) {
				ImGui_ImplSDL2_ProcessEvent(&event);
			}

			// OnKeyPress

			char ascii;
			CuiVkey cui_vk;
			bool shiftPressed;
			bool ctrlPressed;

			if (SdlEventToCuiKeypress(event, ascii, cui_vk, shiftPressed, ctrlPressed)) {

				cui->OnKeyPress(ascii, cui_vk, shiftPressed, ctrlPressed);

				for (auto it = cui->windows.begin(); it != cui->windows.end(); ++it)
				{
					CuiWindow* wnd = *it;

					if (wnd->active)
					{
						wnd->OnKeyPress(ascii, cui_vk, shiftPressed, ctrlPressed);
					}
				}
			}
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

			ImVec2 char_size = ImGui::CalcTextSize("A");
			char_width = (int)char_size.x;
			char_height = (int)char_size.y;

			ImDrawList* draw_list = ImGui::GetWindowDrawList();

			// Draw cui windows

			if (ImGui::BeginChild("frontBuf", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar))
			{
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0)); // Tighten spacing

				for (size_t y = 0; y < cui->conHeight; y++)
				{
					for (size_t x = 0; x < cui->conWidth; x++)
					{
						// Draw cursor
						if (cui->cursor_visible && x == cui->cursor_x && y == cui->cursor_y) {
							
							ImVec2 p0 = ImGui::GetCursorScreenPos();
							p0.x += x * char_width;
							ImVec2 p1 = ImVec2(p0.x, p0.y - char_height);
							draw_list->AddLine(p0, p1, 0x80ffffff, 2.0f);
						}

						CHAR_INFO* char_info = &cui->frontBuf[cui->conWidth * y + x];

						// Background color
						CuiColor bk_color = (CuiColor)((char_info->Attributes >> 4) & 0xf);
						if (bk_color != CuiColor::Black) {

							ImVec2 p0 = ImGui::GetCursorScreenPos();
							p0.x += x * char_width;
							ImVec2 p1 = ImVec2(p0.x + char_width, p0.y - char_height);

							ImU32 fill_color = _BYTESWAP_UINT32(CuiColorToImguiColor(bk_color));
							draw_list->AddRectFilled(p0, p1, fill_color);
						}

						ImVec4 color = ImGui::ColorConvertU32ToFloat4(
							_BYTESWAP_UINT32( CuiColorToImguiColor((CuiColor)(char_info->Attributes & 0xf)) ) );
						ImGui::SameLine();
						ImGui::TextColored(color, "%c", char_info->Char.AsciiChar);
					}
					if (y != cui->conHeight - 1)
						ImGui::Text("\n");
				}

				ImGui::PopStyleVar();
			}
			ImGui::EndChild();

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
		vsprintf(buf, fmt, arg);
		va_end(arg);

		std::string text = buf;
		Print(back, front, x, y, text);
	}

	void CuiWindow::Print(CuiColor front, int x, int y, const char* fmt, ...)
	{
		char    buf[0x1000];
		va_list arg;

		va_start(arg, fmt);
		vsprintf(buf, fmt, arg);
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
