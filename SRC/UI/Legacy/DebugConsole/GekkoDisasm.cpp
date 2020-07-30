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
		FillLine(CuiColor::Cyan, CuiColor::White, 0, ' ');
		std::string head = "[ ] F3";
		Print(CuiColor::Cyan, CuiColor::Black, 1, 0, head);
		if (IsActive())
		{
			Print(CuiColor::Cyan, CuiColor::White, 2, 0, "*");
		}

		char hint[0x100] = { 0, };
		sprintf_s(hint, sizeof(hint), " cursor:0x%08X phys:0x%08X pc:0x%08X", 0, 0, Jdi.GetPc());

		Print(CuiColor::Cyan, CuiColor::Black, (int)(head.size() + 3), 0, hint);
	}

	void GekkoDisasm::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{

	}

	uint32_t GekkoDisasm::GetCursor()
	{
		return cursor;
	}

	void GekkoDisasm::SetCursor(uint32_t address)
	{

	}

}
