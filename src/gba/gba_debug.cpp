// The debug interface of the portable machines (gba_debug.h has the module description).
//
// The reports are formatting over the public state of `GbaSystem` / `GbSystem` and their devices
// (`Bus()`, `Cpu()`, `Lcd()`, `Cartridge()`, ...). Memory is read through `GbaBus::Peek16` and
// `GbBus::Peek`, which decode an address exactly like the CPU does but without charging wait
// states, latching the open bus or advancing a serial port: a debugger looks at the machine, it
// does not run it.
//
// This file is the bridge between the portable module and the host, and it is the only file of the
// module that is compiled with the host: the JDI hub, the Markdown the debugger renders and the
// debugger itself all live on the GameCube side, and their headers (the precompiled one included)
// need the third-party paths the GBA library deliberately does not have (see src/gba/Readme.md).
// It is therefore a source of the `pureikyubu` project in the MSVC build - not of `GBA.vcxproj`,
// which is the portable core - the way `debugui2.cpp` is. The path of the include is the path the
// project declares for the precompiled header (`pureikyubu.vcxproj`), which is the way MSVC tells
// that this file took the header it was given; on the other compilers the two are the same file.

#include "pch.h"

#include "gba_debug.h"
#include "gba.h"
#include "gb.h"
#include "gba_disasm.h"
#include "gb_disasm.h"

#include "../debugui2.h"

#include <cstdio>
#include <string>

namespace GBA
{

	// ========================================================================================
	// The machine the debugger looks at
	// ========================================================================================

	static GbaSystem* debugGba = nullptr;
	static GbSystem* debugGb = nullptr;

	void SetDebugMachine(GbaSystem* machine)
	{
		debugGba = machine;
		if (machine != nullptr)
			debugGb = nullptr;
	}

	void SetDebugMachine(GbSystem* machine)
	{
		debugGb = machine;
		if (machine != nullptr)
			debugGba = nullptr;
	}

	DebugMachine CurrentDebugMachine()
	{
		if (debugGba != nullptr)
			return DebugMachine::Gba;
		if (debugGb != nullptr)
			return DebugMachine::Gb;
		return DebugMachine::None;
	}

	GbaSystem* CurrentGba()
	{
		return debugGba;
	}

	GbSystem* CurrentGb()
	{
		return debugGb;
	}

	std::string DebugMachineRomTitle()
	{
		if (debugGba != nullptr)
			return debugGba->RomTitle();
		if (debugGb != nullptr)
			return debugGb->RomTitle();
		return "";
	}

	bool DebugMachineActive()
	{
		return debugGba != nullptr || debugGb != nullptr;
	}


	// ========================================================================================
	// Markdown helpers
	// ========================================================================================

	static void MdSection(std::string& md, const char* title)
	{
		md += "\n## ";
		md += title;
		md += "\n\n";
	}

	static void MdTable1(std::string& md)
	{
		md += "| Register | Value |\n";
		md += "|---|---|\n";
	}

	static void MdTable2(std::string& md)
	{
		md += "| Register | Value | Register | Value |\n";
		md += "|---|---|---|---|\n";
	}

	static void MdRow1(std::string& md, const char* name, uint32_t value)
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

	static void MdBullet(std::string& md, const char* format, ...)
	{
		char text[0x200];
		va_list args;
		va_start(args, format);
		vsnprintf(text, sizeof(text), format, args);
		va_end(args);

		md += "- ";
		md += text;
		md += "\n";
	}

	static const char* YesNo(bool value)
	{
		return value ? "yes" : "no";
	}

	// A run of memory as a fenced hexdump. `base` only picks the address column, so that a dump of
	// a window that is not at address zero still reads as the addresses the machine uses.
	static void MdHexDump(std::string& md, const GbaBus& bus, uint32_t base, size_t length,
		size_t bytesPerLine = 16, size_t maxLines = 32)
	{
		if (length == 0 || bytesPerLine == 0)
			return;

		size_t lines = (length + bytesPerLine - 1) / bytesPerLine;
		if (lines > maxLines)
			lines = maxLines;

		md += "```\n";

		for (size_t row = 0; row < lines; row++)
		{
			uint32_t address = base + (uint32_t)(row * bytesPerLine);

			char text[0x100];
			sprintf(text, "%08X  ", address);
			md += text;

			std::string chars;

			for (size_t b = 0; b < bytesPerLine; b++)
			{
				// The peek is per byte so that a dump which runs off the end of a region shows
				// the open bus instead of reading past the memory that backs it.
				uint8_t value = (uint8_t)(bus.Peek16(address + (uint32_t)b) >> (((address + b) & 1) ? 8 : 0));
				sprintf(text, "%02X ", value);
				md += text;
				chars += (value >= 0x20 && value < 0x7F) ? (char)value : '.';
			}

			md += "|" + chars + "|\n";
		}

		if (lines < ((length + bytesPerLine - 1) / bytesPerLine))
		{
			char text[0x100];
			sprintf(text, "... (%llu more lines)\n",
				(unsigned long long)(((length + bytesPerLine - 1) / bytesPerLine) - lines));
			md += text;
		}

		md += "```\n";
	}

