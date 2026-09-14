// The Game Boy machine. See gb.h for what a frontend sees.
//
// The interesting parts here are the boot sequence and the hand over. Running the boot ROM is a
// cycle loop that stops when the ROM unmaps itself (a write to 0xFF50), which is how the real
// cartridge start works; the register state the cartridge then finds is set by
// PrepareBootState(), which fixes up the A register the boot ROM cannot know about (it hands over
// 0x01 because it does not know whether the console is a CGB).

#include "gb.h"

#include <cstdio>
#include <cstring>
#include <ctime>

namespace GBA
{
	// ---------------------------------------------------------------------------------------
	// The link cable
	// ---------------------------------------------------------------------------------------

	/// <summary>
	/// One machine's view of the other end of the serial cable. Each machine gives its peer one of
	/// these when AttachLink() runs, so the two ports can read and clock each other's shift
	/// register.
	/// </summary>
	class GbSystem::LinkPeer : public GbSerialPeer
	{
	public:
		explicit LinkPeer(GbSystem* machine) : machine(machine) {}

		bool PeerActive() override
		{
			return machine->bus->SerialActive();
		}

		bool PeerReceiving() override
		{
			// The peer is the slave when it has a transfer running on the external clock.
			return machine->bus->SerialExternalClock();
		}

		u8 PeerByte() override
		{
			// The byte in the peer's shift register: what it is sending.
			return machine->bus->SerialData();
		}

		void PeerClock(u8 bit) override
		{
			// The master's clock shifts a bit through this port too, because the two shift
			// registers share the cable.
			machine->bus->SerialClock(bit);
			machine->bus->SerialTransferBit();
		}

		GbSystem* machine;
	};

	GbSystem::GbSystem()
	{
		bus = std::make_unique<GbBus>();
		bus->cpu.bus = bus.get();
		bus->cpu.OnStop = [](void* user) -> bool
		{
			return static_cast<GbSystem*>(user)->OnStopInstruction();
		};
		bus->cpu.stopUser = this;

		InstallBootRom();
	}

	GbSystem::~GbSystem() = default;

	void GbSystem::InstallBootRom()
	{
		// The machine's own boot ROM first (the built-in one, or the emulator's own animation),
		// and then the file the settings name, if any. The two sizes the hardware has are 256
		// bytes (DMG) and 2304 bytes (CGB): the image is kept *whole*, because the size decides
		// how much of the address space the boot overlay covers. Truncating a CGB image to its
		// first 256 bytes - which is what this used to do - leaves a boot ROM that never reaches
		// its second half (the code and data past the cartridge header), so a colour game cannot
		// boot from it.
		bootImage = settings.cgb ? GbBootRom::CgbImage() : GbBootRom::DmgImage();
		bootImageFromFile = false;

		if (settings.bootRomPath.empty())
			return;

		FILE* file = fopen(settings.bootRomPath.c_str(), "rb");
		if (file == nullptr)
		{
			GBA::Log(LogLevel::Warn, "gb: cannot open the boot ROM %s, using the built-in one",
				settings.bootRomPath.c_str());
			return;
		}

		std::vector<u8> image;
		u8 buffer[512];
		while (image.size() < 0x1000)
		{
			size_t got = fread(buffer, 1, sizeof(buffer), file);
			if (got == 0)
				break;
			image.insert(image.end(), buffer, buffer + got);
		}
		fclose(file);

		// The DMG's boot ROM and the CGB's are the two documented sizes; anything else is either
		// a wrong file or a dump with padding, and either way it is a guess this emulator does not
		// make silently.
		if (image.size() == 0x100 || image.size() == 0x900)
		{
			bootImage = image;
			bootImageFromFile = true;

			const bool matches = (image.size() == 0x100) == !settings.cgb;
			GBA::Log(matches ? LogLevel::Info : LogLevel::Warn,
				"gb: boot ROM %s (%u bytes) %s", settings.bootRomPath.c_str(), (unsigned)image.size(),
				matches ? (settings.cgb ? "for the CGB" : "for the DMG")
					: (settings.cgb ? "is a DMG image, but the machine is a CGB"
						: "is a CGB image, but the machine is a DMG"));
		}
		else
		{
			GBA::Log(LogLevel::Warn, "gb: the boot ROM %s is %u bytes; the DMG's is 256 and the "
				"CGB's is 2304, so the built-in one is used instead",
				settings.bootRomPath.c_str(), (unsigned)image.size());
		}
	}

