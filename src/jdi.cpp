#include "pch.h"

using namespace Debug;

namespace JDI
{
	JdiHub Hub;      // Singletone.

	JdiHub::JdiHub()
	{
	}

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
		for (size_t i = 0; i < size; i++)
		{
			h = (h * 54059) ^ (str[i] * 76963);
		}
		return h % 86969;
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

		size_t jsonTextSize = Util::FileSize(filename);

		auto data = Util::FileLoad(filename);

		uint8_t* jsonText = new uint8_t[jsonTextSize + 1];      // +Safety zero trailer
		memcpy(jsonText, data.data(), data.size());
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

			const wchar_t* helpGroupHead = nullptr;
			Json::Value* info = rootObj->ByName("info");
			if (info == nullptr)
			{
				helpGroupHead = L"Jdi with missing info";
			}
			else
			{
				Json::Value* helpGroup = info->ByName("helpGroup");
				if (helpGroup != nullptr)
				{
					helpGroupHead = helpGroup->type == Json::ValueType::String ?
						helpGroup->value.AsString :
						L"Jdi with invalid helpGroup";
				}
				else
				{
					helpGroupHead = L"Jdi with missing helpGroup";
				}
				helpGroupHead = helpGroup != nullptr ? helpGroup->value.AsString : L"Jdi with missing helpGroup";
			}

			Report(Channel::Header, "## %s\n", Util::WstringToString(helpGroupHead).c_str());

			// Enumerate can commands help texts

			Json::Value* can = rootObj->ByName("can");
			if (can == nullptr)
				continue;

			for (auto cmd = can->children.begin(); cmd != can->children.end(); ++cmd)
			{
				std::string nameWithHint;

				Json::Value* next = *cmd;
				const wchar_t* helpText = L"";
				const wchar_t* hintsText = L"";

				// Skip internal commands

				Json::Value* internalUse = next->ByName("internal");
				if (internalUse != nullptr)
				{
					if (internalUse->value.AsBool)
						continue;
				}

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

				nameWithHint = next->name;
				nameWithHint += " ";
				nameWithHint += Util::WstringToString(hintsText);

				Report(Channel::Norm, "    %-20s - %s\n", nameWithHint.c_str(), Util::WstringToString(helpText).c_str());
			}

			Report(Channel::Norm, "\n");
		}
	}

	// Get "can" entry by command name. Iterates over all available JDI nodes.
	Json::Value* JdiHub::CommandByName(std::string& name)
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
				Report(Channel::Norm, "%s", Util::WstringToString(line->value.AsString).c_str());
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

	// Quickly execute a command without parameters.
	Json::Value* JdiHub::ExecuteFast(const char* command)
	{
		reflexMapLock.Lock();
		auto it = reflexMap.find(command);
		if (it == reflexMap.end())
		{
			reflexMapLock.Unlock();
			return nullptr;
		}
		reflexMapLock.Unlock();

		return it->second(noArgs);
	}

	bool JdiHub::ExecuteFastBool(const char* command)
	{
		Json::Value* output = ExecuteFast(command);
		assert(output);
		bool value = output->value.AsBool;
		delete output;
		return value;
	}

	uint32_t JdiHub::ExecuteFastUInt32(const char* command)
	{
		Json::Value* output = ExecuteFast(command);
		assert(output);
		uint32_t value = output->value.AsUint32;
		delete output;
		return value;
	}

	// Check whether the command is implemented using JDI. Used for compatibility with the old cmd.cpp implementation in the debugger.
	bool JdiHub::CommandExists(const std::string& cmd)
	{
		bool exists = false;

		auto it = reflexMap.find(cmd);
		exists = it != reflexMap.end();

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
				Report(Channel::Norm, "%sObject %s: ", indent,
					value->name ? value->name : "");
				for (auto it = value->children.begin(); it != value->children.end(); ++it)
				{
					Json::Value* child = *it;
					Dump(child, depth + 1);
				}
				break;
			case Json::ValueType::Array:
				Report(Channel::Norm, "%sArray %s: ", indent,
					value->name ? value->name : "");
				for (auto it = value->children.begin(); it != value->children.end(); ++it)
				{
					Json::Value* child = *it;
					Dump(child, depth + 1);
				}
				break;

			case Json::ValueType::Bool:
				Report(Channel::Norm, "%s%s: Bool %s", indent,
					value->name ? value->name : "",
					value->value.AsBool ? "True" : "False");
				break;
			case Json::ValueType::Null:
				Report(Channel::Norm, "%s%s: Null", indent,
					value->name ? value->name : "");
				break;

			case Json::ValueType::Int:
				Report(Channel::Norm, "%s%s: Int: %I64u", indent,
					value->name ? value->name : "", value->value.AsInt);
				break;
			case Json::ValueType::Float:
				Report(Channel::Norm, "%s%s: Float: %.4f", indent,
					value->name ? value->name : "", value->value.AsFloat);
				break;

			case Json::ValueType::String:
				Report(Channel::Norm, "%s%s: String: %s", indent,
					value->name ? value->name : "", Util::WstringToString(value->value.AsString).c_str());
				break;
		}
	}

}
