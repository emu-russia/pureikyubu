// Save states: the image, the round trip through a file, and the property that matters - a machine
// that is loaded back is the machine that was saved, instruction for instruction.
//
// The tests drive the whole machine (the demo cartridge of demo_rom.h, which paints and animates
// from a real ARM program), so a device whose state is not in the image shows up here as a
// divergence between the run that continued and the run that was resumed: the picture, the frame
// counter, the CPU's register file and the cartridge's memory are all compared. The strongest of
// them saves the machine, runs it on, loads it back and saves it again, and requires the two
// images to be byte for byte the same - which can only hold if every member that changed during
// the run was restored by the load.
//
// The format itself is checked separately: the magic, the version, the checksum, the length, the
// refusal of a corrupt or truncated file, and the refusal of a state taken from another cartridge
// (a state is a picture of a machine *running a particular program*; its memory holds that
// program's variables).

#include "gba_test.h"
#include "demo_rom.h"
#include "gb_asm.h"

#include "gba.h"
#include "gba_savestate.h"
#include "gb.h"

using namespace GBA;

namespace
{
	/// <summary>The FNV-1a hash of a frame buffer (the harness uses the same one).</summary>
	uint32_t HashPixels(const uint32_t* pixels, int count)
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
	/// <summary>A machine with the demo cartridge in it, run for `frames` frames from a cold start.
	/// The boot ROM is skipped: the demo does not need the animation and the tests are much
	/// shorter without it.</summary>
	void BuildDemoMachine(GbaSystem& system, int frames = 0)
	{
		GbaSettings settings = GbaSettings::Defaults();
		settings.useCustomBootRom = false;
		system.ApplySettings(settings);

		std::string error;
		if (!system.LoadRomImage(GbaTest::BuildDemoRom(), error))
		{
			GBA_FAIL("the demo cartridge did not load: " + error);
		}

		system.RunFrames(frames);
	}

	/// <summary>The same demo cartridge, but with a save library signature in it, so that the
	/// machine has battery-backed SRAM to write to (the plain demo is a read-only cartridge).
	/// The signature is the one Nintendo's linker leaves in a game that saves.</summary>
	void BuildDemoMachineWithSram(GbaSystem& system, int frames = 0)
	{
		std::vector<uint8_t> image = GbaTest::BuildDemoRom();

		const char* signature = "SRAM_V";
		for (int i = 0; i < 6; i++)
		{
			image[0x120 + i] = (uint8_t)signature[i];
		}

		GbaSettings settings = GbaSettings::Defaults();
		settings.useCustomBootRom = false;
		system.ApplySettings(settings);

		std::string error;
		if (!system.LoadRomImage(image, error))
		{
			GBA_FAIL("the demo cartridge did not load: " + error);
		}

		system.RunFrames(frames);
	}

	/// <summary>A machine with the tiny marker cartridge in it (a different title, a different
	/// size, and a program that does nothing but spin).</summary>
	void BuildMarkerMachine(GbaSystem& system)
	{
		GbaSettings settings = GbaSettings::Defaults();
		settings.useCustomBootRom = false;
		system.ApplySettings(settings);

		std::string error;
		if (!system.LoadRomImage(GbaTest::BuildMarkerRom(0x12345678), error))
		{
			GBA_FAIL("the marker cartridge did not load: " + error);
		}
	}

	/// <summary>The FNV-1a hash of the GBA's frame buffer.</summary>
	uint32_t FrameHash(const GbaSystem& system)
	{
		return HashPixels(system.FrameBuffer(), ScreenWidth * ScreenHeight);
	}

	/// <summary>The FNV-1a hash of the Game Boy's frame buffer.</summary>
	uint32_t FrameHash(const GbSystem& system)
	{
		return HashPixels(system.FrameBuffer(), GbScreenWidth * GbScreenHeight);
	}

	/// <summary>A hash of a run of the machine's memory, so that a divergence inside EWRAM or
	/// IWRAM is caught as well as one on the screen.</summary>
	uint32_t MemoryHash(const GbaSystem& system)
	{
		uint32_t hash = 2166136261u;

		for (uint32_t address = 0x02000000; address < 0x02000000 + EwramSize; address += 4)
		{
			uint32_t value = system.Bus().Peek16(address) | ((uint32_t)system.Bus().Peek16(address + 2) << 16);
			hash = (hash ^ value) * 16777619u;
		}

		for (uint32_t address = 0x03000000; address < 0x03000000 + IwramSize; address += 4)
		{
			uint32_t value = system.Bus().Peek16(address) | ((uint32_t)system.Bus().Peek16(address + 2) << 16);
			hash = (hash ^ value) * 16777619u;
		}

		return hash;
	}

