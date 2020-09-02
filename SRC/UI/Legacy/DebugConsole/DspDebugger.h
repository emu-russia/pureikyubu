// Visual DSP Debugger

#pragma once

#include "DspRegs.h"
#include "DspDmem.h"
#include "DspImem.h"
#include "ReportView.h"
#include "Cmdline.h"

namespace Debug
{
	class DspDebug : public Cui
	{
		static const size_t width = 80;
		static const size_t height = 60;

		DspImem* imemWindow = nullptr;
		CmdlineWindow* cmdline = nullptr;

	public:
		DspDebug();

		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

}
