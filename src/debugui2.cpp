// The new debugger (debugui2). The module description is in debugui2.h.

#include "pch.h"

#include <sstream>
#include <chrono>
#include <cctype>
#include <cerrno>

namespace Debug2
{

	// The running debugger, or nullptr when it is not open.
	Debugger* g_Debugger = nullptr;

	static bool closeRequested = false;





	static const size_t MaxLogItems = 256;			// the message history is capped
	static const size_t MaxCmdHistory = 64;
	static const uint64_t RefreshInterval = 500;	// ms between the live panel updates


	// ========================================================================================
	// Panel
	// ========================================================================================

	Panel& Panel::operator=(const Panel& other)
	{
		if (this == &other)
		{
			return *this;
		}

		for (auto it = subs.begin(); it != subs.end(); ++it)
		{
			delete* it;
		}
		subs.clear();

		title = other.title;
		split = other.split;
		items = other.items;
		cmdline = other.cmdline;
		order = other.order;

		// A deep copy: the snapshot the UI thread renders must not share the sub-panels with the
		// tree the debugger thread keeps mutating.
		for (auto it = other.subs.begin(); it != other.subs.end(); ++it)
		{
			subs.push_back(new Panel(**it));
		}

		return *this;
	}

	Panel::~Panel()
	{
		for (auto it = subs.begin(); it != subs.end(); ++it)
		{
			delete* it;
		}
		subs.clear();
	}

	void Panel::SplitInto(Split direction, size_t count)
	{
		for (auto it = subs.begin(); it != subs.end(); ++it)
		{
			delete* it;
		}
		subs.clear();

		items.clear();
		split = direction;

		for (size_t i = 0; i < count; i++)
		{
			subs.push_back(new Panel());
		}
	}


	// ========================================================================================
	// Markdown -> view
	// ========================================================================================

	static std::string Trim(const std::string& str)
	{
		size_t begin = 0, end = str.size();

		while (begin < end && (str[begin] == ' ' || str[begin] == '\t' || str[begin] == '\r'))
			begin++;
		while (end > begin && (str[end - 1] == ' ' || str[end - 1] == '\t' || str[end - 1] == '\r'))
			end--;

		return str.substr(begin, end - begin);
	}

	// A line that opens (or closes) a fenced code block.
	static bool IsFence(const std::string& line)
	{
		return line.size() >= 3 && (line.compare(0, 3, "```") == 0 || line.compare(0, 3, "~~~") == 0);
	}

	// ---, *** or ___ on a line of its own.
	static bool IsRule(const std::string& line)
	{
		if (line.size() < 3)
			return false;

		char c = line[0];
		if (c != '-' && c != '*' && c != '_')
			return false;

		for (size_t i = 0; i < line.size(); i++)
		{
			if (line[i] != c && line[i] != ' ')
				return false;
		}

		return true;
	}

	// "- foo", "* foo", "+ foo", "1. foo" or "1) foo".
	static bool IsBullet(const std::string& line, std::string& rest)
	{
		if (line.size() >= 2 && (line[0] == '-' || line[0] == '*' || line[0] == '+') && line[1] == ' ')
		{
			rest = line.substr(2);
			return true;
		}

		size_t i = 0;
		while (i < line.size() && isdigit((unsigned char)line[i]))
			i++;

		if (i > 0 && (i + 1) < line.size() && (line[i] == '.' || line[i] == ')') && line[i + 1] == ' ')
		{
			rest = line.substr(i + 2);
			return true;
		}

		return false;
	}

	static bool IsTableRow(const std::string& line)
	{
		return line.find('|') != std::string::npos;
	}

	// |---|:--:|---| - the row that tells a table from a paragraph with a pipe in it.
	static bool IsTableSeparator(const std::string& line)
	{
		bool dashes = false;

		for (size_t i = 0; i < line.size(); i++)
		{
			char c = line[i];

			if (c == '-')
				dashes = true;
			else if (c != '|' && c != ':' && c != ' ')
				return false;
		}

		return dashes;
	}

	static void SplitTableRow(const std::string& line, std::vector<std::string>& cells)
	{
		cells.clear();

		std::string cell;
		for (size_t i = 0; i < line.size(); i++)
		{
			if (line[i] == '|')
			{
				cells.push_back(Trim(cell));
				cell.clear();
			}
			else
			{
				cell += line[i];
			}
		}
		cells.push_back(Trim(cell));

		// The leading and the trailing pipes produce empty cells on both ends.
		if (!cells.empty() && cells.front().empty())
			cells.erase(cells.begin());
		if (!cells.empty() && cells.back().empty())
			cells.pop_back();
	}