	void GbSystem::ApplySettings(const GbSettings& newSettings)
	{
		settings = newSettings;

		// The palette and the sample rate are presentation settings and take effect at once; the
		// console kind takes effect on the next Reset(), like the cartridge.
		bus->ppu.SetPalette(settings.palette);
		bus->apu.SetSampleRate(settings.sampleRate);
		InstallBootRom();
	}

	void GbSystem::SetSampleRate(int hz)
	{
		settings.sampleRate = hz;
		bus->apu.SetSampleRate(hz);
	}

	void GbSystem::SetPalette(GbPalette palette)
	{
		settings.palette = palette;
		bus->ppu.SetPalette(palette);
	}

	// ---------------------------------------------------------------------------------------
	// The cartridge
	// ---------------------------------------------------------------------------------------

	bool GbSystem::LoadRomImage(const std::vector<u8>& image, std::string& error)
	{
		if (!bus->cart.LoadRomImage(image, error))
			return false;

		GBA::Log(LogLevel::Info, "gb: loaded a %u byte cartridge", (unsigned)image.size());
		return true;
	}

	bool GbSystem::LoadRomFile(const std::string& path, std::string& error)
	{
		if (!bus->cart.LoadRomFile(path, error))
			return false;

		// The GBA module's convention: the battery backed RAM is read back from the .sav next to
		// the ROM.
		std::string loadError;
		if (bus->cart.HasBattery() && bus->cart.LoadSaveFile(&loadError))
			GBA::Log(LogLevel::Info, "gb: restored %s", bus->cart.SaveFilePath().c_str());

		return true;
	}

	// ---------------------------------------------------------------------------------------
	// Reset and the boot sequence
	// ---------------------------------------------------------------------------------------

	void GbSystem::Reset()
	{
		// The console kind comes from the settings; a DMG cartridge on a CGB still runs in colour
		// compatibility mode (the CGB simply finds no attribute data in VRAM bank 1).
		bus->SetCgb(settings.cgb);
		bus->ppu.SetPalette(settings.palette);
		bus->apu.SetSampleRate(settings.sampleRate);

		bus->Reset();
		bus->cpu.Reset();
		bus->ppu.Reset();
		bus->apu.Reset();
		bus->cart.SetUnixTime((u64)time(nullptr));

		// On a CGB the picture comes from the colour palette memory, so the monochrome BGP
		// register the boot ROM writes has no effect there: the machine installs a grey ramp
		// instead (the real CGB boot ROM's "compatibility palettes", simplified).
		if (settings.cgb)
			bus->ppu.SetGreyscalePalettes();

		// The interrupt and hardware state the boot ROM (or the direct start) expects.
		bus->SetIe(0x00);
		bus->SetIf(0xE1);
		bus->cpu.bus = bus.get();

		headerChecksum = 0;
		bootRegisterA = 1;
		if (bus->cart.IsLoaded())
			headerChecksum = GbComputeHeaderChecksum(bus->cart.Rom());

		// Nobody is mid-transfer when the machine resets.
		bus->AttachSerialPeer(nullptr);

		if (settings.useBootRom && !settings.skipBootRom)
		{
			bus->SetBootRom(bootImage.data(), (u32)bootImage.size());
			bus->MapBootRom(true);
			bus->ppu.SetLcdEnabled(false);
			inBootRom = true;
		}
		else
		{
			bus->SetBootRom(nullptr, 0);
			bus->MapBootRom(false);
			inBootRom = false;
			StartCartridgeDirect();
		}
	}

