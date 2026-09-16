// The Flipper subsystem reports (hwdebug.h has the module description).
//
// The reports are plain formatting: every value comes straight out of the `XxxState` structure of
// the block, and the decoded fields are the ones a reading of the block's header (or of the
// hardware document it was written from) is needed for. Nothing here touches the emulated machine
// other than to read it, so a report can be taken at any point of a session.

#include "pch.h"

#include <string>
#include <vector>

namespace Flipper
{

	// ========================================================================================
	// Formatting helpers
	// ========================================================================================

	// A Markdown table needs every row to have the same number of cells, so the helpers below
	// take the values and lay them out. Nothing here tries to be clever about the column widths:
	// the front end (and a Markdown reader) pads a table row itself.

	static void MdRow(std::string& md, const char* name, uint32_t value)
	{
		char line[0x100];
		sprintf(line, "| `%s` | `0x%08X` |\n", name, value);
		md += line;
	}

	static void MdRow2(std::string& md, const char* name0, uint32_t value0, const char* name1, uint32_t value1)
	{
		char line[0x100];
		sprintf(line, "| `%s` | `0x%08X` | `%s` | `0x%08X` |\n", name0, value0, name1, value1);
		md += line;
	}

	static void MdTable2(std::string& md)
	{
		md += "| Register | Value | Register | Value |\n";
		md += "|---|---|---|---|\n";
	}

	static void MdTable1(std::string& md)
	{
		md += "| Register | Value |\n";
		md += "|---|---|\n";
	}

	// A title for one section of a report.
	static void MdSection(std::string& md, const char* title)
	{
		md += "\n## ";
		md += title;
		md += "\n\n";
	}

	// `name = value` on one bullet line. Used for the decoded values, where a table cell would be
	// mostly empty space.
	static void MdBullet(std::string& md, const char* format, ...)
	{
		char text[0x100];
		va_list args;
		va_start(args, format);
		vsnprintf(text, sizeof(text), format, args);
		va_end(args);

		md += "- ";
		md += text;
		md += "\n";
	}

	// A range of bytes as a fenced hexdump with a text column. `length` is bounded by the caller.
	static void MdHexDump(std::string& md, const std::vector<uint8_t>& data, uint64_t base, size_t length,
		size_t bytesPerLine = 16, size_t maxLines = 64)
	{
		if (data.empty() || length == 0 || bytesPerLine == 0)
			return;

		if (length > data.size())
			length = data.size();

		// The address column is as wide as the base address needs, capped at eight digits.
		char probe[0x20];
		sprintf(probe, "%llX", (unsigned long long)base);
		size_t addressWidth = strlen(probe);
		if (addressWidth < 4) addressWidth = 4;
		if (addressWidth > 8) addressWidth = 8;

		size_t lines = (length + bytesPerLine - 1) / bytesPerLine;
		if (lines > maxLines)
			lines = maxLines;

		md += "```\n";

		for (size_t row = 0; row < lines; row++)
		{
			size_t offset = row * bytesPerLine;

			char text[0x100];
			sprintf(text, "%0*llX  ", (int)addressWidth, (unsigned long long)(base + offset));
			md += text;

			std::string chars;

			for (size_t b = 0; b < bytesPerLine; b++)
			{
				if ((offset + b) < length)
				{
					uint8_t value = data[offset + b];
					sprintf(text, "%02X ", value);
					chars += (value >= 0x20 && value < 0x7F) ? (char)value : '.';
				}
				else
				{
					// The last row is padded, so the columns still line up.
					sprintf(text, "   ");
					chars += ' ';
				}

				md += text;
			}

			md += "|" + chars + "|\n";
		}

		if (lines < ((length + bytesPerLine - 1) / bytesPerLine))
		{
			char text[0x100];
			sprintf(text, "... (%llu more lines)\n", (unsigned long long)(((length + bytesPerLine - 1) / bytesPerLine) - lines));
			md += text;
		}

		md += "```\n";
	}

	// A list of the bits a masked value has set, by name. The bit names come in as
	// `bit, "name"` pairs; a bit that is set but has no name is reported by its number.
	struct BitName
	{
		uint32_t bit;
		const char* name;
	};

	static std::string DecodeBits(uint32_t value, const BitName* names, size_t count)
	{
		std::string result;

		for (size_t i = 0; i < count; i++)
		{
			if ((value & names[i].bit) == 0)
				continue;

			if (!result.empty())
				result += ", ";
			result += names[i].name;
		}

		return result;
	}

	static const char* YesNo(bool value)
	{
		return value ? "yes" : "no";
	}

	// A byte view of a structure, for the hexdumps (the structures are packed the way the
	// hardware registers are laid out, which is what makes the dump readable).
	template <typename T>
	static std::vector<uint8_t> BytesOf(const T& value)
	{
		const uint8_t* begin = (const uint8_t*)&value;
		return std::vector<uint8_t>(begin, begin + sizeof(T));
	}

