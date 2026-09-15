// Cui using Win32 Console API
#include "pch.h"

namespace Debug
{

#pragma region "Cui"

	Cui::Cui(std::string title, size_t width, size_t height)
	{
		conWidth = width;
		conHeight = height;

		AllocConsole();

		StdInput = GetStdHandle(STD_INPUT_HANDLE);
		assert(StdInput != INVALID_HANDLE_VALUE);
		StdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
		assert(StdOutput != INVALID_HANDLE_VALUE);

		// Disable mouse input
		DWORD flags;
		GetConsoleMode(StdInput, &flags);
		flags &= ~ENABLE_MOUSE_INPUT;
		SetConsoleMode(StdInput, flags);

		// Resize console
		COORD coord;
		SMALL_RECT rect;

		rect.Top = rect.Left = 0;
		rect.Right = (SHORT)width - 1;
		rect.Bottom = (SHORT)height - 1;

		coord.X = (SHORT)width;
		coord.Y = (SHORT)height;

		SetConsoleWindowInfo(StdOutput, TRUE, &rect);
		SetConsoleScreenBufferSize(StdOutput, coord);

		// The title is UTF-8 like every other string of the project, and the console wants Unicode.
		SetConsoleTitleW(Util::StringToWstring(title).c_str());

		cuiThread = EMUCreateThread(CuiThreadProc, false, this, "CuiThread");
	}

	Cui::~Cui()
	{
		EMUJoinThread(cuiThread);

		while (!windows.empty())
		{
			CuiWindow* wnd = windows.back();
			windows.pop_back();
			delete wnd;
		}

		// Don't touch console handles, they can be used by Visual Studio or by other parasites.

		FreeConsole();

		// Required for Win32 internals to stabilize after closing Cui
		Sleep(100);
	}

	/// <summary>
	/// Convert Windows VK to abstract Cui VK, to improve code portability.
	/// </summary>
	static CuiVkey WindowVKToCuiVkey(WORD vk)
	{
		switch (vk)
		{
			case VK_UP: return CuiVkey::Up;
			case VK_DOWN: return CuiVkey::Down;
			case VK_LEFT: return CuiVkey::Left;
			case VK_RIGHT: return CuiVkey::Right;
			case VK_PRIOR: return CuiVkey::PageUp;
			case VK_NEXT: return CuiVkey::PageDown;
			case VK_HOME: return CuiVkey::Home;
			case VK_END: return CuiVkey::End;
			case VK_ESCAPE: return CuiVkey::Escape;
			case VK_RETURN: return CuiVkey::Enter;
			case VK_BACK: return CuiVkey::Backspace;
			case VK_DELETE: return CuiVkey::Delete;
			case VK_F1: return CuiVkey::F1;
			case VK_F2: return CuiVkey::F2;
			case VK_F3: return CuiVkey::F3;
			case VK_F4: return CuiVkey::F4;
			case VK_F5: return CuiVkey::F5;
			case VK_F6: return CuiVkey::F6;
			case VK_F7: return CuiVkey::F7;
			case VK_F8: return CuiVkey::F8;
			case VK_F9: return CuiVkey::F9;
			case VK_F10: return CuiVkey::F10;
			case VK_F11: return CuiVkey::F11;
			case VK_F12: return CuiVkey::F12;
			default: break;
		}
		return CuiVkey::Unknown;
	}

