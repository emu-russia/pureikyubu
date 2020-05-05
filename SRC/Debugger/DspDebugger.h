// Visual DSP Debugger

#pragma once

#include "Cui.h"

namespace Debug
{
	class DspRegs : public CuiWindow
	{
	public:
		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

	class DspDebug : public Cui
	{
	public:
		DspDebug(std::string title, size_t width, size_t height) :
			Cui(title, width, height)
		{

		}

		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};
}
