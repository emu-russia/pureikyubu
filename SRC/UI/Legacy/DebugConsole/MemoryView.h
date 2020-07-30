
#pragma once

namespace Debug
{

	class MemoryView : public CuiWindow
	{
		static const size_t RAMSIZE = 0x0180'0000;	// 24 MBytes

		uint32_t cursor = 0x8000'0000;

		std::string hexbyte(uint32_t addr);
		char charbyte(uint32_t addr);

	public:
		MemoryView(RECT& rect, std::string name, Cui* parent);
		~MemoryView();

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);

		void SetCursor(uint32_t address);
	};

}
