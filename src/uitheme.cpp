// The look of the interface. The module description is in uitheme.h.

#include "pch.h"
#include "uitheme.h"

// ---------------------------------------------------------------------------
// The themes
//
// A theme is data: the palette the widgets, the custom drawing and the fonts are made of.
//
// The default one is "Flipper", named after the codename of the console's graphics chip: the dark
// ink of the project's site (`docs/assets/style.css`) with the blue of the cube of its logo
// (`res/pureikyubu_icon.svg`) for everything that is selected, and the violet of the cube's shaded
// face as the second accent. The themes next to it are hues of the same recipe, and "Daylight" is
// the recipe over a light background, the way the site has a light theme next to its dark one.
//
// `dark` is not decoration: the states a widget has (hovered, held, disabled) are the colours of the
// theme moved in brightness, and which way that is depends on the background the theme is drawn
// over. The helpers below - StandOut and Recede - read the flag, so a theme only has to name the
// colours its widgets rest in.
//
// The palette is stored as ImU32, which is what a draw list takes; the ImGui style wants ImVec4, and
// the two are converted where they meet.
//
// The name of a theme is one word: it is stored in the configuration as a string and travels to the
// debug interface, whose commands are separated by spaces (see JdiClient::SetConfigString).

static const struct
{
	const char* name;
	bool        dark;
	UiPalette   colors;
} UiThemes[] =
{
	// window, child, surface, surface2, border, text, muted, accent, accent2, selection

	{
		"Flipper", true,		// the site and the cube of the logo: dark blue ink, blue and violet
		{
			IM_COL32(0x0e, 0x10, 0x18, 0xff),
			IM_COL32(0x0b, 0x0d, 0x15, 0xff),
			IM_COL32(0x19, 0x1d, 0x30, 0xff),
			IM_COL32(0x23, 0x28, 0x43, 0xff),
			IM_COL32(0x2b, 0x31, 0x50, 0xff),
			IM_COL32(0xe9, 0xec, 0xf6, 0xff),
			IM_COL32(0x9a, 0xa2, 0xc0, 0xff),
			IM_COL32(0x5e, 0xa8, 0xff, 0xff),
			IM_COL32(0x7a, 0x5c, 0xff, 0xff),
			IM_COL32(0x5e, 0xa8, 0xff, 0x30),
		}
	},

	{
		"Nebula", true,			// violet, with the blue of the theme it comes from
		{
			IM_COL32(0x10, 0x0e, 0x1c, 0xff),
			IM_COL32(0x0c, 0x0a, 0x17, 0xff),
			IM_COL32(0x1d, 0x19, 0x32, 0xff),
			IM_COL32(0x28, 0x23, 0x49, 0xff),
			IM_COL32(0x35, 0x2e, 0x58, 0xff),
			IM_COL32(0xec, 0xe8, 0xf8, 0xff),
			IM_COL32(0xa4, 0x9c, 0xc4, 0xff),
			IM_COL32(0xa0, 0x7c, 0xff, 0xff),
			IM_COL32(0x5e, 0xa8, 0xff, 0xff),
			IM_COL32(0xa0, 0x7c, 0xff, 0x30),
		}
	},

	{
		"Emerald", true,		// green and teal
		{
			IM_COL32(0x0a, 0x13, 0x10, 0xff),
			IM_COL32(0x07, 0x10, 0x0d, 0xff),
			IM_COL32(0x14, 0x26, 0x1f, 0xff),
			IM_COL32(0x1d, 0x35, 0x2b, 0xff),
			IM_COL32(0x27, 0x48, 0x3a, 0xff),
			IM_COL32(0xe5, 0xf2, 0xec, 0xff),
			IM_COL32(0x8f, 0xb3, 0xa5, 0xff),
			IM_COL32(0x3f, 0xd3, 0x9b, 0xff),
			IM_COL32(0x56, 0xb8, 0xff, 0xff),
			IM_COL32(0x3f, 0xd3, 0x9b, 0x30),
		}
	},

	{
		"Crimson", true,		// red, with amber next to it
		{
			IM_COL32(0x15, 0x0d, 0x10, 0xff),
			IM_COL32(0x10, 0x09, 0x0c, 0xff),
			IM_COL32(0x26, 0x18, 0x1d, 0xff),
			IM_COL32(0x34, 0x22, 0x29, 0xff),
			IM_COL32(0x4b, 0x30, 0x38, 0xff),
			IM_COL32(0xf4, 0xe8, 0xeb, 0xff),
			IM_COL32(0xc0, 0x9a, 0xa2, 0xff),
			IM_COL32(0xff, 0x6b, 0x7f, 0xff),
			IM_COL32(0xff, 0xab, 0x4d, 0xff),
			IM_COL32(0xff, 0x6b, 0x7f, 0x30),
		}
	},

	{
		"Amber", true,			// warm brown, the colour of the old console lighting
		{
			IM_COL32(0x15, 0x11, 0x0b, 0xff),
			IM_COL32(0x10, 0x0d, 0x08, 0xff),
			IM_COL32(0x26, 0x1d, 0x13, 0xff),
			IM_COL32(0x34, 0x29, 0x1b, 0xff),
			IM_COL32(0x4b, 0x3a, 0x26, 0xff),
			IM_COL32(0xf6, 0xef, 0xe3, 0xff),
			IM_COL32(0xc2, 0xab, 0x8f, 0xff),
			IM_COL32(0xff, 0xab, 0x4d, 0xff),
			IM_COL32(0x6e, 0xc8, 0xff, 0xff),
			IM_COL32(0xff, 0xab, 0x4d, 0x30),
		}
	},

	{
		"Graphite", true,		// the neutral one: both accents are grey, one cool and one warm
		{
			IM_COL32(0x10, 0x11, 0x13, 0xff),
			IM_COL32(0x0c, 0x0d, 0x0f, 0xff),
			IM_COL32(0x1b, 0x1d, 0x21, 0xff),
			IM_COL32(0x26, 0x29, 0x2e, 0xff),
			IM_COL32(0x34, 0x38, 0x3e, 0xff),
			IM_COL32(0xe8, 0xea, 0xed, 0xff),
			IM_COL32(0x9b, 0xa3, 0xad, 0xff),
			IM_COL32(0x9f, 0xb6, 0xd4, 0xff),
			IM_COL32(0xc0, 0xa9, 0x8f, 0xff),
			IM_COL32(0x9f, 0xb6, 0xd4, 0x2e),
		}
	},

	{
		"Sakura", true,			// rose, for the machines that came from Japan
		{
			IM_COL32(0x15, 0x0f, 0x13, 0xff),
			IM_COL32(0x10, 0x0b, 0x0f, 0xff),
			IM_COL32(0x25, 0x19, 0x20, 0xff),
			IM_COL32(0x33, 0x23, 0x2c, 0xff),
			IM_COL32(0x4a, 0x33, 0x40, 0xff),
			IM_COL32(0xf5, 0xe9, 0xee, 0xff),
			IM_COL32(0xc1, 0x9d, 0xaf, 0xff),
			IM_COL32(0xff, 0x85, 0xab, 0xff),
			IM_COL32(0x7f, 0xd6, 0xff, 0xff),
			IM_COL32(0xff, 0x85, 0xab, 0x30),
		}
	},

	{
		"Daylight", false,		// the light half of the site: paper, ink, the same two accents
		{
			IM_COL32(0xf5, 0xf7, 0xfc, 0xff),
			IM_COL32(0xeb, 0xef, 0xf8, 0xff),
			IM_COL32(0xff, 0xff, 0xff, 0xff),
			IM_COL32(0xe9, 0xed, 0xf8, 0xff),
			IM_COL32(0xd9, 0xdf, 0xee, 0xff),
			IM_COL32(0x16, 0x1a, 0x28, 0xff),
			IM_COL32(0x5b, 0x64, 0x7f, 0xff),
			IM_COL32(0x1d, 0x63, 0xd2, 0xff),
			IM_COL32(0x60, 0x38, 0xd6, 0xff),
			IM_COL32(0x1d, 0x63, 0xd2, 0x1a),
		}
	},
};

