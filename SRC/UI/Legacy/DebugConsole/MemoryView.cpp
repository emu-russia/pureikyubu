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
		Fill(CuiColor::Black, CuiColor::Cyan, 'm');

		FillLine(CuiColor::Cyan, CuiColor::White, 0, '-');
	}

	void MemoryView::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{

	}

}
