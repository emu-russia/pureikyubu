
#pragma once

void	cmd_init_handlers();

Json::Value* cmd_showpc(std::vector<std::string>& args);		// set disassembly cursor to PC
Json::Value* cmd_showldst(std::vector<std::string>& args);	// show data pointed by LD/ST-opcode
Json::Value* cmd_unload(std::vector<std::string>& args);		// unload file
Json::Value* cmd_blr(std::vector<std::string>& args);		// insert BLR instruction at cursor (with value)
Json::Value* cmd_boot(std::vector<std::string>& args);		// load file
Json::Value* cmd_d(std::vector<std::string>& args);			// show data at address
Json::Value* cmd_denop(std::vector<std::string>& args);		// restore old NOP'ed value
Json::Value* cmd_disa(std::vector<std::string>& args);		// dump disassembly to text file
Json::Value* cmd_dop(std::vector<std::string>& args);		// apply patches
Json::Value* cmd_full(std::vector<std::string>& args);		// full screen mode
Json::Value* cmd_help(std::vector<std::string>& args);		// help
Json::Value* cmd_log(std::vector<std::string>& args);		// log control
Json::Value* cmd_logfile(std::vector<std::string>& args);	// set log file
Json::Value* cmd_lr(std::vector<std::string>& args);			// show LR back chain
Json::Value* cmd_nop(std::vector<std::string>& args);		// insert NOP instruction at cursor
Json::Value* cmd_plist(std::vector<std::string>& args);		// list all patch data
Json::Value* cmd_script(std::vector<std::string>& args);		// execute batch script
Json::Value* cmd_sd1(std::vector<std::string>& args);		// show data at "small data #1" register
Json::Value* cmd_sd2(std::vector<std::string>& args);		// show data at "small data #2" register
Json::Value* cmd_sop(std::vector<std::string>& args);		// search opcodes (down)
Json::Value* cmd_tree(std::vector<std::string>& args);		// show call tree
Json::Value* cmd_u(std::vector<std::string>& args);			// set disassembly address
