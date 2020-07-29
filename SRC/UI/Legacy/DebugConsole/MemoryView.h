
#pragma once

namespace Debug
{

	class MemoryView : public CuiWindow
	{
	public:
		MemoryView(RECT& rect, std::string name);
		~MemoryView();

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

}
