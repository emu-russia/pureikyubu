// Processor debug commands.
#include "pch.h"

namespace Debug
{
	// Run processor until break or stop
	static Json::Value* CmdRun(std::vector<std::string>& args)
	{
		Core->Run();
		return nullptr;
	}

	// Stop processor execution
	static Json::Value* CmdStop(std::vector<std::string>& args)
	{
		if (Core->IsRunning())
		{
			Core->Suspend();
		}
		return nullptr;
	}

	// r

	// Get pointer to Gekko register.
	static uint32_t* getreg(const char* name)
	{
		if (!_stricmp(name, "r0")) return &Core->regs.gpr[0];
		else if (!_stricmp(name, "r1")) return &Core->regs.gpr[1];
		else if (!_stricmp(name, "r2")) return &Core->regs.gpr[2];
		else if (!_stricmp(name, "r3")) return &Core->regs.gpr[3];
		else if (!_stricmp(name, "r4")) return &Core->regs.gpr[4];
		else if (!_stricmp(name, "r5")) return &Core->regs.gpr[5];
		else if (!_stricmp(name, "r6")) return &Core->regs.gpr[6];
		else if (!_stricmp(name, "r7")) return &Core->regs.gpr[7];
		else if (!_stricmp(name, "r8")) return &Core->regs.gpr[8];
		else if (!_stricmp(name, "r9")) return &Core->regs.gpr[9];
		else if (!_stricmp(name, "r10")) return &Core->regs.gpr[10];
		else if (!_stricmp(name, "r11")) return &Core->regs.gpr[11];
		else if (!_stricmp(name, "r12")) return &Core->regs.gpr[12];
		else if (!_stricmp(name, "r13")) return &Core->regs.gpr[13];
		else if (!_stricmp(name, "r14")) return &Core->regs.gpr[14];
		else if (!_stricmp(name, "r15")) return &Core->regs.gpr[15];
		else if (!_stricmp(name, "r16")) return &Core->regs.gpr[16];
		else if (!_stricmp(name, "r17")) return &Core->regs.gpr[17];
		else if (!_stricmp(name, "r18")) return &Core->regs.gpr[18];
		else if (!_stricmp(name, "r19")) return &Core->regs.gpr[19];
		else if (!_stricmp(name, "r20")) return &Core->regs.gpr[20];
		else if (!_stricmp(name, "r21")) return &Core->regs.gpr[21];
		else if (!_stricmp(name, "r22")) return &Core->regs.gpr[22];
		else if (!_stricmp(name, "r23")) return &Core->regs.gpr[23];
		else if (!_stricmp(name, "r24")) return &Core->regs.gpr[24];
		else if (!_stricmp(name, "r25")) return &Core->regs.gpr[25];
		else if (!_stricmp(name, "r26")) return &Core->regs.gpr[26];
		else if (!_stricmp(name, "r27")) return &Core->regs.gpr[27];
		else if (!_stricmp(name, "r28")) return &Core->regs.gpr[28];
		else if (!_stricmp(name, "r29")) return &Core->regs.gpr[29];
		else if (!_stricmp(name, "r30")) return &Core->regs.gpr[30];
		else if (!_stricmp(name, "r31")) return &Core->regs.gpr[31];

		else if (!_stricmp(name, "sp")) return &Core->regs.gpr[1];
		else if (!_stricmp(name, "sd1")) return &Core->regs.gpr[13];
		else if (!_stricmp(name, "sd2")) return &Core->regs.gpr[2];

		else if (!_stricmp(name, "cr")) return &Core->regs.cr;
		else if (!_stricmp(name, "fpscr")) return &Core->regs.fpscr;
		else if (!_stricmp(name, "xer")) return &Core->regs.spr[(int)Gekko::SPR::XER];
		else if (!_stricmp(name, "lr")) return &Core->regs.spr[(int)Gekko::SPR::LR];
		else if (!_stricmp(name, "ctr")) return &Core->regs.spr[(int)Gekko::SPR::CTR];
		else if (!_stricmp(name, "msr")) return &Core->regs.msr;

		else if (!_stricmp(name, "sr0")) return &Core->regs.sr[0];
		else if (!_stricmp(name, "sr1")) return &Core->regs.sr[1];
		else if (!_stricmp(name, "sr2")) return &Core->regs.sr[2];
		else if (!_stricmp(name, "sr3")) return &Core->regs.sr[3];
		else if (!_stricmp(name, "sr4")) return &Core->regs.sr[4];
		else if (!_stricmp(name, "sr5")) return &Core->regs.sr[5];
		else if (!_stricmp(name, "sr6")) return &Core->regs.sr[6];
		else if (!_stricmp(name, "sr7")) return &Core->regs.sr[7];
		else if (!_stricmp(name, "sr8")) return &Core->regs.sr[8];
		else if (!_stricmp(name, "sr9")) return &Core->regs.sr[9];
		else if (!_stricmp(name, "sr10")) return &Core->regs.sr[10];
		else if (!_stricmp(name, "sr11")) return &Core->regs.sr[11];
		else if (!_stricmp(name, "sr12")) return &Core->regs.sr[12];
		else if (!_stricmp(name, "sr13")) return &Core->regs.sr[13];
		else if (!_stricmp(name, "sr14")) return &Core->regs.sr[14];
		else if (!_stricmp(name, "sr15")) return &Core->regs.sr[15];

		else if (!_stricmp(name, "ibat0u")) return &Core->regs.spr[(int)Gekko::SPR::IBAT0U];
		else if (!_stricmp(name, "ibat1u")) return &Core->regs.spr[(int)Gekko::SPR::IBAT1U];
		else if (!_stricmp(name, "ibat2u")) return &Core->regs.spr[(int)Gekko::SPR::IBAT2U];
		else if (!_stricmp(name, "ibat3u")) return &Core->regs.spr[(int)Gekko::SPR::IBAT3U];
		else if (!_stricmp(name, "ibat0l")) return &Core->regs.spr[(int)Gekko::SPR::IBAT0L];
		else if (!_stricmp(name, "ibat1l")) return &Core->regs.spr[(int)Gekko::SPR::IBAT1L];
		else if (!_stricmp(name, "ibat2l")) return &Core->regs.spr[(int)Gekko::SPR::IBAT2L];
		else if (!_stricmp(name, "ibat3l")) return &Core->regs.spr[(int)Gekko::SPR::IBAT3L];
		else if (!_stricmp(name, "dbat0u")) return &Core->regs.spr[(int)Gekko::SPR::DBAT0U];
		else if (!_stricmp(name, "dbat1u")) return &Core->regs.spr[(int)Gekko::SPR::DBAT1U];
		else if (!_stricmp(name, "dbat2u")) return &Core->regs.spr[(int)Gekko::SPR::DBAT2U];
		else if (!_stricmp(name, "dbat3u")) return &Core->regs.spr[(int)Gekko::SPR::DBAT3U];
		else if (!_stricmp(name, "dbat0l")) return &Core->regs.spr[(int)Gekko::SPR::DBAT0L];
		else if (!_stricmp(name, "dbat1l")) return &Core->regs.spr[(int)Gekko::SPR::DBAT1L];
		else if (!_stricmp(name, "dbat2l")) return &Core->regs.spr[(int)Gekko::SPR::DBAT2L];
		else if (!_stricmp(name, "dbat3l")) return &Core->regs.spr[(int)Gekko::SPR::DBAT3L];

		else if (!_stricmp(name, "sdr1")) return &Core->regs.spr[(int)Gekko::SPR::SDR1];
		else if (!_stricmp(name, "sprg0")) return &Core->regs.spr[(int)Gekko::SPR::SPRG0];
		else if (!_stricmp(name, "sprg1")) return &Core->regs.spr[(int)Gekko::SPR::SPRG1];
		else if (!_stricmp(name, "sprg2")) return &Core->regs.spr[(int)Gekko::SPR::SPRG2];
		else if (!_stricmp(name, "sprg3")) return &Core->regs.spr[(int)Gekko::SPR::SPRG3];
		else if (!_stricmp(name, "dar")) return &Core->regs.spr[(int)Gekko::SPR::DAR];
		else if (!_stricmp(name, "dsisr")) return &Core->regs.spr[(int)Gekko::SPR::DSISR];
		else if (!_stricmp(name, "srr0")) return &Core->regs.spr[(int)Gekko::SPR::SRR0];
		else if (!_stricmp(name, "srr1")) return &Core->regs.spr[(int)Gekko::SPR::SRR1];
		else if (!_stricmp(name, "pmc1")) return &Core->regs.spr[953];
		else if (!_stricmp(name, "pmc2")) return &Core->regs.spr[954];
		else if (!_stricmp(name, "pmc3")) return &Core->regs.spr[957];
		else if (!_stricmp(name, "pmc4")) return &Core->regs.spr[958];
		else if (!_stricmp(name, "mmcr0")) return &Core->regs.spr[952];
		else if (!_stricmp(name, "mmcr1")) return &Core->regs.spr[956];
		else if (!_stricmp(name, "sia")) return &Core->regs.spr[955];
		else if (!_stricmp(name, "sda")) return &Core->regs.spr[959];

		else if (!_stricmp(name, "gqr0")) return &Core->regs.spr[(int)Gekko::SPR::GQR0];
		else if (!_stricmp(name, "gqr1")) return &Core->regs.spr[(int)Gekko::SPR::GQR1];
		else if (!_stricmp(name, "gqr2")) return &Core->regs.spr[(int)Gekko::SPR::GQR2];
		else if (!_stricmp(name, "gqr3")) return &Core->regs.spr[(int)Gekko::SPR::GQR3];
		else if (!_stricmp(name, "gqr4")) return &Core->regs.spr[(int)Gekko::SPR::GQR4];
		else if (!_stricmp(name, "gqr5")) return &Core->regs.spr[(int)Gekko::SPR::GQR5];
		else if (!_stricmp(name, "gqr6")) return &Core->regs.spr[(int)Gekko::SPR::GQR6];
		else if (!_stricmp(name, "gqr7")) return &Core->regs.spr[(int)Gekko::SPR::GQR7];

		else if (!_stricmp(name, "hid0")) return &Core->regs.spr[(int)Gekko::SPR::HID0];
		else if (!_stricmp(name, "hid1")) return &Core->regs.spr[(int)Gekko::SPR::HID1];
		else if (!_stricmp(name, "hid2")) return &Core->regs.spr[(int)Gekko::SPR::HID2];

		else if (!_stricmp(name, "dabr")) return &Core->regs.spr[(int)Gekko::SPR::DABR];
		else if (!_stricmp(name, "iabr")) return &Core->regs.spr[(int)Gekko::SPR::IABR];
		else if (!_stricmp(name, "wpar")) return &Core->regs.spr[(int)Gekko::SPR::WPAR];
		else if (!_stricmp(name, "l2cr")) return &Core->regs.spr[1017];
		else if (!_stricmp(name, "dmau")) return &Core->regs.spr[(int)Gekko::SPR::DMAU];
		else if (!_stricmp(name, "dmal")) return &Core->regs.spr[(int)Gekko::SPR::DMAL];
		else if (!_stricmp(name, "thrm1")) return &Core->regs.spr[1020];
		else if (!_stricmp(name, "thrm2")) return &Core->regs.spr[1021];
		else if (!_stricmp(name, "thrm3")) return &Core->regs.spr[1022];
		else if (!_stricmp(name, "ictc")) return &Core->regs.spr[1019];

		else if (!_stricmp(name, "pc")) return &Core->regs.pc;   // Wow !

		return NULL;
	}