	// The reports below are only meaningful with a machine behind them.
	static bool MachineReady()
	{
		return HW != nullptr;
	}


	// ========================================================================================
	// AI - the audio interface
	// ========================================================================================

	std::string AIReport()
	{
		if (!MachineReady() || HW->ai == nullptr)
			return "";

		const AIState& ai = HW->ai->State();

		static const BitName controlBits[] =
		{
			{ AICR_DFR,        "DFR (DSP DMA rate 32 kHz)" },
			{ AICR_SCRESET,    "SCRESET (reset the sample counter)" },
			{ AICR_AIINTVLD,   "AIINTVLD (the counter does not raise AIINT)" },
			{ AICR_AIINT,      "AIINT (streaming interrupt)" },
			{ AICR_AIINTMSK,   "AIINTMSK (interrupt enabled)" },
			{ AICR_AFR,        "AFR (48 kHz)" },
			{ AICR_PSTAT,      "PSTAT (the DDU streaming clock is on)" },
		};

		std::string md = "# Audio Interface (AI)\n";

		MdSection(md, "AI Streaming");
		MdTable2(md);
		MdRow2(md, "AIS_CR", ai.cr, "AIS_VR", ai.vr);
		MdRow2(md, "AIS_SCNT", ai.scnt, "AIS_IT", ai.it);

		MdSection(md, "Decoded");
		MdBullet(md, "sample rate: **%s**", (ai.cr & AICR_AFR) ? "48000 Hz" : "32000 Hz");
		MdBullet(md, "DSP DMA rate: **%s**", (ai.cr & AICR_DFR) ? "32000 Hz" : "48000 Hz");
		MdBullet(md, "left volume: **%u**, right volume: **%u**", (unsigned)(ai.vr & 0xFF), (unsigned)((ai.vr >> 8) & 0xFF));
		MdBullet(md, "sample counter: **%u** of **%u**", (unsigned)ai.scnt, (unsigned)ai.it);

		std::string bits = DecodeBits(ai.cr, controlBits, _countof(controlBits));
		MdBullet(md, "control: %s", bits.empty() ? "none" : bits.c_str());

		MdSection(md, "Stream FIFO");
		MdBullet(md, "**%u** of **%u** bytes buffered",
			(unsigned)ai.streamFifoPtr, (unsigned)sizeof(ai.streamFifo));

		std::vector<uint8_t> fifo(ai.streamFifo, ai.streamFifo + sizeof(ai.streamFifo));
		MdHexDump(md, fifo, 0, fifo.size(), 16, 4);

		return md;
	}


	// ========================================================================================
	// VI - the video interface
	// ========================================================================================

	std::string VIReport()
	{
		if (!MachineReady() || HW->vi == nullptr)
			return "";

		const VIState& vi = HW->vi->State();

		std::string md = "# Video Interface (VI)\n";

		MdSection(md, "Registers");
		MdTable2(md);
		MdRow2(md, "VI_DISP_CR", vi.disp_cr, "VI_VERT_TIMING", vi.vert_timing);
		MdRow2(md, "VI_TFBL", vi.tfbl, "VI_BFBL", vi.bfbl);
		MdRow2(md, "VI_DISP_POS", vi.pos.val, "VI_INT0", vi.int0.val);
		MdRow2(md, "VI_DISP_LATCH0", vi.latch0.val, "VI_DISP_LATCH1", vi.latch1.val);

		MdSection(md, "Decoded");
		MdBullet(md, "video format: **%s** (%s fuse)",
			VI_CR_FMT(vi.disp_cr) ? "PAL-like" : "NTSC-like",
			vi.videoEncoderFuse ? "PAL" : "NTSC");
		MdBullet(md, "timing generation: **%s**", (vi.disp_cr & VI_CR_ENB) ? "enabled" : "disabled");
		MdBullet(md, "scan mode: **%s**", (vi.disp_cr & VI_CR_NIN) ? "non-interlaced" : "interlaced");
		MdBullet(md, "3D display mode (DLR): **%s**", YesNo((vi.disp_cr & VI_CR_DLR) != 0));
		MdBullet(md, "gun trigger mode (LE0): **%u**, display latch 1 (LE1): **%u**", VI_CR_LE0(vi.disp_cr), VI_CR_LE1(vi.disp_cr));
		MdBullet(md, "raster: line **%u** of **%u**", VI_POS_VCT(vi.pos.val), (unsigned)vi.vcount);
		MdBullet(md, "field: **%s**, frame length: **%lld** ticks",
			vi.inter ? "interlaced" : "single", (long long)vi.one_frame);

		MdSection(md, "Output");
		MdBullet(md, "XFB output: **%s**", YesNo(vi.xfb));
		MdBullet(md, "top field buffer: **0x%08X**, bottom field buffer: **0x%08X**", vi.tfbl, vi.bfbl);
		MdBullet(md, "frames scanned out: **%llu**", (unsigned long long)vi.frames);

		return md;
	}


