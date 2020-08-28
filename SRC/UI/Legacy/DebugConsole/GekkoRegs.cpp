// View Gekko registers. Register values are obtained through Jdi->

#include "pch.h"

namespace Debug
{
	static const char* gprnames[] = {
		"r0" , "sp" , "sd2", "r3" , "r4" , "r5" , "r6" , "r7" ,
		"r8" , "r9" , "r10", "r11", "r12", "sd1", "r14", "r15",
		"r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
		"r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31"
	};

	GekkoRegs::GekkoRegs(RECT& rect, std::string name, Cui* parent)
		: CuiWindow (rect, name, parent)
	{
		Memorize();
	}

	GekkoRegs::~GekkoRegs()
	{
	}

	void GekkoRegs::OnDraw()
	{
		Fill(CuiColor::Black, CuiColor::Normal, ' ');

		FillLine(CuiColor::Cyan, CuiColor::White, 0, ' ');
		std::string head = "[ ] F1";
		Print(CuiColor::Cyan, CuiColor::Black, 1, 0, head);
		if (IsActive())
		{
			Print(CuiColor::Cyan, CuiColor::White, 2, 0, "*");
		}

		// Show status of Gekko and DSP

		std::string coreStatus;

		coreStatus += "Gekko: ";
		coreStatus += Jdi.IsRunning() ? "Run " : "Halt";
		coreStatus += " DSP: ";
		coreStatus += Jdi.DspIsRunning() ? "Run " : "Halt";

		Print(CuiColor::Cyan, CuiColor::Black, (int)(width - coreStatus.size() - 2), 0, coreStatus);

		std::string modeText;

		switch (mode)
		{
			case GekkoRegmode::GPR: modeText = "GPR"; break;
			case GekkoRegmode::FPR: modeText = "FPR"; break;
			case GekkoRegmode::PSR: modeText = "PSR"; break;
			case GekkoRegmode::MMU: modeText = "MMU"; break;
		}

		Print(CuiColor::Cyan, CuiColor::Black, (int)(head.size() + 3), 0, modeText);

		switch (mode)
		{
			case GekkoRegmode::GPR: ShowGprs(); break;
			case GekkoRegmode::FPR: ShowFprs(); break;
			case GekkoRegmode::PSR: ShowPairedSingle(); break;
			case GekkoRegmode::MMU: ShowMmu(); break;
		}
	}

	void GekkoRegs::OnKeyPress(char Ascii, int Vkey, bool shift, bool ctrl)
	{
		switch (Vkey)
		{
			case VK_LEFT:
			case VK_PRIOR:
				RotateView(false);
				break;

			case VK_RIGHT:
			case VK_NEXT:
				RotateView(true);
				break;
		}

		Invalidate();
	}

	void GekkoRegs::Memorize()
	{
		for (size_t n = 0; n < 32; n++)
		{
			savedGpr[n] = Jdi->GetGpr(n);
			savedPs0[n].Raw = Jdi->GetPs0(n);
			savedPs1[n].Raw = Jdi->GetPs1(n);
		}
	}

	void GekkoRegs::ShowGprs()
	{
		int y;

		for (y = 1; y <= 16; y++)
		{
			print_gprreg(0, y, y - 1);
			print_gprreg(14, y, y - 1 + 16);
		}

		ShowOtherRegs();
	}

