// Processor debug commands. Available only after emulation has been started.
#include "pch.h"

namespace Gekko
{
	// Run processor until break or stop
	static Json::Value* cmd_run(std::vector<std::string>& args)
	{
		Gekko->Run();
		return nullptr;
	}

	// Stop processor execution
	static Json::Value* cmd_stop(std::vector<std::string>& args)
	{
		if (Gekko->IsRunning())
		{
			Gekko->Suspend();
		}
		return nullptr;
	}

	// ---------------------------------------------------------------------------
	// r

	// Get pointer to Gekko register.
	static uint32_t* getreg(const char* name)
	{
		if (!_stricmp(name, "r0")) return &Gekko->regs.gpr[0];
		else if (!_stricmp(name, "r1")) return &Gekko->regs.gpr[1];
		else if (!_stricmp(name, "r2")) return &Gekko->regs.gpr[2];
		else if (!_stricmp(name, "r3")) return &Gekko->regs.gpr[3];
		else if (!_stricmp(name, "r4")) return &Gekko->regs.gpr[4];
		else if (!_stricmp(name, "r5")) return &Gekko->regs.gpr[5];
		else if (!_stricmp(name, "r6")) return &Gekko->regs.gpr[6];
		else if (!_stricmp(name, "r7")) return &Gekko->regs.gpr[7];
		else if (!_stricmp(name, "r8")) return &Gekko->regs.gpr[8];
		else if (!_stricmp(name, "r9")) return &Gekko->regs.gpr[9];
		else if (!_stricmp(name, "r10")) return &Gekko->regs.gpr[10];
		else if (!_stricmp(name, "r11")) return &Gekko->regs.gpr[11];
		else if (!_stricmp(name, "r12")) return &Gekko->regs.gpr[12];
		else if (!_stricmp(name, "r13")) return &Gekko->regs.gpr[13];
		else if (!_stricmp(name, "r14")) return &Gekko->regs.gpr[14];
		else if (!_stricmp(name, "r15")) return &Gekko->regs.gpr[15];
		else if (!_stricmp(name, "r16")) return &Gekko->regs.gpr[16];
		else if (!_stricmp(name, "r17")) return &Gekko->regs.gpr[17];
		else if (!_stricmp(name, "r18")) return &Gekko->regs.gpr[18];
		else if (!_stricmp(name, "r19")) return &Gekko->regs.gpr[19];
		else if (!_stricmp(name, "r20")) return &Gekko->regs.gpr[20];
		else if (!_stricmp(name, "r21")) return &Gekko->regs.gpr[21];
		else if (!_stricmp(name, "r22")) return &Gekko->regs.gpr[22];
		else if (!_stricmp(name, "r23")) return &Gekko->regs.gpr[23];
		else if (!_stricmp(name, "r24")) return &Gekko->regs.gpr[24];
		else if (!_stricmp(name, "r25")) return &Gekko->regs.gpr[25];
		else if (!_stricmp(name, "r26")) return &Gekko->regs.gpr[26];
		else if (!_stricmp(name, "r27")) return &Gekko->regs.gpr[27];
		else if (!_stricmp(name, "r28")) return &Gekko->regs.gpr[28];
		else if (!_stricmp(name, "r29")) return &Gekko->regs.gpr[29];
		else if (!_stricmp(name, "r30")) return &Gekko->regs.gpr[30];
		else if (!_stricmp(name, "r31")) return &Gekko->regs.gpr[31];

		else if (!_stricmp(name, "sp")) return &Gekko->regs.gpr[1];
		else if (!_stricmp(name, "sd1")) return &Gekko->regs.gpr[13];
		else if (!_stricmp(name, "sd2")) return &Gekko->regs.gpr[2];

		else if (!_stricmp(name, "cr")) return &Gekko->regs.cr;
		else if (!_stricmp(name, "fpscr")) return &Gekko->regs.fpscr;
		else if (!_stricmp(name, "xer")) return &Gekko->regs.spr[(int)Gekko::SPR::XER];
		else if (!_stricmp(name, "lr")) return &Gekko->regs.spr[(int)Gekko::SPR::LR];
		else if (!_stricmp(name, "ctr")) return &Gekko->regs.spr[(int)Gekko::SPR::CTR];
		else if (!_stricmp(name, "msr")) return &Gekko->regs.msr;

		else if (!_stricmp(name, "sr0")) return &Gekko->regs.sr[0];
		else if (!_stricmp(name, "sr1")) return &Gekko->regs.sr[1];
		else if (!_stricmp(name, "sr2")) return &Gekko->regs.sr[2];
		else if (!_stricmp(name, "sr3")) return &Gekko->regs.sr[3];
		else if (!_stricmp(name, "sr4")) return &Gekko->regs.sr[4];
		else if (!_stricmp(name, "sr5")) return &Gekko->regs.sr[5];
		else if (!_stricmp(name, "sr6")) return &Gekko->regs.sr[6];
		else if (!_stricmp(name, "sr7")) return &Gekko->regs.sr[7];
		else if (!_stricmp(name, "sr8")) return &Gekko->regs.sr[8];
		else if (!_stricmp(name, "sr9")) return &Gekko->regs.sr[9];
		else if (!_stricmp(name, "sr10")) return &Gekko->regs.sr[10];
		else if (!_stricmp(name, "sr11")) return &Gekko->regs.sr[11];
		else if (!_stricmp(name, "sr12")) return &Gekko->regs.sr[12];
		else if (!_stricmp(name, "sr13")) return &Gekko->regs.sr[13];
		else if (!_stricmp(name, "sr14")) return &Gekko->regs.sr[14];
		else if (!_stricmp(name, "sr15")) return &Gekko->regs.sr[15];

		else if (!_stricmp(name, "ibat0u")) return &Gekko->regs.spr[(int)Gekko::SPR::IBAT0U];
		else if (!_stricmp(name, "ibat1u")) return &Gekko->regs.spr[(int)Gekko::SPR::IBAT1U];
		else if (!_stricmp(name, "ibat2u")) return &Gekko->regs.spr[(int)Gekko::SPR::IBAT2U];
		else if (!_stricmp(name, "ibat3u")) return &Gekko->regs.spr[(int)Gekko::SPR::IBAT3U];
		else if (!_stricmp(name, "ibat0l")) return &Gekko->regs.spr[(int)Gekko::SPR::IBAT0L];
		else if (!_stricmp(name, "ibat1l")) return &Gekko->regs.spr[(int)Gekko::SPR::IBAT1L];
		else if (!_stricmp(name, "ibat2l")) return &Gekko->regs.spr[(int)Gekko::SPR::IBAT2L];
		else if (!_stricmp(name, "ibat3l")) return &Gekko->regs.spr[(int)Gekko::SPR::IBAT3L];
		else if (!_stricmp(name, "dbat0u")) return &Gekko->regs.spr[(int)Gekko::SPR::DBAT0U];
		else if (!_stricmp(name, "dbat1u")) return &Gekko->regs.spr[(int)Gekko::SPR::DBAT1U];
		else if (!_stricmp(name, "dbat2u")) return &Gekko->regs.spr[(int)Gekko::SPR::DBAT2U];
		else if (!_stricmp(name, "dbat3u")) return &Gekko->regs.spr[(int)Gekko::SPR::DBAT3U];
		else if (!_stricmp(name, "dbat0l")) return &Gekko->regs.spr[(int)Gekko::SPR::DBAT0L];
		else if (!_stricmp(name, "dbat1l")) return &Gekko->regs.spr[(int)Gekko::SPR::DBAT1L];
		else if (!_stricmp(name, "dbat2l")) return &Gekko->regs.spr[(int)Gekko::SPR::DBAT2L];
		else if (!_stricmp(name, "dbat3l")) return &Gekko->regs.spr[(int)Gekko::SPR::DBAT3L];

		else if (!_stricmp(name, "sdr1")) return &Gekko->regs.spr[(int)Gekko::SPR::SDR1];
		else if (!_stricmp(name, "sprg0")) return &Gekko->regs.spr[(int)Gekko::SPR::SPRG0];
		else if (!_stricmp(name, "sprg1")) return &Gekko->regs.spr[(int)Gekko::SPR::SPRG1];
		else if (!_stricmp(name, "sprg2")) return &Gekko->regs.spr[(int)Gekko::SPR::SPRG2];
		else if (!_stricmp(name, "sprg3")) return &Gekko->regs.spr[(int)Gekko::SPR::SPRG3];
		else if (!_stricmp(name, "dar")) return &Gekko->regs.spr[(int)Gekko::SPR::DAR];
		else if (!_stricmp(name, "dsisr")) return &Gekko->regs.spr[(int)Gekko::SPR::DSISR];
		else if (!_stricmp(name, "srr0")) return &Gekko->regs.spr[(int)Gekko::SPR::SRR0];
		else if (!_stricmp(name, "srr1")) return &Gekko->regs.spr[(int)Gekko::SPR::SRR1];
		else if (!_stricmp(name, "pmc1")) return &Gekko->regs.spr[953];
		else if (!_stricmp(name, "pmc2")) return &Gekko->regs.spr[954];
		else if (!_stricmp(name, "pmc3")) return &Gekko->regs.spr[957];
		else if (!_stricmp(name, "pmc4")) return &Gekko->regs.spr[958];
		else if (!_stricmp(name, "mmcr0")) return &Gekko->regs.spr[952];
		else if (!_stricmp(name, "mmcr1")) return &Gekko->regs.spr[956];
		else if (!_stricmp(name, "sia")) return &Gekko->regs.spr[955];
		else if (!_stricmp(name, "sda")) return &Gekko->regs.spr[959];

		else if (!_stricmp(name, "gqr0")) return &Gekko->regs.spr[(int)Gekko::SPR::GQR0];
		else if (!_stricmp(name, "gqr1")) return &Gekko->regs.spr[(int)Gekko::SPR::GQR1];
		else if (!_stricmp(name, "gqr2")) return &Gekko->regs.spr[(int)Gekko::SPR::GQR2];
		else if (!_stricmp(name, "gqr3")) return &Gekko->regs.spr[(int)Gekko::SPR::GQR3];
		else if (!_stricmp(name, "gqr4")) return &Gekko->regs.spr[(int)Gekko::SPR::GQR4];
		else if (!_stricmp(name, "gqr5")) return &Gekko->regs.spr[(int)Gekko::SPR::GQR5];
		else if (!_stricmp(name, "gqr6")) return &Gekko->regs.spr[(int)Gekko::SPR::GQR6];
		else if (!_stricmp(name, "gqr7")) return &Gekko->regs.spr[(int)Gekko::SPR::GQR7];

		else if (!_stricmp(name, "hid0")) return &Gekko->regs.spr[(int)Gekko::SPR::HID0];
		else if (!_stricmp(name, "hid1")) return &Gekko->regs.spr[(int)Gekko::SPR::HID1];
		else if (!_stricmp(name, "hid2")) return &Gekko->regs.spr[(int)Gekko::SPR::HID2];

		else if (!_stricmp(name, "dabr")) return &Gekko->regs.spr[(int)Gekko::SPR::DABR];
		else if (!_stricmp(name, "iabr")) return &Gekko->regs.spr[(int)Gekko::SPR::IABR];
		else if (!_stricmp(name, "wpar")) return &Gekko->regs.spr[(int)Gekko::SPR::WPAR];
		else if (!_stricmp(name, "l2cr")) return &Gekko->regs.spr[1017];
		else if (!_stricmp(name, "dmau")) return &Gekko->regs.spr[(int)Gekko::SPR::DMAU];
		else if (!_stricmp(name, "dmal")) return &Gekko->regs.spr[(int)Gekko::SPR::DMAL];
		else if (!_stricmp(name, "thrm1")) return &Gekko->regs.spr[1020];
		else if (!_stricmp(name, "thrm2")) return &Gekko->regs.spr[1021];
		else if (!_stricmp(name, "thrm2")) return &Gekko->regs.spr[1022];
		else if (!_stricmp(name, "ictc")) return &Gekko->regs.spr[1019];

		else if (!_stricmp(name, "pc")) return &Gekko->regs.pc;   // Wow !

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

		DBReport("MSR: 0x%08X\n", msr_val);

		if (msr_val & MSR_POW) DBReport("MSR[POW]: 1, power management enabled\n");
		else DBReport("MSR[POW]: 0, power management disabled\n");
		if (msr_val & MSR_ILE) DBReport("MSR[ILE]: 1\n");
		else DBReport("MSR[ILE]: 0\n");
		if (msr_val & MSR_EE) DBReport("MSR[EE] : 1, external interrupts and decrementer exception are enabled\n");
		else DBReport("MSR[EE] : 0, external interrupts and decrementer exception are disabled\n");
		if (msr_val & MSR_PR) DBReport("MSR[PR] : 1, processor execute in user mode (UISA)\n");
		else DBReport("MSR[PR] : 0, processor execute in supervisor mode (OEA)\n");
		if (msr_val & MSR_FP) DBReport("MSR[FP] : 1, floating-point is available\n");
		else DBReport("MSR[FP] : 0, floating-point unavailable\n");
		if (msr_val & MSR_ME) DBReport("MSR[ME] : 1, machine check exceptions are enabled\n");
		else DBReport("MSR[ME] : 0, machine check exceptions are disabled\n");

		fe[0] = msr_val & MSR_FE0 ? 1 : 0;
		fe[1] = msr_val & MSR_FE1 ? 1 : 0;
		f = (fe[0] << 1) | (fe[1]);
		DBReport("MSR[FE] : %i, floating-point %s\n", f, fpmod[f]);

		if (msr_val & MSR_SE) DBReport("MSR[SE] : 1, single-step tracing is enabled\n");
		else DBReport("MSR[SE] : 0, single-step tracing is disabled\n");
		if (msr_val & MSR_BE) DBReport("MSR[BE] : 1, branch tracing is enabled\n");
		else DBReport("MSR[BE] : 0, branch tracing is disabled\n");
		if (msr_val & MSR_IP) DBReport("MSR[IP] : 1, exception prefix to physical address is 0xFFFn_nnnn\n");
		else DBReport("MSR[IP] : 0, exception prefix to physical address is 0x000n_nnnn\n");
		if (msr_val & MSR_IR) DBReport("MSR[IR] : 1, instruction address translation is enabled\n");
		else DBReport("MSR[IR] : 0, instruction address translation is disabled\n");
		if (msr_val & MSR_DR) DBReport("MSR[DR] : 1, data address translation is enabled\n");
		else DBReport("MSR[DR] : 0, data address translation is disabled\n");
		if (msr_val & MSR_PM) DBReport("MSR[PM] : 1, performance monitoring is enabled for this thread\n");
		else DBReport("MSR[PM] : 0, performance monitoring is disabled for this thread\n");
		if (msr_val & MSR_RI) DBReport("MSR[RI] : 1\n");
		else DBReport("MSR[RI] : 0\n");
		if (msr_val & MSR_LE) DBReport("MSR[LE] : 1, processor runs in little-endian mode\n");
		else DBReport("MSR[LE] : 0, processor runs in big-endian mode\n");
	}

