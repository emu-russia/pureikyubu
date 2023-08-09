/*

Old interface from Dolwin 0.10.

Used simply to give the user some kind of interface.

No longer evolving, left until a replacement for a more modern UI appears.

## Technical features

At the heart of the interface is the "Selector" - a custom ListView with a list of executable files (DOL/ELF) and disk images (GCM).

The emulator settings dialog is used only for modifying Settings.json.

## Controller Settings Dialog (PAD)

This dialog is used to configure the PadSimpleWin32 backend.

When there is actual support for USB controllers, it will probably be redesigned.

In short, the current controller settings are strongly tied to the PadSimpleWin32 backend, which is not very good, but for now it is as it is.

## The situation with strings

There is now a miniature hell of using strings.

Historically, Dolwin only supported Ansi (`std::string`). During the code refresh process, all strings were translated to TCHAR. Who does not know - this is such a mutant that depends on the `_UNICODE` macro: if the macro is defined, all TCHARs are Unicode strings, otherwise Ansi.

After switching to cross-platform, it is obvious to completely switch to Unicode (`std::wstring`). But this will be done only by preliminary refactoring (separation of the UI from the emulator core).

*/

#pragma once

#ifdef _WINDOWS
void    AboutDialog(HWND hwndParent);
#endif

// The UI needs to implement a small number of methods that are used in the emulator core.

#define UI_JDI_JSON "./Data/Json/UIJdi.json"

void UIReflector();


// UI configuration variables

// UI section variables
#define USER_DOLDEBUG "DOLDEBUG"			// enable debugger
#define USER_FILTER "FILTER"				// file filter
#define USER_LASTDIR_ALL "LASTDIR_ALL"		// last used directory (all files)
#define USER_LASTDIR_DVD "LASTDIR_DVD"		// last used directory (dvd)
#define USER_LASTDIR_MAP "LASTDIR_MAP"		// last used directory (map)
#define USER_LASTFILE "LASTFILE"			// last loaded file
#define USER_ONTOP "ONTOP"				// window is always on top, if 1
#define USER_PATH "PATH"				// path string for selector
#define USER_PROFILE "PROFILE"			// 1: enable emu profiler
#define USER_RECENT "RECENT%i"			// recent file entry
#define USER_RECENT_NUM "RECENTNUM"			// number of recent files
#define USER_RUNONCE "RUNONCE"			// allow multiple instancies, if 0
#define USER_SELECTOR "SELECTOR"			// selector disabled, if 0
#define USER_SMALLICONS "SMALLICONS"			// show small icons, if 1
#define USER_SORTVIEW "SORTVIEW"			// sort files in selector (1..6, see menu)


/* UI file utilities API. */

namespace UI
{
	enum class FileType
	{
		All = 1,
		Dvd,
		Map,
		Json,
		Directory,
	};

	/* Open/save a file dialog. */
	const wchar_t* FileOpenDialog(FileType type);
	const wchar_t* FileSaveDialog(FileType type);

	std::wstring FileShortName(const std::wstring& filename, int lvl = 3);
	std::wstring FileSmartSize(size_t size);
	std::string FileSmartSizeA(size_t size);
};



// This module is used to communicate with the JDI host. The host can be on the network, in a pluggable DLL or statically linked.

// Now there is a fundamental limitation - all strings are Ansi (std::string). Historically, this is due to the fact that all commands were typed in the debug console. In time, we will move to Unicode (std::wstring).

namespace JDI
{
	typedef Json::Value* (*CmdDelegate)(std::vector<std::string>& args);
	typedef void (*JdiReflector)();
}

namespace UI
{
	class JdiClient
	{
	public:

		JdiClient();
		~JdiClient();

		// Generic

		std::string GetVersion();
		void ExecuteCommand(const std::string& cmdline);

		// Methods for controlling an optical drive

		bool DvdMount(const std::string& path);
		bool DvdMountSDK(const std::string& path);
		void DvdUnmount();

		void DvdSeek(int offset);
		void DvdRead(std::vector<uint8_t>& data);

		uint32_t DvdOpenFile(const std::string& filename);

