#include "pch.h"

// Displaying message history. It can be used both by the system debugger and as part of the DSP Debugger.

namespace Debug
{
	// Make it global so that the message history is saved for the entire lifetime of the application.
	static std::vector<std::pair<CuiColor, std::string>> history;

	ReportWindow::ReportWindow(RECT& rect, std::string name, Cui* parent)
		: CuiWindow(rect, name, parent)
	{
		thread = new Thread(ThreadProc, false, this, "ReportWindow");

		if (history.empty())
		{
			Jdi->Report("Debugger is running. Type help for quick reference.\n");
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

// Status window, to display the current state of the debugger

namespace Debug
{
	StatusWindow::StatusWindow(RECT& rect, std::string name, Cui* parent)
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

	void StatusWindow::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
	}

	void StatusWindow::SetMode(DebugMode mode)
	{
		_mode = mode;
		Invalidate();
	}

}

// Command line. All commands go to Jdi->
// It can be used both by the system debugger and as part of the DSP Debugger.

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

		sprintf_s(cmd, sizeof(cmd), "GetChannelName %i", chan);

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
		sprintf_s(cmd, sizeof(cmd), "GetGpr %zi", n);

		Json::Value* output = CallJdi(cmd);

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	uint64_t JdiClient::GetPs0(size_t n)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "GetPs0 %zi", n);

		Json::Value* output = CallJdi(cmd);

		uint64_t value = output->value.AsInt;

		delete output;

		return value;
	}

	uint64_t JdiClient::GetPs1(size_t n)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "GetPs1 %zi", n);

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
		sprintf_s(cmd, sizeof(cmd), "GetSpr %zi", n);

		Json::Value* output = CallJdi(cmd);

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	uint32_t JdiClient::GetSr(size_t n)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "GetSr %zi", n);

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
		sprintf_s(cmd, sizeof(cmd), "TranslateDMmu 0x%08X", address);

		Json::Value* output = CallJdi(cmd);

		void* ptr = (void *)output->value.AsInt;

		delete output;

		return ptr;
	}

	void* JdiClient::TranslateIMmu(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "TranslateIMmu 0x%08X", address);

		Json::Value* output = CallJdi(cmd);

		void* ptr = (void*)output->value.AsInt;

		delete output;

		return ptr;
	}

	uint32_t JdiClient::VirtualToPhysicalDMmu(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "VirtualToPhysicalDMmu 0x%08X", address);

		Json::Value* output = CallJdi(cmd);

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	uint32_t JdiClient::VirtualToPhysicalIMmu(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "VirtualToPhysicalIMmu 0x%08X", address);

		Json::Value* output = CallJdi(cmd);

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	bool JdiClient::GekkoTestBreakpoint(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "GekkoTestBreakpoint 0x%08X", address);

		Json::Value* output = CallJdi(cmd);

		bool value = output->value.AsBool;

		delete output;

		return value;
	}

	void JdiClient::GekkoToggleBreakpoint(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "GekkoToggleBreakpoint 0x%08X", address);

		Json::Value* output = CallJdi(cmd);
		delete output;
	}

	void JdiClient::GekkoAddOneShotBreakpoint(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "GekkoAddOneShotBreakpoint 0x%08X", address);

		Json::Value* output = CallJdi(cmd);
		delete output;
	}

	std::string JdiClient::GekkoDisasm(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "GekkoDisasm 0x%08X", address);

		char text[0x200];

		CallJdiReturnString(cmd, text, sizeof(text) - 1);

		return text;
	}

	bool JdiClient::GekkoIsBranch(uint32_t address, uint32_t& targetAddress)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "GekkoIsBranch 0x%08X", address);

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
		sprintf_s(cmd, sizeof(cmd), "AddressByName %s", name.c_str());

		Json::Value* output = CallJdi(cmd);

		uint32_t value = output->value.AsUint32;

		delete output;

		return value;
	}

	std::string JdiClient::NameByAddress(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "NameByAddress 0x%08X", address);

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
		sprintf_s(cmd, sizeof(cmd), "DspGetReg %zi", n);

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
		sprintf_s(cmd, sizeof(cmd), "DspTranslateDMem 0x%04X", (uint16_t)address);

		Json::Value* output = CallJdi(cmd);

		void* ptr = (void*)output->value.AsInt;

		delete output;

		return ptr;
	}

	void* JdiClient::DspTranslateIMem(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "DspTranslateIMem 0x%04X", (uint16_t)address);

		Json::Value* output = CallJdi(cmd);

		void* ptr = (void*)output->value.AsInt;

		delete output;

		return ptr;
	}

	bool JdiClient::DspTestBreakpoint(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "DspTestBreakpoint 0x%04X", (uint16_t)address);

		Json::Value* output = CallJdi(cmd);

		bool value = output->value.AsBool;

		delete output;

		return value;
	}

	void JdiClient::DspToggleBreakpoint(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "DspToggleBreakpoint 0x%04X", (uint16_t)address);

		Json::Value* output = CallJdi(cmd);
		delete output;
	}

	void JdiClient::DspAddOneShotBreakpoint(uint32_t address)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "DspAddOneShotBreakpoint 0x%04X", (uint16_t)address);

		Json::Value* output = CallJdi(cmd);
		delete output;
	}

	std::string JdiClient::DspDisasm(uint32_t address, size_t& instrSizeWords, bool& flowControl)
	{
		char cmd[0x40];
		sprintf_s(cmd, sizeof(cmd), "DspDisasm 0x%04X", (uint16_t)address);

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
		sprintf_s(cmd, sizeof(cmd), "DspIsCall 0x%04X", (uint16_t)address);

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
		sprintf_s(cmd, sizeof(cmd), "DspIsCallOrJump 0x%04X", (uint16_t)address);

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

// Visual DSP Debugger

namespace Debug
{
	DspDebug::DspDebug() :
		Cui("DSP Debug", width, height)
	{
		// Create an interface for communicating with the emulator core, if it has not been created yet.

		if (!Jdi)
		{
			Jdi = new JdiClient;
		}

		RECT rect;

		rect.left = 0;
		rect.top = 0;
		rect.right = 79;
		rect.bottom = 8;
		AddWindow(new DspRegs(rect, "DspRegs", this));

		rect.left = 0;
		rect.top = 9;
		rect.right = 79;
		rect.bottom = 17;
		AddWindow(new DspDmem(rect, "DspDmem", this));

		rect.left = 0;
		rect.top = 18;
		rect.right = 79;
		rect.bottom = 49;
		imemWindow = new DspImem(rect, "DspImem", this);
		AddWindow(imemWindow);

		rect.left = 0;
		rect.top = 50;
		rect.right = 79;
		rect.bottom = height - 2;
		AddWindow(new ReportWindow(rect, "ReportWindow", this));

		rect.left = 0;
		rect.top = height - 1;
		rect.right = 79;
		rect.bottom = height - 1;
		cmdline = new CmdlineWindow(rect, "Cmdline", this);
		AddWindow(cmdline);

		SetWindowFocus("DspImem");
	}

	void DspDebug::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		uint32_t targetAddress = 0;

		if ((Vkey == 0x8 || (Ascii >= 0x20 && Ascii < 256)) && !cmdline->IsActive())
		{
			SetWindowFocus("Cmdline");
			InvalidateAll();
			return;
		}

		switch (Vkey)
		{
			case VK_F1:
				SetWindowFocus("DspRegs");
				InvalidateAll();
				break;
			case VK_F2:
				SetWindowFocus("DspDmem");
				InvalidateAll();
				break;
			case VK_F3:
				SetWindowFocus("DspImem");
				InvalidateAll();
				break;
			case VK_F4:
				SetWindowFocus("ReportWindow");
				InvalidateAll();
				break;

			case VK_F5:
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
				break;

			case VK_F10:
				// Step Over
				if (!Jdi->DspIsRunning())
				{
					if (Jdi->DspIsCall(Jdi->DspGetPc(), targetAddress))
					{
						Jdi->DspAddOneShotBreakpoint(Jdi->DspGetPc() + 2);
						Jdi->DspRun();
					}
					else
					{
						Jdi->DspStep();
						if (!imemWindow->AddressVisible(Jdi->DspGetPc()))
						{
							imemWindow->current = imemWindow->cursor = Jdi->DspGetPc();
						}
					}
				}
				break;

			case VK_F11:
				// Step Into
				Jdi->DspStep();
				if (!imemWindow->AddressVisible(Jdi->DspGetPc()))
				{
					imemWindow->current = imemWindow->cursor = Jdi->DspGetPc();
				}
				break;
		}

		InvalidateAll();
	}

}

