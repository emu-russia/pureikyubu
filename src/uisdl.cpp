// Portable UI based on SDL2+imgui
#include "pch.h"
#ifdef _WINDOWS
#include <SDL_syswm.h>
#endif
#include "../thirdparty/imgui-filebrowser/imfilebrowser.h"
#include <codecvt>
#include <locale>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <chrono>
#include "res/pureikyubu_icon.h"
#include "res/portable_banners.h"
#include "bench.h"
#include "uisettings.h"
#include "uisettingsgba.h"
#include "gba/gba_sdl.h"
#include "uitheme.h"

static bool ui_active = false;
static bool show_demo_window = false;
static SDL_Window* window;
static SDL_Window* render_target;
static SDL_Renderer* renderer;
static bool draw_error_box = false;
static bool draw_message_box = false;
static std::string error_text;
static std::string message_text;
static ImGui::FileBrowser fileOpenDialog(ImGuiFileBrowserFlags_CloseOnEsc);
static ImGui::FileBrowser chooseDirectoryDialog(ImGuiFileBrowserFlags_SelectDirectory);
static bool draw_about_box = false;
static bool ui_insert_dvd_menu_item_enabled = false;

/* Show a message to the user in the modal box the core uses for its own errors (`UIError`). The
   status bar is not an option for anything that has to be read: the performance thread owns it and
   rewrites all four of its fields once a second (PerfMetrics::PerfThreadProc), so a message put
   there is gone before a user who was looking at the game can read it. */
static void ui_report_error(const std::string& text)
{
	error_text = text;
	draw_error_box = true;
}

/* What a file the front end's browsers returned is for. The browsers of the settings window (a card
   image, a firmware image, a directory the selector scans) are the window's own, see uisettings.cpp. */
enum class FileReaction
{
	None = 0,
	OpenFile_LoadFile,
	ChooseDirectory_MountSdk,
	ChooseFile_DVDImage,
};
static FileReaction file_reaction = FileReaction::None;

/* The type filters of the file browser, per dialog. The portable cartridges of issue #468 are not
   here: File -> Open loads a file into the console, and a cartridge of a portable machine is not a
   console image (the game selector is where those are started from). */
static const std::vector<std::string> selector_file_filters = { ".dol", ".elf", ".gcm", ".iso", ".rvz", ".map", ".json", ".bin" };
static const std::vector<std::string> dvd_image_filters = { ".gcm", ".iso", ".rvz", ".*" };

/* Open the file browser for one reaction of the UI. The title and the filter belong to the dialog
   that asked for it, because one browser serves them all. */
static void open_file_dialog(FileReaction reaction, const char* title, const std::vector<std::string>& filters)
{
	file_reaction = reaction;
	fileOpenDialog.SetTitle(title);
	fileOpenDialog.SetTypeFilters(filters);
	fileOpenDialog.Open();
}

static uint16_t* SjisToUnicode(wchar_t* sjisText, size_t* size, size_t* chars)
{
	uint16_t* unicodeText, * ptrU, uchar, schar;
	wchar_t* ptrS;

	*size = (wcslen(sjisText) + 1) * sizeof(wchar_t);
	unicodeText = (uint16_t*)malloc(*size);

	if (unicodeText == nullptr)
	{
		*chars = 0;
		return nullptr;
	}

	memset(unicodeText, 0, *size);

	ptrU = unicodeText;
	ptrS = sjisText;
	*chars = 0;

	schar = *ptrS;
	while (schar != 0)
	{
		uchar = SjisTable[schar & 0xFFFF];
		if (uchar == 0xFFFF)
		{
			// Two-byte sequence. The second byte has to be there: a title that ends with a lead
			// byte used to consume the terminator and then keep reading past the string.
			wchar_t next = ptrS[1];
			if (next == 0)
			{
				break;
			}

			ptrS++;
			schar = (schar << 8) | (uint16_t)next;
			uchar = SjisTable[schar & 0xFFFF];
		}
		*ptrU = uchar;

		ptrU++;
		ptrS++;
		(*chars)++;
		schar = *ptrS;
	}
	return unicodeText;
}

