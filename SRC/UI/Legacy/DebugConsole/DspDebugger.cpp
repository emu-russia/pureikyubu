// Visual DSP Debugger

#include "pch.h"

namespace Debug
{
	DspDebug::DspDebug() :
		Cui("DSP Debug", width, height)
	{
		ShowCursor(false);

		RECT rect;

		rect.left = 0;
		rect.top = 0;
		rect.right = 79;
		rect.bottom = 8;
		AddWindow(new DspRegs(rect, "DspRegs", this));

		rect.left = 0;
		rect.top = 9;
		rect.right = 79;
		rect.bottom = 17;
		AddWindow(new DspDmem(rect, "DspDmem", this));

		rect.left = 0;
		rect.top = 18;
		rect.right = 79;
		rect.bottom = 59;
		imemWindow = new DspImem(rect, "DspImem", this);
		AddWindow(imemWindow);

		SetWindowFocus("DspImem");
	}

	void DspDebug::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		DSP::DspAddress targetAddress = 0;

		switch (Vkey)
		{
			case VK_F1:
				SetWindowFocus("DspRegs");
				break;
			case VK_F2:
				SetWindowFocus("DspDmem");
				break;
			case VK_F3:
				SetWindowFocus("DspImem");
				break;

			case VK_F5:
				// Suspend/Run both cores
				if (Flipper::HW->DSP->IsRunning())
				{
					Flipper::HW->DSP->Suspend();
					Gekko::Gekko->Suspend();
				}
				else
				{
					Flipper::HW->DSP->Run();
					Gekko::Gekko->Run();
				}
				break;

			case VK_F10:
				if (!Flipper::HW->DSP->IsRunning())
				{
					if (imemWindow->IsCall(Flipper::HW->DSP->regs.pc, targetAddress))
					{
						Flipper::HW->DSP->AddOneShotBreakpoint(Flipper::HW->DSP->regs.pc + 2);
						Flipper::HW->DSP->Run();
					}
					else
					{
						Flipper::HW->DSP->Step();
						if (!imemWindow->AddressVisible(Flipper::HW->DSP->regs.pc))
						{
							imemWindow->current = imemWindow->cursor = Flipper::HW->DSP->regs.pc;
						}
					}
				}
				break;

			case VK_F11:
				Flipper::HW->DSP->Step();
				if (!imemWindow->AddressVisible(Flipper::HW->DSP->regs.pc))
				{
					imemWindow->current = imemWindow->cursor = Flipper::HW->DSP->regs.pc;
				}
				break;
		}

		InvalidateAll();
	}

#pragma region "DspDmem"

	DspDmem::DspDmem(RECT& rect, std::string name) :
		CuiWindow(rect, name)
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

		if (!Debug::Hub.ExecuteFastBool("IsLoaded"))
		{
			return;
		}

		// Do not forget that DSP addressing is done in 16-bit words.

		size_t lines = height - 1;
		DSP::DspAddress addr = current;
		int y = 1;

		while (lines--)
		{
			char text[0x100];

			uint16_t* ptr = (uint16_t *)Flipper::HW->DSP->TranslateDMem(addr);
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

		if (!Debug::Hub.ExecuteFastBool("IsLoaded"))
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
					current -= (DSP::DspAddress)(lines * 8);
				break;

			case VK_NEXT:
				current += (DSP::DspAddress)(lines * 8);
				if (current > 0x3000)
					current = 0x3000;
				break;

			case VK_HOME:
				current = 0;
				break;

			case VK_END:
				current = DSP::DspCore::DROM_START_ADDRESS;
				break;
		}

		Invalidate();
	}

#pragma endregion "DspDmem"

