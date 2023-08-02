#pragma once

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

void SetStatusText(STATUS_ENUM sbPart, const std::wstring & text, bool post=false);
std::wstring GetStatusText(STATUS_ENUM sbPart);

void StartProgress(int range, int delta);
void StepProgress();
void StopProgress();

/* Recent files menu */
void UpdateRecentMenu(HWND hwnd);
void AddRecentFile(const std::wstring & path);
void LoadRecentFile(int index);

/* Window controls API */
void OnMainWindowOpened(const wchar_t* currentFileName);
void OnMainWindowClosed();
HWND CreateMainWindow(HINSTANCE hInst);
void ResizeMainWindow(int width, int height);

/* Utilities */
void SetAlwaysOnTop(HWND hwnd, BOOL state);
void SetMenuItemText(HMENU hmenu, UINT id, const std::wstring & text);
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
