/*

# The look of the interface

The front end draws itself with ImGui, and ImGui draws itself with the style of its context: the
colours of every widget, the rounding, the paddings and the font. This module is that style, named,
so that the interface has a look of its own instead of the ImGui default one, and so that a look can
be picked in the settings window.

A theme is a palette (the `UiPalette` below). `UiThemeApply` turns it into an `ImGuiStyle`, and the
parts of the interface that draw themselves - the columns of the game selector, the status line and
the mark of the emulator (`UiThemeDrawCube`) - read the same palette, so that there is one
description of the look and not one for the widgets and another for the custom drawing.

The default theme is "Flipper", named after the codename of the console's graphics chip: the dark
ink of the project's site (`docs/assets/style.css`) with the blue of the cube of its logo
(`res/pureikyubu_icon.svg`) for everything that is selected, and the violet of the cube's shaded face
as the second accent. The six themes next to it are hues of the same recipe, and "Daylight" is the
recipe over a light background - the light theme the site has next to its dark one.

The front end applies the theme the configuration asks for once, at startup (`uisdl.cpp`), and the
settings window applies another one when the user picks it; nothing else in the emulator knows that
themes exist.

*/

#pragma once

//! The colours a theme is made of. Everything the interface draws is one of these.
struct UiPalette
{
	ImU32 window;       //!< the background of a window
	ImU32 child;        //!< panels inside a window, and the bands the front end draws itself
	ImU32 surface;      //!< input frames, buttons and every other raised thing
	ImU32 surface2;     //!< the same, hovered
	ImU32 border;       //!< the hairline around everything
	ImU32 text;         //!< the text
	ImU32 muted;        //!< the text that is read second: labels, hints, counters
	ImU32 accent;       //!< the brand colour, and the colour of everything selected
	ImU32 accent2;      //!< the second brand colour, for the few things that have to stand out
	ImU32 selection;    //!< the accent, translucent, for the selected rows and nav items
};

//! How many themes the front end knows about.
int UiThemeCount();

//! Whether the index names a theme.
bool UiThemeValid(int index);

//! The name of the theme at `index`. A theme the build does not have is answered with the default
//! one, so that the answer is always a name that can be stored in the configuration.
const char* UiThemeName(int index);

//! The index of the theme called `name`, or -1 when there is no such theme.
int UiThemeFind(const char* name);

//! The index of the theme that is applied now.
int UiThemeCurrent();

//! Make `index` the theme of the interface: the ImGui style of the current context is rebuilt, and
//! everything drawn after this call (the widgets and the custom drawing alike) uses it.
void UiThemeApply(int index);

//! Apply the theme with this name. A name that is not a theme of this build - an empty one, or one
//! left in the configuration by another build - applies the default theme.
void UiThemeApplyByName(const char* name);

//! The colours of the applied theme.
const UiPalette& UiThemePalette();

//! Whether the applied theme is a dark one. The custom drawing needs it where it leans on the
//! background: a highlight is white over a dark theme and ink over a light one.
bool UiThemeIsDark();

//! The colour as an ImVec4, for the ImGui calls that take one instead of an ImU32.
ImVec4 UiThemeVec4(ImU32 color);

//! The colour at this opacity (0 to 1), for the fills that are a tint of an accent rather than a
//! colour of the palette (the capsule a file type is written in, see uisdl.cpp).
ImU32 UiThemeAlpha(ImU32 color, float alpha);

//! Build the font atlas of the interface.
//!
//! `scale` is how many framebuffer pixels a logical pixel of the interface is: 1.0 on a display that
//! does not scale it, and the 1.25 and 1.5 of a Windows desktop that does. The faces are baked at
//! that size while the interface goes on laying itself out at the logical one, so that a glyph lands
//! on the framebuffer one texel to one pixel - which is what the sharpness of the text is. A face
//! baked at the logical size is handed to the renderer as a texture smaller than the text it draws
//! and scaled up by it, and the renderer scales a texture with linear filtering. Called once, before
//! the first frame.
void UiThemeLoadFonts(float scale = 1.0f);

//! Draw the pureikyubu cube - the mark of the emulator - into a draw list, centered on `center` and
//! `size` pixels wide. It is the logo of the project (`res/pureikyubu_icon.svg`) drawn from the
//! palette, so that the interface is marked by it and not only coloured like it.
void UiThemeDrawCube(ImDrawList* drawList, const ImVec2& center, float size, float alpha = 1.0f);
