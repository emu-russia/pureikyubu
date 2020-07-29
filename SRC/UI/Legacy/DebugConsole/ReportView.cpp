// Displaying message history. It can be used both by the system debugger and as part of the DSP Debugger.

#include "pch.h"

namespace Debug
{
	// Make it global so that the message history is saved for the entire lifetime of the application.
	static std::vector<std::pair<CuiColor, std::string>> history;

	ReportWindow::ReportWindow(RECT& rect, std::string name, Cui* parent)
		: CuiWindow (rect, name, parent)
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

					std::list<std::string> strs = wnd->SplitMessages(it->second);

					for (auto it2 = strs.begin(); it2 != strs.end(); ++it2)
					{
						if (it2->size() == 0)
						{
							continue;
						}

						std::string text = "";

						CuiColor textColor = CuiColor::Normal;

						if (channelName.size() != 0)
						{
							textColor = wnd->ChannelNameToColor(channelName);

							if (!(channelName == "Info" || channelName == "Header"))
							{
								text += channelName + ": ";
							}
						}
						else
						{
							// Command history
							if ((*it2)[0] == ':')
							{
								textColor = CuiColor::Cyan;
							}
						}

						text += *it2;
						history.push_back(std::pair<CuiColor, std::string>(textColor, text));
					}
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

		FillLine(CuiColor::Cyan, CuiColor::White, 0, '-');

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

	std::list<std::string> ReportWindow::SplitMessages(std::string str)
	{
		std::list<std::string> strs;

		std::string delimiter = "\n";

		size_t pos = 0;
		std::string token;
		while ((pos = str.find(delimiter)) != std::string::npos)
		{
			token = str.substr(0, pos);
			strs.push_back(token);
			str.erase(0, pos + delimiter.length());
		}

		return strs;
	}

}
