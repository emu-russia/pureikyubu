// The GBA core test runner and the ROM harness.
//
// Two jobs in one binary:
//
//   * `gba_test [suite[.name]]` runs the registered unit tests (the same sources the emulator is
//     built from, see testing/Readme.md);
//   * `gba_test --run <rom> [--frames N] [--png <dir>] [--bench] ...` is the harness that made
//     the emulator debuggable while it was written: it boots a ROM headlessly, can dump frames as
//     PNG, prints a hash of every frame so a rendering change is visible without a window, and
//     measures the emulation speed. `--bootrom` runs the custom boot ROM alone, `--link-test`
//     plugs two instances into each other, and `--dump-bootrom` writes the generated boot ROM and
//     its listing out for review.

#include "gba_test.h"
#include "png.h"
#include "demo_rom.h"

#include "gba.h"
#include "gba_bootrom.h"
#include "gba_disasm.h"
#include "gb.h"
#include "gb_bus.h"
#include "gb_disasm.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <system_error>

using namespace GBA;

namespace GbaTest
{
	std::vector<TestCase>& Registry()
	{
		static std::vector<TestCase> registry;
		return registry;
	}

	int& FailureCount()
	{
		static int failures = 0;
		return failures;
	}

	static int notes = 0;

	void Note(const std::string& message)
	{
		printf("      %s\n", message.c_str());
		notes++;
	}

	void Fail(const char* file, int line, const std::string& message)
	{
		throw Failure{ std::string(file) + ":" + std::to_string(line) + ": " + message };
	}
}

namespace
{
	// ---------------------------------------------------------------------------------------
	// Small helpers
	// ---------------------------------------------------------------------------------------

	std::string BaseName(const std::string& path)
	{
		size_t slash = path.find_last_of("/\\");
		return (slash == std::string::npos) ? path : path.substr(slash + 1);
	}

	uint32_t FrameHash(const uint32_t* pixels, int count)
	{
		uint32_t hash = 2166136261u;
		for (int i = 0; i < count; i++)
		{
			uint32_t p = pixels[i];
			for (int b = 0; b < 4; b++)
			{
				hash ^= (p >> (b * 8)) & 0xFF;
				hash *= 16777619u;
			}
		}
		return hash;
	}

	bool MakeDirectory(const std::string& path)
	{
		// The harness runs on Linux (build.sh) and on Windows (the gba_bench project of the VS
		// solution), so the directory is created through the standard library instead of mkdir():
		// create_directories also reports success for a directory that is already there.
		std::error_code error;
		std::filesystem::create_directories(path, error);
		return !error;
	}

	// ---------------------------------------------------------------------------------------
	// The unit test runner
	// ---------------------------------------------------------------------------------------

	int RunTests(const std::string& filter)
	{
		auto& registry = GbaTest::Registry();
		int passed = 0;
		int failed = 0;
		int skipped = 0;

		std::string currentSuite;

		for (const auto& test : registry)
		{
			std::string full = std::string(test.suite) + "." + test.name;

			if (!filter.empty() && full.find(filter) == std::string::npos)
			{
				skipped++;
				continue;
			}

			if (currentSuite != test.suite)
			{
				currentSuite = test.suite;
				printf("\n[%s]\n", currentSuite.c_str());
			}

			GbaTest::FailureCount() = 0;
			int before = GbaTest::FailureCount();

			try
			{
				test.fn();
				(void)before;
				printf("  ok   %s\n", test.name);
				passed++;
			}
			catch (const GbaTest::Failure& failure)
			{
				printf("  FAIL %s\n       %s\n", test.name, failure.message.c_str());
				failed++;
			}
			catch (const std::exception& e)
			{
				printf("  FAIL %s\n       unexpected exception: %s\n", test.name, e.what());
				failed++;
			}
			catch (...)
			{
				printf("  FAIL %s\n       unexpected exception\n", test.name);
				failed++;
			}
		}

		printf("\n%i passed, %i failed", passed, failed);
		if (skipped != 0)
			printf(", %i filtered out", skipped);
		printf("\n");

		return failed;
	}

