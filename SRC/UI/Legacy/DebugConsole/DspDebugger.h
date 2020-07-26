// Visual DSP Debugger

#pragma once

namespace Debug
{
	class DspImem;

	class DspDebug : public Cui
	{
		static const size_t width = 80;
		static const size_t height = 60;

		DspImem* imemWindow = nullptr;

	public:
		DspDebug();

		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

	class DspRegs : public CuiWindow
	{
		// Used to highlight reg changes
		//DSP::DspRegs savedRegs;

		void DrawRegs();
		void DrawStatusBits();

	public:
		DspRegs(RECT& rect, std::string name);

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

	class DspDmem : public CuiWindow
	{
		uint32_t current = 0;

	public:
		DspDmem(RECT& rect, std::string name);

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

	class DspImem : public CuiWindow
	{
		friend DspDebug;

		uint32_t current = 0x8000;
		uint32_t cursor = 0x8000;
		size_t wordsOnScreen = 0;

		bool AddressVisible(uint32_t address);
		bool IsCall(uint32_t address, uint32_t& targetAddress);
		bool IsCallOrJump(uint32_t address, uint32_t& targetAddress);

		std::vector<std::pair<uint32_t, uint32_t>> browseHist;

	public:
		DspImem(RECT& rect, std::string name);

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};
}