namespace Debug
{

	DspDmem::DspDmem(RECT& rect, std::string name, Cui* parent) :
		CuiWindow(rect, name, parent)
	{
	}

	void DspDmem::OnDraw()
	{
		Fill(CuiColor::Black, CuiColor::Normal, ' ');
		FillLine(CuiColor::Cyan, CuiColor::Black, 0, ' ');
		Print(CuiColor::Cyan, CuiColor::Black, 1, 0, "F2 - DMEM");

		if (active)
		{
			Print(CuiColor::Cyan, CuiColor::White, 0, 0, "*");
		}

		// If GameCube is not powered on

		if (!Jdi->IsLoaded())
		{
			return;
		}

		// Do not forget that DSP addressing is done in 16-bit words.

		size_t lines = height - 1;
		uint32_t addr = current;
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

			// Address

			sprintf_s(text, sizeof(text) - 1, "%04X: ", addr);
			Print(CuiColor::Black, CuiColor::Normal, 0, y, text);

			// Raw Words

			int x = 6;

			for (size_t i = 0; i < 8; i++)
			{
				uint16_t word = _byteswap_ushort(ptr[i]);
				sprintf_s(text, sizeof(text) - 1, "%04X ", word);
				Print(CuiColor::Black, CuiColor::Normal, x, y, text);
				x += 5;
			}

			addr += 8;
			y++;
		}
	}

	void DspDmem::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		size_t lines = height - 1;

		if (!Jdi->IsLoaded())
		{
			return;
		}

		switch (Vkey)
		{
			case VK_UP:
				if (current >= 8)
					current -= 8;
				break;

			case VK_DOWN:
				current += 8;
				if (current > 0x3000)
					current = 0x3000;
				break;

			case VK_PRIOR:
				if (current < lines * 8)
					current = 0;
				else
					current -= (uint32_t)(lines * 8);
				break;

			case VK_NEXT:
				current += (uint32_t)(lines * 8);
				if (current > 0x3000)
					current = 0x3000;
				break;

			case VK_HOME:
				current = 0;
				break;

			case VK_END:
				current = DROM_START_ADDRESS;
				break;
		}

		Invalidate();
	}

}

