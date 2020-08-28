// System-wide debugger

#include "pch.h"

namespace Debug
{
	GekkoDebug::GekkoDebug()
		: Cui ("Dolwin Debug Console", width, height)
	{
		RECT rect;

		// Create an interface for communicating with the emulator core, if it has not been created yet.

		if (!Jdi)
		{
			Jdi = new JdiClient;
		}

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

		SetWindowFocus("Cmdline");
	}

	void GekkoDebug::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		if ((Vkey == 0x8 || (Ascii >= 0x20 && Ascii < 256)) && !cmdline->IsActive())
		{
			SetWindowFocus("Cmdline");
			status->SetMode(DebugMode::Ready);
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
				status->SetMode(DebugMode::Scrolling);
				InvalidateAll();
				break;

			case VK_F5:
				// Continue/break Gekko execution
				if (Jdi->IsLoaded())
				{
					if (Jdi->IsRunning())
					{
						Jdi->GekkoSuspend();
						disasm->SetCursor(Jdi->GetPc());
					}
					else
					{
						Jdi->GekkoRun();
					}
					InvalidateAll();
				}
				break;

			case VK_F9:
				// Toggle Breakpoint
				Jdi->GekkoToggleBreakpoint(disasm->GetCursor());
				disasm->Invalidate();
				break;

			case VK_F10:
				// Step Over
				if (Jdi->IsLoaded() && !Jdi->IsRunning())
				{
					Jdi->GekkoAddOneShotBreakpoint(Jdi->GetPc() + 4);
					Jdi->GekkoRun();
					InvalidateAll();
				}
				break;

			case VK_F11:
				// Step Into
				if (Jdi->IsLoaded() && !Jdi->IsRunning())
				{
					Jdi->GekkoStep();
					disasm->SetCursor(Jdi->GetPc());
					InvalidateAll();
				}
				break;

			case VK_F12:
				// Skip instruction
				if (Jdi->IsLoaded() && !Jdi->IsRunning())
				{
					Jdi->GekkoSkipInstruction();
					InvalidateAll();
				}
				break;

			case VK_ESCAPE:
				if (msgs->IsActive())
				{
					SetWindowFocus("Cmdline");
					status->SetMode(DebugMode::Ready);
					InvalidateAll();
				}
				break;

			case VK_PRIOR:
				if (cmdline->IsActive())
				{
					SetWindowFocus("ReportWindow");
					status->SetMode(DebugMode::Scrolling);
					InvalidateAll();
				}
				break;
		}
	}

	/// <summary>
	/// Set current address to view memory. Used by "d" command.
	/// </summary>
	/// <param name="virtualAddress"></param>
	void GekkoDebug::SetMemoryCursor(uint32_t virtualAddress)
	{
		memview->SetCursor(virtualAddress);
	}

	/// <summary>
	/// Set the current address to view the disassembled Gekko code. Used by "u" command.
	/// </summary>
	/// <param name="virtualAddress"></param>
	void GekkoDebug::SetDisasmCursor(uint32_t virtualAddress)
	{
		disasm->SetCursor(virtualAddress);
	}

}
