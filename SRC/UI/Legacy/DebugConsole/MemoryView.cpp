// View 1T-SRAM memory, by Gekko virtual addresses. If the virtual address is translated to the physical address of Main memory, then show bytes, otherwise show `?`

#include "pch.h"

namespace Debug
{

	MemoryView::MemoryView(RECT& rect, std::string name, Cui* parent)
		: CuiWindow (rect, name, parent)
	{

	}

	MemoryView::~MemoryView()
	{

	}

	void MemoryView::OnDraw()
	{
		FillLine(CuiColor::Cyan, CuiColor::White, 0, '-');
		std::string head = IsActive() ? "[*] F2" : "[ ] F2";
		Print(CuiColor::Cyan, CuiColor::Normal, 1, 0, head);

		char hint[0x100] = { 0, };
		sprintf_s(hint, sizeof(hint), " phys:0x%08X stack:0x%08X sda1:0x%08X sda2:0x%08X", 0, 0, 0, 0);

		Print(CuiColor::Cyan, CuiColor::Normal, (int)(head.size() + 3), 0, hint);
	}

	void MemoryView::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{

	}

}