static void ui_draw_error_box(bool* enabled)
{
	ImGui::OpenPopup("Error");
	if (ImGui::BeginPopupModal("Error", enabled, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text(error_text.c_str());
		ImGui::Separator();

		if (ImGui::Button("OK", ImVec2(120, 0))) { 
			*enabled = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::EndPopup();
	}
}

static void ui_draw_message_box(bool* enabled)
{
	ImGui::OpenPopup("Report");
	if (ImGui::BeginPopupModal("Report", enabled, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text(message_text.c_str());
		ImGui::Separator();

		if (ImGui::Button("OK", ImVec2(120, 0))) {
			*enabled = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::EndPopup();
	}
}

static void ui_draw_about_box(bool* enabled)
{
	ImGui::OpenPopup("About");
	if (ImGui::BeginPopupModal("About", enabled, ImGuiWindowFlags_AlwaysAutoResize))
	{
#ifdef _DEBUG
		auto version = L"Debug";
#else
		auto version = L"Release";
#endif

#if _M_X64
		auto platform = L"x64";
#else
		auto platform = L"x86";
#endif

		auto jitc = UI::Jdi->JitcEnabled() ? L"JITC" : L"";

		std::string dateStamp = __DATE__;
		std::string timeStamp = __TIME__;

		std::wstring build =
			L"Build " + Util::StringToWstring(UI::Jdi->GetVersion()) + L" " +
			std::wstring(version) + L" " + std::wstring(platform);

		if (jitc[0] != 0)
		{
			build += std::wstring(L" ") + jitc;
		}

		build += L" (" + Util::StringToWstring(dateStamp) + L" " + Util::StringToWstring(timeStamp) + L")";

		// The mark of the emulator next to its name, the way the site has the cube next to its
		// wordmark, and the build under it in the muted colour.
		const UiPalette& pal = UiThemePalette();

		const ImVec2 origin = ImGui::GetCursorScreenPos();
		UiThemeDrawCube(ImGui::GetWindowDrawList(), ImVec2(origin.x + 27.0f, origin.y + 27.0f), 54.0f);

		ImGui::Dummy(ImVec2(60.0f, 54.0f));
		ImGui::SameLine(0.0f, 16.0f);

		ImGui::BeginGroup();
		ImGui::PushStyleColor(ImGuiCol_Text, UiThemeVec4(pal.text));
		ImGui::TextUnformatted(APPNAME_A);
		ImGui::PopStyleColor();
		ImGui::PushStyleColor(ImGuiCol_Text, UiThemeVec4(pal.muted));
		ImGui::TextUnformatted(Util::WstringToString(APPDESC).c_str());
		ImGui::PopStyleColor();
		ImGui::EndGroup();

		ImGui::Separator();

		ImGui::PushStyleColor(ImGuiCol_Text, UiThemeVec4(pal.muted));
		ImGui::TextUnformatted(Util::WstringToString(build).c_str());
		ImGui::TextUnformatted("Copyright 2003-2026 Dolwin team, emu-russia");
		ImGui::PopStyleColor();

		ImGui::Separator();

		if (ImGui::Button("OK", ImVec2(120, 0))) {
			*enabled = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SetItemDefaultFocus();
		ImGui::EndPopup();
	}
}

static Json::Value* CmdUIError(std::vector<std::string>& args)
{
	std::string text = "";

	if (args.size() < 2)
	{
		return nullptr;
	}

	for (size_t i = 1; i < args.size(); i++)
	{
		text += args[i] + " ";
	}

	error_text = text;
	draw_error_box = true;

	return nullptr;
}

static Json::Value* CmdUIReport(std::vector<std::string>& args)
{
	std::string text = "";

	if (args.size() < 2)
	{
		return nullptr;
	}

	for (size_t i = 1; i < args.size(); i++)
	{
		text += args[i] + " ";
	}

	message_text = text;
	draw_message_box = true;

	return nullptr;
}

static Json::Value* CmdGetRenderTarget(std::vector<std::string>& args)
{
	// Return RenderTarget SDL window

	if (!render_target)
		return nullptr;

	Json::Value* value = new Json::Value();
	value->type = Json::ValueType::Int;
	value->value.AsInt = (uint64_t)render_target;
	return value;
}

void UIReflector()
{
	JdiAddCmd("UIError", CmdUIError);
	JdiAddCmd("UIReport", CmdUIReport);
	JdiAddCmd("GetRenderTarget", CmdGetRenderTarget);
}



// Statusbar

std::wstring status_parts[(int)STATUS_ENUM::StatusMax];

/* Set default values of statusbar parts */
static void ResetStatusBar()
{
	SetStatusText(STATUS_ENUM::Progress, L"Idle");
	SetStatusText(STATUS_ENUM::EmuTime, L"");
	SetStatusText(STATUS_ENUM::WallTime, L"");
}

/* Create status bar window */
static void CreateStatusBar()
{
	/* Set default values */
	ResetStatusBar();
}

/* Change text in specified statusbar part */
void SetStatusText(STATUS_ENUM sbPart, const std::wstring& text, bool post)
{
	status_parts[(int)sbPart] = std::wstring(text);
}




/*

# Game selector

The file selector: the list of executable files (DOL/ELF) and disk
images (GCM/ISO/RVZ) found in the configured paths, with the disk banners, titles, sizes and comments
taken from the DVD banner file. The list of paths is stored in the PATH user variable and is
extended with the directory of every loaded file.

*/

// Set by OnMainWindowOpened / OnMainWindowClosed
static bool emu_running = false;

/* The portable cartridge the selector is about to start (issue #468). The stand-alone front end of
   the portable machines owns the loop of the interface while it runs, so the launch happens at the
   end of the frame and not inside it: the selector draws itself locked first, and the veil with its
   reason is what the user sees in the console's window behind the portable one. */
static bool ui_portable_pending = false;
static std::wstring ui_portable_file;
static SELECTOR_FILE ui_portable_type = SELECTOR_FILE::Executable;

/* The banner of a disc, as the selector draws it.
   The picture of a disc is an RGB5A3 texture, and the transparency its alpha channel carries used to
   be all there was of a background: the row and its selection highlight showed through the banner.
   A user who does not want that (issue #112) paints the texels the alpha makes see-through over with
   a colour instead; `bannerBg` says whether anything is painted, and `bannerBgColor` what.

   The picture the disc carries is decoded once and kept (`decoded`): only the compositing is redone
   when the setting changes, because the disc itself never changes. */
struct SelectorBanner
{
	SDL_Texture* texture = nullptr;     // the texture the selector draws, made from `pixels`
	BannerRgba   decoded;               // the picture as the disc carries it, alpha channel and all
	BannerRgba   pixels;                // ... and the same picture with the background painted under it
};

/* File entry */
struct SelectorFile
{
	SELECTOR_FILE   type;               // One of the SELECTOR_FILE kinds
	size_t          size;               // File size
	std::wstring    id;                 // GameID = DiskID
	std::wstring    name;               // File path and name
	std::wstring    title;              // Alternate file name (from the banner)
	std::wstring    comment;            // Some notes (from the banner)
	SelectorBanner  banner;             // The banner texture and the picture it was made of (a disc, or
	                                    // a built-in picture of a portable machine)
};

/* All important data is placed here */
class UserSelector
{
public:

	bool            active = false;                     // 1, if enabled
	bool            smallIcons = false;                 // show small icons
	SELECTOR_SORT   sortBy = SELECTOR_SORT::Default;    // sort rule (one of SELECTOR_SORT_*)

	// What the transparency of the DVD banners becomes (issue #112; SELECTOR_BANNER_BG in
	// uisettings.h). The picture of a disc is decoded with its alpha channel kept, and the texels
	// it makes see-through are painted over with this colour (which may be translucent itself).
	SELECTOR_BANNER_BG bannerBg = SELECTOR_BANNER_BG::Preserve;
	ImU32           bannerBgColor = IM_COL32(0, 0, 0, 255);

	std::vector<std::wstring> paths;                    // path list, where to search files
	std::vector<std::unique_ptr<SelectorFile>> files;   // list of found files

	int             selected = -1;                      // selected file, -1: none
	bool            needUpdate = false;                 // the file list must be rescanned
	bool            scrollToSelected = false;           // scroll the list to the selected file

	~UserSelector()
	{
		clear();
	}

	void clear()
	{
		for (auto& file : files)
		{
			if (file->banner.texture)
			{
				SDL_DestroyTexture(file->banner.texture);
			}
		}

		files.clear();
		selected = -1;
	}
};

static UserSelector usel;

static SDL_Texture* selector_banner_upload(const BannerRgba& image)
{
	// The banner image is decoded to R, G, B, A texels; the renderer wants them in one word each,
	// the way it has always uploaded them.
	std::vector<uint32_t> pixels(DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT, 0);

	for (size_t i = 0; i < pixels.size(); i++)
	{
		const uint8_t* texel = &image.pixels[i * 4];

		pixels[i] = ((uint32_t)texel[3] << 24) | ((uint32_t)texel[0] << 16) |
			((uint32_t)texel[1] << 8) | texel[2];
	}

	SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STATIC, DVD_BANNER_WIDTH, DVD_BANNER_HEIGHT);
	if (texture == nullptr)
	{
		return nullptr;
	}

	SDL_UpdateTexture(texture, nullptr, pixels.data(), DVD_BANNER_WIDTH * sizeof(uint32_t));

	// The picture is premultiplied (see BannerCompositeBackground), so it is blended the way SDL
	// blends one: every texel takes the surface of the row behind it for what it does not cover.
	SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
	SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);      // small icons are scaled down

	return texture;
}

/* The background of a banner, as the setting says it. The colour is an ImU32 (ImGui's packed RGBA,
   the form the colour picker of the settings window edits), so it is unpacked into the four bytes
   the compositor wants. */
static void selector_banner_apply_bg(BannerRgba& image, SELECTOR_BANNER_BG bg, ImU32 bgColor)
{
	if (bg != SELECTOR_BANNER_BG::Fill)
	{
		return;     // the alpha channel of the picture is kept as it is
	}

	BannerCompositeBackground(image,
		(uint8_t)((bgColor >> IM_COL32_R_SHIFT) & 0xff),
		(uint8_t)((bgColor >> IM_COL32_G_SHIFT) & 0xff),
		(uint8_t)((bgColor >> IM_COL32_B_SHIFT) & 0xff),
		(uint8_t)((bgColor >> IM_COL32_A_SHIFT) & 0xff));
}

/* Make the texture of an entry out of the picture the entry decoded to, with the background setting
   applied to it. The disc path (selector_banner_build) decodes the banner of the disc first; a
   portable cartridge (selector_banner_build_rgba) brings a picture that is decoded already. */
static void selector_banner_finish(SelectorBanner& entry, SELECTOR_BANNER_BG bg, ImU32 bgColor)
{
	// The decoded picture is what the background setting is applied to again and again, so what is
	// composited is always a copy of it and never the result of a previous compositing.
	entry.pixels = entry.decoded;
	selector_banner_apply_bg(entry.pixels, bg, bgColor);

	if (entry.texture)
	{
		SDL_DestroyTexture(entry.texture);
	}

	entry.texture = selector_banner_upload(entry.pixels);
}

/* Make the picture and the texture of an entry, from the banner the disc carries. */
static void selector_banner_build(SelectorBanner& entry, const uint8_t* image,
	SELECTOR_BANNER_BG bg, ImU32 bgColor)
{
	entry.decoded = BannerToRgba(image);
	selector_banner_finish(entry, bg, bgColor);
}

/* The same, for a picture that is already RGBA (`bytes` of it, four bytes to a texel): the banner
   of a portable machine is built in (see res/portable_banners.h), because a ROM is not a disc and
   carries no picture of its own. */
static void selector_banner_build_rgba(SelectorBanner& entry, const uint8_t* rgba, size_t bytes,
	SELECTOR_BANNER_BG bg, ImU32 bgColor)
{
	entry.decoded.pixels.assign(rgba, rgba + bytes);
	selector_banner_finish(entry, bg, bgColor);
}

/* True when the type is one of the portable machines: the file selector does not load those into
   the console, it starts the stand-alone portable emulator with them (issue #468). */
static bool selector_type_portable(SELECTOR_FILE type)
{
	return type == SELECTOR_FILE::Dmg || type == SELECTOR_FILE::Cgb || type == SELECTOR_FILE::Gba;
}

/* The built-in picture of a portable machine, or nullptr for the types that have none: a disc has
   the banner the disc carries, an executable has no picture at all. */
static const uint8_t* selector_portable_banner(SELECTOR_FILE type)
{
	switch (type)
	{
		case SELECTOR_FILE::Dmg: return PortableBannerDmg;
		case SELECTOR_FILE::Cgb: return PortableBannerCgb;
		case SELECTOR_FILE::Gba: return PortableBannerGba;
		default: return nullptr;
	}
}

/* Make the texture of an entry again, for the case when only the background setting changed: the
   disc is not read a second time, the picture that its banner decoded to is. */
static void selector_banner_update(SelectorBanner& entry, SELECTOR_BANNER_BG bg, ImU32 bgColor)
{
	BannerRgba image = entry.decoded;

	selector_banner_apply_bg(image, bg, bgColor);

	SDL_Texture* texture = selector_banner_upload(image);

	if (texture == nullptr)
	{
		return;     // the texture that is up is still the one the entry is drawn with
	}

	if (entry.texture)
	{
		SDL_DestroyTexture(entry.texture);
	}

	entry.pixels = std::move(image);
	entry.texture = texture;
}

/* Make sure path have ending directory separator */
static void fix_path(std::wstring& path)
{
	if (path.empty() || (path.back() != L'/' && path.back() != L'\\'))
	{
		path.push_back((wchar_t)std::filesystem::path::preferred_separator);
	}
}

/* Remove all control symbols (below space) */
static void fix_string(std::wstring& str)
{
	for (auto& c : str)
	{
		if (c < L' ') c = L' ';
	}
}

/* Normalize the path, so that "./", ".\" and the same directory with or without the ending
   separator are treated as the same directory */
static std::wstring canonical_path(const std::wstring& path)
{
	std::error_code ec;
	auto canon = std::filesystem::weakly_canonical(std::filesystem::path(path), ec);
	return ec ? path : canon.wstring();
}

/* Load PATH user variable and cut it on pieces into "paths" list */
static void load_path()
{
	auto var = Util::StringToWstring(UI::Jdi->GetConfigString(USER_PATH, USER_UI));

	usel.paths.clear();

	auto path = std::wstring();

	for (auto& c : var)
	{
		if (c == L';')
		{
			if (!path.empty())
			{
				fix_path(path);
				usel.paths.push_back(path);
				path.clear();
			}
		}
		else
		{
			path.push_back(c);
		}
	}

	if (!path.empty())
	{
		fix_path(path);
		usel.paths.push_back(path);
	}
}

/* Add new path into the PATH user variable (called after loading of new file) */
static void AddSelectorPath(const std::wstring& fullPath)
{
	if (fullPath.empty())
	{
		return;
	}

	load_path();

	auto newPath = canonical_path(fullPath);

	for (auto& path : usel.paths)
	{
		if (canonical_path(path) == newPath)
		{
			return;     // path duplicated
		}
	}

	auto path = std::wstring(fullPath);
	fix_path(path);

	auto old = Util::StringToWstring(UI::Jdi->GetConfigString(USER_PATH, USER_UI));

	UI::Jdi->SetConfigString(USER_PATH, Util::WstringToString(old.empty() ? path : (old + L";" + path)), USER_UI);

	usel.paths.push_back(path);
	usel.needUpdate = true;
}

/* Take a path out of the PATH user variable (the "Remove" of the selector settings) */
static void RemoveSelectorPath(const std::wstring& fullPath)
{
	load_path();

	auto forgotten = canonical_path(fullPath);
	std::wstring kept;

	for (auto& path : usel.paths)
	{
		if (canonical_path(path) == forgotten)
		{
			continue;
		}

		if (!kept.empty())
		{
			kept += L';';
		}

		kept += path;
	}

	UI::Jdi->SetConfigString(USER_PATH, Util::WstringToString(kept), USER_UI);

	load_path();
	usel.needUpdate = true;
}

/* Directory of the specified file, with the ending separator */
static std::wstring ParentPath(const std::wstring& filename)
{
	auto path = std::filesystem::path(filename).parent_path().wstring();

	if (!path.empty())
	{
		fix_path(path);
	}

	return path;
}

/* Copy the banner ANSI string (up to the zero terminator) into a wide string */
static std::wstring CopyAnsiStringAsWcharString(const uint8_t* src, size_t maxLen)
{
	std::wstring res;

	for (size_t i = 0; i < maxLen && src[i]; i++)
	{
		res.push_back((wchar_t)src[i]);
	}

	return res;
}

/* Convert the SJIS text of the Japanese banners to Unicode */
static std::wstring SjisToWstring(const std::wstring& sjis)
{
	std::wstring res;
	res.reserve(sjis.size());

	for (size_t i = 0; i < sjis.size(); i++)
	{
		uint16_t c = (uint16_t)sjis[i];
		uint16_t uchar = SjisTable[c];

		if (uchar == 0xFFFF && (i + 1) < sjis.size())
		{
			i++;
			c = (uint16_t)((c << 8) | (uint16_t)sjis[i]);
			uchar = SjisTable[c];
		}

		res.push_back((wchar_t)uchar);
	}

	return res;
}

/* ImGui draws UTF-8 strings */
static std::string ToUtf8(const std::wstring& wstr)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_conv;
	return utf8_conv.to_bytes(wstr);
}

/* Nice value of KB, MB or GB, for output */
static std::string SmartSize(size_t size)
{
	char tempBuf[0x100];

	if (size < 1024)
	{
		sprintf(tempBuf, "%zi byte", size);
	}
	else if (size < 1024 * 1024)
	{
		sprintf(tempBuf, "%zi KB", size / 1024);
	}
	else if (size < 1024 * 1024 * 1024)
	{
		sprintf(tempBuf, "%zi MB", size / 1024 / 1024);
	}
	else
	{
		sprintf(tempBuf, "%1.1f GB", (float)size / 1024 / 1024 / 1024);
	}

	return std::string(tempBuf);
}

/* Insert new file into filelist */
static void add_file(const std::wstring& file, size_t fsize, SELECTOR_FILE type)
{
	// check file size
	if ((fsize < 0x1000) || (fsize > DVD_SIZE))
	{
		return;
	}

	// check already present
	for (auto& entry : usel.files)
	{
		if (entry->name == file)
		{
			return;
		}
	}

	auto item = std::make_unique<SelectorFile>();

	/* Save file info */
	item->type = type;
	item->size = fsize;
	item->name = file;

	if (type == SELECTOR_FILE::Dvd)
	{
		// To get information from the disk, you have to remount it.
		// If the user, for example, mounted DolphinSDK, it is necessary to restore the previous state
		// of the mount so that he does not get upset.

		/* Load DVD banner. */
		std::vector<uint8_t> banner = DVDLoadBanner(file.c_str());
		if (banner.empty())
		{
			return;
		}

		// Keep previous mount state

		std::string path;
		bool mountedAsIso = false;
		bool mounted = UI::Jdi->DvdIsMounted(path, mountedAsIso);

		// get DiskID

		std::vector<uint8_t> diskIDRaw;
		diskIDRaw.resize(4);
		char diskID[0x10] = { 0 };
		UI::Jdi->DvdMount(Util::WstringToString(file));
		UI::Jdi->DvdSeek(0);
		UI::Jdi->DvdRead(diskIDRaw);
		diskID[0] = (char)diskIDRaw[0];
		diskID[1] = (char)diskIDRaw[1];
		diskID[2] = (char)diskIDRaw[2];
		diskID[3] = (char)diskIDRaw[3];

		// Set GameID

		char game_id[0x10] = { 0 };
		sprintf(game_id, "%.4s", diskID);
		item->id = Util::StringToWstring(std::string(game_id));

		// Restore previous mount state

		if (mounted)
		{
			if (mountedAsIso)
			{
				UI::Jdi->DvdMount(path);
			}
			else
			{
				UI::Jdi->DvdMountSDK(path);
			}
		}
		else
		{
			UI::Jdi->DvdUnmount();
		}

		/* Use banner info and remove line-feeds. */

		DVDBanner2* bnr = (DVDBanner2*)banner.data();
		item->title = CopyAnsiStringAsWcharString(bnr->comments[0].longTitle, sizeof(bnr->comments[0].longTitle));
		item->comment = CopyAnsiStringAsWcharString(bnr->comments[0].comment, sizeof(bnr->comments[0].comment));

		// Japanese banners keep the text in SJIS

		if (UI::Jdi->DvdRegionById(diskID) == "JPN")
		{
			item->title = SjisToWstring(item->title);
			item->comment = SjisToWstring(item->comment);
		}

		fix_string(item->title);
		fix_string(item->comment);

		selector_banner_build(item->banner, bnr->image, usel.bannerBg, usel.bannerBgColor);
	}
	else if (type == SELECTOR_FILE::Executable || selector_type_portable(type))
	{
		// A file that is not a disc brings no banner of its own: the title is the name of the file,
		// and a portable cartridge draws the built-in picture of the console it belongs to.
		item->id = L"-";
		item->title = std::filesystem::path(file).stem().wstring();

		const uint8_t* portable = selector_portable_banner(type);

		if (portable != nullptr)
		{
			selector_banner_build_rgba(item->banner, portable,
				(size_t)PortableBannerWidth * PortableBannerHeight * 4, usel.bannerBg, usel.bannerBgColor);
		}
	}
	else
	{
		assert(0);
	}

	/* Extend filelist. */
	usel.files.push_back(std::move(item));
}

/* Set selected file, by item index */
static void selector_set_selected(int item, bool scroll)
{
	if (item < 0 || item >= (int)usel.files.size())
	{
		return;
	}

	usel.selected = item;
	usel.scrollToSelected = scroll;

	if (!emu_running)
	{
		SetStatusText(STATUS_ENUM::Progress, usel.files[item]->name);
	}
}

// if file not present, keep selection unchanged
static void selector_select_by_name(const std::wstring& filename)
{
	if (filename.empty())
	{
		return;
	}

	auto name = canonical_path(filename);

	for (size_t i = 0; i < usel.files.size(); i++)
	{
		if (canonical_path(usel.files[i]->name) == name)
		{
			selector_set_selected((int)i, true);
			break;
		}
	}
}

// Set selected item, by first letter key pressed
static void selector_type_ahead(wchar_t letter)
{
	// Case folding is limited to the ASCII range on purpose: the locale dependent
	// tolower() is undefined for characters outside of that range.
	if (letter >= L'A' && letter <= L'Z') letter = (wchar_t)(letter - L'A' + L'a');

	for (size_t i = 0; i < usel.files.size(); i++)
	{
		wchar_t first = usel.files[i]->title.empty() ? 0 : usel.files[i]->title[0];
		if (first >= L'A' && first <= L'Z') first = (wchar_t)(first - L'A' + L'a');

		if (first == letter)
		{
			selector_set_selected((int)i, true);
			break;
		}
	}
}

/* Sort the filelist (the sort rule is stored in the SORTVIEW user variable) */
static void sort_selector(SELECTOR_SORT sortBy)
{
	auto by_title = [](const std::unique_ptr<SelectorFile>& f1, const std::unique_ptr<SelectorFile>& f2)
	{
		return _wcsicmp(f1->title.c_str(), f2->title.c_str()) < 0;
	};

	// the selection is kept by file name, because sorting changes the indexes
	std::wstring selectedName;

	if (usel.selected >= 0 && usel.selected < (int)usel.files.size())
	{
		selectedName = usel.files[usel.selected]->name;
	}

	switch (sortBy)
	{
		case SELECTOR_SORT::Default:        // first by icon, then by title
			std::stable_sort(usel.files.begin(), usel.files.end(), [&](const auto& f1, const auto& f2)
				{
					if (f1->type != f2->type) return f1->type > f2->type;
					return by_title(f1, f2);
				});
			break;
		case SELECTOR_SORT::Filename:
			std::stable_sort(usel.files.begin(), usel.files.end(),
				[](const auto& f1, const auto& f2) { return _wcsicmp(f1->name.c_str(), f2->name.c_str()) < 0; });
			break;
		case SELECTOR_SORT::Title:
			std::stable_sort(usel.files.begin(), usel.files.end(), by_title);
			break;
		case SELECTOR_SORT::Size:
			std::stable_sort(usel.files.begin(), usel.files.end(),
				[](const auto& f1, const auto& f2) { return f1->size < f2->size; });
			break;
		case SELECTOR_SORT::ID:
			std::stable_sort(usel.files.begin(), usel.files.end(),
				[](const auto& f1, const auto& f2) { return f1->id < f2->id; });
			break;
		case SELECTOR_SORT::Comment:
			std::stable_sort(usel.files.begin(), usel.files.end(),
				[](const auto& f1, const auto& f2) { return _wcsicmp(f1->comment.c_str(), f2->comment.c_str()) < 0; });
			break;
		default:                            // Unsorted: keep the order in which the files were found
			break;
	}

	usel.sortBy = sortBy;
	UI::Jdi->SetConfigInt(USER_SORTVIEW, (int)usel.sortBy, USER_UI);

	selector_select_by_name(selectedName);
}

/* Rescan the configured paths (reload and redraw) */
static void update_selector()
{
	if (!usel.active)
	{
		return;
	}

	// Reading the disk banners mounts the disks, which must not disturb the running game
	if (emu_running)
	{
		return;
	}

	usel.clear();
	usel.needUpdate = false;

	load_path();

	// The same directory may be specified in different ways, so first drop the duplicates,
	// otherwise the files of this directory will be listed twice.
	std::vector<std::wstring> dirs;

	for (auto& path : usel.paths)
	{
		auto dir = canonical_path(path);

		if (std::find(dirs.begin(), dirs.end(), dir) == dirs.end())
		{
			dirs.push_back(dir);
		}
	}

	usel.paths = dirs;

	// File filter: every 8 bits masking an extension. The console's files and the cartridges of the
	// portable machines have a filter variable each (issue #468): the four bytes of FILTER are all
	// taken by the console ones.
	const uint32_t filters[] =
	{
		(uint32_t)UI::Jdi->GetConfigInt(USER_FILTER, USER_UI),
		(uint32_t)UI::Jdi->GetConfigInt(USER_FILTER_PORTABLE, USER_UI),
	};

	static const struct
	{
		const wchar_t* ext;
		SELECTOR_FILE  type;
		bool           portable;    // which of the two filter variables enables the extension
		uint32_t       mask;
	} file_ext[] =
	{
		{ L".dol", SELECTOR_FILE::Executable, false, 0xff000000 },
		{ L".elf", SELECTOR_FILE::Executable, false, 0x00ff0000 },
		{ L".gcm", SELECTOR_FILE::Dvd,        false, 0x0000ff00 },
		{ L".rvz", SELECTOR_FILE::Dvd,        false, 0x0000ff00 },
		{ L".iso", SELECTOR_FILE::Dvd,        false, 0x000000ff },

		{ L".dmg", SELECTOR_FILE::Dmg,        true,  0xff0000 },
		{ L".gb",  SELECTOR_FILE::Dmg,        true,  0xff0000 },
		{ L".cgb", SELECTOR_FILE::Cgb,        true,  0x00ff00 },
		{ L".gbc", SELECTOR_FILE::Cgb,        true,  0x00ff00 },
		{ L".gba", SELECTOR_FILE::Gba,        true,  0x0000ff },
		{ L".agb", SELECTOR_FILE::Gba,        true,  0x0000ff },
	};

	for (auto& dir : usel.paths)
	{
		std::error_code ec;
		std::filesystem::directory_iterator entry(std::filesystem::path(dir), ec);
		if (ec)
		{
			continue;       // no such directory
		}

		for (auto& file : entry)
		{
			if (!file.is_regular_file(ec))
			{
				continue;
			}

			auto ext = file.path().extension().wstring();
			for (auto& c : ext)
			{
				if (c >= L'A' && c <= L'Z') c = (wchar_t)(c - L'A' + L'a');
			}

			for (auto& mask : file_ext)
			{
				if ((filters[mask.portable ? 1 : 0] & mask.mask) && ext == mask.ext)
				{
					add_file(file.path().wstring(), (size_t)file.file_size(ec), mask.type);
				}
			}
		}
	}

	sort_selector(usel.sortBy);

	// scroll to last loaded file
	selector_select_by_name(Util::StringToWstring(UI::Jdi->GetConfigString(USER_LASTFILE, USER_UI)));
}

// ---------------------------------------------------------------------------
// The game selector, as the settings window sees it
//
// The "General" page of the settings window (uisettings.cpp) edits the view of the selector, and
// the selector itself is here. The page is what the user drives; these calls are what keeps the two
// in step - they are the whole interface between the window and the selector.

SelectorSettings SelectorGetSettings()
{
	SelectorSettings settings;

	settings.active = usel.active;
	settings.smallIcons = usel.smallIcons;
	settings.sortBy = usel.sortBy;
	settings.bannerBg = usel.bannerBg;
	settings.bannerBgColor = usel.bannerBgColor;
	settings.paths = usel.paths;

	return settings;
}

void SelectorSetSettings(const SelectorSettings& settings)
{
	usel.active = settings.active;
	UI::Jdi->SetConfigBool(USER_SELECTOR, usel.active, USER_UI);

	usel.smallIcons = settings.smallIcons;
	UI::Jdi->SetConfigBool(USER_SMALLICONS, usel.smallIcons, USER_UI);

	if (usel.sortBy != settings.sortBy)
	{
		sort_selector(settings.sortBy);
	}

	// The background of the banners (issue #112) is neither a file nor a column: the entries and
	// their order do not change with it, so the pictures the selector holds are redrawn instead of a
	// rescan that would mount every disc again.
	if (usel.bannerBg != settings.bannerBg || usel.bannerBgColor != settings.bannerBgColor)
	{
		usel.bannerBg = settings.bannerBg;
		usel.bannerBgColor = settings.bannerBgColor;

		UI::Jdi->SetConfigInt(USER_BANNER_BG, (int)usel.bannerBg, USER_UI);
		UI::Jdi->SetConfigInt(USER_BANNER_BG_COLOR, (int)usel.bannerBgColor, USER_UI);

		for (auto& file : usel.files)
		{
			if (file->banner.texture)
			{
				selector_banner_update(file->banner, usel.bannerBg, usel.bannerBgColor);
			}
		}
	}

	usel.needUpdate = true;
}

void SelectorAddPath(const std::wstring& path)
{
	AddSelectorPath(path);
}

void SelectorRemovePath(const std::wstring& path)
{
	RemoveSelectorPath(path);
}

void SelectorRescan()
{
	usel.needUpdate = true;
}







/*
# The window titles

The video output window - the one the emulated picture is in - says what the machine is drawing with
and how fast: the backend the picture comes from and the frame rate of the last measured second
(issue #458). The main window keeps the plain name of what is running; the state of the machine
belongs to the window that shows it.

The rate is measured by the performance thread, but SDL wants a window touched from the thread that
owns it, so the thread only leaves the measurement here and the frame loop writes the title.
*/

static std::atomic<bool>  ui_title_dirty{ false };
static std::atomic<float> ui_frame_rate{ 0.0f };
static std::atomic<int>   ui_gfx_pipeline{ GFX_PIPELINE_SHADER };

/* The milliseconds of the host's steady clock, counted from the first call of the process. The
   performance thread measures the frame rate and the wall time of the status bar against it. */
static uint64_t ui_now_ms()
{
	static const auto origin = std::chrono::steady_clock::now();
	return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - origin).count();
}

/* A duration the way the status line shows it: `2h 05m 09s`, `5m 09s`, `9s`. The hours and the
   minutes are left out when they are zero, and the seconds keep two digits when something is shown
   in front of them (issue #458). */
static std::wstring ui_duration(int64_t seconds)
{
	if (seconds < 0)
	{
		seconds = 0;
	}

	const int64_t hours = seconds / 3600;
	const int64_t minutes = (seconds / 60) % 60;
	const int64_t rest = seconds % 60;

	wchar_t pair[8];
	std::wstring text;

	if (hours > 0)
	{
		text = std::to_wstring(hours) + L"h ";
	}

	if (hours > 0 || minutes > 0)
	{
		swprintf(pair, _countof(pair), L"%02i", (int)minutes);
		text += std::wstring(pair) + L"m ";
	}

	swprintf(pair, _countof(pair), (hours > 0 || minutes > 0) ? L"%02i" : L"%i", (int)rest);

	return text + pair + L"s";
}

/*

# Performance Counters

Interesting to track :
- The number of emulated Gekko instructions (million per second, mips)
- Number of recompiled and executed GekkoCore recompiler segments.
- Number of DSP instructions emulated (million per second, mips)
- The number of frames a second the emulated picture is output with (the window title)
- The time since the emulation started, as the console's clock (TBR) and the host's clock count it

*/

namespace UI
{

	// Global instance of the utility, which is controlled by the front end
	PerfMetrics* g_perfMetrics = nullptr;


	void PerfMetrics::PerfThreadProc(void* param)
	{
		PerfMetrics* perf = (PerfMetrics*)param;

		// Get and reset counters

		int64_t gekkoMips = perf->GetGekkoInstructionsCounter();
		perf->ResetGekkoInstructionsCounter();

		int64_t compiledSegs = perf->GetGekkoCompiledSegments();
		perf->ResetGekkoCompiledSegments();

		int64_t executedSegs = perf->GetGekkoExecutedSegments();
		perf->ResetGekkoExecutedSegments();

		int64_t dspMips = perf->GetDspInstructionsCounter();
		perf->ResetDspInstructionsCounter();

		// If the number of executed segments is zero, then most likely the emulator is running in interpreter mode
		// or is in debug mode (emulation is temporarily stopped), so there is no point in displaying JITC statistics.

		char str[0x100];
		if (executedSegs != 0)
		{
			sprintf(str, "gekko: %.02f mips (jitc %lld/%.02fM), dsp: %.02f mips",
				(float)gekkoMips / 1000000.f, compiledSegs, (float)executedSegs / 1000000.f, (float)dspMips / 1000000.f);
		}
		else
		{
			sprintf(str, "gekko: %.02f mips, dsp: %.02f mips",
				(float)gekkoMips / 1000000.f, (float)dspMips / 1000000.f);
		}

		// The frame rate (issue #458): how many times the emulated picture was output since the
		// previous sample, over the host time that passed in between. A sample that arrives too
		// early to have a window of its own (a sleeping host, a clock that did not move) keeps the
		// rate of the previous one.
		//
		// The picture is output in one of two places, and exactly one of them counts a frame: the
		// GL backend presents the frames it is given (the counter of those, which is read through
		// the debug interface like the other statistics), and the video back end outputs the XFB
		// when the GL backend is not the one presenting - the software pipeline, or a machine that
		// never reached the GL backend at all (VideoOutTakeFrames, videosdl.cpp).
		uint64_t now = ui_now_ms();

		int64_t frames = perf->GetPresentedFramesCounter() + VideoOutTakeFrames();
		perf->ResetPresentedFramesCounter();

		if (now > perf->lastSampleMs)
		{
			ui_frame_rate.store((float)((double)frames / ((double)(now - perf->lastSampleMs) / 1000.0)));
		}

		perf->lastSampleMs = now;

		// The two clocks of the status bar (issue #458): the console's time base (TBR) and the
		// host's own clock, both measured from the moment the emulation of this image started.
		int64_t emulated = perf->GetEmulatedSeconds() - perf->startEmulatedSeconds;
		int64_t wall = (int64_t)((now - perf->startWallMs) / 1000);

		// Display information in the status bar

		SetStatusText(STATUS_ENUM::Progress, Util::StringToWstring(str));
		SetStatusText(STATUS_ENUM::EmuTime, L"TBR " + ui_duration(emulated));
		SetStatusText(STATUS_ENUM::WallTime, L"wall " + ui_duration(wall));

		// The backend can be switched while a game runs (the Hardware page of the settings, the
		// `gxpipeline` command), so it is asked of the machine with every sample rather than read
		// from the configuration once.
		ui_gfx_pipeline.store(perf->GetGfxPipeline());

		// The titles belong to the frame loop (see the note above this section).
		ui_title_dirty.store(true);

		Thread::Sleep(perf->metricsInterval);
	}

	PerfMetrics::PerfMetrics()
	{
		// The clocks of the status bar are measured from here: the moment the emulation started.
		startEmulatedSeconds = Jdi->GetEmulatedSeconds();
		startWallMs = ui_now_ms();
		lastSampleMs = startWallMs;

		perfThread = EMUCreateThread(PerfThreadProc, false, this, "PerfThread");
	}

	PerfMetrics::~PerfMetrics()
	{
		EMUJoinThread(perfThread);
	}

	int64_t PerfMetrics::GetGekkoInstructionsCounter()
	{
		return Jdi->GetPerformanceCounter((int)Debug::PerfCounter::GekkoInstructions);
	}

	void PerfMetrics::ResetGekkoInstructionsCounter()
	{
		Jdi->ResetPerformanceCounter((int)Debug::PerfCounter::GekkoInstructions);
	}

	int64_t PerfMetrics::GetGekkoCompiledSegments()
	{
		return Jdi->GetPerformanceCounter((int)Debug::PerfCounter::GekkoCompiledSegments);
	}

	void PerfMetrics::ResetGekkoCompiledSegments()
	{
		Jdi->ResetPerformanceCounter((int)Debug::PerfCounter::GekkoCompiledSegments);
	}

	int64_t PerfMetrics::GetGekkoExecutedSegments()
	{
		return Jdi->GetPerformanceCounter((int)Debug::PerfCounter::GekkoExecutedSegments);
	}

	void PerfMetrics::ResetGekkoExecutedSegments()
	{
		Jdi->ResetPerformanceCounter((int)Debug::PerfCounter::GekkoExecutedSegments);
	}

	int64_t PerfMetrics::GetDspInstructionsCounter()
	{
		return Jdi->GetPerformanceCounter((int)Debug::PerfCounter::DspInstructions);
	}

	void PerfMetrics::ResetDspInstructionsCounter()
	{
		Jdi->ResetPerformanceCounter((int)Debug::PerfCounter::DspInstructions);
	}

	int64_t PerfMetrics::GetPresentedFramesCounter()
	{
		return Jdi->GetPerformanceCounter((int)Debug::PerfCounter::PresentedFrames);
	}

	void PerfMetrics::ResetPresentedFramesCounter()
	{
		Jdi->ResetPerformanceCounter((int)Debug::PerfCounter::PresentedFrames);
	}

	int64_t PerfMetrics::GetEmulatedSeconds()
	{
		return Jdi->GetEmulatedSeconds();
	}

	int PerfMetrics::GetGfxPipeline()
	{
		return Jdi->GetGfxPipeline();
	}

}



// The icon is embedded in the source code, so it is available in the SDL port on any platform
static void SetWindowIcon(SDL_Window* wnd)
{
	if (!wnd)
	{
		return;
	}

	SDL_Surface* icon = SDL_CreateRGBSurfaceWithFormatFrom((void*)PureiIconPixels, PureiIconWidth, PureiIconHeight,
		32, PureiIconWidth * sizeof(uint32_t), SDL_PIXELFORMAT_ARGB8888);

	if (icon)
	{
		SDL_SetWindowIcon(wnd, icon);
		SDL_FreeSurface(icon);
	}
}

/*
The name of the video output window - the window the emulated picture is in. It carries what the
machine is drawing with and how fast: the backend the picture comes from and the frame rate of the
last measured second (issue #458). The main window is the front end itself (the selector and the
status line) and keeps the plain name of what is running.
*/

/* The name a title gives the rendering backend: the shader (OpenGL) pipeline or the software one
   (issue #384). The performance thread asks the machine which one it is running (it can be
   switched at any moment) and leaves the answer in `ui_gfx_pipeline`. */
static const wchar_t* ui_backend_name()
{
	return (ui_gfx_pipeline.load() == GFX_PIPELINE_SOFT) ? L"soft" : L"shader";
}

/* The name of the video output window with the backend and the frame rate on it: `Video Output |
   shader | 59.9 fps`. */
static std::wstring ui_video_output_title()
{
	wchar_t rate[0x20];
	swprintf(rate, _countof(rate), L"%.1f", (double)ui_frame_rate.load());

	return std::wstring(L"Video Output | ") + ui_backend_name() + L" | " + rate + L" fps";
}

/* What the main window is called while nothing runs (the name and the version of the emulator) and
   while an image does (OnMainWindowOpened makes that one out of the loaded file). */
static std::wstring ui_idle_title;
static std::wstring ui_running_title;

static void ui_update_window_titles()
{
	if (window != nullptr)
	{
		SDL_SetWindowTitle(window, ToUtf8(ui_running_title.empty() ? ui_idle_title : ui_running_title).c_str());
	}

	// The video output window exists only while an image runs.
	if (render_target != nullptr)
	{
		SDL_SetWindowTitle(render_target, ToUtf8(ui_video_output_title()).c_str());
	}
}

static void CreateRenderTarget()
{
	// Create RenderTarget (for xfb / gfx)
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
	if (cmdline.bench)
	{
		// The benchmark runs unattended, so its video output window stays out of the way.
		window_flags = (SDL_WindowFlags)(window_flags | SDL_WINDOW_HIDDEN);
	}
	render_target = SDL_CreateWindow("Video Output", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, window_flags);
	SetWindowIcon(render_target);
	ui_update_window_titles();
}

static void DestroyRenderTarget()
{
	if (render_target != nullptr) {
		SDL_DestroyWindow(render_target);
		render_target = nullptr;
	}
}


// emulation has started - do proper actions
void OnMainWindowOpened(const wchar_t* currentFileName)
{
	std::wstring newTitle, gameTitle;
	bool dvd = false;
	bool bootrom = !wcscmp(currentFileName, L"Bootrom");

	if (!bootrom)
	{
		// A name without an extension must not be handed to _wcsicmp: it dereferences its
		// arguments, and wcsrchr answers NULL here.
		const wchar_t* extension = wcsrchr(currentFileName, L'.');

		if (extension == nullptr)
		{
			extension = L"";
		}

		if (!_wcsicmp(extension, L".dol"))
		{
			dvd = false;
		}
		else if (!_wcsicmp(extension, L".elf"))
		{
			dvd = false;
		}
		else if (!_wcsicmp(extension, L".iso"))
		{
			dvd = true;
		}
		else if (!_wcsicmp(extension, L".gcm"))
		{
			dvd = true;
		}
		else if (!_wcsicmp(extension, L".rvz"))
		{
			dvd = true;
		}
	}

	// set new title for main window

	if (!bootrom)
	{
		// remember the directory of the loaded file in the selector search paths
		AddSelectorPath(ParentPath(currentFileName));
		UI::Jdi->SetConfigString(USER_LASTFILE, Util::WstringToString(currentFileName), USER_UI);
	}

	emu_running = true;

	if (dvd)
	{
		UI::Jdi->DvdMount(Util::WstringToString(currentFileName));

		// get DiskID
		std::vector<uint8_t> diskID;
		diskID.resize(4);
		UI::Jdi->DvdSeek(0);
		UI::Jdi->DvdRead(diskID);

		// Get title from banner

		std::vector<uint8_t> bnrRaw = DVDLoadBanner(currentFileName);

		DVDBanner2* bnr = (DVDBanner2*)bnrRaw.data();

		wchar_t longTitle[0x200];

		// The banner field has no guaranteed terminator, so the copy has to be bounded by
		// both the source field and the destination buffer (this one is on the stack).
		std::wstring boundedTitle = CopyAnsiStringAsWcharString(bnr->comments[0].longTitle, sizeof(bnr->comments[0].longTitle));

		{
			size_t n = boundedTitle.size();
			if (n > _countof(longTitle) - 1)
			{
				n = _countof(longTitle) - 1;
			}

			memcpy(longTitle, boundedTitle.c_str(), n * sizeof(wchar_t));
			longTitle[n] = 0;
		}

		// Convert SJIS Title to Unicode

		if (UI::Jdi->DvdRegionById((char*)diskID.data()) == "JPN")
		{
			size_t size, chars;
			uint16_t* widePtr = SjisToUnicode(longTitle, &size, &chars);
			uint16_t* unicodePtr;

			if (widePtr)
			{
				wchar_t* wcharPtr = longTitle;
				unicodePtr = widePtr;

				while (*unicodePtr && wcharPtr < (longTitle + _countof(longTitle) - 1))
				{
					*wcharPtr++ = *unicodePtr++;
				}
				*wcharPtr = 0;

				free(widePtr);
			}
		}

		gameTitle = longTitle;
		newTitle = std::wstring(APPNAME) + L" - Running " + gameTitle;
	}
	else
	{
		if (bootrom)
		{
			gameTitle = currentFileName;
		}
		else
		{
			gameTitle = std::wstring(currentFileName) + L" demo";
		}

		newTitle = std::wstring(APPNAME) + L" - Running " + gameTitle;
	}

	ui_running_title = newTitle;

	// The rate of the sample the performance thread is about to take belongs to this image and not
	// to the one before it, which the video output window was showing until a moment ago.
	ui_frame_rate.store(0.0f);

	ui_update_window_titles();

	// A benchmark run samples the counters itself: the metrics thread clears them every second.
	if (!cmdline.bench)
	{
		UI::g_perfMetrics = new UI::PerfMetrics();
	}
}

// emulation stop in progress
void OnMainWindowClosed()
{
	if (UI::g_perfMetrics != nullptr) {
		delete UI::g_perfMetrics;
		UI::g_perfMetrics = nullptr;
	}

	// make the selector visible again
	emu_running = false;
	usel.needUpdate = true;

	// set to Idle
	ui_running_title.clear();
	ui_idle_title = std::wstring(APPNAME) + L" - " + std::wstring(APPDESC) + L" (" + Util::StringToWstring(UI::Jdi->GetVersion()) + L")";
	ui_update_window_titles();
	ResetStatusBar();
}

// Start the IPL (Bootrom) - the same thing that the "File -> Run Bootrom" menu item does.
static void ui_load_bootrom()
{
	CreateRenderTarget();
	UI::Jdi->LoadFile("Bootrom");
	OnMainWindowOpened(L"Bootrom");
	UI::Jdi->Run();
}

/* Defined with the other file loaders, below; the menu needs it first. */
static void reopen_last_file();

/* The save state slot the quick save keys work on (File -> Quick Save/Load State). The ten slots
   are numbered 0 to 9 - the number the `savestate`/`loadstate` commands take as an argument and
   the one in the suffix of the file (`.st0` .. `.st9`) - and Shift+F5/Shift+F7 step the slot
   instead of using it, the same arrangement the GBA front end offers for its own keys (see
   src/gba/gba_sdl.cpp). */
static int state_slot = 0;

/* Write the state of a slot. The work is the debug interface's (`savestate <slot>`), so what the
   menu item does is exactly what the command line does, and the front end neither knows the
   format nor the file names - it only shows the file the answer names. */
static void ui_save_state(int slot)
{
	std::string path, error;
	bool ok = false;

	try
	{
		ok = UI::Jdi->SaveState(slot, path, error);
	}
	catch (...)
	{
		error = "the debug interface refused the command";
	}

	if (ok)
	{
		SetStatusText(STATUS_ENUM::Progress, L"State saved to " + Util::StringToWstring(path));
	}
	else
	{
		ui_report_error("Save state failed: " + error);
	}
}

/* Read the state of a slot back into the machine (`loadstate <slot>`). */
static void ui_load_state(int slot)
{
	std::string path, error;
	bool ok = false;

	try
	{
		ok = UI::Jdi->LoadState(slot, path, error);
	}
	catch (...)
	{
		error = "the debug interface refused the command";
	}

	if (ok)
	{
		SetStatusText(STATUS_ENUM::Progress, L"State loaded from " + Util::StringToWstring(path));
	}
	else
	{
		ui_report_error("Load state failed: " + error);
	}
}

/* The ten slots of one of the two save state submenus of the File menu. The current slot of the
   quick save keys is marked, so that stepping it with Shift+F5 and then looking at the menu says
   which slot the keys are on. */
static void ui_state_slot_menu(bool write)
{
	for (int slot = 0; slot <= SaveStates::MaxSlot; slot++)
	{
		char label[32];
		snprintf(label, sizeof(label), "Slot %i%s", slot, (slot == state_slot) ? " (current)" : "");

		if (ImGui::MenuItem(label))
		{
			if (write)
			{
				ui_save_state(slot);
			}
			else
			{
				ui_load_state(slot);
			}
		}
	}
}

/* Step the quick save slot (Shift+F5 / Shift+F7 and the two menu items). The slot wraps, so the
   ten of them are a ring the user can walk without ever looking at a number. */
static void ui_step_state_slot(int step)
{
	state_slot = (state_slot + step + (SaveStates::MaxSlot + 1)) % (SaveStates::MaxSlot + 1);

	SetStatusText(STATUS_ENUM::Progress, L"Save state slot " + std::to_wstring(state_slot));
}

static void ui_main_menu()
{
	// Menu Bar
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Open", NULL)) {
				open_file_dialog(FileReaction::OpenFile_LoadFile, "Open File", selector_file_filters);
			}
			if (ImGui::MenuItem("Reopen", "F3")) {
				reopen_last_file();
			}
			if (ImGui::MenuItem("Close", NULL)) {		// Unload file (STOP)
				UI::Jdi->Stop();
				Thread::Sleep(100);
				UI::Jdi->Unload();
				DestroyRenderTarget();
				OnMainWindowClosed();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Run Bootrom", NULL)) {		// Load bootrom
				ui_load_bootrom();
			}
			if (ImGui::BeginMenu("Swap Disk"))
			{
				if (ImGui::MenuItem(UI::Jdi->DvdCoverOpened() ? "Close Cover" : "Open Cover", NULL)) {

					if (UI::Jdi->DvdCoverOpened()) {
						UI::Jdi->DvdCloseCover();
						ui_insert_dvd_menu_item_enabled = false;
					}
					else {
						UI::Jdi->DvdOpenCover();
						ui_insert_dvd_menu_item_enabled = true;
					}
				}
				if (ImGui::MenuItem("Change DVD...", NULL)) {
					if (ui_insert_dvd_menu_item_enabled) {
						open_file_dialog(FileReaction::ChooseFile_DVDImage, "Change DVD", dvd_image_filters);
					}
				}
				ImGui::EndMenu();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Refresh View", NULL, false, usel.active)) {
				usel.needUpdate = true;
			}
			ImGui::Separator();
			// The save states (see wiki/savestate.md). The two quick items work on the current
			// slot, the two submenus pick one of the ten, and both go through the same commands
			// the debug interface offers (`savestate`/`loadstate`).
			if (ImGui::MenuItem("Quick Save State", "F5", false, emu.loaded)) {
				ui_save_state(state_slot);
			}
			if (ImGui::MenuItem("Quick Load State", "F7", false, emu.loaded)) {
				ui_load_state(state_slot);
			}
			if (ImGui::BeginMenu("Save State to Slot", emu.loaded))
			{
				ui_state_slot_menu(true);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Load State from Slot", emu.loaded))
			{
				ui_state_slot_menu(false);
				ImGui::EndMenu();
			}
			if (ImGui::MenuItem("Next State Slot", "Shift+F5")) {
				ui_step_state_slot(1);
			}
			if (ImGui::MenuItem("Previous State Slot", "Shift+F7")) {
				ui_step_state_slot(-1);
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Exit", NULL)) {
				ui_active = false;
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Debug"))
		{
			// The debugger (debugui2) opens a window of its own; the item is a switch, and the
			// checkmark says whether that window is up.
			if (ImGui::MenuItem("Open Debugger...", NULL, Debug2::IsDebuggerActive())) {
				if (Debug2::IsDebuggerActive())
					Debug2::StopDebugger();
				else
					Debug2::StartDebugger();
			}

			// The HW interface profiler overlay (issue #394) drawn over the emulated picture: the
			// same switch the `hwsod` command works on, and the checkmark is the choice it keeps in
			// the settings (HW_OSD), so it is remembered for the next run.
			if (ImGui::MenuItem("HW Profiler Overlay", NULL, Debug::HwOsd::Enabled())) {
				Debug::HwOsd::SetEnabled(!Debug::HwOsd::Enabled());
			}

			if (ImGui::MenuItem("Mount DolphinSDK as DVD...", NULL)) {
				file_reaction = FileReaction::ChooseDirectory_MountSdk;
				chooseDirectoryDialog.Open();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Options"))
		{
			// Every setting of the emulator, and every device of the console, is in this one window
			// (see the "Settings" section): the menu does not carry a dialog of its own any more, and
			// what it used to carry - the pads, the memory cards and the selector view - is a page
			// of it.
			if (ImGui::MenuItem("Settings...", NULL)) {
				UiSettingsOpen();
			}

			// The machine the emulator is *not* running is configured in a window of its own:
			// its settings live in DefaultGBASettings.json and the user's GBASettings.json and
			// have nothing to do with the console's (see the "stand-alone GBA settings" module).
			if (ImGui::MenuItem("Stand-alone GBA...", NULL)) {
				UiGbaSettingsOpen();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help"))
		{
			if (ImGui::MenuItem("About...", NULL)) {
				draw_about_box = true;
			}
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}
}

/*
# The stand-alone portable machines

A cartridge of the portable machines is not an image of the console: the file selector cannot hand
it to the GameCube core, so the extension starts the emulator inside the emulator instead - the
Game Boy Advance or the Game Boy front end of issue #388, the same machine the `--gba` option of the
command line runs, with the stand-alone settings (`DefaultGBASettings.json` and the user's
`GBASettings.json` merged over it) the Options menu edits.

The front end opens a window of its own and runs its own loop, and this call does not return until
that window is closed. The loop of the console's interface is inside it for all that time, so the
list of the selector is neither drawn nor read while a portable cartridge runs: that is what the
issue asks for, and it is also what keeps a second cartridge from being started over the running
one (issue #468).
*/

/* Run one portable cartridge in the stand-alone emulator. The type the entry was listed under
   says which machine runs it: the GBA kind is the Game Boy Advance, the DMG kind is the monochrome
   Game Boy and the CGB kind is the colour one (a `.gb` cartridge under the DMG kind runs as a DMG
   even when its header knows about colour, which is what the group it is listed in asks for). */
static void run_portable_file(const std::wstring& filename, SELECTOR_FILE type)
{
	GBA::GbaSettings settings;
	GBA::GbaSettingsFiles files = GBA::FindSettingsFiles();
	std::string error;

	// The settings are the stand-alone ones, never the console's own: the window of
	// "Options -> Stand-alone GBA..." edits exactly this pair of files. A file that cannot be read
	// is not fatal - the machine runs on the built-in defaults, the way the command line's `--gba`
	// does - and the message waits for the interface to be drawn again, after the window is closed.
	if (!GBA::LoadSettings(files, settings, &error))
	{
		ui_report_error(error + " (the defaults are used)");
	}

	std::string rom = Util::WstringToString(filename);

	int result = (type == SELECTOR_FILE::Gba)
		? GBA::RunSdlFrontend(rom, false, settings)
		: GBA::RunSdlFrontendGb(rom, settings, type == SELECTOR_FILE::Dmg);

	if (result != 0)
	{
		ui_report_error("The portable emulator could not start (see the log for the reason)");
	}
}

/* Load and run the file (from the selector or from the Open dialog) */
static void load_file(const std::wstring& filename)
{
	if (filename.empty())
	{
		return;
	}

	CreateRenderTarget();
	UI::Jdi->LoadFile(Util::WstringToString(filename));
	OnMainWindowOpened(filename.c_str());
	UI::Jdi->Run();
}

/* Load and run the selected file (Enter or double click on the list item) */
static void run_selected_file()
{
	if (usel.selected < 0 || usel.selected >= (int)usel.files.size())
	{
		return;
	}

	SelectorFile* file = usel.files[usel.selected].get();

	// A cartridge of the portable machines is not a console image: it starts the stand-alone
	// emulator of its own instead (issue #468). The launch itself waits for the end of the frame
	// (see the main loop), so that the selector is drawn locked first and the user sees the veil
	// with its reason while the portable window is up. The list is not read while the emulation
	// runs, so this cannot be reached with an image already loaded.
	if (selector_type_portable(file->type))
	{
		ui_portable_file = file->name;
		ui_portable_type = file->type;
		ui_portable_pending = true;
		return;
	}

	load_file(file->name);
}

/* Run the image that was loaded last (File -> Reopen, F3), without going through the selector.
   This is the quick way back into the same game, and it is also what an unattended run uses to
   start an image while the selector is still scanning its directories. */
static void reopen_last_file()
{
	const std::string last = UI::Jdi->GetConfigString(USER_LASTFILE, USER_UI);

	if (last.empty())
	{
		return;
	}

	load_file(Util::StringToWstring(last));
}

/*

# Benchmark mode

`--bench <file> [seconds]` runs the loaded image unattended for the requested number of seconds and
measures it. The measurement itself, and its report, live in bench.cpp, because the headless build
uses exactly the same one; this front end only adds the window that the video output needs and pumps
its events while the benchmark runs.

*/

static bool ui_bench_pump()
{
	// Drain the events, so that the render window stays responsive, and end the run when it is
	// closed (`SDL_QUIT`) instead of leaving the process hanging.
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		if (event.type == SDL_QUIT)
		{
			return false;
		}
	}

	return true;
}

static void ui_bench()
{
	CreateRenderTarget();

	UI::Jdi->LoadFile(Util::WstringToString(cmdline.benchFile));
	OnMainWindowOpened(cmdline.benchFile.c_str());
	UI::Jdi->Run();

	Bench::Measure(cmdline.benchFile, cmdline.benchSeconds, ui_bench_pump);

	UI::Jdi->Stop();
	Thread::Sleep(200);
	UI::Jdi->Unload();
	DestroyRenderTarget();
	OnMainWindowClosed();
}

/* The capsule that says what a file is: the kind of machine it is for, in the colour of the kind
   of file it is (a disk image is the second accent, the rest the first). A console image is marked
   with its extension; a cartridge of the portable machines is marked with the machine the entry
   runs it on (issue #468), because the extension a ROM happens to carry says less than that -
   a `.gb` cartridge is a DMG one here, and the badge has to say so. Its text is drawn by the draw
   list and the layout only reserves its room, so that the capsule cannot move the row it is in. */
static void ui_selector_type_badge(const std::wstring& name, SELECTOR_FILE type)
{
	std::string label;

	switch (type)
	{
		case SELECTOR_FILE::Dmg: label = "DMG"; break;
		case SELECTOR_FILE::Cgb: label = "CGB"; break;
		case SELECTOR_FILE::Gba: label = "GBA"; break;
		default:
		{
			const wchar_t* dot = wcsrchr(name.c_str(), L'.');

			if (dot == nullptr || dot[1] == 0)
			{
				return;
			}

			// Only the extensions the selector lists reach this point, but a file of any name can
			// be dropped into a directory that is scanned.
			for (const wchar_t* p = dot + 1; *p != 0; p++)
			{
				if (*p > 0x7f)
				{
					return;
				}

				char ch = (char)*p;
				label += (ch >= 'a' && ch <= 'z') ? (char)(ch - 'a' + 'A') : ch;
			}
			break;
		}
	}

	const UiPalette& pal = UiThemePalette();
	const ImU32 color = (type == SELECTOR_FILE::Dvd) ? pal.accent2 : pal.accent;

	const float height = ImGui::GetTextLineHeight();
	const float width = ImGui::CalcTextSize(label.c_str()).x + 14.0f;

	const ImVec2 pos = ImGui::GetCursorScreenPos();
	const ImVec2 capsuleMin = ImVec2(pos.x, pos.y - 2.0f);
	const ImVec2 capsuleMax = ImVec2(pos.x + width, pos.y + height + 2.0f);
	const float rounding = (capsuleMax.y - capsuleMin.y) * 0.5f;

	ImDrawList* dl = ImGui::GetWindowDrawList();
	dl->AddRectFilled(capsuleMin, capsuleMax, UiThemeAlpha(color, 0.16f), rounding);
	dl->AddRect(capsuleMin, capsuleMax, UiThemeAlpha(color, 0.45f), rounding);
	dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(pos.x + 7.0f, pos.y), color, label.c_str());

	ImGui::Dummy(ImVec2(width, height));
}

/* What the selector shows when it has nothing to list: the mark of the emulator and where the files
   come from. An empty table would say the same thing in a colder way. */
static void ui_selector_empty()
{
	const UiPalette& pal = UiThemePalette();

	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const ImVec2 avail = ImGui::GetContentRegionAvail();

	const float cubeSize = 96.0f;
	const ImVec2 center = ImVec2(origin.x + avail.x * 0.5f, origin.y + avail.y * 0.5f - 40.0f);

	UiThemeDrawCube(ImGui::GetWindowDrawList(), center, cubeSize, 0.30f);

	// The lines are centered by hand: the child holds a table in the other branch, so there is no
	// layout to center them with.
	const char* title = "Nothing to run yet";
	const char* hint = "Add the directories the selector scans in Options -> Settings, General";

	auto centered = [&](const char* text, float y, ImU32 color)
	{
		const ImVec2 size = ImGui::CalcTextSize(text);

		ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
			ImVec2(origin.x + (avail.x - size.x) * 0.5f, y), color, text);
	};

	centered(title, center.y + cubeSize * 0.5f + 24.0f, pal.text);
	centered(hint, center.y + cubeSize * 0.5f + 48.0f, pal.muted);
}

/* The height of the status bar. The selector is a child of the window and has to leave this much at
   its bottom, so the two share the one description of it. */
static float ui_status_bar_height()
{
	return ImGui::GetTextLineHeight() + 12.0f;
}

static void ui_selector()
{
	if (!usel.active)
	{
		return;
	}

	if (usel.needUpdate)
	{
		update_selector();
	}

	// While the emulation runs the list is a view of what could have been started and not a thing
	// to click: a stray key or click would load another image over the running one, and the picture
	// the user is watching is in the video output window anyway. The list is drawn disabled behind
	// a veil that says why, and the keyboard handling below is skipped with it (issue #458).
	//
	// A portable cartridge that is about to start (the end of this frame launches it, see the main
	// loop) is locked in the same way: the veil is what the console's window shows behind the
	// window of the portable machine for as long as that machine runs.
	const bool locked = emu_running || ui_portable_pending;

	if (locked)
	{
		ImGui::BeginDisabled();
	}

	const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y * 2.0f + ui_status_bar_height();
	if (ImGui::BeginChild("selector", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar))
	{
		const float iconWidth = (float)(usel.smallIcons ? (DVD_BANNER_WIDTH >> 1) : DVD_BANNER_WIDTH);
		const float iconHeight = (float)(usel.smallIcons ? (DVD_BANNER_HEIGHT >> 1) : DVD_BANNER_HEIGHT);
		const float rowHeight = iconHeight + ImGui::GetStyle().CellPadding.y * 2;

		// A banner makes a row much taller than a line of text, so the text is centered in it rather
		// than sitting at its top.
		const float textOffset = (rowHeight - ImGui::GetTextLineHeight()) * 0.5f - ImGui::GetStyle().CellPadding.y;

		if (usel.files.empty())
		{
			ui_selector_empty();
		}
		else if (ImGui::BeginTable("selector_grid", 5,
			ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
			ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
			ImVec2(0, ImGui::GetContentRegionAvail().y)))
		{
			ImGui::TableSetupScrollFreeze(0, 1);

			ImGui::TableSetupColumn(Util::WstringToString(SELECTOR_COLUMN_BANNER).c_str(), ImGuiTableColumnFlags_WidthFixed, iconWidth + 8);
			ImGui::TableSetupColumn(Util::WstringToString(SELECTOR_COLUMN_TITLE).c_str(), ImGuiTableColumnFlags_WidthFixed, 200);
			ImGui::TableSetupColumn(Util::WstringToString(SELECTOR_COLUMN_SIZE).c_str(), ImGuiTableColumnFlags_WidthFixed, 70);
			ImGui::TableSetupColumn(Util::WstringToString(SELECTOR_COLUMN_GAMEID).c_str(), ImGuiTableColumnFlags_WidthFixed, 70);
			ImGui::TableSetupColumn(Util::WstringToString(SELECTOR_COLUMN_COMMENT).c_str(), ImGuiTableColumnFlags_WidthStretch);

			ImGui::TableHeadersRow();

			for (int i = 0; i < (int)usel.files.size(); i++)
			{
				SelectorFile* file = usel.files[i].get();

				ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
				ImGui::TableSetColumnIndex(0);

				ImVec2 iconPos = ImGui::GetCursorScreenPos();

				ImGui::PushID(i);
				if (ImGui::Selectable("##item", i == usel.selected,
					ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0, rowHeight)))
				{
					selector_set_selected(i, false);

					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					{
						run_selected_file();
					}
				}
				ImGui::PopID();

				// The banner is drawn over the selected item, because the item itself is a Selectable
				if (file->banner.texture)
				{
					// ImGui draws the interface through the same renderer as the banners and sets
					// a blend mode of its own there, so the texture asks for its own again here.
					SDL_SetTextureBlendMode(file->banner.texture, SDL_BLENDMODE_BLEND);

					ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)file->banner.texture,
						ImVec2(iconPos.x + 2, iconPos.y),
						ImVec2(iconPos.x + 2 + iconWidth, iconPos.y + iconHeight));
				}

				ImGui::TableSetColumnIndex(1);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + textOffset);
				ui_selector_type_badge(file->name, file->type);
				ImGui::SameLine();
				ImGui::TextUnformatted(ToUtf8(file->title).c_str());

				ImGui::TableSetColumnIndex(2);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + textOffset);
				ImGui::TextUnformatted(SmartSize(file->size).c_str());

				ImGui::TableSetColumnIndex(3);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + textOffset);
				ImGui::PushStyleColor(ImGuiCol_Text, UiThemeVec4(UiThemePalette().accent));
				ImGui::TextUnformatted(Util::WstringToString(file->id).c_str());
				ImGui::PopStyleColor();

				ImGui::TableSetColumnIndex(4);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + textOffset);
				ImGui::PushStyleColor(ImGuiCol_Text, UiThemeVec4(UiThemePalette().muted));
				ImGui::TextUnformatted(ToUtf8(file->comment).c_str());
				ImGui::PopStyleColor();

				if (usel.scrollToSelected && i == usel.selected)
				{
					ImGui::SetScrollHereY(0.5f);
					usel.scrollToSelected = false;
				}
			}

			ImGui::EndTable();
		}

		// The table does not handle the keyboard, so the cursor is moved by the usual
		// cursor keys.
		if (!locked && ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows | ImGuiFocusedFlags_RootWindow))
		{
			if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
			{
				selector_set_selected(usel.selected - 1, true);
			}
			if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
			{
				selector_set_selected(usel.selected + 1, true);
			}
			if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
			{
				run_selected_file();
			}

			ImGuiIO& io = ImGui::GetIO();
			for (int i = 0; i < io.InputQueueCharacters.Size; i++)
			{
				if (io.InputQueueCharacters[i] >= L' ')
				{
					selector_type_ahead((wchar_t)io.InputQueueCharacters[i]);
				}
			}
		}

		// The veil of the disabled list: the whole child is covered with the colour of the window
		// and the reason, so that the dimmed rows read as "not now" rather than as a broken list.
		//
		// The state is read again here and not taken from `locked`: a double click on a portable
		// cartridge and the Enter key both ask for it above, and the frame that saw the request
		// must already be drawn with the veil - the machine starts at the end of it, and the
		// console's window stays as it is for as long as the window of the portable machine is up.
		// The veil is drawn after the keyboard, so that both ways of asking are covered.
		if (emu_running || ui_portable_pending)
		{
			const UiPalette& pal = UiThemePalette();

			ImDrawList* dl = ImGui::GetWindowDrawList();
			const ImVec2 min = ImGui::GetWindowPos();
			const ImVec2 size = ImGui::GetWindowSize();
			const ImVec2 max = ImVec2(min.x + size.x, min.y + size.y);

			dl->AddRectFilled(min, max, UiThemeAlpha(pal.window, 0.72f));

			// The reason the list is not clickable: the machine that runs, and how to stop it. A
			// portable cartridge runs in a window of its own and the console's File menu cannot
			// close it, so the two cases say different things.
			const char* portableLines[] = { "The portable emulation is running", "Close its window to stop it" };
			const char* consoleLines[] = { "The emulation is running", "File -> Close stops it" };
			const char* const* lines = ui_portable_pending ? portableLines : consoleLines;
			const int lineCount = 2;

			float y = min.y + size.y * 0.5f - (ImGui::GetTextLineHeightWithSpacing() * lineCount) * 0.5f;

			for (int i = 0; i < lineCount; i++)
			{
				const ImVec2 textSize = ImGui::CalcTextSize(lines[i]);

				dl->AddText(ImVec2(min.x + (size.x - textSize.x) * 0.5f, y),
					(i == 0) ? pal.text : pal.muted, lines[i]);

				y += ImGui::GetTextLineHeightWithSpacing();
			}
		}

	}
	ImGui::EndChild();

	if (locked)
	{
		ImGui::EndDisabled();
	}
}