static int currentTheme = 0;

//! The size of the interface font, in pixels. ImGui draws at the size the atlas was baked at, so
//! this is the whole of the interface's text scale.
static const float UiFontSize = 16.0f;

//! The theme the index names, or the default one (the answers of this module never fail, so that a
//! theme a configuration asks for and this build does not have cannot leave the interface unstyled).
static const auto& ThemeAt(int index)
{
	return UiThemes[UiThemeValid(index) ? index : 0];
}

/* The colour moved towards the light (amount > 0) or towards the dark (amount < 0). The alpha is
   scaled by `alpha`, which is how the pieces of the interface that fade themselves in and out are
   drawn. */
static ImU32 Toward(ImU32 color, float amount, float alpha = 1.0f)
{
	ImVec4 c = ImGui::ColorConvertU32ToFloat4(color);

	if (amount >= 0.0f)
	{
		c.x += (1.0f - c.x) * amount;
		c.y += (1.0f - c.y) * amount;
		c.z += (1.0f - c.z) * amount;
	}
	else
	{
		float k = 1.0f + amount;
		c.x *= k;
		c.y *= k;
		c.z *= k;
	}

	c.w *= alpha;

	return ImGui::ColorConvertFloat4ToU32(c);
}

/* The colour of a widget that is hovered, held or otherwise more prominent than it rests: brighter
   over a dark theme, deeper over a light one. */
