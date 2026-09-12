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
#include "res/pureikyubu_icon.h"

static bool ui_active = false;
static bool show_demo_window = false;
static SDL_Window* window;
static SDL_Window* render_target;
static SDL_Renderer* renderer;
static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
static bool debugger_enabled_check = false;
static bool draw_error_box = false;
static bool draw_message_box = false;
static std::string error_text;
static std::string message_text;
static ImGui::FileBrowser fileOpenDialog(ImGuiFileBrowserFlags_CloseOnEsc);
static ImGui::FileBrowser fileSaveDialog(ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_EnterNewFilename);
static ImGui::FileBrowser chooseDirectoryDialog(ImGuiFileBrowserFlags_SelectDirectory);
static bool draw_about_box = false;
static bool ui_insert_dvd_menu_item_enabled = false;

enum class FileReaction
{
	None = 0,
	OpenFile_LoadFile,
	ChooseDirectory_MountSdk,
	ChooseDirectory_SelectorPath,
	OpenFile_Bootrom,
	OpenFile_DROM,
	OpenFile_IROM,
	OpenFile_MemcardA,
	OpenFile_MemcardB,
	ChooseFile_DVDImage,
};
static FileReaction file_reaction = FileReaction::None;

static uint16_t* SjisToUnicode(wchar_t* sjisText, size_t* size, size_t* chars)
{
	uint16_t* unicodeText, * ptrU, uchar, schar;
	wchar_t* ptrS;

	*size = (wcslen(sjisText) + 1) * sizeof(wchar_t);
	unicodeText = (uint16_t*)malloc(*size);
	assert(unicodeText);
	memset(unicodeText, 0, *size);

	ptrU = unicodeText;
	ptrS = sjisText;
	*chars = 0;

	schar = *ptrS;
	while (schar != 0)
	{
		uchar = SjisTable[schar];
		if (uchar == 0xFFFF)
		{
			ptrS++;
			schar = (schar << 8) | *ptrS;
			uchar = SjisTable[schar];
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

		auto buffer =
			Util::StringToWstring(APPNAME_A) + L" - " + std::wstring(APPDESC) + L"\n" +
			std::wstring(L"Copyright 2003-2026 Dolwin team, emu-russia\n") +
			std::wstring(L"Build version ") +
			Util::StringToWstring(UI::Jdi->GetVersion()) + L" " +
			std::wstring(version) + L" " + std::wstring(platform) + L" " + std::wstring(jitc) + L" (" +
			Util::StringToWstring(dateStamp) + L" " +
			Util::StringToWstring(timeStamp) + L")\n";

		ImGui::Text(Util::WstringToString(buffer).c_str());
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
	SetStatusText(STATUS_ENUM::VIs, L"");
	SetStatusText(STATUS_ENUM::PEs, L"");
	SetStatusText(STATUS_ENUM::SystemTime, L"");
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

The SDL port of the file selector (see ui.cpp): the list of executable files (DOL/ELF) and disk
images (GCM/ISO) found in the configured paths, with the disk banners, titles, sizes and comments
taken from the DVD banner file. The list of paths is stored in the PATH user variable and is
extended with the directory of every loaded file.

*/

// Set by OnMainWindowOpened / OnMainWindowClosed
static bool emu_running = false;

/* File entry */
struct SelectorFile
{
	SELECTOR_FILE   type;               // Executable or Dvd
	size_t          size;               // File size
	std::wstring    id;                 // GameID = DiskID
	std::wstring    name;               // File path and name
	std::wstring    title;              // Alternate file name (from the banner)
	std::wstring    comment;            // Some notes (from the banner)
	SDL_Texture* banner = nullptr;      // Banner texture (Dvd only)
};

/* All important data is placed here */
class UserSelector
{
public:

	bool            active = false;                     // 1, if enabled
	bool            smallIcons = false;                 // show small icons
	SELECTOR_SORT   sortBy = SELECTOR_SORT::Default;    // sort rule (one of SELECTOR_SORT_*)

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
			if (file->banner)
			{
				SDL_DestroyTexture(file->banner);
			}
		}

		files.clear();
		selected = -1;
	}
};

static UserSelector usel;

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

/* Convert the SJIS text of the Japanese banners to Unicode (see SjisToUnicode in ui.cpp) */
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

/* Nice value of KB, MB or GB, for output (see UI::FileSmartSizeA in ui.cpp) */
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

/* Convert the DVD banner (RGB5A3 texture) into an RGBA texture.
   The banner image is stored as 4x4 tiles (the same layout as in the GX texture), so the pixels of
   a tile are scattered over the whole image (see add_banner in ui.cpp).
   The Win32 selector pre-blends the banner with the item background, because the listview cannot
   draw translucent bitmaps. Here the alpha channel is kept, so that ImGui blends the banner with
   the row background (including the selection highlight) by itself. */
static SDL_Texture* make_banner_texture(const uint8_t* image)
{
	const int tiles = (DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT) / 16;
	std::vector<uint32_t> pixels(DVD_BANNER_WIDTH * DVD_BANNER_HEIGHT);

	const uint16_t* tile = (const uint16_t*)image;
	int row = 0, col = 0;

	for (int i = 0; i < tiles; i++, tile += 16)
	{
		for (int j = 0; j < 4; j++)
		{
			for (int k = 0; k < 4; k++)
			{
				uint16_t p = tile[j * 4 + k];
				p = (p << 8) | (p >> 8);        // banner is always big-endian

				uint8_t r, g, b, a;

				if (p & 0x8000)                 // RGB555
				{
					r = (uint8_t)(((p >> 10) & 0x1f) * 255 / 31);
					g = (uint8_t)(((p >> 5) & 0x1f) * 255 / 31);
					b = (uint8_t)((p & 0x1f) * 255 / 31);
					a = 255;
				}
				else                            // RGB4A3
				{
					r = (uint8_t)(((p >> 8) & 0x0f) * 17);
					g = (uint8_t)(((p >> 4) & 0x0f) * 17);
					b = (uint8_t)((p & 0x0f) * 17);
					a = (uint8_t)(((p >> 12) & 0x07) * 255 / 7);
				}

				pixels[(row + j) * DVD_BANNER_WIDTH + (col + k)] =
					((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
			}
		}

		col += 4;
		if (col == DVD_BANNER_WIDTH)
		{
			col = 0;
			row += 4;
		}
	}

	SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
		SDL_TEXTUREACCESS_STATIC, DVD_BANNER_WIDTH, DVD_BANNER_HEIGHT);
	if (texture == nullptr)
	{
		return nullptr;
	}

	SDL_UpdateTexture(texture, nullptr, pixels.data(), DVD_BANNER_WIDTH * sizeof(uint32_t));
	SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
	SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);      // small icons are scaled down

	return texture;
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

		item->banner = make_banner_texture(bnr->image);
	}
	else if (type == SELECTOR_FILE::Executable)
	{
		item->id = L"-";
		item->title = std::filesystem::path(file).stem().wstring();
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

	// file filter: every 8 bits masking an extension (see EditFileFilter in ui.cpp)
	uint32_t filter = (uint32_t)UI::Jdi->GetConfigInt(USER_FILTER, USER_UI);

	static const struct
	{
		const wchar_t* ext;
		SELECTOR_FILE  type;
		uint32_t       mask;
	} file_ext[] =
	{
		{ L".dol", SELECTOR_FILE::Executable, 0xff000000 },
		{ L".elf", SELECTOR_FILE::Executable, 0x00ff0000 },
		{ L".gcm", SELECTOR_FILE::Dvd,        0x0000ff00 },
		{ L".iso", SELECTOR_FILE::Dvd,        0x000000ff },
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
				if ((filter & mask.mask) && ext == mask.ext)
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

/* Options -> Selector */
static void ui_selector_menu()
{
	if (!ImGui::BeginMenu("Selector"))
	{
		return;
	}

	if (ImGui::MenuItem("Enable Selector", NULL, usel.active))
	{
		usel.active = !usel.active;
		UI::Jdi->SetConfigBool(USER_SELECTOR, usel.active, USER_UI);
		usel.needUpdate = true;
	}

	if (ImGui::MenuItem("Refresh", NULL, false, usel.active))
	{
		usel.needUpdate = true;
	}

	ImGui::Separator();

	if (ImGui::MenuItem("Small Icons", NULL, usel.smallIcons, usel.active))
	{
		usel.smallIcons = !usel.smallIcons;
		UI::Jdi->SetConfigBool(USER_SMALLICONS, usel.smallIcons, USER_UI);
	}

	if (ImGui::BeginMenu("Sort by", usel.active))
	{
		if (ImGui::MenuItem("Default", NULL, usel.sortBy == SELECTOR_SORT::Default)) sort_selector(SELECTOR_SORT::Default);
		if (ImGui::MenuItem("Filename", NULL, usel.sortBy == SELECTOR_SORT::Filename)) sort_selector(SELECTOR_SORT::Filename);
		if (ImGui::MenuItem("Title", NULL, usel.sortBy == SELECTOR_SORT::Title)) sort_selector(SELECTOR_SORT::Title);
		if (ImGui::MenuItem("Size", NULL, usel.sortBy == SELECTOR_SORT::Size)) sort_selector(SELECTOR_SORT::Size);
		if (ImGui::MenuItem("Game ID", NULL, usel.sortBy == SELECTOR_SORT::ID)) sort_selector(SELECTOR_SORT::ID);
		if (ImGui::MenuItem("Comment", NULL, usel.sortBy == SELECTOR_SORT::Comment)) sort_selector(SELECTOR_SORT::Comment);
		ImGui::Separator();
		if (ImGui::MenuItem("Unsorted", NULL, usel.sortBy == SELECTOR_SORT::Unsorted)) sort_selector(SELECTOR_SORT::Unsorted);
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("File Filter", usel.active))
	{
		uint32_t filter = (uint32_t)UI::Jdi->GetConfigInt(USER_FILTER, USER_UI);

#define SELECTOR_FILTER_ITEM(label, mask)                                                       \
		if (ImGui::MenuItem(label, NULL, (filter & mask) != 0))                                 \
		{                                                                                       \
			filter ^= mask;                                                                     \
			UI::Jdi->SetConfigInt(USER_FILTER, (int)filter, USER_UI);                           \
			usel.needUpdate = true;                                                             \
		}

		SELECTOR_FILTER_ITEM("*.dol", 0xff000000);
		SELECTOR_FILTER_ITEM("*.elf", 0x00ff0000);
		SELECTOR_FILTER_ITEM("*.gcm", 0x0000ff00);
		SELECTOR_FILTER_ITEM("*.iso", 0x000000ff);

#undef SELECTOR_FILTER_ITEM

		ImGui::EndMenu();
	}

	ImGui::Separator();

	if (ImGui::MenuItem("Add Directory...", NULL, false, usel.active))
	{
		file_reaction = FileReaction::ChooseDirectory_SelectorPath;
		chooseDirectoryDialog.Open();
	}

	ImGui::EndMenu();
}




/*

# Controller settings

The SDL port of the Win32 controller settings dialog (see PADConfigDialogProc and PADConfigure
in ui.cpp): plug the pad, assign a keyboard key and/or a gamepad button or axis to every GameCube
controller control, or clear/restore the bindings. Every control has two bindings, so the keyboard
and the gamepad can be used at the same time.

Clicking a binding button arms the capture, and the next input becomes the new binding (Esc cancels
it, like in the Win32 dialog): a key of the main window for the keyboard column (the modifier keys
and the F1-F12 keys are skipped, because the debugger uses them, see GetVKey in ui.cpp), or an SDL
game controller button or a stick/trigger deflection for the gamepad column. The captured events
are not passed to ImGui, so they cannot also move the selector cursor or navigate the UI.

The dialog edits a copy of the PADCONF of the selected pad. OK writes it to the configuration
("controllers" section) and makes the backend reread it; Cancel drops the copy.

*/

/* The names of the VKEY_FOR_* (keyboard) and GCKEY_FOR_* (gamepad) configuration variables, in the enum order */
static const char* pad_binding_suffix[VKEY_FOR_MAX] =
{
	"UP", "DOWN", "LEFT", "RIGHT",
	"XUP50", "XUP100", "XDOWN50", "XDOWN100",
	"XLEFT50", "XLEFT100", "XRIGHT50", "XRIGHT100",
	"CXUP", "CXDOWN", "CXLEFT", "CXRIGHT",
	"TRIGGERL", "TRIGGERR", "TRIGGERZ",
	"A", "B", "X", "Y", "START",
};

/* The rows of the dialog, in the display order */
static const char* pad_binding_label[VKEY_FOR_MAX] =
{
	"Up", "Down", "Left", "Right",
	"Up 50%", "Up 100%", "Down 50%", "Down 100%",
	"Left 50%", "Left 100%", "Right 50%", "Right 100%",
	"C Up", "C Down", "C Left", "C Right",
	"L", "R", "Z", "A", "B", "X", "Y", "Start",
};

static const int pad_digital_bindings[] =
{
	VKEY_FOR_UP, VKEY_FOR_DOWN, VKEY_FOR_LEFT, VKEY_FOR_RIGHT,
	VKEY_FOR_A, VKEY_FOR_B, VKEY_FOR_X, VKEY_FOR_Y,
	VKEY_FOR_START, VKEY_FOR_TRIGGERL, VKEY_FOR_TRIGGERR, VKEY_FOR_TRIGGERZ,
};

static const int pad_stick_bindings[] =
{
	VKEY_FOR_XUP50, VKEY_FOR_XUP100, VKEY_FOR_XDOWN50, VKEY_FOR_XDOWN100,
	VKEY_FOR_XLEFT50, VKEY_FOR_XLEFT100, VKEY_FOR_XRIGHT50, VKEY_FOR_XRIGHT100,
};

static const int pad_substick_bindings[] =
{
	VKEY_FOR_CXUP, VKEY_FOR_CXDOWN, VKEY_FOR_CXLEFT, VKEY_FOR_CXRIGHT,
};

/* The default keyboard bindings of the first pad (see PADDefaultConfig in ui.cpp), as SDL scancodes */
static const int pad_default_vkeys[VKEY_FOR_MAX] =
{
	SDL_SCANCODE_HOME,      // Up
	SDL_SCANCODE_END,       // Down
	SDL_SCANCODE_DELETE,    // Left
	SDL_SCANCODE_PAGEDOWN,  // Right
	0,                      // Up 50%
	SDL_SCANCODE_UP,        // Up 100%
	0,                      // Down 50%
	SDL_SCANCODE_DOWN,      // Down 100%
	0,                      // Left 50%
	SDL_SCANCODE_LEFT,      // Left 100%
	0,                      // Right 50%
	SDL_SCANCODE_RIGHT,     // Right 100%
	SDL_SCANCODE_KP_8,      // C Up
	SDL_SCANCODE_KP_2,      // C Down
	SDL_SCANCODE_KP_4,      // C Left
	SDL_SCANCODE_KP_6,      // C Right
	SDL_SCANCODE_Q,         // L
	SDL_SCANCODE_W,         // R
	SDL_SCANCODE_E,         // Z
	SDL_SCANCODE_X,         // A
	SDL_SCANCODE_Z,         // B
	SDL_SCANCODE_S,         // X
	SDL_SCANCODE_A,         // Y
	SDL_SCANCODE_RETURN,    // Start
};

/* The default gamepad bindings (the usual Xbox-style layout): the main stick is mapped to the
   left stick, the C stick to the right stick, and the L/R triggers to the analog triggers. */
static const int pad_default_gckeys[VKEY_FOR_MAX] =
{
	PAD_GCKEY_MAKE_BUTTON(SDL_CONTROLLER_BUTTON_DPAD_UP),           // Up
	PAD_GCKEY_MAKE_BUTTON(SDL_CONTROLLER_BUTTON_DPAD_DOWN),         // Down
	PAD_GCKEY_MAKE_BUTTON(SDL_CONTROLLER_BUTTON_DPAD_LEFT),         // Left
	PAD_GCKEY_MAKE_BUTTON(SDL_CONTROLLER_BUTTON_DPAD_RIGHT),        // Right
	0,                                                              // Up 50%
	PAD_GCKEY_MAKE_AXIS(SDL_CONTROLLER_AXIS_LEFTY, false),          // Up 100%
	0,                                                              // Down 50%
	PAD_GCKEY_MAKE_AXIS(SDL_CONTROLLER_AXIS_LEFTY, true),           // Down 100%
	0,                                                              // Left 50%
	PAD_GCKEY_MAKE_AXIS(SDL_CONTROLLER_AXIS_LEFTX, false),          // Left 100%
	0,                                                              // Right 50%
	PAD_GCKEY_MAKE_AXIS(SDL_CONTROLLER_AXIS_LEFTX, true),           // Right 100%
	PAD_GCKEY_MAKE_AXIS(SDL_CONTROLLER_AXIS_RIGHTY, false),         // C Up
	PAD_GCKEY_MAKE_AXIS(SDL_CONTROLLER_AXIS_RIGHTY, true),          // C Down
	PAD_GCKEY_MAKE_AXIS(SDL_CONTROLLER_AXIS_RIGHTX, false),         // C Left
	PAD_GCKEY_MAKE_AXIS(SDL_CONTROLLER_AXIS_RIGHTX, true),          // C Right
	PAD_GCKEY_MAKE_AXIS(SDL_CONTROLLER_AXIS_TRIGGERLEFT, true),     // L
	PAD_GCKEY_MAKE_AXIS(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, true),    // R
	PAD_GCKEY_MAKE_BUTTON(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER),     // Z
	PAD_GCKEY_MAKE_BUTTON(SDL_CONTROLLER_BUTTON_A),                 // A
	PAD_GCKEY_MAKE_BUTTON(SDL_CONTROLLER_BUTTON_B),                 // B
	PAD_GCKEY_MAKE_BUTTON(SDL_CONTROLLER_BUTTON_X),                 // X
	PAD_GCKEY_MAKE_BUTTON(SDL_CONTROLLER_BUTTON_Y),                 // Y
	PAD_GCKEY_MAKE_BUTTON(SDL_CONTROLLER_BUTTON_START),             // Start
};

/* The gamepad bindings offered by the dialog, for the buttons without a friendly name */
static const char* pad_gamepad_button_name[SDL_CONTROLLER_BUTTON_MAX] =
{
	"A", "B", "X", "Y", "Back", "Guide", "Start", "L Stick", "R Stick",
	"L Shoulder", "R Shoulder", "DPad Up", "DPad Down", "DPad Left", "DPad Right",
	"Misc", "Paddle 1", "Paddle 2", "Paddle 3", "Paddle 4", "Touchpad",
};

static const char* pad_gamepad_axis_name[SDL_CONTROLLER_AXIS_MAX] =
{
	"L Stick X", "L Stick Y", "R Stick X", "R Stick Y", "L Trigger", "R Trigger",
};

/* The axis capture ignores the stick noise */
#define PAD_CAPTURE_AXIS_THRESHOLD  16384

/* Dialog state */

static bool     pad_dialog_open = false;
static int      pad_dialog_num = 0;                 // the pad being configured
static PADCONF  pad_dialog_config;                  // the edited copy of the pad configuration

/* Key/gamepad capture, armed by a binding button and fed by the SDL event loop (see ui_main) */

static int      pad_capture_target = -1;            // the VKEY_FOR_* that waits for an input
static bool     pad_capture_gamepad = false;        // true: wait for a gamepad event, false: for a key
static bool     pad_capture_active = false;
static bool     pad_capture_done = false;
static int      pad_captured_binding = 0;

/* The keys that the Win32 dialog skips, because they cannot be bound (or the debugger needs them) */
static bool pad_capture_ignored(SDL_Scancode scancode)
{
	switch (scancode)
	{
		case SDL_SCANCODE_LSHIFT:
		case SDL_SCANCODE_RSHIFT:
		case SDL_SCANCODE_LCTRL:
		case SDL_SCANCODE_RCTRL:
		case SDL_SCANCODE_LALT:
		case SDL_SCANCODE_RALT:
		case SDL_SCANCODE_LGUI:
		case SDL_SCANCODE_RGUI:
		case SDL_SCANCODE_MODE:
			return true;
		default:
			return scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F12;
	}
}

/* Load the pad configuration into the dialog (see PADLoadConfig in padsdl.cpp) */
static void pad_dialog_load(int padnum)
{
	char parm[256];

	pad_dialog_num = padnum;

	sprintf(parm, "PluggedIn_%i", padnum);
	pad_dialog_config.plugged = UI::Jdi->GetConfigBool(parm, USER_PADS);

	for (int i = 0; i < VKEY_FOR_MAX; i++)
	{
		sprintf(parm, "VKEY_FOR_%s_%i", pad_binding_suffix[i], padnum);
		pad_dialog_config.vkeys[i] = UI::Jdi->GetConfigInt(parm, USER_PADS);

		sprintf(parm, "GCKEY_FOR_%s_%i", pad_binding_suffix[i], padnum);
		pad_dialog_config.gckeys[i] = UI::Jdi->GetConfigInt(parm, USER_PADS);
	}
}

/* Write the dialog configuration back (see PADSaveConfig in ui.cpp) and make the backend reread it */
static void pad_dialog_save()
{
	char parm[256];

	sprintf(parm, "PluggedIn_%i", pad_dialog_num);
	UI::Jdi->SetConfigBool(parm, pad_dialog_config.plugged, USER_PADS);

	for (int i = 0; i < VKEY_FOR_MAX; i++)
	{
		sprintf(parm, "VKEY_FOR_%s_%i", pad_binding_suffix[i], pad_dialog_num);
		UI::Jdi->SetConfigInt(parm, pad_dialog_config.vkeys[i], USER_PADS);

		sprintf(parm, "GCKEY_FOR_%s_%i", pad_binding_suffix[i], pad_dialog_num);
		UI::Jdi->SetConfigInt(parm, pad_dialog_config.gckeys[i], USER_PADS);
	}

	PADLoadConfig(pad_dialog_num);
}

static void pad_dialog_abort_capture()
{
	pad_capture_target = -1;
	pad_capture_gamepad = false;
	pad_capture_active = false;
	pad_capture_done = false;
}

static void pad_dialog_open_for(int padnum)
{
	pad_dialog_abort_capture();
	pad_dialog_load(padnum);
	pad_dialog_open = true;
}

static void pad_dialog_close()
{
	pad_dialog_abort_capture();
	pad_dialog_open = false;
}

/* Unplug the pad and drop all the bindings (see PADClearConfig in ui.cpp) */
static void pad_dialog_clear()
{
	pad_dialog_config.plugged = false;

	for (int i = 0; i < VKEY_FOR_MAX; i++)
	{
		pad_dialog_config.vkeys[i] = 0;
		pad_dialog_config.gckeys[i] = 0;
	}
}

/* Restore the default bindings (see PADDefaultConfig in ui.cpp). The gamepad is per port, so every
   port gets the standard gamepad mapping. The keyboard defaults are the same for every port, so
   they are only applied to the first pad (otherwise all the pads would react to the same keys). */
static void pad_dialog_default()
{
	for (int i = 0; i < VKEY_FOR_MAX; i++)
	{
		pad_dialog_config.gckeys[i] = pad_default_gckeys[i];

		if (pad_dialog_num == 0)
		{
			pad_dialog_config.vkeys[i] = pad_default_vkeys[i];
		}
	}
}

/* The name of the key bound to the control, or "..." when the binding is not assigned */
static std::string pad_binding_name(int scancode)
{
	if (scancode <= 0 || scancode >= SDL_NUM_SCANCODES)
	{
		return "...";
	}

	const char* name = SDL_GetScancodeName((SDL_Scancode)scancode);

	return (name && *name) ? name : "?";
}

/* The name of the game controller button or axis bound to the control */
static std::string pad_gamepad_binding_name(int binding)
{
	if (PAD_GCKEY_IS_BUTTON(binding))
	{
		int button = PAD_GCKEY_BUTTON(binding);

		if (button < 0 || button >= SDL_CONTROLLER_BUTTON_MAX)
		{
			return "?";
		}

		return pad_gamepad_button_name[button];
	}

	if (PAD_GCKEY_IS_AXIS(binding))
	{
		int axis = PAD_GCKEY_AXIS(binding);

		if (axis < 0 || axis >= SDL_CONTROLLER_AXIS_MAX)
		{
			return "?";
		}

		return std::string(pad_gamepad_axis_name[axis]) + (PAD_GCKEY_AXIS_POS(binding) ? " +" : " -");
	}

	return "...";
}

/* A binding button: the keyboard one or the gamepad one of the control */
static void pad_dialog_binding_button(int vkey, bool gamepad)
{
	// The button label (and with it the button id) changes, so push a stable id
	ImGui::PushID(vkey * 2 + (gamepad ? 1 : 0));

	std::string name;

	if (pad_capture_active && pad_capture_target == vkey && pad_capture_gamepad == gamepad)
	{
		name = "?";
	}
	else
	{
		name = gamepad
			? pad_gamepad_binding_name(pad_dialog_config.gckeys[vkey])
			: pad_binding_name(pad_dialog_config.vkeys[vkey]);
	}

	ImGui::BeginDisabled(!pad_dialog_config.plugged);
	if (ImGui::Button(name.c_str(), ImVec2(110, 0)))
	{
		pad_capture_target = vkey;
		pad_capture_gamepad = gamepad;
		pad_capture_active = true;
		pad_capture_done = false;
	}
	ImGui::EndDisabled();

	ImGui::PopID();
}

/* A group of binding rows (Control | Keyboard | Gamepad). The table auto-fits its content, so that
   the groups can be placed side by side. */
static void pad_dialog_bindings(const char* id, const int* bindings, int count)
{
	if (!ImGui::BeginTable(id, 3, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX))
	{
		return;
	}

	ImGui::TableSetupColumn("Control");
	ImGui::TableSetupColumn("Keyboard");
	ImGui::TableSetupColumn("Gamepad");
	ImGui::TableHeadersRow();

	for (int i = 0; i < count; i++)
	{
		int vkey = bindings[i];

		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		ImGui::TextUnformatted(pad_binding_label[vkey]);

		ImGui::TableSetColumnIndex(1);
		pad_dialog_binding_button(vkey, false);

		ImGui::TableSetColumnIndex(2);
		pad_dialog_binding_button(vkey, true);
	}

	ImGui::EndTable();
}

static void ui_pad_settings()
{
	if (!pad_dialog_open)
	{
		return;
	}

	// Apply the binding captured by the SDL event loop

	if (pad_capture_done)
	{
		if (pad_capture_target >= 0 && pad_capture_target < VKEY_FOR_MAX)
		{
			if (pad_capture_gamepad)
			{
				pad_dialog_config.gckeys[pad_capture_target] = pad_captured_binding;
			}
			else
			{
				pad_dialog_config.vkeys[pad_capture_target] = pad_captured_binding;
			}
		}

		pad_capture_target = -1;
		pad_capture_done = false;
	}

	char title[0x40];
	sprintf(title, "Configure Controller %i", pad_dialog_num + 1);

	// Buttons on the left, the Control Stick with the C Stick under it on the right. The window
	// auto-fits its content, so there is no empty space around the controls.
	bool open = true;

	if (ImGui::Begin(title, &open, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Checkbox("Plugged in", &pad_dialog_config.plugged);

		// The backend maps the connected game controllers to the ports in the order they are reported by SDL
		ImGui::TextDisabled("Connected gamepads drive Port 1, Port 2, ... in order");

		ImGui::Separator();

		ImGui::BeginGroup();
		ImGui::TextUnformatted("Buttons");
		pad_dialog_bindings("pad_buttons", pad_digital_bindings, sizeof(pad_digital_bindings) / sizeof(pad_digital_bindings[0]));
		ImGui::EndGroup();

		ImGui::SameLine(0, 24);

		ImGui::BeginGroup();
		ImGui::TextUnformatted("Control Stick");
		pad_dialog_bindings("pad_stick", pad_stick_bindings, sizeof(pad_stick_bindings) / sizeof(pad_stick_bindings[0]));
		ImGui::Dummy(ImVec2(0, 6));
		ImGui::TextUnformatted("C Stick");
		pad_dialog_bindings("pad_substick", pad_substick_bindings, sizeof(pad_substick_bindings) / sizeof(pad_substick_bindings[0]));
		ImGui::EndGroup();

		ImGui::Separator();

		if (pad_capture_active)
		{
			ImGui::TextUnformatted(pad_capture_gamepad
				? "Press a gamepad button or move an axis (Esc to cancel)"
				: "Press a key (Esc to cancel)");
		}

		if (ImGui::Button("Clear", ImVec2(65, 0)))
		{
			pad_dialog_clear();
		}

		ImGui::SameLine();

		// Gamepads are per port, so every port can get the standard gamepad mapping. The keyboard
		// part of the defaults is only applied to the first pad (see pad_dialog_default).
		if (ImGui::Button("Default", ImVec2(65, 0)))
		{
			pad_dialog_default();
		}

		ImGui::SameLine();

		if (ImGui::Button("OK", ImVec2(65, 0)))
		{
			pad_dialog_save();
			pad_dialog_close();
		}

		ImGui::SameLine();

		if (ImGui::Button("Cancel", ImVec2(65, 0)))
		{
			pad_dialog_close();
		}
	}

	ImGui::End();

	if (!open)
	{
		pad_dialog_close();
	}
}




/*

# Performance Counters

Interesting to track :
- The number of emulated Gekko instructions (million per second, mips)
- Number of recompiled and executed GekkoCore recompiler segments.
- Number of DSP instructions emulated (million per second, mips)
- Number of VI interrupts (frames per second)
- Number of draw operations (PE DrawDone / second)
- Show formatted value of TBR register (OSSystemTime)

*/

namespace UI
{

	// Global instance of the utility, which is controlled in the ui.cpp module
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

		int32_t vis = perf->GetVICounter();
		perf->ResetVICounter();

		int32_t pes = perf->GetPECounter();
		perf->ResetPECounter();

		// An unattended benchmark leaves a per-second timeline in the debug log (`EMU_LOG=<file>`),
		// so that a run can be examined afterwards without watching the status bar.
		if (cmdline.bench)
		{
			Debug::Report(Debug::Channel::Info, "profile: %s, %d VI/s, %d PE/s\n", str, vis, pes);
		}

		// Display information in the status bar

		SetStatusText(STATUS_ENUM::Progress, Util::StringToWstring(str));
		SetStatusText(STATUS_ENUM::VIs, std::to_wstring(vis) + L" VI/s");
		SetStatusText(STATUS_ENUM::PEs, std::to_wstring(pes) + L" PE/s");
		SetStatusText(STATUS_ENUM::SystemTime, Util::StringToWstring(perf->GetSystemTime()));

		Thread::Sleep(perf->metricsInterval);
	}

	PerfMetrics::PerfMetrics()
	{
		perfThread = EMUCreateThread(PerfThreadProc, false, this, "PerfThread");
	}

	PerfMetrics::~PerfMetrics()
	{
		EMUJoinThread(perfThread);
	}

	int64_t PerfMetrics::GetGekkoInstructionsCounter()
	{
		return Jdi->GetPerformanceCounter(0);
	}

	void PerfMetrics::ResetGekkoInstructionsCounter()
	{
		Jdi->ResetPerformanceCounter(0);
	}

	int64_t PerfMetrics::GetGekkoCompiledSegments()
	{
		return Jdi->GetPerformanceCounter(4);
	}

	void PerfMetrics::ResetGekkoCompiledSegments()
	{
		Jdi->ResetPerformanceCounter(4);
	}

	int64_t PerfMetrics::GetGekkoExecutedSegments()
	{
		return Jdi->GetPerformanceCounter(5);
	}

	void PerfMetrics::ResetGekkoExecutedSegments()
	{
		Jdi->ResetPerformanceCounter(5);
	}

	int64_t PerfMetrics::GetDspInstructionsCounter()
	{
		return Jdi->GetPerformanceCounter(1);
	}

	void PerfMetrics::ResetDspInstructionsCounter()
	{
		Jdi->ResetPerformanceCounter(1);
	}

	int32_t PerfMetrics::GetVICounter()
	{
		return (int32_t)Jdi->GetPerformanceCounter(2);
	}

	void PerfMetrics::ResetVICounter()
	{
		Jdi->ResetPerformanceCounter(2);
	}

	int32_t PerfMetrics::GetPECounter()
	{
		return (int32_t)Jdi->GetPerformanceCounter(3);
	}

	void PerfMetrics::ResetPECounter()
	{
		Jdi->ResetPerformanceCounter(3);
	}

	std::string PerfMetrics::GetSystemTime()
	{
		return Jdi->GetSystemTime();
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
		wchar_t* extension = wcsrchr((wchar_t*)currentFileName, L'.');

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

		char* ansiPtr = (char*)bnr->comments[0].longTitle;
		wchar_t* wcharPtr = longTitle;

		while (*ansiPtr)
		{
			*wcharPtr++ = (uint8_t)*ansiPtr++;
		}
		*wcharPtr++ = 0;

		// Convert SJIS Title to Unicode

		if (UI::Jdi->DvdRegionById((char*)diskID.data()) == "JPN")
		{
			size_t size, chars;
			uint16_t* widePtr = SjisToUnicode(longTitle, &size, &chars);
			uint16_t* unicodePtr;

			if (widePtr)
			{
				wcharPtr = longTitle;
				unicodePtr = widePtr;

				while (*unicodePtr)
				{
					*wcharPtr++ = *unicodePtr++;
				}
				*wcharPtr++ = 0;

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

	std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_conv;
	SDL_SetWindowTitle(window, utf8_conv.to_bytes(newTitle).c_str());

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
	auto win_name = std::wstring(APPNAME) + L" - " + std::wstring(APPDESC) + L" (" + Util::StringToWstring(UI::Jdi->GetVersion()) + L")";
	std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_conv;
	SDL_SetWindowTitle(window, utf8_conv.to_bytes(win_name).c_str());
	ResetStatusBar();
}

// Start the IPL (Bootrom) - the same thing that the "File -> Run Bootrom" menu item does.
static void ui_load_bootrom()
{
	CreateRenderTarget();
	UI::Jdi->LoadFile("Bootrom");
	OnMainWindowOpened(L"Bootrom");
	if (Debug::debugger == nullptr)
	{
		UI::Jdi->Run();
	}
	else
	{
		Debug::debugger->SetDisasmCursor(0xfff0'0100);
		UI::Jdi->ExecuteCommand("echo \"Bootrom is started in Suspended state for debugging purposes. Press F5 to continue.\"");
	}
}

/* Defined with the other file loaders, below; the menu needs it first. */
static void reopen_last_file();

static void ui_main_menu()
{
	// Menu Bar
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Open", NULL)) {
				file_reaction = FileReaction::OpenFile_LoadFile;
				fileOpenDialog.Open();
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
						file_reaction = FileReaction::ChooseFile_DVDImage;
						fileOpenDialog.Open();
					}
				}
				ImGui::EndMenu();
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Refresh View", NULL, false, usel.active)) {
				usel.needUpdate = true;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Exit", NULL)) {
				ui_active = false;
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Debug"))
		{
			if (ImGui::MenuItem("Debug Console", NULL, debugger_enabled_check)) {	// Open/close system-wide debugger
				if (Debug::debugger == nullptr)
				{   // open
					debugger_enabled_check = true;
					Debug::debugger = new Debug::Debugger();
					UI::Jdi->SetConfigBool(USER_DOLDEBUG, true, USER_UI);
					//SetStatusText(STATUS_ENUM::Progress, L"Debugger opened");
				}
				else
				{   // close
					debugger_enabled_check = false;
					delete Debug::debugger;
					Debug::debugger = nullptr;
					UI::Jdi->SetConfigBool(USER_DOLDEBUG, false, USER_UI);
					//SetStatusText(STATUS_ENUM::Progress, L"Debugger closed");
				}
			}
			ImGui::MenuItem("Mount DolphinSDK as DVD...", NULL);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Options"))
		{
			ImGui::MenuItem("Settings...", NULL);
			ImGui::MenuItem("View", NULL);
			ui_selector_menu();
			ImGui::Separator();
			if (ImGui::BeginMenu("Controllers"))
			{
				for (int i = 0; i < 4; i++)
				{
					char label[0x20];
					sprintf(label, "Port %i", i + 1);

					if (ImGui::MenuItem(label, NULL, pad_dialog_open && pad_dialog_num == i))
					{
						pad_dialog_open_for(i);
					}
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Memcards"))
			{
				ImGui::MenuItem("Slot A", NULL);
				ImGui::MenuItem("Slot B", NULL);
				ImGui::EndMenu();
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

/* Load and run the file (from the selector or from the Open dialog) */
static void load_file(const std::wstring& filename)
{
	if (filename.empty())
	{
		return;
	}

	CreateRenderTarget();
	UI::Jdi->LoadFile(Util::WstringToString(filename));
	if (Debug::debugger)
	{
		Debug::debugger->InvalidateAll();
	}
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

	load_file(usel.files[usel.selected]->name);
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

`--bench <file> [seconds]` loads the image, runs it unattended for the requested number of seconds
(30 by default) and prints the measured throughput together with the CPU-side performance counters.
This is the measurement tool for the performance work on the emulator: the counters say how the
emulated instructions split between translated blocks and the interpreter, how long the average
basic block is, how often a block has to be recompiled and how many memory accesses leave the
emulated cache for the PI/MEM. The whole point is to compare configurations (the recompiler against
the interpreter, one memory path against another) on the same workload.

An interactive run shows the same throughput in the status bar one line per second (see
PerfThreadProc); with `--bench` that line also goes to the debug log, so an unattended run leaves a
per-second timeline behind.

*/

static int64_t bench_counter(Debug::PerfCounter counter)
{
	return Debug::g_PerfCounters->GetCounter(counter);
}

// The cycle counters are raw TSC ticks; measure the rate against the SDL high-resolution counter
// once, so that the benchmark can report seconds rather than ticks.
static double MeasureTscFrequency()
{
	uint64_t qpc0 = SDL_GetPerformanceCounter();
	uint64_t tsc0 = Gekko::ReadCycleCounter();
	SDL_Delay(100);
	uint64_t qpc1 = SDL_GetPerformanceCounter();
	uint64_t tsc1 = Gekko::ReadCycleCounter();

	if (qpc1 == qpc0)
	{
		return 1.0;
	}

	return (double)(tsc1 - tsc0) * (double)SDL_GetPerformanceFrequency() / (double)(qpc1 - qpc0);
}

static void ui_bench()
{
	CreateRenderTarget();

	UI::Jdi->LoadFile(Util::WstringToString(cmdline.benchFile));
	if (Debug::debugger)
	{
		Debug::debugger->InvalidateAll();
	}
	OnMainWindowOpened(cmdline.benchFile.c_str());
	UI::Jdi->Run();

	// The cycle counters cost two `rdtsc` per block (about 20% of the throughput), so they are
	// opt-in: `set BENCH_PROFILE=1` turns the host cycle breakdown on, the plain run measures the
	// real throughput.
	bool profiled = getenv("BENCH_PROFILE") != nullptr;
	Gekko::cycleProfile = profiled;
	bool stats = getenv("BENCH_STATS") != nullptr;
	if (stats)
	{
		Core->EnableOpcodeStats(true);
	}
	Debug::g_PerfCounters->ResetAllCounters();
	Gekko::stats.Reset();
	Core->ResetInstructionCounter();

	uint64_t start = SDL_GetTicks64();
	uint64_t durationMs = (uint64_t)cmdline.benchSeconds * 1000;
	uint64_t lastReport = start;
	int64_t lastOps = 0;
	int64_t lastTb = 0;
	uint64_t lastBlocks = 0;
	uint64_t lastCompiles = 0;
	uint64_t lastInval = 0;

	while (SDL_GetTicks64() - start < durationMs)
	{
		// Drain the events, so that the render window stays responsive and `SDL_QUIT` does not
		// leave the process hanging.
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_QUIT)
			{
				durationMs = SDL_GetTicks64() - start;
			}
		}
		SDL_Delay(20);

		uint64_t now = SDL_GetTicks64();
		if (now - lastReport >= 1000)
		{
			int64_t ops = (int64_t)Core->GetInstructionCounter();
			double sec = (double)(now - lastReport) / 1000.0;

			Debug::Report(Debug::Channel::Info,
				"profile: %.2f MIPS (%.2fx real), %.2fM blocks/s (%.2f instr/block), %.0fK compiles/s, %.0fK invalidations/s\n",
				(double)(ops - lastOps) / sec / 1e6,
				(double)(Core->regs.tb.sval - lastTb) / (double)Core->OneSecond() / sec,
				(double)(Gekko::stats.jitBlocks - lastBlocks) / sec / 1e6,
				(Gekko::stats.jitBlocks - lastBlocks) ? (double)(Gekko::stats.jitInstrs) / (double)Gekko::stats.jitBlocks : 0.0,
				(double)(Gekko::stats.jitCompiles - lastCompiles) / sec / 1e3,
				(double)(Gekko::stats.jitInvalidations - lastInval) / sec / 1e3);

			lastOps = ops;
			lastTb = Core->regs.tb.sval;
			lastBlocks = Gekko::stats.jitBlocks;
			lastCompiles = Gekko::stats.jitCompiles;
			lastInval = Gekko::stats.jitInvalidations;
			lastReport = now;
		}
	}

	Gekko::cycleProfile = false;
	if (stats)
	{
		Core->EnableOpcodeStats(false);
	}

	uint64_t elapsed = SDL_GetTicks64() - start;
	uint64_t ops = (uint64_t)Core->GetInstructionCounter();
	double seconds = (double)elapsed / 1000.0;

	const Gekko::CpuStats& st = Gekko::stats;
	double tscHz = MeasureTscFrequency();

	Debug::Report(Debug::Channel::Norm, "\n");
	Debug::Report(Debug::Channel::Norm, "--- benchmark: %s ---\n", Util::WstringToString(cmdline.benchFile).c_str());
	// The emulated time the run covered, which is the metric to compare two configurations by:
	// a faster build gets further into the game in the same wall time, and a game's instruction
	// density per emulated second depends on where it is.
	double emulated = (double)Core->regs.tb.uval / (double)Core->OneSecond();

	Debug::Report(Debug::Channel::Norm, "elapsed            : %.3f s\n", seconds);
	Debug::Report(Debug::Channel::Norm, "emulated time      : %.3f s (%.2fx real time)\n", emulated, emulated / seconds);
	Debug::Report(Debug::Channel::Norm, "instructions       : %llu\n", (unsigned long long)ops);
	Debug::Report(Debug::Channel::Norm, "throughput         : %.2f MIPS\n", (double)ops / seconds / 1e6);
	Debug::Report(Debug::Channel::Norm, "basic blocks run   : %llu (%.2f instructions per block)\n",
		(unsigned long long)st.jitBlocks, st.jitBlocks ? (double)st.jitInstrs / (double)st.jitBlocks : 0.0);
	Debug::Report(Debug::Channel::Norm, "blocks translated  : %llu (%.0f/s, %.2f%% of the blocks run)\n",
		(unsigned long long)st.jitCompiles, (double)st.jitCompiles / seconds,
		st.jitBlocks ? (double)st.jitCompiles / (double)st.jitBlocks * 100.0 : 0.0);
	Debug::Report(Debug::Channel::Norm, "block invalidations: %llu (%.0f/s)\n",
		(unsigned long long)st.jitInvalidations, (double)st.jitInvalidations / seconds);
	Debug::Report(Debug::Channel::Norm, "  exception entry  : %llu\n", (unsigned long long)st.invException);
	Debug::Report(Debug::Channel::Norm, "  rfi              : %llu\n", (unsigned long long)st.invRfi);
	Debug::Report(Debug::Channel::Norm, "  mtmsr            : %llu\n", (unsigned long long)st.invMtmsr);
	Debug::Report(Debug::Channel::Norm, "  mtspr bat/sdr/hid: %llu\n", (unsigned long long)st.invMtspr);
	Debug::Report(Debug::Channel::Norm, "  icbi             : %llu\n", (unsigned long long)st.invIcbi);
	Debug::Report(Debug::Channel::Norm, "  tlbie/tlbsync    : %llu\n", (unsigned long long)st.invTlb);
	Debug::Report(Debug::Channel::Norm, "  cache flush      : %llu\n", (unsigned long long)st.invFlash);
	Debug::Report(Debug::Channel::Norm, "jit fallbacks      : %llu (%.2f%% of the instructions)\n",
		(unsigned long long)st.jitFallbacks, ops ? (double)st.jitFallbacks / (double)ops * 100.0 : 0.0);
	Debug::Report(Debug::Channel::Norm, "interp instructions: %llu\n", (unsigned long long)st.interpInstrs);
	Debug::Report(Debug::Channel::Norm, "data cache fills   : %llu\n", (unsigned long long)st.dcacheFills);
	Debug::Report(Debug::Channel::Norm, "instr cache fills  : %llu\n", (unsigned long long)st.icacheFills);
	Debug::Report(Debug::Channel::Norm, "pi reads           : %llu (mmio %llu)\n",
		(unsigned long long)st.piReads, (unsigned long long)st.mmioReads);
	Debug::Report(Debug::Channel::Norm, "pi writes          : %llu (mmio %llu)\n",
		(unsigned long long)st.piWrites, (unsigned long long)st.mmioWrites);
	if (stats)
	{
		Debug::Report(Debug::Channel::Norm, "top guest instructions:\n");
		Core->PrintOpcodeStats(25);
	}

	Debug::Report(Debug::Channel::Norm, "dsp instructions   : %llu (%llu wakes)\n",
		(unsigned long long)st.dspInstrs, (unsigned long long)st.dspWakes);
	Debug::Report(Debug::Channel::Norm, "ai dma             : %llu feeds, %llu ints\n",
		(unsigned long long)st.aiFeeds, (unsigned long long)st.aiInts);
	Debug::Report(Debug::Channel::Norm, "vi interrupts      : %lld\n", (long long)bench_counter(Debug::PerfCounter::VIs));
	Debug::Report(Debug::Channel::Norm, "pe finishes        : %lld\n", (long long)bench_counter(Debug::PerfCounter::PEs));

	// Where the host cycles went. `jit total` is the time the CPU thread spent inside the
	// recompiler; the rest of the wall time belongs to the other threads, to the UI and to idle time.
	if (!profiled)
	{
		UI::Jdi->Stop();
		Thread::Sleep(200);
		UI::Jdi->Unload();
		DestroyRenderTarget();
		OnMainWindowClosed();
		return;
	}

	Debug::Report(Debug::Channel::Norm, "host cycles (TSC %.2f GHz):\n", tscHz / 1e9);
	Debug::Report(Debug::Channel::Norm, "  jit total        : %.3f s (%.1f%% of the wall), %llu cycles\n",
		(double)st.jitRunCycles / tscHz, seconds > 0 ? (double)st.jitRunCycles / tscHz / seconds * 100.0 : 0.0,
		(unsigned long long)st.jitRunCycles);
	Debug::Report(Debug::Channel::Norm, "    generated block: %.3f s (%.1f%% of jit), %.1f cycles/block\n",
		(double)st.blockCallCycles / tscHz, st.jitRunCycles ? (double)st.blockCallCycles / st.jitRunCycles * 100.0 : 0.0,
		st.jitBlocks ? (double)st.blockCallCycles / (double)st.jitBlocks : 0.0);
	Debug::Report(Debug::Channel::Norm, "    translating    : %.3f s (%.1f%% of jit), %.1f cycles/block\n",
		(double)st.compileCycles / tscHz, st.jitRunCycles ? (double)st.compileCycles / st.jitRunCycles * 100.0 : 0.0,
		st.jitCompiles ? (double)st.compileCycles / (double)st.jitCompiles : 0.0);
	Debug::Report(Debug::Channel::Norm, "    dispatch       : %.3f s (%.1f%% of jit)\n",
		(double)(st.jitRunCycles > st.blockCallCycles + st.compileCycles ? st.jitRunCycles - st.blockCallCycles - st.compileCycles : 0) / tscHz,
		st.jitRunCycles ? (double)(st.jitRunCycles > st.blockCallCycles + st.compileCycles ? st.jitRunCycles - st.blockCallCycles - st.compileCycles : 0) / st.jitRunCycles * 100.0 : 0.0);
	Debug::Report(Debug::Channel::Norm, "    interp fallback: %.3f s (%.1f%% of jit)\n",
		(double)st.fallbackCycles / tscHz, st.jitRunCycles ? (double)st.fallbackCycles / st.jitRunCycles * 100.0 : 0.0);
	Debug::Report(Debug::Channel::Norm, "    cache fills    : %.3f s (%.1f%% of jit)\n",
		(double)st.castInCycles / tscHz, st.jitRunCycles ? (double)st.castInCycles / st.jitRunCycles * 100.0 : 0.0);
	Debug::Report(Debug::Channel::Norm, "    mem helpers    : %.3f s (%.1f%% of jit), %llu calls, %.1f cycles/call\n",
		(double)st.memHelperCycles / tscHz, st.jitRunCycles ? (double)st.memHelperCycles / st.jitRunCycles * 100.0 : 0.0,
		(unsigned long long)st.memHelperCalls, st.memHelperCalls ? (double)st.memHelperCycles / (double)st.memHelperCalls : 0.0);
	Debug::Report(Debug::Channel::Norm, "  interpreted instr: %.3f s\n", (double)st.interpCycles / tscHz);

	UI::Jdi->Stop();
	Thread::Sleep(200);
	UI::Jdi->Unload();
	DestroyRenderTarget();
	OnMainWindowClosed();
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

	const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	if (ImGui::BeginChild("selector", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar))
	{
		const float iconWidth = (float)(usel.smallIcons ? (DVD_BANNER_WIDTH >> 1) : DVD_BANNER_WIDTH);
		const float iconHeight = (float)(usel.smallIcons ? (DVD_BANNER_HEIGHT >> 1) : DVD_BANNER_HEIGHT);
		const float rowHeight = iconHeight + ImGui::GetStyle().CellPadding.y * 2;

		if (ImGui::BeginTable("selector_grid", 5,
			ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_BordersOuter |
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
				if (file->banner)
				{
					ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)file->banner,
						ImVec2(iconPos.x + 2, iconPos.y),
						ImVec2(iconPos.x + 2 + iconWidth, iconPos.y + iconHeight));
				}

				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(ToUtf8(file->title).c_str());

				ImGui::TableSetColumnIndex(2);
				ImGui::TextUnformatted(SmartSize(file->size).c_str());

				ImGui::TableSetColumnIndex(3);
				ImGui::TextUnformatted(Util::WstringToString(file->id).c_str());

				ImGui::TableSetColumnIndex(4);
				ImGui::TextUnformatted(ToUtf8(file->comment).c_str());

				if (usel.scrollToSelected && i == usel.selected)
				{
					ImGui::SetScrollHereY(0.5f);
					usel.scrollToSelected = false;
				}
			}

			ImGui::EndTable();
		}

		// The table does not handle the keyboard, so the cursor is moved by the same
		// keys as in the Win32 selector (see ScrollSelector in ui.cpp).
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows | ImGuiFocusedFlags_RootWindow))
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
	}
	ImGui::EndChild();
	ImGui::Separator();
}

static void ui_status_bar()
{
	for (int i = 0; i < (int)STATUS_ENUM::StatusMax; i++)
	{
		ImGui::TextWrapped(Util::WstringToString(status_parts[i]).c_str());
		ImGui::SameLine();
	}
}

static void ui_main_window()
{
	static bool use_work_area = true;
	static ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(use_work_area ? viewport->WorkPos : viewport->Pos);
	ImGui::SetNextWindowSize(use_work_area ? viewport->WorkSize : viewport->Size);

	if (ImGui::Begin("main_window", nullptr, flags))
	{
		ui_main_menu();
		ui_selector();
		ui_status_bar();
	}
	ImGui::End();
}

static int ui_main()
{
	EMUCtor();

	// debugger enabled ?
	debugger_enabled_check = UI::Jdi->GetConfigBool(USER_DOLDEBUG, USER_UI);
	if (debugger_enabled_check)
	{
		Debug::debugger = new Debug::Debugger();
	}

	// Create an interface for communicating with the emulator core
	UI::Jdi = new UI::JdiClient;

	// The command line may ask for an empty drive (the IPL then takes its "no disk" path,
	// which is the one that shows the cube animation).

	if (cmdline.noDisc)
	{
		UI::Jdi->DvdOpenCover();
	}

	// Add UI methods
	JdiAddNode("UI_JDI_JSON", UI::UiJdi, UIReflector);
	JdiAddNode("DEBUG_UI_JDI_JSON", Debug::DebugUiJdi, Debug::DebugUIReflector);

	// Start the user interface

	fileOpenDialog.SetTitle("Open File");
	fileOpenDialog.SetTypeFilters({ ".dol", ".elf", ".gcm", ".iso", ".map", ".json", ".bin" });
	
	fileSaveDialog.SetTitle("Save File");
	fileSaveDialog.SetTypeFilters({ ".dol", ".elf", ".gcm", ".iso", ".map", ".json", ".bin" });

	chooseDirectoryDialog.SetTitle("Choose Directory");

	// Selector state (see the "Game selector" section)
	usel.active = UI::Jdi->GetConfigBool(USER_SELECTOR, USER_UI);
	usel.smallIcons = UI::Jdi->GetConfigBool(USER_SMALLICONS, USER_UI);
	usel.sortBy = (SELECTOR_SORT)UI::Jdi->GetConfigInt(USER_SORTVIEW, USER_UI);

	CreateStatusBar();

	// From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
	SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

	// Create window with SDL_Renderer graphics context
	SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	window = SDL_CreateWindow(APPNAME_A, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, window_flags);
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

	// Setup Dear ImGui style
	//ImGui::StyleColorsClassic();
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsLight();

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
	// or to run one specific file instead of waiting for the selector.

	if (cmdline.ipl)
	{
		ui_load_bootrom();
	}
	else if (!cmdline.image.empty())
	{
		CreateRenderTarget();
		UI::Jdi->LoadFile(Util::WstringToString(cmdline.image));
		if (Debug::debugger != nullptr)
		{
			Debug::debugger->InvalidateAll();
		}
		OnMainWindowOpened(cmdline.image.c_str());
		if (Debug::debugger == nullptr)
		{
			UI::Jdi->Run();
		}
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
			if (forMainWindow && !emu_running && !pad_capture_active &&
				event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_F3)
			{
				reopen_last_file();
			}

			// The controller settings dialog captures the next key press or gamepad event as the
			// new binding. The captured events must not reach ImGui, otherwise they would also
			// move the selector cursor, trigger a menu item or navigate the UI.
			if (pad_capture_active && !pad_capture_done)
			{
				if (event.type == SDL_KEYDOWN && event.key.windowID == mainWindowID)
				{
					SDL_Scancode scancode = event.key.keysym.scancode;

					if (scancode == SDL_SCANCODE_ESCAPE)
					{
						pad_captured_binding = 0;       // Esc cancels the capture
						pad_capture_done = true;
						pad_capture_active = false;
					}
					else if (!pad_capture_gamepad && !pad_capture_ignored(scancode))
					{
						pad_captured_binding = (int)scancode;
						pad_capture_done = true;
						pad_capture_active = false;
					}

					forMainWindow = false;
				}
				else if (pad_capture_gamepad && event.type == SDL_CONTROLLERBUTTONDOWN)
				{
					if (event.cbutton.button >= 0 && event.cbutton.button < SDL_CONTROLLER_BUTTON_MAX)
					{
						pad_captured_binding = PAD_GCKEY_MAKE_BUTTON(event.cbutton.button);
						pad_capture_done = true;
						pad_capture_active = false;
					}

					forMainWindow = false;
				}
				else if (pad_capture_gamepad && event.type == SDL_CONTROLLERAXISMOTION)
				{
					if (event.caxis.axis >= 0 && event.caxis.axis < SDL_CONTROLLER_AXIS_MAX &&
						(event.caxis.value >= PAD_CAPTURE_AXIS_THRESHOLD || event.caxis.value <= -PAD_CAPTURE_AXIS_THRESHOLD))
					{
						pad_captured_binding = PAD_GCKEY_MAKE_AXIS(event.caxis.axis, event.caxis.value > 0);
						pad_capture_done = true;
						pad_capture_active = false;
					}

					forMainWindow = false;
				}
				else if (event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERAXISMOTION)
				{
					// No gamepad event may navigate the UI while a binding waits for a key
					forMainWindow = false;
				}
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

		// Keep the SDL game controllers of the pads open, and their cached state fresh. The events
		// pumped above have the device list up to date (see PADUpdateControllers in padsdl.cpp).
		PADUpdateControllers();

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

		ui_pad_settings();

		if (Debug::debugger != nullptr) {
			Debug::debugger->DrawInternal();
		}

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

		fileSaveDialog.Display();
		if (fileSaveDialog.HasSelected())
		{
			fileSaveDialog.ClearSelected();
		}

		chooseDirectoryDialog.Display();
		if (chooseDirectoryDialog.HasSelected())
		{
			auto name = chooseDirectoryDialog.GetSelected().string();
			chooseDirectoryDialog.ClearSelected();

			if (file_reaction == FileReaction::ChooseDirectory_SelectorPath && !name.empty())
			{
				AddSelectorPath(Util::StringToWstring(name));
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
		SDL_SetRenderDrawColor(renderer, (Uint8)(clear_color.x * 255), (Uint8)(clear_color.y * 255), (Uint8)(clear_color.z * 255), (Uint8)(clear_color.w * 255));
		SDL_RenderClear(renderer);
		ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
		SDL_RenderPresent(renderer);

		SDL_Delay(10);
	}

	// Main window closed

	// Cleanup
	usel.clear();       // the banner textures must be destroyed before the renderer

	ImGui_ImplSDLRenderer2_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	UI::Jdi->Unload();

	JdiRemoveNode("UI_JDI_JSON");
	JdiRemoveNode("DEBUG_UI_JDI_JSON");

	if (Debug::debugger) {
		delete Debug::debugger;
	}

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

	return ui_main();
}

#endif