/* The text cut to the width there is for it, with an ellipsis where it was cut. The status line is
   the one place where a string the user never sees the end of ("State loaded from ...") would run
   into the counters on its right. The string is walked by UTF-8 sequences and not by bytes, so a
   title in Japanese is not cut in the middle of a character. */
static std::string Ellipsize(const char* text, float width)
{
	if (text == nullptr || text[0] == 0)
	{
		return "";
	}

	if (ImGui::CalcTextSize(text).x <= width)
	{
		return text;
	}

	const float limit = width - ImGui::CalcTextSize("...").x;

	if (limit <= 0.0f)
	{
		return "";
	}

	size_t length = strlen(text);
	size_t cut = 0;

	while (cut < length)
	{
		const unsigned char lead = (unsigned char)text[cut];
		size_t step = (lead >= 0xF0) ? 4 : (lead >= 0xE0) ? 3 : (lead >= 0xC0) ? 2 : 1;

		if (cut + step > length)
		{
			break;
		}

		std::string candidate(text, cut + step);

		if (ImGui::CalcTextSize(candidate.c_str()).x > limit)
		{
			break;
		}

		cut += step;
	}

	return std::string(text, cut) + "...";
}

/*

The line at the bottom of the window: what the machine is doing. Its parts are the ones the core
fills in through `SetStatusText` (the state of the emulation and the two clocks of the performance
thread), and this function only lays them out: the state on the left and the clocks on the right -
the time the emulation has been running, as the console counts it and as the host does (issue #458).

*/