		bool DvdCoverOpened();
		void DvdOpenCover();
		void DvdCloseCover();

		std::string DvdRegionById(char* DiskId);

		bool DvdIsMounted(std::string& path, bool& mountedIso);

		// Configuration access

		std::string GetConfigString(const std::string& var, const std::string& path);
		void SetConfigString(const std::string& var, const std::string& newVal, const std::string& path);
		int GetConfigInt(const std::string& var, const std::string& path);
		void SetConfigInt(const std::string& var, int newVal, const std::string& path);
		bool GetConfigBool(const std::string& var, const std::string& path);
		void SetConfigBool(const std::string& var, bool newVal, const std::string& path);

		// Emulator controls

		void LoadFile(const std::string& filename);
		void Unload();
		void Run();
		void Stop();
		void Reset();

		// Debug interface

		std::string DebugChannelToString(int chan);
		void QueryDebugMessages(std::list<std::pair<int, std::string>>& queue);
		int64_t GetResetGekkoMipsCounter();

		// Performance Counters, SystemTime

		int64_t GetPerformanceCounter(int counter);
		void ResetPerformanceCounter(int counter);
		std::string GetSystemTime();

		// Misc

		bool JitcEnabled();

	};

	extern JdiClient* Jdi;
}



// version info
#define APPNAME L"プレイキューブ"
#define APPDESC L"Nintendo GameCube Emulator"

namespace UI
{
	// basic message output
	void Error(const wchar_t* title, const wchar_t* fmt, ...);
	void Report(const wchar_t* fmt, ...);
}

#ifdef _WINDOWS

/*
 * Calls the memcard settings dialog
 */
void MemcardConfigure(int num, HWND hParent);



void PADConfigure(long padnum, HWND hwndParent);

#endif // _WINDOWS



// The counters are polled once a second after starting the emulation.
// Polling is performed in a separate thread that sleeps after polling so as not to load the CPU. The information is displayed in the status bar.

namespace UI
{

	class PerfMetrics
	{
		size_t metricsInterval = 1000;

		Thread* perfThread;
		static void PerfThreadProc(void* param);

		// The counter values are retrieved and cleared using JDI.

		int64_t GetGekkoInstructionsCounter();
		void ResetGekkoInstructionsCounter();

		int64_t GetGekkoCompiledSegments();
		void ResetGekkoCompiledSegments();

		int64_t GetGekkoExecutedSegments();
		void ResetGekkoExecutedSegments();

		int64_t GetDspInstructionsCounter();
		void ResetDspInstructionsCounter();

		int32_t GetVICounter();
		void ResetVICounter();

		int32_t GetPECounter();
		void ResetPECounter();

		std::string GetSystemTime();

	public:
		PerfMetrics();
		~PerfMetrics();
	};


	extern PerfMetrics* g_perfMetrics;

}



/* File type */
enum class SELECTOR_FILE
{
	Executable = 1,     /* any GC executable (*.dol, *.elf) */
	Dvd                 /* any DVD image (*.gcm, *.iso)     */
};

/* File info limits */
constexpr int MAX_TITLE = 0x100;
constexpr int MAX_COMMENT = 0x100;

/* File entry */
struct UserFile
{
	SELECTOR_FILE   type;       /* See above (one of SELECTOR_FILE_*)   */
	size_t          size;       /* File size                            */
	std::wstring    id;         /* GameID = DiskID + banner checksum    */
	std::wstring    name;       /* File path and name                   */
	wchar_t   title[MAX_TITLE];       // alternate file name
	wchar_t   comment[MAX_COMMENT];   // some notes
	int             icon[2];    /* Banner/icon + same but highlighted   */
};

/* Selector columns */
constexpr auto SELECTOR_COLUMN_BANNER = L"Icon";
constexpr auto SELECTOR_COLUMN_TITLE = L"Title";
constexpr auto SELECTOR_COLUMN_SIZE = L"Size";
constexpr auto SELECTOR_COLUMN_GAMEID = L"Game ID";
constexpr auto SELECTOR_COLUMN_COMMENT = L"Comment";