namespace Debug
{

	DspImem::DspImem(RECT& rect, std::string name, Cui* parent) :
		CuiWindow(rect, name, parent)
	{
	}

	void DspImem::OnDraw()
	{
		Fill(CuiColor::Black, CuiColor::Normal, ' ');
		FillLine(CuiColor::Cyan, CuiColor::Black, 0, ' ');
		Print(CuiColor::Cyan, CuiColor::Black, 1, 0, "F3 - IMEM");

		if (active)
		{
			Print(CuiColor::Cyan, CuiColor::White, 0, 0, "*");
		}

		// If GameCube is not powered on

		if (!Jdi->IsLoaded())
		{
			return;
		}

		// Show Dsp disassembly

		size_t lines = height - 1;
		uint32_t addr = current;
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

			int bgcur = (addr == cursor) ? ((int)CuiColor::Blue) : (0);
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

	void DspImem::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		uint32_t targetAddress = 0;

		if (!Jdi->IsLoaded())
		{
			return;
		}

		switch (Vkey)
		{
			case VK_UP:
				if (AddressVisible(cursor))
				{
					if (cursor > 0)
						cursor--;
				}
				else
				{
					cursor = (uint32_t)(current - wordsOnScreen);
				}
				if (!AddressVisible(cursor))
				{
					if (current < (height - 1))
						current = 0;
					else
						current -= (uint32_t)(height - 1);
				}
				break;

			case VK_DOWN:
				if (AddressVisible(cursor))
					cursor++;
				else
					cursor = current;
				if (!AddressVisible(cursor))
				{
					current = cursor;
				}
				break;

			case VK_PRIOR:
				if (current < (height - 1))
					current = 0;
				else
					current -= (uint32_t)(height - 1);
				break;

			case VK_NEXT:
				current += (uint32_t)(wordsOnScreen ? wordsOnScreen : height - 1);
				if (current >= 0x8A00)
					current = 0x8A00;
				break;

			case VK_HOME:
				if (ctrl)
				{
					current = cursor = 0;
				}
				else
				{
					current = cursor = Jdi->DspGetPc();
				}
				break;

			case VK_END:
				current = IROM_START_ADDRESS;
				break;

			case VK_F9:
				if (AddressVisible(cursor))
				{
					Jdi->DspToggleBreakpoint(cursor);
				}
				break;

			case VK_RETURN:
				if (Jdi->DspIsCallOrJump(cursor, targetAddress))
				{
					std::pair<uint32_t, uint32_t> last(current, cursor);
					browseHist.push_back(last);
					current = cursor = targetAddress;
				}
				break;

			case VK_ESCAPE:
				if (browseHist.size() > 0)
				{
					std::pair<uint32_t, uint32_t> last = browseHist.back();
					current = last.first;
					cursor = last.second;
					browseHist.pop_back();
				}
				break;
		}

		Invalidate();
	}

	bool DspImem::AddressVisible(uint32_t address)
	{
		if (!wordsOnScreen)
			return false;

		return (current <= address && address < (current + wordsOnScreen));
	}

}

namespace Debug
{
	static const char* RegNames[] = {
		"r0", "r1", "r2", "r3",
		"m0", "m1", "m2", "m3",
		"l0", "l1", "l2", "l3",
		"pcs", "pss", "eas", "lcs",
		"a2", "b2", "dpp", "psr",
		"ps0", "ps1", "ps2", "pc1",
		"x0", "y0", "x1", "y1",
		"a0", "b0", "a1", "b1"
	};

	static const char* PsrBitNames[] = {
		"c", "v", "z", "n", "e", "u", "tb", "sv",
		"te0", "te1", "te2", "te3", "et", "im", "xl", "dp",
	};

	DspRegs::DspRegs(RECT& rect, std::string name, Cui* parent) :
		CuiWindow(rect, name, parent)
	{
		Memorize();
	}

