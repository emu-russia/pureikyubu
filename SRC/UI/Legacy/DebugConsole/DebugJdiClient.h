// This module is used to communicate with the JDI host. The host can be on the network, in a pluggable DLL or statically linked.

#pragma once

namespace JDI
{
	typedef Json::Value* (*CmdDelegate)(std::vector<std::string>& args);
	typedef void (*JdiReflector)();
}

#ifdef _WINDOWS
typedef Json::Value* (__cdecl* CALL_JDI)(const char* request);
typedef bool(__cdecl* CALL_JDI_NO_RETURN)(const char* request);
typedef bool(__cdecl* CALL_JDI_RETURN_INT)(const char* request, int* valueOut);
typedef bool(__cdecl* CALL_JDI_RETURN_STRING)(const char* request, char* valueOut, size_t valueSize);
typedef bool(__cdecl* CALL_JDI_RETURN_BOOL)(const char* request, bool* valueOut);

typedef void(__cdecl* JDI_ADD_NODE)(const char* filename, JDI::JdiReflector reflector);
typedef void(__cdecl* JDI_REMOVE_NODE)(const char* filename);
typedef void(__cdecl* JDI_ADD_CMD)(const char* name, JDI::CmdDelegate command);
#endif

namespace Debug
{
	class JdiClient
	{
#ifdef _WINDOWS
		CALL_JDI CallJdi = nullptr;
		CALL_JDI_NO_RETURN CallJdiNoReturn = nullptr;
		CALL_JDI_RETURN_INT CallJdiReturnInt = nullptr;
		CALL_JDI_RETURN_STRING CallJdiReturnString = nullptr;
		CALL_JDI_RETURN_BOOL CallJdiReturnBool = nullptr;

		HMODULE dll = nullptr;
#endif

	public:

#ifdef _WINDOWS
		JDI_ADD_NODE JdiAddNode = nullptr;
		JDI_REMOVE_NODE JdiRemoveNode = nullptr;
		JDI_ADD_CMD JdiAddCmd = nullptr;
#endif

		JdiClient();
		~JdiClient();

		// Generic

		std::string GetVersion();
		void ExecuteCommand(const std::string& cmdline);

		// Debug interface

		std::string DebugChannelToString(int chan);
		void QueryDebugMessages(std::list<std::pair<int, std::string>>& queue);
		void Report(const std::string& text);

	};

	extern JdiClient Jdi;
}