	// Operations.
	static uint32_t op_replace(uint32_t a, uint32_t b) { return b; }
	static uint32_t op_add(uint32_t a, uint32_t b) { return a + b; }
	static uint32_t op_sub(uint32_t a, uint32_t b) { return a - b; }
	static uint32_t op_mul(uint32_t a, uint32_t b) { return a * b; }
	static uint32_t op_div(uint32_t a, uint32_t b) { return b ? (a / b) : 0; }
	static uint32_t op_or(uint32_t a, uint32_t b) { return a | b; }
	static uint32_t op_and(uint32_t a, uint32_t b) { return a & b; }
	static uint32_t op_xor(uint32_t a, uint32_t b) { return a ^ b; }
	static uint32_t op_shl(uint32_t a, uint32_t b) { return a << b; }
	static uint32_t op_shr(uint32_t a, uint32_t b) { return a >> b; }

	// Special handling for MSR register.
	static void describe_msr(uint32_t msr_val)
	{
		static const char* fpmod[4] =
		{
			"exceptions disabled",
			"imprecise nonrecoverable",
			"imprecise recoverable",
			"precise mode",
		};
		int f, fe[2];

		Report(Channel::Norm, "MSR: 0x%08X\n", msr_val);

		if (msr_val & MSR_POW) Report(Channel::Norm, "MSR[POW]: 1, power management enabled\n");
		else Report(Channel::Norm, "MSR[POW]: 0, power management disabled\n");
		if (msr_val & MSR_ILE) Report(Channel::Norm, "MSR[ILE]: 1\n");
		else Report(Channel::Norm, "MSR[ILE]: 0\n");
		if (msr_val & MSR_EE) Report(Channel::Norm, "MSR[EE] : 1, external interrupts and decrementer exception are enabled\n");
		else Report(Channel::Norm, "MSR[EE] : 0, external interrupts and decrementer exception are disabled\n");
		if (msr_val & MSR_PR) Report(Channel::Norm, "MSR[PR] : 1, processor execute in user mode (UISA)\n");
		else Report(Channel::Norm, "MSR[PR] : 0, processor execute in supervisor mode (OEA)\n");
		if (msr_val & MSR_FP) Report(Channel::Norm, "MSR[FP] : 1, floating-point is available\n");
		else Report(Channel::Norm, "MSR[FP] : 0, floating-point unavailable\n");
		if (msr_val & MSR_ME) Report(Channel::Norm, "MSR[ME] : 1, machine check exceptions are enabled\n");
		else Report(Channel::Norm, "MSR[ME] : 0, machine check exceptions are disabled\n");

		fe[0] = msr_val & MSR_FE0 ? 1 : 0;
		fe[1] = msr_val & MSR_FE1 ? 1 : 0;
		f = (fe[0] << 1) | (fe[1]);
		Report(Channel::Norm, "MSR[FE] : %i, floating-point %s\n", f, fpmod[f]);

		if (msr_val & MSR_SE) Report(Channel::Norm, "MSR[SE] : 1, single-step tracing is enabled\n");
		else Report(Channel::Norm, "MSR[SE] : 0, single-step tracing is disabled\n");
		if (msr_val & MSR_BE) Report(Channel::Norm, "MSR[BE] : 1, branch tracing is enabled\n");
		else Report(Channel::Norm, "MSR[BE] : 0, branch tracing is disabled\n");
		if (msr_val & MSR_IP) Report(Channel::Norm, "MSR[IP] : 1, exception prefix to physical address is 0xFFFn_nnnn\n");
		else Report(Channel::Norm, "MSR[IP] : 0, exception prefix to physical address is 0x000n_nnnn\n");
		if (msr_val & MSR_IR) Report(Channel::Norm, "MSR[IR] : 1, instruction address translation is enabled\n");
		else Report(Channel::Norm, "MSR[IR] : 0, instruction address translation is disabled\n");
		if (msr_val & MSR_DR) Report(Channel::Norm, "MSR[DR] : 1, data address translation is enabled\n");
		else Report(Channel::Norm, "MSR[DR] : 0, data address translation is disabled\n");
		if (msr_val & MSR_PM) Report(Channel::Norm, "MSR[PM] : 1, performance monitoring is enabled for this thread\n");
		else Report(Channel::Norm, "MSR[PM] : 0, performance monitoring is disabled for this thread\n");
		if (msr_val & MSR_RI) Report(Channel::Norm, "MSR[RI] : 1\n");
		else Report(Channel::Norm, "MSR[RI] : 0\n");
		if (msr_val & MSR_LE) Report(Channel::Norm, "MSR[LE] : 1, processor runs in little-endian mode\n");
		else Report(Channel::Norm, "MSR[LE] : 0, processor runs in big-endian mode\n");
	}

