// Universal code for interacting with the debug console.
//
// The console is Unicode: the key events carry the typed text as UTF-8 (the Win32 console reports
// the character the key stands for, SDL the text of the event), and the text of a window is a UTF-8
// string that is decoded one code point per cell of the back buffer (issue #372). The command line
// therefore keeps its buffer in UTF-8 and moves its cursor by code point, so a file name outside
// the ASCII range can be typed, edited and sent to JDI.

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

		void PutChar(CuiColor back, CuiColor front, int x, int y, wchar_t c);

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

		// Key event. Comes only if the window is active (SetFocus true).
		// `Text` is the UTF-8 of the character(s) the key produced, and is empty for a key that
		// produces none (an arrow, a function key, or a control combination). It is a string rather
		// than one wide character because that is what the input sides have: the Win32 console
		// reports one UTF-16 code unit (two of which can stand for a character outside the BMP),
		// and SDL reports the whole UTF-8 of the typed text.
		virtual void OnKeyPress(const char* Text, CuiVkey Vkey, bool shift, bool ctrl) = 0;

		void Invalidate() { invalidated = true; }
		bool NeedRedraw() { return invalidated; }

		void SetFocus(bool flag) { active = flag; Invalidate(); }
		bool IsActive() { return active; }

		// The text is UTF-8 (`Print` decodes it), the single characters of Fill are plain cells.
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

		int cursor_x = 0;
		int cursor_y = 0;
		bool cursor_visible = true;

		Thread* cuiThread = nullptr;
		static void CuiThreadProc(void* Parameter);

		void BlitWindow(CuiWindow* wnd);

		CHAR_INFO* frontBuf = nullptr;

	public:
		Cui(std::string title, size_t width, size_t height);
		virtual ~Cui();

		void AddWindow(CuiWindow* wnd);

		void SetWindowFocus(const std::string& name);

		// A global CUI key event handler (for example, to switch focus between windows). 
		// In addition, each active window also receives key event.

		virtual void OnKeyPress(const char* Text, CuiVkey Vkey, bool shift, bool ctrl);

		void ShowCursor(bool show);
		void SetCursor(int x, int y);

		void InvalidateAll();

		/// <summary>
		/// If CUI rendering is integrated into UI rendering, use this call to update the CUI.
		/// A typical example is imgui. You can't create two Renderers (even if it is Soft SDL2 backend. Why - it's not clear, but whatever).
		/// </summary>
		virtual void DrawInternal();
	};
}
