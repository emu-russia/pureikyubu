// Processor debug commands

#pragma once

namespace Debug
{
	void gekko_init_handlers();
	void gekko_help();

	Json::Value* cmd_run(std::vector<std::string>& args);		// Run processor until break or stop	
	Json::Value* cmd_stop(std::vector<std::string>& args);		// Stop processor execution
}

