// Processor debug commands
#include "pch.h"

namespace Debug
{
	void gekko_init_handlers()
	{
		con.cmds["run"] = cmd_run;
		con.cmds["stop"] = cmd_stop;
	}

	void gekko_help()
	{
		DBReport2(DbgChannel::Header, "## Gekko Debug Commands\n");
		DBReport("    run                  - Run processor until break or stop\n");
		DBReport("    stop                 - Stop processor execution\n");
		DBReport("\n");
	}

	// Run processor until break or stop
	void cmd_run(std::vector<std::string>& args)
	{
		if (emu.core)
		{
			emu.core->Run();
		}
	}

	// Stop processor execution
	void cmd_stop(std::vector<std::string>& args)
	{
		if (emu.core)
		{
			if (emu.core->IsRunning()) con_break();
		}
	}

}