	void GekkoRegs::ShowOtherRegs()
	{
		// Names

		Print(CuiColor::Cyan, 28, 1, "cr  ");
		Print(CuiColor::Cyan, 28, 2, "xer ");
		Print(CuiColor::Cyan, 28, 4, "ctr ");
		Print(CuiColor::Cyan, 28, 5, "dec ");
		Print(CuiColor::Cyan, 28, 8, "pc  ");
		Print(CuiColor::Cyan, 28, 9, "lr  ");
		Print(CuiColor::Cyan, 28,14, "tbr ");

		Print(CuiColor::Cyan, 42, 1, "msr   ");
		Print(CuiColor::Cyan, 42, 2, "fpscr ");
		Print(CuiColor::Cyan, 42, 4, "hid0  ");
		Print(CuiColor::Cyan, 42, 5, "hid1  ");
		Print(CuiColor::Cyan, 42, 6, "hid2  ");
		Print(CuiColor::Cyan, 42, 8, "wpar  ");
		Print(CuiColor::Cyan, 42, 9, "dmau  ");
		Print(CuiColor::Cyan, 42,10, "dmal  ");

		Print(CuiColor::Cyan, 58,  1, "dsisr ");
		Print(CuiColor::Cyan, 58,  2, "dar   ");
		Print(CuiColor::Cyan, 58,  4, "srr0  ");
		Print(CuiColor::Cyan, 58,  5, "srr1  ");
		Print(CuiColor::Cyan, 58,  8, "sprg0 ");
		Print(CuiColor::Cyan, 58,  9, "sprg1 ");
		Print(CuiColor::Cyan, 58, 10, "sprg2 ");
		Print(CuiColor::Cyan, 58, 11, "sprg3 ");
		Print(CuiColor::Cyan, 58, 13, "ear   ");
		Print(CuiColor::Cyan, 58, 14, "pvr   ");

		// Values

		Print(CuiColor::Normal, 32, 1, "%08X", Jdi->GetCr());
		Print(CuiColor::Normal, 32, 2, "%08X", Jdi->GetSpr(Gekko_SPR_XER));
		Print(CuiColor::Normal, 32, 4, "%08X", Jdi->GetSpr(Gekko_SPR_CTR));
		Print(CuiColor::Normal, 32, 5, "%08X", Jdi->GetSpr(Gekko_SPR_DEC));
		Print(CuiColor::Normal, 32, 8, "%08X", Jdi->GetPc());
		Print(CuiColor::Normal, 32, 9, "%08X", Jdi->GetSpr(Gekko_SPR_LR));
		Print(CuiColor::Normal, 32, 14, "%08X:%08X", Jdi->GetTbu(), Jdi->GetTbl());

		uint32_t msr = Jdi->GetMsr();
		uint32_t hid2 = Jdi->GetSpr(Gekko_SPR_HID2);

		Print(CuiColor::Normal, 48, 1, "%08X", msr);
		Print(CuiColor::Normal, 48, 2, "%08X", Jdi->GetFpscr());
		Print(CuiColor::Normal, 48, 4, "%08X", Jdi->GetSpr(Gekko_SPR_HID0));
		Print(CuiColor::Normal, 48, 5, "%08X", Jdi->GetSpr(Gekko_SPR_HID1));
		Print(CuiColor::Normal, 48, 6, "%08X", hid2);
		Print(CuiColor::Normal, 48, 8, "%08X", Jdi->GetSpr(Gekko_SPR_WPAR));
		Print(CuiColor::Normal, 48, 9, "%08X", Jdi->GetSpr(Gekko_SPR_DMAU));
		Print(CuiColor::Normal, 48, 10, "%08X", Jdi->GetSpr(Gekko_SPR_DMAL));

		Print(CuiColor::Normal, 64, 1, "%08X", Jdi->GetSpr(Gekko_SPR_DSISR));
		Print(CuiColor::Normal, 64, 2, "%08X", Jdi->GetSpr(Gekko_SPR_DAR));
		Print(CuiColor::Normal, 64, 4, "%08X", Jdi->GetSpr(Gekko_SPR_SRR0));
		Print(CuiColor::Normal, 64, 5, "%08X", Jdi->GetSpr(Gekko_SPR_SRR1));
		Print(CuiColor::Normal, 64, 8, "%08X", Jdi->GetSpr(Gekko_SPR_SPRG0));
		Print(CuiColor::Normal, 64, 9, "%08X", Jdi->GetSpr(Gekko_SPR_SPRG1));
		Print(CuiColor::Normal, 64, 10, "%08X", Jdi->GetSpr(Gekko_SPR_SPRG2));
		Print(CuiColor::Normal, 64, 11, "%08X", Jdi->GetSpr(Gekko_SPR_SPRG3));
		Print(CuiColor::Normal, 64, 13, "%08X", Jdi->GetSpr(Gekko_SPR_EAR));
		Print(CuiColor::Normal, 64, 14, "%08X", Jdi->GetSpr(Gekko_SPR_PVR));

		// Some cpu flags.

		Print(CuiColor::Cyan, 74, 1, "%s", (msr & MSR_PR) ? "UISA" : "OEA");       // Supervisor?
		Print(CuiColor::Cyan, 74, 2, "%s", (msr & MSR_EE) ? "EE" : "NE");          // Interrupts enabled?

		// Names

		Print(CuiColor::Cyan, 74, 4, "PSE ");
		Print(CuiColor::Cyan, 74, 5, "LSQ ");
		Print(CuiColor::Cyan, 74, 6, "WPE ");
		Print(CuiColor::Cyan, 74, 7, "LC  ");

		// Values

		Print(CuiColor::Normal, 78, 4, "%i", (hid2 & HID2_PSE) ? 1 : 0); // Paired Single mode?
		Print(CuiColor::Normal, 78, 5, "%i", (hid2 & HID2_LSQE) ? 1 : 0); // Load/Store Quantization?
		Print(CuiColor::Normal, 78, 6, "%i", (hid2 & HID2_WPE) ? 1 : 0); // Gather buffer?
		Print(CuiColor::Normal, 78, 7, "%i", (hid2 & HID2_LCE) ? 1 : 0); // Cache locked?
	}

