#include "pch.h"

namespace Debug
{
	JdiHub Hub;      // Singletone.

	JdiHub::JdiHub() {}

	// Delete all JDI nodes
	JdiHub::~JdiHub()
	{
		for (auto it = nodes.begin(); it != nodes.end(); ++it)
		{
			if (it->second)
				delete it->second;
		}
		nodes.clear();
	}

	uint32_t JdiHub::SimpleHash(std::wstring str)
	{
		size_t size = str.size();
		unsigned h = 37;
		for (int i=0; i<size; i++)
		{
			h = (h * 54059) ^ (str[i] * 76963);
		}
		return h % 86969;
	}

	// The debugging console can only display text encoded in Ansi
	std::string JdiHub::TcharToString(TCHAR* text)
	{
		char ansiText[0x100] = { 0, };
		char* ansiPtr = ansiText;
		TCHAR* tcharPtr = text;
		while (*tcharPtr)
		{
			*ansiPtr++ = (char)*tcharPtr++;
		}
		*ansiPtr++ = 0;
		return std::string(ansiText);
	}

	// Reflection of the command delegate by its text name
	void JdiHub::AddCmd(std::string name, CmdDelegate command)
	{
		reflexMapLock.Lock();
		reflexMap[name] = command;
		reflexMapLock.Unlock();
	}

	// Register JDI node.
	void JdiHub::AddNode(std::wstring filename, JdiReflector reflector)
	{
		Json* json = new Json();

		size_t jsonTextSize = 0;

		// Load Json
		FILE* f = nullptr;
		_wfopen_s(&f, filename.c_str(), L"rb");
		assert(f);

		fseek(f, 0, SEEK_END);
		jsonTextSize = ftell(f);
		fseek(f, 0, SEEK_SET);

		uint8_t* jsonText = new uint8_t[jsonTextSize + 1];      // +Safety zero trailer
		assert(jsonText);

		size_t read = fread(jsonText, 1, jsonTextSize, f);
		assert(read == jsonTextSize);
		fclose(f);

		jsonText[jsonTextSize] = 0;         // Safety zero trailer

		// Parse
		json->Deserialize(jsonText, jsonTextSize);

		delete[] jsonText;

		nodes[SimpleHash(filename)] = json;

		reflector();
	}

	// Deregister JDI node.
	void JdiHub::RemoveNode(std::wstring filename)
	{
		auto it = nodes.find(SimpleHash(filename));
		if (it != nodes.end())
		{
			if (it->second)
			{
				// Remove commands
				Json* node = it->second;

				Json::Value* rootObj = node->root.children.back();
				if (rootObj->type == Json::ValueType::Object)
				{
					Json::Value* can = rootObj->ByName("can");
					if (can != nullptr)
					{
						for (auto cmd = can->children.begin(); cmd != can->children.end(); ++cmd)
						{
							Json::Value* next = *cmd;

							reflexMapLock.Lock();
							auto it = reflexMap.find(next->name);
							if (it != reflexMap.end())
							{
								reflexMap.erase(it);
							}
							reflexMapLock.Unlock();
						}
					}
				}

				delete node;
			}
			nodes.erase(it);
		}
	}

	// Display help on the basis of meta information from the "can" records of registered JDI nodes.
	void JdiHub::Help()
	{
		for (auto it = nodes.begin(); it != nodes.end(); ++it)
		{
			Json* node = it->second;
			if (node->root.children.size() == 0)
				continue;

			Json::Value* rootObj = node->root.children.back();
			if (rootObj->type != Json::ValueType::Object)
				continue;

			// Print help group header

			TCHAR* helpGroupHead = nullptr;
			Json::Value* info = rootObj->ByName("info");
			if (info == nullptr)
			{
				helpGroupHead = (TCHAR *)_T("Jdi with missing info");
			}
			else
			{
				Json::Value* helpGroup = info->ByName("helpGroup");
				if (helpGroup != nullptr)
				{
					helpGroupHead = helpGroup->type == Json::ValueType::String ?
						helpGroup->value.AsString : 
						(TCHAR*)_T("Jdi with invalid helpGroup");
				}
				else
				{
					helpGroupHead = (TCHAR*)_T("Jdi with missing helpGroup");
				}
				helpGroupHead = helpGroup != nullptr ? helpGroup->value.AsString : (TCHAR *)_T("Jdi with missing helpGroup");
			}

			DBReport2( DbgChannel::Header, "## %s\n", TcharToString(helpGroupHead).c_str());

			// Enumerate can commands help texts

			Json::Value* can = rootObj->ByName("can");
			if (can == nullptr)
				continue;

			for (auto cmd = can->children.begin(); cmd != can->children.end(); ++cmd)
			{
				char nameWithHint[0x100] = { 0, };

				Json::Value* next = *cmd;
				TCHAR* helpText = (TCHAR *)_T("");
				TCHAR* hintsText = (TCHAR*)_T("");

				Json::Value* help = next->ByName("help");
				if (help != nullptr)
				{
					if (help->type == Json::ValueType::String)
					{
						helpText = help->value.AsString;
					}
				}

				Json::Value* hints = next->ByName("hints");
				if (hints != nullptr)
				{
					if (hints->type == Json::ValueType::String)
					{
						hintsText = hints->value.AsString;
					}
				}

				strcpy_s(nameWithHint, sizeof(nameWithHint) - 1, next->name);
				strcat_s(nameWithHint, sizeof(nameWithHint) - 1, " ");
				strcat_s(nameWithHint, sizeof(nameWithHint) - 1, TcharToString(hintsText).c_str());

				size_t nameWithHintSize = strlen(nameWithHint);
				size_t i = nameWithHintSize;
				while (i < 20)
				{
					nameWithHint[i++] = ' ';
				}
				nameWithHint[i++] = '\0';

				DBReport("    %s - %s\n", nameWithHint, TcharToString(helpText).c_str());
			}

			DBReport("\n");
		}
	}

