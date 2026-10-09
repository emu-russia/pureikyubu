/*

# The settings window of the front end

The emulator has one window for its settings: a vertical strip of tabs on the left and a property
grid on the right (see the "Settings" module comment in uisettings.cpp). This header is the whole
interface of that window; it is a module of its own, so that the front end (`uisdl.cpp`) only has to
draw it and hand it the events it needs.

The two directions of the interface are:

  * the front end draws the window (`UiSettingsFrame`), opens it (`UiSettingsOpen`) and hands it the
    SDL events of a binding capture (`UiSettingsSdlEvent`);
  * the window edits the settings of the emulator through the modules that own them - the
    configuration, the peripheral pool (see peripherals.h) and the selector view below.

*/

#pragma once

//! Open the settings window (the "Options -> Settings..." menu item).
void UiSettingsOpen();

//! Draw the settings window. Called once a frame, from the thread that runs the user interface.
void UiSettingsFrame();

//! Hand an SDL event to the binding capture of the window, before it reaches ImGui. Returns true
//! when the event belongs to the capture, so that it cannot also move the selector cursor or
//! trigger a menu item.
bool UiSettingsSdlEvent(const SDL_Event& event, uint32_t mainWindowID);

//! Whether a binding capture is waiting for an input. The front end keeps its own shortcuts (F3)
//! away from the window while it is.
bool UiSettingsCaptureActive();

// ---------------------------------------------------------------------------
// The game selector, as the "General" page of the window sees it
//
// The selector is a view of the file system, and the front end owns it (uisdl.cpp); the page only
// edits the settings of that view, so that this module does not have to know how the selector
// draws itself.

//! What the selector does with the transparency of a DVD banner (issue #112). The banners carry an
//! alpha channel, which the front end has always blended with the row behind it; a user who wants
//! the picture of a disc to be opaque paints the see-through texels over with one colour instead.
enum class SELECTOR_BANNER_BG
{
	Preserve = 0,   //!< keep the alpha channel: the banner is blended with what is behind it
	Fill,           //!< paint the see-through texels over with the colour below
};

struct SelectorSettings
{
	bool                      active;       //!< whether the selector is shown when no game runs
	bool                      smallIcons;   //!< half size banners in the list
	SELECTOR_SORT             sortBy;       //!< the sort rule (see SELECTOR_SORT in ui.h)
	SELECTOR_BANNER_BG        bannerBg;     //!< what the banner background is (issue #112)
	ImU32                     bannerBgColor; //!< ... the colour of it, as ImGui packs a colour
	std::vector<std::wstring> paths;        //!< the directories that are scanned
};

//! The current state of the selector view.
SelectorSettings SelectorGetSettings();

//! Write the state back: the enabled flag, the icons, the sort rule and the banner background.
void SelectorSetSettings(const SelectorSettings& settings);

//! Take a directory into the list of the scanned ones.
void SelectorAddPath(const std::wstring& path);

//! Drop a directory from the list.
void SelectorRemovePath(const std::wstring& path);

//! Rescan the directories at the next frame.
void SelectorRescan();
