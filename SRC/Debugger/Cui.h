// Universal code for interacting with the Win32 console.

#pragma once

#include <string>
#include <list>
#include <Windows.h>
#include "../Common/Thread.h"

namespace Debug
{
	enum class CuiColor
	{
		Black = 0,
	};

	class Cui;

	class CuiWindow
	{
		friend Cui;

		bool invalidated = false;
		bool active = false;
		std::string wndName;

		// This is where the contents of the window are stored. The region data is displayed by the wndRect coordinates.
		CHAR_INFO* backBuf = nullptr;

		// Window layout in CUI.
		RECT wndRect;

	public:
		CuiWindow(RECT& rect, std::string name);
		~CuiWindow();

		// Redraw itself if invalidated.
		virtual void OnDraw() = 0;

		// Key event. Comes only if the window is active (SetFocus true)
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl) = 0;

		void Invalidate() { invalidated = true; }
		bool NeedRedraw() { return invalidated;  }

		void SetFocus(bool flag) { active = flag; }
	};

	class Cui
	{
		std::list<CuiWindow*> windows;

		HANDLE StdInput;
		HANDLE StdOutput;

	public:
		Cui(std::string title, size_t width, size_t height);
		virtual ~Cui();

		void AddWindow(CuiWindow* wnd);

		void SetWindowFocus(std::string name);

		// A global CUI key event handler (for example, to switch focus between windows). 
		// In addition, each active window also receives key event.

		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};
}