	void Cui::CuiThreadProc(void* Parameter)
	{
		INPUT_RECORD record;
		DWORD count;

		Cui* cui = (Cui*)Parameter;

		// The first half of a character outside the BMP is held here until the second one arrives:
		// the console reports the two halves as two key events, and only the pair is text (one
		// UTF-16 half on its own is not a code point, and would become a replacement character).
		wchar_t pendingHighSurrogate = 0;

		Thread::Sleep(10);

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

		// Pass key event

		PeekConsoleInput(cui->StdInput, &record, 1, &count);
		if (!count)
			return;

		// The wide variant: it is the one that reports the character the key stands for, which the
		// ANSI one can only do inside the console code page.
		ReadConsoleInputW(cui->StdInput, &record, 1, &count);
		if (!count)
			return;

		if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown)
		{
			wchar_t unicode = record.Event.KeyEvent.uChar.UnicodeChar;
			int vcode = record.Event.KeyEvent.wVirtualKeyCode;
			int ctrl = record.Event.KeyEvent.dwControlKeyState;
			bool shiftPressed = (ctrl & SHIFT_PRESSED) != 0;
			bool ctrlPressed = (ctrl & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;

			CuiVkey cui_vk = WindowVKToCuiVkey(vcode);

			// The key event carries the UTF-8 of the character it produced, and an empty string for
			// a key that produced none. A control character is not text: the keys that produce one
			// (Enter, Escape, the arrows) are the ones a window reads from `Vkey`.
			if (unicode < 0x20 || unicode == 0x7F)
			{
				unicode = 0;
			}

			std::string text;

			// Windows reports a character outside the BMP as the two halves of its UTF-16 pair, in
			// two key events. The first half is held back until the second one arrives so that the
			// pair becomes one character again: a half on its own is not a code point.
			if (pendingHighSurrogate != 0)
			{
				wchar_t high = pendingHighSurrogate;
				pendingHighSurrogate = 0;

				if (unicode >= 0xDC00 && unicode <= 0xDFFF)
				{
					wchar_t pair[3] = { high, unicode, 0 };
					text = Util::WstringToString(pair);
					unicode = 0;
				}
			}

			if (unicode >= 0xD800 && unicode <= 0xDBFF)
			{
				pendingHighSurrogate = unicode;
			}
			else if (unicode != 0)
			{
				text = Util::WstringToString(std::wstring(1, unicode));
			}

			cui->OnKeyPress(text.c_str(), cui_vk, shiftPressed, ctrlPressed);

			for (auto it = cui->windows.begin(); it != cui->windows.end(); ++it)
			{
				CuiWindow* wnd = *it;

				if (wnd->active)
				{
					wnd->OnKeyPress(text.c_str(), cui_vk, shiftPressed, ctrlPressed);
				}
			}
		}
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

	void Cui::OnKeyPress(const char* Text, CuiVkey Vkey, bool shift, bool ctrl)
	{
	}

	void Cui::ShowCursor(bool show)
	{
		CONSOLE_CURSOR_INFO info = { 0 };

		cursor_visible = show;
		info.bVisible = show;
		info.dwSize = 1;

		SetConsoleCursorInfo(StdOutput, &info);
	}

	void Cui::SetCursor(int x, int y)
	{
		COORD pos;

		cursor_x = x;
		cursor_y = y;

		pos.X = x;
		pos.Y = y;

		SetConsoleCursorPosition(StdOutput, pos);
	}

	void Cui::BlitWindow(CuiWindow* wnd)
	{
		COORD sz;
		sz.X = (SHORT)wnd->width;
		sz.Y = (SHORT)wnd->height;

		COORD pos;
		pos.X = 0;
		pos.Y = 0;

		SMALL_RECT rgn;
		rgn.Left = (SHORT)wnd->wndRect.left;
		rgn.Top = (SHORT)wnd->wndRect.top;
		rgn.Right = (SHORT)wnd->wndRect.right;
		rgn.Bottom = (SHORT)wnd->wndRect.bottom;

		// The wide variant, so a character outside the console code page reaches the screen as
		// itself (the buffer carries UnicodeChar; the ANSI entry point would read its low byte).
		WriteConsoleOutputW(StdOutput, wnd->backBuf, sz, pos, &rgn);
	}

	void Cui::InvalidateAll()
	{
		for (auto it = windows.begin(); it != windows.end(); ++it)
		{
			CuiWindow* wnd = *it;
			wnd->Invalidate();
		}
	}

	void Cui::DrawInternal()
	{
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

	void CuiWindow::PutChar(CuiColor back, CuiColor front, int x, int y, wchar_t c)
	{
		if (x < 0 || x >= width)
			return;
		if (y < 0 || y >= height)
			return;

		CHAR_INFO* info = &backBuf[y * width + x];

		info->Attributes = ((int)back << 4) | (int)front;
		info->Char.UnicodeChar = c;
	}

	void CuiWindow::Print(CuiColor back, CuiColor front, int x, int y, std::string text)
	{
		// The text is UTF-8; the console buffer holds one wide character per cell. A code point
		// outside the BMP is a pair of them on Windows, which is exactly the two cells it renders
		// as, so the pair is stored as it is.
		std::wstring wide = Util::StringToWstring(text);

		for (auto it = wide.begin(); it != wide.end(); ++it)
		{
			PutChar(back, front, x++, y, *it >= L' ' ? *it : L' ');
		}
	}

	void CuiWindow::Print(CuiColor front, int x, int y, std::string text)
	{
		std::wstring wide = Util::StringToWstring(text);

		for (auto it = wide.begin(); it != wide.end(); ++it)
		{
			PutChar(CuiColor::Black, front, x++, y, *it >= L' ' ? *it : L' ');
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