	// HID0 info
	static void describe_hid0(uint32_t val)
	{
		Report(Channel::Norm, "HID0: 0x%08X\n", val);

		if (val & HID0_EMCP) Report(Channel::Norm, "HID0[EMCP] : 1, Asserting MCP causes checkstop or a machine check\n");
		else Report(Channel::Norm, "HID0[EMCP] : 0, Masks MCP. Asserting MCP does not generate a machine check exception or a checkstop\n");
		if (val & HID0_DBP) Report(Channel::Norm, "HID0[DBP]  : 1, Disable parity generation\n");
		else Report(Channel::Norm, "HID0[DBP]  : 0, Parity generation is enabled\n");
		if (val & HID0_EBA) Report(Channel::Norm, "HID0[EBA]  : 1, Allows a address parity error to cause a checkstop or a machine check\n");
		else Report(Channel::Norm, "HID0[EBA]  : 0, Prevents address parity checking\n");
		if (val & HID0_EBD) Report(Channel::Norm, "HID0[EBD]  : 1, Allows a data parity error to cause a checkstop or machine check\n");
		else Report(Channel::Norm, "HID0[EBD]  : 0, Parity checking is disabled\n");
		if (val & HID0_PAR) Report(Channel::Norm, "HID0[PAR]  : 1, Alters bus protocol slightly by preventing the processor from driving ARTRY to high\n");
		else Report(Channel::Norm, "HID0[PAR]  : 0, Precharge of ARTRY enabled\n");
		if (val & HID0_DOZE) Report(Channel::Norm, "HID0[DOZE] : 1, Doze mode enabled\n");
		else Report(Channel::Norm, "HID0[DOZE] : 0, Doze mode disabled\n");
		if (val & HID0_NAP) Report(Channel::Norm, "HID0[NAP]  : 1, Nap mode enabled\n");
		else Report(Channel::Norm, "HID0[NAP]  : 0, Nap mode disabled\n");
		if (val & HID0_SLEEP) Report(Channel::Norm, "HID0[SLEEP]: 1, Sleep mode enabled\n");
		else Report(Channel::Norm, "HID0[SLEEP]: 0, Sleep mode disabled\n");
		if (val & HID0_DPM) Report(Channel::Norm, "HID0[DPM]  : 1, Dynamic power management is enabled\n");
		else Report(Channel::Norm, "HID0[DPM]  : 0, Dynamic power management is disabled\n");
		if (val & HID0_NHR) Report(Channel::Norm, "HID0[NHR]  : 1, Hard reset has not occurred\n");
		else Report(Channel::Norm, "HID0[NHR]  : 0, Hard reset occurred\n");
		if (val & HID0_ICE) Report(Channel::Norm, "HID0[ICE]  : 1, Instruction cache is enabled\n");
		else Report(Channel::Norm, "HID0[ICE]  : 0, Instruction cache is disabled\n");
		if (val & HID0_DCE) Report(Channel::Norm, "HID0[DCE]  : 1, Data cache is enabled\n");
		else Report(Channel::Norm, "HID0[DCE]  : 0, Data cache is disabled\n");
		if (val & HID0_ILOCK) Report(Channel::Norm, "HID0[ILOCK]: 1, Instruction cache locked (frozen)\n");
		else Report(Channel::Norm, "HID0[ILOCK]: 0, Instruction cache not locked\n");
		if (val & HID0_DLOCK) Report(Channel::Norm, "HID0[DLOCK]: 1, Data cache locked (frozen)\n");
		else Report(Channel::Norm, "HID0[DLOCK]: 0, Data cache not locked\n");
		if (val & HID0_ICFI) Report(Channel::Norm, "HID0[ICFI] : 1, Instruction cache invalidating\n");
		else Report(Channel::Norm, "HID0[ICFI] : 0, Instruction cache is not invalidated\n");
		if (val & HID0_DCFI) Report(Channel::Norm, "HID0[DCFI] : 1, Data cache invalidating\n");
		else Report(Channel::Norm, "HID0[DCFI] : 0, Data cache is not invalidated\n");
		if (val & HID0_SPD) Report(Channel::Norm, "HID0[SPD]  : 1, Speculative bus accesses to nonguarded space disabled\n");
		else Report(Channel::Norm, "HID0[SPD]  : 0, Speculative bus accesses to nonguarded space enabled\n");
		if (val & HID0_IFEM) Report(Channel::Norm, "HID0[IFEM] : 1, Instruction fetches reflect the M bit from the WIM settings\n");
		else Report(Channel::Norm, "HID0[IFEM] : 0, Instruction fetches M bit disabled\n");
		if (val & HID0_SGE) Report(Channel::Norm, "HID0[SGE]  : 1, Store gathering is enabled\n");
		else Report(Channel::Norm, "HID0[SGE]  : 0, Store gathering is disabled \n");
		if (val & HID0_DCFA) Report(Channel::Norm, "HID0[DCFA] : 1, Data cache flush assist facility is enabled\n");
		else Report(Channel::Norm, "HID0[DCFA] : 0, Data cache flush assist facility is disabled\n");
		if (val & HID0_BTIC) Report(Channel::Norm, "HID0[BTIC] : 1, BTIC is enabled\n");
		else Report(Channel::Norm, "HID0[BTIC] : 0, BTIC is disabled\n");
		if (val & HID0_ABE) Report(Channel::Norm, "HID0[ABE]  : 1, Address-only operations are broadcast on the 60x bus\n");
		else Report(Channel::Norm, "HID0[ABE]  : 0, Address-only operations affect only local L1 and L2 caches and are not broadcast\n");
		if (val & HID0_BHT) Report(Channel::Norm, "HID0[BHT]  : 1, Branch history enabled\n");
		else Report(Channel::Norm, "HID0[BHT]  : 0, Branch history disabled\n");
		if (val & HID0_NOOPTI) Report(Channel::Norm, "HID0[NOOPTI]: 1, The dcbt and dcbtst instructions are no-oped globally\n");
		else Report(Channel::Norm, "HID0[NOOPTI]: 0, The dcbt and dcbtst instructions are enabled\n");
	}

