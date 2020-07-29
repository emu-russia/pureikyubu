// Disassembling code by Gekko virtual addresses. If the instruction is in Main mem, we disassemble and print, otherwise skip.

#include "pch.h"

namespace Debug
{

	GekkoDisasm::GekkoDisasm(RECT& rect, std::string name, Cui* parent)
		: CuiWindow (rect, name, parent)
	{

	}

	GekkoDisasm::~GekkoDisasm()
	{

	}

	void GekkoDisasm::OnDraw()
	{
		FillLine(CuiColor::Cyan, CuiColor::White, 0, '-');
		std::string head = IsActive() ? "[*] F3" : "[ ] F3";
		Print(CuiColor::Cyan, CuiColor::Normal, 1, 0, head);

		char hint[0x100] = { 0, };
		sprintf_s(hint, sizeof(hint), " cursor:0x%08X phys:0x%08X pc:0x%08X", 0, 0, 0);

		Print(CuiColor::Cyan, CuiColor::Normal, (int)(head.size() + 3), 0, hint);
	}

	void GekkoDisasm::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{

	}

}
