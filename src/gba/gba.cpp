// The GBA machine: what a frontend (the SDL build, the harness) talks to.
//
// The system owns the bus, installs the BIOS (the emulator's own boot ROM or a real image), and
// provides the three things a frontend needs: load a ROM, push the key state, run frames. It also
// owns the two boot conventions the emulator supports:
//
//   * **the boot ROM runs**: the CPU starts at address 0 in the emulator's own boot ROM, which
//     draws the logo animation, checks the cartridge and jumps to it (or starts the link driver);
//   * **HLE boot** (settings.useCustomBootRom == false): no BIOS runs at all and the machine is
//     put directly into the state the real BIOS leaves behind (System mode, the stack pointers
//     0x03007F00/0x03007FA0/0x03007FE0, POSTFLG = 1, PC = 0x08000000), which is what a frontend
//     wants when the user only cares about the game and not about the logo.

#include "gba.h"
#include "gba_bootrom.h"
#include "gba_hlebios.h"

namespace GBA
{
	GbaSystem::GbaSystem()
	{
		bus.reset(new GbaBus());
		settings = GbaSettings::Defaults();
		InstallBootRom();
	}

	GbaSystem::~GbaSystem()
	{
	}

	/// <summary>
	/// Run a piece of setup that must not take the emulator down. The boot ROM is built at run
	/// time (it is source, not a binary blob), so a bug in the builder must leave the emulator
	/// usable - with a message and without the animation - instead of throwing out of the
	/// constructor of every GbaSystem.
	/// </summary>
	template <typename T>
	static bool TrySetup(const char* what, T&& body)
	{
		try
		{
			body();
			return true;
		}
		catch (const std::exception& e)
		{
			Log(LogLevel::Error, "GBA: %s: %s", what, e.what());
		}
		catch (...)
		{
			Log(LogLevel::Error, "GBA: %s: unknown error", what);
		}

		return false;
	}

	// ---------------------------------------------------------------------------------------
	// Configuration
	// ---------------------------------------------------------------------------------------

	void GbaSystem::ApplySettings(const GbaSettings& newSettings)
	{
		settings = newSettings;

		bus->HleBiosEnabled = settings.hleBios;
		bus->cart.SetRtcEnabled(settings.rtcEnabled);
		bus->apu.SetSampleRate(settings.sampleRate);

		InstallBootRom();
	}

	void GbaSystem::UseCustomBootRom(bool use)
	{
		settings.useCustomBootRom = use;
		InstallBootRom();
	}

	void GbaSystem::InstallBootRom()
	{
		biosImage.clear();
		realBios = false;

		if (!settings.biosPath.empty())
		{
			FILE* f = fopen(settings.biosPath.c_str(), "rb");
			if (f != nullptr)
			{
				biosImage.resize(BiosSize, 0xFF);
				size_t read = fread(biosImage.data(), 1, BiosSize, f);
				fclose(f);

				Log(LogLevel::Info, "GBA: using the BIOS image %s (%zu bytes)",
					settings.biosPath.c_str(), read);

				bus->SetBios(biosImage.data(), biosImage.size());
				bus->SetCustomBios(false);

				// A real BIOS is *executed*: the CPU starts at address 0 and lets it boot the
				// cartridge (or decide what to do with no cartridge). Its own SWI handler must run
				// as well, so the host-side BIOS calls are switched off with it - a real BIOS that
				// has its service functions intercepted is not a real BIOS.
				realBios = true;
				settings.hleBios = false;
				bus->HleBiosEnabled = false;
				return;
			}

			Log(LogLevel::Warn, "GBA: cannot open the BIOS image %s; falling back to the custom boot ROM",
				settings.biosPath.c_str());
		}

		if (settings.useCustomBootRom)
		{
			const std::vector<u8>* image = nullptr;

			if (TrySetup("building the boot ROM", [&] { image = &BootRom::GbaImage(); }))
			{
				bus->SetBios(image->data(), image->size());
				bus->SetCustomBios(true);
				return;
			}

			// The builder failed: the machine still has to be usable, so the HLE boot path takes
			// over (no BIOS image at all).
			settings.useCustomBootRom = false;
			Log(LogLevel::Warn, "GBA: the boot ROM could not be built; booting the cartridge directly");
		}

		// No BIOS at all: the HLE boot path sets the machine up directly.
		bus->SetBios(nullptr, 0);
		bus->SetCustomBios(false);
	}