static ImU32 StandOut(ImU32 color, float amount, bool dark)
{
	return Toward(color, dark ? amount : -amount);
}

/* The same for a widget that has to recede, which is what a disabled one does. */
static ImU32 Recede(ImU32 color, float amount, bool dark)
{
	return Toward(color, dark ? -amount : amount);
}

/* The colour at another opacity. A selection is the accent, translucent, and the states of a
   selection (a hovered row, a held menu item) are the same colour with more of it. */
static ImU32 Stronger(ImU32 color, float scale)
{
	ImVec4 c = ImGui::ColorConvertU32ToFloat4(color);

	c.w = (c.w * scale < 1.0f) ? (c.w * scale) : 1.0f;

	return ImGui::ColorConvertFloat4ToU32(c);
}

static void SetStyleColor(ImGuiStyle& style, ImGuiCol which, ImU32 color)
{
	style.Colors[which] = ImGui::ColorConvertU32ToFloat4(color);
}

static bool FileExists(const char* path)
{
	FILE* f = fopen(path, "rb");

	if (f == nullptr)
	{
		return false;
	}

	fclose(f);

	return true;
}

int UiThemeCount()
{
	return (int)(sizeof(UiThemes) / sizeof(UiThemes[0]));
}

bool UiThemeValid(int index)
{
	return index >= 0 && index < UiThemeCount();
}

const char* UiThemeName(int index)
{
	return ThemeAt(index).name;
}

int UiThemeFind(const char* name)
{
	if (name == nullptr)
	{
		return -1;
	}

	for (int i = 0; i < UiThemeCount(); i++)
	{
		if (strcmp(UiThemes[i].name, name) == 0)
		{
			return i;
		}
	}

	return -1;
}

int UiThemeCurrent()
{
	return currentTheme;
}

const UiPalette& UiThemePalette()
{
	return ThemeAt(currentTheme).colors;
}

bool UiThemeIsDark()
{
	return ThemeAt(currentTheme).dark;
}

ImVec4 UiThemeVec4(ImU32 color)
{
	return ImGui::ColorConvertU32ToFloat4(color);
}

ImU32 UiThemeAlpha(ImU32 color, float alpha)
{
	ImVec4 c = ImGui::ColorConvertU32ToFloat4(color);

	c.w = alpha;

	return ImGui::ColorConvertFloat4ToU32(c);
}

