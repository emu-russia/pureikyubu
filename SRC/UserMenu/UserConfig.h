// Json-based config

#pragma once

// user variables API
TCHAR *  GetConfigString(char *var, char* path);
void    SetConfigString(char *var, TCHAR* newVal, char* path);
int     GetConfigInt(char *var, char* path);
void    SetConfigInt(char *var, int newVal, char* path);

// Sections

#define USER_UI			"ui"
#define USER_LOADER		"loader"
#define USER_CORE		"core"
#define USER_HW			"hardware"
#define USER_HLE		"hle"
#define USER_PADS		"controllers"
#define USER_MEMCARDS	"memcards"

// UI

#define USER_DOLDEBUG   "DOLDEBUG"          // enable debugger
#define USER_FILTER     "FILTER"            // file filter
#define USER_LASTDIR_ALL "LASTDIR_ALL"      // last used directory (all files)
#define USER_LASTDIR_DVD "LASTDIR_DVD"      // last used directory (dvd)
#define USER_LASTDIR_MAP "LASTDIR_MAP"      // last used directory (map)
#define USER_LASTDIR_PATCH "LASTDIR_PATCH"  // last used directory (patch)
#define USER_LASTFILE   "LASTFILE"          // last loaded file
#define USER_ONTOP      "ONTOP"             // window is always on top, if 1
#define USER_PATH       "PATH"              // path string for selector
#define USER_PROFILE    "PROFILE"           // 1: enable emu profiler
#define USER_RECENT     "RECENT%i"          // recent file entry
#define USER_RECENT_NUM "RECENTNUM"         // number of recent files
#define USER_RUNONCE    "RUNONCE"           // allow multiple instancies, if 0
#define USER_SELECTOR   "SELECTOR"          // selector disabled, if 0
#define USER_SMALLICONS "SMALLICONS"        // show small icons, if 1
#define USER_SORTVIEW   "SORTVIEW"          // sort files in selector (1..6, see menu)

// Loader

#define USER_BINORG     "BINORG"            // binary file loading offset (physical address)
#define USER_MAKEMAP    "MAKEMAP"			// 1: make map file, if missing (find symbols)
#define USER_PATCH      "PATCH"             // patches allowed, if 1

// Core

#define USER_MMU        "MMU"               // memory translation mode (0: simple, 1: mmu)

// Hardware

#define USER_ANSI       "ANSI"              // bootrom ANSI font
#define USER_SJIS       "SJIS"              // bootrom SJIS font
#define USER_CONSOLE    "CONSOLE"           // console version (see YAGCD)
#define USER_EXI_LOG    "EXI_LOG"           // 1: log EXI activities
#define USER_OS_REPORT  "OS_REPORT"         // 1: allow debugger output (by EXI)
#define USER_PI_RSWHACK "RSWHACK"           // reset button hack
#define USER_VI_COUNT   "VI_COUNT"          // lines count per single frame (0:auto)
#define USER_VI_LOG     "VI_LOG"            // do debugger log output
#define USER_VI_XFB     "VI_XFB"            // enable video frame buffer (GDI)
#define USER_BOOTROM	"BOOTROM"           // Bootrom
#define USER_DSP_DROM	"DSP_DROM"          // DSP DROM
#define USER_DSP_IROM	"DSP_IROM"          // DSP IROM

// High Level

#define USER_HLE_MTX    "HLEMTX"            // 1: use matrix library HLE
