
#pragma once

namespace Debug
{

	class GekkoDisasm : public CuiWindow
	{
	public:
		GekkoDisasm(RECT& rect, std::string name);
		~GekkoDisasm();

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

}