	// The same for the Game Boy, whose address space is 16-bit.
	static void MdHexDump(std::string& md, const GbBus& bus, uint16_t base, size_t length,
		size_t bytesPerLine = 16, size_t maxLines = 32)
	{
		if (length == 0 || bytesPerLine == 0)
			return;

		size_t lines = (length + bytesPerLine - 1) / bytesPerLine;
		if (lines > maxLines)
			lines = maxLines;

		md += "```\n";

		for (size_t row = 0; row < lines; row++)
		{
			uint16_t address = (uint16_t)(base + row * bytesPerLine);

			char text[0x100];
			sprintf(text, "%04X  ", address);
			md += text;

			std::string chars;

			for (size_t b = 0; b < bytesPerLine; b++)
			{
				uint8_t value = bus.Peek((uint16_t)(address + b));
				sprintf(text, "%02X ", value);
				md += text;
				chars += (value >= 0x20 && value < 0x7F) ? (char)value : '.';
			}

			md += "|" + chars + "|\n";
		}

		md += "```\n";
	}

	// Number of lines a `... <lines>` argument asks for, bounded the way the other dumps are.
	static size_t ClampLines(const char* text, size_t fallback)
	{
		int value = (text != nullptr) ? atoi(text) : 0;
		if (value <= 0)
			return fallback;
		if (value > 64)
			return 64;
		return (size_t)value;
	}

	// A byte count a command argument asked for, bounded.
	static size_t ClampCount(const char* text, size_t fallback, size_t limit)
	{
		int value = (text != nullptr) ? atoi(text) : 0;
		if (value <= 0)
			return fallback;
		if ((size_t)value > limit)
			return limit;
		return (size_t)value;
	}


	// ========================================================================================
	// The disassembler's view of a live machine
	// ========================================================================================

	static const size_t MaxDisasmLines = 40;

	// The ARM / Thumb disassembler reads the machine through this: the same `Peek16` a memory
	// panel uses, so a disassembly of an address nobody has mapped shows the open bus rather than
	// taking the machine somewhere.
	class GbaLiveMemory : public DisasmMemory
	{
		const GbaBus& bus;

	public:
		explicit GbaLiveMemory(const GbaBus& bus) : bus(bus) {}

		u16 Read16(u32 address) const override { return bus.Peek16(address); }
	};


	// ========================================================================================
	// GBA - the machine
	// ========================================================================================

	std::string GbaMachineReport()
	{
		if (debugGba == nullptr)
			return "";

		GbaSystem& system = *debugGba;
		const GbaBus& bus = system.Bus();

		std::string md = "# Game Boy Advance\n";

		MdSection(md, "Machine");
		MdBullet(md, "%s", system.Describe().c_str());
		MdBullet(md, "cycles: **%llu** (system clock **%u** Hz)", (unsigned long long)system.Cycles(), CyclesPerSecond);
		MdBullet(md, "frame: **%i**, link port: **%s**",
			system.FrameCounter(),
			system.Link().Peer() != nullptr ? "cable attached" : "no cable");
		MdBullet(md, "BIOS: **%s**, HLE: **%s**",
			bus.UsingCustomBios() ? "boot ROM" : "image",
			YesNo(bus.HleBiosEnabled));

		MdSection(md, "Interrupts");
		MdTable2(md);
		MdRow2(md, "IE", bus.irq.ReadIE(), "IF", bus.irq.ReadIF());
		MdRow1(md, "IME", bus.irq.ReadIME() ? 1 : 0);
		MdBullet(md, "IRQ line: **%s**", bus.irq.Pending() ? "asserted" : "low");

		MdSection(md, "Keypad");
		MdBullet(md, "pressed: **0x%03X** (1 = down), KEYINPUT: **0x%04X**, KEYCNT: **0x%04X**",
			(unsigned)bus.keypad.Pressed(), (unsigned)bus.keypad.ReadKeyInput(), (unsigned)bus.keypad.ReadKeyCnt());

		MdSection(md, "Post boot");
		MdBullet(md, "POSTFLG: **0x%02X**, CPU halted: **%s**",
			(unsigned)bus.PostFlg(), YesNo(bus.cpu.Halted()));

		MdSection(md, "CPU statistics");
		MdBullet(md, "instructions retired: **%llu**", (unsigned long long)bus.cpu.RetiredInstructions());
		MdBullet(md, "undefined instructions: **%llu**", (unsigned long long)bus.cpu.UndefinedInstructions());

		return md;
	}


