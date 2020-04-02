// Processor debug commands. Available only after emulation has been started.
#include "pch.h"

namespace Gekko
{
	// Run processor until break or stop
	static Json::Value* cmd_run(std::vector<std::string>& args)
	{
		Gekko->Run();
		return nullptr;
	}

	// Stop processor execution
	static Json::Value* cmd_stop(std::vector<std::string>& args)
	{
		if (Gekko->IsRunning())
		{
			Gekko->Suspend();
		}
		return nullptr;
	}

	void gekko_init_handlers()
	{
		Debug::Hub.AddCmd("run", cmd_run);
		Debug::Hub.AddCmd("stop", cmd_stop);
	}
}
