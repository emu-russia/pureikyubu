
#include "pch.h"

namespace Debug
{

	DspImem::DspImem(RECT& rect, std::string name, Cui* parent) :
		CuiWindow(rect, name, parent)
	{
	}

	void DspImem::OnDraw()
	{
		Fill(CuiColor::Black, CuiColor::Normal, ' ');
		FillLine(CuiColor::Cyan, CuiColor::Black, 0, ' ');
		Print(CuiColor::Cyan, CuiColor::Black, 1, 0, "F3 - IMEM");

		if (active)
		{
			Print(CuiColor::Cyan, CuiColor::White, 0, 0, "*");
		}

		// If GameCube is not powered on

		if (!Jdi->IsLoaded())
		{
			return;
		}

		// Show Dsp disassembly

		size_t lines = height - 1;
		uint32_t addr = current;
		int y = 1;

		// Do not forget that DSP addressing is done in 16-bit words.

		wordsOnScreen = 0;

		while (lines--)
		{
			size_t instrSizeInWords = 0;
			bool flowControl = false;

			std::string text = Jdi->DspDisasm(addr, instrSizeInWords, flowControl);

			if (text.empty())
			{
				addr++;
				y++;
				continue;
			}

			CuiColor backColor = CuiColor::Black;

			int bgcur = (addr == cursor) ? ((int)CuiColor::Blue) : (0);
			int bgbp = (Jdi->DspTestBreakpoint(addr)) ? ((int)CuiColor::Red) : (0);
			int bg = (addr == Jdi->DspGetPc()) ? ((int)CuiColor::DarkBlue) : (0);
			bg = bg ^ bgcur ^ bgbp;

			backColor = (CuiColor)bg;

			FillLine(backColor, CuiColor::Normal, y, ' ');
			Print(backColor, flowControl ? CuiColor::Green : CuiColor::Normal, 0, y, text);

			addr += (uint32_t)instrSizeInWords;
			wordsOnScreen += instrSizeInWords;
			y++;
		}
	}

	void DspImem::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		uint32_t targetAddress = 0;

		if (!Jdi->IsLoaded())
		{
			return;
		}

		switch (Vkey)
		{
			case VK_UP:
				if (AddressVisible(cursor))
				{
					if (cursor > 0)
						cursor--;
				}
				else
				{
					cursor = (uint32_t)(current - wordsOnScreen);
				}
				if (!AddressVisible(cursor))
				{
					if (current < (height - 1))
						current = 0;
					else
						current -= (uint32_t)(height - 1);
				}
				break;

			case VK_DOWN:
				if (AddressVisible(cursor))
					cursor++;
				else
					cursor = current;
				if (!AddressVisible(cursor))
				{
					current = cursor;
				}
				break;

			case VK_PRIOR:
				if (current < (height - 1))
					current = 0;
				else
					current -= (uint32_t)(height - 1);
				break;

			case VK_NEXT:
				current += (uint32_t)(wordsOnScreen ? wordsOnScreen : height - 1);
				if (current >= 0x8A00)
					current = 0x8A00;
				break;

			case VK_HOME:
				if (ctrl)
				{
					current = cursor = 0;
				}
				else
				{
					current = cursor = Jdi->DspGetPc();
				}
				break;

			case VK_END:
				current = IROM_START_ADDRESS;
				break;

			case VK_F9:
				if (AddressVisible(cursor))
				{
					Jdi->DspToggleBreakpoint(cursor);
				}
				break;

			case VK_RETURN:
				if (Jdi->DspIsCallOrJump(cursor, targetAddress))
				{
					std::pair<uint32_t, uint32_t> last(current, cursor);
					browseHist.push_back(last);
					current = cursor = targetAddress;
				}
				break;

			case VK_ESCAPE:
				if (browseHist.size() > 0)
				{
					std::pair<uint32_t, uint32_t> last = browseHist.back();
					current = last.first;
					cursor = last.second;
					browseHist.pop_back();
				}
				break;
		}

		Invalidate();
	}

	bool DspImem::AddressVisible(uint32_t address)
	{
		if (!wordsOnScreen)
			return false;

		return (current <= address && address < (current + wordsOnScreen));
	}

}