	std::string GbaRegsReport()
	{
		if (debugGba == nullptr)
			return "";

		const Arm7tdmi& cpu = debugGba->Cpu();

		// The mode field of the CPSR: the seven modes of the ARM7TDMI, in the order of their
		// numbers (User/System share r8-r12 and are two encodings of the same window; the last
		// name covers both).
		static const char* modeNames[32] =
		{
			"usr", "?", "?", "?", "?", "?", "?", "?",			// 0x00
			"?", "?", "?", "?", "?", "?", "?", "?",				// 0x08
			"?", "?", "?", "fiq", "?", "?", "?", "?",			// 0x10
			"irq", "?", "?", "svc", "?", "?", "?", "sys",		// 0x18
		};

		uint32_t cpsr = cpu.ReadCPSR();
		unsigned mode = (unsigned)(cpsr & ModeMask);

		std::string md = "# ARM7TDMI Registers\n";

		MdSection(md, "Mode");
		MdBullet(md, "state: **%s** (%u-bit instructions)", cpu.ThumbState() ? "Thumb" : "ARM",
			cpu.ThumbState() ? 16 : 32);
		MdBullet(md, "mode: **%s** (0x%02X)", modeNames[mode & 0x1F], mode);
		MdBullet(md, "halted: **%s**, in exception: **%s**",
			YesNo(cpu.Halted()), YesNo(cpu.InException()));
		MdBullet(md, "CPSR: **0x%08X** (%s)", cpsr, ConditionFlags(cpsr).c_str());

		MdSection(md, "Registers");
		MdTable2(md);
		for (int i = 0; i < 16; i += 2)
		{
			char name0[0x10], name1[0x10];
			sprintf(name0, "r%i", i);
			sprintf(name1, "r%i", i + 1);
			MdRow2(md, name0, cpu.Reg(i), name1, cpu.Reg(i + 1));
		}

		MdSection(md, "Special");
		MdTable2(md);
		MdRow2(md, "PC", cpu.CurrentPC(), "SPSR", cpu.ReadSPSR());

		return md;
	}


	std::string GbaDisasmReport(size_t count)
	{
		if (debugGba == nullptr)
			return "";

		if (count == 0)
			count = 16;
		if (count > MaxDisasmLines)
			count = MaxDisasmLines;

		const Arm7tdmi& cpu = debugGba->Cpu();
		GbaLiveMemory memory(debugGba->Bus());

		bool thumb = cpu.ThumbState();
		uint32_t address = cpu.CurrentPC();

		std::string text;

		for (size_t i = 0; i < count; i++)
		{
			int size = thumb ? 2 : 4;

			std::string mnemonic = Disassemble(memory, address, thumb, &size);
			if (size <= 0)
				size = thumb ? 2 : 4;

			std::string bytes = InstructionBytes(memory, address, size);

			char line[0x100];
			sprintf(line, "%08X  %-10s %s\n", address, bytes.c_str(), mnemonic.c_str());
			text += line;

			address += (uint32_t)size;
		}

		char header[0x100];
		sprintf(header, "### PC = 0x%08X (%s)\n\n", cpu.CurrentPC(), thumb ? "Thumb" : "ARM");

		return std::string(header) + "```\n" + text + "```\n";
	}


	std::string GbaMemReport(uint32_t address, size_t lines)
	{
		if (debugGba == nullptr)
			return "";

		if (lines == 0)
			lines = 16;

		char header[0x100];
		sprintf(header, "# GBA Memory\n\n`0x%08X` .. `0x%08X`\n\n",
			address, address + (uint32_t)(lines * 16) - 1);

		std::string md = header;
		MdHexDump(md, debugGba->Bus(), address, lines * 16);

		return md;
	}