	void GekkoRegs::ShowFprs()
	{
		int y;

		for (y = 1; y <= 16; y++)
		{
			print_fpreg(0, y, y - 1);
			print_fpreg(39, y, y - 1 + 16);
		}
	}

	void GekkoRegs::ShowPairedSingle()
	{
		int y;

		for (y = 1; y <= 16; y++)
		{
			print_ps(0, y, y - 1);
			print_ps(32, y, y - 1 + 16);
		}

		for (y = 1; y <= 8; y++)
		{
			uint32_t gqr = Jdi->GetSpr(Gekko_SPR_GQRs + y - 1);

			Print(CuiColor::Cyan, 64, y, "gqr%i ", y - 1);
			Print(CuiColor::Normal, 69, y, "%08X", gqr);
		}

		uint32_t hid2 = Jdi->GetSpr(Gekko_SPR_HID2);

		Print(CuiColor::Cyan, 64, 10, "PSE   ");
		Print(CuiColor::Cyan, 64, 11, "LSQ   ");

		Print(CuiColor::Normal, 70, 10, "%i", (hid2 & HID2_PSE) ? 1 : 0); // Paired Single mode?
		Print(CuiColor::Normal, 70, 11, "%i", (hid2 & HID2_LSQE) ? 1 : 0); // Load/Store Quantization?
	}

	void GekkoRegs::ShowMmu()
	{
		// Names

		Print(CuiColor::Cyan, 0, 11, "sdr1  ");

		Print(CuiColor::Cyan, 0, 13, "IR    ");
		Print(CuiColor::Cyan, 0, 14, "DR    ");

		Print(CuiColor::Cyan, 0, 1, "dbat0 ");
		Print(CuiColor::Cyan, 0, 2, "dbat1 ");
		Print(CuiColor::Cyan, 0, 3, "dbat2 ");
		Print(CuiColor::Cyan, 0, 4, "dbat3 ");

		Print(CuiColor::Cyan, 0, 6, "ibat0 ");
		Print(CuiColor::Cyan, 0, 7, "ibat1 ");
		Print(CuiColor::Cyan, 0, 8, "ibat2 ");
		Print(CuiColor::Cyan, 0, 9, "ibat3 ");

		// Values

		Print(CuiColor::Normal, 6, 11, "%08X", Jdi->GetSpr(Gekko_SPR_SDR1));

		uint32_t msr = Jdi->GetMsr();

		Print(CuiColor::Normal, 6, 13, "%i", (msr & MSR_IR) ? 1 : 0);
		Print(CuiColor::Normal, 6, 14, "%i", (msr & MSR_DR) ? 1 : 0);

		Print(CuiColor::Normal, 6, 1, "%08X:%08X", Jdi->GetSpr(Gekko_SPR_DBAT0U), Jdi->GetSpr(Gekko_SPR_DBAT0L));
		Print(CuiColor::Normal, 6, 2, "%08X:%08X", Jdi->GetSpr(Gekko_SPR_DBAT1U), Jdi->GetSpr(Gekko_SPR_DBAT1L));
		Print(CuiColor::Normal, 6, 3, "%08X:%08X", Jdi->GetSpr(Gekko_SPR_DBAT2U), Jdi->GetSpr(Gekko_SPR_DBAT2L));
		Print(CuiColor::Normal, 6, 4, "%08X:%08X", Jdi->GetSpr(Gekko_SPR_DBAT3U), Jdi->GetSpr(Gekko_SPR_DBAT3L));

		Print(CuiColor::Normal, 6, 6, "%08X:%08X", Jdi->GetSpr(Gekko_SPR_IBAT0U), Jdi->GetSpr(Gekko_SPR_IBAT0L));
		Print(CuiColor::Normal, 6, 7, "%08X:%08X", Jdi->GetSpr(Gekko_SPR_IBAT1U), Jdi->GetSpr(Gekko_SPR_IBAT1L));
		Print(CuiColor::Normal, 6, 8, "%08X:%08X", Jdi->GetSpr(Gekko_SPR_IBAT2U), Jdi->GetSpr(Gekko_SPR_IBAT2L));
		Print(CuiColor::Normal, 6, 9, "%08X:%08X", Jdi->GetSpr(Gekko_SPR_IBAT3U), Jdi->GetSpr(Gekko_SPR_IBAT3L));

		// BATs detailed

		describe_bat_reg(24, 1, Jdi->GetSpr(Gekko_SPR_DBAT0U), Jdi->GetSpr(Gekko_SPR_DBAT0L), false);
		describe_bat_reg(24, 2, Jdi->GetSpr(Gekko_SPR_DBAT1U), Jdi->GetSpr(Gekko_SPR_DBAT1L), false);
		describe_bat_reg(24, 3, Jdi->GetSpr(Gekko_SPR_DBAT2U), Jdi->GetSpr(Gekko_SPR_DBAT2L), false);
		describe_bat_reg(24, 4, Jdi->GetSpr(Gekko_SPR_DBAT3U), Jdi->GetSpr(Gekko_SPR_DBAT3L), false);

		describe_bat_reg(24, 6, Jdi->GetSpr(Gekko_SPR_IBAT0U), Jdi->GetSpr(Gekko_SPR_IBAT0L), true);
		describe_bat_reg(24, 7, Jdi->GetSpr(Gekko_SPR_IBAT1U), Jdi->GetSpr(Gekko_SPR_IBAT1L), true);
		describe_bat_reg(24, 8, Jdi->GetSpr(Gekko_SPR_IBAT2U), Jdi->GetSpr(Gekko_SPR_IBAT2L), true);
		describe_bat_reg(24, 9, Jdi->GetSpr(Gekko_SPR_IBAT3U), Jdi->GetSpr(Gekko_SPR_IBAT3L), true);

		// Segment regs

		for (int n = 0, y = 1; n < 16; n++, y++)
		{
			uint32_t sr = Jdi->GetSr(n);
			CuiColor prefix = sr & 0x80000000 ? CuiColor::BrightRed : CuiColor::Normal;

			Print(CuiColor::Cyan, 64, y, "sr%-2i  ", n);
			Print(prefix, 70, y, "%08X", sr);
		}
	}

