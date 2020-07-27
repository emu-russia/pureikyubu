
#pragma once

namespace Debug
{

	class ReportWindow : public CuiWindow
	{
		static const size_t maxMessages = 32 * 1024;

		Thread* thread = nullptr;

		static void ThreadProc(void* param);

		std::vector<std::pair<CuiColor, std::string>> history;
		size_t messagePtr = 0;

		CuiColor ChannelNameToColor(const std::string& name);

	public:
		ReportWindow(RECT& rect, std::string name);
		~ReportWindow();

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

}