	std::string GbaPpuReport()
	{
		if (debugGba == nullptr)
			return "";

		const Ppu& ppu = debugGba->Lcd();
		const GbaBus& bus = debugGba->Bus();

		static const char* modeNames[] = { "0 (text)", "1 (text + affine)", "2 (affine)", "3 (bitmap 16-bit)",
			"4 (bitmap 8-bit, double buffered)", "5 (bitmap 16-bit, 160x128)" };

		uint16_t dispcnt = ppu.DispCnt();
		unsigned mode = dispcnt & 7;

		std::string md = "# GBA LCD (PPU)\n";

		MdSection(md, "Registers");
		MdTable2(md);
		MdRow2(md, "DISPCNT", dispcnt, "DISPSTAT", ppu.DispStat());
		MdRow2(md, "VCOUNT", ppu.VCount(), "BG0CNT", bus.Peek16(0x04000008));
		MdRow2(md, "BG1CNT", bus.Peek16(0x0400000A), "BG2CNT", bus.Peek16(0x0400000C));
		MdRow2(md, "BG3CNT", bus.Peek16(0x0400000E), "BG0HOFS", bus.Peek16(0x04000010));
		MdRow2(md, "BG0VOFS", bus.Peek16(0x04000012), "MOSAIC", bus.Peek16(0x0400004C));
		MdRow2(md, "WIN0H", bus.Peek16(0x04000040), "WIN0V", bus.Peek16(0x04000044));
		MdRow2(md, "WININ", bus.Peek16(0x04000048), "WINOUT", bus.Peek16(0x0400004A));
		MdRow2(md, "BLDCNT", bus.Peek16(0x04000050), "BLDALPHA", bus.Peek16(0x04000052));
		MdRow2(md, "BLDY", bus.Peek16(0x04000054), "BLDXY", bus.Peek16(0x04000054));

		MdSection(md, "Decoded");
		MdBullet(md, "mode: **%s**", mode < 6 ? modeNames[mode] : "?");
		MdBullet(md, "frame: **%i**, VISIBLE lines: **160**, forced blank: **%s**",
			ppu.FrameCounter(), YesNo(ppu.ForcedBlank()));
		MdBullet(md, "layers: BG0 %s, BG1 %s, BG2 %s, BG3 %s",
			YesNo((dispcnt & 0x0100) != 0), YesNo((dispcnt & 0x0200) != 0),
			YesNo((dispcnt & 0x0400) != 0), YesNo((dispcnt & 0x0800) != 0));
		MdBullet(md, "objects: **%s**, object window: **%s**, windows 0/1: %s / %s",
			YesNo((dispcnt & 0x1000) != 0), YesNo((dispcnt & 0x8000) != 0),
			YesNo((dispcnt & 0x2000) != 0), YesNo((dispcnt & 0x4000) != 0));
		MdBullet(md, "background 2/3 uses: **%s**",
			(mode == 0) ? "text" : ((mode == 1 || mode == 2) ? "affine" : "n/a"));

		return md;
	}


	std::string GbaDmaReport()
	{
		if (debugGba == nullptr)
			return "";

		const Dma& dma = debugGba->Bus().dma;

		static const char* timingNames[] = { "immediate", "VBlank", "HBlank", "special" };

		std::string md = "# GBA DMA\n";

		MdSection(md, "Channels");
		MdTable2(md);

		for (int i = 0; i < 4; i++)
		{
			const Dma::Channel& channel = dma.Get(i);
			char name0[0x10], name1[0x10];
			sprintf(name0, "DMA%i SAD", i);
			sprintf(name1, "DMA%i DAD", i);
			MdRow2(md, name0, channel.source, name1, channel.dest);

			char name2[0x10], name3[0x10];
			sprintf(name2, "DMA%i CNT_L", i);
			sprintf(name3, "DMA%i CNT_H", i);
			MdRow2(md, name2, channel.count, name3, channel.control);
		}

		MdSection(md, "Decoded");
		for (int i = 0; i < 4; i++)
		{
			const Dma::Channel& channel = dma.Get(i);
			unsigned timing = (channel.control >> 12) & 3;

			MdBullet(md, "DMA%i: %s, start timing **%s**, %s",
				i,
				channel.active ? (channel.pending ? "active (waiting for its slice)" : "active") : "idle",
				timingNames[timing],
				(channel.control & 0x0400) ? "32-bit" : "16-bit");
			MdBullet(md, "DMA%i: words left **%i**, source step %s, destination step %s",
				i, channel.latched,
				((channel.control >> 7) & 3) == 3 ? "decrement" : (((channel.control >> 7) & 3) == 0 ? "increment" : "fixed"),
				((channel.control >> 5) & 3) == 3 ? "decrement" : (((channel.control >> 5) & 3) == 0 ? "increment" : "fixed"));
		}

		return md;
	}


