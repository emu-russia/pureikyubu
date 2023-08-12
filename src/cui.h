// Universal code for interacting with the debug console.

#pragma once

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

	/// <summary>
	/// In order not to use Win32 RECT this intermediate portable representation is used.
	/// </summary>
	struct CuiRect
	{
		long left;
		long top;
		long right;
		long bottom;
	};

	enum class CuiVkey
	{
		Unknown = 0,
		Up,
		Down,
		Left,
		Right,
		PageUp,
		PageDown,
		Home,
		End,
		Escape,
		Enter,
		Backspace,
		Delete,
		F1,
		F2,
		F3,
		F4,
		F5,
		F6,
		F7,
		F8,
		F9,
		F10,
		F11,
		F12,
	};

#ifndef _WINDOWS
	struct CHAR_INFO {
		union {
			wchar_t UnicodeChar;
			char   AsciiChar;
		} Char;
		uint16_t Attributes;
};
#endif

	class Cui;

	class CuiWindow
	{
		friend Cui;

		std::string wndName;

		// This is where the contents of the window are stored. The region data is displayed by the wndRect coordinates.
		CHAR_INFO* backBuf = nullptr;

		// Window layout in CUI.
		CuiRect wndRect{};

		void PutChar(CuiColor back, CuiColor front, int x, int y, char c);

	protected:
		size_t width = 0;
		size_t height = 0;
		bool invalidated = true;
		bool active = false;
		Cui* cui = nullptr;

	public:
		CuiWindow(CuiRect& rect, std::string name, Cui* parent);
		virtual ~CuiWindow();

		// Redraw itself if invalidated.
		virtual void OnDraw() = 0;

		// Key event. Comes only if the window is active (SetFocus true)
		virtual void OnKeyPress(char Ascii, CuiVkey Vkey, bool shift, bool ctrl) = 0;

		void Invalidate() { invalidated = true; }
		bool NeedRedraw() { return invalidated; }

		void SetFocus(bool flag) { active = flag; Invalidate(); }
		bool IsActive() { return active; }

		void Print(CuiColor back, CuiColor front, int x, int y, std::string text);
		void Print(CuiColor front, int x, int y, std::string text);
		void Print(CuiColor back, CuiColor front, int x, int y, const char* fmt, ...);
		void Print(CuiColor front, int x, int y, const char* fmt, ...);
		void Fill(CuiColor back, CuiColor front, char c);
		void FillLine(CuiColor back, CuiColor front, int y, char c);

		void SetCursor(int x, int y);
	};

	class Cui
	{
		std::list<CuiWindow*> windows;

#ifdef _WINDOWS
		HANDLE StdInput = 0;
		HANDLE StdOutput = 0;
#endif

		size_t conWidth = 0;
		size_t conHeight = 0;

		Thread* cuiThread = nullptr;
		static void CuiThreadProc(void* Parameter);

		void BlitWindow(CuiWindow* wnd);

	public:
		Cui(std::string title, size_t width, size_t height);
		virtual ~Cui();

		void AddWindow(CuiWindow* wnd);

		void SetWindowFocus(const std::string& name);

		// A global CUI key event handler (for example, to switch focus between windows). 
		// In addition, each active window also receives key event.

		virtual void OnKeyPress(char Ascii, CuiVkey Vkey, bool shift, bool ctrl);

		void ShowCursor(bool show);
		void SetCursor(int x, int y);

		void InvalidateAll();
	};
}
