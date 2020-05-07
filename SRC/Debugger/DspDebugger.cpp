// Visual DSP Debugger

#include "pch.h"
#include "../DSP/DspAnalyzer.h"
#include "../DSP/DspDisasm.h"

namespace Debug
{
	DspDebug::DspDebug(std::string title, size_t width, size_t height) :
		Cui(title, width, height)
	{
		ShowCursor(false);

		RECT rect;

		rect.left = 0;
		rect.top = 0;
		rect.right = 79;
		rect.bottom = 8;
		AddWindow(new DspRegs(rect, "DspRegs"));

		rect.left = 0;
		rect.top = 9;
		rect.right = 79;
		rect.bottom = 17;
		AddWindow(new DspDmem(rect, "DspDmem"));

		rect.left = 0;
		rect.top = 18;
		rect.right = 79;
		rect.bottom = 59;
		AddWindow(new DspImem(rect, "DspImem"));

		SetWindowFocus("DspImem");
	}

	void DspDebug::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
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
		}

		InvalidateAll();
	}

#pragma region "DspRegs"

	DspRegs::DspRegs(RECT& rect, std::string name) :
		CuiWindow(rect, name)
	{
	}

	void DspRegs::OnDraw()
	{
		FillLine(CuiColor::Cyan, CuiColor::Black, 0, ' ');
		Print(CuiColor::Cyan, CuiColor::Black, 1, 0, "F1 - Regs");
		
		if (active)
		{
			Print(CuiColor::Cyan, CuiColor::White, 0, 0, "*");
		}

		// If GameCube is not powered on

		if (!Debug::Hub.ExecuteFastBool("IsLoaded"))
		{
			return;
		}

		// DSP Run State

		if (Flipper::HW->DSP->IsRunning())
		{
			Print(CuiColor::Cyan, CuiColor::Lime, 73, 0, "Running");
		}
		else
		{
			Print(CuiColor::Cyan, CuiColor::Red, 70, 0, "Suspended");
		}

		// Registers with changes

		DrawRegs();

		// 40-bit regs overview

		Print(CuiColor::Black, CuiColor::Normal, 48, 1, "a0: %02X_%04X_%04X",
			(uint8_t)Flipper::HW->DSP->regs.ac[0].h,
			Flipper::HW->DSP->regs.ac[0].m,
			Flipper::HW->DSP->regs.ac[0].l);
		Print(CuiColor::Black, CuiColor::Normal, 48, 2, "a1: %02X_%04X_%04X",
			(uint8_t)Flipper::HW->DSP->regs.ac[1].h,
			Flipper::HW->DSP->regs.ac[1].m,
			Flipper::HW->DSP->regs.ac[1].l);

		DSP::DspProduct prod = Flipper::HW->DSP->regs.prod;
		DSP::DspCore::PackProd(prod);
		Print(CuiColor::Black, CuiColor::Normal, 48, 3, " p: %02X_%04X_%04X",
			(uint8_t)(prod.bitsPacked >> 32), 
			(uint16_t)(prod.bitsPacked >> 16), 
			(uint16_t)prod.bitsPacked);

		// Program Counter

		Print(CuiColor::Black, CuiColor::Normal, 48, 5, "pc: %04X", Flipper::HW->DSP->regs.pc);

		// Status as individual bits

		DrawStatusBits();

		savedRegs = Flipper::HW->DSP->regs;
	}

	void DspRegs::DrawRegs()
	{
		Print(CuiColor::Black, 
			savedRegs.ar[0] != Flipper::HW->DSP->regs.ar[0] ? CuiColor::Lime : CuiColor::Normal,
			0, 1, "ar0: %04X", Flipper::HW->DSP->regs.ar[0]);
		Print(CuiColor::Black, 
			savedRegs.ar[1] != Flipper::HW->DSP->regs.ar[1] ? CuiColor::Lime : CuiColor::Normal,
			0, 2, "ar1: %04X", Flipper::HW->DSP->regs.ar[1]);
		Print(CuiColor::Black, 
			savedRegs.ar[2] != Flipper::HW->DSP->regs.ar[2] ? CuiColor::Lime : CuiColor::Normal,
			0, 3, "ar2: %04X", Flipper::HW->DSP->regs.ar[2]);
		Print(CuiColor::Black, 
			savedRegs.ar[3] != Flipper::HW->DSP->regs.ar[3] ? CuiColor::Lime : CuiColor::Normal,
			0, 4, "ar3: %04X", Flipper::HW->DSP->regs.ar[3]);
		Print(CuiColor::Black, 
			savedRegs.ix[0] != Flipper::HW->DSP->regs.ix[0] ? CuiColor::Lime : CuiColor::Normal,
			0, 5, "ix0: %04X", Flipper::HW->DSP->regs.ix[0]);
		Print(CuiColor::Black, 
			savedRegs.ix[1] != Flipper::HW->DSP->regs.ix[1] ? CuiColor::Lime : CuiColor::Normal,
			0, 6, "ix1: %04X", Flipper::HW->DSP->regs.ix[1]);
		Print(CuiColor::Black, 
			savedRegs.ix[2] != Flipper::HW->DSP->regs.ix[2] ? CuiColor::Lime : CuiColor::Normal,
			0, 7, "ix2: %04X", Flipper::HW->DSP->regs.ix[2]);
		Print(CuiColor::Black, 
			savedRegs.ix[3] != Flipper::HW->DSP->regs.ix[3] ? CuiColor::Lime : CuiColor::Normal,
			0, 8, "ix3: %04X", Flipper::HW->DSP->regs.ix[3]);

		Print(CuiColor::Black, 
			savedRegs.lm[0] != Flipper::HW->DSP->regs.lm[0] ? CuiColor::Lime : CuiColor::Normal,
			12, 1, "lm0: %04X", Flipper::HW->DSP->regs.lm[0]);
		Print(CuiColor::Black, 
			savedRegs.lm[1] != Flipper::HW->DSP->regs.lm[1] ? CuiColor::Lime : CuiColor::Normal,
			12, 2, "lm1: %04X", Flipper::HW->DSP->regs.lm[1]);
		Print(CuiColor::Black, 
			savedRegs.lm[2] != Flipper::HW->DSP->regs.lm[2] ? CuiColor::Lime : CuiColor::Normal,
			12, 3, "lm2: %04X", Flipper::HW->DSP->regs.lm[2]);
		Print(CuiColor::Black, 
			savedRegs.lm[3] != Flipper::HW->DSP->regs.lm[3] ? CuiColor::Lime : CuiColor::Normal,
			12, 4, "lm3: %04X", Flipper::HW->DSP->regs.lm[3]);
		Print(CuiColor::Black, 
			CuiColor::Normal,
			12, 5, "st0: %04X", Flipper::HW->DSP->regs.st[0].size() != 0 ? (uint16_t)Flipper::HW->DSP->regs.st[0].back() : 0);
		Print(CuiColor::Black, 
			CuiColor::Normal,
			12, 6, "st1: %04X", Flipper::HW->DSP->regs.st[1].size() != 0 ? (uint16_t)Flipper::HW->DSP->regs.st[1].back() : 0);
		Print(CuiColor::Black, 
			CuiColor::Normal,
			12, 7, "st2: %04X", Flipper::HW->DSP->regs.st[2].size() != 0 ? (uint16_t)Flipper::HW->DSP->regs.st[2].back() : 0);
		Print(CuiColor::Black, 
			CuiColor::Normal,
			12, 8, "st3: %04X", Flipper::HW->DSP->regs.st[3].size() != 0 ? (uint16_t)Flipper::HW->DSP->regs.st[3].back() : 0);

		Print(CuiColor::Black, 
			savedRegs.ac[0].h != Flipper::HW->DSP->regs.ac[0].h ? CuiColor::Lime : CuiColor::Normal,
			24, 1, "a0h: %04X", Flipper::HW->DSP->regs.ac[0].h);
		Print(CuiColor::Black, 
			savedRegs.ac[1].h != Flipper::HW->DSP->regs.ac[1].h ? CuiColor::Lime : CuiColor::Normal,
			24, 2, "a1h: %04X", Flipper::HW->DSP->regs.ac[1].h);
		Print(CuiColor::Black, 
			savedRegs.bank != Flipper::HW->DSP->regs.bank ? CuiColor::Lime : CuiColor::Normal,
			24, 3, "br : %04X", Flipper::HW->DSP->regs.bank);
		Print(CuiColor::Black, 
			savedRegs.sr.bits != Flipper::HW->DSP->regs.sr.bits ? CuiColor::Lime : CuiColor::Normal,
			24, 4, "sr : %04X", Flipper::HW->DSP->regs.sr.bits);
		Print(CuiColor::Black, 
			savedRegs.prod.l != Flipper::HW->DSP->regs.prod.l ? CuiColor::Lime : CuiColor::Normal,
			24, 5, "pl : %04X", Flipper::HW->DSP->regs.prod.l);
		Print(CuiColor::Black, 
			savedRegs.prod.m1 != Flipper::HW->DSP->regs.prod.m1 ? CuiColor::Lime : CuiColor::Normal,
			24, 6, "pm1: %04X", Flipper::HW->DSP->regs.prod.m1);
		Print(CuiColor::Black, 
			savedRegs.prod.h != Flipper::HW->DSP->regs.prod.h ? CuiColor::Lime : CuiColor::Normal,
			24, 7, "ph : %04X", Flipper::HW->DSP->regs.prod.h);
		Print(CuiColor::Black, 
			savedRegs.prod.m2 != Flipper::HW->DSP->regs.prod.m2 ? CuiColor::Lime : CuiColor::Normal,
			24, 8, "pm2: %04X", Flipper::HW->DSP->regs.prod.m2);

		Print(CuiColor::Black, 
			savedRegs.ax[0].l != Flipper::HW->DSP->regs.ax[0].l ? CuiColor::Lime : CuiColor::Normal,
			36, 1, "x0l: %04X", Flipper::HW->DSP->regs.ax[0].l);
		Print(CuiColor::Black, 
			savedRegs.ax[0].h != Flipper::HW->DSP->regs.ax[0].h ? CuiColor::Lime : CuiColor::Normal,
			36, 2, "x0h: %04X", Flipper::HW->DSP->regs.ax[0].h);
		Print(CuiColor::Black, 
			savedRegs.ax[1].l != Flipper::HW->DSP->regs.ax[1].l ? CuiColor::Lime : CuiColor::Normal,
			36, 3, "x1l: %04X", Flipper::HW->DSP->regs.ax[1].l);
		Print(CuiColor::Black, 
			savedRegs.ax[1].h != Flipper::HW->DSP->regs.ax[1].h ? CuiColor::Lime : CuiColor::Normal,
			36, 4, "x1h: %04X", Flipper::HW->DSP->regs.ax[1].h);
		Print(CuiColor::Black, 
			savedRegs.ac[0].l != Flipper::HW->DSP->regs.ac[0].l ? CuiColor::Lime : CuiColor::Normal,
			36, 5, "a0l: %04X", Flipper::HW->DSP->regs.ac[0].l);
		Print(CuiColor::Black, 
			savedRegs.ac[1].l != Flipper::HW->DSP->regs.ac[1].l ? CuiColor::Lime : CuiColor::Normal,
			36, 6, "a1l: %04X", Flipper::HW->DSP->regs.ac[1].l);
		Print(CuiColor::Black, 
			savedRegs.ac[0].m != Flipper::HW->DSP->regs.ac[0].m ? CuiColor::Lime : CuiColor::Normal,
			36, 7, "a0m: %04X", Flipper::HW->DSP->regs.ac[0].m);
		Print(CuiColor::Black, 
			savedRegs.ac[1].m != Flipper::HW->DSP->regs.ac[1].m ? CuiColor::Lime : CuiColor::Normal,
			36, 8, "a1m: %04X", Flipper::HW->DSP->regs.ac[1].m);
	}

	void DspRegs::DrawStatusBits()
	{
		Print(CuiColor::Black,
			savedRegs.sr.c != Flipper::HW->DSP->regs.sr.c ? CuiColor::Lime : CuiColor::Normal,
			66, 1, " C: %i", Flipper::HW->DSP->regs.sr.c);
		Print(CuiColor::Black,
			savedRegs.sr.o != Flipper::HW->DSP->regs.sr.o ? CuiColor::Lime : CuiColor::Normal,
			66, 2, " O: %i", Flipper::HW->DSP->regs.sr.o);
		Print(CuiColor::Black,
			savedRegs.sr.z != Flipper::HW->DSP->regs.sr.z ? CuiColor::Lime : CuiColor::Normal,
			66, 3, " Z: %i", Flipper::HW->DSP->regs.sr.z);
		Print(CuiColor::Black,
			savedRegs.sr.s != Flipper::HW->DSP->regs.sr.s ? CuiColor::Lime : CuiColor::Normal,
			66, 4, " S: %i", Flipper::HW->DSP->regs.sr.s);
		Print(CuiColor::Black,
			savedRegs.sr.as != Flipper::HW->DSP->regs.sr.as ? CuiColor::Lime : CuiColor::Normal,
			66, 5, "AS: %i", Flipper::HW->DSP->regs.sr.as);
		Print(CuiColor::Black,
			savedRegs.sr.tt != Flipper::HW->DSP->regs.sr.tt ? CuiColor::Lime : CuiColor::Normal,
			66, 6, "TT: %i", Flipper::HW->DSP->regs.sr.tt);
		Print(CuiColor::Black,
			savedRegs.sr.ok != Flipper::HW->DSP->regs.sr.ok ? CuiColor::Lime : CuiColor::Normal,
			66, 7, "OK: %i", Flipper::HW->DSP->regs.sr.ok);
		Print(CuiColor::Black,
			savedRegs.sr.os != Flipper::HW->DSP->regs.sr.os ? CuiColor::Lime : CuiColor::Normal,
			66, 8, "OS: %i", Flipper::HW->DSP->regs.sr.os);

		Print(CuiColor::Black,
			savedRegs.sr.hwz != Flipper::HW->DSP->regs.sr.hwz ? CuiColor::Lime : CuiColor::Normal,
			73, 1, "08: %i", Flipper::HW->DSP->regs.sr.hwz);
		Print(CuiColor::Black,
			savedRegs.sr.ie != Flipper::HW->DSP->regs.sr.ie ? CuiColor::Lime : CuiColor::Normal,
			73, 2, "09: %i", Flipper::HW->DSP->regs.sr.ie);
		Print(CuiColor::Black,
			savedRegs.sr.unk10 != Flipper::HW->DSP->regs.sr.unk10 ? CuiColor::Lime : CuiColor::Normal,
			73, 3, "10: %i", Flipper::HW->DSP->regs.sr.unk10);
		Print(CuiColor::Black,
			savedRegs.sr.eie != Flipper::HW->DSP->regs.sr.eie ? CuiColor::Lime : CuiColor::Normal,
			73, 4, "11: %i", Flipper::HW->DSP->regs.sr.eie);
		Print(CuiColor::Black,
			savedRegs.sr.unk12 != Flipper::HW->DSP->regs.sr.unk12 ? CuiColor::Lime : CuiColor::Normal,
			73, 5, "12: %i", Flipper::HW->DSP->regs.sr.unk12);
		Print(CuiColor::Black,
			savedRegs.sr.am != Flipper::HW->DSP->regs.sr.am ? CuiColor::Lime : CuiColor::Normal,
			73, 6, "AM: %i", Flipper::HW->DSP->regs.sr.am);
		Print(CuiColor::Black,
			savedRegs.sr.sxm != Flipper::HW->DSP->regs.sr.sxm ? CuiColor::Lime : CuiColor::Normal,
			73, 7, "XM: %i", Flipper::HW->DSP->regs.sr.sxm);
		Print(CuiColor::Black,
			savedRegs.sr.su != Flipper::HW->DSP->regs.sr.su ? CuiColor::Lime : CuiColor::Normal,
			73, 8, "SU: %i", Flipper::HW->DSP->regs.sr.su);
	}

	void DspRegs::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		if (!Debug::Hub.ExecuteFastBool("IsLoaded"))
		{
			return;
		}

		Invalidate();
	}

