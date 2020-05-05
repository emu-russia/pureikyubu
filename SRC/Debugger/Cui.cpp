
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
		assert(cuiThread);
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

		FreeConsole();
	}

	void Cui::CuiThreadProc(void* Parameter)
	{
		INPUT_RECORD record;
		DWORD count;

		Cui* cui = (Cui*)Parameter;

		while (true)
		{
			Sleep(10);

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
				continue;

			ReadConsoleInput(cui->StdInput, &record, 1, &count);
			if (!count)
				continue;

			if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown)
			{
				char ascii = record.Event.KeyEvent.uChar.AsciiChar;
				int vcode = record.Event.KeyEvent.wVirtualKeyCode;
				int ctrl = record.Event.KeyEvent.dwControlKeyState;
				bool shiftPressed = ctrl & SHIFT_PRESSED;
				bool ctrlPressed = ctrl & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED);

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
	}

	void Cui::AddWindow(CuiWindow* wnd)
	{
		windows.push_back(wnd);
	}

	void Cui::SetWindowFocus(std::string name)
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

	void Cui::SetCursor(COORD& pos)
	{
		SetConsoleCursorPosition(StdOutput, pos);
	}

	void Cui::BlitWindow(CuiWindow* wnd)
	{
		COORD sz;
		sz.X = wnd->width;
		sz.Y = wnd->height;

		COORD pos;
		pos.X = 0;
		pos.Y = 0;

		SMALL_RECT rgn;
		rgn.Left = wnd->wndRect.left;
		rgn.Top = wnd->wndRect.top;
		rgn.Right = wnd->wndRect.right;
		rgn.Bottom = wnd->wndRect.bottom;

		WriteConsoleOutput(StdOutput, wnd->backBuf, sz, pos, &rgn);
	}

#pragma endregion "Cui"


#pragma region "CuiWindow"

	CuiWindow::CuiWindow(RECT& rect, std::string name)
	{
		wndRect = rect;
		wndName = name;

		width = (size_t)rect.right - (size_t)rect.left;
		height = (size_t)rect.bottom - (size_t)rect.top;

		backBuf = new CHAR_INFO[width * height];
		assert(backBuf);

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
			PutChar(back, front, x++, y, *it);
		}
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

#pragma endregion "CuiWindow"

}
