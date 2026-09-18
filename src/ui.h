/*

# The interface of a front end

The emulator core asks a front end for a small number of things: it hands it the current state of
the emulation (the status bar), it tells it when a file is loaded or unloaded, and it asks it - over
the Json Debug Interface - where the picture is presented. Everything else that used to be part of a
front end (the game selector, the settings dialogs, the file dialogs) is the business of the front
end itself.

There are two front ends:

  * the windowed one, uisdl.cpp. SDL2 provides the window, the events, the audio device and the
    controllers, ImGui draws the dialogs and the game selector. It is the only windowed front end
    there is: the Win32 one (ui.cpp) and its DirectSound/GDI/Win32 back ends were removed in issue
    #421;

  * the headless one, uinull.cpp. No window, no input, no audio and no video output; the builds of
    the CMake `HEADLESS` option and of pureikyubu_headless.vcxproj.

*/

#pragma once

// The UI needs to implement a small number of methods that are used in the emulator core.

void UIReflector();


// UI configuration variables

// UI section variables
#define USER_FILTER "FILTER"				// file filter
#define USER_LASTFILE "LASTFILE"			// last loaded file
#define USER_HW_OSD "HW_OSD"			// 1: draw the HW interface profiler overlay over the emulated picture (issue #394)
#define USER_PATH "PATH"				// path string for selector
#define USER_SELECTOR "SELECTOR"			// selector disabled, if 0
#define USER_SMALLICONS "SMALLICONS"			// show small icons, if 1
#define USER_SORTVIEW "SORTVIEW"			// sort files in selector (1..6, see menu)


// version info
#define APPNAME L"プレイキューブ"
#define APPNAME_A "pureikyubu"
#define APPDESC L"Nintendo GameCube Emulator"



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
	Dvd                 /* any DVD image (*.gcm, *.iso, *.rvz) */
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


void OnMainWindowOpened(const wchar_t* currentFileName);
void OnMainWindowClosed();
