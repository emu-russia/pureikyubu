// Json-based config

#pragma once

constexpr auto DOLWIN_DEFAULT_SETTINGS = L"Data\\DefaultSettings.json";		// Must exist
constexpr auto DOLWIN_SETTINGS = L"Data\\Settings.json";

// Sections
#define USER_UI "ui"
#define USER_PADS "controllers"
#define USER_LOADER		"loader"
#define USER_CORE		"core"
#define USER_HW			"hardware"
#define USER_HLE		"hle"
#define USER_MEMCARDS	"memcards"

// Loader section variables
#define USER_BINORG	"BINORG"			// binary file loading offset (physical address)
#define USER_MAKEMAP "MAKEMAP"			// 1: make map file, if missing (find symbols)

// Hardware section variables
#define USER_ANSI		"ANSI"			// bootrom ANSI font
#define USER_SJIS		"SJIS"          // bootrom SJIS font
#define USER_CONSOLE	"CONSOLE"       // console version (see YAGCD)
#define USER_OS_REPORT	"OS_REPORT"     // 1: allow debugger output (by EXI)
#define USER_PI_RSWHACK	"RSWHACK"		// reset button hack
#define USER_VI_COUNT	"VI_COUNT"      // lines count per single frame (0:auto)
#define USER_VI_XFB		"VI_XFB"        // enable video frame buffer (GDI)
#define USER_BOOTROM	"BOOTROM"		// Bootrom
#define USER_DSP_DROM	"DSP_DROM"      // DSP DROM
#define USER_DSP_IROM	"DSP_IROM"		// DSP IROM

// TODO: Add more
#define USER_EXI_LOG "EXI_LOG"			// 1: log EXI activities
#define USER_VI_LOG "VI_LOG"			// do debugger log output

// User variables API
TCHAR* GetConfigString(const char* var, const char* path);
void SetConfigString(const char* var, const TCHAR* newVal, const char* path);
int GetConfigInt(const char* var, const char* path);
void SetConfigInt(const char* var, int newVal, const char* path);
bool GetConfigBool(const char* var, const char* path);
void SetConfigBool(const char* var, bool newVal, const char* path);
