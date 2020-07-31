
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

	// Processor Status Register bits

	#define PSR_C 0x0001		// Carry
	#define PSR_V 0x0002		// Overflow
	#define PSR_Z 0x0004		// Zero
	#define PSR_N 0x0008		// Negative
	#define PSR_E 0x0010		// Extension
	#define PSR_U 0x0020		// Unnormalization
	#define PSR_TB 0x0040		// Test bit
	#define PSR_SV 0x0080		// Sticky overflow
	#define PSR_TE0 0x0100		// Interrupt enable 0
	#define PSR_TE1 0x0200		// Interrupt enable 1
	#define PSR_TE2 0x0400		// Interrupt enable 2
	#define PSR_TE3 0x0800		// Interrupt enable 3
	#define PSR_ET 0x1000		// Global interrupt enable
	#define PSR_IM 0x2000		// Integer/fraction mode
	#define PSR_XL 0x4000		// Extension limit mode
	#define PSR_DP 0x8000		// Double precision mode

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