	/// <summary>Where the tests write their state files. A relative name so that they work in a
	/// Windows working directory as well as in a Linux one (the module tests write to /tmp where
	/// they can, but the harness runs on both).</summary>
	const char* StateScratch = "gba_bench_savestate_test.st0";
}

// ---------------------------------------------------------------------------------------
// The image
// ---------------------------------------------------------------------------------------

GBA_TEST(SaveState, TheImageIsVersionedAndChecksummed)
{
	GbaSystem system;
	BuildDemoMachine(system, 20);

	std::vector<uint8_t> image;
	std::string error;
	GBA_CHECK_MSG(system.SaveState(image, &error), error);
	GBA_CHECK(error.empty());

	// The header: the magic, the version and the payload that follows it.
	for (int i = 0; i < 8; i++)
	{
		GBA_CHECK_EQ(image[i], StateMagic[i]);
	}

	GBA_CHECK_EQ(image.size() > StateHeaderSize, true);

	uint32_t version = (uint32_t)image[0x08] | ((uint32_t)image[0x09] << 8) |
		((uint32_t)image[0x0A] << 16) | ((uint32_t)image[0x0B] << 24);
	GBA_CHECK_HEX32(version, StateFormatVersion);

	// The declared payload length has to be the rest of the file, and the checksum has to be the
	// one the payload really hashes to.
	uint64_t length = 0;
	for (int i = 0; i < 8; i++)
	{
		length |= (uint64_t)image[0x10 + i] << (i * 8);
	}

	GBA_CHECK_EQ(length, (uint64_t)(image.size() - StateHeaderSize));

	uint64_t checksum = 0;
	for (int i = 0; i < 8; i++)
	{
		checksum |= (uint64_t)image[0x18 + i] << (i * 8);
	}

	GBA_CHECK_EQ(checksum, StateChecksum(image.data() + StateHeaderSize, (size_t)length));

	// A whole machine is not a handful of bytes: the memory alone (EWRAM 256K, IWRAM 32K, VRAM
	// 96K, the frame buffer, the cartridge's save memory) puts it far past a megabyte... which it
	// is not, quite - the point is only that nothing was accidentally left empty.
	GBA_CHECK_MSG(image.size() > 400 * 1024u,
		"a state of the whole machine should be several hundred KByte; this one is " +
		std::to_string(image.size()));
}

GBA_TEST(SaveState, ACorruptImageIsRefused)
{
	GbaSystem system;
	BuildDemoMachine(system, 20);

	std::vector<uint8_t> image;
	std::string error;
	GBA_CHECK(system.SaveState(image, &error));

	// One flipped bit in the payload (a byte of EWRAM, say) has to be caught by the checksum.
	std::vector<uint8_t> corrupt = image;
	corrupt[StateHeaderSize + 64] ^= 0x40;

	error.clear();
	GBA_CHECK_MSG(!system.LoadState(corrupt, &error), "a corrupt state must not load");
	GBA_CHECK_MSG(!error.empty(), "a refused state must say why");
	GBA_CHECK_MSG(error.find("checksum") != std::string::npos,
		"the message should name the checksum: " + error);

	// A truncated file is refused as well (the header says how long the payload is).
	std::vector<uint8_t> truncated(image.begin(), image.begin() + image.size() / 2);
	GBA_CHECK(!system.LoadState(truncated, &error));

	// So is something that is not a save state at all.
	std::vector<uint8_t> garbage(512, 0xAB);
	GBA_CHECK(!system.LoadState(garbage, &error));

	// And a state whose format version this build does not know.
	std::vector<uint8_t> future = image;
	future[0x08] = (uint8_t)(StateFormatVersion + 1);
	GBA_CHECK(!system.LoadState(future, &error));
	GBA_CHECK_MSG(error.find("version") != std::string::npos,
		"the message should name the version: " + error);
}

GBA_TEST(SaveState, AStateOfAnotherCartridgeIsRefused)
{
	GbaSystem system;
	BuildDemoMachine(system, 20);

	std::vector<uint8_t> image;
	std::string error;
	GBA_CHECK(system.SaveState(image, &error));

	// The same cartridge, another program: a state carries the variables and the program counter
	// of the game it was taken from, so it belongs to that game and to no other.
	GbaSystem other;
	BuildMarkerMachine(other);

	error.clear();
	GBA_CHECK_MSG(!other.LoadState(image, &error), "a state of another cartridge must not load");
	GBA_CHECK_MSG(error.find(system.RomTitle()) != std::string::npos,
		"the message should name the cartridge the state belongs to: " + error);
	GBA_CHECK_MSG(error.find(other.RomTitle()) != std::string::npos,
		"the message should name the machine it was offered to: " + error);

	// A machine with no cartridge at all refuses it too.
	GbaSystem empty;
	GbaSettings settings = GbaSettings::Defaults();
	settings.useCustomBootRom = false;
	empty.ApplySettings(settings);

	GBA_CHECK(!empty.LoadState(image, &error));
}