	static Json::Value* CmdGetSetReg(std::vector<std::string>& args)
	{
		uint32_t(*op)(uint32_t a, uint32_t b) = NULL;

		uint32_t* n = getreg(args[1].c_str());
		if (n == NULL)
		{
			Report(Channel::Norm, "unknown register : %s\n", args[1].c_str());
			return nullptr;
		}

		// show register
		if (args.size() <= 3)
		{
			if (!_stricmp(args[1].c_str(), "msr")) describe_msr(*n);
			else if (!_stricmp(args[1].c_str(), "hid0")) describe_hid0(*n);
			else Report(Channel::Norm, "%s = %i (0x%X)\n", args[1].c_str(), *n, *n);
			return nullptr;
		}

		// Get operation.
		if (!strcmp(args[2].c_str(), "=")) op = op_replace;
		else if (!strcmp(args[2].c_str(), "+")) op = op_add;
		else if (!strcmp(args[2].c_str(), "-")) op = op_sub;
		else if (!strcmp(args[2].c_str(), "*")) op = op_mul;
		else if (!strcmp(args[2].c_str(), "/")) op = op_div;
		else if (!strcmp(args[2].c_str(), "|")) op = op_or;
		else if (!strcmp(args[2].c_str(), "&")) op = op_and;
		else if (!strcmp(args[2].c_str(), "^")) op = op_xor;
		else if (!strcmp(args[2].c_str(), "<<")) op = op_shl;
		else if (!strcmp(args[2].c_str(), ">>")) op = op_shr;
		if (op == NULL)
		{
			Report(Channel::Norm, "Unknown operation: %s\n", args[2].c_str());
			return nullptr;
		}

		// New value
		uint32_t* m = getreg(args[3].c_str());
		if (m == NULL)
		{
			int i = strtoul(args[3].c_str(), NULL, 0);
			Report(Channel::Norm, "%s %s %i (0x%X)\n", args[1].c_str(), args[2].c_str(), i, i);
			*n = op(*n, i);
		}
		else
		{
			Report(Channel::Norm, "%s %s %s\n", args[1].c_str(), args[2].c_str(), args[3].c_str());
			*n = op(*n, *m);
		}

		return nullptr;
	}

