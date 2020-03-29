// Various commands for debugging hardware (Flipper)

#pragma once

namespace Debug
{
	void hw_init_handlers();
	void hw_help();

	Json::Value* cmd_ramload(std::vector<std::string>& args);	// Load binary file to main memory
	Json::Value* cmd_ramsave(std::vector<std::string>& args);	// Save main memory content to file
	Json::Value* cmd_aramload(std::vector<std::string>& args);	// Load binary file to ARAM
	Json::Value* cmd_aramsave(std::vector<std::string>& args);	// Save ARAM content to file

};