	// Get "can" entry by command name. Iterates over all available JDI nodes.
	Json::Value * JdiHub::CommandByName(std::string& name)
	{
		for (auto it = nodes.begin(); it != nodes.end(); ++it)
		{
			Json* node = it->second;
			if (node->root.children.size() == 0)
				continue;

			Json::Value* rootObj = node->root.children.back();
			if (rootObj->type != Json::ValueType::Object)
				continue;

			Json::Value* can = rootObj->ByName("can");
			if (can == nullptr)
				continue;

			for (auto cmd = can->children.begin(); cmd != can->children.end(); ++cmd)
			{
				Json::Value* next = *cmd;

				if (!_stricmp(next->name, name.c_str()))
				{
					return next;
				}
			}
		}

		return nullptr;
	}

	bool JdiHub::CheckParameters(Json::Value* cmd, std::vector<std::string>& args)
	{
		Json::Value* argsJson = cmd->ByName("args");
		if (argsJson == nullptr)
			return true;
		if (argsJson->type != Json::ValueType::Int)
			return true;
		
		// The command argument list contains the command name as args[0].
		// The "args" parameter in JDI specifies the minimum number of actual arguments, ignoring the command name.

		return args.size() >= (argsJson->value.AsInt + 1);
	}

	// Print a description of the command if the command is called with insufficient arguments.
	void JdiHub::PrintUsage(Json::Value* cmd)
	{
		Json::Value* usage = cmd->ByName("usage");
		if (usage == nullptr)
			return;
		if (usage->type != Json::ValueType::Array)
			return;

		// Print usage text lines

		for (auto it = usage->children.begin(); it != usage->children.end(); ++it)
		{
			Json::Value* line = *it;

			if (line->type == Json::ValueType::String)
			{
				DBReport("%s", TcharToString(line->value.AsString).c_str());
			}
		}
	}

	// Run JDI Command. Argument checking is performed automatically.
	Json::Value* JdiHub::Execute(std::vector<std::string>& args)
	{
		if (args.size() == 0)
			return nullptr;

		Json::Value* cmd = CommandByName(args[0]);
		if (cmd == nullptr)
			return nullptr;

		if (!CheckParameters(cmd, args))
		{
			PrintUsage(cmd);
			return nullptr;
		}

		reflexMapLock.Lock();
		auto it = reflexMap.find(args[0]);
		if (it == reflexMap.end())
		{
			reflexMapLock.Unlock();
			return nullptr;
		}
		reflexMapLock.Unlock();

		return it->second(args);
	}

	// Check whether the command is implemented using JDI. Used for compatibility with the old cmd.cpp implementation in the debugger.
	bool JdiHub::CommandExists(std::vector<std::string>& args)
	{
		bool exists = false;

		if (args.size() == 0)
			return false;

		reflexMapLock.Lock();
		auto it = reflexMap.find(args[0]);
		exists = it != reflexMap.end();
		reflexMapLock.Unlock();

		return exists;
	}

	void JdiHub::Dump(Json::Value* value, int depth)
	{
		char indent[0x100] = { 0, };
		char* indentPtr = indent;

		for (int i = 0; i < depth; i++)
		{
			*indentPtr++ = ' ';
			*indentPtr++ = ' ';
		}
		*indentPtr++ = 0;

		switch (value->type)
		{
			case Json::ValueType::Object:
				DBReport("%sObject %s: ", indent,
					value->name ? value->name : "");
				for (auto it = value->children.begin(); it != value->children.end(); ++it)
				{
					Json::Value* child = *it;
					Dump(child, depth + 1);
				}
				break;
			case Json::ValueType::Array:
				DBReport("%sArray %s: ", indent,
					value->name ? value->name : "");
				for (auto it = value->children.begin(); it != value->children.end(); ++it)
				{
					Json::Value* child = *it;
					Dump(child, depth + 1);
				}
				break;

			case Json::ValueType::Bool:
				DBReport("%s%s: Bool %s", indent, 
					value->name ? value->name : "",
					value->value.AsBool ? "True" : "False");
				break;
			case Json::ValueType::Null:
				DBReport("%s%s: Null", indent,
					value->name ? value->name : "");
				break;

			case Json::ValueType::Int:
				DBReport("%s%s: Int: %I64u", indent,
					value->name ? value->name : "", value->value.AsInt);
				break;
			case Json::ValueType::Float:
				DBReport("%s%s: Float: %.4f", indent,
					value->name ? value->name : "", value->value.AsFloat);
				break;

			case Json::ValueType::String:
				DBReport("%s%s: String: %s", indent,
					value->name ? value->name : "", Debug::Hub.TcharToString(value->value.AsString).c_str());
				break;
		}
	}

}
