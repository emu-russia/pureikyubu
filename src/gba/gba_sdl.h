// The SDL2 frontend of the GBA and Game Boy emulators.
//
// This is the "GBA Link / GBA Player" mode of pureikyubu: the emulator runs the portable machine
// with SDL2 as the backend (window, streaming texture, audio queue, keyboard and game controller),
// either on a cartridge given on the command line or, with no cartridge at all, on the emulator's
// own boot ROM, whose SIO link driver initializes the link port - which is what a GBA Link peer or
// a Game Boy Player replacement needs.
//
// Two machines share the frontend because they differ only in the frame size and in the key
// mapping: the GBA (240x160, eleven buttons, the bindings from GBASettings.json) and the Game Boy
// (160x144, eight buttons, the fixed bindings documented in wiki/gba.md). Everything else - the
// window, the texture, the audio queue, the frame pacing and the hotkeys - is the same code.
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

	/// <summary>True when the cartridge is a Game Boy one (.gb/.gbc/.sgb), not a GBA one.</summary>
	bool IsDmgImage(const std::string& path);

	/// <summary>
	/// The settings file the frontend reads. `Data/GBASettings.json` relative to the working
	/// directory is what the shipped build uses; a missing file means "the built-in defaults".
	/// </summary>
	const char* DefaultSettingsPath();

	/// <summary>
	/// Load the GBA settings, trying the shipped locations in turn (the application runs with
	/// `Data/` in its working directory; a developer build often runs from the repository root).
	/// </summary>
	bool LoadSettings(const std::string& path, GbaSettings& settings, std::string& usedPath, std::string* error);

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
	/// window and audio options; the console kind follows the cartridge's CGB flag and
	/// `forceCgb`.
	/// </summary>
	int RunSdlFrontendGb(const std::string& romPath, const GbaSettings& settings, bool forceDmg);
}