	void DspRegs::OnDraw()
	{
		FillLine(CuiColor::Cyan, CuiColor::Black, 0, ' ');
		Print(CuiColor::Cyan, CuiColor::Black, 1, 0, "F1 - Regs");

		if (active)
		{
			Print(CuiColor::Cyan, CuiColor::White, 0, 0, "*");
		}

		// If GameCube is not powered on

		if (!Jdi->IsLoaded())
		{
			return;
		}

		// DSP Run State

		if (Jdi->DspIsRunning())
		{
			Print(CuiColor::Cyan, CuiColor::Lime, 73, 0, "Running");
		}
		else
		{
			Print(CuiColor::Cyan, CuiColor::Red, 70, 0, "Suspended");
		}

		// Registers with changes

		DrawRegs();

		// 40-bit regs overview

		Print(CuiColor::Black, CuiColor::Normal, 48, 1, "a: %02X_%04X_%04X",
			(uint8_t)Jdi->DspGetReg((size_t)DspReg::a2),
			Jdi->DspGetReg((size_t)DspReg::a1),
			Jdi->DspGetReg((size_t)DspReg::a0));
		Print(CuiColor::Black, CuiColor::Normal, 48, 2, "b: %02X_%04X_%04X",
			(uint8_t)Jdi->DspGetReg((size_t)DspReg::b2),
			Jdi->DspGetReg((size_t)DspReg::b1),
			Jdi->DspGetReg((size_t)DspReg::b0));

		Print(CuiColor::Black, CuiColor::Normal, 48, 3, "x: %04X_%04X",
			Jdi->DspGetReg((size_t)DspReg::x1),
			Jdi->DspGetReg((size_t)DspReg::x0));
		Print(CuiColor::Black, CuiColor::Normal, 48, 4, "y: %04X_%04X",
			Jdi->DspGetReg((size_t)DspReg::y1),
			Jdi->DspGetReg((size_t)DspReg::y0));

		uint64_t bitsPacked = Jdi->DspPackProd();
		Print(CuiColor::Black, CuiColor::Normal, 48, 5, "p: %02X_%04X_%04X",
			(uint8_t)(bitsPacked >> 32),
			(uint16_t)(bitsPacked >> 16),
			(uint16_t)bitsPacked);

		// Program Counter

		Print(CuiColor::Black, CuiColor::Normal, 48, 7, "pc: %04X", Jdi->DspGetPc());

		// Status as individual bits

		DrawStatusBits();

		Memorize();
	}

	void DspRegs::DrawRegs()
	{
		for (int i = 0; i < 8; i++)
		{
			PrintReg(0, i + 1, (DspReg)i);
		}

		for (int i = 0; i < 8; i++)
		{
			PrintReg(12, i + 1, (DspReg)(8 + i));
		}

		for (int i = 0; i < 8; i++)
		{
			PrintReg(24, i + 1, (DspReg)(16 + i));
		}

		for (int i = 0; i < 8; i++)
		{
			PrintReg(36, i + 1, (DspReg)(24 + i));
		}
	}

	void DspRegs::DrawStatusBits()
	{
		for (int i = 0; i < 8; i++)
		{
			PrintPsrBit(66, i + 1, i);
		}

		for (int i = 0; i < 8; i++)
		{
			PrintPsrBit(73, i + 1, 8 + i);
		}
	}

	void DspRegs::PrintReg(int x, int y, DspReg n)
	{
		uint16_t value = Jdi->DspGetReg((size_t)n);
		bool same = savedRegs[(size_t)n] == value;

		Print( !same ? CuiColor::Lime : CuiColor::Normal,
			x, y, "%-3s: %04X", RegNames[(size_t)n], value);
	}

	void DspRegs::PrintPsrBit(int x, int y, int n)
	{
		uint16_t mask = (1 << n);
		uint16_t psr = Jdi->DspGetPsr();
		bool same = (savedPsr & mask) == (psr & mask);

		Print( !same ? CuiColor::Lime : CuiColor::Normal,
			x, y, "%-3s: %i", PsrBitNames[n], (psr & mask) ? 1 : 0);
	}

	void DspRegs::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		Invalidate();
	}

	void DspRegs::Memorize()
	{
		for (size_t i = 0; i < 32; i++)
		{
			savedRegs[i] = Jdi->DspGetReg(i);
		}
		savedPsr = Jdi->DspGetPsr();
	}

}

// System-wide debugger