	void GekkoRegs::RotateView(bool forward)
	{
		if (forward)
		{
			switch (mode)
			{
				case GekkoRegmode::GPR: mode = GekkoRegmode::FPR; break;
				case GekkoRegmode::FPR: mode = GekkoRegmode::PSR; break;
				case GekkoRegmode::PSR: mode = GekkoRegmode::MMU; break;
				case GekkoRegmode::MMU: mode = GekkoRegmode::GPR; break;
			}
		}
		else
		{
			switch (mode)
			{
				case GekkoRegmode::GPR: mode = GekkoRegmode::MMU; break;
				case GekkoRegmode::FPR: mode = GekkoRegmode::GPR; break;
				case GekkoRegmode::PSR: mode = GekkoRegmode::FPR; break;
				case GekkoRegmode::MMU: mode = GekkoRegmode::PSR; break;
			}
		}
	}

	void GekkoRegs::print_gprreg(int x, int y, int num)
	{
		uint32_t value = Jdi->GetGpr(num);

		Print (CuiColor::Cyan, x, y, "%-3s ", gprnames[num]);

		if (value != savedGpr[num])
		{
			Print(CuiColor::Green, x + 4, y, "%08X", value);
			savedGpr[num] = value;
		}
		else
		{
			Print(CuiColor::Normal, x + 4, y, "%08X", value);
		}
	}

	void GekkoRegs::print_fpreg(int x, int y, int num)
	{
		Fpreg value;

		value.Raw = Jdi->GetPs0(num);

		Print(CuiColor::Cyan, x, y, "f%-2i ", num);

		if (value.Raw != savedPs0[num].Raw)
		{
			if (value.Float >= 0.0) Print(CuiColor::Green, x + 4, y, " %e", value.Float);
			else Print(CuiColor::Green, x + 4, y, "%e", value.Float);

			Print(CuiColor::Green, x + 20, y, "%08X %08X", value.u.High, value.u.Low);

			savedPs0[num].Raw = value.Raw;
		}
		else
		{
			if (value.Float >= 0.0) Print(CuiColor::Normal, x + 4, y, " %e", value.Float);
			else Print(CuiColor::Normal, x + 4, y, "%e", value.Float);

			Print(CuiColor::Normal, x + 20, y, "%08X %08X", value.u.High, value.u.Low);
		}
	}