	// ========================================================================================
	// PI - the processor interface
	// ========================================================================================

	static const char* InterruptName(PIInterruptSource source)
	{
		switch (source)
		{
		case PIInterruptSource::PI: return "PI";
		case PIInterruptSource::RSW: return "RSW";
		case PIInterruptSource::DI: return "DI";
		case PIInterruptSource::SI: return "SI";
		case PIInterruptSource::EXI: return "EXI";
		case PIInterruptSource::AI: return "AI";
		case PIInterruptSource::DSP: return "DSP";
		case PIInterruptSource::MEM: return "MEM";
		case PIInterruptSource::VI: return "VI";
		case PIInterruptSource::PE_TOKEN: return "PE_TOKEN";
		case PIInterruptSource::PE_FINISH: return "PE_FINISH";
		case PIInterruptSource::CP: return "CP";
		case PIInterruptSource::DEBUG: return "DEBUG";
		case PIInterruptSource::HSP: return "HSP";
		default: return "?";
		}
	}

	std::string PIReport()
	{
		if (!MachineReady() || HW->pi == nullptr)
			return "";

		const PIState& pi = HW->pi->State();

		static const BitName intBits[] =
		{
			{ PI_INTERRUPT_PI,        "PI" },
			{ PI_INTERRUPT_RSW,       "RSW" },
			{ PI_INTERRUPT_DI,        "DI" },
			{ PI_INTERRUPT_SI,        "SI" },
			{ PI_INTERRUPT_EXI,       "EXI" },
			{ PI_INTERRUPT_AI,        "AI" },
			{ PI_INTERRUPT_DSP,       "DSP" },
			{ PI_INTERRUPT_MEM,       "MEM" },
			{ PI_INTERRUPT_VI,        "VI" },
			{ PI_INTERRUPT_PE_TOKEN,  "PE_TOKEN" },
			{ PI_INTERRUPT_PE_FINISH, "PE_FINISH" },
			{ PI_INTERRUPT_CP,        "CP" },
			{ PI_INTERRUPT_DEBUG,     "DEBUG" },
			{ PI_INTERRUPT_HSP,       "HSP" },
		};

		std::string md = "# Processor Interface (PI)\n";

		MdSection(md, "Registers");
		MdTable2(md);
		MdRow2(md, "PI_INTSR", pi.intsr, "PI_INTMR", pi.intmr);
		MdRow2(md, "PI_CHIPID", pi.chipid, "console version", pi.consoleVer);
		MdRow2(md, "PI_CPBAS", pi.cp_base, "PI_CPTOP", pi.cp_top);
		MdRow2(md, "PI_CPWRT", pi.cp_wrptr, "wrap bit", pi.wrap_bit);

		MdSection(md, "Interrupts");
		MdBullet(md, "pending (INTSR): %s",
			DecodeBits(pi.intsr, intBits, _countof(intBits)).empty() ? "none" :
			DecodeBits(pi.intsr, intBits, _countof(intBits)).c_str());
		MdBullet(md, "enabled (INTMR): %s",
			DecodeBits(pi.intmr, intBits, _countof(intBits)).empty() ? "none" :
			DecodeBits(pi.intmr, intBits, _countof(intBits)).c_str());
		MdBullet(md, "one-shot break: `0x%04X`", pi.intbrk);

		MdSection(md, "Interrupt counters");
		MdTable1(md);
		for (size_t i = 0; i < (size_t)PIInterruptSource::Max; i++)
		{
			char line[0x100];
			sprintf(line, "| `%s` | %lld |\n", InterruptName((PIInterruptSource)i),
				(long long)pi.intCounters[i]);
			md += line;
		}

		return md;
	}


	// ========================================================================================
	// MI - the memory interface
	// ========================================================================================