namespace Debug
{
	GekkoDebug::GekkoDebug()
		: Cui ("Debug Console", width, height)
	{
		RECT rect;

		// Create an interface for communicating with the emulator core, if it has not been created yet.

		if (!Jdi)
		{
			Jdi = new JdiClient;
		}

		// Gekko registers

		rect.left = 0;
		rect.top = 0;
		rect.right = width;
		rect.bottom = regsHeight - 1;

		regs = new GekkoRegs(rect, "GekkoRegs", this);

		AddWindow(regs);

		// Flipper main memory hexview

		rect.left = 0;
		rect.top = regsHeight;
		rect.right = width;
		rect.bottom = regsHeight + memViewHeight - 1;

		memview = new MemoryView(rect, "MemoryView", this);

		AddWindow(memview);

		// Gekko disasm

		rect.left = 0;
		rect.top = regsHeight + memViewHeight;
		rect.right = width;
		rect.bottom = regsHeight + memViewHeight + disaHeight - 1;

		disasm = new GekkoDisasm(rect, "GekkoDisasm", this);

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

	void GekkoDebug::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		if ((Vkey == 0x8 || (Ascii >= 0x20 && Ascii < 256)) && !cmdline->IsActive())
		{
			SetWindowFocus("Cmdline");
			status->SetMode(DebugMode::Ready);
			InvalidateAll();
			return;
		}

		switch (Vkey)
		{
			case VK_F1:
				SetWindowFocus("GekkoRegs");
				InvalidateAll();
				break;

			case VK_F2:
				SetWindowFocus("MemoryView");
				InvalidateAll();
				break;

			case VK_F3:
				SetWindowFocus("GekkoDisasm");
				InvalidateAll();
				break;

			case VK_F4:
				SetWindowFocus("ReportWindow");
				status->SetMode(DebugMode::Scrolling);
				InvalidateAll();
				break;

			case VK_F5:
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
				break;

			case VK_F9:
				// Toggle Breakpoint
				Jdi->GekkoToggleBreakpoint(disasm->GetCursor());
				disasm->Invalidate();
				break;

			case VK_F10:
				// Step Over
				if (Jdi->IsLoaded() && !Jdi->IsRunning())
				{
					Jdi->GekkoAddOneShotBreakpoint(Jdi->GetPc() + 4);
					Jdi->GekkoRun();
					InvalidateAll();
				}
				break;

			case VK_F11:
				// Step Into
				if (Jdi->IsLoaded() && !Jdi->IsRunning())
				{
					Jdi->GekkoStep();
					disasm->SetCursor(Jdi->GetPc());
					InvalidateAll();
				}
				break;

			case VK_F12:
				// Skip instruction
				if (Jdi->IsLoaded() && !Jdi->IsRunning())
				{
					Jdi->GekkoSkipInstruction();
					InvalidateAll();
				}
				break;

			case VK_ESCAPE:
				if (msgs->IsActive())
				{
					SetWindowFocus("Cmdline");
					status->SetMode(DebugMode::Ready);
					InvalidateAll();
				}
				break;

			case VK_PRIOR:
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
	void GekkoDebug::SetMemoryCursor(uint32_t virtualAddress)
	{
		memview->SetCursor(virtualAddress);
	}

	/// <summary>
	/// Set the current address to view the disassembled Gekko code. Used by "u" command.
	/// </summary>
	/// <param name="virtualAddress"></param>
	void GekkoDebug::SetDisasmCursor(uint32_t virtualAddress)
	{
		disasm->SetCursor(virtualAddress);
	}

}

// Disassembling code by Gekko virtual addresses. If the instruction is in Main mem, we disassemble and print, otherwise skip.

namespace Debug
{

	GekkoDisasm::GekkoDisasm(RECT& rect, std::string name, Cui* parent)
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

	GekkoDisasm::~GekkoDisasm()
	{
	}

	void GekkoDisasm::OnDraw()
	{
		FillLine(CuiColor::Cyan, CuiColor::White, 0, ' ');
		std::string head = "[ ] F3";
		Print(CuiColor::Cyan, CuiColor::Black, 1, 0, head);
		if (IsActive())
		{
			Print(CuiColor::Cyan, CuiColor::White, 2, 0, "*");
		}

		char hint[0x100] = { 0, };
		sprintf_s(hint, sizeof(hint), " cursor:0x%08X phys:0x%08X pc:0x%08X", 
			cursor, Jdi->VirtualToPhysicalIMmu(cursor), Jdi->GetPc());

		Print(CuiColor::Cyan, CuiColor::Black, (int)(head.size() + 3), 0, hint);

		// Code

		uint32_t addr = address & ~3;
		disa_sub_h = 0;

		for (int line = 1; line < height; line++, addr += 4)
		{
			int n = DisasmLine(line, addr);
			if (n > 1) disa_sub_h += n - 1;
			line += n - 1;
		}
	}

	void GekkoDisasm::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		uint32_t targetAddress = 0;

		switch (Vkey)
		{
			case VK_HOME:
				SetCursor(Jdi->GetPc());
				break;

			case VK_END:
				break;

			case VK_UP:
				if (cursor < address)
				{
					cursor = address;
					break;
				}
				if (cursor >= (address + (uint32_t)(4 * height) - 4))
				{
					cursor = address + (uint32_t)(4 * height) - 8;
					break;
				}
				cursor -= 4;
				if (cursor < address)
				{
					address -= 4;
				}
				break;

			case VK_DOWN:
				if (cursor < address)
				{
					cursor = address;
					break;
				}
				if (cursor >= (address + 4 * (uint32_t)(height - disa_sub_h) - 4))
				{
					cursor = address + 4 * (uint32_t)(height - disa_sub_h) - 8;
					break;
				}
				cursor += 4;
				if (cursor >= (address + ((uint32_t)(height - disa_sub_h) - 1) * 4))
				{
					address += 4;
				}
				break;

			case VK_PRIOR:
				address -= (uint32_t)(4 * height - 4);
				if (!IsCursorVisible())
				{
					cursor = address;
				}
				break;

			case VK_NEXT:
				address += (uint32_t)(4 * (height - disa_sub_h) - 4);
				if (!IsCursorVisible())
				{
					cursor = address + ((uint32_t)(height - disa_sub_h) - 2) * 4;
				}
				break;

			case VK_RETURN:
				if (Jdi->GekkoIsBranch(cursor, targetAddress))
				{
					std::pair<uint32_t, uint32_t> last(address, cursor);
					browseHist.push_back(last);
					address = cursor = targetAddress;
				}
				break;

			case VK_ESCAPE:
				if (browseHist.size() > 0)
				{
					std::pair<uint32_t, uint32_t> last = browseHist.back();
					address = last.first;
					cursor = last.second;
					browseHist.pop_back();
				}
				break;
		}

		Invalidate();
	}

	uint32_t GekkoDisasm::GetCursor()
	{
		return cursor;
	}

	void GekkoDisasm::SetCursor(uint32_t addr)
	{
		cursor = addr & ~3;
		address = cursor - (uint32_t)(height - 1) / 2 * 4;
		Invalidate();
	}

	bool GekkoDisasm::IsCursorVisible()
	{
		uint32_t limit;
		limit = address + (uint32_t)((height - 1) * 4);
		return ((cursor < limit) && (cursor >= address));
	}

	int GekkoDisasm::DisasmLine(int line, uint32_t addr)
	{
		CuiColor bgpc, bgcur, bgbp;
		CuiColor bg;
		std::string symbol;
		int addend = 1;

		bgcur = (addr == cursor) ? (CuiColor::Gray) : (CuiColor::Black);
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

		uint32_t opcode = _byteswap_ulong (*ptr);

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

}

// View Gekko registers. Register values are obtained through Jdi->

namespace Debug
{
	static const char* gprnames[] = {
		"r0" , "sp" , "sd2", "r3" , "r4" , "r5" , "r6" , "r7" ,
		"r8" , "r9" , "r10", "r11", "r12", "sd1", "r14", "r15",
		"r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
		"r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31"
	};

	GekkoRegs::GekkoRegs(RECT& rect, std::string name, Cui* parent)
		: CuiWindow (rect, name, parent)
	{
		Memorize();
	}

	GekkoRegs::~GekkoRegs()
	{
	}

	void GekkoRegs::OnDraw()
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
			case GekkoRegmode::GPR: modeText = "GPR"; break;
			case GekkoRegmode::FPR: modeText = "FPR"; break;
			case GekkoRegmode::PSR: modeText = "PSR"; break;
			case GekkoRegmode::MMU: modeText = "MMU"; break;
		}

		Print(CuiColor::Cyan, CuiColor::Black, (int)(head.size() + 3), 0, modeText);

		switch (mode)
		{
			case GekkoRegmode::GPR: ShowGprs(); break;
			case GekkoRegmode::FPR: ShowFprs(); break;
			case GekkoRegmode::PSR: ShowPairedSingle(); break;
			case GekkoRegmode::MMU: ShowMmu(); break;
		}
	}

