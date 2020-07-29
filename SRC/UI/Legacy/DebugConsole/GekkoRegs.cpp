// View Gekko registers. Register values are obtained through JDI.

#include "pch.h"

namespace Debug
{

	GekkoRegs::GekkoRegs(RECT& rect, std::string name, Cui* parent)
		: CuiWindow (rect, name, parent)
	{
	}

	GekkoRegs::~GekkoRegs()
	{
	}

	void GekkoRegs::OnDraw()
	{
		FillLine(CuiColor::Cyan, CuiColor::White, 0, '-');
		std::string head = IsActive() ? "[*] F1" : "[ ] F1";
		Print(CuiColor::Cyan, CuiColor::Normal, 1, 0, head);

		std::string modeText;

		switch (mode)
		{
			case GekkoRegmode::GPR: modeText = "GPR"; break;
			case GekkoRegmode::FPR: modeText = "FPR"; break;
			case GekkoRegmode::PSR: modeText = "PSR"; break;
			case GekkoRegmode::MMU: modeText = "MMU"; break;
		}

		Print(CuiColor::Cyan, CuiColor::Normal, (int)(head.size() + 3), 0, modeText);

		switch (mode)
		{
			case GekkoRegmode::GPR: ShowGprs(); break;
			case GekkoRegmode::FPR: ShowFprs(); break;
			case GekkoRegmode::PSR: ShowPairedSingle(); break;
			case GekkoRegmode::MMU: ShowMmu(); break;
		}
	}

	void GekkoRegs::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		switch (Vkey)
		{
			case VK_LEFT:
				RotateView(false);
				break;

			case VK_RIGHT:
				RotateView(true);
				break;
		}

		Invalidate();
	}

	void GekkoRegs::Memorize()
	{

	}

	void GekkoRegs::ShowGprs()
	{

	}

	void GekkoRegs::ShowFprs()
	{

	}

	void GekkoRegs::ShowPairedSingle()
	{

	}

	void GekkoRegs::ShowMmu()
	{

	}

	void GekkoRegs::RotateView(bool forward)
	{
		if (forward)
		{
			switch (mode)
			{
				case GekkoRegmode::GPR: mode = GekkoRegmode::FPR; break;
				case GekkoRegmode::FPR: mode = GekkoRegmode::PSR; break;
				case GekkoRegmode::PSR: mode = GekkoRegmode::MMU; break;
				case GekkoRegmode::MMU: mode = GekkoRegmode::GPR; break;
			}
		}
		else
		{
			switch (mode)
			{
				case GekkoRegmode::GPR: mode = GekkoRegmode::MMU; break;
				case GekkoRegmode::FPR: mode = GekkoRegmode::GPR; break;
				case GekkoRegmode::PSR: mode = GekkoRegmode::FPR; break;
				case GekkoRegmode::MMU: mode = GekkoRegmode::PSR; break;
			}
		}
	}

}
