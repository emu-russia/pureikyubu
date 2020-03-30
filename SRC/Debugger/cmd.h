
#pragma once

void	cmd_init_handlers();

Json::Value* cmd_showpc(std::vector<std::string>& args);		// set disassembly cursor to PC
Json::Value* cmd_showldst(std::vector<std::string>& args);	// show data pointed by LD/ST-opcode
Json::Value* cmd_unload(std::vector<std::string>& args);		// unload file
Json::Value* cmd_exit(std::vector<std::string>& args);		// exit
Json::Value* cmd_blr(std::vector<std::string>& args);		// insert BLR instruction at cursor (with value)
Json::Value* cmd_boot(std::vector<std::string>& args);		// load file
Json::Value* cmd_d(std::vector<std::string>& args);			// show data at address
Json::Value* cmd_denop(std::vector<std::string>& args);		// restore old NOP'ed value
Json::Value* cmd_disa(std::vector<std::string>& args);		// dump disassembly to text file
Json::Value* cmd_dop(std::vector<std::string>& args);		// apply patches
Json::Value* cmd_dvdopen(std::vector<std::string>& args);	// get DVD file position
Json::Value* cmd_full(std::vector<std::string>& args);		// full screen mode
Json::Value* cmd_help(std::vector<std::string>& args);		// help
Json::Value* cmd_log(std::vector<std::string>& args);		// log control
Json::Value* cmd_logfile(std::vector<std::string>& args);	// set log file
Json::Value* cmd_lr(std::vector<std::string>& args);			// show LR back chain
Json::Value* cmd_name(std::vector<std::string>& args);		// add symbol
Json::Value* cmd_nop(std::vector<std::string>& args);		// insert NOP instruction at cursor
Json::Value* cmd_ostest(std::vector<std::string>& args);		// test OS HLE internals
Json::Value* cmd_plist(std::vector<std::string>& args);		// list all patch data
Json::Value* cmd_r(std::vector<std::string>& args);			// register operations
Json::Value* cmd_savemap(std::vector<std::string>& args);	// save symbolic map into file
Json::Value* cmd_script(std::vector<std::string>& args);		// execute batch script
Json::Value* cmd_sd1(std::vector<std::string>& args);		// show data at "small data #1" register
Json::Value* cmd_sd2(std::vector<std::string>& args);		// show data at "small data #2" register
Json::Value* cmd_sop(std::vector<std::string>& args);		// search opcodes (down)
Json::Value* cmd_stat(std::vector<std::string>& args);		// show hardware state/stats
Json::Value* cmd_syms(std::vector<std::string>& args);		// show symbolic info
Json::Value* cmd_tree(std::vector<std::string>& args);		// show call tree
Json::Value* cmd_top10(std::vector<std::string>& args);		// show HLE calls toplist
Json::Value* cmd_u(std::vector<std::string>& args);			// set disassembly address
Json::Value* cmd_sleep(std::vector<std::string>& args);		// Sleep specified number of milliseconds

#include "HwCommands.h"    // HW debug command handlers
#include "GekkoCommands.h"		// Processor debug commands
