/*

# The stand-alone GBA settings window

The window that edits the settings of the Game Boy Advance the emulator runs by itself (`--gba`),
which live in `Data/DefaultGBASettings.json` and the user's `Data/GBASettings.json` (see the module
comment in uisettingsgba.cpp). It is a module of its own, so the front end only has to draw it, open
it from its menu and hand it the SDL events of a key capture.

*/

#pragma once

//! Open the window (the "Options -> Stand-alone GBA..." menu item). The settings are read the
//! first time the window is opened.
void UiGbaSettingsOpen();

//! Draw the window. Called once a frame, from the thread that runs the user interface.
void UiGbaSettingsFrame();

//! Hand an SDL event to the key capture of the window, before it reaches ImGui. Returns true when
//! the event belongs to the capture.
bool UiGbaSettingsSdlEvent(const SDL_Event& event, uint32_t mainWindowID);

//! Whether a key capture is waiting for an input. The front end keeps its own shortcuts away from
//! the window while it is.
bool UiGbaSettingsCaptureActive();
