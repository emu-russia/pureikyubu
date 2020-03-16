// Dsp debug commands

#pragma once

void	dsp_init_handlers();
void    dsp_help();

void    cmd_dspdisa(std::vector<std::string>& args);	// disasm dsp ucode to file
void    cmd_dregs(std::vector<std::string>& args);	// Show dsp registers
void    cmd_dmem(std::vector<std::string>& args);	// Dump DSP DMEM
void    cmd_imem(std::vector<std::string>& args);	// Dump DSP IMEM
void    cmd_drun(std::vector<std::string>& args);	// Run DSP thread until break, halt or dstop
void    cmd_dstop(std::vector<std::string>& args);	// Stop DSP thread
void    cmd_dstep(std::vector<std::string>& args);	// Step DSP instruction
void    cmd_dbrk(std::vector<std::string>& args);	// Add IMEM breakpoint
void    cmd_dunbrk(std::vector<std::string>& args);	// Clear all IMEM breakpoints
void    cmd_dlist(std::vector<std::string>& args);	// List IMEM breakpoints
void    cmd_dpc(std::vector<std::string>& args);	// Set DSP pc
void    cmd_dreset(std::vector<std::string>& args);	// Issue DSP reset
void    cmd_du(std::vector<std::string>& args);		// Disassemble some DSP instructions at program counter
void    cmd_dst(std::vector<std::string>& args);	// Dump DSP call stack
void    cmd_difx(std::vector<std::string>& args);	// Dump DSP IFX
