// View Gekko registers. Register values are obtained through JDI.

#include "pch.h"

namespace Debug
{

	GekkoRegs::GekkoRegs(RECT& rect, std::string name)
		: CuiWindow (rect, name)
	{

	}

	GekkoRegs::~GekkoRegs()
	{

	}

	void GekkoRegs::OnDraw()
	{
		Fill(CuiColor::Black, CuiColor::Lime, 'r');
	}

	void GekkoRegs::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{

	}

}