	// The inline markup: `code`, **strong**, *emphasis*, ![alt](ref) and [text](url).
	static void ParseInline(const std::string& text, MdStyle style, int level, std::vector<MdSpan>& spans)
	{
		std::string pending;
		size_t i = 0, n = text.size();

		// Hand the text collected so far to the span list, with the style the whole line has.
		auto flushPending = [&]()
		{
			if (!pending.empty())
			{
				MdSpan plain;
				plain.style = style;
				plain.level = level;
				plain.text = pending;
				spans.push_back(plain);
				pending.clear();
			}
		};

		// The same, followed by a run with its own style.
		auto flush = [&](MdStyle runStyle, int runLevel, const std::string& value)
		{
			flushPending();

			if (!value.empty() || runStyle == MdStyle::Code)
			{
				MdSpan run;
				run.style = runStyle;
				run.level = runLevel;
				run.text = value;
				spans.push_back(run);
			}
		};

		while (i < n)
		{
			char c = text[i];

			if (c == '`')
			{
				size_t close = text.find('`', i + 1);
				if (close != std::string::npos)
				{
					flush(MdStyle::Code, 0, text.substr(i + 1, close - i - 1));
					i = close + 1;
					continue;
				}
			}
			else if (c == '*')
			{
				if ((i + 1) < n && text[i + 1] == '*')
				{
					size_t close = text.find("**", i + 2);
					if (close != std::string::npos)
					{
						flush(MdStyle::Strong, 0, text.substr(i + 2, close - i - 2));
						i = close + 2;
						continue;
					}
				}
				else
				{
					size_t close = text.find('*', i + 1);
					if (close != std::string::npos)
					{
						flush(MdStyle::Emphasis, 0, text.substr(i + 1, close - i - 1));
						i = close + 1;
						continue;
					}
				}
			}
			else if (c == '!' && (i + 1) < n && text[i + 1] == '[')
			{
				size_t altEnd = text.find(']', i + 2);
				if (altEnd != std::string::npos && (altEnd + 1) < n && text[altEnd + 1] == '(')
				{
					size_t refEnd = text.find(')', altEnd + 2);
					if (refEnd != std::string::npos)
					{
						flushPending();

						MdSpan image;
						image.style = MdStyle::Image;
						image.text = text.substr(i + 2, altEnd - i - 2);
						image.ref = text.substr(altEnd + 2, refEnd - altEnd - 2);
						spans.push_back(image);

						i = refEnd + 1;
						continue;
					}
				}
			}
			else if (c == '[')
			{
				// A link: the URL is dropped, the text stays (the debugger has no browser).
				size_t textEnd = text.find(']', i + 1);
				if (textEnd != std::string::npos && (textEnd + 1) < n && text[textEnd + 1] == '(')
				{
					size_t urlEnd = text.find(')', textEnd + 2);
					if (urlEnd != std::string::npos)
					{
						pending += text.substr(i + 1, textEnd - i - 1);
						i = urlEnd + 1;
						continue;
					}
				}
			}

			pending += c;
			i++;
		}

		if (!pending.empty())
		{
			MdSpan plain;
			plain.style = style;
			plain.level = level;
			plain.text = pending;
			spans.push_back(plain);
		}
	}

	static MdLine SimpleLine(MdStyle style, const std::string& text)
	{
		MdLine line;
		MdSpan span;
		span.style = style;
		span.text = text;
		line.spans.push_back(span);
		return line;
	}

