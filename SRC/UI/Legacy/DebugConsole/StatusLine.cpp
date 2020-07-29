// Status window, to display the current state of the debugger

#include "pch.h"

namespace Debug
{
	StatusWindow::StatusWindow(RECT& rect, std::string name, Cui* parent)
		: CuiWindow (rect, name, parent)
	{
	}

	StatusWindow::~StatusWindow()
	{
	}

	void StatusWindow::OnDraw()
	{
		FillLine(CuiColor::Cyan, CuiColor::Black, 0, ' ');

		std::string text = "";

		switch (_mode)
		{
			case DebugMode::Ready:
				text = "Ready. Press PgUp to look behind.";
				break;
			case DebugMode::Scrolling:
				text = "Scroll Mode - Press PgUp, PgDown, Up, Down to scroll.";
				break;
		}

		Print(CuiColor::Cyan, CuiColor::Black, 2, 0, text);
	}

	void StatusWindow::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
	}

	void StatusWindow::SetMode(DebugMode mode)
	{
		_mode = mode;
		Invalidate();
	}

}
