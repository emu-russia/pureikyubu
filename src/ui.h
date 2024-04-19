/*

## Technical features

At the heart of the interface is the "Selector" - a custom ListView with a list of executable files (DOL/ELF) and disk images (GCM).

The emulator settings dialog is used only for modifying Settings.json.

## Controller Settings Dialog (PAD)

This dialog is used to configure the PadSimpleWin32 backend.

When there is actual support for USB controllers, it will probably be redesigned.

In short, the current controller settings are strongly tied to the PadSimpleWin32 backend, which is not very good, but for now it is as it is.

*/

#pragma once

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



// version info
#define APPNAME L"プレイキューブ"
#define APPNAME_A "pureikyubu"
#define APPDESC L"Nintendo GameCube Emulator"

namespace UI
{
	// basic message output
	void Error(const wchar_t* title, const wchar_t* fmt, ...);
	void Report(const wchar_t* fmt, ...);
}



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
	SELECTOR_FILE   type;       // See above (one of SELECTOR_FILE_*)
	size_t          size;       // File size
	std::wstring    id;         // GameID = DiskID
	std::wstring    name;       // File path and name
	wchar_t			title[MAX_TITLE];       // alternate file name
	wchar_t			comment[MAX_COMMENT];   // some notes
	int             icon[2];    // Banner/icon + same but highlighted
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



/* Status bar parts enumerator */
enum class STATUS_ENUM
{
	Progress = 0,       // Current emu state / Gekko/DSP performance counters
	VIs,                // VI / second
	PEs,                // PE DrawDone / second
	SystemTime,         // OS System Time
	StatusMax,
};

void SetStatusText(STATUS_ENUM sbPart, const std::wstring& text, bool post = false);
std::wstring GetStatusText(STATUS_ENUM sbPart);

void StartProgress(int range, int delta);
void StepProgress();
void StopProgress();


void OnMainWindowOpened(const wchar_t* currentFileName);
void OnMainWindowClosed();