// ---------------------------------------------------------------------------------------
// The machine that comes back
// ---------------------------------------------------------------------------------------

GBA_TEST(SaveState, AMachineWithNoCartridgeHasStatesToo)
{
	// The emulator's own headline mode is a Game Boy Advance with nothing in its slot: the boot
	// ROM's link driver owns the port and serves the cable. That machine is a state like any
	// other - and a state taken with a cartridge in the slot is still refused by it.
	GbaSystem system;

	GbaSettings settings = GbaSettings::Defaults();
	settings.useCustomBootRom = false;			// no BIOS image at all: the CPU sits halted
	system.ApplySettings(settings);
	system.Reset();
	system.RunFrames(2);

	GBA_CHECK(!system.RomLoaded());

	std::vector<uint8_t> image;
	std::string error;
	GBA_CHECK_MSG(system.SaveState(image, &error), error);
	GBA_CHECK_MSG(system.LoadState(image, &error), error);

	GbaSystem withRom;
	BuildDemoMachine(withRom, 2);

	std::vector<uint8_t> romState;
	GBA_CHECK(withRom.SaveState(romState, &error));

	error.clear();
	GBA_CHECK_MSG(!system.LoadState(romState, &error),
		"a state of a cartridge must not load into a machine with no cartridge");
	GBA_CHECK_MSG(error.find("no cartridge") != std::string::npos,
		"the message should say the machine has no cartridge: " + error);
}

GBA_TEST(SaveState, TheMachineResumesAtTheSameFrame)
{
	GbaSystem system;
	BuildDemoMachine(system, 20);

	// Take the state, then run on: this is the run the resumed machine has to reproduce.
	std::vector<uint8_t> image;
	std::string error;
	GBA_CHECK(system.SaveState(image, &error));

	system.RunFrames(10);

	int expectedFrame = system.FrameCounter();
	uint32_t expectedPicture = FrameHash(system);
	uint32_t expectedMemory = MemoryHash(system);
	uint64_t expectedCycles = system.Cycles();

	// Put the machine back and run exactly the same ten frames again.
	GBA_CHECK_MSG(system.LoadState(image, &error), error);

	GBA_CHECK_EQ(system.FrameCounter(), 20);			// the frame the state was taken at

	system.RunFrames(10);

	GBA_CHECK_EQ(system.FrameCounter(), expectedFrame);
	GBA_CHECK_HEX32(FrameHash(system), expectedPicture);
	GBA_CHECK_HEX32(MemoryHash(system), expectedMemory);
	GBA_CHECK_EQ(system.Cycles(), expectedCycles);
}

GBA_TEST(SaveState, AStateTakenInsideAFrameResumesExactly)
{
	GbaSystem system;
	BuildDemoMachine(system, 20);

	// Half a frame past a frame boundary: the CPU is in the middle of repainting the screen, the
	// LCD is somewhere inside a scanline and the sound controller is part way through a sample
	// period. Nothing about the state may depend on being taken at a frame boundary.
	system.RunCycles(CyclesPerFrame / 2 + 1234);

	std::vector<uint8_t> image;
	std::string error;
	GBA_CHECK(system.SaveState(image, &error));

	// Run on by a whole number of frames and a piece of one, then reproduce it.
	system.RunFrames(4);
	system.RunCycles(97);

	int expectedFrame = system.FrameCounter();
	uint32_t expectedPicture = FrameHash(system);
	uint32_t expectedMemory = MemoryHash(system);
	uint64_t expectedCycles = system.Cycles();

	GBA_CHECK_MSG(system.LoadState(image, &error), error);

	system.RunFrames(4);
	system.RunCycles(97);

	GBA_CHECK_EQ(system.FrameCounter(), expectedFrame);
	GBA_CHECK_HEX32(FrameHash(system), expectedPicture);
	GBA_CHECK_HEX32(MemoryHash(system), expectedMemory);
	GBA_CHECK_EQ(system.Cycles(), expectedCycles);

	// The picture that is on the screen is part of the state too: a frontend presents the frame
	// buffer before it runs the next frame, so a load has to put the picture it was taken from
	// back rather than leaving the one the machine happened to be showing.
	GBA_CHECK_MSG(FrameHash(system) == expectedPicture, "the frame buffer is part of the state");
}

