
#pragma once

namespace Debug
{

	class GekkoRegs : public CuiWindow
	{
	public:
		GekkoRegs(RECT& rect, std::string name);
		~GekkoRegs();

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

}