	// HID0 info
	static void describe_hid0(uint32_t val)
	{
		DBReport("HID0: 0x%08X\n", val);

		if (val & HID0_EMCP) DBReport("HID0[EMCP] : 1, Asserting MCP causes checkstop or a machine check\n");
		else DBReport("HID0[EMCP] : 0, Masks MCP. Asserting MCP does not generate a machine check exception or a checkstop\n");
		if (val & HID0_DBP) DBReport("HID0[DBP]  : 1, Disable parity generation\n");
		else DBReport("HID0[DBP]  : 0, Parity generation is enabled\n");
		if (val & HID0_EBA) DBReport("HID0[EBA]  : 1, Allows a address parity error to cause a checkstop or a machine check\n");
		else DBReport("HID0[EBA]  : 0, Prevents address parity checking\n");
		if (val & HID0_EBD) DBReport("HID0[EBD]  : 1, Allows a data parity error to cause a checkstop or machine check\n");
		else DBReport("HID0[EBD]  : 0, Parity checking is disabled\n");
		if (val & HID0_PAR) DBReport("HID0[PAR]  : 1, Alters bus protocol slightly by preventing the processor from driving ARTRY to high\n");
		else DBReport("HID0[PAR]  : 0, Precharge of ARTRY enabled\n");
		if (val & HID0_DOZE) DBReport("HID0[DOZE] : 1, Doze mode enabled\n");
		else DBReport("HID0[DOZE] : 0, Doze mode disabled\n");
		if (val & HID0_NAP) DBReport("HID0[NAP]  : 1, Nap mode enabled\n");
		else DBReport("HID0[NAP]  : 0, Nap mode disabled\n");
		if (val & HID0_SLEEP) DBReport("HID0[SLEEP]: 1, Sleep mode enabled\n");
		else DBReport("HID0[SLEEP]: 0, Sleep mode disabled\n");
		if (val & HID0_DPM) DBReport("HID0[DPM]  : 1, Dynamic power management is enabled\n");
		else DBReport("HID0[DPM]  : 0, Dynamic power management is disabled\n");
		if (val & HID0_NHR) DBReport("HID0[NHR]  : 1, Hard reset has not occurred\n");
		else DBReport("HID0[NHR]  : 0, Hard reset occurred\n");
		if (val & HID0_ICE) DBReport("HID0[ICE]  : 1, Instruction cache is enabled\n");
		else DBReport("HID0[ICE]  : 0, Instruction cache is disabled\n");
		if (val & HID0_DCE) DBReport("HID0[DCE]  : 1, Data cache is enabled\n");
		else DBReport("HID0[DCE]  : 0, Data cache is disabled\n");
		if (val & HID0_ILOCK) DBReport("HID0[ILOCK]: 1, Instruction cache locked (frozen)\n");
		else DBReport("HID0[ILOCK]: 0, Instruction cache not locked\n");
		if (val & HID0_DLOCK) DBReport("HID0[DLOCK]: 1, Data cache locked (frozen)\n");
		else DBReport("HID0[DLOCK]: 0, Data cache not locked\n");
		if (val & HID0_ICFI) DBReport("HID0[ICFI] : 1, Instruction cache invalidating\n");
		else DBReport("HID0[ICFI] : 0, Instruction cache is not invalidated\n");
		if (val & HID0_DCFI) DBReport("HID0[DCFI] : 1, Data cache invalidating\n");
		else DBReport("HID0[DCFI] : 0, Data cache is not invalidated\n");
		if (val & HID0_SPD) DBReport("HID0[SPD]  : 1, Speculative bus accesses to nonguarded space disabled\n");
		else DBReport("HID0[SPD]  : 0, Speculative bus accesses to nonguarded space enabled\n");
		if (val & HID0_IFEM) DBReport("HID0[IFEM] : 1, Instruction fetches reflect the M bit from the WIM settings\n");
		else DBReport("HID0[IFEM] : 0, Instruction fetches M bit disabled\n");
		if (val & HID0_SGE) DBReport("HID0[SGE]  : 1, Store gathering is enabled\n");
		else DBReport("HID0[SGE]  : 0, Store gathering is disabled \n");
		if (val & HID0_DCFA) DBReport("HID0[DCFA] : 1, Data cache flush assist facility is enabled\n");
		else DBReport("HID0[DCFA] : 0, Data cache flush assist facility is disabled\n");
		if (val & HID0_BTIC) DBReport("HID0[BTIC] : 1, BTIC is enabled\n");
		else DBReport("HID0[BTIC] : 0, BTIC is disabled\n");
		if (val & HID0_ABE) DBReport("HID0[ABE]  : 1, Address-only operations are broadcast on the 60x bus\n");
		else DBReport("HID0[ABE]  : 0, Address-only operations affect only local L1 and L2 caches and are not broadcast\n");
		if (val & HID0_BHT) DBReport("HID0[BHT]  : 1, Branch history enabled\n");
		else DBReport("HID0[BHT]  : 0, Branch history disabled\n");
		if (val & HID0_NOOPTI) DBReport("HID0[NOOPTI]: 1, The dcbt and dcbtst instructions are no-oped globally\n");
		else DBReport("HID0[NOOPTI]: 0, The dcbt and dcbtst instructions are enabled\n");
	}

