// Disassembling code by Gekko virtual addresses. If the instruction is in Main mem, we disassemble and print, otherwise skip.

#include "pch.h"

namespace Debug
{

	GekkoDisasm::GekkoDisasm(RECT& rect, std::string name, Cui* parent)
		: CuiWindow (rect, name, parent)
	{
		uint32_t main = Jdi.AddressByName("main");
		if (main)
		{
			SetCursor(main);
		}
		else
		{
			SetCursor(Jdi.GetPc());
		}
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
		sprintf_s(hint, sizeof(hint), " cursor:0x%08X phys:0x%08X pc:0x%08X", 
			cursor, Jdi.VirtualToPhysicalIMmu(cursor), Jdi.GetPc());

		Print(CuiColor::Cyan, CuiColor::Black, (int)(head.size() + 3), 0, hint);

		// Code

		uint32_t addr = address & ~3;
		disa_sub_h = 0;

		for (int line = 1; line < height; line++, addr += 4)
		{
			int n = DisasmLine(line, addr);
			if (n > 1) disa_sub_h += n - 1;
			line += n - 1;
		}
	}

	void GekkoDisasm::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		uint32_t targetAddress = 0;

		switch (Vkey)
		{
			case VK_HOME:
				SetCursor(cursor);
				break;

			case VK_END:
				break;

			case VK_UP:
				if (cursor < address)
				{
					cursor = address;
					break;
				}
				if (cursor >= (address + (uint32_t)(4 * height) - 4))
				{
					cursor = address + (uint32_t)(4 * height) - 8;
					break;
				}
				cursor -= 4;
				if (cursor < address)
				{
					address -= 4;
				}
				break;

			case VK_DOWN:
				if (cursor < address)
				{
					cursor = address;
					break;
				}
				if (cursor >= (address + 4 * (uint32_t)(height - disa_sub_h) - 4))
				{
					cursor = address + 4 * (uint32_t)(height - disa_sub_h) - 8;
					break;
				}
				cursor += 4;
				if (cursor >= (address + ((uint32_t)(height - disa_sub_h) - 1) * 4))
				{
					address += 4;
				}
				break;

			case VK_PRIOR:
				address -= (uint32_t)(4 * height - 4);
				if (!IsCursorVisible())
				{
					cursor = address;
				}
				break;

			case VK_NEXT:
				address += (uint32_t)(4 * (height - disa_sub_h) - 4);
				if (!IsCursorVisible())
				{
					cursor = address + ((uint32_t)(height - disa_sub_h) - 2) * 4;
				}
				break;

			case VK_RETURN:
				if (Jdi.GekkoIsBranch(cursor, targetAddress))
				{
					std::pair<uint32_t, uint32_t> last(address, cursor);
					browseHist.push_back(last);
					address = cursor = targetAddress;
				}
				break;

			case VK_ESCAPE:
				if (browseHist.size() > 0)
				{
					std::pair<uint32_t, uint32_t> last = browseHist.back();
					address = last.first;
					cursor = last.second;
					browseHist.pop_back();
				}
				break;
		}

		Invalidate();
	}

	uint32_t GekkoDisasm::GetCursor()
	{
		return cursor;
	}

	void GekkoDisasm::SetCursor(uint32_t addr)
	{
		cursor = addr & ~3;
		address = cursor - (uint32_t)(height - 1) / 2 * 4;
		Invalidate();
	}

	bool GekkoDisasm::IsCursorVisible()
	{
		uint32_t limit;
		limit = address + (uint32_t)((height - 1) * 4);
		return ((cursor < limit) && (cursor >= address));
	}

	int GekkoDisasm::DisasmLine(int line, uint32_t addr)
	{
		CuiColor bgpc, bgcur, bgbp;
		CuiColor bg;
		std::string symbol;
		int addend = 1;

		bgcur = (addr == cursor) ? (CuiColor::Gray) : (CuiColor::Black);
		bgbp = (Jdi.GekkoTestBreakpoint(addr)) ? (CuiColor::Red) : (CuiColor::Black);
		bgpc = (addr == Jdi.GetPc()) ? (CuiColor::DarkBlue) : (CuiColor::Black);
		bg = (CuiColor)((int)bgpc ^ (int)bgcur ^ (int)bgbp);

		FillLine(bg, CuiColor::Normal, line, ' ');

		// Symbolic information at address

		symbol = Jdi.NameByAddress(addr);
		if (!symbol.empty())
		{
			Print(bg, CuiColor::Green, 0, line, "%s", symbol.c_str());
			line++;
			addend++;

			FillLine(bg, CuiColor::Normal, line, ' ');
		}

		// Translate address

		uint32_t* ptr = (uint32_t*)Jdi.TranslateIMmu(addr);
		if (!ptr)
		{
			// No memory
			Print(bg, CuiColor::Normal, 0, line, "%08X  ", addr);
			Print(bg, CuiColor::Cyan, 10, line, "%08X  ", 0);
			Print(bg, CuiColor::Normal, 20, line, "???");
			return 1;
		}

		// Print address and opcode

		uint32_t opcode = _byteswap_ulong (*ptr);

		Print(bg, CuiColor::Normal, 0, line, "%08X  ", addr);
		Print(bg, CuiColor::Cyan, 10, line, "%08X  ", opcode);

		// Disasm

		std::string text = Jdi.GekkoDisasm(addr);

		bool flow = false;
		uint32_t targetAddress = 0;

		Print(bg, CuiColor::Normal, 20, line, "%s", text.c_str());

		// Branch hints

		flow = Jdi.GekkoIsBranch(addr, targetAddress);

		if (flow && targetAddress != 0)
		{
			const char* dir;

			if (targetAddress > addr) dir = " \x19";
			else if (targetAddress < addr) dir = " \x18";
			else dir = " \x1b";

			Print(bg, CuiColor::Cyan, 20 + (int)text.size(), line, "%s", dir);

			symbol = Jdi.NameByAddress(targetAddress);
			if (!symbol.empty())
			{
				Print(bg, CuiColor::Brown, 47, line, "; %s", symbol.c_str());
			}
		}

		// Rlwinm-like mask hint

		if (text.size() > 2)
		{
			if (text[0] == 'r' && text[1] == 'l')
			{
				int mb = ((opcode >> 6) & 0x1f);
				int me = ((opcode >> 1) & 0x1f);
				uint32_t mask = ((uint32_t)-1 >> mb) ^ ((me >= 31) ? 0 : ((uint32_t)-1) >> (me + 1));

				Print(bg, CuiColor::Normal, 60, line, "mask:0x%08X", mask);
			}
		}

		return addend;
	}

}
