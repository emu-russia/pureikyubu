
#pragma once

namespace Debug
{

	class GekkoDisasm : public CuiWindow
	{
		uint32_t address = 0;
		uint32_t cursor = 0;

		std::vector<std::pair<uint32_t, uint32_t>> browseHist;

	public:
		GekkoDisasm(RECT& rect, std::string name, Cui* parent);
		~GekkoDisasm();

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);

		uint32_t GetCursor();
		void SetCursor(uint32_t address);
	};

}