	std::string MIReport()
	{
		if (!MachineReady() || HW->mem == nullptr)
			return "";

		const MIState& mi = HW->mem->State();

		std::string md = "# Memory Interface (MI)\n";

		MdSection(md, "Memory Arbitration Registers (MARR)");
		MdTable2(md);
		for (int i = 0; i < 4; i++)
		{
			char name0[0x20], name1[0x20];
			sprintf(name0, "MARR%i start", i);
			sprintf(name1, "MARR%i end", i);
			MdRow2(md, name0, mi.marr_start[i], name1, mi.marr_end[i]);
		}
		MdRow(md, "MARR control", mi.marr_control.bits);

		MdSection(md, "MARR control decoded");
		MdBullet(md, "MARR0 read %s, write %s",
			YesNo(mi.marr_control.marr0_read_enable), YesNo(mi.marr_control.marr0_write_enable));
		MdBullet(md, "MARR1 read %s, write %s",
			YesNo(mi.marr_control.marr1_read_enable), YesNo(mi.marr_control.marr1_write_enable));
		MdBullet(md, "MARR2 read %s, write %s",
			YesNo(mi.marr_control.marr2_read_enable), YesNo(mi.marr_control.marr2_write_enable));
		MdBullet(md, "MARR3 read %s, write %s",
			YesNo(mi.marr_control.marr3_read_enable), YesNo(mi.marr_control.marr3_write_enable));

		MdSection(md, "Interrupts");
		MdTable2(md);
		MdRow2(md, "MEM_INT_ENABLE", mi.int_enable.bits, "MEM_INT_STATUS", mi.int_status.bits);
		MdBullet(md, "protection: MARR0 %s, MARR1 %s, MARR2 %s, MARR3 %s, address error %s",
			YesNo(mi.int_status.marr0 != 0), YesNo(mi.int_status.marr1 != 0),
			YesNo(mi.int_status.marr2 != 0), YesNo(mi.int_status.marr3 != 0),
			YesNo(mi.int_status.addr_err != 0));

		MdSection(md, "Counters");
		MdTable2(md);
		MdRow2(md, "CP counter", mi.cp_counter.cnt, "TC counter", mi.tc_counter.cnt);
		MdRow2(md, "PI read counter", mi.pi_read_counter.cnt, "PI write counter", mi.pi_write_counter.cnt);
		MdRow2(md, "DSP counter", mi.dsp_counter.cnt, "IO counter", mi.io_counter.cnt);
		MdRow2(md, "VI counter", mi.vi_counter.cnt, "PE counter", mi.pe_counter.cnt);

		MdSection(md, "Memory");
		MdBullet(md, "Splash: **%llu** bytes", (unsigned long long)mi.ramSize);

		return md;
	}


	// ========================================================================================
	// DI - the disk interface
	// ========================================================================================

	std::string DIReport()
	{
		if (!MachineReady() || HW->di == nullptr)
			return "";

		const DIState& di = HW->di->State();

		static const BitName statusBits[] =
		{
			{ DI_SR_BRKINT,    "BRKINT (break interrupt)" },
			{ DI_SR_BRKINTMSK, "BRKINTMSK (break interrupt enabled)" },
			{ DI_SR_TCINT,     "TCINT (transfer complete)" },
			{ DI_SR_TCINTMSK,  "TCINTMSK (transfer complete enabled)" },
			{ DI_SR_DEINT,     "DEINT (device error)" },
			{ DI_SR_DEINTMSK,  "DEINTMSK (device error enabled)" },
			{ DI_SR_BRK,       "BRK (break requested)" },
		};

		std::string md = "# Disk Interface (DI)\n";

		MdSection(md, "Registers");
		MdTable2(md);
		MdRow2(md, "DI_SR", di.sr, "DI_CVR", di.cvr);
		MdRow2(md, "DI_MAR", di.mar, "DI_LEN", di.len);
		MdRow2(md, "DI_CR", di.cr, "DI_CFG", di.cfg);

		MdSection(md, "Status decoded");
		MdBullet(md, "cover: **%s**", (di.cvr & DI_CVR_CVR) ? "open" : "closed");
		MdBullet(md, "cover interrupt: %s, mask %s",
			YesNo((di.cvr & DI_CVR_CVRINT) != 0), YesNo((di.cvr & DI_CVR_CVRINTMSK) != 0));
		MdBullet(md, "transfer: **%s**, **%s**", (di.cr & DI_CR_DMA) ? "DMA" : "immediate",
			(di.cr & DI_CR_RW) ? "host to drive" : "drive to host");
		MdBullet(md, "start: **%s**", YesNo((di.cr & DI_CR_TSTART) != 0));
		MdBullet(md, "status: %s", DecodeBits(di.sr, statusBits, _countof(statusBits)).empty() ?
			"none" : DecodeBits(di.sr, statusBits, _countof(statusBits)).c_str());

		MdSection(md, "Command and immediate buffers");
		std::vector<uint8_t> cmd(di.cmdbuf, di.cmdbuf + sizeof(di.cmdbuf));
		MdHexDump(md, cmd, 0, cmd.size(), 12, 2);

		std::vector<uint8_t> imm(di.immbuf, di.immbuf + sizeof(di.immbuf));
		MdHexDump(md, imm, 0, imm.size(), 4, 2);

		MdSection(md, "Transfer");
		MdBullet(md, "drive to host: **%i** bytes left", di.dduToHostByteCounter);
		MdBullet(md, "host to drive: **%i** bytes left", di.hostToDduByteCounter);

		return md;
	}