	static Json::Value* CmdBreakExec(std::vector<std::string>& args)
	{
		uint32_t addr = strtoul(args[1].c_str(), nullptr, 0);
		Core->AddBreakpoint(addr);
		return nullptr;
	}

	static Json::Value* CmdBreakRead(std::vector<std::string>& args)
	{
		uint32_t addr = strtoul(args[1].c_str(), nullptr, 0);
		Core->AddReadBreak(addr);
		return nullptr;
	}

	static Json::Value* CmdBreakWrite(std::vector<std::string>& args)
	{
		uint32_t addr = strtoul(args[1].c_str(), nullptr, 0);
		Core->AddWriteBreak(addr);
		return nullptr;
	}

	static Json::Value* CmdBreakClearAll(std::vector<std::string>& args)
	{
		Core->ClearBreakpoints();
		return nullptr;
	}

	static Json::Value* CmdCacheLog(std::vector<std::string>& args)
	{
		Gekko::CacheLogLevel level = (Gekko::CacheLogLevel)atoi(args[1].c_str());
		Core->cache->SetLogLevel(level);
		return nullptr;
	}

	static Json::Value* CmdIsRunning(std::vector<std::string>& args)
	{
		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Bool;

		output->value.AsBool = Core->IsRunning();

		return output;
	}

	static Json::Value* CmdGekkoRun(std::vector<std::string>& args)
	{
		Core->Run();
		return nullptr;
	}

	static Json::Value* CmdGekkoSuspend(std::vector<std::string>& args)
	{
		Core->Suspend();
		return nullptr;
	}

	static Json::Value* CmdGekkoStep(std::vector<std::string>& args)
	{
		if (!Core->IsRunning())
		{
			Core->Step();
		}
		return nullptr;
	}

	static Json::Value* CmdGekkoSkipInstruction(std::vector<std::string>& args)
	{
		if (!Core->IsRunning())
		{
			Report(Channel::CPU, "Skipped instruction at: 0x%08X!\n", Core->regs.pc);
			Core->regs.pc += 4;
		}
		return nullptr;
	}

	static Json::Value* CmdGetGpr(std::vector<std::string>& args)
	{
		int n = atoi(args[1].c_str());

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Int;

		output->value.AsUint32 = Core->regs.gpr[n];

		return output;
	}

	static Json::Value* CmdGetPs0(std::vector<std::string>& args)
	{
		int n = atoi(args[1].c_str());

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Int;

		output->value.AsInt = Core->regs.fpr[n].uval;

		return output;
	}

	static Json::Value* CmdGetPs1(std::vector<std::string>& args)
	{
		int n = atoi(args[1].c_str());

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Int;

		output->value.AsInt = Core->regs.ps1[n].uval;

		return output;
	}

