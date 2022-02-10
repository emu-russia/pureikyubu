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
		FillLine(CuiColor::Cyan, CuiColor::White, 0, ' ');
		std::string head = "[ ] F2";
		Print(CuiColor::Cyan, CuiColor::Black, 1, 0, head);
		if (IsActive())
		{
			Print(CuiColor::Cyan, CuiColor::White, 2, 0, "*");
		}

		char hint[0x100] = { 0, };
		sprintf_s(hint, sizeof(hint), " phys:0x%08X stack:0x%08X sda1:0x%08X sda2:0x%08X", 
			Jdi->VirtualToPhysicalDMmu(cursor), Jdi->GetGpr(1), Jdi->GetGpr(13), Jdi->GetGpr(2));

		Print(CuiColor::Cyan, CuiColor::Black, (int)(head.size() + 3), 0, hint);

		// Hexview

		for (int row = 0; row < height - 1; row++)
		{
			Print(CuiColor::Normal, 0, row + 1, "%08X", cursor + row * 16);

			for (int col = 0; col < 8; col++)
			{
				Print(CuiColor::Normal, 10 + col * 3, row + 1, hexbyte(cursor + row * 16 + col));
			}

			for (int col = 0; col < 8; col++)
			{
				Print(CuiColor::Normal, 35 + col * 3, row + 1, hexbyte(cursor + row * 16 + col + 8));
			}

			for (int col = 0; col < 16; col++)
			{
				Print(CuiColor::Normal, 60 + col, row + 1, "%c", charbyte(cursor + row * 16 + col));
			}
		}
	}

	void MemoryView::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		switch (Vkey)
		{
			case VK_HOME:
				cursor = 0x8000'0000;
				break;
			case VK_END:
				cursor = (0x8000'0000 | RAMSIZE) - (uint32_t)((height - 1) * 16);
				break;
			case VK_NEXT:
				cursor += (uint32_t)((height - 1) * 16);
				break;
			case VK_PRIOR:
				cursor -= (uint32_t)((height - 1) * 16);
				break;
			case VK_UP:
				cursor -= 16;
				break;
			case VK_DOWN:
				cursor += 16;
				break;
		}

		Invalidate();
	}

	void MemoryView::SetCursor(uint32_t address)
	{
		cursor = address;
		Invalidate();
	}

	std::string MemoryView::hexbyte(uint32_t addr)
	{
		uint8_t* ptr = (uint8_t *)Jdi->TranslateDMmu(addr);

		if (ptr)
		{
			char buf[0x10];
			sprintf_s(buf, sizeof(buf), "%02X", *ptr);
			return buf;
		}
		else
		{
			return "??";
		}
	}

	char MemoryView::charbyte(uint32_t addr)
	{
		char* ptr = (char*)Jdi->TranslateDMmu(addr);

		if (ptr)
		{
			uint8_t data = *ptr;
			if ((data >= 32) && (data <= 255)) return (char)data;
			else return '.';
		}
		else
		{
			return '?';
		}
	}

}
