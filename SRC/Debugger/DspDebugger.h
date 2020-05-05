// Visual DSP Debugger

#pragma once

#include "Cui.h"

namespace Debug
{
	class DspDebug : public Cui
	{
	public:
		DspDebug(std::string title, size_t width, size_t height);

		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

	class DspRegs : public CuiWindow
	{
	public:
		DspRegs(RECT& rect, std::string name);

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};
}
