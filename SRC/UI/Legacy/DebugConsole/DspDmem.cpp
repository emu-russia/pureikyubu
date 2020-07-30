
#include "pch.h"

namespace Debug
{

	DspDmem::DspDmem(RECT& rect, std::string name, Cui* parent) :
		CuiWindow(rect, name, parent)
	{
	}

	void DspDmem::OnDraw()
	{
		Fill(CuiColor::Black, CuiColor::Normal, ' ');
		FillLine(CuiColor::Cyan, CuiColor::Black, 0, ' ');
		Print(CuiColor::Cyan, CuiColor::Black, 1, 0, "F2 - DMEM");

		if (active)
		{
			Print(CuiColor::Cyan, CuiColor::White, 0, 0, "*");
		}

		// If GameCube is not powered on

		if (!Jdi.IsLoaded())
		{
			return;
		}

		// Do not forget that DSP addressing is done in 16-bit words.

		size_t lines = height - 1;
		uint32_t addr = current;
		int y = 1;

		while (lines--)
		{
			char text[0x100];

			uint16_t* ptr = (uint16_t*) Jdi.DspTranslateDMem(addr);
			if (!ptr)
			{
				addr += 8;
				y++;
				continue;
			}

			// Address

			sprintf_s(text, sizeof(text) - 1, "%04X: ", addr);
			Print(CuiColor::Black, CuiColor::Normal, 0, y, text);

			// Raw Words

			int x = 6;

			for (size_t i = 0; i < 8; i++)
			{
				uint16_t word = _byteswap_ushort(ptr[i]);
				sprintf_s(text, sizeof(text) - 1, "%04X ", word);
				Print(CuiColor::Black, CuiColor::Normal, x, y, text);
				x += 5;
			}

			addr += 8;
			y++;
		}
	}

	void DspDmem::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		size_t lines = height - 1;

		if (!Jdi.IsLoaded())
		{
			return;
		}

		switch (Vkey)
		{
		case VK_UP:
			if (current >= 8)
				current -= 8;
			break;

		case VK_DOWN:
			current += 8;
			if (current > 0x3000)
				current = 0x3000;
			break;

		case VK_PRIOR:
			if (current < lines * 8)
				current = 0;
			else
				current -= (uint32_t)(lines * 8);
			break;

		case VK_NEXT:
			current += (uint32_t)(lines * 8);
			if (current > 0x3000)
				current = 0x3000;
			break;

		case VK_HOME:
			current = 0;
			break;

		case VK_END:
			current = DROM_START_ADDRESS;
			break;
		}

		Invalidate();
	}

}