	// ========================================================================================
	// SI - the serial interface
	// ========================================================================================

	std::string SIReport()
	{
		if (!MachineReady() || HW->si == nullptr)
			return "";

		const SIState& si = HW->si->State();

		static const BitName statusBits[] =
		{
			{ (uint32_t)SI_SR_WR,     "WR (channel buffer write)" },
			{ SI_SR_RDST0,  "RDST0" },
			{ SI_SR_WRST0,  "WRST0" },
			{ SI_SR_NOREP0, "NOREP0 (no response)" },
			{ SI_SR_COLL0,  "COLL0 (collision)" },
			{ SI_SR_OVRUN0, "OVRUN0 (overrun)" },
			{ SI_SR_UNRUN0, "UNRUN0 (underrun)" },
			{ SI_SR_RDST1,  "RDST1" },
			{ SI_SR_WRST1,  "WRST1" },
			{ SI_SR_NOREP1, "NOREP1" },
			{ SI_SR_COLL1,  "COLL1" },
			{ SI_SR_OVRUN1, "OVRUN1" },
			{ SI_SR_UNRUN1, "UNRUN1" },
			{ SI_SR_RDST2,  "RDST2" },
			{ SI_SR_WRST2,  "WRST2" },
			{ SI_SR_NOREP2, "NOREP2" },
			{ SI_SR_COLL2,  "COLL2" },
			{ SI_SR_OVRUN2, "OVRUN2" },
			{ SI_SR_UNRUN2, "UNRUN2" },
			{ SI_SR_RDST3,  "RDST3" },
			{ SI_SR_WRST3,  "WRST3" },
			{ SI_SR_NOREP3, "NOREP3" },
			{ SI_SR_COLL3,  "COLL3" },
			{ SI_SR_OVRUN3, "OVRUN3" },
			{ SI_SR_UNRUN3, "UNRUN3" },
		};

		std::string md = "# Serial Interface (SI)\n";

		MdSection(md, "Registers");
		MdTable2(md);
		MdRow2(md, "SI_POLL", si.poll, "SI_COMCSR", si.comcsr);
		MdRow2(md, "SI_SR", si.sr, "SI_EXILK", si.exilk);

		MdSection(md, "Channels");
		MdTable2(md);
		for (int i = 0; i < 4; i++)
		{
			char name0[0x20], name1[0x20];
			sprintf(name0, "SI_CHAN%i_OUTBUF", i);
			sprintf(name1, "SI_CHAN%i_SHDW", i);
			MdRow2(md, name0, si.out[i], name1, si.shdw[i]);
		}

		MdSection(md, "Communication");
		MdBullet(md, "transfer: **%s**", (si.comcsr & SI_COMCSR_TSTART) ? "running" : "idle");
		MdBullet(md, "channel: **%u**, out length: **%u**, in length: **%u**",
			SI_COMCSR_CHAN(si.comcsr), SI_COMCSR_OUTLEN(si.comcsr), SI_COMCSR_INLEN(si.comcsr));
		MdBullet(md, "transfer complete interrupt: %s, mask %s",
			YesNo((si.comcsr & SI_COMCSR_TCINT) != 0), YesNo((si.comcsr & SI_COMCSR_TCINTMSK) != 0));
		MdBullet(md, "communication error: **%s**", YesNo((si.comcsr & SI_COMCSR_COMERR) != 0));

		std::string bits = DecodeBits(si.sr, statusBits, _countof(statusBits));
		MdBullet(md, "status: %s", bits.empty() ? "none" : bits.c_str());

		MdSection(md, "Poll");
		MdBullet(md, "poll every **%u** lines, at most **%u** polls per frame",
			SI_POLL_X(si.poll), SI_POLL_Y(si.poll));
		MdBullet(md, "polling enabled: port0 %s, port1 %s, port2 %s, port3 %s",
			YesNo((si.poll & SI_POLL_EN0) != 0), YesNo((si.poll & SI_POLL_EN1) != 0),
			YesNo((si.poll & SI_POLL_EN2) != 0), YesNo((si.poll & SI_POLL_EN3) != 0));
		MdBullet(md, "video copy: port0 %s, port1 %s, port2 %s, port3 %s",
			YesNo((si.poll & SI_POLL_VBCPY0) != 0), YesNo((si.poll & SI_POLL_VBCPY1) != 0),
			YesNo((si.poll & SI_POLL_VBCPY2) != 0), YesNo((si.poll & SI_POLL_VBCPY3) != 0));
		MdBullet(md, "polls issued this frame: **%u**", si.pollsThisFrame);

		MdSection(md, "Controllers");
		MdTable2(md);
		for (int i = 0; i < 4; i++)
		{
			char name0[0x20], name1[0x20];
			sprintf(name0, "PAD%i buttons", i);
			sprintf(name1, "PAD%i stick", i);
			MdRow2(md, name0, si.pad[i].button, name1,
				(uint32_t)(si.pad[i].stickX | (si.pad[i].stickY << 8)));
		}
		MdBullet(md, "rumble: port0 %s, port1 %s, port2 %s, port3 %s",
			YesNo(si.rumble[0]), YesNo(si.rumble[1]), YesNo(si.rumble[2]), YesNo(si.rumble[3]));

		return md;
	}


