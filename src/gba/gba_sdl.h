// The SDL2 frontend of the GBA and Game Boy emulators.
//
// This is the "GBA Link / GBA Player" mode of pureikyubu: the emulator runs the portable machine
// with SDL2 as the backend (window, streaming texture, the sound device with its mixer buffer,
// keyboard and game controller), either on a cartridge given on the command line or, with no
// cartridge at all, on the emulator's own boot ROM, whose SIO link driver initializes the link
// port - which is what a GBA Link peer or a Game Boy Player replacement needs.
//
// Two machines share the frontend because they differ only in the frame size and in the key
// mapping: the GBA (240x160, eleven buttons) and the Game Boy (160x144, eight buttons), both bound
// from the `input` section of the settings files - the Game Boy takes the eight bindings it shares
// with the Game Boy Advance (see GbInput in gba_sdl.cpp). Everything else - the window, the
// texture, the sound buffer, the frame pacing and the hotkeys - is the same code.
//
// The GameCube side of the emulator is not involved at all (the two machines share the executable
// and nothing else).

#pragma once

#include <string>

#include "gba_settings.h"

namespace GBA
{
	/// <summary>True when the file name has a Game Boy / Game Boy Advance cartridge extension.</summary>
	bool IsGameBoyImage(const std::string& path);

	/// <summary>True when the cartridge is a Game Boy one (.gb/.gbc/.sgb/.dmg/.cgb), not a GBA one.</summary>
	bool IsDmgImage(const std::string& path);

	/// <summary>
	/// The two documents the settings of the portable machines come from: the shipped defaults and
	/// the user's own values, which are merged over the defaults member by member. The two are a
	/// pair of one directory (the user's file sits next to the defaults), and the settings window
	/// writes `user` when Save is asked for.
	/// </summary>
	struct GbaSettingsFiles
	{
		std::string defaults;	//!< the shipped defaults (Data/DefaultGBASettings.json)
		std::string user;		//!< the user's own values (Data/GBASettings.json)
	};

	/// <summary>
	/// Where the two files are: `Data/` of the working directory (the shipped build runs there) or
	/// the `build/Data/` of a run from the repository root. With no shipped file anywhere the
	/// working directory's names are answered, so that a save still has a place to write to.
	/// </summary>
	GbaSettingsFiles FindSettingsFiles();

	/// <summary>
	/// Load the settings of the portable machines: the shipped defaults, then the members the
	/// user's file names over them. A file that is not there is not an error - the built-in
	/// defaults stand in for a shipped one, and a machine that has never saved a user's file runs
	/// them - so the caller is left with a configuration it can run in every case. The answer is
	/// false only for a file that exists and could not be read.
	/// </summary>
	bool LoadSettings(const GbaSettingsFiles& files, GbaSettings& settings, std::string* error);

	/// <summary>
	/// Run the GBA emulator with the SDL2 backend until the user closes the window.
	/// </summary>
	/// <param name="romPath">The cartridge to run, or an empty string for the no-cartridge (link) mode.</param>
	/// <param name="linkMode">Force the link mode even when a cartridge is loaded.</param>
	/// <param name="settings">The settings to run with (already merged with the command line).</param>
	/// <returns>The process exit code (0 on a clean exit, non-zero when SDL or the ROM failed).</returns>
	int RunSdlFrontend(const std::string& romPath, bool linkMode, const GbaSettings& settings);

	/// <summary>
	/// Run the Game Boy (DMG/CGB) emulator with the SDL2 backend. The GBA settings supply the
	/// window and audio options and the keys: the Game Boy takes the eight bindings it shares with
	/// the Game Boy Advance from the `input` section. The console kind follows the cartridge's CGB
	/// flag and `forceDmg`.
	/// </summary>
	int RunSdlFrontendGb(const std::string& romPath, const GbaSettings& settings, bool forceDmg);
}