	std::string GbaTimersReport()
	{
		if (debugGba == nullptr)
			return "";

		const GbaBus& bus = debugGba->Bus();
		const Timers& timers = bus.timers;

		std::string md = "# GBA Timers and Interrupts\n";

		MdSection(md, "Timers");
		MdTable2(md);

		for (int i = 0; i < 4; i++)
		{
			char name0[0x10], name1[0x10];
			sprintf(name0, "TM%iCNT_L", i);
			sprintf(name1, "TM%iCNT_H", i);
			MdRow2(md, name0, timers.Read16((u32)(0x100 + i * 4)), name1, timers.Read16((u32)(0x102 + i * 4)));
		}

		MdSection(md, "Decoded");
		for (int i = 0; i < 4; i++)
		{
			uint16_t control = timers.Read16((u32)(0x102 + i * 4));

			MdBullet(md, "TM%i: counter **0x%04X**, %s, prescaler **%u**, cascade: **%s**, IRQ: **%s**",
				i, timers.Counter(i), timers.Running(i) ? "running" : "stopped",
				1u << Timers::PrescaleShift(control),
				YesNo((control & 0x04) != 0),
				YesNo((control & 0x40) != 0 && (control & 0x80) != 0));
		}

		MdSection(md, "Interrupt controller");
		MdTable2(md);
		MdRow2(md, "IE", bus.irq.ReadIE(), "IF", bus.irq.ReadIF());
		MdRow1(md, "IME", bus.irq.ReadIME() ? 1 : 0);
		MdBullet(md, "IRQ line: **%s**", bus.irq.Pending() ? "asserted" : "low");

		return md;
	}


	std::string GbaSioReport()
	{
		if (debugGba == nullptr)
			return "";

		const GbaBus& bus = debugGba->Bus();
		const Sio& sio = bus.sio;

		std::string md = "# GBA Serial I/O (link port)\n";

		MdSection(md, "Registers");
		MdTable2(md);
		MdRow2(md, "SIODATA32_L", bus.Peek16(0x04000120), "SIODATA32_H", bus.Peek16(0x04000122));
		MdRow2(md, "SIOCNT", bus.Peek16(0x04000128), "SIODATA8", bus.Peek16(0x0400012A));
		MdRow2(md, "RCNT", bus.Peek16(0x04000134), "JOYCNT", bus.Peek16(0x04000140));
		MdRow2(md, "JOYSTAT", bus.Peek16(0x04000158), "JOY_RECV", bus.Peek16(0x04000150));
		MdRow2(md, "JOY_TRANS", bus.Peek16(0x04000154), "SIOCNT (baud)", bus.Peek16(0x04000128) & 3);

		MdSection(md, "State");
		MdBullet(md, "transfer: **%s**", sio.Busy() ? "running" : "idle");
		MdBullet(md, "peers on the cable: **%i**", sio.ConnectedPlayers());
		MdBullet(md, "cable: **%s**", sio.Peer() != nullptr ? "attached" : "none");
		MdBullet(md, "last transfer: sent **0x%04X**, received **0x%04X**",
			(unsigned)sio.LastSent(), (unsigned)sio.LastReceived());
		MdBullet(md, "mode: **%s%s%s**",
			(bus.Peek16(0x04000134) & 0x8000) ? "JOY bus" : "normal",
			(bus.Peek16(0x04000128) & 0x2000) ? ", IRQ" : "",
			(bus.Peek16(0x04000128) & 0x4000) ? ", 32-bit" : ", 8-bit");

		return md;
	}


	std::string GbaCartReport()
	{
		if (debugGba == nullptr)
			return "";

		const Cart& cart = debugGba->Bus().cart;

		std::string md = "# GBA Cartridge\n";

		MdSection(md, "Header");
		MdBullet(md, "title: **%s**", cart.Title().empty() ? "(none)" : cart.Title().c_str());
		MdBullet(md, "game code: **%s**", cart.GameCode().empty() ? "(none)" : cart.GameCode().c_str());
		MdBullet(md, "ROM: **%llu** bytes, save: **%s**%s",
			(unsigned long long)cart.RomSize(), cart.SaveTypeName(),
			cart.SaveDirty() ? " (modified, not written yet)" : "");
		MdBullet(md, "save file: **%s**", cart.SaveFilePath().empty() ? "(none)" : cart.SaveFilePath().c_str());
		MdBullet(md, "RTC port: **%s**, EEPROM busy: **%s**, flash ID mode: **%s**",
			YesNo(cart.HasRtc()), YesNo(cart.EepromBusy()), YesNo(cart.FlashReadId()));

		MdSection(md, "Header bytes");
		MdHexDump(md, debugGba->Bus(), 0x08000000, 0xC0, 16, 12);

		return md;
	}


