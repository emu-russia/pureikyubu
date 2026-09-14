// The GBA emulator's settings, stored in build/Data/GBASettings.json.
//
// The file is the same shape as the rest of the emulator's settings (a flat object per section),
// but this module keeps its own tiny reader/writer: the GBA core must compile and be tested
// without the GameCube side of the emulator (and therefore without its Json/JDI machinery), and
// the settings file is part of what the harness verifies.
//
// The shipped defaults live in build/Data/GBASettings.json. A missing file is not an error: the
// defaults from Defaults() are used and the file is written back on the first change.

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

		std::string biosPath;			// a real 16 KByte BIOS image; empty = the built-in one
		bool useCustomBootRom = true;	// run the pureikyubu boot animation
		bool skipBootAnimation = false;	// jump straight to the cartridge after the logo pass
		bool hleBios = true;			// handle the BIOS calls in the host

		// -- video -------------------------------------------------------------------------

		int videoScale = 3;
		bool fullscreen = false;
		bool vsync = true;
		bool integerScale = true;
		bool showFps = true;
		bool frameSkip = false;

		// -- audio -------------------------------------------------------------------------

		bool audioEnabled = true;
		int sampleRate = 32768;
		int volume = 100;

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
		std::string saveDirectory;		// empty = the directory of the ROM
		int logLevel = 1;				// 0 = errors, 1 = warnings, 2 = info, 3 = debug

		/// <summary>The built-in defaults (the same values build/Data/GBASettings.json has).</summary>
		static GbaSettings Defaults();

		/// <summary>Load a settings file; a missing file leaves `out` at the defaults.</summary>
		static bool Load(const std::string& path, GbaSettings& out, std::string* error);

		/// <summary>Write the settings back.</summary>
		bool Save(const std::string& path, std::string* error) const;

		/// <summary>Parse a settings document (used by Load and by the tests).</summary>
		static bool Parse(const std::string& text, GbaSettings& out, std::string* error);

		/// <summary>Serialize to the file format (tabs, one member per line, as the other
		/// settings files do).</summary>
		std::string ToJson() const;

		/// <summary>The document as it is shipped in build/Data/GBASettings.json.</summary>
		static std::string DefaultJson();

		/// <summary>The host key name bound to an action ("" when it is not bound).</summary>
		std::string KeyFor(const std::string& action) const;

		/// <summary>Apply a host key event. Returns the KEY_* bit it changed, or 0.</summary>
		u16 KeyBitFor(const std::string& key) const;

		/// <summary>Set one binding (used by the UI).</summary>
		void Bind(const std::string& action, const std::string& key);
	};
}