GBA_TEST(SaveState, SavingLoadingAndSavingAgainGivesTheSameImage)
{
	// The completeness test. Everything the machine has that a state is supposed to carry is
	// compared at once: the two images can only be identical if every member that the run changed
	// was written by SaveState *and* restored by LoadState. A device state that was forgotten is a
	// byte that differs here.
	GbaSystem system;
	BuildDemoMachine(system, 15);

	std::vector<uint8_t> first;
	std::string error;
	GBA_CHECK_MSG(system.SaveState(first, &error), error);

	// Run the machine on: the CPU, the memory, the LCD, the timers, the cartridge's memory and
	// its dirty flags all move.
	system.RunFrames(7);
	system.RunCycles(557);

	GBA_CHECK_MSG(system.LoadState(first, &error), error);

	std::vector<uint8_t> second;
	GBA_CHECK_MSG(system.SaveState(second, &error), error);

	GBA_CHECK_EQ(second.size(), first.size());

	size_t differences = 0;
	size_t firstDifference = 0;

	for (size_t i = 0; i < first.size() && i < second.size(); i++)
	{
		if (first[i] != second[i])
		{
			if (differences == 0)
			{
				firstDifference = i;
			}
			differences++;
		}
	}

	GBA_CHECK_MSG(differences == 0,
		"the image of the loaded machine differs from the one that was saved in " +
		std::to_string(differences) + " bytes, the first at offset " + GbaTest::Hex(firstDifference));
}

GBA_TEST(SaveState, TheBankedRegistersAndTheCpuModeComeBack)
{
	GbaSystem system;
	BuildDemoMachine(system, 5);

	Arm7tdmi& cpu = system.Cpu();

	// Put distinctive values in the banks of a mode the program is not in, so that a load which
	// only wrote the current window would be caught.
	cpu.SwitchMode(ModeIrq);
	cpu.SetReg(13, 0x03007FA0);
	cpu.SetReg(14, 0x11112222);
	cpu.SwitchMode(ModeFiq);
	cpu.SetReg(8, 0x33334444);
	cpu.SetReg(13, 0x55556666);
	cpu.SwitchMode(ModeSupervisor);
	cpu.SetReg(13, 0x03007FE0);
	cpu.SwitchMode(ModeSystem);
	cpu.SetReg(7, 0x77778888);

	std::vector<uint8_t> image;
	std::string error;
	GBA_CHECK(system.SaveState(image, &error));

	// Wreck every one of them.
	cpu.SwitchMode(ModeIrq);
	cpu.SetReg(13, 0);
	cpu.SetReg(14, 0);
	cpu.SwitchMode(ModeFiq);
	cpu.SetReg(8, 0);
	cpu.SetReg(13, 0);
	cpu.SwitchMode(ModeSupervisor);
	cpu.SetReg(13, 0);
	cpu.SwitchMode(ModeSystem);
	cpu.SetReg(7, 0);

	GBA_CHECK_MSG(system.LoadState(image, &error), error);

	GBA_CHECK_EQ(cpu.Mode(), ModeSystem);
	GBA_CHECK_HEX32(cpu.Reg(7), 0x77778888);

	cpu.SwitchMode(ModeIrq);
	GBA_CHECK_HEX32(cpu.Reg(13), 0x03007FA0);
	GBA_CHECK_HEX32(cpu.Reg(14), 0x11112222);

	cpu.SwitchMode(ModeFiq);
	GBA_CHECK_HEX32(cpu.Reg(8), 0x33334444);
	GBA_CHECK_HEX32(cpu.Reg(13), 0x55556666);

	cpu.SwitchMode(ModeSupervisor);
	GBA_CHECK_HEX32(cpu.Reg(13), 0x03007FE0);

	cpu.SwitchMode(ModeSystem);
}

