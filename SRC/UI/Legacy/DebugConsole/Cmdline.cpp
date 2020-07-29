// Command line. All commands go to JDI.
// It can be used both by the system debugger and as part of the DSP Debugger.

#include "pch.h"

namespace Debug
{

	CmdlineWindow::CmdlineWindow(RECT& rect, std::string name)
		: CuiWindow (rect, name)
	{

	}

	CmdlineWindow::~CmdlineWindow()
	{

	}

	void CmdlineWindow::OnDraw()
	{
		Fill(CuiColor::Black, CuiColor::Cyan, 'c');
	}

	void CmdlineWindow::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{

	}

}
