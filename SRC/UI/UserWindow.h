// WS_CLIPCHILDREN and WS_CLIPSIBLINGS are need for OpenGL, but GX plugin
// should take care about proper window style itself !!
#define WIN_STYLE   ( WS_OVERLAPPED | WS_SYSMENU | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_SIZEBOX)
#define WIN_NAME    APPNAME _T(" - ") APPDESC _T(" (") APPVER _T(")")

// status bar parts enumerator
enum class STATUS_ENUM
{
    Progress = 1,        // current emu state
    Fps,                 // fps counter
    Timing,              // * Obsolete *
    Time,                // time counter
};

void SetStatusText(STATUS_ENUM sbPart, const TCHAR *text, bool post=false);
TCHAR* GetStatusText(STATUS_ENUM sbPart);

void    StartProgress(int range, int delta);
void    StepProgress();
void    StopProgress();

// recent files menu
void    UpdateRecentMenu(HWND hwnd);
void    AddRecentFile(TCHAR *path);
void    LoadRecentFile(int index);

// window controls API
void    OnMainWindowOpened();
void    OnMainWindowClosed();
HWND    CreateMainWindow();
void    ResizeMainWindow(int width, int height);

// utilities
void    SetAlwaysOnTop(HWND hwnd, BOOL state);
void    SetMenuItemText(HMENU hmenu, UINT id, const TCHAR *text);
void    CenterChildWindow(HWND hParent, HWND hChild);

// all important data is placed here
typedef struct UserWindow
{
    bool    ontop;              // main window is on top ?

    HWND    hMainWindow;        // main window
    HWND    hStatusWindow;      // statusbar window
    HWND    hProgress;          // progress bar
    HMENU   hMainMenu;          // main menu
} UserWindow;

extern  UserWindow wnd;