	// ---------------------------------------------------------------------------------------
	// A minimal WAV writer, so a run can be listened to afterwards (`--wav <file>`). The audio the
	// emulator mixes is 16-bit stereo at the system's sample rate; the header is written first
	// with placeholder sizes and patched when the file is closed.
	// ---------------------------------------------------------------------------------------

	class WavWriter
	{
		FILE* file = nullptr;
		uint32_t dataBytes = 0;

	public:
		bool Open(const std::string& path, int sampleRate)
		{
			file = fopen(path.c_str(), "wb");
			if (file == nullptr)
				return false;

			uint8_t header[44] = { 0 };
			memcpy(header + 0, "RIFF", 4);
			memcpy(header + 8, "WAVEfmt ", 8);
			uint32_t chunkSize = 16;
			uint16_t format = 1, channels = 2, bits = 16;
			uint32_t byteRate = (uint32_t)sampleRate * 2 * 2;
			uint16_t blockAlign = 4;
			memcpy(header + 16, &chunkSize, 4);
			memcpy(header + 20, &format, 2);
			memcpy(header + 22, &channels, 2);
			memcpy(header + 24, &sampleRate, 4);
			memcpy(header + 28, &byteRate, 4);
			memcpy(header + 32, &blockAlign, 2);
			memcpy(header + 34, &bits, 2);
			memcpy(header + 36, "data", 4);
			fwrite(header, 1, sizeof(header), file);
			return true;
		}

		void Write(const s16* samples, int frames)
		{
			if (file == nullptr || frames <= 0)
				return;

			size_t bytes = (size_t)frames * 2 * sizeof(s16);
			fwrite(samples, 1, bytes, file);
			dataBytes += (uint32_t)bytes;
		}

		void Close()
		{
			if (file == nullptr)
				return;

			uint32_t riffSize = 36 + dataBytes;
			fseek(file, 4, SEEK_SET);
			fwrite(&riffSize, 1, 4, file);
			fseek(file, 40, SEEK_SET);
			fwrite(&dataBytes, 1, 4, file);
			fclose(file);
			file = nullptr;
		}

		~WavWriter() { Close(); }
	};

	// ---------------------------------------------------------------------------------------
	// The ROM harness
	// ---------------------------------------------------------------------------------------

	struct HarnessOptions
	{
		std::string rom;
		std::string bios;
		std::string pngDir;
		std::string dumpBootRom;
		std::string wavPath;
		std::string disasmFile;		// --disasm-arm/--disasm-thumb/--disasm-gb
		std::string disasmKind;		// "arm", "thumb" or "gb"
		u32 disasmOffset = 0;
		int disasmCount = 0;
		int trace = 0;				// --trace: how many instructions of the last frame to keep
		int frames = 60;
		int pngEvery = 0;
		bool bootRomOnly = false;
		bool demo = false;
		bool noCustomBoot = false;
		bool bench = false;
		bool linkTest = false;
		bool quiet = false;
		bool gb = false;			// run the Game Boy machine instead of the GBA
		bool gbDmg = false;			// force the monochrome console
		u16 keys = 0;
	};

	/// <summary>A disassembly memory that reads through a live machine's bus, so the listing shows
	/// what the CPU would fetch rather than what a file holds.</summary>
	class BusDisasmMemory : public DisasmMemory
	{
	public:
		explicit BusDisasmMemory(GbaBus& bus) : bus(bus) {}

		u16 Read16(u32 address) const override
		{
			return (u16)(bus.Read16(address) & 0xFFFF);
		}

	private:
		GbaBus& bus;
	};

	/// <summary>The Game Boy's bus as a byte stream for its disassembler.</summary>
	class GbBusDisasmMemory : public DisasmMemory
	{
	public:
		explicit GbBusDisasmMemory(GbBus& bus) : bus(bus) {}

