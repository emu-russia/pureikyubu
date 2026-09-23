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
#define USER_LOADER		"loader"
#define USER_CORE		"core"
#define USER_HW			"hardware"
#define USER_HLE		"hle"
#define USER_PERIPH		"peripherals"   // the device pool (see peripherals.h)

// The sections the pads and the memory cards had their settings in before the device pool. Nothing
// reads them any more; they are named here because the settings that are read drop them from the
// document (see config.cpp), so that a configuration this build writes does not carry them along.
#define USER_PADS_OBSOLETE      "controllers"
#define USER_MEMCARDS_OBSOLETE  "memcards"

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

// ---------------------------------------------------------------------------
// Lists of objects
//
// A section can hold a list of objects, one object per entry:
//
//   "peripherals":
//   {
//       "Devices":
//       [
//           { "Type": 65537, "Name": "Controller 1", "VKEY_FOR_A": 27 },
//           { "Type": 131073, "Name": "Memory Card A", "File": "Data/card.mci" }
//       ]
//   }
//
// This is how the pool of peripheral devices is stored (see peripherals.h). What a module keeps is
// the *entry* - a handle of its own object - and not its position in the list, so an entry can be
// added or taken out without disturbing the others: the list is a list, not a table with holes.

//! An entry of such a list. What a module does with it is to pass it back to the accessors below.
struct ConfigEntry;

//! How many entries the list has (0 when there is no such list).
int ConfigListSize(const char* var, const char* path);

//! The entry at t, or nullptr when the list is shorter.
ConfigEntry* ConfigListAt(const char* var, const char* path, int at);

//! Append an entry to the list (the list itself is created when it is not there yet).
ConfigEntry* ConfigListAppend(const char* var, const char* path);

//! Take an entry out of the list, with the settings it holds.
void ConfigListRemove(const char* var, const char* path, ConfigEntry* entry);

//! The settings of an entry. A member that is written to an entry that does not have it is added.
bool ConfigEntryValueExists(const ConfigEntry* entry, const char* member);
int GetConfigEntryInt(const ConfigEntry* entry, const char* member, int def);
void SetConfigEntryInt(ConfigEntry* entry, const char* member, int value);
const wchar_t* GetConfigEntryString(const ConfigEntry* entry, const char* member);
void SetConfigEntryString(ConfigEntry* entry, const char* member, const wchar_t* value);

//! Drop every variable of a section except `keep`. A module whose settings used to be kept in
//! variables of their own calls it once, so that a configuration written by a build before it does
//! not carry them along (see peripherals.cpp).
void ConfigSectionKeepOnly(const char* path, const char* keep);
