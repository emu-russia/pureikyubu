// Various commands for debugging hardware (Flipper)

#pragma once

namespace Debug
{
	void hw_init_handlers();
	void hw_help();

	void cmd_ramload(std::vector<std::string>& args);	// Load binary file to main memory
	void cmd_ramsave(std::vector<std::string>& args);	// Save main memory content to file
	void cmd_aramload(std::vector<std::string>& args);	// Load binary file to ARAM
	void cmd_aramsave(std::vector<std::string>& args);	// Save ARAM content to file

};
