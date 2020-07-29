
#pragma once

namespace Debug
{

	class DspRegs : public CuiWindow
	{
		// Used to highlight reg changes
		//DSP::DspRegs savedRegs;

		void DrawRegs();
		void DrawStatusBits();

	public:
		DspRegs(RECT& rect, std::string name, Cui* parent);

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

}