	void MarkdownToView(const std::string& markdown, MdView& view)
	{
		view.clear();

		// Split the source into lines (without the line terminators).
		std::vector<std::string> lines;
		{
			std::string line;
			for (size_t i = 0; i < markdown.size(); i++)
			{
				char c = markdown[i];
				if (c == '\n')
				{
					lines.push_back(line);
					line.clear();
				}
				else if (c != '\r')
				{
					line += c;
				}
			}
			lines.push_back(line);
		}

		size_t i = 0;

		while (i < lines.size())
		{
			std::string line = Trim(lines[i]);

			if (line.empty())
			{
				// A paragraph break, unless there is nothing before it or one is already there.
				if (!view.empty() && !view.back().spans.empty())
					view.push_back(MdLine());
				i++;
				continue;
			}

			if (IsFence(line))
			{
				i++;
				while (i < lines.size() && !IsFence(Trim(lines[i])))
				{
					view.push_back(SimpleLine(MdStyle::Code, lines[i]));
					i++;
				}
				if (i < lines.size())
					i++;
				continue;
			}

			if (IsRule(line))
			{
				view.push_back(SimpleLine(MdStyle::Rule, ""));
				i++;
				continue;
			}

			if (line[0] == '#')
			{
				size_t level = 0;
				while (level < line.size() && line[level] == '#')
					level++;

				if (level <= 6 && (level == line.size() || line[level] == ' '))
				{
					MdLine heading;
					ParseInline(Trim(line.substr(level)), MdStyle::Heading, (int)level, heading.spans);
					view.push_back(heading);
					i++;
					continue;
				}
			}

			// A table: this row and the next one, which must be the separator.
			if (IsTableRow(line) && (i + 1) < lines.size() && IsTableSeparator(Trim(lines[i + 1])))
			{
				std::vector<std::vector<std::string>> rows;
				std::vector<std::string> cells;

				SplitTableRow(line, cells);
				rows.push_back(cells);
				i += 2;

				while (i < lines.size())
				{
					std::string row = Trim(lines[i]);
					if (row.empty() || !IsTableRow(row))
						break;
					SplitTableRow(row, cells);
					rows.push_back(cells);
					i++;
				}

				// DejaVu Sans Mono is a monospaced font, so the columns can simply be padded.
				size_t columns = 0;
				for (size_t r = 0; r < rows.size(); r++)
					columns = my_max(columns, rows[r].size());

				std::vector<size_t> widths(columns, 0);
				for (size_t r = 0; r < rows.size(); r++)
				{
					for (size_t c = 0; c < rows[r].size(); c++)
						widths[c] = my_max(widths[c], rows[r][c].size());
				}

				for (size_t r = 0; r < rows.size(); r++)
				{
					std::string text;
					for (size_t c = 0; c < rows[r].size(); c++)
					{
						text += rows[r][c];
						if ((c + 1) < rows[r].size())
							text += std::string(widths[c] - rows[r][c].size() + 2, ' ');
					}

					view.push_back(SimpleLine(r == 0 ? MdStyle::Strong : MdStyle::Norm, text));

					if (r == 0)
						view.push_back(SimpleLine(MdStyle::Rule, ""));
				}

				continue;
			}

			std::string rest;
			if (IsBullet(line, rest))
			{
				MdLine bullet;
				MdSpan marker;
				marker.style = MdStyle::Bullet;
				marker.text = "*";
				bullet.spans.push_back(marker);
				ParseInline(rest, MdStyle::Norm, 0, bullet.spans);
				view.push_back(bullet);
				i++;
				continue;
			}

			MdLine paragraph;
			ParseInline(line, MdStyle::Norm, 0, paragraph.spans);
			view.push_back(paragraph);
			i++;
		}
	}


	// ========================================================================================
	// The debugger
	// ========================================================================================

	static uint64_t NowMs()
	{
		using namespace std::chrono;
		return (uint64_t)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
	}

	static bool MakeDirectory(const std::string& path)
	{
		// True only when this call created the directory (an existing one counts as a failure).
#if defined(_WINDOWS)
		return CreateDirectoryA(path.c_str(), nullptr) != FALSE;
#else
		return mkdir(path.c_str(), 0755) == 0;
#endif
	}

	static std::string SanitizeName(const std::string& name)
	{
		std::string result;

		for (size_t i = 0; i < name.size() && result.size() < 48; i++)
		{
			char c = name[i];

			if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')
				result += c;
			else
				result += '_';
		}

		while (!result.empty() && (result.front() == '.' || result.front() == '_'))
			result.erase(result.begin());
		while (!result.empty() && (result.back() == '.' || result.back() == '_'))
			result.pop_back();

		return result.empty() ? "session" : result;
	}

	Debugger::Debugger()
	{
	}

	Debugger::~Debugger()
	{
		Stop();
	}

