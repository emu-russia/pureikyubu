
#pragma once

namespace Debug
{

	class ReportWindow : public CuiWindow
	{
		static const size_t maxMessages = 32 * 1024;

		Thread* thread = nullptr;

		static void ThreadProc(void* param);

		int messagePtr = 0;

		CuiColor ChannelNameToColor(const std::string& name);
		std::list<std::string> SplitMessages(std::string str);

	public:
		ReportWindow(RECT& rect, std::string name, Cui* parent);
		~ReportWindow();

		virtual void OnDraw();
		virtual void OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl);
	};

}