		u16 Read16(u32 address) const override
		{
			u8 low = bus.ReadByte((u16)address);
			u8 high = bus.ReadByte((u16)(address + 1));
			return (u16)(low | (high << 8));
		}

	private:
		GbBus& bus;
	};

	/// <summary>List an image (a BIOS or a cartridge file) or a live machine's memory, one
	/// instruction per line, with the address and the raw bytes in front of the mnemonic.</summary>
	int RunDisassembly(const HarnessOptions& options)
	{
		// The file is read into a flat image; the offsets are given relative to its base, which is
		// where the machine maps it (a BIOS at 0, a cartridge at 0x08000000).
		std::vector<u8> image;
		u32 base = 0;

		FILE* file = fopen(options.disasmFile.c_str(), "rb");
		if (file == nullptr)
		{
			printf("harness: cannot open %s\n", options.disasmFile.c_str());
			return 2;
		}

		u8 buffer[65536];
		size_t got;
		while ((got = fread(buffer, 1, sizeof buffer, file)) > 0)
			image.insert(image.end(), buffer, buffer + got);
		fclose(file);

		ImageMemory memory(image.data(), image.size(), base);
		bool thumb = options.disasmKind == "thumb";
		bool gb = options.disasmKind == "gb";

		printf("harness: %s, %zu bytes, listing %i instruction(s) from 0x%X as %s\n",
			options.disasmFile.c_str(), image.size(), options.disasmCount, options.disasmOffset,
			gb ? "SM83" : (thumb ? "Thumb" : "ARM"));

		u32 address = base + options.disasmOffset;

		for (int i = 0; i < options.disasmCount; i++)
		{
			int size = 0;
			std::string text;
			std::string bytes;

			if (gb)
			{
				text = GbDisassemble(memory, (u16)address, &size);
				bytes = GbInstructionBytes(memory, (u16)address, size);
				printf("  %04X: %-8s %s\n", (unsigned)address, bytes.c_str(), text.c_str());
			}
			else
			{
				text = Disassemble(memory, address, thumb, &size);
				bytes = InstructionBytes(memory, address, size);
				printf("  %08X: %-10s %s\n", (unsigned)address, bytes.c_str(), text.c_str());
			}

			address += (u32)size;
		}

		return 0;
	}

	/// <summary>One traced instruction: where it was, what it was and - for the last few - what the
	/// registers held.</summary>
	struct TraceEntry
	{
		u32 pc = 0;
		bool thumb = false;
		bool halted = false;
		int size = 2;
		std::string text;
	};

