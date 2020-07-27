// Displaying message history

#include "pch.h"

namespace Debug
{

	ReportWindow::ReportWindow(RECT& rect, std::string name)
		: CuiWindow (rect, name)
	{
		thread = new Thread(ThreadProc, false, this, "ReportWindow");
	}

	ReportWindow::~ReportWindow()
	{
		delete thread;
	}

	void ReportWindow::ThreadProc(void* param)
	{
		ReportWindow* wnd = (ReportWindow*)param;

		while (true)
		{
			// When the story gets too big - clean up.

			if (wnd->history.size() >= wnd->maxMessages)
			{
				wnd->history.clear();
			}

			// Append history

			std::list<std::pair<int, std::string>> queue;

			Jdi.QueryDebugMessages(queue);

			if (!queue.empty())
			{
				for (auto it = queue.begin(); it != queue.end(); ++it)
				{
					std::string channelName = Jdi.DebugChannelToString(it->first);

					//if (channelName.size() != 0)
					//{
					//	printf("%s: ", channelName.c_str());
					//}

					//printf("%s", it->second.c_str());

					wnd->history.push_back(std::pair<CuiColor, std::string>(wnd->ChannelNameToColor(channelName), it->second));
				}

				queue.clear();

				wnd->Invalidate();
			}

			Sleep(100);
		}
	}

	void ReportWindow::OnDraw()
	{
		// Show window title with hints

		// Show messages starting with messagePtr backwards

		if (history.size() == 0)
		{
			return;
		}

		int i = history.size() - 1;

		for (int y = height - 1; y != 0; y--)
		{
			FillLine(CuiColor::Black, CuiColor::Normal, y, ' ');
			Print(CuiColor::Black, history[i].first, 0, y, history[i].second);

			i--;
			if (i < 0)
			{
				break;
			}
		}
	}

	void ReportWindow::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		// Up, Down, Home, End, PageUp, PageDown
	}

	CuiColor ReportWindow::ChannelNameToColor(const std::string& name)
	{
		if (name.size() == 0)
		{
			return CuiColor::Normal;
		}

        if (name == "Info")
        {
            return CuiColor::Green;
        }
        else if (name == "Error")
        {
            return CuiColor::BrightRed;
        }
        else if (name == "Header")
        {
            return CuiColor::Cyan;
        }

        else if (name == "Loader")
        {
            return CuiColor::Yellow;
        }

        else if (name == "HLE")
        {
            return CuiColor::Green;
        }

        else if (name == "DVD")
        {
            return CuiColor::Lime;
        }
        else if (name == "AX")
        {
            return CuiColor::Lime;
        }

		return CuiColor::Cyan;
	}

}
