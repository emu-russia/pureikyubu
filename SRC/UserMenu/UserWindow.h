// WS_CLIPCHILDREN and WS_CLIPSIBLINGS are need for OpenGL, but GX plugin
// should take care about proper window style itself !!
#define WIN_STYLE   ( WS_OVERLAPPED | WS_SYSMENU | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_SIZEBOX)
#define WIN_NAMEW   APPNAMEW    L" - "  APPDESCW    L" ("   APPVERW     L")"

// status bar parts enumerator
enum STATUS_ENUM
{
    STATUS_PROGRESS = 1,        // current emu state
    STATUS_FPS,                 // fps counter
    STATUS_TIMING,              // * Obsolete *
    STATUS_TIME,                // time counter
};

void SetStatusText(int sbPart, const wchar_t *text, bool post=false);
wchar_t* GetStatusText(int sbPart);

void    StartProgress(int range, int delta);
void    StepProgress();
void    StopProgress();

// recent files menu
void    UpdateRecentMenu(HWND hwnd);
void    AddRecentFile(char *path);
void    LoadRecentFile(int index);

// window controls API
void    OnMainWindowOpened();
void    OnMainWindowClosed();
HWND    CreateMainWindow();
void    ResizeMainWindow(int width, int height);

// utilities
void    SetAlwaysOnTop(HWND hwnd, BOOL state);
void    SetMenuItemText(HMENU hmenu, UINT id, wchar_t *text);
void    CenterChildWindow(HWND hParent, HWND hChild);

// all important data is placed here
typedef struct UserWindow
{
    BOOL    ontop;              // main window is on top ?

    HWND    hMainWindow;        // main window
    HWND    hStatusWindow;      // statusbar window
    HWND    hProgress;          // progress bar
    HMENU   hMainMenu;          // main menu
    HMENU   hPopupMenu;         // popup menus
} UserWindow;

extern  UserWindow wnd;