	void GekkoRegs::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		switch (Vkey)
		{
			case VK_LEFT:
			case VK_PRIOR:
				RotateView(false);
				break;

			case VK_RIGHT:
			case VK_NEXT:
				RotateView(true);
				break;
		}

		Invalidate();
	}

	void GekkoRegs::Memorize()
	{
		for (size_t n = 0; n < 32; n++)
		{
			savedGpr[n] = Jdi->GetGpr(n);
			savedPs0[n].Raw = Jdi->GetPs0(n);
			savedPs1[n].Raw = Jdi->GetPs1(n);
		}
	}

	void GekkoRegs::ShowGprs()
	{
		int y;

		for (y = 1; y <= 16; y++)
		{
			print_gprreg(0, y, y - 1);
			print_gprreg(14, y, y - 1 + 16);
		}

		ShowOtherRegs();
	}

	void GekkoRegs::ShowOtherRegs()
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
		Print(CuiColor::Normal, 32, 2, "%08X", Jdi->GetSpr(Gekko_SPR_XER));
		Print(CuiColor::Normal, 32, 4, "%08X", Jdi->GetSpr(Gekko_SPR_CTR));
		Print(CuiColor::Normal, 32, 5, "%08X", Jdi->GetSpr(Gekko_SPR_DEC));
		Print(CuiColor::Normal, 32, 8, "%08X", Jdi->GetPc());
		Print(CuiColor::Normal, 32, 9, "%08X", Jdi->GetSpr(Gekko_SPR_LR));
		Print(CuiColor::Normal, 32, 14, "%08X:%08X", Jdi->GetTbu(), Jdi->GetTbl());

		uint32_t msr = Jdi->GetMsr();
		uint32_t hid2 = Jdi->GetSpr(Gekko_SPR_HID2);

		Print(CuiColor::Normal, 48, 1, "%08X", msr);
		Print(CuiColor::Normal, 48, 2, "%08X", Jdi->GetFpscr());
		Print(CuiColor::Normal, 48, 4, "%08X", Jdi->GetSpr(Gekko_SPR_HID0));
		Print(CuiColor::Normal, 48, 5, "%08X", Jdi->GetSpr(Gekko_SPR_HID1));
		Print(CuiColor::Normal, 48, 6, "%08X", hid2);
		Print(CuiColor::Normal, 48, 8, "%08X", Jdi->GetSpr(Gekko_SPR_WPAR));
		Print(CuiColor::Normal, 48, 9, "%08X", Jdi->GetSpr(Gekko_SPR_DMAU));
		Print(CuiColor::Normal, 48, 10, "%08X", Jdi->GetSpr(Gekko_SPR_DMAL));

		Print(CuiColor::Normal, 64, 1, "%08X", Jdi->GetSpr(Gekko_SPR_DSISR));
		Print(CuiColor::Normal, 64, 2, "%08X", Jdi->GetSpr(Gekko_SPR_DAR));
		Print(CuiColor::Normal, 64, 4, "%08X", Jdi->GetSpr(Gekko_SPR_SRR0));
		Print(CuiColor::Normal, 64, 5, "%08X", Jdi->GetSpr(Gekko_SPR_SRR1));
		Print(CuiColor::Normal, 64, 8, "%08X", Jdi->GetSpr(Gekko_SPR_SPRG0));
		Print(CuiColor::Normal, 64, 9, "%08X", Jdi->GetSpr(Gekko_SPR_SPRG1));
		Print(CuiColor::Normal, 64, 10, "%08X", Jdi->GetSpr(Gekko_SPR_SPRG2));
		Print(CuiColor::Normal, 64, 11, "%08X", Jdi->GetSpr(Gekko_SPR_SPRG3));
		Print(CuiColor::Normal, 64, 13, "%08X", Jdi->GetSpr(Gekko_SPR_EAR));
		Print(CuiColor::Normal, 64, 14, "%08X", Jdi->GetSpr(Gekko_SPR_PVR));

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

	void GekkoRegs::ShowFprs()
	{
		int y;

		for (y = 1; y <= 16; y++)
		{
			print_fpreg(0, y, y - 1);
			print_fpreg(39, y, y - 1 + 16);
		}
	}

	void GekkoRegs::ShowPairedSingle()
	{
		int y;

		for (y = 1; y <= 16; y++)
		{
			print_ps(0, y, y - 1);
			print_ps(32, y, y - 1 + 16);
		}

		for (y = 1; y <= 8; y++)
		{
			uint32_t gqr = Jdi->GetSpr(Gekko_SPR_GQRs + y - 1);

			Print(CuiColor::Cyan, 64, y, "gqr%i ", y - 1);
			Print(CuiColor::Normal, 69, y, "%08X", gqr);
		}

		uint32_t hid2 = Jdi->GetSpr(Gekko_SPR_HID2);

		Print(CuiColor::Cyan, 64, 10, "PSE   ");
		Print(CuiColor::Cyan, 64, 11, "LSQ   ");

		Print(CuiColor::Normal, 70, 10, "%i", (hid2 & HID2_PSE) ? 1 : 0); // Paired Single mode?
		Print(CuiColor::Normal, 70, 11, "%i", (hid2 & HID2_LSQE) ? 1 : 0); // Load/Store Quantization?
	}

	void GekkoRegs::ShowMmu()
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

		Print(CuiColor::Normal, 6, 11, "%08X", Jdi->GetSpr(Gekko_SPR_SDR1));

		uint32_t msr = Jdi->GetMsr();

		Print(CuiColor::Normal, 6, 13, "%i", (msr & MSR_IR) ? 1 : 0);
		Print(CuiColor::Normal, 6, 14, "%i", (msr & MSR_DR) ? 1 : 0);

		Print(CuiColor::Normal, 6, 1, "%08X:%08X", Jdi->GetSpr(Gekko_SPR_DBAT0U), Jdi->GetSpr(Gekko_SPR_DBAT0L));
		Print(CuiColor::Normal, 6, 2, "%08X:%08X", Jdi->GetSpr(Gekko_SPR_DBAT1U), Jdi->GetSpr(Gekko_SPR_DBAT1L));
		Print(CuiColor::Normal, 6, 3, "%08X:%08X", Jdi->GetSpr(Gekko_SPR_DBAT2U), Jdi->GetSpr(Gekko_SPR_DBAT2L));
		Print(CuiColor::Normal, 6, 4, "%08X:%08X", Jdi->GetSpr(Gekko_SPR_DBAT3U), Jdi->GetSpr(Gekko_SPR_DBAT3L));

		Print(CuiColor::Normal, 6, 6, "%08X:%08X", Jdi->GetSpr(Gekko_SPR_IBAT0U), Jdi->GetSpr(Gekko_SPR_IBAT0L));
		Print(CuiColor::Normal, 6, 7, "%08X:%08X", Jdi->GetSpr(Gekko_SPR_IBAT1U), Jdi->GetSpr(Gekko_SPR_IBAT1L));
		Print(CuiColor::Normal, 6, 8, "%08X:%08X", Jdi->GetSpr(Gekko_SPR_IBAT2U), Jdi->GetSpr(Gekko_SPR_IBAT2L));
		Print(CuiColor::Normal, 6, 9, "%08X:%08X", Jdi->GetSpr(Gekko_SPR_IBAT3U), Jdi->GetSpr(Gekko_SPR_IBAT3L));

		// BATs detailed

		describe_bat_reg(24, 1, Jdi->GetSpr(Gekko_SPR_DBAT0U), Jdi->GetSpr(Gekko_SPR_DBAT0L), false);
		describe_bat_reg(24, 2, Jdi->GetSpr(Gekko_SPR_DBAT1U), Jdi->GetSpr(Gekko_SPR_DBAT1L), false);
		describe_bat_reg(24, 3, Jdi->GetSpr(Gekko_SPR_DBAT2U), Jdi->GetSpr(Gekko_SPR_DBAT2L), false);
		describe_bat_reg(24, 4, Jdi->GetSpr(Gekko_SPR_DBAT3U), Jdi->GetSpr(Gekko_SPR_DBAT3L), false);

		describe_bat_reg(24, 6, Jdi->GetSpr(Gekko_SPR_IBAT0U), Jdi->GetSpr(Gekko_SPR_IBAT0L), true);
		describe_bat_reg(24, 7, Jdi->GetSpr(Gekko_SPR_IBAT1U), Jdi->GetSpr(Gekko_SPR_IBAT1L), true);
		describe_bat_reg(24, 8, Jdi->GetSpr(Gekko_SPR_IBAT2U), Jdi->GetSpr(Gekko_SPR_IBAT2L), true);
		describe_bat_reg(24, 9, Jdi->GetSpr(Gekko_SPR_IBAT3U), Jdi->GetSpr(Gekko_SPR_IBAT3L), true);

		// Segment regs

		for (int n = 0, y = 1; n < 16; n++, y++)
		{
			uint32_t sr = Jdi->GetSr(n);
			CuiColor prefix = sr & 0x80000000 ? CuiColor::BrightRed : CuiColor::Normal;

			Print(CuiColor::Cyan, 64, y, "sr%-2i  ", n);
			Print(prefix, 70, y, "%08X", sr);
		}
	}

	void GekkoRegs::RotateView(bool forward)
	{
		if (forward)
		{
			switch (mode)
			{
				case GekkoRegmode::GPR: mode = GekkoRegmode::FPR; break;
				case GekkoRegmode::FPR: mode = GekkoRegmode::PSR; break;
				case GekkoRegmode::PSR: mode = GekkoRegmode::MMU; break;
				case GekkoRegmode::MMU: mode = GekkoRegmode::GPR; break;
			}
		}
		else
		{
			switch (mode)
			{
				case GekkoRegmode::GPR: mode = GekkoRegmode::MMU; break;
				case GekkoRegmode::FPR: mode = GekkoRegmode::GPR; break;
				case GekkoRegmode::PSR: mode = GekkoRegmode::FPR; break;
				case GekkoRegmode::MMU: mode = GekkoRegmode::PSR; break;
			}
		}
	}

	void GekkoRegs::print_gprreg(int x, int y, int num)
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

	void GekkoRegs::print_fpreg(int x, int y, int num)
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

	void GekkoRegs::print_ps(int x, int y, int num)
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

	int GekkoRegs::cntlzw(uint32_t val)
	{
		int i;
		for (i = 0; i < 32; i++)
		{
			if (val & (1 << (31 - i))) break;
		}
		return ((i == 32) ? 31 : i);
	}

	void GekkoRegs::describe_bat_reg(int x, int y, uint32_t up, uint32_t lo, bool instr)
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

		sprintf_s(temp, sizeof(temp), "%08X->%08X" " %-6s" " %c%c%c%c" " %s %s" " %s",
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

	std::string GekkoRegs::smart_size(size_t size)
	{
		char tempBuf[0x20] = { 0, };

		if (size < 1024)
		{
			sprintf_s(tempBuf, sizeof(tempBuf), "%zi byte", size);
		}
		else if (size < 1024 * 1024)
		{
			sprintf_s(tempBuf, sizeof(tempBuf), "%zi KB", size / 1024);
		}
		else if (size < 1024 * 1024 * 1024)
		{
			sprintf_s(tempBuf, sizeof(tempBuf), "%zi MB", size / 1024 / 1024);
		}
		else
		{
			sprintf_s(tempBuf, sizeof(tempBuf), "%1.1f GB", (float)size / 1024 / 1024 / 1024);
		}

		return tempBuf;
	}

}


