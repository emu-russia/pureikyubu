// The Game Boy machine: the bus, the boot ROM and the frame/audio scheduling a frontend needs.
//
// A frontend (the SDL build, the test harness) only has to do three things: load a cartridge,
// push the key state and run frames. Everything else - the boot ROM, the save file, the link
// cable, the console kind - is set up from here.
//
// The DMG/CGB pair live in one machine because the hardware does: a CGB is a DMG with more
// memory, more colours and two extra registers, and a cartridge says which of the two it wants.
// GbSettings::cgb selects the console; the cartridge's CGB flag decides whether a CGB runs it in
// colour or in compatibility mode.

#pragma once

#include "gba_types.h"
#include "gb_bus.h"
#include "gb_bootrom.h"

#include <string>
#include <vector>

namespace GBA
{
	/// <summary>How the machine is configured (the frontend's settings, not the cartridge's).</summary>
	struct GbSettings
	{
		/// <summary>The console to emulate. A CGB runs DMG cartridges in compatibility mode.</summary>
		bool cgb = false;

		/// <summary>Run the boot ROM (the built-in one unless `bootRomPath` names a file).</summary>
		bool useBootRom = true;

		/// <summary>A real boot ROM image to use instead of the built-in one (empty = built-in).
		/// The file may be the 256 byte DMG image or the 2304 byte CGB one; the first 256 bytes
		/// are used.</summary>
		std::string bootRomPath;

		/// <summary>Start the cartridge directly at 0x0100 with the post-boot registers (the
		/// fast path a frontend uses when it does not want the boot animation).</summary>
		bool skipBootRom = false;

		/// <summary>The shades the monochrome picture is drawn with.</summary>
		GbPalette palette = GbPalette::Green;

		/// <summary>The host sample rate for ReadAudio().</summary>
		int sampleRate = 48000;

		/// <summary>0 = quiet, 1 = errors, 2 = warnings, 3 = info, 4 = debug.</summary>
		int logLevel = 2;

		static GbSettings Defaults() { return GbSettings{}; }
	};

	class GbSystem
	{
	public:
		GbSystem();
		~GbSystem();

		// -- configuration -------------------------------------------------------------------

		void ApplySettings(const GbSettings& settings);
		const GbSettings& Settings() const { return settings; }
		GbSettings& MutableSettings() { return settings; }

		/// <summary>Use the built-in boot ROM (on by default).</summary>
		void UseBootRom(bool use) { settings.useBootRom = use; }

		/// <summary>Start the cartridge directly at 0x0100 instead of running the boot ROM.</summary>
		void SkipBootRom(bool skip) { settings.skipBootRom = skip; }

		/// <summary>Choose the console kind (a CGB has the colour palettes and the extra
		/// registers). Reset() applies it to the bus.</summary>
		void SetCgb(bool cgb) { settings.cgb = cgb; }
		bool Cgb() const { return settings.cgb; }

		// -- the cartridge -------------------------------------------------------------------

		bool LoadRomFile(const std::string& path, std::string& error);
		bool LoadRomImage(const std::vector<u8>& image, std::string& error);

		/// <summary>Start with no cartridge in the slot (the boot ROM then shows its marker).</summary>
		void EjectRom() { bus->cart.Eject(); }

		bool RomLoaded() const { return bus->cart.IsLoaded(); }

		/// <summary>Save the cartridge's `.sav` (called on a clean shutdown).</summary>
		bool SaveBattery(std::string* error) { return bus->cart.SaveSaveFile(error); }

		/// <summary>Read the battery backed RAM and the RTC state back from the `.sav`.</summary>
		bool LoadBattery(std::string* error) { return bus->cart.LoadSaveFile(error); }

		// -- running -------------------------------------------------------------------------

		/// <summary>Reset the machine (the settings and the cartridge stay loaded).</summary>
		void Reset();

		/// <summary>Run `cycles` of the 4.194304 MHz system clock.</summary>
		void RunCycles(int cycles);