	// ========================================================================================
	// EXI - the external interface
	// ========================================================================================

	std::string EXIReport()
	{
		if (!MachineReady() || HW->exi == nullptr)
			return "";

		const EXIState& exi = HW->exi->State();

		std::string md = "# External Interface (EXI)\n";

		MdSection(md, "Channels");
		MdTable2(md);
		for (int i = 0; i < 3; i++)
		{
			char name0[0x20], name1[0x20], name2[0x20];
			sprintf(name0, "EXI%i_CSR", i);
			sprintf(name1, "EXI%i_CR", i);
			sprintf(name2, "EXI%i_DATA", i);
			MdRow2(md, name0, exi.regs[i].csr, name1, exi.regs[i].cr);
			MdRow(md, name2, exi.regs[i].data);
		}

		MdSection(md, "DMA");
		MdTable2(md);
		for (int i = 0; i < 3; i++)
		{
			char name0[0x20], name1[0x20];
			sprintf(name0, "EXI%i_MADR", i);
			sprintf(name1, "EXI%i_LEN", i);
			MdRow2(md, name0, exi.regs[i].madr, name1, exi.regs[i].len);
		}

		MdSection(md, "Decoded");
		for (int i = 0; i < 3; i++)
		{
			uint16_t csr = exi.regs[i].csr;
			MdBullet(md, "EXI%i: device %s, attached %s, clock **%u** MHz, transfer %s",
				i,
				(csr & EXI_CSR_CS0B) ? "0" : ((csr & EXI_CSR_CS1B) ? "1" : ((csr & EXI_CSR_CS2B) ? "2" : "none")),
				YesNo((csr & EXI_CSR_EXT) != 0),
				(unsigned)((csr & 0x70) ? (8 >> EXI_CSR_CLK(csr)) : 8),
				(exi.regs[i].cr & EXI_CR_TSTART) ? "running" : "idle");
			MdBullet(md, "EXI%i: transfer complete %s, external interrupt %s, external insert %s",
				i,
				YesNo((csr & EXI_CSR_TCINT) != 0),
				YesNo((csr & EXI_CSR_EXIINT) != 0),
				YesNo((csr & EXI_CSR_EXTINT) != 0));
		}

		MdSection(md, "Selected device");
		MdBullet(md, "channel: **%i**", exi.chan);
		MdBullet(md, "device: **%i**%s", exi.sel, exi.sel < 0 ? " (none)" : "");
		MdBullet(md, "Macronix address: **0x%08X**", exi.mxaddr);
		MdBullet(md, "RTC value: **0x%08X**", exi.rtcVal);
		MdBullet(md, "boot ROM installed: **%s** (%llu bytes)", YesNo(exi.BootromPresent),
			(unsigned long long)exi.bootromSize);

		MdSection(md, "SRAM (console settings)");
		std::vector<uint8_t> sram((const uint8_t*)&exi.sram, (const uint8_t*)&exi.sram + sizeof(exi.sram));
		MdHexDump(md, sram, 0, sram.size(), 16, 4);

		return md;
	}


	// ========================================================================================
	// CP - the command processor
	// ========================================================================================

	static const char* ArrayIdName(ArrayId id)
	{
		switch (id)
		{
		case ArrayId::Pos: return "Pos";
		case ArrayId::Nrm: return "Nrm";
		case ArrayId::Color0: return "Color0";
		case ArrayId::Color1: return "Color1";
		case ArrayId::Tex0Coord: return "Tex0";
		case ArrayId::Tex1Coord: return "Tex1";
		case ArrayId::Tex2Coord: return "Tex2";
		case ArrayId::Tex3Coord: return "Tex3";
		case ArrayId::Tex4Coord: return "Tex4";
		case ArrayId::Tex5Coord: return "Tex5";
		case ArrayId::Tex6Coord: return "Tex6";
		case ArrayId::Tex7Coord: return "Tex7";
		case ArrayId::IndexRegA: return "IndexA";
		case ArrayId::IndexRegB: return "IndexB";
		case ArrayId::IndexRegC: return "IndexC";
		case ArrayId::IndexRegD: return "IndexD";
		default: return "?";
		}
	}

