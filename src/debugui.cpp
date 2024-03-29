#include "pch.h"

namespace Debug
{
	Debugger* debugger;
}

namespace Debug
{
	// Make it global so that the message history is saved for the entire lifetime of the application.
	static std::vector<std::pair<CuiColor, std::string>> history;

	ReportWindow::ReportWindow(CuiRect& rect, std::string name, Cui* parent)
		: CuiWindow(rect, name, parent)
	{
		thread = EMUCreateThread(ThreadProc, false, this, "ReportWindow");

		if (history.empty())
		{
			Jdi->Report("Debugger is running. Type help for quick reference.\n");
		}
	}

	ReportWindow::~ReportWindow()
	{
		EMUJoinThread(thread);
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

		Jdi->QueryDebugMessages(queue);

		if (!queue.empty())
		{
			for (auto it = queue.begin(); it != queue.end(); ++it)
			{
				std::string channelName = Jdi->DebugChannelToString(it->first);

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

						if (channelName == "Error")
						{
							channelName = "Break";
						}

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

		for (y; y != 0; y--)
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

	void ReportWindow::OnKeyPress(char Ascii, CuiVkey Vkey, bool shift, bool ctrl)
	{
		// Up, Down, Home, End, PageUp, PageDown

		switch (Vkey)
		{
			case CuiVkey::Up:
				messagePtr++;
				break;

			case CuiVkey::Down:
				messagePtr--;
				break;

			case CuiVkey::PageUp:
				messagePtr += (int)(height - 1);
				break;

			case CuiVkey::PageDown:
				messagePtr -= (int)(height - 1);
				break;

			case CuiVkey::Home:
				messagePtr = (int)history.size() - 2;
				break;

			case CuiVkey::End:
				messagePtr = 0;
				break;
		}

		messagePtr = my_max(0, my_min(messagePtr, (int)history.size() - 2));

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


namespace Debug
{
	StatusWindow::StatusWindow(CuiRect& rect, std::string name, Cui* parent)
		: CuiWindow(rect, name, parent)
	{
	}

	StatusWindow::~StatusWindow()
	{
	}

	void StatusWindow::OnDraw()
	{
		FillLine(CuiColor::Cyan, CuiColor::Black, 0, ' ');

		std::string text = "";

		switch (_mode)
		{
			case DebugMode::Ready:
				text = "Ready. Press PgUp to look behind.";
				break;
			case DebugMode::Scrolling:
				text = "Scroll Mode - Press PgUp, PgDown, Up, Down to scroll.";
				break;
		}

		Print(CuiColor::Cyan, CuiColor::Black, 2, 0, text);
	}

	void StatusWindow::OnKeyPress(char Ascii, CuiVkey Vkey, bool shift, bool ctrl)
	{
	}

	void StatusWindow::SetMode(DebugMode mode)
	{
		_mode = mode;
		Invalidate();
	}

}

// Command line. All commands go to Jdi->

namespace Debug
{

	CmdlineWindow::CmdlineWindow(CuiRect& rect, std::string name, Cui* parent)
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

	void CmdlineWindow::OnKeyPress(char Ascii, CuiVkey Vkey, bool shift, bool ctrl)
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
				case CuiVkey::Left:
					if (ctrl) SearchLeft();
					else if (curpos != 0) curpos--;
					break;
				case CuiVkey::Right:
					if (editlen != 0 && (curpos != editlen))
					{
						if (ctrl) SearchRight();
						else if (curpos < editlen) curpos++;
					}
					break;
				case CuiVkey::Enter:
					Process();
					break;
				case CuiVkey::Backspace:
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
				case CuiVkey::Delete:
					if (editlen != 0)
					{
						text = text.substr(0, curpos) + (((curpos + 1) < editlen) ? text.substr(curpos + 1) : "");
					}
					break;
				case CuiVkey::Home:
					curpos = 0;
					break;
				case CuiVkey::End:
					curpos = editlen;
					break;
				case CuiVkey::Up:
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
				case CuiVkey::Down:
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

		Jdi->Report(": " + text);

		if (Jdi->IsCommandExists(text))
		{
			Jdi->ExecuteCommand(text);
		}
		else
		{
			Jdi->Report("Unknown command, try \'help\'");
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

// JDI Communication.

namespace Debug
{
	JdiClient * Jdi;

	JdiClient::JdiClient()
	{
	}

	JdiClient::~JdiClient()
	{
	}

	// Generic

	std::string JdiClient::GetVersion()
	{
		char str[0x100] = { 0 };

		bool res = CallJdiReturnString("GetVersion", str, sizeof(str) - 1);
		if (!res)
		{
			throw "GetVersion failed!";
		}

		return std::string(str);
	}

	void JdiClient::ExecuteCommand(const std::string& cmdline)
	{
		bool res = CallJdiNoReturn(cmdline.c_str());
		if (!res)
		{
			throw "ExecuteCommand failed!";
		}
	}

	// Generic debug

	std::string JdiClient::DebugChannelToString(int chan)
	{
		char str[0x100] = { 0 };
		char cmd[0x30] = { 0, };

		sprintf(cmd, "GetChannelName %i", chan);

		bool res = CallJdiReturnString(cmd, str, sizeof(str) - 1);
		if (!res)
		{
			throw "GetChannelName failed!";
		}

		return std::string(str);
	}

	// Oldest messages first
	void JdiClient::QueryDebugMessages(std::list<std::pair<int, std::string>>& queue)
	{
		Json::Value* value = CallJdi("qd");
		if (value == nullptr)
		{
			throw "QueryDebugMessages failed!";
		}

		if (value->type != Json::ValueType::Array)
		{
			throw "QueryDebugMessages invalid format!";
		}

		auto it = value->children.begin();

		while (it != value->children.end())
		{
			Json::Value* channel = *it;
			++it;

			Json::Value* message = *it;
			++it;

			if (channel->type != Json::ValueType::Int || message->type != Json::ValueType::String)
			{
				throw "QueryDebugMessages invalid format of array key-values!";
			}

			queue.push_back(std::pair<int, std::string>((int)channel->value.AsInt, Util::WstringToString(message->value.AsString)));
		}

		delete value;
	}

	void JdiClient::Report(const std::string& text)
	{
		ExecuteCommand("echo " + text);
	}

	bool JdiClient::IsLoaded()
	{
		bool loaded = false;

		bool res = CallJdiReturnBool("IsLoaded", &loaded);
		if (!res)
		{
			throw "IsLoaded failed!";
		}

		return loaded;
	}

	bool JdiClient::IsCommandExists(const std::string& cmdline)
	{
		bool exists = false;

		std::string cmd = "IsCommandExists " + cmdline;

		bool res = CallJdiReturnBool(cmd.c_str(), &exists);
		if (!res)
		{
			throw "IsCommandExists failed!";
		}

		return exists;
	}

	// Gekko

	bool JdiClient::IsRunning()
	{
		bool running = false;
		CallJdiReturnBool("IsRunning", &running);
		return running;
	}

	void JdiClient::GekkoRun()
	{
		CallJdiNoReturn("GekkoRun");
	}

	void JdiClient::GekkoSuspend()
	{
		CallJdiNoReturn("GekkoSuspend");
	}

	void JdiClient::GekkoStep()
	{
		CallJdiNoReturn("GekkoStep");
	}

	void JdiClient::GekkoSkipInstruction()
	{
		CallJdiNoReturn("GekkoSkipInstruction");
	}

	uint32_t JdiClient::GetGpr(size_t n)
	{
		char cmd[0x40];
		sprintf(cmd, "GetGpr %zi", n);

		Json::Value* output = CallJdi(cmd);

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	uint64_t JdiClient::GetPs0(size_t n)
	{
		char cmd[0x40];
		sprintf(cmd, "GetPs0 %zi", n);

		Json::Value* output = CallJdi(cmd);

		uint64_t value = output->value.AsInt;

		delete output;

		return value;
	}

	uint64_t JdiClient::GetPs1(size_t n)
	{
		char cmd[0x40];
		sprintf(cmd, "GetPs1 %zi", n);

		Json::Value* output = CallJdi(cmd);

		uint64_t value = output->value.AsInt;

		delete output;

		return value;
	}

	uint32_t JdiClient::GetPc()
	{
		Json::Value* output = CallJdi("GetPc");
		
		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	uint32_t JdiClient::GetMsr()
	{
		Json::Value* output = CallJdi("GetMsr");

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	uint32_t JdiClient::GetCr()
	{
		Json::Value* output = CallJdi("GetCr");

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	uint32_t JdiClient::GetFpscr()
	{
		Json::Value* output = CallJdi("GetFpscr");

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	uint32_t JdiClient::GetSpr(size_t n)
	{
		char cmd[0x40];
		sprintf(cmd, "GetSpr %zi", n);

		Json::Value* output = CallJdi(cmd);

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	uint32_t JdiClient::GetSr(size_t n)
	{
		char cmd[0x40];
		sprintf(cmd, "GetSr %zi", n);

		Json::Value* output = CallJdi(cmd);

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	uint32_t JdiClient::GetTbu()
	{
		Json::Value* output = CallJdi("GetTbu");

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	uint32_t JdiClient::GetTbl()
	{
		Json::Value* output = CallJdi("GetTbl");

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	void* JdiClient::TranslateDMmu(uint32_t address)
	{
		char cmd[0x40];
		sprintf(cmd, "TranslateDMmu 0x%08X", address);

		Json::Value* output = CallJdi(cmd);

		void* ptr = (void *)output->value.AsInt;

		delete output;

		return ptr;
	}

	void* JdiClient::TranslateIMmu(uint32_t address)
	{
		char cmd[0x40];
		sprintf(cmd, "TranslateIMmu 0x%08X", address);

		Json::Value* output = CallJdi(cmd);

		void* ptr = (void*)output->value.AsInt;

		delete output;

		return ptr;
	}

	uint32_t JdiClient::VirtualToPhysicalDMmu(uint32_t address)
	{
		char cmd[0x40];
		sprintf(cmd, "VirtualToPhysicalDMmu 0x%08X", address);

		Json::Value* output = CallJdi(cmd);

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	uint32_t JdiClient::VirtualToPhysicalIMmu(uint32_t address)
	{
		char cmd[0x40];
		sprintf(cmd, "VirtualToPhysicalIMmu 0x%08X", address);

		Json::Value* output = CallJdi(cmd);

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	bool JdiClient::GekkoTestBreakpoint(uint32_t address)
	{
		char cmd[0x40];
		sprintf(cmd, "GekkoTestBreakpoint 0x%08X", address);

		Json::Value* output = CallJdi(cmd);

		bool value = output->value.AsBool;

		delete output;

		return value;
	}

	void JdiClient::GekkoToggleBreakpoint(uint32_t address)
	{
		char cmd[0x40];
		sprintf(cmd, "GekkoToggleBreakpoint 0x%08X", address);

		Json::Value* output = CallJdi(cmd);
		delete output;
	}

	void JdiClient::GekkoAddOneShotBreakpoint(uint32_t address)
	{
		char cmd[0x40];
		sprintf(cmd, "GekkoAddOneShotBreakpoint 0x%08X", address);

		Json::Value* output = CallJdi(cmd);
		delete output;
	}

	std::string JdiClient::GekkoDisasm(uint32_t address)
	{
		char cmd[0x40];
		sprintf(cmd, "GekkoDisasm 0x%08X", address);

		char text[0x200];

		CallJdiReturnString(cmd, text, sizeof(text) - 1);

		return text;
	}

	bool JdiClient::GekkoIsBranch(uint32_t address, uint32_t& targetAddress)
	{
		char cmd[0x40];
		sprintf(cmd, "GekkoIsBranch 0x%08X", address);

		Json::Value* output = CallJdi(cmd);

		bool flowControl = false;

		for (auto it = output->children.begin(); it != output->children.end(); ++it)
		{
			Json::Value* child = *it;

			if (child->type == Json::ValueType::Bool)
			{
				flowControl = child->value.AsBool;
			}

			if (child->type == Json::ValueType::Int)
			{
				targetAddress = child->value.AsUint32;
			}
		}

		delete output;

		return flowControl;
	}

	// Actually from HLE Symbols

	uint32_t JdiClient::AddressByName(const std::string& name)
	{
		char cmd[0x100];
		sprintf(cmd, "AddressByName %s", name.c_str());

		Json::Value* output = CallJdi(cmd);

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	std::string JdiClient::NameByAddress(uint32_t address)
	{
		char cmd[0x40];
		sprintf(cmd, "NameByAddress 0x%08X", address);

		char name[0x200];

		CallJdiReturnString(cmd, name, sizeof(name) - 1);

		return name;
	}

	// DSP

	bool JdiClient::DspIsRunning()
	{
		bool running = false;
		CallJdiReturnBool("DspIsRunning", &running);
		return running;
	}

	void JdiClient::DspRun()
	{
		CallJdiNoReturn("DspRun");
	}

	void JdiClient::DspSuspend()
	{
		CallJdiNoReturn("DspSuspend");
	}

	void JdiClient::DspStep()
	{
		CallJdiNoReturn("DspStep");
	}

	uint16_t JdiClient::DspGetReg(size_t n)
	{
		char cmd[0x40];
		sprintf(cmd, "DspGetReg %zi", n);

		Json::Value* output = CallJdi(cmd);

		uint16_t value = output->value.AsUint16;

		delete output;

		return value;
	}

	uint16_t JdiClient::DspGetPsr()
	{
		Json::Value* output = CallJdi("DspGetPsr");

		uint16_t value = output->value.AsUint16;

		delete output;

		return value;
	}

	uint16_t JdiClient::DspGetPc()
	{
		Json::Value* output = CallJdi("DspGetPc");

		uint16_t value = output->value.AsUint16;

		delete output;

		return value;
	}

	uint64_t JdiClient::DspPackProd()
	{
		Json::Value* output = CallJdi("DspPackProd");

		uint64_t value = output->value.AsInt;

		delete output;

		return value;
	}

	void* JdiClient::DspTranslateDMem(uint32_t address)
	{
		char cmd[0x40];
		sprintf(cmd, "DspTranslateDMem 0x%04X", (uint16_t)address);

		Json::Value* output = CallJdi(cmd);

		void* ptr = (void*)output->value.AsInt;

		delete output;

		return ptr;
	}

	void* JdiClient::DspTranslateIMem(uint32_t address)
	{
		char cmd[0x40];
		sprintf(cmd, "DspTranslateIMem 0x%04X", (uint16_t)address);

		Json::Value* output = CallJdi(cmd);

		void* ptr = (void*)output->value.AsInt;

		delete output;

		return ptr;
	}

	bool JdiClient::DspTestBreakpoint(uint32_t address)
	{
		char cmd[0x40];
		sprintf(cmd, "DspTestBreakpoint 0x%04X", (uint16_t)address);

		Json::Value* output = CallJdi(cmd);

		bool value = output->value.AsBool;

		delete output;

		return value;
	}

	void JdiClient::DspToggleBreakpoint(uint32_t address)
	{
		char cmd[0x40];
		sprintf(cmd, "DspToggleBreakpoint 0x%04X", (uint16_t)address);

		Json::Value* output = CallJdi(cmd);
		delete output;
	}

	void JdiClient::DspAddOneShotBreakpoint(uint32_t address)
	{
		char cmd[0x40];
		sprintf(cmd, "DspAddOneShotBreakpoint 0x%04X", (uint16_t)address);

		Json::Value* output = CallJdi(cmd);
		delete output;
	}

	std::string JdiClient::DspDisasm(uint32_t address, size_t& instrSizeWords, bool& flowControl)
	{
		char cmd[0x40];
		sprintf(cmd, "DspDisasm 0x%04X", (uint16_t)address);

		Json::Value* output = CallJdi(cmd);

		std::string text = "";

		for (auto it = output->children.begin(); it != output->children.end(); ++it)
		{
			Json::Value* child = *it;

			if (child->type == Json::ValueType::Bool)
			{
				flowControl = child->value.AsBool;
			}

			if (child->type == Json::ValueType::Int)
			{
				instrSizeWords = (size_t)child->value.AsInt;
			}

			if (child->type == Json::ValueType::String)
			{
				text = Util::WstringToString(child->value.AsString);
			}
		}

		delete output;

		return text;
	}

	bool JdiClient::DspIsCall(uint32_t address, uint32_t& targetAddress)
	{
		char cmd[0x40];
		sprintf(cmd, "DspIsCall 0x%04X", (uint16_t)address);

		Json::Value* output = CallJdi(cmd);

		bool flag = false;

		for (auto it = output->children.begin(); it != output->children.end(); ++it)
		{
			Json::Value* child = *it;

			if (child->type == Json::ValueType::Bool)
			{
				flag = child->value.AsBool;
			}

			if (child->type == Json::ValueType::Int)
			{
				targetAddress = child->value.AsUint16;
			}
		}

		delete output;

		return flag;
	}

	bool JdiClient::DspIsCallOrJump(uint32_t address, uint32_t& targetAddress)
	{
		char cmd[0x40];
		sprintf(cmd, "DspIsCallOrJump 0x%04X", (uint16_t)address);

		Json::Value* output = CallJdi(cmd);

		bool flag = false;

		for (auto it = output->children.begin(); it != output->children.end(); ++it)
		{
			Json::Value* child = *it;

			if (child->type == Json::ValueType::Bool)
			{
				flag = child->value.AsBool;
			}

			if (child->type == Json::ValueType::Int)
			{
				targetAddress = child->value.AsUint16;
			}
		}

		delete output;

		return flag;
	}

}


// System-wide debugger

namespace Debug
{
	Debugger::Debugger()
		: Cui ("Debug Console", width, height)
	{
		CuiRect rect;

		// Create an interface for communicating with the emulator core, if it has not been created yet.

		if (!Jdi)
		{
			Jdi = new JdiClient;
		}

		// Registers

		rect.left = 0;
		rect.top = 0;
		rect.right = width;
		rect.bottom = regsHeight - 1;

		regs = new DebugRegs(rect, "DebugRegs", this);

		AddWindow(regs);

		// Memory hexview

		rect.left = 0;
		rect.top = regsHeight;
		rect.right = width;
		rect.bottom = regsHeight + memViewHeight - 1;

		memview = new MemoryView(rect, "MemoryView", this);

		AddWindow(memview);

		// Gekko/DSP disasm

		rect.left = 0;
		rect.top = regsHeight + memViewHeight;
		rect.right = width;
		rect.bottom = regsHeight + memViewHeight + disaHeight - 1;

		disasm = new Disasm(rect, "Disasm", this);

		AddWindow(disasm);

		// Message history

		rect.left = 0;
		rect.top = regsHeight + memViewHeight + disaHeight;
		rect.right = width;
		rect.bottom = height - 3;

		msgs = new ReportWindow(rect, "ReportWindow", this);

		AddWindow(msgs);

		// Command line

		rect.left = 0;
		rect.top = height - 2;
		rect.right = width;
		rect.bottom = height - 2;

		cmdline = new CmdlineWindow(rect, "Cmdline", this);

		AddWindow(cmdline);

		// Status

		rect.left = 0;
		rect.top = height - 1;
		rect.right = width;
		rect.bottom = height - 1;

		status = new StatusWindow(rect, "Status", this);

		AddWindow(status);

		SetWindowFocus("Cmdline");
	}

	void Debugger::OnKeyPress(char Ascii, CuiVkey Vkey, bool shift, bool ctrl)
	{
		if ((Vkey == CuiVkey::Backspace || (Ascii >= 0x20 && Ascii < 256)) && !cmdline->IsActive())
		{
			SetWindowFocus("Cmdline");
			status->SetMode(DebugMode::Ready);
			InvalidateAll();
			return;
		}

		switch (Vkey)
		{
			case CuiVkey::F1:
				SetWindowFocus("DebugRegs");
				InvalidateAll();
				break;

			case CuiVkey::F2:
				SetWindowFocus("MemoryView");
				InvalidateAll();
				break;

			case CuiVkey::F3:
				SetWindowFocus("Disasm");
				InvalidateAll();
				break;

			case CuiVkey::F4:
				SetWindowFocus("ReportWindow");
				status->SetMode(DebugMode::Scrolling);
				InvalidateAll();
				break;

			case CuiVkey::F5:
				if (disasm->GetMode() == DisasmMode::GekkoDisasm) {
					// Continue/break Gekko execution
					if (Jdi->IsLoaded())
					{
						if (Jdi->IsRunning())
						{
							Jdi->GekkoSuspend();
							disasm->SetCursor(Jdi->GetPc());
						}
						else
						{
							Jdi->GekkoRun();
						}
						InvalidateAll();
					}
				}
				else if (disasm->GetMode() == DisasmMode::DSPDisasm) {
					// Suspend/Run both cores
					if (Jdi->DspIsRunning())
					{
						Jdi->DspSuspend();
						Jdi->GekkoSuspend();
					}
					else
					{
						Jdi->DspRun();
						Jdi->GekkoRun();
					}
				}
				break;

			case CuiVkey::F9:
				if (disasm->GetMode() == DisasmMode::GekkoDisasm) {
					// Toggle Gekko Breakpoint
					Jdi->GekkoToggleBreakpoint(disasm->GetCursor());
					disasm->Invalidate();
				}
				else if (disasm->GetMode() == DisasmMode::DSPDisasm) {
					// Toggle DSP Breakpoint
					// TODO
				}
				break;

			case CuiVkey::F10:
				if (disasm->GetMode() == DisasmMode::GekkoDisasm) {
					// Step Over Gekko
					if (Jdi->IsLoaded() && !Jdi->IsRunning())
					{
						Jdi->GekkoAddOneShotBreakpoint(Jdi->GetPc() + 4);
						Jdi->GekkoRun();
						InvalidateAll();
					}
				}
				else if (disasm->GetMode() == DisasmMode::DSPDisasm) {
					// Step Over DSPCore
					if (Jdi->IsLoaded() && !Jdi->DspIsRunning())
					{
						uint32_t targetAddress = 0;
						if (Jdi->DspIsCall(Jdi->DspGetPc(), targetAddress))
						{
							Jdi->DspAddOneShotBreakpoint(Jdi->DspGetPc() + 2);
							Jdi->DspRun();
						}
						else
						{
							Jdi->DspStep();
							if (!disasm->DspAddressVisible(Jdi->DspGetPc()))
							{
								disasm->DspEnsurePcVisible();
							}
						}
						InvalidateAll();
					}
				}
				break;

			case CuiVkey::F11:
				if (disasm->GetMode() == DisasmMode::GekkoDisasm) {
					// Step Into Gekko
					if (Jdi->IsLoaded() && !Jdi->IsRunning())
					{
						Jdi->GekkoStep();
						disasm->SetCursor(Jdi->GetPc());
						InvalidateAll();
					}
				}
				else if (disasm->GetMode() == DisasmMode::DSPDisasm) {
					// Step Into DSPCore
					if (Jdi->IsLoaded() && !Jdi->IsRunning()) {
						Jdi->DspStep();
						if (!disasm->DspAddressVisible(Jdi->DspGetPc()))
						{
							disasm->DspEnsurePcVisible();
						}
						InvalidateAll();
					}
				}
				break;

			case CuiVkey::F12:
				if (disasm->GetMode() == DisasmMode::GekkoDisasm) {
					// Skip Gekko instruction
					if (Jdi->IsLoaded() && !Jdi->IsRunning())
					{
						Jdi->GekkoSkipInstruction();
						InvalidateAll();
					}
				}
				break;

			case CuiVkey::Escape:
				if (msgs->IsActive())
				{
					SetWindowFocus("Cmdline");
					status->SetMode(DebugMode::Ready);
					InvalidateAll();
				}
				break;

			case CuiVkey::PageUp:
				if (cmdline->IsActive())
				{
					SetWindowFocus("ReportWindow");
					status->SetMode(DebugMode::Scrolling);
					InvalidateAll();
				}
				break;
		}
	}

	/// <summary>
	/// Set current address to view memory. Used by "d" command.
	/// </summary>
	/// <param name="virtualAddress"></param>
	void Debugger::SetMemoryCursor(uint32_t virtualAddress)
	{
		memview->SetCursor(virtualAddress);
	}

	/// <summary>
	/// Set the current address to view the disassembled Gekko code. Used by "u" command.
	/// </summary>
	/// <param name="virtualAddress"></param>
	void Debugger::SetDisasmCursor(uint32_t virtualAddress)
	{
		disasm->SetCursor(virtualAddress);
	}
}

// Disassembling code by Gekko virtual addresses. If the instruction is in Main mem, we disassemble and print, otherwise skip.

namespace Debug
{

	Disasm::Disasm(CuiRect& rect, std::string name, Cui* parent)
		: CuiWindow (rect, name, parent)
	{
		uint32_t main = Jdi->AddressByName("main");
		if (main)
		{
			SetCursor(main);
		}
		else
		{
			SetCursor(Jdi->GetPc());
		}
	}

	Disasm::~Disasm()
	{
	}

	void Disasm::OnDraw()
	{
		FillLine(CuiColor::Cyan, CuiColor::White, 0, ' ');
		std::string head = "[ ] F3";
		Print(CuiColor::Cyan, CuiColor::Black, 1, 0, head);
		if (IsActive())
		{
			Print(CuiColor::Cyan, CuiColor::White, 2, 0, "*");
		}

		char hint[0x100] = { 0, };

		if (mode == DisasmMode::GekkoDisasm) {
			sprintf(hint, " Gekko disasm, cursor:0x%08X phys:0x%08X pc:0x%08X",
				gekko_cursor, Jdi->VirtualToPhysicalIMmu(gekko_cursor), Jdi->GetPc());
		}
		else if (mode == DisasmMode::DSPDisasm) {
			sprintf(hint, " DSP disasm, cursor:0x%08X", dsp_cursor);
		}

		Print(CuiColor::Cyan, CuiColor::Black, (int)(head.size() + 3), 0, hint);

		if (mode == DisasmMode::DSPDisasm) {
			DspImemOnDraw();
			return;
		}

		// Code

		uint32_t addr = gekko_address & ~3;
		disa_sub_h = 0;

		for (int line = 1; line < height; line++, addr += 4)
		{
			int n = DisasmLine(line, addr);
			if (n > 1) disa_sub_h += n - 1;
			line += n - 1;
		}
	}

	void Disasm::OnKeyPress(char Ascii, CuiVkey Vkey, bool shift, bool ctrl)
	{
		uint32_t targetAddress = 0;

		if (mode == DisasmMode::DSPDisasm) {
			DspImemOnKeyPress(Ascii, Vkey, shift, ctrl);
			return;
		}

		switch (Vkey)
		{
			case CuiVkey::Home:
				SetCursor(Jdi->GetPc());
				break;

			case CuiVkey::End:
				break;

			case CuiVkey::Up:
				if (gekko_cursor < gekko_address)
				{
					gekko_cursor = gekko_address;
					break;
				}
				if (gekko_cursor >= (gekko_address + (uint32_t)(4 * height) - 4))
				{
					gekko_cursor = gekko_address + (uint32_t)(4 * height) - 8;
					break;
				}
				gekko_cursor -= 4;
				if (gekko_cursor < gekko_address)
				{
					gekko_address -= 4;
				}
				break;

			case CuiVkey::Down:
				if (gekko_cursor < gekko_address)
				{
					gekko_cursor = gekko_address;
					break;
				}
				if (gekko_cursor >= (gekko_address + 4 * (uint32_t)(height - disa_sub_h) - 4))
				{
					gekko_cursor = gekko_address + 4 * (uint32_t)(height - disa_sub_h) - 8;
					break;
				}
				gekko_cursor += 4;
				if (gekko_cursor >= (gekko_address + ((uint32_t)(height - disa_sub_h) - 1) * 4))
				{
					gekko_address += 4;
				}
				break;

			case CuiVkey::PageUp:
				gekko_address -= (uint32_t)(4 * height - 4);
				if (!IsCursorVisible())
				{
					gekko_cursor = gekko_address;
				}
				break;

			case CuiVkey::PageDown:
				gekko_address += (uint32_t)(4 * (height - disa_sub_h) - 4);
				if (!IsCursorVisible())
				{
					gekko_cursor = gekko_address + ((uint32_t)(height - disa_sub_h) - 2) * 4;
				}
				break;

			case CuiVkey::Enter:
				if (Jdi->GekkoIsBranch(gekko_cursor, targetAddress))
				{
					std::pair<uint32_t, uint32_t> last(gekko_address, gekko_cursor);
					browseHist.push_back(last);
					gekko_address = gekko_cursor = targetAddress;
				}
				break;

			case CuiVkey::Escape:
				if (browseHist.size() > 0)
				{
					std::pair<uint32_t, uint32_t> last = browseHist.back();
					gekko_address = last.first;
					gekko_cursor = last.second;
					browseHist.pop_back();
				}
				break;

			case CuiVkey::Left:
				RotateView(false);
				break;
			case CuiVkey::Right:
				RotateView(true);
				break;
		}

		Invalidate();
	}

	uint32_t Disasm::GetCursor()
	{
		if (mode == DisasmMode::GekkoDisasm) {
			return gekko_cursor;
		}
		else if (mode == DisasmMode::DSPDisasm) {
			return dsp_cursor;
		}
		return 0;
	}

	void Disasm::SetCursor(uint32_t addr)
	{
		if (mode == DisasmMode::GekkoDisasm) {
			gekko_cursor = addr & ~3;
			gekko_address = gekko_cursor - (uint32_t)(height - 1) / 2 * 4;
		}
		else if (mode == DisasmMode::DSPDisasm) {
			dsp_cursor = addr;
			dsp_current = dsp_cursor - (uint32_t)(height - 1) / 2;
		}
		Invalidate();
	}

	bool Disasm::IsCursorVisible()
	{
		uint32_t limit;
		limit = gekko_address + (uint32_t)((height - 1) * 4);
		return ((gekko_cursor < limit) && (gekko_cursor >= gekko_address));
	}

	int Disasm::DisasmLine(int line, uint32_t addr)
	{
		CuiColor bgpc, bgcur, bgbp;
		CuiColor bg;
		std::string symbol;
		int addend = 1;

		bgcur = (addr == gekko_cursor) ? (CuiColor::Gray) : (CuiColor::Black);
		bgbp = (Jdi->GekkoTestBreakpoint(addr)) ? (CuiColor::Red) : (CuiColor::Black);
		bgpc = (addr == Jdi->GetPc()) ? (CuiColor::DarkBlue) : (CuiColor::Black);
		bg = (CuiColor)((int)bgpc ^ (int)bgcur ^ (int)bgbp);

		FillLine(bg, CuiColor::Normal, line, ' ');

		// Symbolic information at address

		symbol = Jdi->NameByAddress(addr);
		if (!symbol.empty())
		{
			Print(bg, CuiColor::Green, 0, line, "%s", symbol.c_str());
			line++;
			addend++;

			FillLine(bg, CuiColor::Normal, line, ' ');
		}

		// Translate address

		uint32_t* ptr = (uint32_t*)Jdi->TranslateIMmu(addr);
		if (!ptr)
		{
			// No memory
			Print(bg, CuiColor::Normal, 0, line, "%08X  ", addr);
			Print(bg, CuiColor::Cyan, 10, line, "%08X  ", 0);
			Print(bg, CuiColor::Normal, 20, line, "???");
			return 1;
		}

		// Print address and opcode

		uint32_t opcode = _BYTESWAP_UINT32(*ptr);

		Print(bg, CuiColor::Normal, 0, line, "%08X  ", addr);
		Print(bg, CuiColor::Cyan, 10, line, "%08X  ", opcode);

		// Disasm

		std::string text = Jdi->GekkoDisasm(addr);

		bool flow = false;
		uint32_t targetAddress = 0;

		Print(bg, CuiColor::Normal, 20, line, "%s", text.c_str());

		// Branch hints

		flow = Jdi->GekkoIsBranch(addr, targetAddress);

		if (flow && targetAddress != 0)
		{
			const char* dir;

			if (targetAddress > addr) dir = " \x19";
			else if (targetAddress < addr) dir = " \x18";
			else dir = " \x1b";

			Print(bg, CuiColor::Cyan, 20 + (int)text.size(), line, "%s", dir);

			symbol = Jdi->NameByAddress(targetAddress);
			if (!symbol.empty())
			{
				Print(bg, CuiColor::Brown, 47, line, "; %s", symbol.c_str());
			}
		}

		// Rlwinm-like mask hint

		if (text.size() > 2)
		{
			if (text[0] == 'r' && text[1] == 'l')
			{
				int mb = ((opcode >> 6) & 0x1f);
				int me = ((opcode >> 1) & 0x1f);
				uint32_t mask = ((uint32_t)-1 >> mb) ^ ((me >= 31) ? 0 : ((uint32_t)-1) >> (me + 1));

				Print(bg, CuiColor::Normal, 60, line, "mask:0x%08X", mask);
			}
		}

		return addend;
	}

	void Disasm::RotateView(bool forward)
	{
		if (forward)
		{
			switch (mode)
			{
				case DisasmMode::GekkoDisasm: mode = DisasmMode::DSPDisasm; break;
				case DisasmMode::DSPDisasm: mode = DisasmMode::GekkoDisasm; break;
			}
		}
		else
		{
			switch (mode)
			{
				case DisasmMode::GekkoDisasm: mode = DisasmMode::DSPDisasm; break;
				case DisasmMode::DSPDisasm: mode = DisasmMode::GekkoDisasm; break;
			}
		}
	}

	void Disasm::DspEnsurePcVisible()
	{
		dsp_current = dsp_cursor = Jdi->DspGetPc();
	}

	bool Disasm::DspAddressVisible(uint32_t address)
	{
		if (!wordsOnScreen)
			return false;

		return (dsp_current <= address && address < (dsp_current + wordsOnScreen));
	}

	void Disasm::DspImemOnDraw()
	{
		// If GameCube is not powered on

		if (!Jdi->IsLoaded())
		{
			return;
		}

		// Show Dsp disassembly

		size_t lines = height - 1;
		uint32_t addr = dsp_current;
		int y = 1;

		// Do not forget that DSP addressing is done in 16-bit words.

		wordsOnScreen = 0;

		while (lines--)
		{
			size_t instrSizeInWords = 0;
			bool flowControl = false;

			std::string text = Jdi->DspDisasm(addr, instrSizeInWords, flowControl);

			if (text.empty())
			{
				addr++;
				y++;
				continue;
			}

			CuiColor backColor = CuiColor::Black;

			int bgcur = (addr == dsp_cursor) ? ((int)CuiColor::Blue) : (0);
			int bgbp = (Jdi->DspTestBreakpoint(addr)) ? ((int)CuiColor::Red) : (0);
			int bg = (addr == Jdi->DspGetPc()) ? ((int)CuiColor::DarkBlue) : (0);
			bg = bg ^ bgcur ^ bgbp;

			backColor = (CuiColor)bg;

			FillLine(backColor, CuiColor::Normal, y, ' ');
			Print(backColor, flowControl ? CuiColor::Green : CuiColor::Normal, 0, y, text);

			addr += (uint32_t)instrSizeInWords;
			wordsOnScreen += instrSizeInWords;
			y++;
		}
	}

	void Disasm::DspImemOnKeyPress(char Ascii, CuiVkey Vkey, bool shift, bool ctrl)
	{
		uint32_t targetAddress = 0;

		switch (Vkey)
		{
			case CuiVkey::Up:
				if (DspAddressVisible(dsp_cursor))
				{
					if (dsp_cursor > 0)
						dsp_cursor--;
				}
				else
				{
					dsp_cursor = (uint32_t)(dsp_current - wordsOnScreen);
				}
				if (!DspAddressVisible(dsp_cursor))
				{
					if (dsp_current < (height - 1))
						dsp_current = 0;
					else
						dsp_current -= (uint32_t)(height - 1);
				}
				break;

			case CuiVkey::Down:
				if (DspAddressVisible(dsp_cursor))
					dsp_cursor++;
				else
					dsp_cursor = dsp_current;
				if (!DspAddressVisible(dsp_cursor))
				{
					dsp_current = dsp_cursor;
				}
				break;

			case CuiVkey::PageUp:
				if (dsp_current < (height - 1))
					dsp_current = 0;
				else
					dsp_current -= (uint32_t)(height - 1);
				break;

			case CuiVkey::PageDown:
				dsp_current += (uint32_t)(wordsOnScreen ? wordsOnScreen : height - 1);
				if (dsp_current >= 0x8A00)
					dsp_current = 0x8A00;
				break;

			case CuiVkey::Home:
				if (ctrl)
				{
					dsp_current = dsp_cursor = 0;
				}
				else
				{
					dsp_current = dsp_cursor = Jdi->DspGetPc();
				}
				break;

			case CuiVkey::End:
				dsp_current = IROM_START_ADDRESS;
				break;

			case CuiVkey::F9:
				if (DspAddressVisible(dsp_cursor))
				{
					Jdi->DspToggleBreakpoint(dsp_cursor);
				}
				break;

			case CuiVkey::Enter:
				if (Jdi->DspIsCallOrJump(dsp_cursor, targetAddress))
				{
					std::pair<uint32_t, uint32_t> last(dsp_current, dsp_cursor);
					dspBrowseHist.push_back(last);
					dsp_current = dsp_cursor = targetAddress;
				}
				break;

			case CuiVkey::Escape:
				if (dspBrowseHist.size() > 0)
				{
					std::pair<uint32_t, uint32_t> last = dspBrowseHist.back();
					dsp_current = last.first;
					dsp_cursor = last.second;
					dspBrowseHist.pop_back();
				}
				break;

			case CuiVkey::Left:
				RotateView(false);
				break;
			case CuiVkey::Right:
				RotateView(true);
				break;
		}

		Invalidate();
	}
}

// View registers

namespace Debug
{
	static const char* gprnames[] = {
		"r0" , "sp" , "sd2", "r3" , "r4" , "r5" , "r6" , "r7" ,
		"r8" , "r9" , "r10", "r11", "r12", "sd1", "r14", "r15",
		"r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
		"r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31"
	};

	static const char* DspRegNames[] = {
		"r0", "r1", "r2", "r3",
		"m0", "m1", "m2", "m3",
		"l0", "l1", "l2", "l3",
		"pcs", "pss", "eas", "lcs",
		"a2", "b2", "dpp", "psr",
		"ps0", "ps1", "ps2", "pc1",
		"x0", "y0", "x1", "y1",
		"a0", "b0", "a1", "b1"
	};

	static const char* DspPsrBitNames[] = {
		"c", "v", "z", "n", "e", "u", "tb", "sv",
		"te0", "te1", "te2", "te3", "et", "im", "xl", "dp",
	};

	DebugRegs::DebugRegs(CuiRect& rect, std::string name, Cui* parent)
		: CuiWindow (rect, name, parent)
	{
		Memorize();
	}

	DebugRegs::~DebugRegs()
	{
	}

	void DebugRegs::OnDraw()
	{
		Fill(CuiColor::Black, CuiColor::Normal, ' ');

		FillLine(CuiColor::Cyan, CuiColor::White, 0, ' ');
		std::string head = "[ ] F1";
		Print(CuiColor::Cyan, CuiColor::Black, 1, 0, head);
		if (IsActive())
		{
			Print(CuiColor::Cyan, CuiColor::White, 2, 0, "*");
		}

		// Show status of Gekko and DSP

		std::string coreStatus;

		coreStatus += "Gekko: ";
		coreStatus += Jdi->IsRunning() ? "Run " : "Halt";
		coreStatus += " DSP: ";
		coreStatus += Jdi->DspIsRunning() ? "Run " : "Halt";

		Print(CuiColor::Cyan, CuiColor::Black, (int)(width - coreStatus.size() - 2), 0, coreStatus);

		std::string modeText;

		switch (mode)
		{
			case DebugRegmode::GPR: modeText = "Gekko GPR"; break;
			case DebugRegmode::FPR: modeText = "Gekko FPR"; break;
			case DebugRegmode::PSR: modeText = "Gekko PSR"; break;
			case DebugRegmode::MMU: modeText = "Gekko MMU"; break;
			case DebugRegmode::DSP: modeText = "DSP Regs"; break;
		}

		Print(CuiColor::Cyan, CuiColor::Black, (int)(head.size() + 3), 0, modeText);

		switch (mode)
		{
			case DebugRegmode::GPR: ShowGprs(); break;
			case DebugRegmode::FPR: ShowFprs(); break;
			case DebugRegmode::PSR: ShowPairedSingle(); break;
			case DebugRegmode::MMU: ShowMmu(); break;
			case DebugRegmode::DSP: ShowDspRegs(); break;
		}
	}

	void DebugRegs::OnKeyPress(char Ascii, CuiVkey Vkey, bool shift, bool ctrl)
	{
		switch (Vkey)
		{
			case CuiVkey::Left:
			case CuiVkey::PageUp:
				RotateView(false);
				break;

			case CuiVkey::Right:
			case CuiVkey::PageDown:
				RotateView(true);
				break;
		}

		Invalidate();
	}

	void DebugRegs::Memorize()
	{
		for (size_t n = 0; n < 32; n++)
		{
			savedGpr[n] = Jdi->GetGpr(n);
			savedPs0[n].Raw = Jdi->GetPs0(n);
			savedPs1[n].Raw = Jdi->GetPs1(n);
		}
	}

	void DebugRegs::ShowGprs()
	{
		int y;

		for (y = 1; y <= 16; y++)
		{
			print_gprreg(0, y, y - 1);
			print_gprreg(14, y, y - 1 + 16);
		}

		ShowOtherRegs();
	}

	void DebugRegs::ShowOtherRegs()
	{
		// Names

		Print(CuiColor::Cyan, 28, 1, "cr  ");
		Print(CuiColor::Cyan, 28, 2, "xer ");
		Print(CuiColor::Cyan, 28, 4, "ctr ");
		Print(CuiColor::Cyan, 28, 5, "dec ");
		Print(CuiColor::Cyan, 28, 8, "pc  ");
		Print(CuiColor::Cyan, 28, 9, "lr  ");
		Print(CuiColor::Cyan, 28,14, "tbr ");

		Print(CuiColor::Cyan, 42, 1, "msr   ");
		Print(CuiColor::Cyan, 42, 2, "fpscr ");
		Print(CuiColor::Cyan, 42, 4, "hid0  ");
		Print(CuiColor::Cyan, 42, 5, "hid1  ");
		Print(CuiColor::Cyan, 42, 6, "hid2  ");
		Print(CuiColor::Cyan, 42, 8, "wpar  ");
		Print(CuiColor::Cyan, 42, 9, "dmau  ");
		Print(CuiColor::Cyan, 42,10, "dmal  ");

		Print(CuiColor::Cyan, 58,  1, "dsisr ");
		Print(CuiColor::Cyan, 58,  2, "dar   ");
		Print(CuiColor::Cyan, 58,  4, "srr0  ");
		Print(CuiColor::Cyan, 58,  5, "srr1  ");
		Print(CuiColor::Cyan, 58,  8, "sprg0 ");
		Print(CuiColor::Cyan, 58,  9, "sprg1 ");
		Print(CuiColor::Cyan, 58, 10, "sprg2 ");
		Print(CuiColor::Cyan, 58, 11, "sprg3 ");
		Print(CuiColor::Cyan, 58, 13, "ear   ");
		Print(CuiColor::Cyan, 58, 14, "pvr   ");

		// Values

		Print(CuiColor::Normal, 32, 1, "%08X", Jdi->GetCr());
		Print(CuiColor::Normal, 32, 2, "%08X", Jdi->GetSpr(Gekko::SPR::XER));
		Print(CuiColor::Normal, 32, 4, "%08X", Jdi->GetSpr(Gekko::SPR::CTR));
		Print(CuiColor::Normal, 32, 5, "%08X", Jdi->GetSpr(Gekko::SPR::DEC));
		Print(CuiColor::Normal, 32, 8, "%08X", Jdi->GetPc());
		Print(CuiColor::Normal, 32, 9, "%08X", Jdi->GetSpr(Gekko::SPR::LR));
		Print(CuiColor::Normal, 32, 14, "%08X:%08X", Jdi->GetTbu(), Jdi->GetTbl());

		uint32_t msr = Jdi->GetMsr();
		uint32_t hid2 = Jdi->GetSpr(Gekko::SPR::HID2);

		Print(CuiColor::Normal, 48, 1, "%08X", msr);
		Print(CuiColor::Normal, 48, 2, "%08X", Jdi->GetFpscr());
		Print(CuiColor::Normal, 48, 4, "%08X", Jdi->GetSpr(Gekko::SPR::HID0));
		Print(CuiColor::Normal, 48, 5, "%08X", Jdi->GetSpr(Gekko::SPR::HID1));
		Print(CuiColor::Normal, 48, 6, "%08X", hid2);
		Print(CuiColor::Normal, 48, 8, "%08X", Jdi->GetSpr(Gekko::SPR::WPAR));
		Print(CuiColor::Normal, 48, 9, "%08X", Jdi->GetSpr(Gekko::SPR::DMAU));
		Print(CuiColor::Normal, 48, 10, "%08X", Jdi->GetSpr(Gekko::SPR::DMAL));

		Print(CuiColor::Normal, 64, 1, "%08X", Jdi->GetSpr(Gekko::SPR::DSISR));
		Print(CuiColor::Normal, 64, 2, "%08X", Jdi->GetSpr(Gekko::SPR::DAR));
		Print(CuiColor::Normal, 64, 4, "%08X", Jdi->GetSpr(Gekko::SPR::SRR0));
		Print(CuiColor::Normal, 64, 5, "%08X", Jdi->GetSpr(Gekko::SPR::SRR1));
		Print(CuiColor::Normal, 64, 8, "%08X", Jdi->GetSpr(Gekko::SPR::SPRG0));
		Print(CuiColor::Normal, 64, 9, "%08X", Jdi->GetSpr(Gekko::SPR::SPRG1));
		Print(CuiColor::Normal, 64, 10, "%08X", Jdi->GetSpr(Gekko::SPR::SPRG2));
		Print(CuiColor::Normal, 64, 11, "%08X", Jdi->GetSpr(Gekko::SPR::SPRG3));
		Print(CuiColor::Normal, 64, 13, "%08X", Jdi->GetSpr(Gekko::SPR::EAR));
		Print(CuiColor::Normal, 64, 14, "%08X", Jdi->GetSpr(Gekko::SPR::PVR));

		// Some cpu flags.

		Print(CuiColor::Cyan, 74, 1, "%s", (msr & MSR_PR) ? "UISA" : "OEA");       // Supervisor?
		Print(CuiColor::Cyan, 74, 2, "%s", (msr & MSR_EE) ? "EE" : "NE");          // Interrupts enabled?

		// Names

		Print(CuiColor::Cyan, 74, 4, "PSE ");
		Print(CuiColor::Cyan, 74, 5, "LSQ ");
		Print(CuiColor::Cyan, 74, 6, "WPE ");
		Print(CuiColor::Cyan, 74, 7, "LC  ");

		// Values

		Print(CuiColor::Normal, 78, 4, "%i", (hid2 & HID2_PSE) ? 1 : 0); // Paired Single mode?
		Print(CuiColor::Normal, 78, 5, "%i", (hid2 & HID2_LSQE) ? 1 : 0); // Load/Store Quantization?
		Print(CuiColor::Normal, 78, 6, "%i", (hid2 & HID2_WPE) ? 1 : 0); // Gather buffer?
		Print(CuiColor::Normal, 78, 7, "%i", (hid2 & HID2_LCE) ? 1 : 0); // Cache locked?
	}

	void DebugRegs::ShowFprs()
	{
		int y;

		for (y = 1; y <= 16; y++)
		{
			print_fpreg(0, y, y - 1);
			print_fpreg(39, y, y - 1 + 16);
		}
	}

	void DebugRegs::ShowPairedSingle()
	{
		int y;

		for (y = 1; y <= 16; y++)
		{
			print_ps(0, y, y - 1);
			print_ps(32, y, y - 1 + 16);
		}

		for (y = 1; y <= 8; y++)
		{
			uint32_t gqr = Jdi->GetSpr(Gekko::SPR::GQRs + y - 1);

			Print(CuiColor::Cyan, 64, y, "gqr%i ", y - 1);
			Print(CuiColor::Normal, 69, y, "%08X", gqr);
		}

		uint32_t hid2 = Jdi->GetSpr(Gekko::SPR::HID2);

		Print(CuiColor::Cyan, 64, 10, "PSE   ");
		Print(CuiColor::Cyan, 64, 11, "LSQ   ");

		Print(CuiColor::Normal, 70, 10, "%i", (hid2 & HID2_PSE) ? 1 : 0); // Paired Single mode?
		Print(CuiColor::Normal, 70, 11, "%i", (hid2 & HID2_LSQE) ? 1 : 0); // Load/Store Quantization?
	}

	void DebugRegs::ShowMmu()
	{
		// Names

		Print(CuiColor::Cyan, 0, 11, "sdr1  ");

		Print(CuiColor::Cyan, 0, 13, "IR    ");
		Print(CuiColor::Cyan, 0, 14, "DR    ");

		Print(CuiColor::Cyan, 0, 1, "dbat0 ");
		Print(CuiColor::Cyan, 0, 2, "dbat1 ");
		Print(CuiColor::Cyan, 0, 3, "dbat2 ");
		Print(CuiColor::Cyan, 0, 4, "dbat3 ");

		Print(CuiColor::Cyan, 0, 6, "ibat0 ");
		Print(CuiColor::Cyan, 0, 7, "ibat1 ");
		Print(CuiColor::Cyan, 0, 8, "ibat2 ");
		Print(CuiColor::Cyan, 0, 9, "ibat3 ");

		// Values

		Print(CuiColor::Normal, 6, 11, "%08X", Jdi->GetSpr(Gekko::SPR::SDR1));

		uint32_t msr = Jdi->GetMsr();

		Print(CuiColor::Normal, 6, 13, "%i", (msr & MSR_IR) ? 1 : 0);
		Print(CuiColor::Normal, 6, 14, "%i", (msr & MSR_DR) ? 1 : 0);

		Print(CuiColor::Normal, 6, 1, "%08X:%08X", Jdi->GetSpr(Gekko::SPR::DBAT0U), Jdi->GetSpr(Gekko::SPR::DBAT0L));
		Print(CuiColor::Normal, 6, 2, "%08X:%08X", Jdi->GetSpr(Gekko::SPR::DBAT1U), Jdi->GetSpr(Gekko::SPR::DBAT1L));
		Print(CuiColor::Normal, 6, 3, "%08X:%08X", Jdi->GetSpr(Gekko::SPR::DBAT2U), Jdi->GetSpr(Gekko::SPR::DBAT2L));
		Print(CuiColor::Normal, 6, 4, "%08X:%08X", Jdi->GetSpr(Gekko::SPR::DBAT3U), Jdi->GetSpr(Gekko::SPR::DBAT3L));

		Print(CuiColor::Normal, 6, 6, "%08X:%08X", Jdi->GetSpr(Gekko::SPR::IBAT0U), Jdi->GetSpr(Gekko::SPR::IBAT0L));
		Print(CuiColor::Normal, 6, 7, "%08X:%08X", Jdi->GetSpr(Gekko::SPR::IBAT1U), Jdi->GetSpr(Gekko::SPR::IBAT1L));
		Print(CuiColor::Normal, 6, 8, "%08X:%08X", Jdi->GetSpr(Gekko::SPR::IBAT2U), Jdi->GetSpr(Gekko::SPR::IBAT2L));
		Print(CuiColor::Normal, 6, 9, "%08X:%08X", Jdi->GetSpr(Gekko::SPR::IBAT3U), Jdi->GetSpr(Gekko::SPR::IBAT3L));

		// BATs detailed

		describe_bat_reg(24, 1, Jdi->GetSpr(Gekko::SPR::DBAT0U), Jdi->GetSpr(Gekko::SPR::DBAT0L), false);
		describe_bat_reg(24, 2, Jdi->GetSpr(Gekko::SPR::DBAT1U), Jdi->GetSpr(Gekko::SPR::DBAT1L), false);
		describe_bat_reg(24, 3, Jdi->GetSpr(Gekko::SPR::DBAT2U), Jdi->GetSpr(Gekko::SPR::DBAT2L), false);
		describe_bat_reg(24, 4, Jdi->GetSpr(Gekko::SPR::DBAT3U), Jdi->GetSpr(Gekko::SPR::DBAT3L), false);

		describe_bat_reg(24, 6, Jdi->GetSpr(Gekko::SPR::IBAT0U), Jdi->GetSpr(Gekko::SPR::IBAT0L), true);
		describe_bat_reg(24, 7, Jdi->GetSpr(Gekko::SPR::IBAT1U), Jdi->GetSpr(Gekko::SPR::IBAT1L), true);
		describe_bat_reg(24, 8, Jdi->GetSpr(Gekko::SPR::IBAT2U), Jdi->GetSpr(Gekko::SPR::IBAT2L), true);
		describe_bat_reg(24, 9, Jdi->GetSpr(Gekko::SPR::IBAT3U), Jdi->GetSpr(Gekko::SPR::IBAT3L), true);

		// Segment regs

		for (int n = 0, y = 1; n < 16; n++, y++)
		{
			uint32_t sr = Jdi->GetSr(n);
			CuiColor prefix = sr & 0x80000000 ? CuiColor::BrightRed : CuiColor::Normal;

			Print(CuiColor::Cyan, 64, y, "sr%-2i  ", n);
			Print(prefix, 70, y, "%08X", sr);
		}
	}

	void DebugRegs::RotateView(bool forward)
	{
		if (forward)
		{
			switch (mode)
			{
				case DebugRegmode::GPR: mode = DebugRegmode::FPR; break;
				case DebugRegmode::FPR: mode = DebugRegmode::PSR; break;
				case DebugRegmode::PSR: mode = DebugRegmode::MMU; break;
				case DebugRegmode::MMU: mode = DebugRegmode::DSP; break;
				case DebugRegmode::DSP: mode = DebugRegmode::GPR; break;
			}
		}
		else
		{
			switch (mode)
			{
				case DebugRegmode::GPR: mode = DebugRegmode::DSP; break;
				case DebugRegmode::FPR: mode = DebugRegmode::GPR; break;
				case DebugRegmode::PSR: mode = DebugRegmode::FPR; break;
				case DebugRegmode::MMU: mode = DebugRegmode::PSR; break;
				case DebugRegmode::DSP: mode = DebugRegmode::MMU; break;
			}
		}
	}

	void DebugRegs::print_gprreg(int x, int y, int num)
	{
		uint32_t value = Jdi->GetGpr(num);

		Print (CuiColor::Cyan, x, y, "%-3s ", gprnames[num]);

		if (value != savedGpr[num])
		{
			Print(CuiColor::Green, x + 4, y, "%08X", value);
			savedGpr[num] = value;
		}
		else
		{
			Print(CuiColor::Normal, x + 4, y, "%08X", value);
		}
	}

	void DebugRegs::print_fpreg(int x, int y, int num)
	{
		Fpreg value;

		value.Raw = Jdi->GetPs0(num);

		Print(CuiColor::Cyan, x, y, "f%-2i ", num);

		if (value.Raw != savedPs0[num].Raw)
		{
			if (value.Float >= 0.0) Print(CuiColor::Green, x + 4, y, " %e", value.Float);
			else Print(CuiColor::Green, x + 4, y, "%e", value.Float);

			Print(CuiColor::Green, x + 20, y, "%08X %08X", value.u.High, value.u.Low);

			savedPs0[num].Raw = value.Raw;
		}
		else
		{
			if (value.Float >= 0.0) Print(CuiColor::Normal, x + 4, y, " %e", value.Float);
			else Print(CuiColor::Normal, x + 4, y, "%e", value.Float);

			Print(CuiColor::Normal, x + 20, y, "%08X %08X", value.u.High, value.u.Low);
		}
	}

	void DebugRegs::print_ps(int x, int y, int num)
	{
		Fpreg ps0, ps1;

		ps0.Raw = Jdi->GetPs0(num);
		ps1.Raw = Jdi->GetPs1(num);

		Print(CuiColor::Cyan, x, y, "ps%-2i ", num);

		if (ps0.Raw != savedPs0[num].Raw)
		{
			if (ps0.Float >= 0.0f) Print(CuiColor::Green, x + 6, y, " %.4e", ps0.Float);
			else Print(CuiColor::Green, x + 6, y, "%.4e", ps0.Float);

			savedPs0[num].Raw = ps0.Raw;
		}
		else
		{
			if (ps0.Float >= 0.0f) Print(CuiColor::Normal, x + 6, y, " %.4e", ps0.Float);
			else Print(CuiColor::Normal, x + 6, y, "%.4e", ps0.Float);
		}

		if (ps1.Raw != savedPs1[num].Raw)
		{
			if (ps1.Float >= 0.0f) Print(CuiColor::Green, x + 18, y, " %.4e", ps1.Float);
			else Print(CuiColor::Green, x + 18, y, "%.4e", ps1.Float);

			savedPs1[num].Raw = ps1.Raw;
		}
		else
		{
			if (ps1.Float >= 0.0f) Print(CuiColor::Normal, x + 18, y, " %.4e", ps1.Float);
			else Print(CuiColor::Normal, x + 18, y, "%.4e", ps1.Float);
		}
	}

	int DebugRegs::cntlzw(uint32_t val)
	{
		int i;
		for (i = 0; i < 32; i++)
		{
			if (val & (1 << (31 - i))) break;
		}
		return ((i == 32) ? 31 : i);
	}

	void DebugRegs::describe_bat_reg(int x, int y, uint32_t up, uint32_t lo, bool instr)
	{
		// Use plain numbers, no definitions (for best compatibility).
		uint32_t bepi = (up >> 17) & 0x7fff;
		uint32_t bl = (up >> 2) & 0x7ff;
		uint32_t vs = (up >> 1) & 1;
		uint32_t vp = up & 1;
		uint32_t brpn = (lo >> 17) & 0x7fff;
		uint32_t w = (lo >> 6) & 1;
		uint32_t i = (lo >> 5) & 1;
		uint32_t m = (lo >> 4) & 1;
		uint32_t g = (lo >> 3) & 1;
		uint32_t pp = lo & 3;

		uint32_t EStart = bepi << 17, PStart = brpn << 17;
		uint32_t blkSize = 1 << (17 + 11 - cntlzw((bl << (32 - 11)) | 0x00100000));

		const char* ppstr = "NA";
		if (pp)
		{
			if (instr) { ppstr = ((pp & 1) ? (char*)("X") : (char*)("XW")); }
			else { ppstr = ((pp & 1) ? (char*)("R") : (char*)("RW")); }
		}

		char temp[0x100];

		sprintf(temp, "%08X->%08X" " %-6s" " %c%c%c%c" " %s %s" " %s",
			EStart, PStart, smart_size(blkSize).c_str(),
			w ? 'W' : '-',
			i ? 'I' : '-',
			m ? 'M' : '-',
			g ? 'G' : '-',
			vs ? "Vs" : "Ns",
			vp ? "Vp" : "Np",
			ppstr);

		Print(CuiColor::Normal, x, y, temp);
	}

	std::string DebugRegs::smart_size(size_t size)
	{
		char tempBuf[0x20] = { 0, };

		if (size < 1024)
		{
			sprintf(tempBuf, "%zi byte", size);
		}
		else if (size < 1024 * 1024)
		{
			sprintf(tempBuf, "%zi KB", size / 1024);
		}
		else if (size < 1024 * 1024 * 1024)
		{
			sprintf(tempBuf, "%zi MB", size / 1024 / 1024);
		}
		else
		{
			sprintf(tempBuf, "%1.1f GB", (float)size / 1024 / 1024 / 1024);
		}

		return tempBuf;
	}

	void DebugRegs::ShowDspRegs()
	{
		// If GameCube is not powered on

		if (!Jdi->IsLoaded())
		{
			return;
		}

		// Registers with changes

		DspDrawRegs();

		// 40-bit regs overview

		Print(CuiColor::Black, CuiColor::Normal, 24, 1, "a: %02X_%04X_%04X",
			(uint8_t)Jdi->DspGetReg((size_t)DspReg::a2),
			Jdi->DspGetReg((size_t)DspReg::a1),
			Jdi->DspGetReg((size_t)DspReg::a0));
		Print(CuiColor::Black, CuiColor::Normal, 24, 2, "b: %02X_%04X_%04X",
			(uint8_t)Jdi->DspGetReg((size_t)DspReg::b2),
			Jdi->DspGetReg((size_t)DspReg::b1),
			Jdi->DspGetReg((size_t)DspReg::b0));

		Print(CuiColor::Black, CuiColor::Normal, 24, 3, "x: %04X_%04X",
			Jdi->DspGetReg((size_t)DspReg::x1),
			Jdi->DspGetReg((size_t)DspReg::x0));
		Print(CuiColor::Black, CuiColor::Normal, 24, 4, "y: %04X_%04X",
			Jdi->DspGetReg((size_t)DspReg::y1),
			Jdi->DspGetReg((size_t)DspReg::y0));

		uint64_t bitsPacked = Jdi->DspPackProd();
		Print(CuiColor::Black, CuiColor::Normal, 24, 5, "p: %02X_%04X_%04X",
			(uint8_t)(bitsPacked >> 32),
			(uint16_t)(bitsPacked >> 16),
			(uint16_t)bitsPacked);

		// Program Counter

		Print(CuiColor::Black, CuiColor::Normal, 24, 7, "pc: %04X", Jdi->DspGetPc());

		// Status as individual bits

		DspDrawStatusBits();

		DspMemorize();
	}

	void DebugRegs::DspDrawRegs()
	{
		for (int i = 0; i < 16; i++)
		{
			DspPrintReg(0, i + 1, (DspReg)(0 + i));
		}

		for (int i = 0; i < 16; i++)
		{
			DspPrintReg(12, i + 1, (DspReg)(16 + i));
		}
	}

	void DebugRegs::DspDrawStatusBits()
	{
		for (int i = 0; i < 16; i++)
		{
			DspPrintPsrBit(42, i + 1, i);
		}
	}

	void DebugRegs::DspPrintReg(int x, int y, DspReg n)
	{
		uint16_t value = Jdi->DspGetReg((size_t)n);
		bool same = savedDspRegs[(size_t)n] == value;

		Print( !same ? CuiColor::Lime : CuiColor::Normal,
			x, y, "%-3s: %04X", DspRegNames[(size_t)n], value);
	}

	void DebugRegs::DspPrintPsrBit(int x, int y, int n)
	{
		uint16_t mask = (1 << n);
		uint16_t psr = Jdi->DspGetPsr();
		bool same = (savedDspPsr & mask) == (psr & mask);

		Print( !same ? CuiColor::Lime : CuiColor::Normal,
			x, y, "%-3s: %i", DspPsrBitNames[n], (psr & mask) ? 1 : 0);
	}

	void DebugRegs::DspMemorize()
	{
		for (size_t i = 0; i < 32; i++)
		{
			savedDspRegs[i] = Jdi->DspGetReg(i);
		}
		savedDspPsr = Jdi->DspGetPsr();
	}

}


// Generic memory viewer (HexBox)

namespace Debug
{

	MemoryView::MemoryView(CuiRect& rect, std::string name, Cui* parent)
		: CuiWindow (rect, name, parent)
	{
	}

	MemoryView::~MemoryView()
	{
	}

	void MemoryView::OnDraw()
	{
		FillLine(CuiColor::Cyan, CuiColor::White, 0, ' ');
		std::string head = "[ ] F2";
		Print(CuiColor::Cyan, CuiColor::Black, 1, 0, head);
		if (IsActive())
		{
			Print(CuiColor::Cyan, CuiColor::White, 2, 0, "*");
		}

		char hint[0x100] = { 0, };
		uint32_t cursor = 0;

		switch (mode)
		{
			case MemoryViewMode::GekkoVirtual:
			{
				cursor = vm_cursor;
				sprintf(hint, " Gekko virtual memory, addr:0x%08X", Jdi->VirtualToPhysicalDMmu(cursor));
				Print(CuiColor::Cyan, CuiColor::Black, (int)(head.size() + 3), 0, hint);
			}
			break;
			case MemoryViewMode::GekkoDataCache: 
				cursor = dcache_cursor;
				sprintf(hint, " Gekko data cache, addr:0x%08X", cursor);
				Print(CuiColor::Cyan, CuiColor::Black, (int)(head.size() + 3), 0, hint);
				break;
			case MemoryViewMode::MainMem:
				cursor = mmem_cursor;
				sprintf(hint, " Main memory (1T-SRAM), addr:0x%08X", cursor);
				Print(CuiColor::Cyan, CuiColor::Black, (int)(head.size() + 3), 0, hint);
				break;
			case MemoryViewMode::DSP_ARAM:
				cursor = aram_cursor;
				sprintf(hint, " ARAM, addr:0x%08X", cursor);
				Print(CuiColor::Cyan, CuiColor::Black, (int)(head.size() + 3), 0, hint);
				break;
			case MemoryViewMode::DSP_DMEM:
				cursor = dmem_current;
				sprintf(hint, " DMEM, addr:0x%08X", cursor);
				Print(CuiColor::Cyan, CuiColor::Black, (int)(head.size() + 3), 0, hint);
				break;
		}

		if (mode == MemoryViewMode::DSP_DMEM) {
			DspDmemOnDraw();
			return;
		}

		// Hexview

		for (int row = 0; row < height - 1; row++)
		{
			FillLine(CuiColor::Black, CuiColor::Normal, row + 1, ' ');
			Print(CuiColor::Normal, 0, row + 1, "%08X", cursor + row * 16);

			for (int col = 0; col < 8; col++)
			{
				Print(CuiColor::Normal, 10 + col * 3, row + 1, hexbyte(cursor + row * 16 + col));
			}

			for (int col = 0; col < 8; col++)
			{
				Print(CuiColor::Normal, 35 + col * 3, row + 1, hexbyte(cursor + row * 16 + col + 8));
			}

			for (int col = 0; col < 16; col++)
			{
				Print(CuiColor::Normal, 60 + col, row + 1, "%c", charbyte(cursor + row * 16 + col));
			}
		}
	}

	void MemoryView::OnKeyPress(char Ascii, CuiVkey Vkey, bool shift, bool ctrl)
	{
		uint32_t* cursor = nullptr;
		uint32_t top = 0;
		uint32_t bottom = 0;

		if (mode == MemoryViewMode::DSP_DMEM) {
			DspDmemOnKeyPress(Ascii, Vkey, shift, ctrl);
			return;
		}

		switch (mode)
		{
			case MemoryViewMode::GekkoVirtual:
				cursor = &vm_cursor;
				top = 0x8000'0000;
				bottom = 0x8000'0000 | RAMSIZE;
				break;
			case MemoryViewMode::GekkoDataCache: 
				cursor = &dcache_cursor;
				top = 0;
				bottom = RAMSIZE;
				break;
			case MemoryViewMode::MainMem:
				cursor = &mmem_cursor;
				top = 0;
				bottom = RAMSIZE;
				break;
			case MemoryViewMode::DSP_ARAM: 
				cursor = &aram_cursor;
				top = 0;
				bottom = ARAMSIZE;
				break;
		}

		switch (Vkey)
		{
			case CuiVkey::Home:
				if (cursor != nullptr)
					*cursor = top;
				break;
			case CuiVkey::End:
				if (cursor != nullptr)
					*cursor = bottom - (uint32_t)((height - 1) * 16);
				break;
			case CuiVkey::PageDown:
				if (cursor != nullptr)
					*cursor += (uint32_t)((height - 1) * 16);
				break;
			case CuiVkey::PageUp:
				if (cursor != nullptr)
					*cursor -= (uint32_t)((height - 1) * 16);
				break;
			case CuiVkey::Up:
				if (cursor != nullptr)
					*cursor -= 16;
				break;
			case CuiVkey::Down:
				if (cursor != nullptr)
					*cursor += 16;
				break;
			case CuiVkey::Left:
				RotateView(false);
				break;
			case CuiVkey::Right:
				RotateView(true);
				break;
		}

		Invalidate();
	}


	void MemoryView::RotateView(bool forward)
	{
		if (forward)
		{
			switch (mode)
			{
				case MemoryViewMode::GekkoVirtual: mode = MemoryViewMode::GekkoDataCache; break;
				case MemoryViewMode::GekkoDataCache: mode = MemoryViewMode::MainMem; break;
				case MemoryViewMode::MainMem: mode = MemoryViewMode::DSP_ARAM; break;
				case MemoryViewMode::DSP_ARAM: mode = MemoryViewMode::DSP_DMEM; break;
				case MemoryViewMode::DSP_DMEM: mode = MemoryViewMode::GekkoVirtual; break;
			}
		}
		else
		{
			switch (mode)
			{
				case MemoryViewMode::GekkoVirtual: mode = MemoryViewMode::DSP_DMEM; break;
				case MemoryViewMode::GekkoDataCache: mode = MemoryViewMode::GekkoVirtual; break;
				case MemoryViewMode::MainMem: mode = MemoryViewMode::GekkoDataCache; break;
				case MemoryViewMode::DSP_ARAM: mode = MemoryViewMode::MainMem; break;
				case MemoryViewMode::DSP_DMEM: mode = MemoryViewMode::DSP_ARAM; break;
			}
		}
	}

	void MemoryView::SetCursor(uint32_t address)
	{
		switch (mode)
		{
			case MemoryViewMode::GekkoVirtual:
				vm_cursor = address;
				break;
			case MemoryViewMode::GekkoDataCache: 
				dcache_cursor = address;
				break;
			case MemoryViewMode::MainMem:
				mmem_cursor = address;
				break;
			case MemoryViewMode::DSP_ARAM: 
				aram_cursor = address;
				break;
			case MemoryViewMode::DSP_DMEM:
				dmem_current = address;
				break;
		}

		Invalidate();
	}

	std::string MemoryView::hexbyte(uint32_t addr)
	{
		uint8_t* ptr = nullptr;

		switch (mode)
		{
			case MemoryViewMode::GekkoVirtual:
				ptr = (uint8_t*)Jdi->TranslateDMmu(addr);
				break;
			case MemoryViewMode::GekkoDataCache: 
				ptr = Core->GetDataCachePointer(addr);
				break;
			case MemoryViewMode::MainMem:
				ptr = MITranslatePhysicalAddress(addr, 1);
				break;
			case MemoryViewMode::DSP_ARAM: 
				ptr = Jdi->IsLoaded() ? &ARAM[addr] : nullptr;
				break;
		}

		if (ptr)
		{
			char buf[0x10];
			sprintf(buf, "%02X", *ptr);
			return buf;
		}
		else
		{
			return "??";
		}
	}

	char MemoryView::charbyte(uint32_t addr)
	{
		uint8_t* ptr = nullptr;

		switch (mode)
		{
			case MemoryViewMode::GekkoVirtual:
				ptr = (uint8_t*)Jdi->TranslateDMmu(addr);
				break;
			case MemoryViewMode::GekkoDataCache: 
				ptr = Core->GetDataCachePointer(addr);
				break;
			case MemoryViewMode::MainMem:
				ptr = MITranslatePhysicalAddress(addr, 1);
				break;
			case MemoryViewMode::DSP_ARAM: 
				ptr = Jdi->IsLoaded() ? &ARAM[addr] : nullptr;
				break;
		}

		if (ptr)
		{
			uint8_t data = *ptr;
			if ((data >= 32) && (data <= 255)) return (char)data;
			else return '.';
		}
		else
		{
			return '?';
		}
	}

	// The memory view integrated from DSP Debug is a bit different from the usual hexbyte / charbyte view

	void MemoryView::DspDmemOnDraw()
	{
		// If GameCube is not powered on

		if (!Jdi->IsLoaded())
		{
			return;
		}

		// Do not forget that DSP addressing is done in 16-bit words.

		size_t lines = height - 1;
		uint32_t addr = dmem_current;
		int y = 1;

		while (lines--)
		{
			char text[0x100];

			uint16_t* ptr = (uint16_t*) Jdi->DspTranslateDMem(addr);
			if (!ptr)
			{
				addr += 8;
				y++;
				continue;
			}

			FillLine(CuiColor::Black, CuiColor::Normal, y, ' ');

			// Address

			sprintf(text, "%04X: ", addr);
			Print(CuiColor::Black, CuiColor::Normal, 0, y, text);

			// Raw Words

			int x = 6;

			for (size_t i = 0; i < 8; i++)
			{
				uint16_t word = _BYTESWAP_UINT16(ptr[i]);
				sprintf(text, "%04X ", word);
				Print(CuiColor::Black, CuiColor::Normal, x, y, text);
				x += 5;
			}

			addr += 8;
			y++;
		}
	}

	void MemoryView::DspDmemOnKeyPress(char Ascii, CuiVkey Vkey, bool shift, bool ctrl)
	{
		size_t lines = height - 1;

		switch (Vkey)
		{
			case CuiVkey::Up:
				if (dmem_current >= 8)
					dmem_current -= 8;
				break;

			case CuiVkey::Down:
				dmem_current += 8;
				if (dmem_current > 0x3000)
					dmem_current = 0x3000;
				break;

			case CuiVkey::PageUp:
				if (dmem_current < lines * 8)
					dmem_current = 0;
				else
					dmem_current -= (uint32_t)(lines * 8);
				break;

			case CuiVkey::PageDown:
				dmem_current += (uint32_t)(lines * 8);
				if (dmem_current > 0x3000)
					dmem_current = 0x3000;
				break;

			case CuiVkey::Home:
				dmem_current = 0;
				break;

			case CuiVkey::End:
				dmem_current = DROM_START_ADDRESS;
				break;

			case CuiVkey::Left:
				RotateView(false);
				break;
			case CuiVkey::Right:
				RotateView(true);
				break;
		}

		Invalidate();
	}

}


namespace Debug
{
	Json::Value* CmdShowMemory(std::vector<std::string>& args)
	{
		if (debugger)
		{
			debugger->SetMemoryCursor(strtoul(args[1].c_str(), nullptr, 0));
		}
		return nullptr;
	}

	Json::Value* CmdShowDisassembly(std::vector<std::string>& args)
	{
		if (debugger)
		{
			debugger->SetDisasmCursor(strtoul(args[1].c_str(), nullptr, 0));
		}
		return nullptr;
	}

	void DebugUIReflector()
	{
		JdiAddCmd("d", CmdShowMemory);
		JdiAddCmd("u", CmdShowDisassembly);
	}
}