#pragma region "DspImem"

	DspImem::DspImem(RECT& rect, std::string name) :
		CuiWindow(rect, name)
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

		if (!Debug::Hub.ExecuteFastBool("IsLoaded"))
		{
			return;
		}

		// Show Dsp disassembly

		size_t lines = height - 1;
		DSP::DspAddress addr = current;
		int y = 1;

		// Do not forget that DSP addressing is done in 16-bit words.

		wordsOnScreen = 0;

		while (lines--)
		{
			DSP::AnalyzeInfo info = { 0 };

			uint8_t * ptr = Flipper::HW->DSP->TranslateIMem(addr);
			if (!ptr)
			{
				addr++;
				y++;
				continue;
			}
			
			if (!DSP::Analyzer::Analyze(ptr, DSP::DspCore::MaxInstructionSizeInBytes, info))
				break;

			std::string text = DSP::DspDisasm::Disasm(addr, info);

			CuiColor backColor = CuiColor::Black;

			int bgcur = (addr == cursor) ? ((int)CuiColor::Blue) : (0);
			int bgbp = (Flipper::HW->DSP->TestBreakpoint(addr)) ? ((int)CuiColor::Red) : (0);
			int bg = (addr == Flipper::HW->DSP->regs.pc) ? ((int)CuiColor::DarkBlue) : (0);
			bg = bg ^ bgcur ^ bgbp;

			backColor = (CuiColor)bg;

			FillLine(backColor, CuiColor::Normal, y, ' ');
			Print(backColor, info.flowControl ? CuiColor::Green : CuiColor::Normal, 0, y, text);
			
			addr += (DSP::DspAddress)(info.sizeInBytes / sizeof(uint16_t));
			wordsOnScreen += info.sizeInBytes / sizeof(uint16_t);
			y++;
		}
	}

	void DspImem::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		DSP::DspAddress targetAddress = 0;

		if (!Debug::Hub.ExecuteFastBool("IsLoaded"))
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
					cursor = (DSP::DspAddress)(current - wordsOnScreen);
				}
				if (!AddressVisible(cursor))
				{
					if (current < (height - 1))
						current = 0;
					else
						current -= (DSP::DspAddress)(height - 1);
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
					current -= (DSP::DspAddress)(height - 1);
				break;

			case VK_NEXT:
				current += (DSP::DspAddress)(wordsOnScreen ? wordsOnScreen : height - 1);
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
					current = cursor = Flipper::HW->DSP->regs.pc;
				}
				break;

			case VK_END:
				current = DSP::DspCore::IROM_START_ADDRESS;
				break;
			
			case VK_F9:
				if (AddressVisible(cursor))
				{
					Flipper::HW->DSP->ToggleBreakpoint(cursor);
				}
				break;

			case VK_RETURN:
				if (IsCallOrJump(cursor, targetAddress))
				{
					std::pair<DSP::DspAddress, DSP::DspAddress> last(current, cursor);
					browseHist.push_back(last);
					current = cursor = targetAddress;
				}
				break;

			case VK_ESCAPE:
				if (browseHist.size() > 0)
				{
					std::pair<DSP::DspAddress, DSP::DspAddress> last = browseHist.back();
					current = last.first;
					cursor = last.second;
					browseHist.pop_back();
				}
				break;
		}

		Invalidate();
	}

	bool DspImem::AddressVisible(DSP::DspAddress address)
	{
		if (!wordsOnScreen)
			return false;

		return (current <= address && address < (current + wordsOnScreen));
	}

	bool DspImem::IsCall(DSP::DspAddress address, DSP::DspAddress& targetAddress)
	{
		DSP::AnalyzeInfo info = { 0 };

		targetAddress = 0;

		uint8_t* ptr = Flipper::HW->DSP->TranslateIMem(address);
		if (!ptr)
		{
			return false;
		}

		if (!DSP::Analyzer::Analyze(ptr, DSP::DspCore::MaxInstructionSizeInBytes, info))
			return false;

		if (info.flowControl)
		{
			if (info.instr == DSP::DspInstruction::CALLcc || info.instr == DSP::DspInstruction::CALLR)
			{
				targetAddress = info.ImmOperand.Address;
				return true;
			}
		}

		return false;
	}

	bool DspImem::IsCallOrJump(DSP::DspAddress address, DSP::DspAddress& targetAddress)
	{
		DSP::AnalyzeInfo info = { 0 };

		targetAddress = 0;

		uint8_t* ptr = Flipper::HW->DSP->TranslateIMem(address);
		if (!ptr)
		{
			return false;
		}

		if (!DSP::Analyzer::Analyze(ptr, DSP::DspCore::MaxInstructionSizeInBytes, info))
			return false;

		if (info.flowControl)
		{
			if (info.instr == DSP::DspInstruction::Jcc ||
				info.instr == DSP::DspInstruction::CALLcc)
			{
				targetAddress = info.ImmOperand.Address;
				return true;
			}
		}

		return false;
	}

#pragma endregion "DspImem"

}
