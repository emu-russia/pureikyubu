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

		if (history.empty())
		{
			Jdi.Report("Debugger is running. Type help for quick reference.\n");
		}
	}

	ReportWindow::~ReportWindow()
	{
		delete thread;
	}

	void ReportWindow::ThreadProc(void* param)
	{
		ReportWindow* wnd = (ReportWindow*)param;

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

		Thread::Sleep(100);
	}

	void ReportWindow::OnDraw()
	{
		// Show window title with hints

		FillLine(CuiColor::Cyan, CuiColor::White, 0, ' ');
		std::string head = "[ ] F4";
		Print(CuiColor::Cyan, CuiColor::Black, 1, 0, head);
		if (IsActive())
		{
			Print(CuiColor::Cyan, CuiColor::White, 2, 0, "*");
		}
		Print(CuiColor::Cyan, CuiColor::Black, (int)(head.size() + 3), 0, "debug output");

		// Show messages starting with messagePtr backwards

		if (history.size() == 0)
		{
			return;
		}

		int i = (int)history.size() - messagePtr - 1;

		size_t y = height - 1;

		for (y ; y != 0; y--)
		{
			FillLine(CuiColor::Black, CuiColor::Normal, (int)y, ' ');
			Print(CuiColor::Black, history[i].first, 0, (int)y, history[i].second);

			i--;
			if (i < 0)
			{
				break;
			}
		}

		for (y; y != 0; y--)
		{
			FillLine(CuiColor::Black, CuiColor::Normal, (int)y, ' ');
		}
	}

	void ReportWindow::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		// Up, Down, Home, End, PageUp, PageDown

		switch (Vkey)
		{
			case VK_UP:
				messagePtr++;
				break;

			case VK_DOWN:
				messagePtr--;
				break;

			case VK_PRIOR:	// Page Up
				messagePtr += (int)(height - 1);
				break;

			case VK_NEXT:	// Page Down
				messagePtr -= (int)(height - 1);
				break;

			case VK_HOME:
				messagePtr = (int)history.size() - 2;
				break;

			case VK_END:
				messagePtr = 0;
				break;
		}

		messagePtr = max(0, min(messagePtr, (int)history.size() - 2));

		Invalidate();
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
