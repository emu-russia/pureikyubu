// System-wide debugger

#include "pch.h"

namespace Debug
{
	GekkoDebug::GekkoDebug()
		: Cui ("Dolwin Debug Console", width, height)
	{
		RECT rect;

		// Gekko registers

		rect.left = 0;
		rect.top = 0;
		rect.right = width;
		rect.bottom = regsHeight - 1;

		regs = new GekkoRegs(rect, "GekkoRegs");

		AddWindow(regs);

		// Flipper main memory hexview

		rect.left = 0;
		rect.top = regsHeight;
		rect.right = width;
		rect.bottom = regsHeight + memViewHeight - 1;

		memview = new MemoryView(rect, "MemoryView");

		AddWindow(memview);

		// Gekko disasm

		rect.left = 0;
		rect.top = regsHeight + memViewHeight;
		rect.right = width;
		rect.bottom = regsHeight + memViewHeight + disaHeight - 1;

		disasm = new GekkoDisasm(rect, "GekkoDisasm");

		AddWindow(disasm);

		// Message history

		rect.left = 0;
		rect.top = regsHeight + memViewHeight + disaHeight;
		rect.right = width;
		rect.bottom = height - 3;

		msgs = new ReportWindow(rect, "ReportWindow");

		AddWindow(msgs);

		// Command line

		rect.left = 0;
		rect.top = height - 2;
		rect.right = width;
		rect.bottom = height - 2;

		cmdline = new CmdlineWindow(rect, "Cmdline");

		AddWindow(cmdline);

		// Status

		rect.left = 0;
		rect.top = height - 1;
		rect.right = width;
		rect.bottom = height - 1;

		status = new StatusWindow(rect, "Status");

		AddWindow(status);

		SetWindowFocus("ReportWindow");
	}

	void GekkoDebug::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{

	}

}
