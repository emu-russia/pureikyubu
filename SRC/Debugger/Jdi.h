// Json Debug Inteface 

#pragma once

#include <map>
#include <vector>
#include <list>
#include <string>

#include "../Common/Spinlock.h"
#include "../Common/Json.h"

namespace Debug
{
	typedef Json::Value* (*CmdDelegate)(std::vector<std::string>& args);
	typedef void (*JdiReflector)();

	class JdiHub
	{
		std::map<std::string, CmdDelegate> reflexMap;
		MySpinLock::LOCK reflexMapLock = MySpinLock::LOCK_IS_FREE;

		std::map<uint32_t, Json*> nodes;

		Json::Value* CommandByName(std::string& name);
		bool CheckParameters(Json::Value* cmd, std::vector<std::string>& args);
		void PrintUsage(Json::Value* cmd);

		uint32_t SimpleHash(std::wstring str);

	public:
		JdiHub();
		~JdiHub();

		std::string TcharToString(TCHAR* text);

		void AddCmd(std::string name, CmdDelegate command);

		void AddNode(std::wstring filename, JdiReflector reflector);
		void RemoveNode(std::wstring filename);

		void Help();
		Json::Value* Execute(std::vector<std::string>& args);
		bool CommandExists(std::vector<std::string>& args);

		void Dump(Json::Value * value, int depth=0);
	};

	// External API

	extern JdiHub Hub;
}
