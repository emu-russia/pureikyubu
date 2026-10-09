// The GBA emulator's settings: the pair of documents the rest of the emulator keeps its own
// settings in. build/Data/DefaultGBASettings.json is the shipped one (it is what
// GbaSettings::DefaultJson() writes, and the built-in defaults of Defaults() stand in for it when
// it is not there), and build/Data/GBASettings.json is the user's, merged over the defaults member
// by member - a member the user's file does not name keeps the default. This module is the reader
// and the writer of one document; the pair belongs to the frontend (GBA::LoadSettings in
// gba_sdl.h), and the two files are the same format.
//
// The file is the same shape as the rest of the emulator's settings (a flat object per section).
// The document is read with the emulator's shared Json engine (src/json.cpp), which is self
// contained (the C++ standard library and verify.h only), so the GBA core still compiles and is
// tested without the GameCube side of the emulator - it links the engine rather than carrying a
// parser of its own. The writer below is the module's own, because the GBA settings file keeps the
// project's tab/blank-line layout.

#pragma once

#include "gba_types.h"

namespace GBA
{
	/// <summary>The key names of the emulated keypad, in the order the settings file lists them.</summary>
	struct GbaKeyBinding
	{
		std::string action;			// "A", "B", "SELECT", ... (see KeyBindingNames)
		std::string key;			// a host key name, e.g. "X", "Z", "Return", "Up"
	};

	/// <summary>The eleven bindings a GBA has, in the order they are written to the file.</summary>
	const char* const* KeyBindingNames();

	struct GbaSettings
	{
		// -- boot --------------------------------------------------------------------------

		/// <summary>A real 16 KByte Game Boy Advance BIOS image; empty = the built-in boot ROM.</summary>
		std::string biosPath;

		/// <summary>A real 256 byte DMG boot ROM (the Game Boy's "BIOS"); empty = the built-in
		/// one. It is used when the monochrome machine runs the cartridge.</summary>
		std::string dmgBiosPath;

		/// <summary>A real 2304 byte CGB boot ROM (the Game Boy Color's "BIOS"); empty = the
		/// built-in one. It is used when a colour machine runs the cartridge.</summary>
		std::string cgbBiosPath;

		bool useCustomBootRom = true;	// run the pureikyubu boot animation
		bool skipBootAnimation = false;	// jump straight to the cartridge after the logo pass
		bool hleBios = true;			// handle the BIOS calls in the host

		// -- video -------------------------------------------------------------------------

		int videoScale = 3;
		bool fullscreen = false;
		bool vsync = true;
		bool integerScale = true;
		bool showFps = true;

		/// <summary>The LCD ghosting effect dmgemu's `lcd_effect` draws: every shown frame is
		/// blended half and half with the one before it. On by default; the setting is the
		/// frontend's, the cores themselves always return a clean frame.</summary>
		bool lcdEffect = true;

		bool frameSkip = false;

		// -- audio -------------------------------------------------------------------------

		bool audioEnabled = true;
		int sampleRate = 32768;
		int volume = 100;

		/// <summary>The DMG/CGB APU's output high pass filter (on by default; see
		/// GbApu::SetHighPassFilter). Off, the sound is the DACs' raw sum, DC offset and all.</summary>
		bool highPassFilter = true;

		// -- input -------------------------------------------------------------------------

		std::vector<GbaKeyBinding> keys;

		// -- link --------------------------------------------------------------------------

		bool linkEnabled = false;		// open the link port (a no-cartridge start is link mode)
		bool linkServer = false;		// listen for another instance instead of connecting
		std::string linkAddress = "127.0.0.1:33333";
		int linkPlayers = 2;

		// -- emulation ---------------------------------------------------------------------

		bool rtcEnabled = true;
		bool bootWithNoCartridge = false;
		bool debugger = false;			// open the debugger window when the machine starts
		std::string saveDirectory;		// empty = the directory of the ROM
		int logLevel = 1;				// 0 = errors, 1 = warnings, 2 = info, 3 = debug

		/// <summary>The built-in defaults (the same values build/Data/DefaultGBASettings.json has).</summary>
		static GbaSettings Defaults();

		/// <summary>Load a settings file; a missing file leaves `out` at the defaults.</summary>
		static bool Load(const std::string& path, GbaSettings& out, std::string* error);

		/// <summary>
		/// Read a settings document and apply the members it names over `out`; a member the
		/// document does not name keeps the value `out` already has. This is how the user's file
		/// is merged over the shipped defaults. A missing file is not an error and leaves `out`
		/// untouched; a document that is refused leaves `out` untouched as well.
		/// </summary>
		static bool Merge(const std::string& path, GbaSettings& out, std::string* error);

		/// <summary>Write the settings back.</summary>
		bool Save(const std::string& path, std::string* error) const;

		/// <summary>Parse a settings document (used by Load and by the tests).</summary>
		static bool Parse(const std::string& text, GbaSettings& out, std::string* error);

		/// <summary>Serialize to the file format (tabs, one member per line, as the other
		/// settings files do).</summary>
		std::string ToJson() const;

		/// <summary>The document as it is shipped in build/Data/DefaultGBASettings.json.</summary>
		static std::string DefaultJson();

		/// <summary>The host key name bound to an action ("" when it is not bound).</summary>
		std::string KeyFor(const std::string& action) const;

		/// <summary>Apply a host key event. Returns the KEY_* bit it changed, or 0.</summary>
		uint16_t KeyBitFor(const std::string& key) const;

		/// <summary>Set one binding (used by the UI).</summary>
		void Bind(const std::string& action, const std::string& key);
	};
}