static void ui_status_bar()
{
	const UiPalette& pal = UiThemePalette();

	const float height = ui_status_bar_height();
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const float width = ImGui::GetContentRegionAvail().x;

	ImGui::Dummy(ImVec2(width, height));

	ImDrawList* dl = ImGui::GetWindowDrawList();
	const ImVec2 lowerRight = ImVec2(origin.x + width, origin.y + height);
	const float middleY = origin.y + height * 0.5f;

	dl->AddRectFilled(origin, lowerRight, pal.child, 9.0f);
	dl->AddRect(origin, lowerRight, pal.border, 9.0f);

	// The two clocks are laid out from the right edge, so that they stay where they are while the
	// line of the state changes length (issue #458: the console's clock and the host's).
	struct StatusPart
	{
		std::string text;
		ImU32       color;
	};

	StatusPart parts[] =
	{
		{ Util::WstringToString(status_parts[(int)STATUS_ENUM::EmuTime]),  pal.accent },
		{ Util::WstringToString(status_parts[(int)STATUS_ENUM::WallTime]), pal.muted },
	};

	const float gap = 22.0f;
	float partsWidth = 0.0f;
	int partsShown = 0;

	for (const StatusPart& part : parts)
	{
		if (part.text.empty())
		{
			continue;
		}

		partsWidth += ImGui::CalcTextSize(part.text.c_str()).x;
		partsShown++;
	}

	if (partsShown > 1)
	{
		partsWidth += (partsShown - 1) * gap;
	}

	const float partsX = origin.x + width - 14.0f - partsWidth;

	// The state on the left: a lamp in the colour of the state and the line the core wrote. It is
	// cut where the counters begin, because that line can be a path of any length.
	const std::string progress = Ellipsize(Util::WstringToString(status_parts[(int)STATUS_ENUM::Progress]).c_str(),
		partsX - origin.x - 44.0f);

	dl->AddCircleFilled(ImVec2(origin.x + 16.0f, middleY), 4.0f, emu_running ? pal.accent : pal.muted);

	ImGui::SetCursorScreenPos(ImVec2(origin.x + 28.0f, middleY - ImGui::GetTextLineHeight() * 0.5f));
	ImGui::PushStyleColor(ImGuiCol_Text, UiThemeVec4(pal.text));
	ImGui::TextUnformatted(progress.c_str());
	ImGui::PopStyleColor();

	float x = partsX;

	for (const StatusPart& part : parts)
	{
		if (part.text.empty())
		{
			continue;
		}

		if (x > partsX)
		{
			// A dot between two parts, in the colour of the hairline.
			dl->AddCircleFilled(ImVec2(x - gap * 0.5f, middleY), 1.5f, pal.border);
		}

		ImGui::SetCursorScreenPos(ImVec2(x, middleY - ImGui::GetTextLineHeight() * 0.5f));
		ImGui::PushStyleColor(ImGuiCol_Text, UiThemeVec4(part.color));
		ImGui::TextUnformatted(part.text.c_str());
		ImGui::PopStyleColor();

		x += ImGui::CalcTextSize(part.text.c_str()).x + gap;
	}
}