	void GekkoRegs::print_ps(int x, int y, int num)
	{
		Fpreg ps0, ps1;

		ps0.Raw = Jdi->GetPs0(num);
		ps1.Raw = Jdi->GetPs1(num);

		Print(CuiColor::Cyan, x, y, "ps%-2i ", num);

		if (ps0.Raw != savedPs0[num].Raw)
		{
			if (ps0.Float >= 0.0f) Print(CuiColor::Green, x + 6, y, " %.4e", ps0.Float);
			else Print(CuiColor::Green, x + 6, y, "%.4e", ps0.Float);

			savedPs0[num].Raw = ps0.Raw;
		}
		else
		{
			if (ps0.Float >= 0.0f) Print(CuiColor::Normal, x + 6, y, " %.4e", ps0.Float);
			else Print(CuiColor::Normal, x + 6, y, "%.4e", ps0.Float);
		}

		if (ps1.Raw != savedPs1[num].Raw)
		{
			if (ps1.Float >= 0.0f) Print(CuiColor::Green, x + 18, y, " %.4e", ps1.Float);
			else Print(CuiColor::Green, x + 18, y, "%.4e", ps1.Float);

			savedPs1[num].Raw = ps1.Raw;
		}
		else
		{
			if (ps1.Float >= 0.0f) Print(CuiColor::Normal, x + 18, y, " %.4e", ps1.Float);
			else Print(CuiColor::Normal, x + 18, y, "%.4e", ps1.Float);
		}
	}

	int GekkoRegs::cntlzw(uint32_t val)
	{
		int i;
		for (i = 0; i < 32; i++)
		{
			if (val & (1 << (31 - i))) break;
		}
		return ((i == 32) ? 31 : i);
	}

	void GekkoRegs::describe_bat_reg(int x, int y, uint32_t up, uint32_t lo, bool instr)
	{
		// Use plain numbers, no definitions (for best compatibility).
		uint32_t bepi = (up >> 17) & 0x7fff;
		uint32_t bl = (up >> 2) & 0x7ff;
		uint32_t vs = (up >> 1) & 1;
		uint32_t vp = up & 1;
		uint32_t brpn = (lo >> 17) & 0x7fff;
		uint32_t w = (lo >> 6) & 1;
		uint32_t i = (lo >> 5) & 1;
		uint32_t m = (lo >> 4) & 1;
		uint32_t g = (lo >> 3) & 1;
		uint32_t pp = lo & 3;

		uint32_t EStart = bepi << 17, PStart = brpn << 17;
		uint32_t blkSize = 1 << (17 + 11 - cntlzw((bl << (32 - 11)) | 0x00100000));

		const char* ppstr = "NA";
		if (pp)
		{
			if (instr) { ppstr = ((pp & 1) ? (char*)("X") : (char*)("XW")); }
			else { ppstr = ((pp & 1) ? (char*)("R") : (char*)("RW")); }
		}

		char temp[0x100];

		sprintf_s(temp, sizeof(temp), "%08X->%08X" " %-6s" " %c%c%c%c" " %s %s" " %s",
			EStart, PStart, smart_size(blkSize).c_str(),
			w ? 'W' : '-',
			i ? 'I' : '-',
			m ? 'M' : '-',
			g ? 'G' : '-',
			vs ? "Vs" : "Ns",
			vp ? "Vp" : "Np",
			ppstr);

		Print(CuiColor::Normal, x, y, temp);
	}

	std::string GekkoRegs::smart_size(size_t size)
	{
		char tempBuf[0x20] = { 0, };

		if (size < 1024)
		{
			sprintf_s(tempBuf, sizeof(tempBuf), "%zi byte", size);
		}
		else if (size < 1024 * 1024)
		{
			sprintf_s(tempBuf, sizeof(tempBuf), "%zi KB", size / 1024);
		}
		else if (size < 1024 * 1024 * 1024)
		{
			sprintf_s(tempBuf, sizeof(tempBuf), "%zi MB", size / 1024 / 1024);
		}
		else
		{
			sprintf_s(tempBuf, sizeof(tempBuf), "%1.1f GB", (float)size / 1024 / 1024 / 1024);
		}

		return tempBuf;
	}

}
