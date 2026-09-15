// Json Debug Inteface 
// More information can be found in [JsonDebugInteface.md](https://github.com/ogamespec/dolwin-docs/blob/master/EMU/JsonDebugInterface.md)
//
// The interface speaks UTF-8: a command line, the name of a command, its arguments and the strings
// it answers with are all UTF-8 (issue #372). This is what lets a command carry a file name outside
// the ANSI code page, and it is also the one encoding every front end of the emulator already uses
// (the Json documents, ImGui/SDL, and the Win32 front end, which converts to UTF-8 on its side).
// A handler that needs the emulator's wide text converts its arguments with Util::StringToWstring.

#pragma once

namespace JDI
{
	// `args` is the tokenized command line: args[0] is the command name, the rest are its
	// arguments, each one a UTF-8 string. The vector belongs to the caller.
	typedef Json::Value* (*CmdDelegate)(std::vector<std::string>& args);
	typedef void (*JdiReflector)();

	class JdiHub
	{
		std::map<std::string, CmdDelegate> reflexMap;
		SpinLock reflexMapLock;

		std::map<uint32_t, Json*> nodes;

		Json::Value* CommandByName(std::string& name);
		bool CheckParameters(Json::Value* cmd, std::vector<std::string>& args);
		void PrintUsage(Json::Value* cmd);

		uint32_t SimpleHash(std::wstring str);

		std::vector<std::string> noArgs;

	public:
		JdiHub();
		~JdiHub();

		void AddCmd(std::string name, CmdDelegate command);

		void AddNode(std::wstring filename, const char *jsonText, JdiReflector reflector);
		void RemoveNode(std::wstring filename);

		void Help();
		Json::Value* Execute(std::vector<std::string>& args);
		Json::Value* ExecuteFast(const char* command);
		bool ExecuteFastBool(const char* command);
		uint32_t ExecuteFastUInt32(const char* command);
		bool CommandExists(const std::string& cmd);

		void Dump(Json::Value* value, int depth = 0);

		// One command of the interface, as its node declares it.
		struct CommandInfo
		{
			const char* name;		// The name of the "can" record, which is the name of the command
			Json::Value* spec;		// The record itself (help, hints, args, usage, output, ...)
		};

		// Hand every registered command to the caller. It is how a component that presents the
		// interface rather than runs it - the MCP server (`mcp.cpp`), which publishes the whole
		// command list as its tools - enumerates what there is. The records belong to the nodes
		// and stay valid until a node is removed.
		void EnumCommands(std::vector<CommandInfo>& commands);
	};

	// External API

	extern JdiHub Hub;
}