	/// <summary>Run the last frame of the machine one instruction at a time, keeping the last
	/// `count` instructions, and print them. This is the tool that answers "what is it doing now"
	/// for a program that seems to be stuck.</summary>
	void TraceLastFrame(GbaSystem& system, int count)
	{
		BusDisasmMemory memory(system.Bus());
		std::vector<TraceEntry> trace;
		trace.reserve((size_t)count);

		int frame = system.Bus().ppu.FrameCounter();
		u64 guard = 0;
		const u64 maxCycles = (u64)CyclesPerFrame * 4;

		while (system.Bus().ppu.FrameCounter() == frame && guard < maxCycles)
		{
			TraceEntry entry;
			entry.pc = system.Cpu().CurrentPC();
			entry.thumb = system.Cpu().ThumbState();
			entry.halted = system.Cpu().Halted();
			entry.text = entry.halted ? std::string("halted (waiting for IE & IF)") :
				Disassemble(memory, entry.pc, entry.thumb, &entry.size);

			// A halted core is stepped (the bus still advances) but recorded once: otherwise a
			// program that waits out a whole frame fills the trace with one address.
			if (!entry.halted || trace.empty() || !trace.back().halted ||
				trace.back().pc != entry.pc)
				trace.push_back(entry);

			if ((int)trace.size() > count)
				trace.erase(trace.begin());

			int taken = system.Cpu().Step();
			if (taken < 1)
				taken = 1;
			system.Bus().Tick(taken);
			guard += (u64)taken;
		}

		printf("harness: the last frame ran %zu traced instructions (showing the last %zu)\n",
			trace.size(), trace.size());

		// The last few entries carry the register file as well: a trace that says "it halts here"
		// is much more useful with the arguments the program passed in.
		const size_t withRegisters = 12;
		size_t index = 0;

		for (const TraceEntry& entry : trace)
		{
			bool showRegisters = index + withRegisters >= trace.size();
			index++;

			if (!showRegisters)
			{
				printf("  %08X: %s\n", (unsigned)entry.pc, entry.text.c_str());
				continue;
			}

			printf("  %08X: %-28s r0=%08X r1=%08X r2=%08X r3=%08X\n", (unsigned)entry.pc,
				entry.text.c_str(), (unsigned)system.Cpu().Reg(0), (unsigned)system.Cpu().Reg(1),
				(unsigned)system.Cpu().Reg(2), (unsigned)system.Cpu().Reg(3));
		}

		printf("harness: cpu at %08X, cpsr %08X (%s), IE %04X IF %04X IME %i, DISPSTAT %04X\n",
			(unsigned)system.Cpu().CurrentPC(), (unsigned)system.Cpu().ReadCPSR(),
			ConditionFlags(system.Cpu().ReadCPSR()).c_str(),
			(unsigned)system.Bus().irq.ReadIE(), (unsigned)system.Bus().irq.ReadIF(),
			(int)system.Bus().irq.ReadIME(), (unsigned)system.Bus().ppu.DispStat());
	}

	int RunHarness(const HarnessOptions& options)
	{
		GbaSystem system;
		GbaSettings settings = GbaSettings::Defaults();
		settings.logLevel = options.quiet ? 0 : 3;
		if (options.noCustomBoot)
			settings.useCustomBootRom = false;
		system.ApplySettings(settings);

		if (!options.bios.empty())
		{
			std::string error;
			if (!system.LoadBiosFile(options.bios, error))
			{
				printf("harness: cannot load the BIOS: %s\n", error.c_str());
				return 2;
			}
		}

		if (!options.rom.empty())
		{
			std::string error;
			if (!system.LoadRomFile(options.rom, error))
			{
				printf("harness: cannot load the ROM: %s\n", error.c_str());
				return 2;
			}
		}
		else if (options.demo)
		{
			// The demo cartridge is assembled here, in memory, by the same emitter the boot ROM
			// uses: nothing has to be shipped with the repository for the harness to have
			// something to run. The demo synchronizes to the display itself, so the boot
			// animation is skipped.
			GbaSettings adjusted = system.Settings();
			adjusted.useCustomBootRom = false;
			system.ApplySettings(adjusted);

			std::string error;
			if (!system.LoadRomImage(GbaTest::BuildDemoRom(), error))
			{
				printf("harness: cannot load the demo cartridge: %s\n", error.c_str());
				return 2;
			}
		}

		system.Reset();
		system.SetPressedKeys(options.keys);

		printf("harness: %s\n", system.Describe().c_str());

		if (!options.pngDir.empty())
			MakeDirectory(options.pngDir);

		// `--wav <file>`: record what the sound hardware produces while the frames run, so a boot
		// animation or a game's music can be listened to afterwards.
		WavWriter wav;
		std::vector<s16> audio;

		if (!options.wavPath.empty())
		{
			if (!wav.Open(options.wavPath, system.SampleRate()))
			{
				printf("harness: cannot write %s\n", options.wavPath.c_str());
				return 2;
			}

			printf("harness: recording the audio to %s at %i Hz\n",
				options.wavPath.c_str(), system.SampleRate());
		}

		auto start = std::chrono::steady_clock::now();
		u64 startCycles = system.Cycles();

		for (int frame = 0; frame < options.frames; frame++)
		{
			// --trace steps the *last* frame one instruction at a time (and prints it afterwards),
			// which is what answers "what is this program doing right now"; the frames before it
			// run at full speed.
			if (options.trace > 0 && frame == options.frames - 1)
				TraceLastFrame(system, options.trace);
			else
				system.RunFrame();

			if (!options.wavPath.empty())
			{
				audio.resize(4096 * 2);
				int got = system.ReadAudio(audio.data(), 4096);
				wav.Write(audio.data(), got);
			}

			uint32_t hash = FrameHash(system.FrameBuffer(), ScreenWidth * ScreenHeight);

			if (!options.quiet && (options.pngEvery == 0 || (frame % options.pngEvery) == 0))
			{
				printf("  frame %5i  hash %08X%s\n", frame, hash,
					system.LinkMode() ? "  (link mode)" : "");
			}

			if (!options.pngDir.empty() && options.pngEvery > 0 && (frame % options.pngEvery) == 0)
			{
				char name[256];
				snprintf(name, sizeof(name), "%s/%s_frame%04i.png", options.pngDir.c_str(),
					BaseName(options.rom.empty() ? (options.demo ? "demo.gba" : "bootrom") : options.rom).c_str(), frame);
				if (!GbaTest::WritePng(name, system.FrameBuffer(), ScreenWidth, ScreenHeight, 2))
					printf("  (cannot write %s)\n", name);
			}
		}

		auto end = std::chrono::steady_clock::now();
		double seconds = std::chrono::duration<double>(end - start).count();
		u64 cycles = system.Cycles() - startCycles;

		wav.Close();

		if (!options.wavPath.empty())
		{
			printf("harness: wrote %s\n", options.wavPath.c_str());
		}

		if (options.bench && seconds > 0)
		{
			printf("harness: %i frames in %.2f s = %.1f fps, %.1f MHz emulated (%.2fx real time)\n",
				options.frames, seconds, options.frames / seconds,
				(cycles / seconds) / 1e6, (cycles / seconds) / (double)CyclesPerSecond);
		}

		std::string error;
		system.SaveBattery(&error);

		return 0;
	}

