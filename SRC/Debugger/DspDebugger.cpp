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

		// Registers with changes

		Print(CuiColor::Black, CuiColor::Normal, 0, 1, "ar0: %04X", Flipper::HW->DSP->regs.ar[0]);
		Print(CuiColor::Black, CuiColor::Normal, 0, 2, "ar1: %04X", Flipper::HW->DSP->regs.ar[1]);
		Print(CuiColor::Black, CuiColor::Normal, 0, 3, "ar2: %04X", Flipper::HW->DSP->regs.ar[2]);
		Print(CuiColor::Black, CuiColor::Normal, 0, 4, "ar3: %04X", Flipper::HW->DSP->regs.ar[3]);
		Print(CuiColor::Black, CuiColor::Normal, 0, 5, "ix0: %04X", Flipper::HW->DSP->regs.ix[0]);
		Print(CuiColor::Black, CuiColor::Normal, 0, 6, "ix1: %04X", Flipper::HW->DSP->regs.ix[1]);
		Print(CuiColor::Black, CuiColor::Normal, 0, 7, "ix2: %04X", Flipper::HW->DSP->regs.ix[2]);
		Print(CuiColor::Black, CuiColor::Normal, 0, 8, "ix3: %04X", Flipper::HW->DSP->regs.ix[3]);

		Print(CuiColor::Black, CuiColor::Normal, 12, 1, "lm0: %04X", Flipper::HW->DSP->regs.lm[0]);
		Print(CuiColor::Black, CuiColor::Normal, 12, 2, "lm1: %04X", Flipper::HW->DSP->regs.lm[1]);
		Print(CuiColor::Black, CuiColor::Normal, 12, 3, "lm2: %04X", Flipper::HW->DSP->regs.lm[2]);
		Print(CuiColor::Black, CuiColor::Normal, 12, 4, "lm3: %04X", Flipper::HW->DSP->regs.lm[3]);
		Print(CuiColor::Black, CuiColor::Normal, 12, 5, "st0: %04X", (uint16_t)Flipper::HW->DSP->regs.st[0].back());
		Print(CuiColor::Black, CuiColor::Normal, 12, 6, "st1: %04X", (uint16_t)Flipper::HW->DSP->regs.st[1].back());
		Print(CuiColor::Black, CuiColor::Normal, 12, 7, "st2: %04X", (uint16_t)Flipper::HW->DSP->regs.st[2].back());
		Print(CuiColor::Black, CuiColor::Normal, 12, 8, "st3: %04X", (uint16_t)Flipper::HW->DSP->regs.st[3].back());

		Print(CuiColor::Black, CuiColor::Normal, 24, 1, "a0h: %04X", Flipper::HW->DSP->regs.ac[0].h);
		Print(CuiColor::Black, CuiColor::Normal, 24, 2, "a1h: %04X", Flipper::HW->DSP->regs.ac[1].h);
		Print(CuiColor::Black, CuiColor::Normal, 24, 3, "br : %04X", Flipper::HW->DSP->regs.bank);
		Print(CuiColor::Black, CuiColor::Normal, 24, 4, "sr : %04X", Flipper::HW->DSP->regs.sr.bits);
		Print(CuiColor::Black, CuiColor::Normal, 24, 5, "pl : %04X", Flipper::HW->DSP->regs.prod.l);
		Print(CuiColor::Black, CuiColor::Normal, 24, 6, "pm1: %04X", Flipper::HW->DSP->regs.prod.m1);
		Print(CuiColor::Black, CuiColor::Normal, 24, 7, "ph : %04X", Flipper::HW->DSP->regs.prod.h);
		Print(CuiColor::Black, CuiColor::Normal, 24, 8, "pm2: %04X", Flipper::HW->DSP->regs.prod.m2);

		Print(CuiColor::Black, CuiColor::Normal, 36, 1, "x0l: %04X", Flipper::HW->DSP->regs.ax[0].l);
		Print(CuiColor::Black, CuiColor::Normal, 36, 2, "x0h: %04X", Flipper::HW->DSP->regs.ax[0].h);
		Print(CuiColor::Black, CuiColor::Normal, 36, 3, "x1l: %04X", Flipper::HW->DSP->regs.ax[1].l);
		Print(CuiColor::Black, CuiColor::Normal, 36, 4, "x1h: %04X", Flipper::HW->DSP->regs.ax[1].h);
		Print(CuiColor::Black, CuiColor::Normal, 36, 5, "a0l: %04X", Flipper::HW->DSP->regs.ac[0].l);
		Print(CuiColor::Black, CuiColor::Normal, 36, 6, "a1l: %04X", Flipper::HW->DSP->regs.ac[1].l);
		Print(CuiColor::Black, CuiColor::Normal, 36, 7, "a0m: %04X", Flipper::HW->DSP->regs.ac[0].m);
		Print(CuiColor::Black, CuiColor::Normal, 36, 8, "a1m: %04X", Flipper::HW->DSP->regs.ac[1].m);

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
		Print(CuiColor::Black, CuiColor::Normal, 48, 2, " p: %02X_%04X_%04X",
			(uint8_t)(prod.bitsPacked >> 32), 
			(uint16_t)(prod.bitsPacked >> 16), 
			(uint16_t)prod.bitsPacked);

		// Status as individual bits

		Print(CuiColor::Black, CuiColor::Normal, 66, 1, " C: %i", Flipper::HW->DSP->regs.sr.c);
		Print(CuiColor::Black, CuiColor::Normal, 66, 2, " O: %i", Flipper::HW->DSP->regs.sr.o);
		Print(CuiColor::Black, CuiColor::Normal, 66, 3, " Z: %i", Flipper::HW->DSP->regs.sr.z);
		Print(CuiColor::Black, CuiColor::Normal, 66, 4, " S: %i", Flipper::HW->DSP->regs.sr.s);
		Print(CuiColor::Black, CuiColor::Normal, 66, 5, "AS: %i", Flipper::HW->DSP->regs.sr.as);
		Print(CuiColor::Black, CuiColor::Normal, 66, 6, "TT: %i", Flipper::HW->DSP->regs.sr.tt);
		Print(CuiColor::Black, CuiColor::Normal, 66, 7, "OK: %i", Flipper::HW->DSP->regs.sr.ok);
		Print(CuiColor::Black, CuiColor::Normal, 66, 8, "OS: %i", Flipper::HW->DSP->regs.sr.os);

		Print(CuiColor::Black, CuiColor::Normal, 73, 1, "08: %i", Flipper::HW->DSP->regs.sr.hwz);
		Print(CuiColor::Black, CuiColor::Normal, 73, 2, "09: %i", Flipper::HW->DSP->regs.sr.ie);
		Print(CuiColor::Black, CuiColor::Normal, 73, 3, "10: %i", Flipper::HW->DSP->regs.sr.unk10);
		Print(CuiColor::Black, CuiColor::Normal, 73, 4, "11: %i", Flipper::HW->DSP->regs.sr.eie);
		Print(CuiColor::Black, CuiColor::Normal, 73, 5, "12: %i", Flipper::HW->DSP->regs.sr.unk12);
		Print(CuiColor::Black, CuiColor::Normal, 73, 6, "AM: %i", Flipper::HW->DSP->regs.sr.am);
		Print(CuiColor::Black, CuiColor::Normal, 73, 7, "XM: %i", Flipper::HW->DSP->regs.sr.sxm);
		Print(CuiColor::Black, CuiColor::Normal, 73, 8, "SU: %i", Flipper::HW->DSP->regs.sr.su);
	}

	void DspRegs::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
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

			Print(CuiColor::Black, info.flowControl ? CuiColor::Green : CuiColor::Normal, 0, y, text);
			
			addr += (DSP::DspAddress)(info.sizeInBytes / sizeof(uint16_t));
			y++;
		}
	}

	void DspImem::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		switch (Vkey)
		{
			case VK_UP:
				if (current > 0)
					current--;
				break;

			case VK_DOWN:
				current++;
				if (current >= 0x8A00)
					current = 0x8A00;
				break;

			case VK_PRIOR:
				if (current < (height - 1))
					current = 0;
				else
					current -= (DSP::DspAddress)(height - 1);
				break;

			case VK_NEXT:
				current += (DSP::DspAddress)(height - 1);
				if (current >= 0x8A00)
					current = 0x8A00;
				break;

			case VK_HOME:
				current = 0;
				break;

			case VK_END:
				current = DSP::DspCore::IROM_START_ADDRESS;
				break;
		}

		Invalidate();
	}

#pragma endregion "DspImem"

}