	bool Debugger::CreateSession()
	{
		std::string name = "session";

		// Name the session after the image that is running (the full path is what the emulator
		// keeps; only the file name without the extension is of interest here).
		Json::Value* loaded = CallJdi("GetLoaded");
		if (loaded != nullptr)
		{
			if (loaded->type == Json::ValueType::Object)
			{
				Json::Value* pathValue = loaded->ByName("loaded");
				if (pathValue != nullptr && pathValue->type == Json::ValueType::String)
				{
					std::string path = Util::WstringToString(pathValue->value.AsString);

					size_t slash = path.find_last_of("/\\");
					std::string file = (slash == std::string::npos) ? path : path.substr(slash + 1);

					size_t dot = file.find_last_of('.');
					if (dot != std::string::npos && dot > 0)
						file = file.substr(0, dot);

					if (!file.empty())
						name = SanitizeName(file);
				}
			}

			delete loaded;
		}

		MakeDirectory("Data");
		MakeDirectory("Data/Sessions");

		// The ordinal keeps the sessions of the same image apart. The folder is claimed by
		// creating it rather than by asking whether the name is free first, which closes the
		// window between the question and the answer.
		for (int ordinal = 1; ordinal < 10000; ordinal++)
		{
			char path[0x200];
			sprintf(path, "Data/Sessions/%s_%i", name.c_str(), ordinal);

			if (!MakeDirectory(path))
				continue;

			sessionName = name;
			sessionPath = path;
			return true;
		}

		Debug::Report(Debug::Channel::Error, "debugui2: cannot create a session folder for '%s'\n", name.c_str());
		return false;
	}

	std::string Debugger::SessionSave(const std::string& filename, const std::string& text) const
	{
		if (sessionPath.empty() || filename.empty())
			return "";

		std::string path = sessionPath + "/" + filename;

		std::vector<uint8_t> data(text.begin(), text.end());

		if (!Util::FileSave(path, data))
			return "";

		return path;
	}

	bool Debugger::Start()
	{
		if (active)
			return false;

		if (!CreateSession())
		{
			return false;
		}


		// The mockup layout: the main panel is split vertically into the message history on the
		// left and the three live panels on the right, which are stacked horizontally.
		root.SetTitle(sessionPath);
		root.SplitInto(Split::Vertical, 2);

		log = &root.Sub(0);
		log->SetTitle("Debug Messages");
		log->SetCmdline(true);

		Panel& right = root.Sub(1);
		right.SplitInto(Split::Horizontal, 3);

		regs = &right.Sub(0);
		regs->SetTitle("Gekko Registers");

		disasm = &right.Sub(1);
		disasm->SetTitle("Gekko Disassembly");

		memdump = &right.Sub(2);
		memdump->SetTitle("Splash Memory");

		// Whatever the panels will show, they are never empty: this is also the hint about why
		// the live panels stay blank until something is running.
		AppendItem(log, "**debugui2**: the new debugger is running. The command line is at the bottom of this panel.\n", ItemAlign::Left);
		AppendItem(regs, "_Load an image to see the live registers._\n", ItemAlign::Left);
		AppendItem(disasm, "_Load an image to see the live disassembly._\n", ItemAlign::Left);
		AppendItem(memdump, "_Load an image to see the physical memory._\n", ItemAlign::Left);

		// The window is the only part that needs a GL context, so it is optional: without it the
		// debugger still works (the session and the commands are there for JDI).
		ui = CreateGlUi();
		if (ui != nullptr && !ui->Open(sessionPath, this))
		{
			delete ui;
			ui = nullptr;
		}

		if (ui == nullptr)
		{
			Debug::Report(Debug::Channel::Error, "debugui2: no window (SDL/OpenGL unavailable), the debugger works headless\n");
		}

		lock.Lock();
		active = true;
		closeRequested = false;
		lock.Unlock();

		Publish();

		thread = EMUCreateThread(ThreadEntry, false, this, "DebugUI2");
		return true;
	}

	void Debugger::Stop()
	{
		if (thread == nullptr && !active)
			return;

		Debug::Report(Debug::Channel::Norm, "debugui2: session '%s' closed\n", sessionPath.c_str());

		// The log is the artifact a session leaves behind.
		if (!sessionPath.empty())
		{
			std::string logText = "# " + sessionPath + "\n\n";
			for (size_t i = 0; i < log->ItemCount(); i++)
			{
				logText += log->GetItem(i).source;
				logText += "\n";
			}
			SessionSave("log.md", logText);
		}

		lock.Lock();
		active = false;
		lock.Unlock();

		if (thread != nullptr)
		{
			// Let the worker finish the iteration it is in, so that it is not torn down in the
			// middle of a JDI call.
			Thread::Sleep(150);
			EMUJoinThread(thread);
			thread = nullptr;
		}

		if (ui != nullptr)
		{
			ui->Close();
			delete ui;
			ui = nullptr;
		}

		lock.Lock();
		published.reset();
		lock.Unlock();

		sessionPath.clear();
		sessionName.clear();
	}

