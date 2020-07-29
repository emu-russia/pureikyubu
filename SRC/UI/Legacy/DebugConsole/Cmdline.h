
#pragma once

namespace Debug
{
	
	class CmdlineWindow : public CuiWindow
	{
		const std::string prompt = "> ";

		size_t curpos = 0;			// Current cursor position
		std::string text;		// Current command line text

		std::vector<std::string> history;
		int historyPos = 0;

		bool TestEmpty();
		void SearchLeft();
		void SearchRight();
		void Process();
		void ClearCmdline();

	public:
		CmdlineWindow(RECT& rect, std::string name, Cui* parent);
		~CmdlineWindow();

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

}