	// ========================================================================================
	// Game Boy - the machine
	// ========================================================================================

	static const u16 GbHighRam = 0xFF80;		// HRAM, which Peek answers for the whole region

	std::string GbMachineReport()
	{
		if (debugGb == nullptr)
			return "";

		GbSystem& system = *debugGb;
		const GbBus& bus = system.Bus();

		std::string md = "# Game Boy\n";

		MdSection(md, "Machine");
		MdBullet(md, "%s", system.Describe().c_str());
		MdBullet(md, "console: **%s**, double speed: **%s**",
			system.Cgb() ? "CGB" : "DMG", YesNo(bus.DoubleSpeed()));
		MdBullet(md, "frame: **%i**, cycles: **%llu** (system clock **%i** Hz)",
			system.FrameCounter(), (unsigned long long)bus.TotalCycles(), bus.ClockSpeed());
		MdBullet(md, "serial: **%s**, transfers: **%i**",
			bus.SerialActive() ? "running" : "idle", bus.SerialTransfers());
		MdBullet(md, "cartridge: **%s**, mapper: **%s**",
			system.RomTitle().empty() ? "(none)" : system.RomTitle().c_str(),
			GbMapperName(system.Cartridge().Header().mapper));

		MdSection(md, "Interrupts");
		MdTable2(md);
		MdRow2(md, "IE", bus.Ie(), "IF", bus.If());
		MdBullet(md, "pending: **%s**", bus.Pending() ? "yes" : "no");

		MdSection(md, "CPU");
		MdBullet(md, "halted: **%s**, stopped: **%s**",
			YesNo(system.Cpu().halted), YesNo(system.Cpu().stopped));

		return md;
	}


	std::string GbRegsReport()
	{
		if (debugGb == nullptr)
			return "";

		const GbCpu& cpu = debugGb->Cpu();

		std::string md = "# LR35902 Registers\n";

		MdSection(md, "Mode");
		MdBullet(md, "interrupts: **%s**, halted: **%s**, stopped: **%s**",
			cpu.ime ? "enabled" : "disabled", YesNo(cpu.halted), YesNo(cpu.stopped));
		MdBullet(md, "flags: **%s%s%s%s**",
			cpu.Flag(GbFlagZ) ? "Z" : "-", cpu.Flag(GbFlagN) ? "N" : "-",
			cpu.Flag(GbFlagH) ? "H" : "-", cpu.Flag(GbFlagC) ? "C" : "-");

		MdSection(md, "Registers");
		MdTable2(md);
		MdRow2(md, "AF", cpu.AF(), "BC", cpu.BC());
		MdRow2(md, "DE", cpu.DE(), "HL", cpu.HL());
		MdRow2(md, "SP", cpu.sp, "PC", cpu.pc);

		return md;
	}


	std::string GbDisasmReport(size_t count)
	{
		if (debugGb == nullptr)
			return "";

		if (count == 0)
			count = 16;
		if (count > MaxDisasmLines)
			count = MaxDisasmLines;

		const GbBus& bus = debugGb->Bus();

		// The SM83 disassembler asks for bytes; the bus answers them without a side effect.
		class GbLiveMemory : public DisasmMemory
		{
			const GbBus& bus;

		public:
			explicit GbLiveMemory(const GbBus& bus) : bus(bus) {}
			u16 Read16(u32 address) const override
			{
				return (u16)(bus.Peek((u16)address) | (bus.Peek((u16)(address + 1)) << 8));
			}
			u8 Read8(u32 address) const override { return bus.Peek((u16)address); }
		};

		GbLiveMemory memory(bus);

		u16 address = debugGb->Cpu().pc;
		std::string text;

		for (size_t i = 0; i < count; i++)
		{
			int size = 1;

			std::string mnemonic = GbDisassemble(memory, address, &size);
			if (size <= 0)
				size = 1;

			std::string bytes = GbInstructionBytes(memory, address, size);

			char line[0x100];
			sprintf(line, "%04X  %-8s %s\n", address, bytes.c_str(), mnemonic.c_str());
			text += line;

			address = (u16)(address + size);
		}

		char header[0x100];
		sprintf(header, "### PC = 0x%04X\n\n", debugGb->Cpu().pc);

		return std::string(header) + "```\n" + text + "```\n";
	}