	bool GbaSystem::LoadBiosFile(const std::string& path, std::string& error)
	{
		if (path.empty())
		{
			settings.biosPath.clear();
			InstallBootRom();
			return true;
		}

		FILE* f = fopen(path.c_str(), "rb");
		if (f == nullptr)
		{
			error = "cannot open " + path;
			return false;
		}

		std::vector<u8> image(BiosSize, 0xFF);
		size_t read = fread(image.data(), 1, BiosSize, f);
		fclose(f);

		if (read == 0)
		{
			error = path + " is empty";
			return false;
		}

		biosImage = image;
		settings.biosPath = path;
		settings.hleBios = false;
		realBios = true;

		bus->SetBios(biosImage.data(), biosImage.size());
		bus->SetCustomBios(false);
		bus->HleBiosEnabled = false;

		return true;
	}

	// ---------------------------------------------------------------------------------------
	// The cartridge
	// ---------------------------------------------------------------------------------------

	bool GbaSystem::LoadRomFile(const std::string& path, std::string& error)
	{
		if (!bus->cart.LoadRomFile(path, error))
		{
			return false;
		}

		// The save file lives next to the ROM unless the settings say otherwise.
		std::string savePath;

		if (!settings.saveDirectory.empty())
		{
			size_t slash = path.find_last_of("/\\");
			std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
			size_t dot = base.find_last_of('.');
			if (dot != std::string::npos)
			{
				base = base.substr(0, dot);
			}
			savePath = settings.saveDirectory + "/" + base + ".sav";
		}
		else
		{
			size_t dot = path.find_last_of('.');
			savePath = (dot == std::string::npos) ? (path + ".sav") : (path.substr(0, dot) + ".sav");
		}

		bus->cart.SetSaveFilePath(savePath);

		std::string saveError;
		if (!bus->cart.LoadSaveFile(savePath, &saveError) && !saveError.empty())
		{
			Log(LogLevel::Warn, "GBA: %s", saveError.c_str());
		}

		Reset();

		return true;
	}

	bool GbaSystem::LoadRomImage(const std::vector<u8>& image, std::string& error)
	{
		if (!bus->cart.LoadRom(image, error))
		{
			return false;
		}

		Reset();
		return true;
	}

	void GbaSystem::EjectRom()
	{
		std::string error;
		SaveBattery(&error);
		bus->cart.Eject();
		Reset();
	}

	bool GbaSystem::SaveBattery(std::string* error)
	{
		if (!bus->cart.IsLoaded())
		{
			return true;
		}

		if (bus->cart.SaveFilePath().empty())
		{
			return true;
		}

		return bus->cart.SaveSaveFile(bus->cart.SaveFilePath(), error);
	}

	// ---------------------------------------------------------------------------------------
	// Boot and run
	// ---------------------------------------------------------------------------------------

	void GbaSystem::Reset()
	{
		bus->Reset();
		HleBios::Reset();
		bus->apu.SetSampleRate(settings.sampleRate);

		// With no cartridge the boot ROM's link driver takes over; that is the "GBA Link" mode
		// the emulator is built for.
		linkMode = !bus->cart.IsLoaded();

		if (!settings.useCustomBootRom)
		{
			if (realBios)
			{
				// A real BIOS is installed: the CPU starts at address 0 and the BIOS does the
				// booting (with or without a cartridge in the slot), exactly as on the console.
				// Nothing to set up here - the CPU's reset state is PC = 0 in the BIOS.
			}
			else if (bus->cart.IsLoaded())
			{
				// The state the real BIOS leaves behind when it starts a cartridge (GBATEK
				// "GBA BIOS Init"): System mode, the three stack pointers, POSTFLG set.
				bus->cpu.SwitchMode(ModeIrq);
				bus->cpu.SetReg(13, 0x03007FA0);

				bus->cpu.SwitchMode(ModeSupervisor);
				bus->cpu.SetReg(13, 0x03007FE0);

				bus->cpu.SwitchMode(ModeSystem);
				bus->cpu.SetReg(13, 0x03007F00);

				for (int i = 0; i < 13; i++)
				{
					bus->cpu.SetReg(i, 0);
				}

				bus->cpu.WriteCPSR(ModeSystem);
				bus->irq.WriteIME(true);
				bus->Write8(0x04000300, 1);		// POSTFLG
				bus->cpu.BranchTo(MemRom1);
			}
			else
			{
				// Nothing to run: leave the CPU halted at the reset vector.
				bus->cpu.Halt();
			}
		}
	}