GBA_TEST(SaveState, TheDisplayAndTheSaveMemoryAreInTheState)
{
	GbaSystem system;
	BuildDemoMachineWithSram(system, 5);

	// A display register, a byte of video memory and a byte of the cartridge's save memory, each
	// one written through the bus the way a game writes it.
	system.Bus().Write16(0x04000000, 0x0403);
	system.Bus().Write16(0x04000040, 0x2810);
	system.Bus().Write16(0x06000000, 0x1234);
	system.Bus().Write16(0x0E000000, 0x5A5A);

	GBA_CHECK_HEX16(system.Bus().Read16(0x0E000000), 0x5A5A);

	std::vector<uint8_t> image;
	std::string error;
	GBA_CHECK(system.SaveState(image, &error));

	// Change all four, and the machine's clock and key state with them.
	system.Bus().Write16(0x04000000, 0x0000);
	system.Bus().Write16(0x04000040, 0x0000);
	system.Bus().Write16(0x06000000, 0xFFFF);
	system.Bus().Write16(0x0E000000, 0x0000);
	system.SetPressedKeys(KEY_A | KEY_B);

	GBA_CHECK_MSG(system.LoadState(image, &error), error);

	GBA_CHECK_HEX16(system.Bus().Read16(0x04000000), 0x0403);
	GBA_CHECK_HEX16(system.Bus().Read16(0x04000040), 0x2810);
	GBA_CHECK_HEX16(system.Bus().Read16(0x06000000), 0x1234);
	GBA_CHECK_HEX16(system.Bus().Read16(0x0E000000), 0x5A5A);
	GBA_CHECK_HEX16(system.PressedKeys(), 0);
}

// ---------------------------------------------------------------------------------------
// The files a frontend uses
// ---------------------------------------------------------------------------------------

GBA_TEST(SaveState, TheFileRoundTrip)
{
	std::remove(StateScratch);

	GbaSystem system;
	BuildDemoMachine(system, 12);

	const char* path = StateScratch;

	std::string error;
	GBA_CHECK_MSG(system.SaveStateFile(path, &error), error);
	GBA_CHECK(error.empty());

	// The file exists and is exactly the image SaveState builds.
	std::vector<uint8_t> memoryImage;
	GBA_CHECK(system.SaveState(memoryImage, &error));

	FILE* f = fopen(path, "rb");
	GBA_CHECK_MSG(f != nullptr, "the state file should exist");

	std::vector<uint8_t> fileImage;

	if (f != nullptr)
	{
		uint8_t buffer[4096];
		size_t got = 0;

		while ((got = fread(buffer, 1, sizeof(buffer), f)) > 0)
		{
			fileImage.insert(fileImage.end(), buffer, buffer + got);
		}

		fclose(f);
	}

	GBA_CHECK_EQ(fileImage.size(), memoryImage.size());
	GBA_CHECK(fileImage == memoryImage);

	// Run on and read the file back: the machine has to be where the file says.
	system.RunFrames(6);
	GBA_CHECK_MSG(system.LoadStateFile(path, &error), error);

	// A missing file is an error with a message, not a silent success.
	std::string missingError;
	GBA_CHECK(!system.LoadStateFile("gba_bench_no_such_state.st0", &missingError));
	GBA_CHECK_MSG(!missingError.empty(), "a missing state file must say so");

	std::remove(path);
}

GBA_TEST(SaveState, TheStateFileIsNamedAfterTheCartridge)
{
	GbaSystem system;
	BuildDemoMachine(system, 2);

	// Loaded from an image (the tests never write the ROM to disk), so the name comes from the
	// cartridge's own title.
	std::string path = system.StateFilePath(3);
	GBA_CHECK_MSG(path.find(system.RomTitle()) != std::string::npos,
		"the state file should be named after the ROM: " + path);
	GBA_CHECK_MSG(path.size() > 3 && path.substr(path.size() - 3) == "st3",
		"the slot is the extension of the state file: " + path);

	// The slot is bounded, and every slot has its own file.
	std::string path1 = system.StateFilePath(1);
	GBA_CHECK_MSG(path1.size() > 3 && path1.substr(path1.size() - 3) == "st1",
		"every slot has its own state file: " + path1);
	GBA_CHECK(system.StateFilePath(0) != system.StateFilePath(1));
	GBA_CHECK_STR(system.StateFilePath(-5), system.StateFilePath(0));
	GBA_CHECK_STR(system.StateFilePath(MaxStateSlot + 100), system.StateFilePath(MaxStateSlot));
}

// =======================================================================================
// The Game Boy and the Game Boy Color
// =======================================================================================
//
// The Game Boy is a machine of its own in the same module and it has save states of its own; the
// interesting half of them is the CGB's: the two VRAM banks with their attribute map, the two
// colour palette banks with their index registers, the HDMA transfer that runs during HBlank and
// the KEY1 speed switch that puts the CPU into double speed. The cartridge these tests run is
// assembled here with the module's own LR35902 emitter (the same one the boot ROM is built with):
// every one of those features is switched on by the program itself, so the state that comes back
// has to carry all of them.

