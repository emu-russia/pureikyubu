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

		std::list<Json*> nodes;

	public:
		JdiHub();
		~JdiHub();

		void AddCmd(std::string name, CmdDelegate command);

		void AddJson(std::wstring filename, JdiReflector reflector);

		void Help();
		Json::Value* Execute(std::vector<std::string>& args);
		bool CommandExists(std::vector<std::string>& args);

	};

	// External API

	void AddCmd(std::string name, CmdDelegate command);
	void AddJson(std::wstring filename, JdiReflector reflector);
	void Help();
	Json::Value* Execute(std::vector<std::string>& args);
	bool CommandExists(std::vector<std::string>& args);
}
