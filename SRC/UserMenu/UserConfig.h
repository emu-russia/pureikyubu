// registry settings
#define USER_REG_HKEY    HKEY_CURRENT_USER
#define USER_KEY_NAME    "Software\\" APPNAME " Emulator\\" APPVER

// specify INI filename
#define USER_INI_FILE   ".\\Data\\GAMES.ini"

// user variables API
char *  GetConfigString(char *var, char *defVal, char *path = USER_KEY_NAME);
void    SetConfigString(char *var, char *newVal, char *path = USER_KEY_NAME);
int     GetConfigInt(char *var, int defVal, char *path = USER_KEY_NAME);
void    SetConfigInt(char *var, int newVal, char *path = USER_KEY_NAME);
void    KillConfigInt(char *var, char *path = USER_KEY_NAME);

// game ini API
BOOL    GetGameInfo(char *gameId, char title[64], char comment[128]);
void    SetGameInfo(char *gameId, char title[64], char comment[128]);
char *  GetIniVar(char *gameId, char *var);

// ---------------------------------------------------------------------------
// Dolwin User Variables list (a-z, for every sub-system).
// ---------------------------------------------------------------------------

// Emulator

#define USER_BINORG     "BINORG"            // binary file loading offset (physical address)
#define USER_BINORG_DEFAULT     0x3100

#define USER_CPU        "CPU"               // CPU core (see Gekko.h)
#define USER_CPU_DEFAULT        CPU_INTERPRETER

#define USER_CPU_CF     "CPU_CF"            // CPU counter factor
#define USER_CPU_CF_DEFAULT     1

#define USER_CPU_DELAY  "CPU_DELAY"         // TBR/DEC update delay (number of instructions)
#define USER_CPU_DELAY_DEFAULT  12

#define USER_CPU_TIME   "CPU_TIME"          // CPU bailout initial time
#define USER_CPU_TIME_DEFAULT   1

#define USER_DOLDEBUG   "DOLDEBUG"          // enable debugger
#define USER_DOLDEBUG_DEFAULT   FALSE

#define USER_MAKEMAP            "MAKEMAP"   // 1: make map file, if missing (find symbols)
#define USER_MAKEMAP_DEFAULT    TRUE

#define USER_MMU        "MMU"               // memory translation mode (0: simple, 1: mmu)
#define USER_MMU_DEFAULT        0

#define USER_MMX        "MMX"               // 1: use MMX/SSE when possible
#define USER_MMX_DEFAULT        1

#define USER_PATCH      "PATCH"             // patches allowed, if 1
#define USER_PATCH_DEFAULT      1

// UserMenu

#define USER_ANSI       "ANSI"              // bootrom ANSI font
#define USER_ANSI_DEFAULT       "Data\\Arial 16.szp"

#define USER_FILTER     "FILTER"            // file filter
#define USER_FILTER_DEFAULT     0xffffffff

#define USER_LASTDIR_ALL "LASTDIR_ALL"      // last used directory (all files)
#define USER_LASTDIR_ALL_DEFAULT ".\\"

#define USER_LASTDIR_DVD "LASTDIR_DVD"      // last used directory (dvd)
#define USER_LASTDIR_DVD_DEFAULT ".\\"

#define USER_LASTDIR_MAP "LASTDIR_MAP"      // last used directory (map)
#define USER_LASTDIR_MAP_DEFAULT ".\\Data"

#define USER_LASTDIR_PATCH "LASTDIR_PATCH"  // last used directory (patch)
#define USER_LASTDIR_PATCH_DEFAULT ".\\Data"

#define USER_LASTFILE   "LASTFILE"          // last loaded file
#define USER_LASTFILE_DEFAULT   ""

#define USER_ONTOP      "ONTOP"             // window is always on top, if 1
#define USER_ONTOP_DEFAULT      FALSE

#define USER_PATH       "PATH"              // path string for selector
#define USER_PATH_DEFAULT       ".\\;c:\\"

