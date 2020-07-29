// Displaying message history. It can be used both by the system debugger and as part of the DSP Debugger.

#include "pch.h"

namespace Debug
{
	// Make it global so that the message history is saved for the entire lifetime of the application.
	static std::vector<std::pair<CuiColor, std::string>> history;

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

			if (history.size() >= wnd->maxMessages)
			{
				history.clear();
			}

			// Append history

			std::list<std::pair<int, std::string>> queue;

			Jdi.QueryDebugMessages(queue);

			if (!queue.empty())
			{
				for (auto it = queue.begin(); it != queue.end(); ++it)
				{
					std::string channelName = Jdi.DebugChannelToString(it->first);

					std::string text = "";

					if (channelName.size() != 0)
					{
						text += channelName + ": ";
					}

					text += it->second;

					history.push_back(std::pair<CuiColor, std::string>(wnd->ChannelNameToColor(channelName), text));
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

		int i = (int)history.size() - 1;

		for (size_t y = height - 1; y != 0; y--)
		{
			FillLine(CuiColor::Black, CuiColor::Normal, (int)y, ' ');
			Print(CuiColor::Black, history[i].first, 0, (int)y, history[i].second);

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
