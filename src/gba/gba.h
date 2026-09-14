// The GBA machine: the bus, the BIOS image and the frame/audio scheduling a frontend needs.
//
// A frontend (the SDL build, the test harness) only has to do three things: load a ROM, push the
// key state and run frames. Everything else - the boot ROM, the HLE BIOS, the save files, the
// link cable - is set up from here.

#pragma once

#include "gba_types.h"
#include "gba_bus.h"
#include "gba_settings.h"

namespace GBA
{
	class GbaSystem
	{
	public:
		GbaSystem();
		~GbaSystem();

		// -- configuration -----------------------------------------------------------------

		/// <summary>Apply a settings document (the boot ROM choice, the HLE switch, the RTC...).</summary>
		void ApplySettings(const GbaSettings& settings);

		const GbaSettings& Settings() const { return settings; }
		GbaSettings& MutableSettings() { return settings; }

		/// <summary>Use the built-in (custom) boot ROM. On by default.</summary>
		void UseCustomBootRom(bool use);

		/// <summary>Install a real BIOS image from disk (empty `path` restores the custom ROM).</summary>
		bool LoadBiosFile(const std::string& path, std::string& error);

		// -- the cartridge -----------------------------------------------------------------

		bool LoadRomFile(const std::string& path, std::string& error);
		bool LoadRomImage(const std::vector<u8>& image, std::string& error);

		/// <summary>Start with no cartridge in the slot (the boot ROM then runs its link driver).</summary>
		void EjectRom();

		bool RomLoaded() const { return bus->cart.IsLoaded(); }

		/// <summary>Save the cartridge's `.sav` (called on a clean shutdown).</summary>
		bool SaveBattery(std::string* error);

		// -- running -----------------------------------------------------------------------

		/// <summary>Reset the machine (the BIOS and the cartridge stay loaded).</summary>
		void Reset();

		/// <summary>Run `cycles` of the system clock.</summary>
		void RunCycles(int cycles);

		/// <summary>Run exactly one frame (228 scanlines), whatever the CPU does.</summary>
		void RunFrame();

		/// <summary>Run until the LCD has drawn `count` frames (the harness uses it to skip the
		/// boot animation).</summary>
		void RunFrames(int count);

		// -- input -------------------------------------------------------------------------

		/// <summary>Set the pressed keys (the KEY_* bits of gba_keypad.h).</summary>
		void SetPressedKeys(u16 mask);
		u16 PressedKeys() const { return bus->keypad.Pressed(); }

		// -- output ------------------------------------------------------------------------

		/// <summary>The current frame, XRGB8888 (0xAARRGGBB), ScreenWidth * ScreenHeight pixels.</summary>
		const u32* FrameBuffer() const { return bus->ppu.Frame(); }

		int FrameCounter() const { return bus->ppu.FrameCounter(); }

		/// <summary>Drain the mixed audio (interleaved stereo, 16-bit).</summary>
		int ReadAudio(s16* out, int maxFrames);

		void SetSampleRate(int hz) { bus->apu.SetSampleRate(hz); }
		int SampleRate() const { return bus->apu.SampleRate(); }

		// -- the link cable ----------------------------------------------------------------

		/// <summary>Plug the two machines into each other.</summary>
		void AttachLink(GbaSystem* other);
		void DetachLink();

		/// <summary>True when the emulator runs with no cartridge and the boot ROM's link driver
		/// is in charge of the port.</summary>
		bool LinkMode() const { return linkMode; }

		// -- introspection -----------------------------------------------------------------

		GbaBus& Bus() { return *bus; }
		const GbaBus& Bus() const { return *bus; }

		Arm7tdmi& Cpu() { return bus->cpu; }
		Ppu& Lcd() { return bus->ppu; }
		Sio& Link() { return bus->sio; }

		/// <summary>The ROM header title, for the window title and the harness report.</summary>
		std::string RomTitle() const;
		std::string RomGameCode() const;

		/// <summary>A one line summary of what is loaded and how it boots (the harness prints it).</summary>
		std::string Describe() const;

		/// <summary>How many cycles the machine has run (the harness measures the speed with it).</summary>
		u64 Cycles() const { return bus->TotalCycles(); }

	private:
		std::unique_ptr<GbaBus> bus;
		GbaSettings settings;
		std::vector<u8> biosImage;		// the installed BIOS (custom or a real one)
		bool linkMode = false;

		/// <summary>True when `biosImage` is a real BIOS the CPU should execute from address 0
		/// (as opposed to the emulator's own boot ROM, or to no BIOS at all and the HLE boot).</summary>
		bool realBios = false;

		void InstallBootRom();
	};
}