	// ---------------------------------------------------------------------------------------
	// The Game Boy harness (the same shape as the GBA one, for the other machine)
	// ---------------------------------------------------------------------------------------

	int RunGbHarness(const HarnessOptions& options)
	{
		GbSystem system;

		GbSettings settings = GbSettings::Defaults();
		settings.cgb = !options.gbDmg;
		settings.useBootRom = !options.noCustomBoot;
		settings.logLevel = options.quiet ? 0 : 4;
		system.ApplySettings(settings);

		std::string error;

		if (!options.rom.empty() && !system.LoadRomFile(options.rom, error))
		{
			printf("gb harness: cannot load the ROM: %s\n", error.c_str());
			return 2;
		}

		system.Reset();

		printf("gb harness: %s\n", system.Describe().c_str());

		if (!options.pngDir.empty())
			MakeDirectory(options.pngDir);

		auto start = std::chrono::steady_clock::now();
		u64 startCycles = system.Cycles();

		for (int frame = 0; frame < options.frames; frame++)
		{
			system.RunFrame();

			uint32_t hash = FrameHash(system.FrameBuffer(), GbScreenWidth * GbScreenHeight);

			if (!options.quiet && (options.pngEvery == 0 || (frame % options.pngEvery) == 0))
			{
				printf("  frame %5i  hash %08X\n", frame, hash);
			}

			if (!options.pngDir.empty() && options.pngEvery > 0 && (frame % options.pngEvery) == 0)
			{
				char name[256];
				snprintf(name, sizeof(name), "%s/%s_frame%04i.png", options.pngDir.c_str(),
					BaseName(options.rom.empty() ? "gb-bootrom" : options.rom).c_str(), frame);

				if (!GbaTest::WritePng(name, system.FrameBuffer(), GbScreenWidth, GbScreenHeight, 3))
					printf("  (cannot write %s)\n", name);
			}
		}

		auto end = std::chrono::steady_clock::now();
		double seconds = std::chrono::duration<double>(end - start).count();
		u64 cycles = system.Cycles() - startCycles;

		if (options.bench && seconds > 0)
		{
			printf("gb harness: %i frames in %.2f s = %.1f fps, %.2f MHz emulated (%.2fx real time)\n",
				options.frames, seconds, options.frames / seconds,
				(cycles / seconds) / 1e6, (cycles / seconds) / 4194304.0);
		}

		std::string saveError;
		system.SaveBattery(&saveError);

		return 0;
	}

