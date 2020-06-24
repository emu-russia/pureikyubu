// Json-based config

#pragma once
#include <string>

constexpr auto DOLWIN_DEFAULT_SETTINGS = L"Data\\DefaultSettings.json";		// Must exist
constexpr auto DOLWIN_SETTINGS = L"Data\\Settings.json";

// User variables API
std::wstring_view GetConfigString(std::string_view var, std::string_view path);
void SetConfigString(std::string_view var, std::wstring_view newVal, std::string_view path);
int GetConfigInt(std::string_view var, std::string_view path);
void SetConfigInt(std::string_view var, int newVal, std::string_view path);
bool GetConfigBool(std::string_view var, std::string_view path);
void SetConfigBool(std::string_view var, bool newVal, std::string_view path);

// Sections
constexpr auto USER_UI			= "ui";
constexpr auto USER_LOADER		= "loader";
constexpr auto USER_CORE		= "core";
constexpr auto USER_HW			= "hardware";
constexpr auto USER_HLE			= "hle";
constexpr auto USER_PADS		= "controllers";
constexpr auto USER_MEMCARDS	= "memcards";

// UI section variables
constexpr auto USER_DOLDEBUG		= "DOLDEBUG";			// enable debugger
constexpr auto USER_FILTER			= "FILTER";				// file filter
constexpr auto USER_LASTDIR_ALL		= "LASTDIR_ALL";		// last used directory (all files)
constexpr auto USER_LASTDIR_DVD		= "LASTDIR_DVD";		// last used directory (dvd)
constexpr auto USER_LASTDIR_MAP		= "LASTDIR_MAP";		// last used directory (map)
constexpr auto USER_LASTDIR_PATCH	= "LASTDIR_PATCH";		// last used directory (patch)
constexpr auto USER_LASTFILE		= "LASTFILE";			// last loaded file
constexpr auto USER_ONTOP			= "ONTOP";				// window is always on top, if 1
constexpr auto USER_PATH			= "PATH";				// path string for selector
constexpr auto USER_PROFILE			= "PROFILE";			// 1: enable emu profiler
constexpr auto USER_RECENT			= "RECENT%i";			// recent file entry
constexpr auto USER_RECENT_NUM		= "RECENTNUM";			// number of recent files
constexpr auto USER_RUNONCE			= "RUNONCE";			// allow multiple instancies, if 0
constexpr auto USER_SELECTOR		= "SELECTOR";			// selector disabled, if 0
constexpr auto USER_SMALLICONS		= "SMALLICONS";			// show small icons, if 1
constexpr auto USER_SORTVIEW		= "SORTVIEW";			// sort files in selector (1..6, see menu)

// Loader section variables
constexpr auto USER_BINORG	= "BINORG";				// binary file loading offset (physical address)
constexpr auto USER_MAKEMAP = "MAKEMAP";			// 1: make map file, if missing (find symbols)
constexpr auto USER_PATCH	= "PATCH";				// patches allowed, if 1

// Hardware section variables
constexpr auto USER_ANSI		= "ANSI";			// bootrom ANSI font
constexpr auto USER_SJIS		= "SJIS";           // bootrom SJIS font
constexpr auto USER_CONSOLE		= "CONSOLE";        // console version (see YAGCD)
constexpr auto USER_EXI_LOG		= "EXI_LOG";        // 1: log EXI activities
constexpr auto USER_OS_REPORT	= "OS_REPORT";      // 1: allow debugger output (by EXI)
constexpr auto USER_PI_RSWHACK	= "RSWHACK";		// reset button hack
constexpr auto USER_VI_COUNT	= "VI_COUNT";       // lines count per single frame (0:auto)
constexpr auto USER_VI_LOG		= "VI_LOG";         // do debugger log output
constexpr auto USER_VI_XFB		= "VI_XFB";         // enable video frame buffer (GDI)
constexpr auto USER_BOOTROM		= "BOOTROM";		// Bootrom
constexpr auto USER_DSP_DROM	= "DSP_DROM";       // DSP DROM
constexpr auto USER_DSP_IROM	= "DSP_IROM";       // DSP IROM

// High Level section variables
constexpr auto USER_HLE_MTX = "HLEMTX";            // 1: use matrix library HLE
