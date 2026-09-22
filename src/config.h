// Json-based config

#pragma once

// The settings of the emulator. There is one build now (the SDL one, issue #421), so there is one
// pair of files: DefaultSettings.json is the shipped set of values (it must exist) and
// Settings.json is the file the user changes. The current settings are the defaults overridden by
// that file, see config.cpp.

constexpr auto EMU_DEFAULT_SETTINGS = L"./Data/DefaultSettings.json";	// Must exist
constexpr auto EMU_SETTINGS = L"./Data/Settings.json";

// Sections
#define USER_UI "ui"
#define USER_PADS "controllers"
#define USER_LOADER		"loader"
#define USER_CORE		"core"
#define USER_HW			"hardware"
#define USER_HLE		"hle"
#define USER_MEMCARDS	"memcards"
#define USER_PERIPH		"peripherals"   // the device pool (see peripherals.h)

// Loader section variables
#define USER_MAKEMAP "MAKEMAP"			// 1: make map file, if missing (find symbols)

// Hardware section variables
#define USER_ANSI		"ANSI"			// bootrom ANSI font
#define USER_SJIS		"SJIS"          // bootrom SJIS font
#define USER_CONSOLE	"CONSOLE"       // console version (see YAGCD)
#define USER_OS_REPORT	"OS_REPORT"     // 1: allow debugger output (by EXI)
#define USER_VI_COUNT	"VI_COUNT"      // lines count per single frame (0:auto)
#define USER_VI_XFB		"VI_XFB"        // enable video frame buffer (the XFB picture in the window)
#define USER_BOOTROM	"BOOTROM"		// Bootrom
#define USER_DSP_DROM	"DSP_DROM"      // DSP DROM
#define USER_DSP_IROM	"DSP_IROM"		// DSP IROM
// 0: the shader (OpenGL) GFX pipeline, 1: the software GFX pipeline (EXPERIMENTAL, issue #384: it
// renders correctly but is young, and a few demos still have picture defects)
#define USER_GFX_PIPELINE "GFX_PIPELINE"

// TODO: Add more
#define USER_PI_LOG "PI_LOG"			// PI interrupts & fifo
#define USER_EXI_LOG "EXI_LOG"			// 1: log EXI activities
#define USER_VI_LOG "VI_LOG"			// do debugger log output
#define USER_DI_LOG "DI_LOG"
#define USER_SI_LOG "SI_LOG"
#define USER_AI_LOG "AI_LOG"
#define USER_MI_LOG "MI_LOG"
#define USER_CP_LOG "CP_LOG"

// MC: Names of the keys used to store to configuration
#define MemcardA_Connected_Key "MemcardA_Connected"
#define MemcardB_Connected_Key "MemcardB_Connected"
#define MemcardA_Filename_Key "MemcardA_Filename"
#define MemcardB_Filename_Key "MemcardB_Filename"
#define Memcard_SyncSave_Key "Memcard_SyncSave"

// User variables API
wchar_t* GetConfigString(const char* var, const char* path);
void SetConfigString(const char* var, const wchar_t* newVal, const char* path);
int GetConfigInt(const char* var, const char* path);
void SetConfigInt(const char* var, int newVal, const char* path);
bool GetConfigBool(const char* var, const char* path);
void SetConfigBool(const char* var, bool newVal, const char* path);

// Whether the variable is there at all. The getters above cannot say: they answer a variable that
// is not there with its default and create it with that value, which is what makes them usable
// everywhere in the emulator, but a caller that has to tell a configuration written before a
// setting existed from one that holds its default needs this (see peripherals.cpp).
bool ConfigValueExists(const char* var, const char* path);
