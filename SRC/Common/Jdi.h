// Json Debug Inteface 

#pragma once

#include <map>
#include <vector>
#include <list>
#include <string>

#include "Spinlock.h"
#include "Json.h"

namespace JDI
{
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

		void AddNode(std::wstring filename, JdiReflector reflector);
		void RemoveNode(std::wstring filename);

		void Help();
		Json::Value* Execute(std::vector<std::string>& args);
		Json::Value* ExecuteFast(char* command);
		bool ExecuteFastBool(char* command);
		uint32_t ExecuteFastUInt32(char* command);
		bool CommandExists(std::vector<std::string>& args);

		void Dump(Json::Value * value, int depth=0);
	};

	// External API

	extern JdiHub Hub;
}