	// ---------------------------------------------------------------------------------------
	// The link harness: two machines, one cable.
	// ---------------------------------------------------------------------------------------

	int RunLinkTest()
	{
		GbaSystem master, slave;

		GbaSettings settings = GbaSettings::Defaults();
		settings.logLevel = 3;

		master.ApplySettings(settings);
		slave.ApplySettings(settings);

		master.AttachLink(&slave);
		master.Reset();
		slave.Reset();

		printf("link: %s\n", master.Describe().c_str());
		printf("link: %s\n", slave.Describe().c_str());

		for (int frame = 0; frame < 10; frame++)
		{
			master.RunFrame();
			slave.RunFrame();
		}

		printf("link: master sent %04X, received %04X\n",
			master.Link().LastSent(), master.Link().LastReceived());
		printf("link: slave  sent %04X, received %04X\n",
			slave.Link().LastSent(), slave.Link().LastReceived());

		return 0;
	}

	// ---------------------------------------------------------------------------------------
	// The boot ROM dump
	// ---------------------------------------------------------------------------------------

	int DumpBootRom(const std::string& path)
	{
		const std::vector<u8>& image = BootRom::GbaImage();

		FILE* f = fopen(path.c_str(), "wb");
		if (f == nullptr)
		{
			printf("cannot write %s\n", path.c_str());
			return 2;
		}
		fwrite(image.data(), 1, image.size(), f);
		fclose(f);

		std::string listingPath = path + ".txt";
		FILE* l = fopen(listingPath.c_str(), "w");
		if (l != nullptr)
		{
			std::string listing = BootRom::GbaListing();
			fwrite(listing.data(), 1, listing.size(), l);
			fclose(l);
		}

		printf("boot rom: %zu bytes -> %s (%s)\n", image.size(), path.c_str(), listingPath.c_str());
		return 0;
	}

	void PrintUsage()
	{
		printf(
			"gba_test - the GBA core test runner and ROM harness\n"
			"\n"
			"  gba_test [suite[.name]]            run the unit tests (a substring filter)\n"
			"  gba_test --list                    list the registered tests\n"
			"  gba_test --run <rom> [options]     run a ROM headlessly\n"
			"  gba_test --bootrom [options]       run the custom boot ROM alone\n"
			"  gba_test --demo [options]          run the demo cartridge the harness assembles\n"
			"  gba_test --dump-bootrom <file>     write the generated boot ROM and its listing\n"
			"  gba_test --link-test               plug two instances into each other\n"
			"  gba_test --disasm-arm <file> <offset> <count>     list ARM instructions\n"
			"  gba_test --disasm-thumb <file> <offset> <count>   list Thumb instructions\n"
			"  gba_test --disasm-gb <file> <offset> <count>      list SM83 instructions\n"
			"\n"
			"options for --run/--bootrom/--demo:\n"
			"  --frames N        how many frames to run (default 60)\n"
			"  --png <dir>       dump frames as PNG into <dir>\n"
			"  --png-every N     dump every N-th frame (default: only with --png, every 10th)\n"
			"  --keys <mask>     hold the keys named by the bits (see gba_keypad.h)\n"
			"  --bios <file>     use a real BIOS image instead of the built-in one\n"
			"  --wav <file>      record what the sound hardware produces into a WAV file\n"
			"  --trace N         step the last frame instruction by instruction and print the last N\n"
			"                    of them (with the disassembly), for a program that seems stuck\n"
			"  --no-custom-boot  do not run the custom boot ROM\n"
			"  --gb              run the Game Boy machine instead of the GBA (with --gb-dmg for the\n"
			"                    monochrome console)\n"
			"  --bench           print the emulation speed\n"
			"  --quiet           only print the final summary\n");
	}
}

