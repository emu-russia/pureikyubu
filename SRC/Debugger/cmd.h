
#pragma once

typedef void (*cmd_handler)(int argc, char argv[][CON_LINELEN]);

void    con_command(int argc, char argv[][CON_LINELEN], int lnum=0);
void	cmd_init_handlers();

void    cmd_showpc(int argc, char argv[][CON_LINELEN]);		// set disassembly cursor to PC
void    cmd_showldst(int argc, char argv[][CON_LINELEN]);	// show data pointed by LD/ST-opcode
void    cmd_unload(int argc, char argv[][CON_LINELEN]);		// unload file
void	cmd_exit(int argc, char argv[][CON_LINELEN]);		// exit
void    cmd_blr(int argc, char argv[][CON_LINELEN]);		// insert BLR instruction at cursor (with value)
void    cmd_boot(int argc, char argv[][CON_LINELEN]);		// load file
void    cmd_d(int argc, char argv[][CON_LINELEN]);			// show data at address
void    cmd_denop(int argc, char argv[][CON_LINELEN]);		// restore old NOP'ed value
void    cmd_disa(int argc, char argv[][CON_LINELEN]);		// dump disassembly to text file
void    cmd_dop(int argc, char argv[][CON_LINELEN]);		// apply patches
void    cmd_dvdopen(int argc, char argv[][CON_LINELEN]);	// get DVD file position
void    cmd_full(int argc, char argv[][CON_LINELEN]);		// full screen mode
void    cmd_help(int argc, char argv[][CON_LINELEN]);		// help
void    cmd_log(int argc, char argv[][CON_LINELEN]);		// log control
void    cmd_logfile(int argc, char argv[][CON_LINELEN]);	// set log file
void    cmd_lr(int argc, char argv[][CON_LINELEN]);			// show LR back chain
void    cmd_name(int argc, char argv[][CON_LINELEN]);		// add symbol
void    cmd_nop(int argc, char argv[][CON_LINELEN]);		// insert NOP instruction at cursor
void    cmd_ostest(int argc, char argv[][CON_LINELEN]);		// test OS HLE internals
void    cmd_plist(int argc, char argv[][CON_LINELEN]);		// list all patch data
void    cmd_r(int argc, char argv[][CON_LINELEN]);			// register operations
void    cmd_savemap(int argc, char argv[][CON_LINELEN]);	// save symbolic map into file
void    cmd_script(int argc, char argv[][CON_LINELEN]);		// execute batch script
void    cmd_sd1(int argc, char argv[][CON_LINELEN]);		// show data at "small data #1" register
void    cmd_sd2(int argc, char argv[][CON_LINELEN]);		// show data at "small data #2" register
void    cmd_sop(int argc, char argv[][CON_LINELEN]);		// search opcodes (down)
void    cmd_stat(int argc, char argv[][CON_LINELEN]);		// show hardware state/stats
void    cmd_syms(int argc, char argv[][CON_LINELEN]);		// show symbolic info
void    cmd_tree(int argc, char argv[][CON_LINELEN]);		// show call tree
void    cmd_top10(int argc, char argv[][CON_LINELEN]);		// show HLE calls toplist
void    cmd_u(int argc, char argv[][CON_LINELEN]);			// set disassembly address
