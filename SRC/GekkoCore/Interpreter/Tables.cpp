// interpreter tables setup
#include "../pch.h"
#include "InterpreterPrivate.h"

using namespace Debug;

namespace Gekko
{
	// opcode tables

	void (*bx[4])(uint32_t);
	void (*c_1[64])(uint32_t);
	void (*c_19[2048])(uint32_t);
	void (*c_31[2048])(uint32_t);
	void (*c_59[64])(uint32_t);
	void (*c_63[2048])(uint32_t);
	void (*c_4[2048])(uint32_t);

	// not implemented opcode
	OP(NI)
	{
		Halt("** CPU ERROR **\n"
			"unimplemented opcode : %08X <%08X> (%i, %i)\n",
			Gekko->regs.pc, op, op >> 26, op & 0x7ff);

		Gekko->PrCause = PrivilegedCause::IllegalInstruction;
		Gekko->Exception(Exception::PROGRAM);
	}

	// switch to extension opcode table
	OP(OP19) { c_19[op & 0x7ff](op); }
	OP(OP31) { c_31[op & 0x7ff](op); }
	OP(OP59) { c_59[op & 0x3f](op); }
	OP(OP63) { c_63[op & 0x7ff](op); }
	OP(OP4) { c_4[op & 0x7ff](op); }

	// high level call
	OP(HL)
	{
		// Dolwin module base should be specified as 0x400000 in project properties
		void (*pcall)() = (void (*)())((void*)(uint64_t)op);

		if (op == 0)
		{
			Halt(
				"Something goes wrong in interpreter, \n"
				"program is trying to execute NULL opcode.\n\n"
				"pc:%08X", Gekko->regs.pc);
			return;
		}

		pcall();
	}

	// setup extension tables
	void Interpreter::InitTables()
	{
		int i;
		uint8_t scale;

		// build rotate mask table
		for (int mb = 0; mb < 32; mb++)
		{
			for (int me = 0; me < 32; me++)
			{
				uint32_t mask = ((uint32_t)-1 >> mb) ^ ((me >= 31) ? 0 : ((uint32_t)-1) >> (me + 1));
				rotmask[mb][me] = (mb > me) ? (~mask) : (mask);
			}
		}

		// build paired-single load scale
		for (scale = 0; scale < 64; scale++)
		{
			int factor;
			if (scale & 0x20)    // -32 ... -1
			{
				factor = -32 + (scale & 0x1f);
			}
			else                // 0 ... 31
			{
				factor = 0 + (scale & 0x1f);
			}
			ldScale[scale] = powf(2, -1.0f * (float)factor);
		}

		// build paired-single store scale
		for (scale = 0; scale < 64; scale++)
		{
			int factor;
			if (scale & 0x20)    // -32 ... -1
			{
				factor = -32 + (scale & 0x1f);
			}
			else                // 0 ... 31
			{
				factor = 0 + (scale & 0x1f);
			}
			stScale[scale] = powf(2, +1.0f * (float)factor);
		}

		// set all tables to default "not implemented" opcode
		for (i = 0; i < 2048; i++)
		{
			if (i < 64) c_59[i] = c_NI;
			c_19[i] = c_31[i] = c_63[i] = c_NI;
			c_4[i] = c_NI;
		}

		// Main opcode table
		c_1[0] = c_HL;
		c_1[1] = c_NI;
		c_1[2] = c_NI;
		c_1[3] = c_TWI;
		c_1[4] = c_OP4;
		c_1[5] = c_NI;
		c_1[6] = c_NI;
		
		c_1[9] = c_NI;

		c_1[17] = c_SC;
		c_1[19] = c_OP19;
		c_1[22] = c_NI;

		c_1[24] = c_ORI;
		c_1[25] = c_ORIS;
		c_1[26] = c_XORI; 
		c_1[27] = c_XORIS;
		c_1[28] = c_ANDID;
		c_1[29] = c_ANDISD;
		c_1[30] = c_NI;
		c_1[31] = c_OP31;

		c_1[58] = c_NI;
		c_1[59] = c_OP59;
		c_1[62] = c_NI;
		c_1[63] = c_OP63;

		// "19" extension
		c_19[100] = c_RFI;
		c_19[300] = c_ISYNC;

		// "31" extension
		c_31[8] = c_TW;
		c_31[38] = c_MFCR;
		c_31[40] = c_LWARX;
		c_31[52] = c_CNTLZW;
		c_31[53] = c_CNTLZWD;
		c_31[56] = c_AND;
		c_31[57] = c_ANDD;
		c_31[108] = c_DCBST;
		c_31[120] = c_ANDC;
		c_31[121] = c_ANDCD;
		c_31[166] = c_MFMSR;
		c_31[172] = c_DCBF;
		c_31[248] = c_NOR;
		c_31[249] = c_NORD;
		c_31[288] = c_MTCRF;
		c_31[292] = c_MTMSR;
		c_31[301] = c_STWCXD;
		c_31[420] = c_MTSR;
		c_31[484] = c_MTSRIN;
		c_31[492] = c_DCBTST;
		c_31[556] = c_DCBT;
		c_31[568] = c_EQV;
		c_31[569] = c_EQVD;
		c_31[612] = c_TLBIE;
		c_31[632] = c_XOR;
		c_31[633] = c_XORD;
		c_31[678] = c_MFSPR;
		c_31[742] = c_MFTB;
		c_31[824] = c_ORC;
		c_31[825] = c_ORCD;
		c_31[888] = c_OR;
		c_31[889] = c_ORD;
		c_31[934] = c_MTSPR;
		c_31[940] = c_DCBI;
		c_31[952] = c_NAND;
		c_31[953] = c_NANDD;
		c_31[1024] = c_MCRXR;
		c_31[1132] = c_TLBSYNC;
		c_31[1190] = c_MFSR;
		c_31[1196] = c_SYNC;
		c_31[1318] = c_MFSRIN;
		c_31[1708] = c_EIEIO;
		c_31[1844] = c_EXTSH;
		c_31[1845] = c_EXTSHD;
		c_31[1908] = c_EXTSB;
		c_31[1909] = c_EXTSBD;
		c_31[1964] = c_ICBI;
		c_31[2028] = c_DCBZ;

		// "4" extension
		c_4[2028] = c_DCBZ_L;
	}

}