	static const char* AttrTypeName(unsigned type)
	{
		switch (type)
		{
		case VCD_NONE: return "none";
		case VCD_DIRECT: return "direct";
		case VCD_INDEX8: return "index8";
		case VCD_INDEX16: return "index16";
		default: return "?";
		}
	}

	std::string CPReport()
	{
		if (!MachineReady() || HW->cp == nullptr)
			return "";

		const CPHostRegs& regs = HW->cp->HostRegs();
		const CPState& cp = HW->cp->State();

		uint32_t occupancy = 0;
		HW->cp->FifoOccupancy(&occupancy);

		std::string md = "# Command Processor (CP)\n";

		MdSection(md, "Registers");
		MdTable2(md);
		MdRow2(md, "CP_STATUS", regs.sr, "CP_ENABLE", regs.cr);
		MdRow2(md, "CP_FIFO_BASE", regs.base, "CP_FIFO_TOP", regs.top);
		MdRow2(md, "CP_FIFO_WPTR", regs.wrptr, "CP_FIFO_RPTR", regs.rdptr);
		MdRow2(md, "CP_FIFO_COUNT", regs.cnt, "CP_FIFO_BRK", regs.bpptr);
		MdRow2(md, "CP_FIFO_HICNT", regs.himark, "CP_FIFO_LOCNT", regs.lomark);
		MdRow2(md, "CP_XF_ADDR", regs.xfAddr, "CP_XF_DATA", regs.xfData);

		MdSection(md, "Status decoded");
		MdBullet(md, "reader: **%s**, command parser: **%s**",
			(regs.sr & CP_SR_RD_IDLE) ? "idle" : "busy",
			(regs.sr & CP_SR_CMD_IDLE) ? "idle" : "busy");
		MdBullet(md, "FIFO overflow: **%s**, underflow: **%s**",
			YesNo((regs.sr & CP_SR_OVF) != 0), YesNo((regs.sr & CP_SR_UVF) != 0));
		MdBullet(md, "break point hit: **%s**", YesNo((regs.sr & CP_SR_BPINT) != 0));
		MdBullet(md, "FIFO reads %s, overflow interrupt %s, underflow interrupt %s, break point interrupt %s",
			YesNo((regs.cr & CP_CR_RDEN) != 0), YesNo((regs.cr & CP_CR_OVFEN) != 0),
			YesNo((regs.cr & CP_CR_UVFEN) != 0), YesNo((regs.cr & CP_CR_BPINTEN) != 0));
		MdBullet(md, "ring occupancy: **%u** of **%u** entries (32 bytes each)",
			occupancy, regs.top > regs.base ? (regs.top - regs.base) / 32 : 0);

		MdSection(md, "Vertex formats in use");
		MdBullet(md, "VCD low: **0x%08X**, VCD high: **0x%08X**", cp.vcdLo.bits, cp.vcdHi.bits);
		MdBullet(md, "position: **%s**, normal: **%s**, color0: **%s**, color1: **%s**",
			AttrTypeName(cp.vcdLo.Position), AttrTypeName(cp.vcdLo.Normal),
			AttrTypeName(cp.vcdLo.Color0), AttrTypeName(cp.vcdLo.Color1));
		MdBullet(md, "texture coordinates: Tex0 **%s**, Tex1 **%s**, Tex2 **%s**, Tex3 **%s**",
			AttrTypeName(cp.vcdHi.Tex0Coord), AttrTypeName(cp.vcdHi.Tex1Coord),
			AttrTypeName(cp.vcdHi.Tex2Coord), AttrTypeName(cp.vcdHi.Tex3Coord));
		MdBullet(md, "texture coordinates: Tex4 **%s**, Tex5 **%s**, Tex6 **%s**, Tex7 **%s**",
			AttrTypeName(cp.vcdHi.Tex4Coord), AttrTypeName(cp.vcdHi.Tex5Coord),
			AttrTypeName(cp.vcdHi.Tex6Coord), AttrTypeName(cp.vcdHi.Tex7Coord));
		MdBullet(md, "matrix indices: PosNrm **%u**, Tex0 **%u**, Tex1 **%u**",
			cp.matIndexA.PosNrmIndex, cp.matIndexA.Tex0Index, cp.matIndexA.Tex1Index);

		MdSection(md, "Vertex attribute tables");
		MdTable2(md);
		for (int i = 0; i < 8; i++)
		{
			char name0[0x20], name1[0x20];
			sprintf(name0, "VAT%i A", i);
			sprintf(name1, "VAT%i B", i);
			MdRow2(md, name0, cp.vatA[i].bits, name1, cp.vatB[i].bits);
		}
		MdTable1(md);
		for (int i = 0; i < 8; i++)
		{
			char name[0x20];
			sprintf(name, "VAT%i C", i);
			MdRow(md, name, cp.vatC[i].bits);
		}

		MdSection(md, "Array base and stride");
		MdTable2(md);
		for (size_t i = 0; i < (size_t)ArrayId::Max; i++)
		{
			char name0[0x20], name1[0x20];
			sprintf(name0, "%s base", ArrayIdName((ArrayId)i));
			sprintf(name1, "%s stride", ArrayIdName((ArrayId)i));
			MdRow2(md, name0, cp.arrayBase[i].bits, name1, cp.arrayStride[i].bits);
		}

		return md;
	}


