// Universal code for interacting with the Win32 console.

#pragma once

#include <string>
#include <list>
#include <Windows.h>
#include "../Common/Thread.h"

namespace Debug
{
	enum class CuiColor : int8_t
	{
		Black = 0,
		DarkBlue,
		Green,
		Cyan,
		Red,
		Purple,
		Brown,
		Normal,
		Gray,
		Blue,
		Lime,
		BrightCyan,
		BrightRed,
		BrightPurple,
		Yellow,
		White,
	};

	class Cui;

	class CuiWindow
	{
		friend Cui;

		bool invalidated = true;
		bool active = false;
		std::string wndName;

		// This is where the contents of the window are stored. The region data is displayed by the wndRect coordinates.
		CHAR_INFO* backBuf = nullptr;

		// Window layout in CUI.
		RECT wndRect;

		size_t width;
		size_t height;

		void PutChar(CuiColor back, CuiColor front, int x, int y, char c);

	public:
		CuiWindow(RECT& rect, std::string name);
		virtual ~CuiWindow();

		// Redraw itself if invalidated.
		virtual void OnDraw() = 0;

		// Key event. Comes only if the window is active (SetFocus true)
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl) = 0;

		void Invalidate() { invalidated = true; }
		bool NeedRedraw() { return invalidated;  }

		void SetFocus(bool flag) { active = flag; }

		void Print(CuiColor back, CuiColor front, int x, int y, std::string text);
		void Fill(CuiColor back, CuiColor front, char c);
	};

	class Cui
	{
		std::list<CuiWindow*> windows;

		HANDLE StdInput;
		HANDLE StdOutput;

		size_t conWidth;
		size_t conHeight;

		Thread* cuiThread = nullptr;
		static void CuiThreadProc(void* Parameter);

		void BlitWindow(CuiWindow* wnd);

	public:
		Cui(std::string title, size_t width, size_t height);
		virtual ~Cui();

		void AddWindow(CuiWindow* wnd);

		void SetWindowFocus(std::string name);

		// A global CUI key event handler (for example, to switch focus between windows). 
		// In addition, each active window also receives key event.

		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);

		void ShowCursor(bool show);
		void SetCursor(COORD& pos);
	};
}