	std::string GbMemReport(uint16_t address, size_t lines)
	{
		if (debugGb == nullptr)
			return "";

		if (lines == 0)
			lines = 16;

		char header[0x100];
		sprintf(header, "# Game Boy Memory\n\n`0x%04X` .. `0x%04X`\n\n",
			address, (uint16_t)(address + lines * 16 - 1));

		std::string md = header;
		MdHexDump(md, debugGb->Bus(), address, lines * 16);

		return md;
	}


	std::string GbPpuReport()
	{
		if (debugGb == nullptr)
			return "";

		const GbPpu& ppu = debugGb->Lcd();
		const GbBus& bus = debugGb->Bus();

		uint8_t lcdc = ppu.Lcdc();

		std::string md = "# Game Boy LCD (PPU)\n";

		MdSection(md, "Registers");
		MdTable2(md);
		MdRow2(md, "LCDC", lcdc, "STAT", ppu.Stat());
		MdRow2(md, "LY", ppu.Ly(), "SCY", bus.Peek(0xFF42));
		MdRow2(md, "SCX", bus.Peek(0xFF43), "WY", bus.Peek(0xFF4A));
		MdRow1(md, "WX", bus.Peek(0xFF4B));

		MdSection(md, "Palettes");
		MdTable2(md);
		MdRow2(md, "BGP", bus.Peek(0xFF47), "OBP0", bus.Peek(0xFF48));
		MdRow1(md, "OBP1", bus.Peek(0xFF49));

		MdSection(md, "Decoded");
		MdBullet(md, "LCD: **%s**, mode: **%i**, line: **%i**",
			YesNo(ppu.LcdEnabled()), ppu.Mode(), ppu.Ly());
		MdBullet(md, "frame: **%i**", ppu.FrameCounter());
		MdBullet(md, "background: **%s**, window: **%s**, objects: **%s**",
			YesNo((lcdc & 0x01) != 0), YesNo((lcdc & 0x20) != 0), YesNo((lcdc & 0x02) != 0));
		MdBullet(md, "tile data: **0x%04X**, background map: **0x%04X**",
			(lcdc & 0x10) ? 0x8000 : 0x8800, (lcdc & 0x08) ? 0x9C00 : 0x9800);
		MdBullet(md, "sprites: **%s**, window layer: **%s**",
			(lcdc & 0x04) ? "8x16" : "8x8", YesNo(ppu.WindowActive()));
		MdBullet(md, "CGB: **%s**, VRAM bank: **%i**, WRAM bank: **%i**",
			YesNo(ppu.Cgb()), bus.Vbk() & 1, bus.Svbk() & 7);

		return md;
	}


	// ========================================================================================
	// The commands
	// ========================================================================================

	static Json::Value* MarkdownAnswer(const std::string& markdown, const char* command)
	{
		if (markdown.empty())
		{
			Debug::Report(Debug::Channel::Norm, "%s: no machine is running\n", command);
			return nullptr;
		}

		Json::Value* output = new Json::Value();
		output->type = Json::ValueType::Object;
		output->AddUtf8String("markdown", markdown.c_str());
		return output;
	}

	static Json::Value* CmdGba(std::vector<std::string>& args)
	{
		return MarkdownAnswer(GbaMachineReport(), "gba");
	}

	static Json::Value* CmdGbaRegs(std::vector<std::string>& args)
	{
		return MarkdownAnswer(GbaRegsReport(), "gbaregs");
	}

	static Json::Value* CmdGbaCpu(std::vector<std::string>& args)
	{
		size_t count = (args.size() > 1) ? ClampCount(args[1].c_str(), 16, MaxDisasmLines) : 16;
		return MarkdownAnswer(GbaDisasmReport(count), "gbacpu");
	}

	static Json::Value* CmdGbaMem(std::vector<std::string>& args)
	{
		uint32_t address = (args.size() > 1) ? (uint32_t)strtoul(args[1].c_str(), nullptr, 0) : 0x08000000;
		size_t lines = (args.size() > 2) ? ClampLines(args[2].c_str(), 16) : 16;
		return MarkdownAnswer(GbaMemReport(address, lines), "gbamem");
	}

	static Json::Value* CmdGbaPpu(std::vector<std::string>& args)
	{
		return MarkdownAnswer(GbaPpuReport(), "gbappu");
	}

