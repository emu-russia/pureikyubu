// Do not forget that DSP addressing is done with freaking 16-bit slots (`words`)

#pragma once

namespace Debug
{
	class DspDebug;

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
		DspImem(RECT& rect, std::string name, Cui* parent);

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

}
