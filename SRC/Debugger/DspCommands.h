// Dsp debug commands

#pragma once

void	dsp_init_handlers();
void    dsp_help();

void    cmd_dspdisa(int argc, char argv[][CON_LINELEN]);	// disasm dsp ucode to file
void    cmd_dregs(int argc, char argv[][CON_LINELEN]);	// Show dsp registers
void    cmd_dmem(int argc, char argv[][CON_LINELEN]);	// Dump DSP DMEM
void    cmd_imem(int argc, char argv[][CON_LINELEN]);	// Dump DSP IMEM
void    cmd_drun(int argc, char argv[][CON_LINELEN]);	// Run DSP thread until break, halt or dstop
void    cmd_dstop(int argc, char argv[][CON_LINELEN]);	// Stop DSP thread
void    cmd_dstep(int argc, char argv[][CON_LINELEN]);	// Step DSP instruction
void    cmd_dbrk(int argc, char argv[][CON_LINELEN]);	// Add IMEM breakpoint
void    cmd_dunbrk(int argc, char argv[][CON_LINELEN]);	// Clear all IMEM breakpoints
void    cmd_dlist(int argc, char argv[][CON_LINELEN]);	// List IMEM breakpoints
void    cmd_dpc(int argc, char argv[][CON_LINELEN]);	// Set DSP pc
void    cmd_dreset(int argc, char argv[][CON_LINELEN]);	// Issue DSP reset
void    cmd_du(int argc, char argv[][CON_LINELEN]);		// Disassemble some DSP instructions at program counter
void    cmd_dst(int argc, char argv[][CON_LINELEN]);	// Dump DSP call stack
void    cmd_difx(int argc, char argv[][CON_LINELEN]);	// Dump DSP IFX