	void GbSystem::StartCartridgeDirect()
	{
		// The "post-boot state": the machine looks exactly as it does when a real boot ROM jumps
		// to the cartridge, without the animation (Pan Docs "Power Up Sequence": LCDC = 0x91,
		// BGP = 0xFC, the sound registers silenced, and PC = 0x0100 with the registers the boot
		// ROM leaves). This is what a frontend uses when the user turns the animation off.
		if (settings.cgb)
			bus->ppu.SetGreyscalePalettes();

		GbPpu& ppu = bus->ppu;
		ppu.WriteRegister(0xFF47, 0xFC);		// BGP
		ppu.WriteRegister(0xFF48, 0xFF);		// OBP0 (uninitialised on real hardware)
		ppu.WriteRegister(0xFF49, 0xFF);		// OBP1
		ppu.WriteRegister(0xFF40, 0x91);		// LCDC (LCD, BG and OBJ on)
		ppu.WriteRegister(0xFF43, 0x00);		// SCX
		ppu.WriteRegister(0xFF42, 0x00);		// SCY

		bus->SetIf(0xE1);
		bus->SetIe(0x00);

		// The sound registers the boot ROM leaves (Pan Docs "Power Up Sequence").
		const u8 sound[][2] =
		{
			{ 0x10, 0x80 }, { 0x11, 0xBF }, { 0x12, 0xF3 }, { 0x14, 0xBF },
			{ 0x16, 0x3F }, { 0x19, 0xBF }, { 0x1A, 0x7F }, { 0x1C, 0x9F },
			{ 0x1E, 0xBF }, { 0x23, 0xBF }, { 0x24, 0x77 }, { 0x25, 0xF3 },
		};
		for (const auto& entry : sound)
			bus->apu.WriteRegister((u16)(0xFF00 | entry[0]), entry[1]);

		PrepareBootState();
	}

	void GbSystem::PrepareBootState()
	{
		// The boot ROM leaves A = 0x01 (it cannot tell a DMG from a CGB); this fixes the value up
		// and hands the cartridge the registers the hardware leaves at 0x0100.
		if (settings.cgb)
		{
			// On a CGB the boot ROM reports 0x11 for a cartridge whose CGB flag is set and (as
			// the task that produced this module specifies) 0x00 for a DMG only one. The Pan Docs
			// "Power Up Sequence" table gives 0x11 in both cases and carries the mode decision in
			// KEY0 (0xFF4C) instead; A = 0x00 is a deliberate deviation that makes the CGB flag
			// easy to read from the cartridge's point of view, and it is documented here and in
			// GbCart::BootRegisterA().
			bootRegisterA = (bus->cart.IsLoaded() && !bus->cart.CgbCompatible()) ? 0x00 : 0x11;
		}
		else
		{
			bootRegisterA = 0x01;
		}

		// The DMG leaves F = 0xB0 (Z = 1, N = 0, H and C set because the header checksum is
		// non-zero); a CGB leaves Z set and the rest of the flags clear.
		u8 f = settings.cgb ? 0x80 : 0xB0;

		bus->cpu.LoadPostBootRegisters(bootRegisterA, headerChecksum, f);
	}

	// ---------------------------------------------------------------------------------------
	// Running
	// ---------------------------------------------------------------------------------------

	void GbSystem::RunCycles(int cycles)
	{
		int remaining = cycles;
		while (remaining > 0)
		{
			int step = remaining > 64 ? 64 : remaining;
			int used = bus->Run(step);
			if (used <= 0)
				used = step;
			remaining -= used;

			if (inBootRom && !bus->BootRomMapped())
			{
				inBootRom = false;
				PrepareBootState();
			}
		}
	}

	void GbSystem::RunFrame()
	{
		// One frame is 70224 dots (Pan Docs "Rendering"), but a system clock is one dot in normal
		// speed and half a dot in double speed, and a program may switch speed in the middle of a
		// frame. The loop therefore runs until the LCD has finished a frame, with a clock budget
		// as the safety net for the two cases where the LCD will never finish one: the LCD is off
		// (a boot ROM that has not turned it on yet, or a program that switched it off), or a
		// program that never lets the CPU run (a boot ROM that never returns, say).
		int startFrame = bus->ppu.FrameCounter();
		u64 startCycles = bus->TotalCycles();
		u64 budget = (u64)GbDotsPerFrame * (bus->DoubleSpeed() ? 2 : 4);

		while (bus->ppu.FrameCounter() == startFrame && bus->TotalCycles() - startCycles < budget)
		{
			int step = bus->DoubleSpeed() ? 8 : 16;
			int used = bus->Run(step);
			if (used <= 0)
				used = step;

			if (inBootRom && !bus->BootRomMapped())
			{
				inBootRom = false;
				PrepareBootState();
			}
		}
	}

	void GbSystem::RunFrames(int count)
	{
		for (int i = 0; i < count; i++)
			RunFrame();
	}

	// ---------------------------------------------------------------------------------------
	// Input and output
	// ---------------------------------------------------------------------------------------