	static Json::Value* cmd_r(std::vector<std::string>& args)
	{
		uint32_t(*op)(uint32_t a, uint32_t b) = NULL;

		uint32_t* n = getreg(args[1].c_str());
		if (n == NULL)
		{
			DBReport("unknown register : %s\n", args[1].c_str());
			return nullptr;
		}

		// show register
		if (args.size() <= 3)
		{
			if (!_stricmp(args[1].c_str(), "msr")) describe_msr(*n);
			else if (!_stricmp(args[1].c_str(), "hid0")) describe_hid0(*n);
			else DBReport("%s = %i (0x%X)\n", args[1].c_str(), *n, *n);
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
			DBReport("Unknown operation: %s\n", args[2].c_str());
			return nullptr;
		}

		// New value
		uint32_t* m = getreg(args[3].c_str());
		if (m == NULL)
		{
			int i = strtoul(args[3].c_str(), NULL, 0);
			DBReport("%s %s %i (0x%X)\n", args[1].c_str(), args[2].c_str(), i, i);
			*n = op(*n, i);
		}
		else
		{
			DBReport("%s %s %s\n", args[1].c_str(), args[2].c_str(), args[3].c_str());
			*n = op(*n, *m);
		}

		return nullptr;
	}

	static Json::Value* cmd_b(std::vector<std::string>& args)
	{
		uint32_t addr = strtoul(args[1].c_str(), nullptr, 0);
		Gekko->AddBreakpoint(addr);
		return nullptr;
	}