	static Json::Value* CmdGetPc(std::vector<std::string>& args)
	{
		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Int;

		output->value.AsInt = Core->regs.pc;

		return output;
	}

	static Json::Value* CmdGetMsr(std::vector<std::string>& args)
	{
		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Int;

		output->value.AsInt = Core->regs.msr;

		return output;
	}

	static Json::Value* CmdGetCr(std::vector<std::string>& args)
	{
		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Int;

		output->value.AsInt = Core->regs.cr;

		return output;
	}

	static Json::Value* CmdGetFpscr(std::vector<std::string>& args)
	{
		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Int;

		output->value.AsInt = Core->regs.fpscr;

		return output;
	}

	static Json::Value* CmdGetSpr(std::vector<std::string>& args)
	{
		int n = atoi(args[1].c_str());

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Int;

		output->value.AsUint32 = Core->regs.spr[n];

		return output;
	}

	static Json::Value* CmdGetSr(std::vector<std::string>& args)
	{
		int n = atoi(args[1].c_str());

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Int;

		output->value.AsUint32 = Core->regs.sr[n];

		return output;
	}

	static Json::Value* CmdGetTbu(std::vector<std::string>& args)
	{
		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Int;

		output->value.AsUint32 = Core->regs.tb.Part.u;

		return output;
	}

	static Json::Value* CmdGetTbl(std::vector<std::string>& args)
	{
		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Int;

		output->value.AsUint32 = Core->regs.tb.Part.l;

		return output;
	}

	static Json::Value* CmdTranslateDMmu(std::vector<std::string>& args)
	{
		uint32_t addr = strtoul(args[1].c_str(), nullptr, 0);

		uint32_t pa = Gekko::BadAddress;

		if (JDI::Hub.ExecuteFastBool("IsLoaded"))
		{
			int WIMG;
			pa = Core->EffectiveToPhysical(addr, Gekko::MmuAccess::Read, WIMG);
		}

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Int;

		output->value.AsInt = (uint64_t)MITranslatePhysicalAddress(pa, sizeof(uint32_t));

		return output;
	}

	static Json::Value* CmdTranslateIMmu(std::vector<std::string>& args)
	{
		uint32_t addr = strtoul(args[1].c_str(), nullptr, 0);

		uint32_t pa = Gekko::BadAddress;

		if (JDI::Hub.ExecuteFastBool("IsLoaded"))
		{
			int WIMG;
			pa = Core->EffectiveToPhysical(addr, Gekko::MmuAccess::Execute, WIMG);
		}

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Int;

		output->value.AsInt = (uint64_t)MITranslatePhysicalAddress(pa, sizeof(uint32_t));

		return output;
	}

	static Json::Value* CmdVirtualToPhysicalDMmu(std::vector<std::string>& args)
	{
		uint32_t addr = strtoul(args[1].c_str(), nullptr, 0);

		uint32_t pa = Gekko::BadAddress;

		if (JDI::Hub.ExecuteFastBool("IsLoaded"))
		{
			int WIMG;
			pa = Core->EffectiveToPhysical(addr, Gekko::MmuAccess::Read, WIMG);
		}

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Int;

		output->value.AsUint32 = pa;

		return output;
	}

	static Json::Value* CmdVirtualToPhysicalIMmu(std::vector<std::string>& args)
	{
		uint32_t addr = strtoul(args[1].c_str(), nullptr, 0);

		uint32_t pa = Gekko::BadAddress;

		if (JDI::Hub.ExecuteFastBool("IsLoaded"))
		{
			int WIMG;
			pa = Core->EffectiveToPhysical(addr, Gekko::MmuAccess::Execute, WIMG);
		}

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Int;

		output->value.AsUint32 = pa;

		return output;
	}

	static Json::Value* CmdGekkoTestBreakpoint(std::vector<std::string>& args)
	{
		uint32_t addr = strtoul(args[1].c_str(), nullptr, 0);

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Bool;

		output->value.AsBool = Core->IsBreakpoint(addr);

		return output;
	}

	static Json::Value* CmdGekkoToggleBreakpoint(std::vector<std::string>& args)
	{
		uint32_t addr = strtoul(args[1].c_str(), nullptr, 0);

		Core->ToggleBreakpoint(addr);

		return nullptr;
	}

	static Json::Value* CmdGekkoAddOneShotBreakpoint(std::vector<std::string>& args)
	{
		uint32_t addr = strtoul(args[1].c_str(), nullptr, 0);

		Core->AddOneShotBreakpoint(addr);

		return nullptr;
	}

	// Disassemble instruction at Gekko virtual memory address
	static Json::Value* CmdGekkoDisasm(std::vector<std::string>& args)
	{
		uint32_t addr = strtoul(args[1].c_str(), nullptr, 0);

		uint32_t pa = Gekko::BadAddress;

		if (JDI::Hub.ExecuteFastBool("IsLoaded"))
		{
			int WIMG;
			pa = Core->EffectiveToPhysical(addr, Gekko::MmuAccess::Execute, WIMG);
		}

		std::string text = "";

		uint8_t* ptr = MITranslatePhysicalAddress(pa, sizeof(uint32_t));

		if (ptr != nullptr)
		{
			Gekko::DecoderInfo info = { 0 };

			uint32_t instr = _BYTESWAP_UINT32(*(uint32_t*)ptr);

			Gekko::Decoder::Decode(addr, instr, &info);

			text = Gekko::GekkoDisasm::Disasm(addr, &info, false, false);
		}

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Array;

		output->AddAnsiString(nullptr, text.c_str());

		return output;
	}