namespace
{
	/// <summary>
	/// A Game Boy cartridge whose program turns on everything a CGB has (the speed switch, VRAM
	/// bank 1, a colour palette and an HBlank HDMA of 128 blocks), then counts VBlanks into WRAM
	/// at 0xC000 and poll-loops on LY. Built with the module's own emitter, so the test cartridge
	/// is source rather than a binary blob.
	///
	/// The DMG variant is the same cartridge without the CGB-only sequence (a DMG would sit in
	/// STOP for ever and never reach its frame loop), which is what lets the console check of the
	/// state be exercised with two machines that are running the same program.
	/// </summary>
	std::vector<uint8_t> BuildGbRom(bool cgb)
	{
		using namespace GbAsm;

		Assembler a;

		a.Org(0x100);
		a.Nop();						// the entry the boot ROM jumps to (unused: the tests skip it)

		a.Org(GbHeaderTitle);
		a.DataBytes((const uint8_t*)"PUREIKYUBU GB", 13);
		a.Data8(0x00);
		a.Data8(0x00);
		a.Org(GbHeaderCgbFlag);
		a.Data8(cgb ? 0x80 : 0x00);
		a.Org(GbHeaderCartType);
		a.Data8(0x00);					// ROM only
		a.Org(GbHeaderRomSize);
		a.Data8(0x00);					// 32 KByte
		a.Org(GbHeaderRamSize);
		a.Data8(0x00);

		a.Org(0x150);
		a.Di();
		a.Ld16(R16::SP, 0xFFFE);

		if (cgb)
		{
			// The speed switch: KEY1's bit 0 asks for it and STOP performs it.
			a.Ld(R8::A, 0x01);
			a.LdA8(0x4D);
			a.Stop();

			// Bank 1 of VRAM: the attribute map a colour cartridge writes its tile attributes to.
			// The bank stays selected, so the state has to carry a VBK whose bit 0 is set.
			a.Ld(R8::A, 0x01);
			a.LdA8(0x4F);
			a.Ld16(R16::HL, 0x9800);
			a.Ld(R8::A, 0x08);
			a.Ld(R8::HL, R8::A);

			// Two colour palette entries through BGPI/BGPD (the auto-increment index).
			a.Ld(R8::A, 0x80);
			a.LdA8(0x68);
			a.Ld(R8::A, 0x1F);
			a.LdA8(0x69);
			a.Ld(R8::A, 0x03);
			a.LdA8(0x69);
			a.Ld(R8::A, 0xE0);
			a.LdA8(0x69);
			a.Ld(R8::A, 0x7F);
			a.LdA8(0x69);
		}

		// The frame loop: wait for the start of VBlank, count it in WRAM, wait for the picture.
		a.Label("vblank");
		a.Ld8A(0x44);
		a.Cp(144);
		a.Jr(Cond::NZ, "vblank");

		a.LdA16(0xC000);
		a.Inc(R8::A);
		a.Ld16A(0xC000);

		if (cgb)
		{
			// An HBlank HDMA of 128 blocks (0xFF in HDMA5: bit 7 selects the HBlank mode and the
			// low seven bits are the block count minus one), re-armed at the start of every frame:
			// 2 KByte from the ROM at 0x0200 into VRAM at 0x8000, sixteen bytes per HBlank. Its
			// 128 blocks take 128 visible lines, so a state taken anywhere in the visible part of
			// a frame catches the transfer part way through - which is what makes the HDMA's own
			// state (its source and destination pointers and the blocks left) something a save
			// state has to carry.
			a.Ld(R8::A, 0x02);
			a.LdA8(0x51);
			a.Ld(R8::A, 0x00);
			a.LdA8(0x52);
			a.Ld(R8::A, 0x80);
			a.LdA8(0x53);
			a.Ld(R8::A, 0x00);
			a.LdA8(0x54);
			a.Ld(R8::A, 0xFF);
			a.LdA8(0x55);
		}

		a.Label("visible");
		a.Ld8A(0x44);
		a.Cp(144);
		a.Jr(Cond::C, "visible");
		a.Jr("vblank");

		// The HDMA's source: a recognizable pattern.
		a.Org(0x200);
		for (int i = 0; i < 16; i++)
			a.Data8((uint8_t)(0x40 + i));

		std::vector<uint8_t> image = a.TakeImage(0x8000);

		// The header checksum the boot ROM would hand over in A.
		uint8_t sum = 0;
		for (uint16_t address = GbHeaderTitle; address <= GbHeaderVersion; address++)
			sum = (uint8_t)(sum - image[address] - 1);
		image[GbHeaderChecksum] = sum;

		return image;
	}

