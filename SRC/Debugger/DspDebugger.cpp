// Visual DSP Debugger

#include "pch.h"

namespace Debug
{
	DspDebug::DspDebug(std::string title, size_t width, size_t height) :
		Cui(title, width, height)
	{
		ShowCursor(false);

		RECT rect;
		rect.left = 10;
		rect.top = 10;
		rect.right = 20;
		rect.bottom = 20;
		AddWindow(new DspRegs(rect, "DspRegs"));

		SetWindowFocus("DspRegs");

	}

	void DspDebug::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
	}

	DspRegs::DspRegs(RECT& rect, std::string name) :
		CuiWindow(rect, name)
	{
	}

	void DspRegs::OnDraw()
	{
		Print(CuiColor::Black, CuiColor::Lime, 3, 3, "xxx");
	}

	void DspRegs::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		std::string keyStr;

		keyStr.push_back(Ascii);
		keyStr += " " + std::to_string(Vkey);

		Fill(CuiColor::DarkBlue, CuiColor::Normal, ' ');
		Print(CuiColor::Black, CuiColor::Lime, 3, 5, keyStr);;

		Invalidate();
	}

}
