// System-wide debugger

#include "pch.h"

namespace Debug
{
	GekkoDebug::GekkoDebug()
		: Cui ("Dolwin Debug Console", width, height)
	{

		RECT rect;

		rect.left = 0;
		rect.top = 0;
		rect.right = width;
		rect.bottom = height;

		ReportWindow* reportWnd = new ReportWindow(rect, "ReportWindow");

		AddWindow(reportWnd);
	}

	void GekkoDebug::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{

	}

}
