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
		if (!_stricmp(name, "r0")) return &GPR[0];
		else if (!_stricmp(name, "r1")) return &GPR[1];
		else if (!_stricmp(name, "r2")) return &GPR[2];
		else if (!_stricmp(name, "r3")) return &GPR[3];
		else if (!_stricmp(name, "r4")) return &GPR[4];
		else if (!_stricmp(name, "r5")) return &GPR[5];
		else if (!_stricmp(name, "r6")) return &GPR[6];
		else if (!_stricmp(name, "r7")) return &GPR[7];
		else if (!_stricmp(name, "r8")) return &GPR[8];
		else if (!_stricmp(name, "r9")) return &GPR[9];
		else if (!_stricmp(name, "r10")) return &GPR[10];
		else if (!_stricmp(name, "r11")) return &GPR[11];
		else if (!_stricmp(name, "r12")) return &GPR[12];
		else if (!_stricmp(name, "r13")) return &GPR[13];
		else if (!_stricmp(name, "r14")) return &GPR[14];
		else if (!_stricmp(name, "r15")) return &GPR[15];
		else if (!_stricmp(name, "r16")) return &GPR[16];
		else if (!_stricmp(name, "r17")) return &GPR[17];
		else if (!_stricmp(name, "r18")) return &GPR[18];
		else if (!_stricmp(name, "r19")) return &GPR[19];
		else if (!_stricmp(name, "r20")) return &GPR[20];
		else if (!_stricmp(name, "r21")) return &GPR[21];
		else if (!_stricmp(name, "r22")) return &GPR[22];
		else if (!_stricmp(name, "r23")) return &GPR[23];
		else if (!_stricmp(name, "r24")) return &GPR[24];
		else if (!_stricmp(name, "r25")) return &GPR[25];
		else if (!_stricmp(name, "r26")) return &GPR[26];
		else if (!_stricmp(name, "r27")) return &GPR[27];
		else if (!_stricmp(name, "r28")) return &GPR[28];
		else if (!_stricmp(name, "r29")) return &GPR[29];
		else if (!_stricmp(name, "r30")) return &GPR[30];
		else if (!_stricmp(name, "r31")) return &GPR[31];

		else if (!_stricmp(name, "sp")) return &GPR[1];
		else if (!_stricmp(name, "sd1")) return &GPR[13];
		else if (!_stricmp(name, "sd2")) return &GPR[2];

		else if (!_stricmp(name, "cr")) return &PPC_CR;
		else if (!_stricmp(name, "fpscr")) return &FPSCR;
		else if (!_stricmp(name, "xer")) return &XER;
		else if (!_stricmp(name, "lr")) return &PPC_LR;
		else if (!_stricmp(name, "ctr")) return &CTR;
		else if (!_stricmp(name, "msr")) return &MSR;

		else if (!_stricmp(name, "sr0")) return &PPC_SR[0];
		else if (!_stricmp(name, "sr1")) return &PPC_SR[1];
		else if (!_stricmp(name, "sr2")) return &PPC_SR[2];
		else if (!_stricmp(name, "sr3")) return &PPC_SR[3];
		else if (!_stricmp(name, "sr4")) return &PPC_SR[4];
		else if (!_stricmp(name, "sr5")) return &PPC_SR[5];
		else if (!_stricmp(name, "sr6")) return &PPC_SR[6];
		else if (!_stricmp(name, "sr7")) return &PPC_SR[7];
		else if (!_stricmp(name, "sr8")) return &PPC_SR[8];
		else if (!_stricmp(name, "sr9")) return &PPC_SR[9];
		else if (!_stricmp(name, "sr10")) return &PPC_SR[10];
		else if (!_stricmp(name, "sr11")) return &PPC_SR[11];
		else if (!_stricmp(name, "sr12")) return &PPC_SR[12];
		else if (!_stricmp(name, "sr13")) return &PPC_SR[13];
		else if (!_stricmp(name, "sr14")) return &PPC_SR[14];
		else if (!_stricmp(name, "sr15")) return &PPC_SR[15];

		else if (!_stricmp(name, "ibat0u")) return &IBAT0U;
		else if (!_stricmp(name, "ibat1u")) return &IBAT1U;
		else if (!_stricmp(name, "ibat2u")) return &IBAT2U;
		else if (!_stricmp(name, "ibat3u")) return &IBAT3U;
		else if (!_stricmp(name, "ibat0l")) return &IBAT0L;
		else if (!_stricmp(name, "ibat1l")) return &IBAT1L;
		else if (!_stricmp(name, "ibat2l")) return &IBAT2L;
		else if (!_stricmp(name, "ibat3l")) return &IBAT3L;
		else if (!_stricmp(name, "dbat0u")) return &DBAT0U;
		else if (!_stricmp(name, "dbat1u")) return &DBAT1U;
		else if (!_stricmp(name, "dbat2u")) return &DBAT2U;
		else if (!_stricmp(name, "dbat3u")) return &DBAT3U;
		else if (!_stricmp(name, "dbat0l")) return &DBAT0L;
		else if (!_stricmp(name, "dbat1l")) return &DBAT1L;
		else if (!_stricmp(name, "dbat2l")) return &DBAT2L;
		else if (!_stricmp(name, "dbat3l")) return &DBAT3L;

		else if (!_stricmp(name, "sdr1")) return &SDR1;
		else if (!_stricmp(name, "sprg0")) return &SPRG0;
		else if (!_stricmp(name, "sprg1")) return &SPRG1;
		else if (!_stricmp(name, "sprg2")) return &SPRG2;
		else if (!_stricmp(name, "sprg3")) return &SPRG3;
		else if (!_stricmp(name, "dar")) return &PPC_DAR;
		else if (!_stricmp(name, "dsisr")) return &DSISR;
		else if (!_stricmp(name, "srr0")) return &SRR0;
		else if (!_stricmp(name, "srr1")) return &SRR1;
		else if (!_stricmp(name, "pmc1")) return &SPR[953];
		else if (!_stricmp(name, "pmc2")) return &SPR[954];
		else if (!_stricmp(name, "pmc3")) return &SPR[957];
		else if (!_stricmp(name, "pmc4")) return &SPR[958];
		else if (!_stricmp(name, "mmcr0")) return &SPR[952];
		else if (!_stricmp(name, "mmcr1")) return &SPR[956];
		else if (!_stricmp(name, "sia")) return &SPR[955];
		else if (!_stricmp(name, "sda")) return &SPR[959];

		else if (!_stricmp(name, "gqr0")) return &GQR[0];
		else if (!_stricmp(name, "gqr1")) return &GQR[1];
		else if (!_stricmp(name, "gqr2")) return &GQR[2];
		else if (!_stricmp(name, "gqr3")) return &GQR[3];
		else if (!_stricmp(name, "gqr4")) return &GQR[4];
		else if (!_stricmp(name, "gqr5")) return &GQR[5];
		else if (!_stricmp(name, "gqr6")) return &GQR[6];
		else if (!_stricmp(name, "gqr7")) return &GQR[7];

		else if (!_stricmp(name, "hid0")) return &HID0;
		else if (!_stricmp(name, "hid1")) return &HID1;
		else if (!_stricmp(name, "hid2")) return &HID2;

		else if (!_stricmp(name, "dabr")) return &DABR;
		else if (!_stricmp(name, "iabr")) return &IABR;
		else if (!_stricmp(name, "wpar")) return &WPAR;
		else if (!_stricmp(name, "l2cr")) return &SPR[1017];
		else if (!_stricmp(name, "dmau")) return &DMAU;
		else if (!_stricmp(name, "dmal")) return &DMAL;
		else if (!_stricmp(name, "thrm1")) return &SPR[1020];
		else if (!_stricmp(name, "thrm2")) return &SPR[1021];
		else if (!_stricmp(name, "thrm2")) return &SPR[1022];
		else if (!_stricmp(name, "ictc")) return &SPR[1019];

		else if (!_stricmp(name, "pc")) return &PC;   // Wow !

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

	static Json::Value* cmd_r(std::vector<std::string>& args)
	{
		if (args.size() < 2)
		{
		}
		else
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
		}

		return nullptr;
	}

	void gekko_init_handlers()
	{
		Debug::Hub.AddCmd("run", cmd_run);
		Debug::Hub.AddCmd("stop", cmd_stop);
		Debug::Hub.AddCmd("r", cmd_r);
	}
}