#pragma endregion "DspRegs"

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
				current = cursor = Flipper::HW->DSP->regs.pc;
				break;

			case VK_END:
				current = DSP::DspCore::IROM_START_ADDRESS;
				break;

			case VK_F5:
				if (Flipper::HW->DSP->IsRunning())
				{
					Flipper::HW->DSP->Suspend();
				}
				else
				{
					Flipper::HW->DSP->Run();
				}
				break;
			
			case VK_F9:
				if (AddressVisible(cursor))
				{
					Flipper::HW->DSP->ToggleBreakpoint(cursor);
				}
				break;

			case VK_F10:
				if (!Flipper::HW->DSP->IsRunning())
				{
					if (IsCall(Flipper::HW->DSP->regs.pc, targetAddress))
					{
						Flipper::HW->DSP->AddOneShotBreakpoint(Flipper::HW->DSP->regs.pc + 2);
						Flipper::HW->DSP->Run();
					}
					else
					{
						Flipper::HW->DSP->Step();
						if (!AddressVisible(Flipper::HW->DSP->regs.pc))
						{
							current = cursor = Flipper::HW->DSP->regs.pc;
						}
					}
				}
				break;

			case VK_F11:
				Flipper::HW->DSP->Step();
				if (!AddressVisible(Flipper::HW->DSP->regs.pc))
				{
					current = cursor = Flipper::HW->DSP->regs.pc;
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
