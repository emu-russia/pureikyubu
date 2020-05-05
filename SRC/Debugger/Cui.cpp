
#include "pch.h"

namespace Debug
{

#pragma region "Cui"

	Cui::Cui(std::string title, size_t width, size_t height)
	{
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
	}

	Cui::~Cui()
	{
		while (!windows.empty())
		{
			CuiWindow* wnd = windows.back();
			windows.pop_back();
			delete wnd;
		}

		FreeConsole();
	}

	void Cui::AddWindow(CuiWindow* wnd)
	{

	}

	void Cui::SetWindowFocus(std::string name)
	{

	}

	void Cui::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{

	}

#pragma endregion "Cui"


#pragma region "CuiWindow"

	CuiWindow::CuiWindow(RECT& rect, std::string name)
	{
		wndRect = rect;
		wndName = name;

		size_t width = (size_t)rect.right - (size_t)rect.left;
		size_t height = (size_t)rect.bottom - (size_t)rect.top;

		backBuf = new CHAR_INFO[width * height];
		assert(backBuf);

		memset(backBuf, 0, width * height * sizeof(CHAR_INFO));
	}

	CuiWindow::~CuiWindow()
	{
		delete[] backBuf;
	}

#pragma endregion "CuiWindow"

}