	void Debugger::ThreadEntry(void* param)
	{
		((Debugger*)param)->ThreadProc();
	}

	void Debugger::ThreadProc()
	{
		lock.Lock();
		bool run = active;
		bool close = closeRequested;
		lock.Unlock();

		if (!run || close)
		{
			// One idle iteration: the thread belongs to the emulator's thread list and is torn
			// down from the outside, so it stays alive until then.
			Thread::Sleep(20);
			return;
		}

		// The commands the user typed.
		std::deque<std::string> commands;
		lock.Lock();
		commands.swap(pendingCommands);
		lock.Unlock();

		for (auto it = commands.begin(); it != commands.end(); ++it)
		{
			ExecuteCommand(*it);
		}

		PumpMessages();

		if ((NowMs() - lastRefresh) >= RefreshInterval)
		{
			lastRefresh = NowMs();
			RefreshLivePanels();
		}

		if (dirty)
		{
			Publish();
		}

		Thread::Sleep(20);
	}

	void Debugger::AppendItem(Panel* panel, const std::string& markdown, ItemAlign align)
	{
		if (panel == nullptr)
			return;

		Item item;
		item.source = markdown;
		item.align = align;
		MarkdownToView(markdown, item.view);
		panel->AddItem(std::move(item));

		while (panel->ItemCount() > MaxLogItems)
			panel->DropFirstItem();

		dirty = true;
	}

	void Debugger::ReplaceItems(Panel* panel, const std::string& markdown)
	{
		if (panel == nullptr)
			return;

		panel->ClearItems();
		AppendItem(panel, markdown, ItemAlign::Left);
	}

	void Debugger::Publish()
	{
		std::shared_ptr<View> view = std::make_shared<View>();
		view->title = sessionPath;
		view->root = root;
		view->cmdHistory = cmdHistory;

		lock.Lock();
		published = view;
		lock.Unlock();

		dirty = false;
	}

	std::string Debugger::JdiCommandToMarkdown(const std::string& cmdline)
	{
		Json::Value* result = nullptr;

		try
		{
			result = CallJdi(cmdline.c_str());
		}
		catch (...)
		{
			return "**JDI:** unbalanced quotation marks";
		}

		if (result == nullptr)
			return "";

		std::string markdown;

		if (result->type == Json::ValueType::Object)
		{
			Json::Value* md = result->ByName("markdown");
			if (md != nullptr && md->type == Json::ValueType::String)
				markdown = Util::WstringToString(md->value.AsString);
		}
		else if (result->type == Json::ValueType::Array)
		{
			for (auto it = result->children.begin(); it != result->children.end(); ++it)
			{
				Json::Value* child = *it;
				if (child->type == Json::ValueType::String)
				{
					if (!markdown.empty())
						markdown += "\n";
					markdown += Util::WstringToString(child->value.AsString);
				}
			}
		}
		else if (result->type == Json::ValueType::Int)
		{
			char buf[0x40];
			sprintf(buf, "0x%08X (%i)", (uint32_t)result->value.AsInt, (int32_t)result->value.AsInt);
			markdown = buf;
		}
		else if (result->type == Json::ValueType::Bool)
		{
			markdown = result->value.AsBool ? "true" : "false";
		}

		delete result;
		return markdown;
	}

	void Debugger::ExecuteCommand(const std::string& cmdline)
	{
		// The command the user typed is echoed to the right, the answers go to the left.
		AppendItem(log, "`" + cmdline + "`", ItemAlign::Right);

		std::string markdown = JdiCommandToMarkdown(cmdline);
		if (!markdown.empty())
		{
			AppendItem(log, markdown, ItemAlign::Left);
		}

		if (cmdHistory.empty() || cmdHistory.back() != cmdline)
		{
			cmdHistory.push_back(cmdline);
			if (cmdHistory.size() > MaxCmdHistory)
				cmdHistory.erase(cmdHistory.begin());
		}

		cmdHistoryPos = cmdHistory.size();
	}