void UiThemeApply(int index)
{
	if (!UiThemeValid(index))
	{
		index = 0;
	}

	currentTheme = index;

	const bool dark = UiThemes[index].dark;
	ImGuiStyle& style = ImGui::GetStyle();
	const UiPalette& c = UiThemes[index].colors;

	// The shape of the widgets: the corners are rounded and the frames are outlined, which is the
	// language of the site (cards of 10 to 14 pixels of radius over a hairline) and not the square
	// and flat one ImGui is set in.

	style.WindowRounding = 10.0f;
	style.ChildRounding = 9.0f;
	style.FrameRounding = 8.0f;
	style.PopupRounding = 10.0f;
	style.ScrollbarRounding = 8.0f;
	style.GrabRounding = 8.0f;
	style.TabRounding = 8.0f;

	style.WindowBorderSize = 1.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupBorderSize = 1.0f;
	style.FrameBorderSize = 1.0f;
	style.TabBorderSize = 0.0f;

	style.WindowPadding = ImVec2(16.0f, 14.0f);
	style.FramePadding = ImVec2(11.0f, 6.0f);
	style.CellPadding = ImVec2(8.0f, 7.0f);
	style.ItemSpacing = ImVec2(10.0f, 8.0f);
	style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
	style.IndentSpacing = 22.0f;
	style.ScrollbarSize = 13.0f;
	style.GrabMinSize = 12.0f;

	style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
	style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
	style.SeparatorTextAlign = ImVec2(0.0f, 0.5f);
	style.SeparatorTextBorderSize = 1.0f;
	style.SeparatorTextPadding = ImVec2(16.0f, style.FramePadding.y);

	style.AntiAliasedLines = true;
	style.AntiAliasedFill = true;

	// The colours. The states a widget has are the resting colour moved in brightness (StandOut,
	// Recede) or a selection with more of it (Stronger), so a theme names a handful of colours and
	// gets the rest of the interface for free.

	SetStyleColor(style, ImGuiCol_Text, c.text);
	SetStyleColor(style, ImGuiCol_TextDisabled, Recede(c.muted, 0.22f, dark));
	SetStyleColor(style, ImGuiCol_WindowBg, c.window);
	SetStyleColor(style, ImGuiCol_ChildBg, c.child);
	SetStyleColor(style, ImGuiCol_PopupBg, c.surface);
	SetStyleColor(style, ImGuiCol_Border, c.border);
	SetStyleColor(style, ImGuiCol_BorderShadow, IM_COL32(0, 0, 0, 0));
	SetStyleColor(style, ImGuiCol_FrameBg, c.surface);
	SetStyleColor(style, ImGuiCol_FrameBgHovered, c.surface2);
	SetStyleColor(style, ImGuiCol_FrameBgActive, StandOut(c.surface, 0.10f, dark));
	SetStyleColor(style, ImGuiCol_TitleBg, c.child);
	SetStyleColor(style, ImGuiCol_TitleBgActive, c.child);
	SetStyleColor(style, ImGuiCol_TitleBgCollapsed, c.window);
	SetStyleColor(style, ImGuiCol_MenuBarBg, c.window);
	SetStyleColor(style, ImGuiCol_ScrollbarBg, IM_COL32(0, 0, 0, 0));
	SetStyleColor(style, ImGuiCol_ScrollbarGrab, c.border);
	SetStyleColor(style, ImGuiCol_ScrollbarGrabHovered, StandOut(c.border, 0.18f, dark));
	SetStyleColor(style, ImGuiCol_ScrollbarGrabActive, c.accent);
	SetStyleColor(style, ImGuiCol_CheckMark, c.accent);
	SetStyleColor(style, ImGuiCol_SliderGrab, c.accent);
	SetStyleColor(style, ImGuiCol_SliderGrabActive, StandOut(c.accent, 0.20f, dark));
	SetStyleColor(style, ImGuiCol_Button, c.surface2);
	SetStyleColor(style, ImGuiCol_ButtonHovered, StandOut(c.surface2, 0.10f, dark));
	SetStyleColor(style, ImGuiCol_ButtonActive, StandOut(c.surface2, 0.20f, dark));
	SetStyleColor(style, ImGuiCol_Header, c.selection);
	SetStyleColor(style, ImGuiCol_HeaderHovered, Stronger(c.selection, 1.7f));
	SetStyleColor(style, ImGuiCol_HeaderActive, Stronger(c.selection, 2.4f));
	SetStyleColor(style, ImGuiCol_Separator, c.border);
	SetStyleColor(style, ImGuiCol_SeparatorHovered, StandOut(c.border, 0.20f, dark));
	SetStyleColor(style, ImGuiCol_SeparatorActive, c.accent);
	SetStyleColor(style, ImGuiCol_ResizeGrip, StandOut(c.border, 0.05f, dark));
	SetStyleColor(style, ImGuiCol_ResizeGripHovered, c.accent);
	SetStyleColor(style, ImGuiCol_ResizeGripActive, StandOut(c.accent, 0.15f, dark));
	SetStyleColor(style, ImGuiCol_Tab, c.child);
	SetStyleColor(style, ImGuiCol_TabHovered, c.accent2);
	SetStyleColor(style, ImGuiCol_TabActive, c.surface);
	SetStyleColor(style, ImGuiCol_TabUnfocused, c.child);
	SetStyleColor(style, ImGuiCol_TabUnfocusedActive, c.surface);
	SetStyleColor(style, ImGuiCol_PlotLines, c.accent);
	SetStyleColor(style, ImGuiCol_PlotLinesHovered, c.accent2);
	SetStyleColor(style, ImGuiCol_PlotHistogram, c.accent);
	SetStyleColor(style, ImGuiCol_PlotHistogramHovered, c.accent2);
	SetStyleColor(style, ImGuiCol_TableHeaderBg, c.child);
	SetStyleColor(style, ImGuiCol_TableBorderStrong, c.border);
	SetStyleColor(style, ImGuiCol_TableBorderLight, StandOut(c.window, 0.06f, dark));
	SetStyleColor(style, ImGuiCol_TableRowBg, IM_COL32(0, 0, 0, 0));
	SetStyleColor(style, ImGuiCol_TableRowBgAlt, UiThemeAlpha(c.muted, 0.10f));
	SetStyleColor(style, ImGuiCol_TextSelectedBg, c.selection);
	SetStyleColor(style, ImGuiCol_DragDropTarget, c.accent);
	SetStyleColor(style, ImGuiCol_NavHighlight, c.accent2);
	SetStyleColor(style, ImGuiCol_NavWindowingHighlight, UiThemeAlpha(c.text, 0.80f));

	// The veil over everything but a modal window: ink over a dark theme, and only a shade of slate
	// over a light one, where a black veil would read as a broken picture.
	SetStyleColor(style, ImGuiCol_NavWindowingDimBg, dark ? IM_COL32(0x05, 0x06, 0x0b, 0x88) : IM_COL32(0x2b, 0x33, 0x4a, 0x44));
	SetStyleColor(style, ImGuiCol_ModalWindowDimBg, dark ? IM_COL32(0x05, 0x06, 0x0b, 0xa8) : IM_COL32(0x2b, 0x33, 0x4a, 0x66));
}

