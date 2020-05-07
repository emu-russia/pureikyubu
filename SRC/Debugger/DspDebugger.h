// Visual DSP Debugger

#pragma once

#include "Cui.h"

namespace Debug
{
	class DspDebug : public Cui
	{
	public:
		DspDebug(std::string title, size_t width, size_t height);

		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

	class DspRegs : public CuiWindow
	{
		// Used to highlight reg changes
		DSP::DspRegs savedRegs;

		void DrawRegs();
		void DrawStatusBits();

	public:
		DspRegs(RECT& rect, std::string name);

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

	class DspDmem : public CuiWindow
	{
		DSP::DspAddress current = 0;

	public:
		DspDmem(RECT& rect, std::string name);

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

	class DspImem : public CuiWindow
	{
		DSP::DspAddress current = 0x8000;
		DSP::DspAddress cursor = 0x8000;
		size_t wordsOnScreen = 0;

		bool AddressVisible(DSP::DspAddress address);
		bool IsCall(DSP::DspAddress address, DSP::DspAddress& targetAddress);
		bool IsCallOrJump(DSP::DspAddress address, DSP::DspAddress& targetAddress);

		std::vector<std::pair<DSP::DspAddress, DSP::DspAddress>> browseHist;

	public:
		DspImem(RECT& rect, std::string name);

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};
}