	void GbSystem::SetPressedKeys(u8 mask)
	{
		u8 previous = bus->PressedKeys();
		bus->SetPressedKeys(mask);

		// A button going low requests the joypad interrupt (Pan Docs "Interrupt Sources").
		if (mask != previous)
			bus->RequestInterrupt(GbIntJoypad);

		// A new button also ends STOP mode (Pan Docs "Reducing Power Consumption": STOP is
		// terminated by one of P10..P13 going low).
		if (bus->InStopMode() && mask != 0)
			bus->WakeFromStop();
	}

	int GbSystem::ReadAudio(s16* out, int maxFrames)
	{
		return bus->apu.ReadSamples(out, maxFrames);
	}

	// ---------------------------------------------------------------------------------------
	// The link cable
	// ---------------------------------------------------------------------------------------

	void GbSystem::AttachLink(GbSystem* other)
	{
		DetachLink();
		if (other == nullptr)
			return;

		// Each machine gives the other a peer that reads its own shift register, so the two ports
		// see each other's byte. The peers are owned by the machines (one each) and stay alive
		// until the link is detached.
		other->DetachLink();
		peerToOther = std::make_unique<LinkPeer>(other);
		other->peerToOther = std::make_unique<LinkPeer>(this);

		bus->AttachSerialPeer(peerToOther.get());
		other->bus->AttachSerialPeer(other->peerToOther.get());
		linkAttached = true;
	}

	void GbSystem::DetachLink()
	{
		if (!linkAttached)
			return;

		bus->AttachSerialPeer(nullptr);
		linkAttached = false;
		peerToOther.reset();
	}

	// ---------------------------------------------------------------------------------------
	// The STOP instruction's hook
	// ---------------------------------------------------------------------------------------

	bool GbSystem::OnStopInstruction()
	{
		// Pan Docs "Reducing Power Consumption": STOP on a CGB performs a speed switch when
		// KEY1's bit 0 (the switch request) is set, and otherwise puts the CPU into the very low
		// power standby that a button press ends. A DMG has no speed to switch.
		if (settings.cgb && (bus->Key1() & 0x01))
		{
			bus->SetDoubleSpeed(!bus->DoubleSpeed());

			// The speed switch resets DIV and makes the switch request read back as zero
			// (Pan Docs "CGB Registers": the switch itself takes 2050 clocks, which this models
			// as an immediate change of rate).
			bus->WriteByte(0xFF04, 0x00);
			bus->WriteByte(0xFF4D, 0x00);

			GBA::Log(LogLevel::Info, "gb: speed switch to %s", bus->DoubleSpeed() ? "double" : "normal");
			return true;			// the CPU carries straight on
		}

		return false;				// enter STOP mode until a button is pressed
	}

	// ---------------------------------------------------------------------------------------
	// The report
	// ---------------------------------------------------------------------------------------

	std::string GbSystem::Describe() const
	{
		std::string text;
		char buffer[256];

		snprintf(buffer, sizeof(buffer), "%s, %s, %u MHz",
			settings.cgb ? "Game Boy Color" : "Game Boy (DMG)",
			settings.cgb ? "colour" : GbPpu::PaletteName(settings.palette),
			(unsigned)(bus->ClockSpeed() / 1000000));
		text = buffer;

		if (bus->cart.IsLoaded())
		{
			std::string cartText;
			bus->cart.Describe(cartText);
			text += ", cartridge ";
			text += cartText;
			if (settings.cgb && !bus->cart.CgbCompatible())
				text += ", DMG compatibility mode";
		}
		else
		{
			text += ", no cartridge";
		}

		if (settings.useBootRom && !settings.skipBootRom)
		{
			// The size says which of the two boot ROMs is in use (256 bytes is the DMG's, 2304
			// the CGB's, split around the cartridge header), so it is worth printing.
			if (bootImageFromFile)
			{
				snprintf(buffer, sizeof(buffer), ", boot ROM %s (%u bytes%s)",
					settings.bootRomPath.c_str(), (unsigned)bootImage.size(),
					bootImage.size() == 0x900 ? ", split around the header" : "");
			}
			else
			{
				snprintf(buffer, sizeof(buffer), ", boot ROM built in (%u bytes, %d frames of animation)",
					(unsigned)bootImage.size(), GbBootRom::AnimationFrames());
			}

			text += buffer;
		}
		else
		{
			text += ", starting at 0x0100 (post-boot state)";
		}

		return text;
	}
}