// View 1T-SRAM memory, by Gekko virtual addresses. If the virtual address is translated to the physical address of Main memory, then show bytes, otherwise show `?`

namespace Debug
{

	MemoryView::MemoryView(RECT& rect, std::string name, Cui* parent)
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
		sprintf_s(hint, sizeof(hint), " phys:0x%08X stack:0x%08X sda1:0x%08X sda2:0x%08X", 
			Jdi->VirtualToPhysicalDMmu(cursor), Jdi->GetGpr(1), Jdi->GetGpr(13), Jdi->GetGpr(2));

		Print(CuiColor::Cyan, CuiColor::Black, (int)(head.size() + 3), 0, hint);

		// Hexview

		for (int row = 0; row < height - 1; row++)
		{
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

	void MemoryView::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		switch (Vkey)
		{
			case VK_HOME:
				cursor = 0x8000'0000;
				break;
			case VK_END:
				cursor = (0x8000'0000 | RAMSIZE) - (uint32_t)((height - 1) * 16);
				break;
			case VK_NEXT:
				cursor += (uint32_t)((height - 1) * 16);
				break;
			case VK_PRIOR:
				cursor -= (uint32_t)((height - 1) * 16);
				break;
			case VK_UP:
				cursor -= 16;
				break;
			case VK_DOWN:
				cursor += 16;
				break;
		}

		Invalidate();
	}

	void MemoryView::SetCursor(uint32_t address)
	{
		cursor = address;
		Invalidate();
	}

	std::string MemoryView::hexbyte(uint32_t addr)
	{
		uint8_t* ptr = (uint8_t *)Jdi->TranslateDMmu(addr);

		if (ptr)
		{
			char buf[0x10];
			sprintf_s(buf, sizeof(buf), "%02X", *ptr);
			return buf;
		}
		else
		{
			return "??";
		}
	}

	char MemoryView::charbyte(uint32_t addr)
	{
		char* ptr = (char*)Jdi->TranslateDMmu(addr);

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

}
