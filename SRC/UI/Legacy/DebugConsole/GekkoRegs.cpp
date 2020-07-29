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
		Fill(CuiColor::Black, CuiColor::Lime, 'r');

		FillLine(CuiColor::Cyan, CuiColor::White, 0, '-');
	}

	void GekkoRegs::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{

	}

}
