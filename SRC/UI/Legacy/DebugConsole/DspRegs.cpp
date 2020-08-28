
#include "pch.h"

namespace Debug
{
	static const char* RegNames[] = {
		"r0", "r1", "r2", "r3",
		"m0", "m1", "m2", "m3",
		"l0", "l1", "l2", "l3",
		"pcs", "pss", "eas", "lcs",
		"a2", "b2", "dpp", "psr",
		"ps0", "ps1", "ps2", "pc1",
		"x0", "y0", "x1", "y1",
		"a0", "b0", "a1", "b1"
	};

	static const char* PsrBitNames[] = {
		"C", "V", "Z", "N", "E", "U", "TB", "SV",
		"TE0", "TE1", "TE2", "TE3", "ET", "IM", "XL", "DP",
	};

	DspRegs::DspRegs(RECT& rect, std::string name, Cui* parent) :
		CuiWindow(rect, name, parent)
	{
		Memorize();
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

		if (!Jdi->IsLoaded())
		{
			return;
		}

		// DSP Run State

		if (Jdi->DspIsRunning())
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

		Print(CuiColor::Black, CuiColor::Normal, 48, 1, "a: %02X_%04X_%04X",
			(uint8_t)Jdi->DspGetReg((size_t)DspReg::a2),
			Jdi->DspGetReg((size_t)DspReg::a1),
			Jdi->DspGetReg((size_t)DspReg::a0));
		Print(CuiColor::Black, CuiColor::Normal, 48, 2, "b: %02X_%04X_%04X",
			(uint8_t)Jdi->DspGetReg((size_t)DspReg::b2),
			Jdi->DspGetReg((size_t)DspReg::b1),
			Jdi->DspGetReg((size_t)DspReg::b0));

		Print(CuiColor::Black, CuiColor::Normal, 48, 3, "x: %04X_%04X",
			Jdi->DspGetReg((size_t)DspReg::x1),
			Jdi->DspGetReg((size_t)DspReg::x0));
		Print(CuiColor::Black, CuiColor::Normal, 48, 4, "y: %04X_%04X",
			Jdi->DspGetReg((size_t)DspReg::y1),
			Jdi->DspGetReg((size_t)DspReg::y0));

		uint64_t bitsPacked = Jdi->DspPackProd();
		Print(CuiColor::Black, CuiColor::Normal, 48, 5, "p: %02X_%04X_%04X",
			(uint8_t)(bitsPacked >> 32),
			(uint16_t)(bitsPacked >> 16),
			(uint16_t)bitsPacked);

		// Program Counter

		Print(CuiColor::Black, CuiColor::Normal, 48, 7, "pc: %04X", Jdi->DspGetPc());

		// Status as individual bits

		DrawStatusBits();

		Memorize();
	}

	void DspRegs::DrawRegs()
	{
		for (int i = 0; i < 8; i++)
		{
			PrintReg(0, i + 1, (DspReg)i);
		}

		for (int i = 0; i < 8; i++)
		{
			PrintReg(12, i + 1, (DspReg)(8 + i));
		}

		for (int i = 0; i < 8; i++)
		{
			PrintReg(24, i + 1, (DspReg)(16 + i));
		}

		for (int i = 0; i < 8; i++)
		{
			PrintReg(36, i + 1, (DspReg)(24 + i));
		}
	}

	void DspRegs::DrawStatusBits()
	{
		for (int i = 0; i < 8; i++)
		{
			PrintPsrBit(66, i + 1, i);
		}

		for (int i = 0; i < 8; i++)
		{
			PrintPsrBit(73, i + 1, 8 + i);
		}
	}

	void DspRegs::PrintReg(int x, int y, DspReg n)
	{
		uint16_t value = Jdi->DspGetReg((size_t)n);
		bool same = savedRegs[(size_t)n] == value;

		Print( !same ? CuiColor::Lime : CuiColor::Normal,
			x, y, "%-3s: %04X", RegNames[(size_t)n], value);
	}

	void DspRegs::PrintPsrBit(int x, int y, int n)
	{
		uint16_t mask = (1 << n);
		uint16_t psr = Jdi->DspGetPsr();
		bool same = (savedPsr & mask) == (psr & mask);

		Print( !same ? CuiColor::Lime : CuiColor::Normal,
			x, y, "%-3s: %i", PsrBitNames[n], (psr & mask) ? 1 : 0);
	}

	void DspRegs::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		Invalidate();
	}

	void DspRegs::Memorize()
	{
		for (size_t i = 0; i < 32; i++)
		{
			savedRegs[i] = Jdi->DspGetReg(i);
		}
		savedPsr = Jdi->DspGetPsr();
	}

}