	static Json::Value* cmd_br(std::vector<std::string>& args)
	{
		uint32_t addr = strtoul(args[1].c_str(), nullptr, 0);
		Gekko->AddReadBreak(addr);
		return nullptr;
	}

	static Json::Value* cmd_bw(std::vector<std::string>& args)
	{
		uint32_t addr = strtoul(args[1].c_str(), nullptr, 0);
		Gekko->AddWriteBreak(addr);
		return nullptr;
	}

	static Json::Value* cmd_bc(std::vector<std::string>& args)
	{
		Gekko->ClearBreakpoints();
		return nullptr;
	}

	static Json::Value* CacheLog(std::vector<std::string>& args)
	{
		CacheLogLevel level = (CacheLogLevel)atoi(args[1].c_str());
		Gekko->cache.SetLogLevel(level);
		return nullptr;
	}

	static Json::Value* CacheDebugDisable(std::vector<std::string>& args)
	{
		bool disable = atoi(args[1].c_str()) ? true : false;
		Gekko->cache.DebugDisable(disable);
		return nullptr;
	}

	void gekko_init_handlers()
	{
		Debug::Hub.AddCmd("run", cmd_run);
		Debug::Hub.AddCmd("stop", cmd_stop);
		Debug::Hub.AddCmd("r", cmd_r);
		Debug::Hub.AddCmd("b", cmd_b);
		Debug::Hub.AddCmd("br", cmd_br);
		Debug::Hub.AddCmd("bw", cmd_bw);
		Debug::Hub.AddCmd("bc", cmd_bc);
		Debug::Hub.AddCmd("CacheLog", CacheLog);
		Debug::Hub.AddCmd("CacheDebugDisable", CacheDebugDisable);
	}
}
