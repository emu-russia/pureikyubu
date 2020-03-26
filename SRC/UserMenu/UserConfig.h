// registry settings
#define USER_REG_HKEY    HKEY_CURRENT_USER
#define USER_KEY_NAME    _T("Software\\") APPNAME _T(" Emulator\\") APPVER

// specify INI filename
#define USER_INI_FILE   _T(".\\Data\\GAMES.ini")

// user variables API
TCHAR *  GetConfigString(TCHAR*var, TCHAR* defVal, TCHAR* path = USER_KEY_NAME);
void    SetConfigString(TCHAR*var, TCHAR* newVal, TCHAR* path = USER_KEY_NAME);
int     GetConfigInt(TCHAR *var, int defVal, TCHAR *path = USER_KEY_NAME);
void    SetConfigInt(TCHAR *var, int newVal, TCHAR *path = USER_KEY_NAME);
void    KillConfigInt(TCHAR *var, TCHAR *path = USER_KEY_NAME);

// ---------------------------------------------------------------------------
// Dolwin User Variables list (a-z, for every sub-system).
// ---------------------------------------------------------------------------

// Emulator

#define USER_BINORG     _T("BINORG")            // binary file loading offset (physical address)
#define USER_BINORG_DEFAULT     0x3100

#define USER_DOLDEBUG   _T("DOLDEBUG")          // enable debugger
#define USER_DOLDEBUG_DEFAULT   FALSE

#define USER_MAKEMAP            _T("MAKEMAP")   // 1: make map file, if missing (find symbols)
#define USER_MAKEMAP_DEFAULT    TRUE

#define USER_MMU        _T("MMU")               // memory translation mode (0: simple, 1: mmu)
#define USER_MMU_DEFAULT        0

#define USER_PATCH      _T("PATCH")             // patches allowed, if 1
#define USER_PATCH_DEFAULT      1

// UserMenu

#define USER_ANSI       _T("ANSI")              // bootrom ANSI font
#define USER_ANSI_DEFAULT       _T("Data\\AnsiFont.szp")

#define USER_FILTER     _T("FILTER")            // file filter
#define USER_FILTER_DEFAULT     0xffffffff

#define USER_LASTDIR_ALL _T("LASTDIR_ALL")      // last used directory (all files)
#define USER_LASTDIR_ALL_DEFAULT _T(".\\")

#define USER_LASTDIR_DVD _T("LASTDIR_DVD")      // last used directory (dvd)
#define USER_LASTDIR_DVD_DEFAULT _T(".\\")

#define USER_LASTDIR_MAP _T("LASTDIR_MAP")      // last used directory (map)
#define USER_LASTDIR_MAP_DEFAULT _T(".\\Data")

#define USER_LASTDIR_PATCH _T("LASTDIR_PATCH")  // last used directory (patch)
#define USER_LASTDIR_PATCH_DEFAULT _T(".\\Data")

#define USER_LASTFILE   _T("LASTFILE")          // last loaded file
#define USER_LASTFILE_DEFAULT   _T("")

#define USER_ONTOP      _T("ONTOP")             // window is always on top, if 1
#define USER_ONTOP_DEFAULT      FALSE

#define USER_PATH       _T("PATH")              // path string for selector
#define USER_PATH_DEFAULT       _T(".\\")

#define USER_PROFILE    _T("PROFILE")           // 1: enable emu profiler
#define USER_PROFILE_DEFAULT    TRUE

#define USER_RECENT     _T("RECENT%i")          // recent file entry
#define USER_RECENT_DEFAULT     _T("")

#define USER_RECENT_NUM _T("RECENTNUM")         // number of recent files
#define USER_RECENT_NUM_DEFAULT 0

#define USER_RUNONCE    _T("RUNONCE")           // allow multiple instancies, if 0
#define USER_RUNONCE_DEFAULT    TRUE

#define USER_SELECTOR   _T("SELECTOR")          // selector disabled, if 0
#define USER_SELECTOR_DEFAULT   TRUE

#define USER_SJIS       _T("SJIS")              // bootrom SJIS font
#define USER_SJIS_DEFAULT       _T("Data\\SjisFont.szp")

#define USER_SMALLICONS _T("SMALLICONS")        // show small icons, if 1
#define USER_SMALLICONS_DEFAULT FALSE

#define USER_SORTVIEW   _T("SORTVIEW")          // sort files in selector (1..6, see menu)
#define USER_SORTVIEW_DEFAULT   SELECTOR_SORT_DEFAULT

// Hardware

#define USER_CONSOLE    _T("CONSOLE")           // console version (see YAGCD)
#define USER_CONSOLE_DEFAULT    3           // 0x00000003: latest production board

#define USER_EXI_LOG    _T("EXI_LOG")           // 1: log EXI activities
#define USER_EXI_LOG_DEFAULT    1

#define USER_GX_POLL    _T("GX_POLL")           // 1: poll controllers after GX draw done
#define USER_GX_POLL_DEFAULT    0

#define USER_HW_ASSERT  _T("HW_ASSERT")         // assert on not implemented HW in non DEBUG
#define USER_HW_ASSERT_DEFAULT  1

#define USER_OS_REPORT  _T("OS_REPORT")         // 1: allow debugger output (by EXI)
#define USER_OS_REPORT_DEFAULT  1

#define USER_PI_RSWHACK _T("RSWHACK")           // reset button hack
#define USER_PI_RSWHACK_DEFAULT 1

#define USER_RTC        _T("RTC")               // 1: real-time clock enabled
#define USER_RTC_DEFAULT        0

#define USER_VI_COUNT   _T("VI_COUNT")          // lines count per single frame (0:auto)
#define USER_VI_COUNT_DEFAULT   0

#define USER_VI_LOG     _T("VI_LOG")            // do debugger log output
#define USER_VI_LOG_DEFAULT     1

#define USER_VI_STRETCH   _T("VI_STRETCH")      // 1: stretch VI framebuffer to fit whole window
#define USER_VI_STRETCH_DEFAULT 0

#define USER_VI_XFB     _T("VI_XFB")            // enable video frame buffer (GDI)
#define USER_VI_XFB_DEFAULT     1

#define USER_BOOTROM	_T("BOOTROM")           // Bootrom
#define USER_BOOTROM_DEFAULT       _T("")

#define USER_DSP_DROM	_T("DSP_DROM")          // DSP DROM
#define USER_DSP_DROM_DEFAULT       _T("")

#define USER_DSP_IROM	_T("DSP_IROM")          // DSP IROM
#define USER_DSP_IROM_DEFAULT       _T("")

// High Level

#define USER_HLE_MTX    _T("HLEMTX")            // 1: use matrix library HLE
#define USER_HLE_MTX_DEFAULT    1
