// Dsp debug commands

#pragma once

namespace Debug
{
	void dsp_init_handlers();
	void dsp_help();

	Json::Value* cmd_dspdisa(std::vector<std::string>& args);	// disasm dsp ucode to file
	Json::Value* cmd_dregs(std::vector<std::string>& args);	// Show dsp registers
	Json::Value* cmd_dmem(std::vector<std::string>& args);	// Dump DSP DMEM
	Json::Value* cmd_imem(std::vector<std::string>& args);	// Dump DSP IMEM
	Json::Value* cmd_drun(std::vector<std::string>& args);	// Run DSP thread until break, halt or dstop
	Json::Value* cmd_dstop(std::vector<std::string>& args);	// Stop DSP thread
	Json::Value* cmd_dstep(std::vector<std::string>& args);	// Step DSP instruction
	Json::Value* cmd_dbrk(std::vector<std::string>& args);	// Add IMEM breakpoint
	Json::Value* cmd_dcan(std::vector<std::string>& args);	// Add IMEM canary
	Json::Value* cmd_dbrkclr(std::vector<std::string>& args);	// Clear all IMEM breakpoints
	Json::Value* cmd_dcanclr(std::vector<std::string>& args);	// Clear all IMEM canaries
	Json::Value* cmd_dlist(std::vector<std::string>& args);	// List IMEM breakpoints and canaries
	Json::Value* cmd_dpc(std::vector<std::string>& args);	// Set DSP pc
	Json::Value* cmd_dreset(std::vector<std::string>& args);	// Issue DSP reset
	Json::Value* cmd_du(std::vector<std::string>& args);		// Disassemble some DSP instructions at program counter
	Json::Value* cmd_dst(std::vector<std::string>& args);	// Dump DSP call stack
	Json::Value* cmd_difx(std::vector<std::string>& args);	// Dump DSP IFX
	Json::Value* cmd_cpumbox(std::vector<std::string>& args);	// Write message to CPU Mailbox
	Json::Value* cmd_dspmbox(std::vector<std::string>& args);	// Read message from DSP Mailbox
	Json::Value* cmd_cpudspint(std::vector<std::string>& args);	// Send CPU->DSP interrupt
	Json::Value* cmd_dspcpuint(std::vector<std::string>& args);	// Send DSP->CPU interrupt
	Json::Value* cmd_dreg(std::vector<std::string>& args);	// Modify DSP register
}
