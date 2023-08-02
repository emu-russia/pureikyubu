
#pragma once

namespace Debug
{
	// DSP registers

	enum class DspReg
	{
		r0, r1, r2, r3,
		m0, m1, m2, m3,
		l0, l1, l2, l3,
		pcs, pss, eas, lcs,
		a2, b2, dpp, psr,
		ps0, ps1, ps2, pc1,
		x0, y0, x1, y1,
		a0, b0, a1, b1
	};

	class DspRegs : public CuiWindow
	{
		// Used to highlight reg changes
		uint16_t savedRegs[32] = { 0 };
		uint16_t savedPsr = 0;

		void DrawRegs();
		void DrawStatusBits();

		void PrintReg(int x, int y, DspReg n);
		void PrintPsrBit(int x, int y, int n);

		void Memorize();

	public:
		DspRegs(RECT& rect, std::string name, Cui* parent);

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

}
