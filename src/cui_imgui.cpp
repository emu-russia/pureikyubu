// Cui using imgui
#include "pch.h"

namespace Debug
{

#pragma region "Cui"

	Cui::Cui(std::string title, size_t width, size_t height)
	{
		conWidth = width;
		conHeight = height;

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
	}

	void Cui::CuiThreadProc(void* Parameter)
	{
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

		// TODO
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
		// TODO
	}

	void Cui::SetCursor(int x, int y)
	{
		// TODO
	}

	void Cui::BlitWindow(CuiWindow* wnd)
	{
		// TODO
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