	// ========================================================================================
	// DSP - the signal processor
	// ========================================================================================

	std::string DSPReport()
	{
		if (!MachineReady() || DSP == nullptr || DSP->core == nullptr)
			return "";

		const DSP::DspRegs& regs = DSP->core->regs;

		std::string md = "# DSP\n";

		MdSection(md, "State");
		MdBullet(md, "reset: **%s**, halted: **%s**, CPU interrupt pending: **%s**",
			YesNo(DSP->GetResetBit()), YesNo(DSP->GetHaltBit()), YesNo(DSP->CpuIntRequested()));

		MdSection(md, "Registers");
		MdTable2(md);
		MdRow2(md, "PC", regs.pc, "DPP", regs.dpp);
		for (int i = 0; i < 4; i++)
		{
			char name0[0x20], name1[0x20], name2[0x20];
			sprintf(name0, "r%i", i);
			sprintf(name1, "m%i", i);
			sprintf(name2, "l%i", i);
			MdRow2(md, name0, regs.r[i], name1, regs.m[i]);
			MdRow(md, name2, regs.l[i]);
		}
		MdRow2(md, "A (low)", regs.a.l, "A (high)", regs.a.hm);
		MdRow2(md, "B (low)", regs.b.l, "B (high)", regs.b.hm);
		MdRow2(md, "X (low)", regs.x.l, "X (high)", regs.x.h);
		MdRow2(md, "Y (low)", regs.y.l, "Y (high)", regs.y.h);
		MdRow(md, "PSR", regs.psr.bits);
		MdRow2(md, "PROD (low)", regs.prod.l, "PROD (high)", (uint32_t)(regs.prod.m1 | (regs.prod.h << 16)));

		return md;
	}


	// ========================================================================================
	// The commands
	// ========================================================================================

	// Turn a report into what a JDI command answers with (the Markdown object the debugger
	// renders), or into the report of a problem. The reports are only asked for with a machine
	// behind them; asking for one without it is a user error, not a crash.
	static Json::Value* ReportAnswer(const std::string& markdown, const char* command)
	{
		if (markdown.empty())
		{
			Debug::Report(Debug::Channel::Norm, "%s: nothing is running\n", command);
			return nullptr;
		}

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Object;
		output->AddUtf8String("markdown", markdown.c_str());
		return output;
	}

	static Json::Value* CmdAIRegs(std::vector<std::string>& args) { return ReportAnswer(AIReport(), "airegs"); }
	static Json::Value* CmdVIRegs(std::vector<std::string>& args) { return ReportAnswer(VIReport(), "viregs"); }
	static Json::Value* CmdPIRegs(std::vector<std::string>& args) { return ReportAnswer(PIReport(), "piregs"); }
	static Json::Value* CmdMIRegs(std::vector<std::string>& args) { return ReportAnswer(MIReport(), "miregs"); }
	static Json::Value* CmdDIRegs(std::vector<std::string>& args) { return ReportAnswer(DIReport(), "diregs"); }
	static Json::Value* CmdSIRegs(std::vector<std::string>& args) { return ReportAnswer(SIReport(), "siregs"); }
	static Json::Value* CmdEXIRegs(std::vector<std::string>& args) { return ReportAnswer(EXIReport(), "exiregs"); }
	static Json::Value* CmdCPRegs(std::vector<std::string>& args) { return ReportAnswer(CPReport(), "cpregs"); }
	static Json::Value* CmdDSPRegs(std::vector<std::string>& args) { return ReportAnswer(DSPReport(), "dspstate"); }

	void hwdebug_init_handlers()
	{
		JDI::Hub.AddCmd("airegs", CmdAIRegs);
		JDI::Hub.AddCmd("viregs", CmdVIRegs);
		JDI::Hub.AddCmd("piregs", CmdPIRegs);
		JDI::Hub.AddCmd("miregs", CmdMIRegs);
		JDI::Hub.AddCmd("diregs", CmdDIRegs);
		JDI::Hub.AddCmd("siregs", CmdSIRegs);
		JDI::Hub.AddCmd("exiregs", CmdEXIRegs);
		JDI::Hub.AddCmd("cpregs", CmdCPRegs);
		JDI::Hub.AddCmd("dspstate", CmdDSPRegs);
	}

}