int main(int argc, char** argv)
{
	std::vector<std::string> args;
	for (int i = 1; i < argc; i++)
		args.push_back(argv[i]);

	if (args.empty())
		return RunTests("");

	if (args[0] == "--help" || args[0] == "-h")
	{
		PrintUsage();
		return 0;
	}

	if (args[0] == "--list")
	{
		for (const auto& test : GbaTest::Registry())
			printf("%s.%s\n", test.suite, test.name);
		return 0;
	}

	if (args[0] == "--dump-bootrom")
	{
		if (args.size() < 2)
		{
			printf("--dump-bootrom needs a file name\n");
			return 2;
		}
		return DumpBootRom(args[1]);
	}

	if (args[0] == "--link-test")
		return RunLinkTest();

	if (args[0] == "--disasm-arm" || args[0] == "--disasm-thumb" || args[0] == "--disasm-gb")
	{
		// --disasm-arm <file> <offset> <count>: list an instruction stream. The offsets are given
		// in the file's own numbering (a BIOS starts at 0, a cartridge at 0x08000000 is listed by
		// its file offset).
		if (args.size() < 4)
		{
			printf("%s needs a file, an offset and an instruction count\n", args[0].c_str());
			return 2;
		}

		HarnessOptions options;
		options.disasmFile = args[1];
		options.disasmOffset = (u32)strtoul(args[2].c_str(), nullptr, 0);
		options.disasmCount = atoi(args[3].c_str());
		options.disasmKind = (args[0] == "--disasm-arm") ? "arm" :
			((args[0] == "--disasm-thumb") ? "thumb" : "gb");
		return RunDisassembly(options);
	}

	if (args[0] == "--run" || args[0] == "--bootrom" || args[0] == "--demo")
	{
		HarnessOptions options;
		options.bootRomOnly = (args[0] == "--bootrom");
		options.demo = (args[0] == "--demo");

		for (size_t i = 1; i < args.size(); i++)
		{
			const std::string& arg = args[i];

			auto next = [&](const char* what) -> std::string
			{
				if (i + 1 >= args.size())
				{
					printf("%s needs a value\n", what);
					exit(2);
				}
				return args[++i];
			};

			if (arg == "--frames") options.frames = atoi(next("--frames").c_str());
			else if (arg == "--png") options.pngDir = next("--png");
			else if (arg == "--png-every") options.pngEvery = atoi(next("--png-every").c_str());
			else if (arg == "--keys") options.keys = (u16)strtoul(next("--keys").c_str(), nullptr, 0);
			else if (arg == "--bios") options.bios = next("--bios");
			else if (arg == "--wav") options.wavPath = next("--wav");
			else if (arg == "--demo") options.demo = true;
			else if (arg == "--gb") options.gb = true;
			else if (arg == "--gb-dmg") { options.gb = true; options.gbDmg = true; }
			else if (arg == "--no-custom-boot") options.noCustomBoot = true;
			else if (arg == "--bench") options.bench = true;
			else if (arg == "--trace") options.trace = atoi(next("--trace").c_str());
			else if (arg == "--quiet") options.quiet = true;
			else if (arg[0] != '-') options.rom = arg;
			else
			{
				printf("unknown option %s\n", arg.c_str());
				return 2;
			}
		}

		if (options.pngDir.empty())
			options.pngEvery = 0;
		else if (options.pngEvery == 0)
			options.pngEvery = 10;

		if (options.gb)
		{
			return RunGbHarness(options);
		}

		return RunHarness(options);
	}

	if (args[0][0] == '-')
	{
		PrintUsage();
		return 2;
	}

	return RunTests(args[0]);
}