	// Disassemble the instruction without accessing memory (all necessary information is passed through parameters)
	static Json::Value* CmdGekkoDisasmNoMemAccess(std::vector<std::string>& args)
	{
		uint32_t addr = strtoul(args[1].c_str(), nullptr, 0);
		uint32_t instr = strtoul(args[2].c_str(), nullptr, 0);
		bool showAddress = strtoul(args[3].c_str(), nullptr, 0) != 0;
		bool showBytes = strtoul(args[4].c_str(), nullptr, 0) != 0;

		Gekko::DecoderInfo info = { 0 };
		Gekko::Decoder::Decode(addr, instr, &info);

		std::string text = Gekko::GekkoDisasm::Disasm(addr, &info, showAddress, showBytes);

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Array;

		output->AddAnsiString(nullptr, text.c_str());

		return output;
	}

	static Json::Value* CmdGekkoIsBranch(std::vector<std::string>& args)
	{
		uint32_t addr = strtoul(args[1].c_str(), nullptr, 0);

		uint32_t pa = Gekko::BadAddress;

		if (JDI::Hub.ExecuteFastBool("IsLoaded"))
		{
			int WIMG;
			pa = Core->EffectiveToPhysical(addr, Gekko::MmuAccess::Execute, WIMG);
		}

		bool flowControl = false;
		uint32_t targetAddress = 0;

		if (pa < RAMSIZE)
		{
			Gekko::DecoderInfo info = { 0 };

			uint8_t* ptr = &mi.ram[pa];
			uint32_t instr = _BYTESWAP_UINT32(*(uint32_t*)ptr);

			Gekko::Decoder::Decode(addr, instr, &info);

			flowControl = info.flow;
			targetAddress = info.Imm.Address;
		}

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Array;

		output->AddBool(nullptr, flowControl);
		output->AddUInt32(nullptr, targetAddress);

		return output;
	}