/* Sort by ... */
enum class SELECTOR_SORT
{
	Unsorted = 0,
	Default = 1,      /* First by icon, then by title */
	Filename,
	Title,
	Size,
	ID,
	Comment,
};

#ifdef _WINDOWS

/* Selector API */
void CreateSelector();
void CloseSelector();
void SetSelectorIconSize(bool smallIcon);
bool AddSelectorPath(const std::wstring& fullPath);            // FALSE, if path duplicated
void ResizeSelector(int width, int height);
void UpdateSelector();
int  SelectorGetSelected();
void SelectorSetSelected(size_t item);
void SelectorSetSelected(const std::wstring& filename);
void SortSelector(SELECTOR_SORT sortBy);
void DrawSelectorItem(LPDRAWITEMSTRUCT item);
void NotifySelector(LPNMHDR pnmh);
void ScrollSelector(int letter);
uint16_t* SjisToUnicode(wchar_t* sjisText, size_t* size, size_t* chars);

// all important data is placed here
class UserSelector
{
public:
	bool        active;             // 1, if enabled (under control of UserWindow)
	bool        opened;             // 1, if visible
	bool        smallIcons;         // show small icons
	SELECTOR_SORT   sortBy;         // sort rule (one of SELECTOR_SORT_*)
	int         width;              // selector width
	int         height;             // selector height

	HWND        hSelectorWindow;    // selector window handler
	HMENU       hFileMenu;          // popup file menu

	// path list, where to search files.
	std::vector<std::wstring> paths;

	// file filter
	uint32_t    filter;             // every 8-bits masking extension : [DOL][ELF][GCM][GMP]

	// list of found files
	std::vector<std::unique_ptr<UserFile>> files;

	std::atomic<bool> updateInProgress;

};

extern  UserSelector usel;

#endif // _WINDOWS


// TODO: Make settings as a generalized unified version of PropertyGrid, so as not to suffer from scattered controls and tabs.

#ifdef _WINDOWS

void    ResetAllSettings();
void    OpenSettingsDialog(HWND hParent, HINSTANCE hInst);
void    EditFileFilter(HWND hwnd);

#endif // _WINDOWS


#ifdef _WINDOWS

/* WS_CLIPCHILDREN and WS_CLIPSIBLINGS are need for OpenGL, but GX plugin   */
/* should take care about proper window style itself !!                     */
constexpr int WIN_STYLE = WS_OVERLAPPED | WS_SYSMENU | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_SIZEBOX;

/* Status bar parts enumerator */
enum class STATUS_ENUM
{
	Progress = 1,       // Current emu state / Gekko/DSP performance counters
	VIs,                // VI / second
	PEs,                // PE DrawDone / second
	SystemTime,         // OS System Time
};

void SetStatusText(STATUS_ENUM sbPart, const std::wstring& text, bool post = false);
std::wstring GetStatusText(STATUS_ENUM sbPart);

void StartProgress(int range, int delta);
void StepProgress();
void StopProgress();

/* Recent files menu */
void UpdateRecentMenu(HWND hwnd);
void AddRecentFile(const std::wstring& path);
void LoadRecentFile(int index);

/* Window controls API */
void OnMainWindowOpened(const wchar_t* currentFileName);
void OnMainWindowClosed();
HWND CreateMainWindow(HINSTANCE hInst);
void ResizeMainWindow(int width, int height);

/* Utilities */
void SetAlwaysOnTop(HWND hwnd, BOOL state);
void SetMenuItemText(HMENU hmenu, UINT id, const std::wstring& text);
void CenterChildWindow(HWND hParent, HWND hChild);

/* All important data is placed here */
struct UserWindow
{
	bool    ontop;                  // main window is on top ?
	HWND    hMainWindow;            // main window
	HWND    hStatusWindow;          // statusbar window
	HWND    hProgress;              // progress bar
	HMENU   hMainMenu;              // main menu
	std::wstring  cwd;              // current working directory
};

extern UserWindow wnd;

extern Debug::DspDebug* dspDebug;
extern Debug::GekkoDebug* gekkoDebug;

#endif // _WINDOWS
