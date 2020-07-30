// Visual DSP Debugger

#include "pch.h"

namespace Debug
{
	DspDebug::DspDebug() :
		Cui("DSP Debug", width, height)
	{
		ShowCursor(false);

		RECT rect;

		rect.left = 0;
		rect.top = 0;
		rect.right = 79;
		rect.bottom = 8;
		AddWindow(new DspRegs(rect, "DspRegs", this));

		rect.left = 0;
		rect.top = 9;
		rect.right = 79;
		rect.bottom = 17;
		AddWindow(new DspDmem(rect, "DspDmem", this));

		rect.left = 0;
		rect.top = 18;
		rect.right = 79;
		rect.bottom = 59;
		imemWindow = new DspImem(rect, "DspImem", this);
		AddWindow(imemWindow);

		SetWindowFocus("DspImem");
	}

	void DspDebug::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		uint32_t targetAddress = 0;

		switch (Vkey)
		{
			case VK_F1:
				SetWindowFocus("DspRegs");
				break;
			case VK_F2:
				SetWindowFocus("DspDmem");
				break;
			case VK_F3:
				SetWindowFocus("DspImem");
				break;

			case VK_F5:
				// Suspend/Run both cores
				if (Jdi.DspIsRunning())
				{
					Jdi.DspSuspend();
					Jdi.GekkoSuspend();
				}
				else
				{
					Jdi.DspRun();
					Jdi.GekkoRun();
				}
				break;

			case VK_F10:
				// Step Over
				if (!Jdi.DspIsRunning())
				{
					if (Jdi.DspIsCall(Jdi.DspGetPc(), targetAddress))
					{
						Jdi.DspAddOneShotBreakpoint(Jdi.DspGetPc() + 2);
						Jdi.DspRun();
					}
					else
					{
						Jdi.DspStep();
						if (!imemWindow->AddressVisible(Jdi.DspGetPc()))
						{
							imemWindow->current = imemWindow->cursor = Jdi.DspGetPc();
						}
					}
				}
				break;

			case VK_F11:
				// Step Into
				Jdi.DspStep();
				if (!imemWindow->AddressVisible(Jdi.DspGetPc()))
				{
					imemWindow->current = imemWindow->cursor = Jdi.DspGetPc();
				}
				break;
		}

		InvalidateAll();
	}

}