#define USER_PROFILE    "PROFILE"           // 1: enable emu profiler
#define USER_PROFILE_DEFAULT    TRUE

#define USER_RECENT     "RECENT%i"          // recent file entry
#define USER_RECENT_DEFAULT     ""

#define USER_RECENT_NUM "RECENTNUM"         // number of recent files
#define USER_RECENT_NUM_DEFAULT 0

#define USER_RUNONCE    "RUNONCE"           // allow multiple instancies, if 0
#define USER_RUNONCE_DEFAULT    TRUE

#define USER_SELECTOR   "SELECTOR"          // selector disabled, if 0
#define USER_SELECTOR_DEFAULT   TRUE

#define USER_SJIS       "SJIS"              // bootrom SJIS font
#define USER_SJIS_DEFAULT       "Data\\Lucida 16.szp"

#define USER_SMALLICONS "SMALLICONS"        // show small icons, if 1
#define USER_SMALLICONS_DEFAULT FALSE

#define USER_SORTVIEW   "SORTVIEW"          // sort files in selector (1..6, see menu)
#define USER_SORTVIEW_DEFAULT   SELECTOR_SORT_DEFAULT

// Hardware

#define USER_CONSOLE    "CONSOLE"           // console version (see YAGCD)
#define USER_CONSOLE_DEFAULT    3           // 0x00000003: latest production board

#define USER_DSP_FAKE   "DSP_FAKE"          // 1: use fake DSP mode (very dirty hack)
#define USER_DSP_FAKE_DEFAULT   1

#define USER_EXI_LOG    "EXI_LOG"           // 1: log EXI activities
#define USER_EXI_LOG_DEFAULT    1

#define USER_GX_POLL    "GX_POLL"           // 1: poll controllers after GX draw done
#define USER_GX_POLL_DEFAULT    0

#define USER_HW_ASSERT  "HW_ASSERT"         // assert on not implemented HW in non DEBUG
#define USER_HW_ASSERT_DEFAULT  1

#define USER_OS_REPORT  "OS_REPORT"         // 1: allow debugger output (by EXI)
#define USER_OS_REPORT_DEFAULT  1

#define USER_PI_RSWHACK "RSWHACK"           // reset button hack
#define USER_PI_RSWHACK_DEFAULT 1

#define USER_RTC        "RTC"               // 1: real-time clock enabled
#define USER_RTC_DEFAULT        0

#define USER_VI_COUNT   "VI_COUNT"          // lines count per single frame (0:auto)
#define USER_VI_COUNT_DEFAULT   0

#define USER_VI_LOG     "VI_LOG"            // do debugger log output
#define USER_VI_LOG_DEFAULT     1

#define USER_VI_STRETCH   "VI_STRETCH"      // 1: stretch VI framebuffer to fit whole window
#define USER_VI_STRETCH_DEFAULT 0

#define USER_VI_XFB     "VI_XFB"            // enable video frame buffer (GDI)
#define USER_VI_XFB_DEFAULT     1

// High Level

#define USER_APPLDR     "APPLDR"            // 0:simulate, 1:boot apploader
#define USER_APPLDR_DEFAULT     1

#define USER_HLE_MTX    "HLEMTX"            // 1: use matrix library HLE
#define USER_HLE_MTX_DEFAULT    1

#define USER_HLE_ONLY   "HLEONLY"           // 1: do not use GCN hardware at all
#define USER_HLE_ONLY_DEFAULT   0

#define USER_HLE_PAD    "HLEPAD"            // 1: use HLE pad driver
#define USER_HLE_PAD_DEFAULT    0

#define USER_ARENA_LO   "ARENALO"           // arenaLo value
#define USER_ARENA_LO_DEFAULT   0x80403100

#define USER_ARENA_HI   "ARENAHI"           // arenaHi value
#define USER_ARENA_HI_DEFAULT   0x81600000

#define USER_KEEP_ARENA "ARENAKEEP"         // 1: override arena settings after apploader (DVD only)
#define USER_KEEP_ARENA_DEFAULT 0
