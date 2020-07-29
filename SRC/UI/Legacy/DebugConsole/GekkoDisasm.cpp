// Disassembling code by Gekko virtual addresses. If the instruction is in Main mem, we disassemble and print, otherwise skip.

#include "pch.h"

namespace Debug
{

	GekkoDisasm::GekkoDisasm(RECT& rect, std::string name)
		: CuiWindow (rect, name)
	{

	}

	GekkoDisasm::~GekkoDisasm()
	{

	}

	void GekkoDisasm::OnDraw()
	{
		Fill(CuiColor::Black, CuiColor::Lime, 'd');
	}

	void GekkoDisasm::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{

	}

}