static void ui_main_window()
{
	static bool use_work_area = true;
	static ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(use_work_area ? viewport->WorkPos : viewport->Pos);
	ImGui::SetNextWindowSize(use_work_area ? viewport->WorkSize : viewport->Size);

	// The window is the whole client area of the window SDL opened, so it is square and carries no
	// frame of its own: the system rounds and outlines that window, and a second rounding drawn
	// inside it only shows as a seam in the corners. The rounding of the theme belongs to the
	// dialogs, which are drawn inside this window.
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

	if (ImGui::Begin("main_window", nullptr, flags))
	{
		ui_main_menu();
		ui_selector();
		ui_status_bar();
	}
	ImGui::End();

	ImGui::PopStyleVar(2);
}

static int ui_main()
{
	EMUCtor();

	// Create an interface for communicating with the emulator core
	UI::Jdi = new UI::JdiClient;

	// The command line may ask for an empty drive (the IPL then takes its "no disk" path,
	// which is the one that shows the cube animation).

	if (cmdline.noDisc)
	{
		UI::Jdi->DvdOpenCover();
	}

	// Add UI methods
	JdiAddNode("UI_JDI_JSON", JdiSpecs::UiJdi, UIReflector);
	JdiAddNode("DEBUG_UI2_JDI_JSON", JdiSpecs::DebugUi2Jdi, Debug2::Reflector);

	// The local MCP server (issue #383): an MCP client that started the emulator drives its debug
	// interface over stdin/stdout while the window stays open and usable.
	if (cmdline.mcp)
	{
		Mcp::StartTransport();
	}

	// Start the user interface

	// The file browser titles and type filters are set by open_file_dialog, so that every dialog
	// gets the ones that belong to it; only the directory browser is fixed.
	chooseDirectoryDialog.SetTitle("Choose Directory");

	// Selector state (see the "Game selector" section)
	usel.active = UI::Jdi->GetConfigBool(USER_SELECTOR, USER_UI);
	usel.smallIcons = UI::Jdi->GetConfigBool(USER_SMALLICONS, USER_UI);
	usel.sortBy = (SELECTOR_SORT)UI::Jdi->GetConfigInt(USER_SORTVIEW, USER_UI);
	usel.bannerBg = (SELECTOR_BANNER_BG)UI::Jdi->GetConfigInt(USER_BANNER_BG, USER_UI);
	usel.bannerBgColor = (ImU32)UI::Jdi->GetConfigInt(USER_BANNER_BG_COLOR, USER_UI);

	CreateStatusBar();

	// From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
	SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

	// Create window with SDL_Renderer graphics context
	//
	// The window is the game selector, and it opens wide screen (16:9): that is the shape the table
	// of the selector - with the banners next to the titles - and the pages of the settings window
	// are read in. The size is capped by the work area of the display, because a 1280x720 window
	// does not fit above the taskbar of a 1366x768 screen, and the status line at the bottom of the
	// window is the last thing that may end up under it. Asking the display needs the video
	// subsystem, which SDL_CreateWindow below would initialize by itself; it is asked for here,
	// before the size is known. The call is counted, so the one SDL makes later is a no-op.
	SDL_InitSubSystem(SDL_INIT_VIDEO);

	int windowWidth = 1280, windowHeight = 720;

	SDL_Rect workArea = { 0, 0, 0, 0 };

	if (SDL_GetDisplayUsableBounds(0, &workArea) == 0 && workArea.w > 0 && workArea.h > 0)
	{
		const int maxWidth = workArea.w * 9 / 10;
		const int maxHeight = workArea.h * 9 / 10;

		if (windowHeight > maxHeight)
		{
			windowHeight = maxHeight;
			windowWidth = windowHeight * 16 / 9;
		}

		if (windowWidth > maxWidth)
		{
			windowWidth = maxWidth;
			windowHeight = windowWidth * 9 / 16;
		}
	}

	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	window = SDL_CreateWindow(APPNAME_A, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, window_flags);
	SetWindowIcon(window);
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_SOFTWARE);
	if (renderer == nullptr) {
		SDL_Log("Error creating SDL_Renderer!");
		return -1;
	}

	// simulate close operation, like we just stopped emu
	OnMainWindowClosed();

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// The look of the interface: the font atlas and the palette of the theme the configuration asks
	// for (see uitheme.h). The style is applied once, here, and everything that is drawn after this
	// point - the selector, the settings window and the dialogs - is drawn with it.
	//
	// The atlas is baked for the pixels the display really has. The renderer answers how many
	// framebuffer pixels a logical pixel of the window is (1.25 and 1.5 are what a Windows desktop at
	// 125% and 150% answers), and a face baked at the logical size is scaled up to them by the
	// renderer, which scales a texture with linear filtering: that is the text coming out soft.
	int clientWidth = 0, clientHeight = 0;
	int framebufferWidth = 0, framebufferHeight = 0;

	SDL_GetWindowSize(window, &clientWidth, &clientHeight);
	SDL_GetRendererOutputSize(renderer, &framebufferWidth, &framebufferHeight);

	const float uiScale = (clientWidth > 0 && framebufferWidth > 0)
		? (float)framebufferWidth / (float)clientWidth
		: 1.0f;

	UiThemeLoadFonts(uiScale);
	UiThemeApplyByName(UI::Jdi->GetConfigString(USER_THEME, USER_UI).c_str());

	// Setup Platform/Renderer backends
	ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer2_Init(renderer);

	ui_active = true;

	// The command line may ask for an unattended benchmark run of a specific image.

	if (cmdline.bench)
	{
		ui_bench();
		ui_active = false;
	}

	// The command line may ask to start the IPL right away (as if File -> Run Bootrom was clicked),
	// or to run one specific file instead of waiting for the selector. `--ipl` together with a disk
	// image starts the IPL *with that disk in the drive*: the boot ROM finds it on the DI and boots
	// it by itself, which is what the real console does. Without an image the IPL is started on its
	// own and takes the "no disk" path (or shows the console menu).

	if (cmdline.ipl)
	{
		if (cmdline.image.empty())
		{
			ui_load_bootrom();
		}
		else
		{
			CreateRenderTarget();
			UI::Jdi->LoadFile(Util::WstringToString(cmdline.image));
			OnMainWindowOpened(cmdline.image.c_str());
			UI::Jdi->Run();
		}
	}
	else if (!cmdline.image.empty())
	{
		CreateRenderTarget();
		UI::Jdi->LoadFile(Util::WstringToString(cmdline.image));
		OnMainWindowOpened(cmdline.image.c_str());
		UI::Jdi->Run();
	}
  
	// The emulator has more than one SDL window: the video output is a separate window, which can
	// cover the main window with the selector. Only the events of the main window are passed to
	// ImGui, otherwise the movements and clicks over the video output window are interpreted as
	// movements and clicks over the selector (the ImGui backend does not filter events by window).
	const Uint32 mainWindowID = SDL_GetWindowID(window);

	// Main loop

	while (ui_active) {

		SDL_Event event;
		while (SDL_PollEvent(&event)) {

			// The new debugger (debugui2) draws into a window of its own and takes the events
			// that belong to it. Everything else goes on to ImGui as before.
			if (Debug2::UiSdlEvent(event))
			{
				continue;
			}

			bool forMainWindow = true;

			switch (event.type)
			{
				case SDL_WINDOWEVENT:     forMainWindow = (event.window.windowID == mainWindowID); break;
				case SDL_KEYDOWN:
				case SDL_KEYUP:           forMainWindow = (event.key.windowID == mainWindowID); break;
				case SDL_MOUSEMOTION:     forMainWindow = (event.motion.windowID == mainWindowID); break;
				case SDL_MOUSEBUTTONDOWN: forMainWindow = (event.button.windowID == mainWindowID); break;
				// SDL_MOUSEBUTTONUP is always passed to ImGui: the backend keeps its own state of the
				// pressed buttons and captures the mouse with SDL_CaptureMouse() while any button is
				// down. If the release event (which may be delivered to another window) is skipped, the
				// mouse stays captured by the main window and the other windows stop responding to it.
				case SDL_MOUSEWHEEL:      forMainWindow = (event.wheel.windowID == mainWindowID); break;
				// The game controllers are used by the pad backend (see padsdl.cpp). They are only
				// passed to ImGui while the selector is shown, so that a gamepad can navigate the UI,
				// but does not move the ImGui cursor while a game is running.
				case SDL_CONTROLLERAXISMOTION:
				case SDL_CONTROLLERBUTTONDOWN:
				case SDL_CONTROLLERBUTTONUP:
				case SDL_CONTROLLERDEVICEADDED:
				case SDL_CONTROLLERDEVICEREMOVED:
				case SDL_CONTROLLERDEVICEREMAPPED: forMainWindow = !emu_running; break;
				default: break;
			}

			// File -> Reopen (F3): reload the image that was loaded last. The menu item is the
			// primary way in, this is the shortcut for it.
			if (forMainWindow && !emu_running && !UiSettingsCaptureActive() && !UiGbaSettingsCaptureActive() &&
				event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_F3)
			{
				reopen_last_file();
			}

			// File -> Quick Save State (F5) and Quick Load State (F7), with Shift+ the same keys
			// stepping the slot. Unlike the reopen shortcut these work while a game runs - a state
			// is taken in the middle of one - and they are why the two keys are F5/F7 rather than
			// the F3/F4 the GBA front end uses (F3 is taken here).
			if (forMainWindow && !UiSettingsCaptureActive() && !UiGbaSettingsCaptureActive() && emu.loaded &&
				event.type == SDL_KEYDOWN &&
				(event.key.keysym.scancode == SDL_SCANCODE_F5 ||
					event.key.keysym.scancode == SDL_SCANCODE_F7))
			{
				bool write = (event.key.keysym.scancode == SDL_SCANCODE_F5);

				if (event.key.keysym.mod & KMOD_SHIFT)
				{
					ui_step_state_slot(write ? 1 : -1);
				}
				else if (write)
				{
					ui_save_state(state_slot);
				}
				else
				{
					ui_load_state(state_slot);
				}
			}

			// The settings window captures the next key press or game controller event as the
			// binding of a control (see uisettings.cpp). The captured event must not reach ImGui,
			// otherwise it would also move the selector cursor, trigger a menu item or navigate the
			// interface.
			if (UiGbaSettingsSdlEvent(event, mainWindowID) || UiSettingsSdlEvent(event, mainWindowID))
			{
				forMainWindow = false;
			}

			if (forMainWindow)
			{
				ImGui_ImplSDL2_ProcessEvent(&event);
			}

			if (event.type == SDL_QUIT)
				ui_active = false;
			if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == mainWindowID)
				ui_active = false;
		}

		// Keep the host game controllers open, and their cached state fresh, for the peripheral
		// devices that are driven by them (see padsdl.cpp). The events pumped above have the device
		// list up to date.
		HostInputUpdate();

		// The frame rate and the backend the performance thread measured go into the names of the
		// two windows from here: SDL wants a window touched from the thread that owns it, and this
		// loop is that thread (see the note on the window titles).
		if (ui_title_dirty.exchange(false))
		{
			ui_update_window_titles();
		}

		// The release of a pressed button can be delivered to another window or to another
		// application, so do not leave the mouse captured by the main window when it is not active.
		if ((SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) == 0)
		{
			for (Uint8 button = SDL_BUTTON_LEFT; button <= SDL_BUTTON_X2; button++)
			{
				SDL_Event up = { 0 };
				up.type = SDL_MOUSEBUTTONUP;
				up.button.type = SDL_MOUSEBUTTONUP;
				up.button.windowID = mainWindowID;
				up.button.button = button;
				up.button.state = SDL_RELEASED;
				ImGui_ImplSDL2_ProcessEvent(&up);
			}
		}

		// Start the Dear ImGui frame
		ImGui_ImplSDLRenderer2_NewFrame();
		ImGui_ImplSDL2_NewFrame();

		// While the application is focused, the ImGui SDL2 backend restores the mouse position from
		// the global mouse state, even if the pointer is over another window (for example, over the
		// video output window). Tell ImGui that there is no mouse, so that the covered selector is
		// not highlighted under the pointer.
		if ((SDL_GetWindowFlags(window) & SDL_WINDOW_MOUSE_FOCUS) == 0)
		{
			ImGui::GetIO().AddMousePosEvent(-FLT_MAX, -FLT_MAX);
		}

		ImGui::NewFrame();

		// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
		ui_main_window();

		UiSettingsFrame();
		UiGbaSettingsFrame();

		if (show_demo_window) {
			ImGui::ShowDemoWindow(&show_demo_window);
		}
		
		fileOpenDialog.Display();
		if (fileOpenDialog.HasSelected())
		{
			auto name = fileOpenDialog.GetSelected().string();
			fileOpenDialog.ClearSelected();

			switch (file_reaction)
			{
				case FileReaction::OpenFile_LoadFile:
					if (!name.empty())
					{
						load_file(Util::StringToWstring(name));
					}
					break;

				case FileReaction::ChooseFile_DVDImage:
					if (!name.empty() && UI::Jdi->DvdCoverOpened())
					{
						// Bad?
						if (!UI::Jdi->DvdMount(name)) {
							break;
						}
						// Close lid
						UI::Jdi->DvdCloseCover();
					}
					break;
			}

			file_reaction = FileReaction::None;
		}

		// The directory browser of the front end picks the Dolphin SDK folder; the directories the
		// selector scans are picked by the browser of the settings window (see uisettings.cpp).

		chooseDirectoryDialog.Display();
		if (chooseDirectoryDialog.HasSelected())
		{
			auto name = chooseDirectoryDialog.GetSelected().string();
			chooseDirectoryDialog.ClearSelected();

			// Dolphin SDK folder as a virtual disk (the same thing `MountSDK` does from the
			// command line). The path is UTF-8 already, as JDI wants it.
			if (file_reaction == FileReaction::ChooseDirectory_MountSdk && !name.empty())
			{
				UI::Jdi->DvdMountSDK(name);
			}

			file_reaction = FileReaction::None;
		}

		if (draw_message_box) {
			ui_draw_message_box(&draw_message_box);
		}

		if (draw_error_box) {
			ui_draw_error_box(&draw_error_box);
		}

		if (draw_about_box) {
			ui_draw_about_box(&draw_about_box);
		}

		// Rendering
		ImGui::Render();
		ImGuiIO& io = ImGui::GetIO();
		SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);

		// The window is cleared in the background of the theme: the windows are rounded and the work
		// area of the main one does not always cover the whole viewport, so the colour behind them is
		// a part of the look and not an arbitrary one. It is read every frame, so that a theme picked
		// in the settings window takes effect here too.
		const ImVec4 background = UiThemeVec4(UiThemePalette().window);
		SDL_SetRenderDrawColor(renderer, (Uint8)(background.x * 255), (Uint8)(background.y * 255), (Uint8)(background.z * 255), 255);
		SDL_RenderClear(renderer);
		ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
		SDL_RenderPresent(renderer);

		// The new debugger draws its own window on top of the same frame loop. If the user closed
		// that window, the debugger is stopped here, outside of its own event callback.
		Debug2::Frame();
		if (Debug2::CloseRequested())
		{
			Debug2::StopDebugger();
		}

		// The cartridge of a portable machine the selector asked for runs the stand-alone front end
		// of its own, and this call does not return until its window is closed (issue #468). It is
		// started here, after the frame that drew the selector locked and veiled has been presented,
		// so the console's window says why the list is not clickable for as long as the portable
		// machine is up. The call is on the thread that owns the console's SDL objects, so it is a
		// frame of this loop and not a second front end running beside this one.
		if (ui_portable_pending)
		{
			std::wstring file = ui_portable_file;
			SELECTOR_FILE type = ui_portable_type;

			ui_portable_pending = false;
			ui_portable_file.clear();

			run_portable_file(file, type);
		}

		SDL_Delay(10);
	}

	// Main window closed

	// Cleanup
	usel.clear();       // the banner textures must be destroyed before the renderer

	// The new debugger owns an SDL window and a GL context of its own, so it goes before the
	// renderer and the video subsystem do.
	Debug2::StopDebugger();

	ImGui_ImplSDLRenderer2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	UI::Jdi->Unload();

	JdiRemoveNode("UI_JDI_JSON");
	JdiRemoveNode("DEBUG_UI2_JDI_JSON");

	EMUDtor();
	UI::Jdi->ExecuteCommand("exit");

	return 0;
}

#ifdef _WINDOWS

// Entry point for Win32 applications, quickly going straight to SDL

int WINAPI WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nShowCmd )
{
	EMUParseCmdLine(lpCmdLine);

	if (cmdline.help)
	{
		EMUPrintUsage();
		return 0;
	}

	if (cmdline.selftest)
	{
		return EMUSelfTest();
	}

	// The GBA emulator is a separate machine inside the same executable (issue #388): when the
	// command line asks for it - `--gba`, or a file with a Game Boy extension - the GBA frontend
	// takes over instead of the GameCube UI.
	if (cmdline.gba)
	{
		return EMURunGba();
	}

	return ui_main();
}

#else

int main(int argc, char** argv)
{
	EMUParseCmdLine(argc, argv);

	if (cmdline.help)
	{
		EMUPrintUsage();
		return 0;
	}

	if (cmdline.selftest)
	{
		return EMUSelfTest();
	}

	if (cmdline.gba)
	{
		return EMURunGba();
	}

	return ui_main();
}

#endif