void UiThemeApplyByName(const char* name)
{
	int index = UiThemeFind(name);

	UiThemeApply(UiThemeValid(index) ? index : 0);
}

void UiThemeLoadFonts()
{
	ImGuiIO& io = ImGui::GetIO();

	// The interface is set in a proportional face, the way the site is set in Segoe UI, and the
	// faces below are the ones the systems have. The first of them is the very face the site asks
	// for; a system with none of them keeps the font ImGui has built in.
	//
	// The ranges include Cyrillic: the paths, the file names and the labels of a user can be in it,
	// and the built-in font cannot draw a single one of those characters.
	static const char* faces[] =
	{
		"C:/Windows/Fonts/segoeui.ttf",						// Windows
		"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",	// Debian and Ubuntu
		"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
		"/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
		"/usr/share/fonts/TTF/DejaVuSans.ttf",				// Arch
	};

	bool loaded = false;

	for (const char* face : faces)
	{
		// The file is looked at before the atlas is asked to read it: a face that is not there
		// would otherwise trip the assertion of ImFontAtlas in a debug build.
		if (!FileExists(face))
		{
			continue;
		}

		if (io.Fonts->AddFontFromFileTTF(face, UiFontSize, nullptr, io.Fonts->GetGlyphRangesCyrillic()) != nullptr)
		{
			loaded = true;
			break;
		}
	}

	if (!loaded)
	{
		io.Fonts->AddFontDefault();
	}

	// Japanese is merged into the face above rather than set in a face of its own: the name and the
	// comments of a Japanese disk are kana and kanji (they are decoded from the SJIS of the banner,
	// see SjisToWstring in uisdl.cpp), and not one of the proportional faces has such a glyph - the
	// whole row of the selector would come out blank without this. The ranges are ImGui's list of
	// the 2999 ideographs of the Joyo and Jinmeiyo sets, which is what a title of a game is made of.
	//
	// A system without a Japanese face of its own shows the titles the way it did before.
	static const char* japaneseFaces[] =
	{
		"C:/Windows/Fonts/meiryo.ttc",						// Windows: Meiryo
		"C:/Windows/Fonts/YuGothR.ttc",						// Windows 10: Yu Gothic
		"C:/Windows/Fonts/YuGothM.ttc",
		"C:/Windows/Fonts/msgothic.ttc",					// Windows: MS Gothic
		"C:/Windows/Fonts/msmincho.ttc",
		"C:/Windows/Fonts/BIZ-UDGothicR.ttc",
		"C:/Windows/Fonts/UDDigiKyokashoN-R.ttc",
		"C:/Windows/Fonts/arialuni.ttf",					// Arial Unicode MS, where it is installed
		"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",	// Debian and Ubuntu
		"/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
		"/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
		"/usr/share/fonts/truetype/fonts-japanese-gothic.ttf",		// VL Gothic
		"/usr/share/fonts/opentype/vlgothic/VL-Gothic-Regular.ttf",
		"/usr/share/fonts/opentype/ipafont-gothic/ipag.ttf",		// IPA Gothic
		"/usr/share/fonts/truetype/takao-gothic/TakaoPGothic.ttf",
		"/usr/share/fonts/truetype/droid/DroidSansFallback.ttf",	// the fallback of a minimal Debian
		"/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
	};

	for (const char* face : japaneseFaces)
	{
		if (!FileExists(face))
		{
			continue;
		}

		ImFontConfig config;
		config.MergeMode = true;

		if (io.Fonts->AddFontFromFileTTF(face, UiFontSize, &config, io.Fonts->GetGlyphRangesJapanese()) != nullptr)
		{
			break;
		}
	}
}