		/// <summary>Run until the LCD has finished one frame, whatever the CPU does.</summary>
		void RunFrame();

		/// <summary>Run until the LCD has drawn `count` more frames.</summary>
		void RunFrames(int count);

		// -- input ---------------------------------------------------------------------------

		/// <summary>Set the pressed buttons (the GbButton bits of gb_bus.h).</summary>
		void SetPressedKeys(u8 mask);
		u8 PressedKeys() const { return bus->PressedKeys(); }

		// -- output --------------------------------------------------------------------------

		/// <summary>The current frame, XRGB8888 (0xAARRGGBB), 160 x 144 pixels.</summary>
		const u32* FrameBuffer() const { return bus->ppu.Frame(); }

		int FrameCounter() const { return bus->ppu.FrameCounter(); }

		/// <summary>Drain the mixed audio (interleaved stereo, 16-bit).</summary>
		int ReadAudio(s16* out, int maxFrames);

		void SetSampleRate(int hz);
		int SampleRate() const { return bus->apu.SampleRate(); }

		/// <summary>Choose the shades of the monochrome picture (a CGB ignores it).</summary>
		void SetPalette(GbPalette palette);

		// -- the link cable ------------------------------------------------------------------

		/// <summary>Plug two machines into each other (either order; the cable is symmetric).</summary>
		void AttachLink(GbSystem* other);
		void DetachLink();
		bool LinkAttached() const { return linkAttached; }

		/// <summary>The bytes the last completed transfer sent and received.</summary>
		u8 LastSent() const { return bus->LastSent(); }
		u8 LastReceived() const { return bus->LastReceived(); }
		int SerialTransfers() const { return bus->SerialTransfers(); }

		// -- introspection -------------------------------------------------------------------

		GbBus& Bus() { return *bus; }
		const GbBus& Bus() const { return *bus; }

		GbCpu& Cpu() { return bus->cpu; }
		GbPpu& Lcd() { return bus->ppu; }
		GbApu& Sound() { return bus->apu; }
		GbCart& Cartridge() { return bus->cart; }

		/// <summary>The boot ROM image the machine runs, whole: the built-in one (256 bytes for
		/// both machines' own animation) or the file `bootRomPath` named, which is the DMG's 256
		/// bytes or the CGB's 2304.</summary>
		const std::vector<u8>& BootRomImage() const { return bootImage; }

		/// <summary>True when the boot ROM in use came from a file rather than being built in.</summary>
		bool BootRomFromFile() const { return bootImageFromFile; }

		/// <summary>The cartridge's title, for the window title and the harness report.</summary>
		std::string RomTitle() const { return bus->cart.Header().title; }

		/// <summary>A one line summary of what is loaded and how it boots (the harness prints it).</summary>
		std::string Describe() const;

		/// <summary>The header checksum the boot ROM hands over (0x014D) and the value A gets.</summary>
		u8 HeaderChecksum() const { return headerChecksum; }
		u8 BootRegisterA() const { return bootRegisterA; }

		/// <summary>How many system clocks the machine has run.</summary>
		u64 Cycles() const { return bus->TotalCycles(); }

	private:
		/// <summary>The other end of the serial cable, as this machine sees it.</summary>
		class LinkPeer;

		std::unique_ptr<GbBus> bus;
		GbSettings settings;
		std::vector<u8> bootImage;
		bool bootImageFromFile = false;
		u8 headerChecksum = 0x00;
		u8 bootRegisterA = 0x01;
		bool linkAttached = false;
		bool inBootRom = false;
		std::unique_ptr<LinkPeer> peerToOther;

		void InstallBootRom();

		/// <summary>Set the registers the boot ROM leaves before the cartridge starts.</summary>
		void PrepareBootState();

		/// <summary>Start the cartridge without running the boot ROM (the post-boot state).</summary>
		void StartCartridgeDirect();

		/// <summary>The CPU's STOP hook: a CGB speed switch, or the DMG's standby.</summary>
		bool OnStopInstruction();
	};
}
