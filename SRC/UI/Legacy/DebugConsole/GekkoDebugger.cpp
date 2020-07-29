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

		regs = new GekkoRegs(rect, "GekkoRegs", this);

		AddWindow(regs);

		// Flipper main memory hexview

		rect.left = 0;
		rect.top = regsHeight;
		rect.right = width;
		rect.bottom = regsHeight + memViewHeight - 1;

		memview = new MemoryView(rect, "MemoryView", this);

		AddWindow(memview);

		// Gekko disasm

		rect.left = 0;
		rect.top = regsHeight + memViewHeight;
		rect.right = width;
		rect.bottom = regsHeight + memViewHeight + disaHeight - 1;

		disasm = new GekkoDisasm(rect, "GekkoDisasm", this);

		AddWindow(disasm);

		// Message history

		rect.left = 0;
		rect.top = regsHeight + memViewHeight + disaHeight;
		rect.right = width;
		rect.bottom = height - 3;

		msgs = new ReportWindow(rect, "ReportWindow", this);

		AddWindow(msgs);

		// Command line

		rect.left = 0;
		rect.top = height - 2;
		rect.right = width;
		rect.bottom = height - 2;

		cmdline = new CmdlineWindow(rect, "Cmdline", this);

		AddWindow(cmdline);

		// Status

		rect.left = 0;
		rect.top = height - 1;
		rect.right = width;
		rect.bottom = height - 1;

		status = new StatusWindow(rect, "Status", this);

		AddWindow(status);

		SetWindowFocus("ReportWindow");
	}

	void GekkoDebug::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		if ((Vkey == 0x8 || (Ascii >= 0x20 && Ascii < 256)) && !cmdline->IsActive())
		{
			SetWindowFocus("Cmdline");
			InvalidateAll();
			return;
		}

		switch (Vkey)
		{
			case VK_F1:
				SetWindowFocus("GekkoRegs");
				InvalidateAll();
				break;

			case VK_F2:
				SetWindowFocus("MemoryView");
				InvalidateAll();
				break;

			case VK_F3:
				SetWindowFocus("GekkoDisasm");
				InvalidateAll();
				break;

			case VK_F4:
				SetWindowFocus("ReportWindow");
				InvalidateAll();
				break;

			case VK_F5:
				// Continue/break Gekko execution
				break;

			case VK_F9:
				// Toggle Breakpoint
				break;

			case VK_F10:
				// Step Over
				break;

			case VK_F11:
				// Step Into
				break;

			case VK_F12:
				// Skip instruction
				break;

			case VK_ESCAPE:
				if (msgs->IsActive())
				{
					SetWindowFocus("Cmdline");
					InvalidateAll();
				}
				break;

			case VK_PRIOR:
				if (cmdline->IsActive())
				{
					SetWindowFocus("ReportWindow");
					InvalidateAll();
				}
				break;
		}
	}

}