	void GbaSystem::RunCycles(int cycles)
	{
		int budget = cycles;

		while (budget > 0)
		{
			int taken = bus->cpu.Step();

			if (taken < 1)
			{
				taken = 1;
			}

			bus->Tick(taken);
			budget -= taken;
		}
	}

	void GbaSystem::RunFrame()
	{
		int frame = bus->ppu.FrameCounter();

		// A frame is 228 scanlines; the guard is only there so that a hung ROM cannot hang the
		// frontend (the CPU is under the ROM's control, and the emulator never blocks on it).
		u64 guard = 0;
		const u64 maxCycles = (u64)CyclesPerFrame * 4;

		while (bus->ppu.FrameCounter() == frame && guard < maxCycles)
		{
			int taken = bus->cpu.Step();

			if (taken < 1)
			{
				taken = 1;
			}

			bus->Tick(taken);
			guard += taken;
		}
	}

	void GbaSystem::RunFrames(int count)
	{
		for (int i = 0; i < count; i++)
		{
			RunFrame();
		}
	}

	// ---------------------------------------------------------------------------------------
	// Input and output
	// ---------------------------------------------------------------------------------------

	void GbaSystem::SetPressedKeys(u16 mask)
	{
		bus->keypad.SetPressed(mask);
	}

	int GbaSystem::ReadAudio(s16* out, int maxFrames)
	{
		if (!settings.audioEnabled)
		{
			return 0;
		}

		int frames = bus->apu.ReadSamples(out, maxFrames);

		// The settings' volume is applied here so the core can mix at its own level.
		if (settings.volume != 100)
		{
			for (int i = 0; i < frames * 2; i++)
			{
				out[i] = (s16)((out[i] * settings.volume) / 100);
			}
		}

		return frames;
	}

	void GbaSystem::AttachLink(GbaSystem* other)
	{
		bus->sio.Attach(other == nullptr ? nullptr : &other->bus->sio);
	}

	void GbaSystem::DetachLink()
	{
		bus->sio.Attach(nullptr);
	}

	// ---------------------------------------------------------------------------------------
	// Introspection
	// ---------------------------------------------------------------------------------------

	std::string GbaSystem::RomTitle() const
	{
		return bus->cart.Title();
	}

	std::string GbaSystem::RomGameCode() const
	{
		return bus->cart.GameCode();
	}

	std::string GbaSystem::Describe() const
	{
		std::string text = "GBA";

		if (bus->cart.IsLoaded())
		{
			text += " \"" + bus->cart.Title() + "\" [" + bus->cart.GameCode() + "]";
			text += " " + std::to_string((unsigned long long)bus->cart.RomSize()) + " bytes";
			text += " save=" + std::string(bus->cart.SaveTypeName());
		}
		else
		{
			text += " no cartridge";
		}

		text += bus->UsingCustomBios() ? " bios=custom-bootrom" : " bios=image";
		text += bus->HleBiosEnabled ? " hle=on" : " hle=off";
		text += linkMode ? " link=driver" : " link=idle";

		if (bus->sio.Peer() != nullptr)
		{
			text += " cable=connected";
		}

		text += " sampleRate=" + std::to_string(bus->apu.SampleRate());

		return text;
	}
}
