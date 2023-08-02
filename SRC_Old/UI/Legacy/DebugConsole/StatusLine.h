
#pragma once

namespace Debug
{

	enum class DebugMode
	{
		Ready = 0,
		Scrolling,
	};

	class StatusWindow : public CuiWindow
	{
		DebugMode _mode = DebugMode::Ready;

	public:
		StatusWindow(RECT& rect, std::string name, Cui* parent);
		~StatusWindow();

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);

		void SetMode(DebugMode mode);
	};

}