	static Json::Value* CmdGbaDma(std::vector<std::string>& args)
	{
		return MarkdownAnswer(GbaDmaReport(), "gbadma");
	}

	static Json::Value* CmdGbaTimers(std::vector<std::string>& args)
	{
		return MarkdownAnswer(GbaTimersReport(), "gbtimers");
	}

	static Json::Value* CmdGbaSio(std::vector<std::string>& args)
	{
		return MarkdownAnswer(GbaSioReport(), "gbsio");
	}

	static Json::Value* CmdGbaCart(std::vector<std::string>& args)
	{
		return MarkdownAnswer(GbaCartReport(), "gbcart");
	}

	static Json::Value* CmdGb(std::vector<std::string>& args)
	{
		return MarkdownAnswer(GbMachineReport(), "gb");
	}

	static Json::Value* CmdGbRegs(std::vector<std::string>& args)
	{
		return MarkdownAnswer(GbRegsReport(), "gbregs");
	}

	static Json::Value* CmdGbCpu(std::vector<std::string>& args)
	{
		size_t count = (args.size() > 1) ? ClampCount(args[1].c_str(), 16, MaxDisasmLines) : 16;
		return MarkdownAnswer(GbDisasmReport(count), "gbcpu");
	}

	static Json::Value* CmdGbMem(std::vector<std::string>& args)
	{
		uint16_t address = (args.size() > 1) ? (uint16_t)strtoul(args[1].c_str(), nullptr, 0) : 0x0000;
		size_t lines = (args.size() > 2) ? ClampLines(args[2].c_str(), 16) : 16;
		return MarkdownAnswer(GbMemReport(address, lines), "gbmem");
	}

	static Json::Value* CmdGbPpu(std::vector<std::string>& args)
	{
		return MarkdownAnswer(GbPpuReport(), "gbppu");
	}

	void DebugReflector()
	{
		JDI::Hub.AddCmd("gba", CmdGba);
		JDI::Hub.AddCmd("gbaregs", CmdGbaRegs);
		JDI::Hub.AddCmd("gbacpu", CmdGbaCpu);
		JDI::Hub.AddCmd("gbamem", CmdGbaMem);
		JDI::Hub.AddCmd("gbappu", CmdGbaPpu);
		JDI::Hub.AddCmd("gbadma", CmdGbaDma);
		JDI::Hub.AddCmd("gbtimers", CmdGbaTimers);
		JDI::Hub.AddCmd("gbsio", CmdGbaSio);
		JDI::Hub.AddCmd("gbcart", CmdGbaCart);
		JDI::Hub.AddCmd("gb", CmdGb);
		JDI::Hub.AddCmd("gbregs", CmdGbRegs);
		JDI::Hub.AddCmd("gbcpu", CmdGbCpu);
		JDI::Hub.AddCmd("gbmem", CmdGbMem);
		JDI::Hub.AddCmd("gbppu", CmdGbPpu);
	}


	// ========================================================================================
	// What the SDL frontend of the portable machines drives
	// ========================================================================================

	// The JDI node is registered on the first start and stays registered: the commands answer
	// "no machine is running" when the debugger is closed, and a second registration (which is
	// what a `--gba` run that opens and closes the debugger several times would do) would publish
	// the same names twice.
	static void RegisterNode()
	{
		static bool registered = false;

		if (registered)
			return;

		registered = true;
		JDI::Hub.AddNode(L"GBA_JDI_JSON", JdiSpecs::GbaJdi, DebugReflector);
	}

	void DebugStart()
	{
		if (!DebugMachineActive())
			return;

		RegisterNode();

		// `--mcp` starts the local MCP server in the GameCube front end, and the portable
		// machines are a front end of their own: without this, an MCP client that started
		// `pureikyubu --gba` would find the GameCube commands (registered by the emulator's
		// constructor, which this mode does run) and none of the portable ones.
		if (cmdline.mcp)
			Mcp::StartTransport();

		Debug2::StartDebugger();
	}

	void DebugStop()
	{
		Debug2::StopDebugger();

		// The machine belongs to the frontend that is closing (the caller's `GbaSystem` /
		// `GbSystem`), so it is forgotten here. A report asked for afterwards then says "no
		// machine is running" instead of walking a machine that is about to go away.
		debugGba = nullptr;
		debugGb = nullptr;
	}

	bool DebugActive()
	{
		return Debug2::IsDebuggerActive();
	}

	void DebugPumpEvents()
	{
		Debug2::UiPumpSdlEvents();
	}

	void DebugFrame()
	{
		Debug2::Frame();
	}

}
