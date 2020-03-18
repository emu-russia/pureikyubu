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
		DBReport("--- Gekko debug commands -----------------------------------------------------\n");
		DBReport("    run				   - Run processor until break or stop\n");
		DBReport("    stop				   - Stop processor execution\n");
		DBReport("\n");
	}

	// Run processor until break or stop
	void cmd_run(std::vector<std::string>& args)
	{
		if (!emu.running) con_run_execute();
	}

	// Stop processor execution
	void cmd_stop(std::vector<std::string>& args)
	{
		if (emu.running) con_break();
	}

}