	/// <summary>A Game Boy machine with the test cartridge in it, run for `frames` frames. The
	/// boot ROM is skipped: the cartridge program is what the tests care about.</summary>
	void BuildGbMachine(GbSystem& system, bool cgb, int frames)
	{
		GbSettings settings = GbSettings::Defaults();
		settings.cgb = cgb;
		settings.useBootRom = false;
		system.ApplySettings(settings);

		std::string error;
		if (!system.LoadRomImage(BuildGbRom(cgb), error))
		{
			GBA_FAIL("the Game Boy test cartridge did not load: " + error);
		}

		system.Reset();
		system.RunFrames(frames);
	}

	/// <summary>The VBlank counter the test cartridge keeps in WRAM.</summary>
	uint8_t GbCounter(const GbSystem& system)
	{
		return system.Bus().Peek(0xC000);
	}
}

GBA_TEST(GameBoyState, TheCgbMachineResumesExactly)
{
	GbSystem system;
	BuildGbMachine(system, true, 2);

	// The program really did switch the machine into double speed, wrote bank 1 and counted its
	// frames; if this ever stops being true the rest of the test would be checking nothing.
	GBA_CHECK_MSG(system.Bus().DoubleSpeed(), "the test cartridge should have switched to double speed");
	GBA_CHECK_MSG(GbCounter(system) > 0, "the test cartridge should have counted a VBlank");

	// A hundred lines into the frame: the HDMA the program re-armed at the last VBlank is part
	// way through its 128 blocks, the LCD is somewhere inside a line, and the CPU is in the
	// middle of its poll loop.
	system.RunCycles(GbDotsPerLine * 100);

	GBA_CHECK_MSG(system.Bus().HdmaActive(), "the HDMA should still be running a hundred lines in");

	std::vector<uint8_t> image;
	std::string error;
	GBA_CHECK_MSG(system.SaveState(image, &error), error);

	int expectedBlocks = system.Bus().HdmaRemainingBlocks();
	int savedFrame = system.FrameCounter();
	GBA_CHECK_MSG(expectedBlocks > 0 && expectedBlocks < 128,
		"the state should be taken with the HDMA part way through, not finished: " +
		std::to_string(expectedBlocks));

	system.RunFrames(4);
	system.RunCycles(333);

	int expectedFrame = system.FrameCounter();
	uint8_t expectedCounter = GbCounter(system);
	uint32_t expectedPicture = FrameHash(system);
	uint64_t expectedCycles = system.Cycles();

	GBA_CHECK_MSG(system.LoadState(image, &error), error);

	// The colour state is back: the speed, the HDMA's remaining blocks, the frame the state was
	// taken at and the picture on the screen.
	GBA_CHECK_MSG(system.Bus().DoubleSpeed(), "the load should have restored the double speed mode");
	GBA_CHECK_EQ(system.Bus().HdmaRemainingBlocks(), expectedBlocks);
	GBA_CHECK_EQ(system.Lcd().FrameCounter(), savedFrame);

	system.RunFrames(4);
	system.RunCycles(333);

	GBA_CHECK_EQ(system.FrameCounter(), expectedFrame);
	GBA_CHECK_EQ(GbCounter(system), expectedCounter);
	GBA_CHECK_HEX32(FrameHash(system), expectedPicture);
	GBA_CHECK_EQ(system.Cycles(), expectedCycles);
}

GBA_TEST(GameBoyState, TheCgbOnlyStateIsInTheImage)
{
	GbSystem system;
	BuildGbMachine(system, true, 2);
	system.RunCycles(GbDotsPerLine * 100);

	// What the program set up, read back through the machine before anything is saved.
	const uint8_t* palette = system.Lcd().PaletteRam(false);
	GBA_CHECK_HEX16((uint16_t)(palette[0] | (palette[1] << 8)), 0x031F);
	GBA_CHECK_HEX16((uint16_t)(palette[2] | (palette[3] << 8)), 0x7FE0);
	GBA_CHECK_HEX16(system.Lcd().VramBank(1)[0x1800], 0x08);

	std::vector<uint8_t> image;
	std::string error;
	GBA_CHECK(system.SaveState(image, &error));

	int blocks = system.Bus().HdmaRemainingBlocks();
	GBA_CHECK(blocks > 0 && blocks < 128);

	// Wreck every CGB-only piece of state there is.
	system.Bus().SetDoubleSpeed(false);
	system.Bus().WriteByte(0xFF4F, 0x00);			// VBK back to bank 0
	system.Bus().WriteByte(0xFF68, 0x00);			// BGPI
	system.Bus().WriteByte(0xFF69, 0x00);			// the first palette entry
	system.Lcd().VramBank(1)[0x1800] = 0x00;
	system.Bus().WriteByte(0xFF55, 0x00);			// stop the HBlank DMA (bit 7 clear stops it)

	GBA_CHECK_MSG(system.LoadState(image, &error), error);

	GBA_CHECK_MSG(system.Bus().DoubleSpeed(), "the CGB's double speed mode is part of the state");
	GBA_CHECK_HEX16(system.Bus().Vbk(), 0xFF);		// bit 0 is the bank, the rest read as ones
	GBA_CHECK_HEX16((uint16_t)(system.Lcd().PaletteRam(false)[0] | (system.Lcd().PaletteRam(false)[1] << 8)), 0x031F);
	GBA_CHECK_HEX16((uint16_t)(system.Lcd().PaletteRam(false)[2] | (system.Lcd().PaletteRam(false)[3] << 8)), 0x7FE0);
	GBA_CHECK_HEX16(system.Lcd().VramBank(1)[0x1800], 0x08);
	GBA_CHECK_EQ(system.Bus().HdmaRemainingBlocks(), blocks);
}

