// Processor debug commands.

#pragma once

#define GEKKO_CORE_JDI_JSON L"./Data/Json/GekkoCoreJdi.json"

namespace Debug
{
	/// <summary>
	/// To avoid making the entire Jitc context public, just friend this class.
	/// </summary>
	class JitCommands
	{
	public:
		static Json::Value* CmdJitcDumpSeg(std::vector<std::string>& args);
		static Json::Value* CmdJitcInvSeg(std::vector<std::string>& args);
		static Json::Value* CmdJitcInvAll(std::vector<std::string>& args);
	};

	void gekko_init_handlers();
}