	void Debugger::PumpMessages()
	{
		std::list<std::pair<Debug::Channel, std::string>> queue;

		Debug::Msgs.QueryDebugMessages(queue);

		if (queue.empty())
			return;

		for (auto it = queue.begin(); it != queue.end(); ++it)
		{
			std::string channel;

			char name[0x100] = { 0, };
			char cmd[0x40];
			sprintf(cmd, "GetChannelName %i", (int)it->first);
			if (CallJdiReturnString(cmd, name, sizeof(name) - 1))
				channel = name;

			// Info and Header are not really a source of the message, they are its formatting.
			std::string prefix;
			if (!channel.empty() && channel != "Info" && channel != "Header")
				prefix = "**" + channel + ":** ";

			std::istringstream stream(it->second);
			std::string line;
			bool first = true;

			while (std::getline(stream, line))
			{
				if (!line.empty() && line.back() == '\r')
					line.pop_back();

				AppendItem(log, (first ? prefix : std::string()) + line, ItemAlign::Left);
				first = false;
			}
		}
	}

	void Debugger::RefreshLivePanels()
	{
		// The live panels read the emulated machine, so there is nothing to show until it runs.
		if (!JDI::Hub.ExecuteFastBool("IsLoaded"))
			return;

		// 2.1: the Gekko registers.
		{
			std::string markdown = JdiCommandToMarkdown("regs");
			if (!markdown.empty())
			{
				ReplaceItems(regs, markdown);
			}
		}

		// 2.2: the Gekko disassembly around the program counter.
		{
			uint32_t pc = JDI::Hub.ExecuteFastUInt32("GetPc");

			std::string text;
			uint32_t address = pc;

			for (int n = 0; n < 20; n++)
			{
				char cmd[0x40];
				sprintf(cmd, "GekkoDisasm 0x%08X", address);

				std::string line;
				Json::Value* value = CallJdi(cmd);
				if (value != nullptr)
				{
					if (value->type == Json::ValueType::Array && !value->children.empty())
					{
						Json::Value* first = value->children.front();
						if (first->type == Json::ValueType::String)
							line = Util::WstringToString(first->value.AsString);
					}
					delete value;
				}

				if (line.empty())
					line = "???";

				text += line;
				text += "\n";
				address += 4;
			}

			char header[0x40];
			sprintf(header, "### PC = 0x%08X\n", pc);

			ReplaceItems(disasm, std::string(header) + "```\n" + text + "```\n");
		}

		// 2.3: the physical memory (Splash).
		{
			std::string markdown = JdiCommandToMarkdown("memdump 0x00000000 16");
			if (!markdown.empty())
			{
				ReplaceItems(memdump, markdown);
			}
		}
	}

	void Debugger::Frame()
	{
		if (ui == nullptr)
			return;

		std::shared_ptr<const View> view;

		lock.Lock();
		view = published;
		lock.Unlock();

		if (view)
			ui->Render(*view);
	}

	void Debugger::OnUiCommand(const std::string& cmdline)
	{
		if (cmdline.empty())
			return;

		lock.Lock();
		pendingCommands.push_back(cmdline);
		lock.Unlock();
	}

	void Debugger::OnUiClose()
	{
		// Do not tear anything down here: this is called from inside the front end, and the
		// front end is deleted by StopDebugger(). The host checks CloseRequested() and stops
		// the debugger from its own frame loop instead.
		lock.Lock();
		closeRequested = true;
		lock.Unlock();
	}


	// ========================================================================================
	// The front end entry points
	// ========================================================================================

	// The path of the session the debugger is working in (the session is a JDI entity, so that
	// the command handlers can put their artifacts next to it).
	static Json::Value* CmdSessionPath(std::vector<std::string>& args)
	{
		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Array;
		output->AddAnsiString(nullptr, g_Debugger != nullptr ? g_Debugger->SessionPath().c_str() : "");
		return output;
	}

	// SessionSave <file> <text> - store a text artifact in the session folder.
	static Json::Value* CmdSessionSave(std::vector<std::string>& args)
	{
		std::string path;
		if (g_Debugger != nullptr)
			path = g_Debugger->SessionSave(args[1], args[2]);

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Array;
		output->AddAnsiString(nullptr, path.c_str());
		return output;
	}