GBA_TEST(GameBoyState, ASaveStateOfTheOtherMachineIsRefused)
{
	// A Game Boy Advance state offered to a Game Boy, and the other way round: the first field of
	// the state's first section is the machine it belongs to.
	GbaSystem gba;
	BuildDemoMachine(gba, 5);

	std::vector<uint8_t> gbaImage;
	std::string error;
	GBA_CHECK(gba.SaveState(gbaImage, &error));

	GbSystem gb;
	BuildGbMachine(gb, true, 2);

	error.clear();
	GBA_CHECK_MSG(!gb.LoadState(gbaImage, &error), "a GBA state must not load into a Game Boy");
	GBA_CHECK_MSG(error.find("Game Boy Advance") != std::string::npos,
		"the message should name the machine the state belongs to: " + error);

	std::vector<uint8_t> gbImage;
	GBA_CHECK(gb.SaveState(gbImage, &error));

	error.clear();
	GBA_CHECK_MSG(!gba.LoadState(gbImage, &error), "a Game Boy state must not load into a GBA");
	GBA_CHECK_MSG(error.find("Game Boy") != std::string::npos,
		"the message should name the machine the state belongs to: " + error);
}

GBA_TEST(GameBoyState, AStateOfTheOtherConsoleIsRefused)
{
	// The same cartridge, the same program, two consoles: a CGB state carries the colour palettes,
	// the two VRAM banks and the double speed mode, and a DMG has none of them.
	GbSystem colour;
	BuildGbMachine(colour, true, 3);

	std::vector<uint8_t> image;
	std::string error;
	GBA_CHECK(colour.SaveState(image, &error));

	GbSystem mono;
	BuildGbMachine(mono, false, 3);

	error.clear();
	GBA_CHECK_MSG(!mono.LoadState(image, &error), "a CGB state must not load into a DMG");
	GBA_CHECK_MSG(error.find("Color") != std::string::npos,
		"the message should name the console: " + error);

	// And the monochrome machine's own state loads back into itself.
	std::vector<uint8_t> monoImage;
	GBA_CHECK(mono.SaveState(monoImage, &error));
	GBA_CHECK_MSG(mono.LoadState(monoImage, &error), error);
}

GBA_TEST(GameBoyState, TheFileRoundTrip)
{
	const char* path = "gba_bench_savestate_gb_test.st0";
	std::remove(path);

	GbSystem system;
	BuildGbMachine(system, true, 3);

	std::string error;
	GBA_CHECK_MSG(system.SaveStateFile(path, &error), error);

	// The name a frontend would have used for the slot is the title of the cartridge, because the
	// test loads it from an image rather than a file.
	std::string named = system.StateFilePath(0);
	GBA_CHECK_MSG(named.find(system.RomTitle()) != std::string::npos,
		"the state file should be named after the ROM: " + named);

	std::vector<uint8_t> image;
	GBA_CHECK(system.SaveState(image, &error));

	FILE* f = fopen(path, "rb");
	GBA_CHECK_MSG(f != nullptr, "the state file should exist");

	std::vector<uint8_t> fileImage;

	if (f != nullptr)
	{
		uint8_t buffer[4096];
		size_t got = 0;

		while ((got = fread(buffer, 1, sizeof(buffer), f)) > 0)
		{
			fileImage.insert(fileImage.end(), buffer, buffer + got);
		}

		fclose(f);
	}

	GBA_CHECK(fileImage == image);

	system.RunFrames(5);
	GBA_CHECK_MSG(system.LoadStateFile(path, &error), error);

	std::remove(path);
}
