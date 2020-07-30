// Command line. All commands go to JDI.
// It can be used both by the system debugger and as part of the DSP Debugger.

#include "pch.h"

namespace Debug
{

	CmdlineWindow::CmdlineWindow(RECT& rect, std::string name, Cui* parent)
		: CuiWindow (rect, name, parent)
	{
	}

	CmdlineWindow::~CmdlineWindow()
	{
	}

	void CmdlineWindow::OnDraw()
	{
		size_t promptSize = prompt.size();

		Fill(CuiColor::Black, CuiColor::Normal, ' ');
		Print(CuiColor::Black, CuiColor::Normal, 0, 0, prompt);
		Print(CuiColor::Black, CuiColor::Normal, (int)promptSize, 0, text);

		SetCursor((int)(promptSize + curpos), 0);
	}

	void CmdlineWindow::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		if (Ascii >= 0x20 && Ascii < 128)
		{
			// Add character at cursor position

			size_t promptSize = prompt.size();

			if (curpos >= (width - promptSize))
				return;

			if (curpos == text.size())
			{
				text.push_back(Ascii);
			}
			else
			{
				text = text.substr(0, curpos) + Ascii +
					text.substr(curpos);
			}
			curpos++;
		}
		else
		{
			// Enter, Up, Down, Home, End, Left, Right, Backspace, Delete

			size_t editlen = text.size();

			switch (Vkey)
			{
				case VK_LEFT:
					if (ctrl) SearchLeft();
					else if (curpos != 0) curpos--;
					break;
				case VK_RIGHT:
					if (editlen != 0 && (curpos != editlen))
					{
						if (ctrl) SearchRight();
						else if (curpos < editlen) curpos++;
					}
					break;
				case VK_RETURN:
					Process();
					break;
				case VK_BACK:
					if (curpos != 0)
					{
						if (editlen == curpos)
						{
							text = text.substr(0, editlen - 1);
						}
						else
						{
							text = text.substr(0, curpos - 1) + text.substr(curpos);
						}
						curpos--;
					}
					break;
				case VK_DELETE:
					if (editlen != 0)
					{
						text = text.substr(0, curpos) + (((curpos + 1) < editlen) ? text.substr(curpos + 1) : "");
					}
					break;
				case VK_HOME:
					curpos = 0;
					break;
				case VK_END:
					curpos = editlen;
					break;
				case VK_UP:
					historyPos--;
					if (historyPos < 0)
					{
						historyPos = 0;
					}
					else
					{
						text = history[historyPos];
						curpos = text.size();
					}
					break;
				case VK_DOWN:
					historyPos++;
					if (historyPos >= history.size())
					{
						historyPos = (int)history.size();
						ClearCmdline();
					}
					else
					{
						text = history[historyPos];
						curpos = text.size();
					}
					break;
			}
		}

		Invalidate();
	}

	bool CmdlineWindow::TestEmpty()
	{
		return text.find_first_not_of(' ') == std::string::npos;
	}

	// Searching for beginning of each word left
	void CmdlineWindow::SearchLeft()
	{
		size_t editlen = text.size();

		if (curpos == 0 || editlen == 0)
			return;

		// While spaces
		while (text[curpos - 1] == ' ')
		{
			curpos--;
			if (curpos == 0)
				break;
		}

		if (curpos == 0)
			return;

		// While non-space
		while (text[curpos - 1] != ' ')
		{
			curpos--;
			if (curpos == 0)
				break;
		}
	}

	// Searching for beginning of each word right
	void CmdlineWindow::SearchRight()
	{
		size_t editlen = text.size();

		if (curpos == editlen || editlen == 0)
			return;

		// While non-space
		while (text[curpos] != ' ')
		{
			curpos++;
			if (curpos == editlen)
				break;
		}

		if (curpos == editlen)
			return;

		// While spaces
		while (text[curpos] == ' ')
		{
			curpos++;
			if (curpos == editlen)
				break;
		}
	}

	void CmdlineWindow::Process()
	{
		if (TestEmpty())
		{
			return;
		}

		if (history.size() != 0)
		{
			if (history[history.size() - 1] != text)
			{
				history.push_back(text);
			}
		}
		else
		{
			history.push_back(text);
		}
		historyPos = (int)history.size();

		Jdi.Report(": " + text);

		if (Jdi.IsCommandExists(text))
		{
			Jdi.ExecuteCommand(text);
		}
		else
		{
			Jdi.Report("Unknown command, try \'help\'");
		}

		ClearCmdline();
		Invalidate();
	}

	void CmdlineWindow::ClearCmdline()
	{
		curpos = 0;
		text = "";
	}

}
