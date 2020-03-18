
#pragma once

void	cmd_init_handlers();

void    cmd_showpc(std::vector<std::string>& args);		// set disassembly cursor to PC
void    cmd_showldst(std::vector<std::string>& args);	// show data pointed by LD/ST-opcode
void    cmd_unload(std::vector<std::string>& args);		// unload file
void	cmd_exit(std::vector<std::string>& args);		// exit
void    cmd_blr(std::vector<std::string>& args);		// insert BLR instruction at cursor (with value)
void    cmd_boot(std::vector<std::string>& args);		// load file
void    cmd_d(std::vector<std::string>& args);			// show data at address
void    cmd_denop(std::vector<std::string>& args);		// restore old NOP'ed value
void    cmd_disa(std::vector<std::string>& args);		// dump disassembly to text file
void    cmd_dop(std::vector<std::string>& args);		// apply patches
void    cmd_dvdopen(std::vector<std::string>& args);	// get DVD file position
void    cmd_full(std::vector<std::string>& args);		// full screen mode
void    cmd_help(std::vector<std::string>& args);		// help
void    cmd_log(std::vector<std::string>& args);		// log control
void    cmd_logfile(std::vector<std::string>& args);	// set log file
void    cmd_lr(std::vector<std::string>& args);			// show LR back chain
void    cmd_name(std::vector<std::string>& args);		// add symbol
void    cmd_nop(std::vector<std::string>& args);		// insert NOP instruction at cursor
void    cmd_ostest(std::vector<std::string>& args);		// test OS HLE internals
void    cmd_plist(std::vector<std::string>& args);		// list all patch data
void    cmd_r(std::vector<std::string>& args);			// register operations
void    cmd_savemap(std::vector<std::string>& args);	// save symbolic map into file
void    cmd_script(std::vector<std::string>& args);		// execute batch script
void    cmd_sd1(std::vector<std::string>& args);		// show data at "small data #1" register
void    cmd_sd2(std::vector<std::string>& args);		// show data at "small data #2" register
void    cmd_sop(std::vector<std::string>& args);		// search opcodes (down)
void    cmd_stat(std::vector<std::string>& args);		// show hardware state/stats
void    cmd_syms(std::vector<std::string>& args);		// show symbolic info
void    cmd_tree(std::vector<std::string>& args);		// show call tree
void    cmd_top10(std::vector<std::string>& args);		// show HLE calls toplist
void    cmd_u(std::vector<std::string>& args);			// set disassembly address

#include "DspCommands.h"    // dsp command handlers
#include "HwCommands.h"    // HW debug command handlers
#include "GekkoCommands.h"		// Processor debug commands