	static Json::Value* CmdNop(std::vector<std::string>& args)
	{
		if (!JDI::Hub.ExecuteFastBool("IsLoaded"))
		{
			return nullptr;
		}

		uint32_t ea = strtoul(args[1].c_str(), nullptr, 0);
		Core->WriteWord(ea, 0x6000'0000);

		return nullptr;
	}

	// Enables or disables the maintenance of opcode usage statistics
	static Json::Value* CmdEnableOpcodeStats(std::vector<std::string>& args)
	{
		bool enable = strtoul(args[1].c_str(), nullptr, 0) != 0 ? true : false;
		Core->EnableOpcodeStats(enable);
		return nullptr;
	}

	// Displays the most commonly used Gekko opcodes
	static Json::Value* CmdPrintOpcodeStats(std::vector<std::string>& args)
	{
		size_t maxCount = strtoul(args[1].c_str(), nullptr, 0);
		Core->PrintOpcodeStats(maxCount);
		return nullptr;
	}

	// Clears statistics of opcode usage
	static Json::Value* CmdResetOpcodeStats(std::vector<std::string>& args)
	{
		Core->ResetOpcodeStats();
		return nullptr;
	}

	// Runs a low priority thread that prints opcode statistics once a second
	static Json::Value* CmdRunOpcodeStats(std::vector<std::string>& args)
	{
		Core->RunOpcodeStatsThread();
		return nullptr;
	}

	// Stop the thread that outputs the opcode statistics
	static Json::Value* CmdStopOpcodeStats(std::vector<std::string>& args)
	{
		Core->StopOpcodeStatsThread();
		return nullptr;
	}

	// Parse Gekko instruction
	static Json::Value* CmdGekkoAnalyze(std::vector<std::string>& args)
	{
		uint32_t pc = strtoul(args[1].c_str(), nullptr, 0);
		uint32_t opcode = strtoul(args[2].c_str(), nullptr, 0);

		Gekko::DecoderInfo info = { 0 };

		Gekko::Decoder::Decode(pc, opcode, &info);

		// Array: [Int instr, Int numParams, Int param0, Int paramBits0, Int param1, Int paramBits1, Int param2, Int paramBits2, Int param3, Int paramBits3, Int param4, Int paramBits4, UInt32 immedValue, UInt32 newPc, Bool flow]

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Array;

		output->AddInt(nullptr, (int)info.instr);
		output->AddInt(nullptr, (int)info.numParam);

		uint32_t imm = 0;

		for (int i = 0; i < 5; i++)
		{
			if (i >= info.numParam)
			{
				output->AddInt(nullptr, (int)Gekko::Param::Unknown);
				output->AddInt(nullptr, 0);
				continue;
			}

			// Immediate parameter values are stored in a separate field.

			switch (info.param[i])
			{
			case Gekko::Param::Simm:
				imm = (uint32_t)(int32_t)info.Imm.Signed;
				break;
			case Gekko::Param::Uimm:
				imm = (uint32_t)info.Imm.Unsigned;
				break;
			case Gekko::Param::Address:
				imm = info.Imm.Address;
				break;
			}

			output->AddInt(nullptr, (int)info.param[i]);
			output->AddInt(nullptr, (int)info.paramBits[i]);		// Doesn't make sense for Immediate parameters, but remains for conformity.
		}

		output->AddUInt32(nullptr, imm);			// If the Immediate parameter is missing, it is 0
		output->AddUInt32(nullptr, info.pc);
		output->AddBool(nullptr, info.flow);

		return output;
	}

	// Return the name of the Gekko instruction (Gekko::Instruction)
	static Json::Value* CmdGekkoInstrToString(std::vector<std::string>& args)
	{
		Gekko::DecoderInfo info = { 0 };
		info.instr = (Gekko::Instruction)strtoul(args[1].c_str(), nullptr, 0);

		std::string instrName = Gekko::GekkoDisasm::InstrToString(&info);

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Array;

		output->AddAnsiString(nullptr, instrName.c_str());

		return output;
	}

	// Return the parameter name of a Gekko instruction (Gekko::Param)
	static Json::Value* CmdGekkoInstrParamToString(std::vector<std::string>& args)
	{
		Gekko::DecoderInfo info = { 0 };

		info.param[0] = (Gekko::Param)strtoul(args[1].c_str(), nullptr, 0);
		info.paramBits[0] = strtoul(args[2].c_str(), nullptr, 0);
		uint32_t imm = strtoul(args[3].c_str(), nullptr, 0);

		// Immediate parameter values are stored in a separate field.

		switch (info.param[0])
		{
			case Gekko::Param::Simm:
				info.Imm.Signed = imm;
				break;
			case Gekko::Param::Uimm:
				info.Imm.Unsigned = imm;
				break;
			case Gekko::Param::Address:
				info.Imm.Address = imm;
				break;
		}

		std::string paramName = Gekko::GekkoDisasm::ParamName(info.param[0]);
		std::string paramText = Gekko::GekkoDisasm::ParamToString(info.param[0], info.paramBits[0], &info);

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Array;

		output->AddAnsiString(nullptr, paramName.c_str());
		output->AddAnsiString(nullptr, paramText.c_str());

		return output;
	}

	void gekko_init_handlers()
	{
		JDI::Hub.AddCmd("run", CmdRun);
		JDI::Hub.AddCmd("stop", CmdStop);
		JDI::Hub.AddCmd("r", CmdGetSetReg);
		JDI::Hub.AddCmd("b", CmdBreakExec);
		JDI::Hub.AddCmd("br", CmdBreakRead);
		JDI::Hub.AddCmd("bw", CmdBreakWrite);
		JDI::Hub.AddCmd("bc", CmdBreakClearAll);
		JDI::Hub.AddCmd("CacheLog", CmdCacheLog);

		JDI::Hub.AddCmd("IsRunning", CmdIsRunning);
		JDI::Hub.AddCmd("GekkoRun", CmdGekkoRun);
		JDI::Hub.AddCmd("GekkoSuspend", CmdGekkoSuspend);
		JDI::Hub.AddCmd("GekkoStep", CmdGekkoStep);
		JDI::Hub.AddCmd("GekkoSkipInstruction", CmdGekkoSkipInstruction);

		JDI::Hub.AddCmd("GetGpr", CmdGetGpr);
		JDI::Hub.AddCmd("GetPs0", CmdGetPs0);
		JDI::Hub.AddCmd("GetPs1", CmdGetPs1);
		JDI::Hub.AddCmd("GetPc", CmdGetPc);
		JDI::Hub.AddCmd("GetMsr", CmdGetMsr);
		JDI::Hub.AddCmd("GetCr", CmdGetCr);
		JDI::Hub.AddCmd("GetFpscr", CmdGetFpscr);
		JDI::Hub.AddCmd("GetSpr", CmdGetSpr);
		JDI::Hub.AddCmd("GetSr", CmdGetSr);
		JDI::Hub.AddCmd("GetTbu", CmdGetTbu);
		JDI::Hub.AddCmd("GetTbl", CmdGetTbl);

		JDI::Hub.AddCmd("TranslateDMmu", CmdTranslateDMmu);
		JDI::Hub.AddCmd("TranslateIMmu", CmdTranslateIMmu);
		JDI::Hub.AddCmd("VirtualToPhysicalDMmu", CmdVirtualToPhysicalDMmu);
		JDI::Hub.AddCmd("VirtualToPhysicalIMmu", CmdVirtualToPhysicalIMmu);

		JDI::Hub.AddCmd("GekkoTestBreakpoint", CmdGekkoTestBreakpoint);
		JDI::Hub.AddCmd("GekkoToggleBreakpoint", CmdGekkoToggleBreakpoint);
		JDI::Hub.AddCmd("GekkoAddOneShotBreakpoint", CmdGekkoAddOneShotBreakpoint);

		JDI::Hub.AddCmd("GekkoDisasm", CmdGekkoDisasm);
		JDI::Hub.AddCmd("GekkoDisasmNoMemAccess", CmdGekkoDisasmNoMemAccess);
		JDI::Hub.AddCmd("GekkoIsBranch", CmdGekkoIsBranch);

		JDI::Hub.AddCmd("nop", CmdNop);

		JDI::Hub.AddCmd("EnableOpcodeStats", CmdEnableOpcodeStats);
		JDI::Hub.AddCmd("PrintOpcodeStats", CmdPrintOpcodeStats);
		JDI::Hub.AddCmd("ResetOpcodeStats", CmdResetOpcodeStats);
		JDI::Hub.AddCmd("RunOpcodeStats", CmdRunOpcodeStats);
		JDI::Hub.AddCmd("StopOpcodeStats", CmdStopOpcodeStats);

		JDI::Hub.AddCmd("GekkoAnalyze", CmdGekkoAnalyze);
		JDI::Hub.AddCmd("GekkoInstrToString", CmdGekkoInstrToString);
		JDI::Hub.AddCmd("GekkoInstrParamToString", CmdGekkoInstrParamToString);
	}
}
