
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

		SetConsoleTitleA(title.c_str());

		cuiThread = new Thread(CuiThreadProc, false, this, "CuiThread");
	}

	Cui::~Cui()
	{
		delete cuiThread;

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

	void Cui::CuiThreadProc(void* Parameter)
	{
		INPUT_RECORD record;
		DWORD count;

		Cui* cui = (Cui*)Parameter;

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

		ReadConsoleInput(cui->StdInput, &record, 1, &count);
		if (!count)
			return;

		if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown)
		{
			char ascii = record.Event.KeyEvent.uChar.AsciiChar;
			int vcode = record.Event.KeyEvent.wVirtualKeyCode;
			int ctrl = record.Event.KeyEvent.dwControlKeyState;
			bool shiftPressed = (ctrl & SHIFT_PRESSED) != 0;
			bool ctrlPressed = (ctrl & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;

			cui->OnKeyPress(ascii, vcode, shiftPressed, ctrlPressed);

			for (auto it = cui->windows.begin(); it != cui->windows.end(); ++it)
			{
				CuiWindow* wnd = *it;

				if (wnd->active)
				{
					wnd->OnKeyPress(ascii, vcode, shiftPressed, ctrlPressed);
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

	void Cui::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
	}

	void Cui::ShowCursor(bool show)
	{
		CONSOLE_CURSOR_INFO info = { 0 };

		info.bVisible = show;
		info.dwSize = 1;

		SetConsoleCursorInfo(StdOutput, &info);
	}

	void Cui::SetCursor(int x, int y)
	{
		COORD pos;

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

		WriteConsoleOutput(StdOutput, wnd->backBuf, sz, pos, &rgn);
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

	CuiWindow::CuiWindow(RECT& rect, std::string name, Cui* parent)
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
			PutChar(back, front, x++, y, *it >= ' ' ? *it : ' ' );
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