	// TestImage [width] [height] - draw a small test picture, store it in the session folder and
	// answer with the Markdown that shows it.
	//
	// This is the command to reach for when the picture pipeline is in doubt: it goes through all
	// of its steps at once (an artifact in the session folder, an image reference in Markdown, and
	// the front end loading the file relative to the session root).
	static Json::Value* CmdTestImage(std::vector<std::string>& args)
	{
		if (g_Debugger == nullptr || g_Debugger->SessionPath().empty())
		{
			Debug::Report(Debug::Channel::Norm, "testimage: the new debugger is not running\n");
			return nullptr;
		}

		int width = (args.size() > 1) ? atoi(args[1].c_str()) : 256;
		int height = (args.size() > 2) ? atoi(args[2].c_str()) : 160;

		if (width < 16) width = 16;
		if (width > 1024) width = 1024;
		if (height < 16) height = 16;
		if (height > 1024) height = 1024;

		static const uint8_t bars[4][3] =
		{
			{ 0xFF, 0x00, 0x00 },
			{ 0x00, 0xFF, 0x00 },
			{ 0x00, 0x00, 0xFF },
			{ 0xFF, 0xFF, 0xFF },
		};

		int barTop = height - height / 5;

		std::vector<uint8_t> rgb((size_t)width * height * 3);

		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				uint8_t* pixel = &rgb[((size_t)y * width + x) * 3];

				// A checkerboard with a colour ramp over it: enough to see at a glance whether the
				// picture made it through the whole pipeline, and where its edges ended up.
				bool checker = (((x / 16) + (y / 16)) & 1) != 0;
				uint8_t base = checker ? 0x30 : 0x58;

				pixel[0] = (uint8_t)(base + (x * 0x80 / width));
				pixel[1] = (uint8_t)(base + (y * 0x80 / height));
				pixel[2] = (uint8_t)(0xFF - (x * 0xBF / width));

				if (y >= barTop)
				{
					int bar = x * 4 / width;
					pixel[0] = bars[bar][0];
					pixel[1] = bars[bar][1];
					pixel[2] = bars[bar][2];
				}

				if (x < 2 || y < 2 || x >= (width - 2) || y >= (height - 2))
				{
					pixel[0] = pixel[1] = pixel[2] = 0xFF;
				}
			}
		}

		const char* file = "testimage.png";
		std::string path = g_Debugger->SessionPath() + "/" + file;

		if (!Util::SavePng(path.c_str(), rgb.data(), width, height))
		{
			Debug::Report(Debug::Channel::Error, "testimage: cannot store %s\n", path.c_str());
			return nullptr;
		}

		char size[0x40];
		sprintf(size, "%ix%i", width, height);

		// The picture is referenced relative to the session folder (the Markdown "root" of the
		// debugger), so the item can be moved between sessions without touching the file name.
		std::string markdown;
		markdown += "# Test picture\n\n";
		markdown += "![test picture](" + std::string(file) + ")\n\n";
		markdown += "`" + std::string(size) + "`, stored as `" + path + "`\n";

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Object;
		output->AddAnsiString("markdown", markdown.c_str());

		return output;
	}

	void Reflector()
	{
		JdiAddCmd("SessionPath", CmdSessionPath);
		JdiAddCmd("SessionSave", CmdSessionSave);
		JdiAddCmd("testimage", CmdTestImage);
	}

	void StartDebugger()
	{
		// Opening the window pumps the host's messages (SDL does that while it creates the
		// window), so this can be re-entered before `g_Debugger` is there to stop it: the flag
		// keeps a second session from being opened behind the first one's back.
		static bool starting = false;

		if (g_Debugger != nullptr || starting)
			return;

		starting = true;

		Debugger* debugger = new Debugger();

		if (debugger->Start())
		{
			g_Debugger = debugger;
		}
		else
		{
			delete debugger;
		}

		starting = false;
	}

	void StopDebugger()
	{
		if (g_Debugger == nullptr)
			return;

		Debugger* debugger = g_Debugger;
		g_Debugger = nullptr;

		debugger->Stop();
		delete debugger;

		closeRequested = false;
	}

	bool IsDebuggerActive()
	{
		return g_Debugger != nullptr;
	}

	bool CloseRequested()
	{
		return closeRequested;
	}

	void Frame()
	{
		if (g_Debugger != nullptr)
			g_Debugger->Frame();
	}

}