void UiThemeDrawCube(ImDrawList* drawList, const ImVec2& center, float size, float alpha)
{
	if (drawList == nullptr || size <= 0.0f)
	{
		return;
	}

	const UiPalette& c = UiThemePalette();
	const bool dark = UiThemeIsDark();

	// The vertices of the mark, in the coordinates of the logo, placed around its own center. The
	// cube is seen a little from above: `top` is the far corner of the top face, `middle` is the
	// near one, and the two sides drop from its left and its right corner.
	const float k = size / 94.0f;

	auto point = [&](float x, float y)
	{
		return ImVec2(center.x + (x - 50.0f) * k, center.y + (y - 46.0f) * k);
	};

	const ImVec2 top = point(50, 2);
	const ImVec2 right = point(97, 26);
	const ImVec2 middle = point(50, 50);
	const ImVec2 left = point(3, 26);
	const ImVec2 lowerLeft = point(3, 66);
	const ImVec2 lowerRight = point(97, 66);
	const ImVec2 bottom = point(50, 90);

	// The three faces are one colour at three brightnesses: this is what the gradients of the logo
	// read as when they are drawn flat. A light theme has to keep them from washing out, so its top
	// face is only a little lighter than the accent.
	const float lift = dark ? 0.34f : 0.10f;
	const float side = dark ? -0.40f : -0.46f;

	drawList->AddQuadFilled(top, right, middle, left, Toward(c.accent, lift, alpha));
	drawList->AddQuadFilled(left, middle, bottom, lowerLeft, Toward(c.accent, 0.0f, alpha));
	drawList->AddQuadFilled(middle, right, lowerRight, bottom, Toward(c.accent, side, alpha));

	// The light along the two near edges of the top face, as in the logo: white over a dark theme
	// and ink over a light one, where white would not be seen at all.
	const ImVec2 ridge[3] = { left, middle, right };
	const float thickness = (size * 0.02f > 1.0f) ? (size * 0.02f) : 1.0f;
	const ImU32 edge = dark ? IM_COL32(0xff, 0xff, 0xff, (ImU32)(0.35f * alpha * 255.0f))
							: IM_COL32(0x00, 0x00, 0x00, (ImU32)(0.20f * alpha * 255.0f));

	drawList->AddPolyline(ridge, 3, edge, ImDrawFlags_None, thickness);
}
