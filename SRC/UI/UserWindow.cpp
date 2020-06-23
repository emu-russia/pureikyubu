/* Main window controls and creation.                                       */
/* main window consist from 3 parts : main menu, selector and statusbar.    */
/* selector is active only when emulator is in Idle state (not running);    */
/* statusbar is used to show current emulator state and performance.        */
/* last note : DO NOT USE WINDOWS API CODE IN OTHER SUB-SYSTEMS!!           */
#include "pch.h"
#include <locale>
#include <codecvt>
#include "../Common/String.h"

/* All important data is placed here */
UserWindow wnd;

Debug::DspDebug* dspDebug;

/* ---------------------------------------------------------------------------  */
/* statusbar                                                                    */

/* Set default values of statusbar parts */
static void ResetStatusBar()
{
    SetStatusText(STATUS_ENUM::Progress, L"Idle");
    SetStatusText(STATUS_ENUM::Fps,      L"");
    SetStatusText(STATUS_ENUM::Timing,   L"");
    SetStatusText(STATUS_ENUM::Time,     L"");
}

/* Create status bar window */
static void CreateStatusBar()
{
    int parts[] = { 0, 360, 420, 480, -1 };

    if (wnd.hMainWindow == NULL) return;

    /* Create window */
    wnd.hStatusWindow = CreateStatusWindow(
        WS_CHILD | WS_VISIBLE,
        NULL,
        wnd.hMainWindow,
        ID_STATUS_BAR
    );
    assert(wnd.hStatusWindow);

    /* Depart statusbar */
    SendMessage( wnd.hStatusWindow, 
                 SB_SETPARTS, 
                 (WPARAM)sizeof(parts) / sizeof(int), 
                 (LPARAM)parts);

    /* Set default values */
    ResetStatusBar();
}

/* Change text in specified statusbar part */
void SetStatusText(STATUS_ENUM sbPart, std::wstring_view text, bool post)
{
    if (wnd.hStatusWindow == NULL)
    {
        return;
    }

    if(post)
    {
        PostMessage(wnd.hStatusWindow, SB_SETTEXT, (WPARAM)(sbPart), (LPARAM)text.data());
    }
    else
    {
        SendMessage(wnd.hStatusWindow, SB_SETTEXT, (WPARAM)(sbPart), (LPARAM)text.data());
    }
}

/* Get text of statusbar part */
std::wstring GetStatusText(STATUS_ENUM sbPart)
{
    static auto sbText = std::wstring(256, 0);

    if (wnd.hStatusWindow == NULL) return NULL;

    SendMessage(wnd.hStatusWindow, SB_GETTEXT, (WPARAM)(sbPart), (LPARAM)sbText.data());
    return sbText;
}

void StartProgress(int range, int delta)
{
    StopProgress();

    RECT rect;
    SendMessage(wnd.hStatusWindow, SB_GETRECT, 1, (LPARAM)&rect);
    int cyVScroll = GetSystemMetrics(SM_CYVSCROLL);

    wnd.hProgress = CreateWindowEx(0, PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE, 
        rect.left, rect.top + abs(rect.bottom - rect.top) / 2 - cyVScroll / 2, rect.right, cyVScroll, 
        wnd.hStatusWindow, NULL, GetModuleHandle(NULL), 0);
    if(wnd.hProgress == NULL) return;
    
    SendMessage(wnd.hProgress, PBM_SETPOS, 0, 0);
    SendMessage(wnd.hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, range));
    SendMessage(wnd.hProgress, PBM_SETSTEP, delta, 0);
}

void StepProgress()
{
    if(wnd.hProgress)
    {
        SendMessage(wnd.hProgress, PBM_STEPIT, 0, 0);
    }
}

void StopProgress()
{
    if(wnd.hProgress)
    {
        DestroyWindow(wnd.hProgress);
        wnd.hProgress = NULL;
    }
}

/* ---------------------------------------------------------------------------  */
/* Recent files list (moved from UserLoader - it was bad place there)           */

#define MAX_RECENT  5   // if you want to increase, you must also add new ID_FILE_RECENT_*

/* Returns -1 if not found */
static int GetMenuItemIndex(HMENU hMenu, std::wstring_view item)
{
    int  index = 0;
    auto buffer = std::wstring(MAX_PATH, 0);
    
    while(index < GetMenuItemCount(hMenu))
    {
        if(GetMenuString(hMenu, index, buffer.data(), buffer.size() - 1, MF_BYPOSITION))
        {
            if (item == buffer)
            {
                return index;
            }
        }

        index++;
    }

    return -1;
}

static void SetRecentEntry(int index, TCHAR *str)
{
    char var[256] = { 0, };
    sprintf_s (var, sizeof(var), USER_RECENT, index);
    SetConfigString(var, str, USER_UI);
}

static TCHAR *GetRecentEntry(int index)
{
    char var[256];
    sprintf_s (var, sizeof(var), USER_RECENT, index);
    return GetConfigString(var, USER_UI).data();
}

void UpdateRecentMenu(HWND hwnd)
{
    HMENU   hMainMenu;
    HMENU   hFileMenu;
    HMENU   hReloadMenu;
    UINT    idx;

    // search for required menu sub-item
    hMainMenu = GetMenu(hwnd);
    if(hMainMenu == NULL) return;

    idx = GetMenuItemIndex(hMainMenu, _T("&File"));             // take care about it
    if(idx < 0) return;
    hFileMenu = GetSubMenu(hMainMenu, idx);
    if(hFileMenu == NULL) return;

    idx = GetMenuItemIndex(hFileMenu, _T("&Reopen\tCtrl+R"));   // take care about it
    if(idx < 0) return;
    hReloadMenu = GetSubMenu(hFileMenu, idx);
    if(hReloadMenu == NULL) return;

    // clear recent list
    while(GetMenuItemCount(hReloadMenu))
    {
        DeleteMenu(hReloadMenu, 0, MF_BYPOSITION);
    }

    // if no recent, add empty
    if(GetConfigInt(USER_RECENT_NUM, USER_UI) == 0)
    {
        AppendMenu(hReloadMenu, MF_GRAYED | MF_STRING, ID_FILE_RECENT_1, _T("None"));
    }
    else
    {
        auto buffer = std::wstring();
        int RecentNum = GetConfigInt(USER_RECENT_NUM, USER_UI);

        for(int i = 0, n = RecentNum; i < RecentNum; i++, n--)
        {
            buffer = fmt::format(L"{:s}", UI::FileShortName(GetRecentEntry(n), 3));
            AppendMenu(hReloadMenu, MF_STRING, ID_FILE_RECENT_1 + i, buffer.data());
        }
    }

    DrawMenuBar(hwnd);
}

void AddRecentFile(std::wstring_view path)
{
    int RecentNum = GetConfigInt(USER_RECENT_NUM, USER_UI);

    // check if item already present in list
    for(int n = 1; n <= RecentNum; n++)
    {
        if(path == GetRecentEntry(n))
        {
            // place old recent to the top
            // and move upper recents down
            auto old = fmt::format(L"{:s}", GetRecentEntry(n));
            for(n = n + 1; n <= RecentNum; n++)
            {
                SetRecentEntry(n-1, GetRecentEntry(n));
            }

            SetRecentEntry(RecentNum, old.data());
            UpdateRecentMenu(wnd.hMainWindow);
            return;
        }
    }

    // increase amount of recent files
    RecentNum++;
    if(RecentNum > MAX_RECENT)
    {
        // move list up
        for(int n = 1; n < MAX_RECENT; n++)
        {
            SetRecentEntry(n, GetRecentEntry(n + 1));
        }
        RecentNum = 5;
    }

    SetConfigInt(USER_RECENT_NUM, RecentNum, USER_UI);

    // add new entry
    SetRecentEntry(RecentNum, (wchar_t*)path.data());
    UpdateRecentMenu(wnd.hMainWindow);
}

// index = 1..max
void LoadRecentFile(int index)
{
    int RecentNum = GetConfigInt(USER_RECENT_NUM, USER_UI);
    TCHAR *path = GetRecentEntry((RecentNum+1) - index);
    LoadFile(path);
}

// ---------------------------------------------------------------------------
// window actions, on emu start/stop (skip "static" functions, see OnMainWindow*)
// tip : this is nice place for your windows-related init code

// select sort method (Options->View->Sort by)
static void SelectSort()
{
    CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_1, MF_BYCOMMAND | MF_UNCHECKED);
    CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_2, MF_BYCOMMAND | MF_UNCHECKED);
    CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_3, MF_BYCOMMAND | MF_UNCHECKED);
    CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_4, MF_BYCOMMAND | MF_UNCHECKED);
    CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_5, MF_BYCOMMAND | MF_UNCHECKED);
    CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_6, MF_BYCOMMAND | MF_UNCHECKED);
    CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_7, MF_BYCOMMAND | MF_UNCHECKED);
    
    switch((SELECTOR_SORT)GetConfigInt(USER_SORTVIEW, USER_UI))
    {
        case SELECTOR_SORT::Default:
            CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_1, MF_BYCOMMAND | MF_CHECKED);
            break;
        case SELECTOR_SORT::Filename:
            CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_2, MF_BYCOMMAND | MF_CHECKED);
            break;
        case SELECTOR_SORT::Title:
            CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_3, MF_BYCOMMAND | MF_CHECKED);
            break;
        case SELECTOR_SORT::Size:
            CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_4, MF_BYCOMMAND | MF_CHECKED);
            break;
        case SELECTOR_SORT::ID:
            CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_5, MF_BYCOMMAND | MF_CHECKED);
            break;
        case SELECTOR_SORT::Comment:
            CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_6, MF_BYCOMMAND | MF_CHECKED);
            break;
        default:
            CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SORTBY_7, MF_BYCOMMAND | MF_CHECKED);
    }
}

// change Swap Controls
void ModifySwapControls(bool stateOpened)
{
    if(stateOpened)       // opened
    {
        SetMenuItemText(wnd.hMainMenu, ID_FILE_COVER, _T("&Close Cover"));
        EnableMenuItem(wnd.hMainMenu, ID_FILE_CHANGEDVD, MF_BYCOMMAND | MF_ENABLED);
    }
    else            // closed
    {
        SetMenuItemText(wnd.hMainMenu, ID_FILE_COVER, _T("&Open Cover"));
        EnableMenuItem(wnd.hMainMenu, ID_FILE_CHANGEDVD, MF_BYCOMMAND | MF_GRAYED);
    }
}

// set menu selector-related controls state
void ModifySelectorControls(bool active)
{
    if(active)
    {
        SetMenuItemText(wnd.hMainMenu, ID_OPTIONS_VIEW_DISABLE, L"&Disable Selector");
        EnableMenuItem(wnd.hMainMenu, ID_FILE_EDITINFO, MF_BYCOMMAND | MF_ENABLED);
        EnableMenuItem(wnd.hMainMenu, ID_FILE_REFRESH, MF_BYCOMMAND | MF_ENABLED);
        EnableMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SMALLICONS, MF_BYCOMMAND | MF_ENABLED);
        EnableMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_LARGEICONS, MF_BYCOMMAND | MF_ENABLED);
        EnableMenuItem( GetSubMenu(GetSubMenu(wnd.hMainMenu, 2), 1),    // Sort By..
                        5, MF_BYPOSITION | MF_ENABLED);
        EnableMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_FILEFILTER, MF_BYCOMMAND | MF_ENABLED);
    }
    else
    {
        SetMenuItemText(wnd.hMainMenu, ID_OPTIONS_VIEW_DISABLE, L"&Enable Selector");
        EnableMenuItem(wnd.hMainMenu, ID_FILE_EDITINFO, MF_BYCOMMAND | MF_GRAYED);
        EnableMenuItem(wnd.hMainMenu, ID_FILE_REFRESH, MF_BYCOMMAND | MF_GRAYED);
        EnableMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SMALLICONS, MF_BYCOMMAND | MF_GRAYED);
        EnableMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_LARGEICONS, MF_BYCOMMAND | MF_GRAYED);
        EnableMenuItem( GetSubMenu(GetSubMenu(wnd.hMainMenu, 2), 1),    // Sort By..
                        5, MF_BYPOSITION | MF_GRAYED);
        EnableMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_FILEFILTER, MF_BYCOMMAND | MF_GRAYED);
    }
}

// called once, during main window creation
static void OnMainWindowCreate(HWND hwnd)
{
    // save handlers
    wnd.hMainWindow = hwnd;
    wnd.hMainMenu = GetMenu(wnd.hMainWindow);

    // emulator
    EMUCtor();

    // run once ?
    if(GetConfigBool(USER_RUNONCE, USER_UI))
        CheckMenuItem(wnd.hMainMenu, ID_RUN_ONCE, MF_BYCOMMAND | MF_CHECKED);
    else
        CheckMenuItem(wnd.hMainMenu, ID_RUN_ONCE, MF_BYCOMMAND | MF_UNCHECKED);

    // allow patches ?
    if(GetConfigBool(USER_PATCH, USER_LOADER))
        CheckMenuItem(wnd.hMainMenu, ID_ALLOW_PATCHES, MF_BYCOMMAND | MF_CHECKED);
    else
        CheckMenuItem(wnd.hMainMenu, ID_ALLOW_PATCHES, MF_BYCOMMAND | MF_UNCHECKED);

    // debugger enabled ?
    emu.doldebug = GetConfigBool(USER_DOLDEBUG, USER_UI);
    CheckMenuItem(wnd.hMainMenu, ID_DEBUG_CONSOLE, MF_BYCOMMAND | MF_UNCHECKED);
    if(emu.doldebug)
    {
        DBOpen();            
        CheckMenuItem(wnd.hMainMenu, ID_DEBUG_CONSOLE, MF_BYCOMMAND | MF_CHECKED);
    }

    // load accelerators
    InitCommonControls();

    // always on top (not in debug)
    wnd.ontop = GetConfigBool(USER_ONTOP, USER_UI);
    if(wnd.ontop) CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_ALWAYSONTOP, MF_CHECKED);
    else CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_ALWAYSONTOP, MF_UNCHECKED);
    SetAlwaysOnTop(wnd.hMainWindow, wnd.ontop);

    // recent menu
    UpdateRecentMenu(wnd.hMainWindow);

    // dvd swap controls
    ModifySwapControls(false);

    // child windows
    CreateStatusBar();
    ResizeMainWindow(640, 480-32);  // 32 pixels overscan

    // selector disabled ?
    usel.active = GetConfigBool(USER_SELECTOR, USER_UI);
    ModifySelectorControls(usel.active);

    // icon size
    bool smallSize = GetConfigBool(USER_SMALLICONS, USER_UI);
    if(smallSize)
    {
        CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_LARGEICONS, MF_BYCOMMAND | MF_UNCHECKED);
        CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SMALLICONS, MF_BYCOMMAND | MF_CHECKED);
    }
    else
    {
        CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SMALLICONS, MF_BYCOMMAND | MF_UNCHECKED);
        CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_LARGEICONS, MF_BYCOMMAND | MF_CHECKED);
    }

    // select sort method
    SelectSort();

    // enable drop operation
    DragAcceptFiles(wnd.hMainWindow, TRUE);

    // simulate close operation, like we just stopped emu
    OnMainWindowClosed();
}

// called once, when Dolwin exits to OS
static void OnMainWindowDestroy()
{
    // disable drop operation
    DragAcceptFiles(wnd.hMainWindow, FALSE);

    if (dspDebug)
    {
        delete dspDebug;
    }

    std::vector<std::string> cmd { "exit" };
    Debug::Hub.Execute(cmd);
}

// emulation has started - do proper actions
void OnMainWindowOpened()
{
    // disable selector
    CloseSelector();
    ModifySelectorControls(false);
    EnableMenuItem( GetSubMenu(wnd.hMainMenu, GetMenuItemIndex(     // View
                    wnd.hMainMenu, _T("&Options")) ), 1, MF_BYPOSITION | MF_GRAYED );

    // set new title for main window
    std::wstring newTitle, gameTitle;
    if (ldat.dvd)
    {
        gameTitle = fmt::format(L"{:s}", ldat.currentFileName);
        newTitle = fmt::format(L"{:s} Running {:s}", APPNAME, gameTitle);

        //_stprintf_s (gameTitle, _countof(gameTitle) - 1, _T("%s"), ldat.currentFileName);
        //_stprintf_s (newTitle, _countof(newTitle) - 1, prefix, gameTitle);
    }
    else
    {
        auto currentFileName = std::wstring(ldat.currentFileName);
        if (currentFileName == L"Bootrom")
        {
            gameTitle = fmt::format(L"{:s}", ldat.currentFileName);
            //_stprintf_s(gameTitle, _countof(gameTitle) - 1, _T("%s"), ldat.currentFileName);
        }
        else
        {
            gameTitle = fmt::format(L"{:s} demo", ldat.currentFileName);
            //_stprintf_s(gameTitle, _countof(gameTitle) - 1, _T("%s demo"), ldat.currentFileName);
        }
        
        newTitle = fmt::format(L"{:s} Running {:s}", APPNAME, gameTitle);
        //_stprintf_s (newTitle, _countof(newTitle) - 1, prefix, gameTitle);
    }
    
    SetWindowText(wnd.hMainWindow, newTitle.c_str());
    
    // user profiler
    OpenProfiler(GetConfigBool(USER_PROFILE, USER_UI));
}

// emulation stop in progress
void OnMainWindowClosed()
{
    // restore current working directory
    SetCurrentDirectory(ldat.cwd.c_str());

    // enable selector
    CreateSelector();
    ModifySelectorControls(usel.active);
    EnableMenuItem(GetSubMenu(wnd.hMainMenu, GetMenuItemIndex(     // View
        wnd.hMainMenu, _T("&Options"))), 1, MF_BYPOSITION | MF_ENABLED);

    // set to Idle
    auto win_name = fmt::format(L"{:s} - {:s} ({:s})", APPNAME, APPDESC, APPVER);
    SetWindowText(wnd.hMainWindow, win_name.c_str());
    ResetStatusBar();
}

// ---------------------------------------------------------------------------
// window controls

// resize client area to fit in given width and height
void ResizeMainWindow(int width, int height)
{
    RECT rc;
    int x, y, w, h;

    GetWindowRect(wnd.hMainWindow, &rc);

    // left-upper corner
    x = rc.left;
    y = rc.top;

    // calculate adjustment
    rc.left = 0;
    rc.top = 0;
    rc.right = width;
    rc.bottom = height;
    AdjustWindowRect(&rc, WIN_STYLE, 1);

    // width and height
    w = rc.right - rc.left;
    h = rc.bottom - rc.top + GetSystemMetrics(SM_CYCAPTION) + 9;

    // adjust by statusbar height
    if(IsWindow(wnd.hStatusWindow))
    {
        GetWindowRect(wnd.hStatusWindow, &rc);
        h += (WORD)(rc.bottom - rc.top);
    }

    // move window
    MoveWindow(wnd.hMainWindow, x, y, w, h, TRUE);
    SendMessage(wnd.hMainWindow, WM_SIZE, 0, 0);
}

/* Main window procedure : "return 0" to leave, "break" to continue DefWindowProc() */
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto name = std::wstring();
    int recent;

    switch(msg)
    {
        /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */ 
        /* Create/destroy messages                                                      */

        case WM_CREATE:
        {
            OnMainWindowCreate(hwnd);
            return 0;
        }
        case WM_CLOSE:
        {
            DestroyWindow(hwnd);
            return 0;
        }
        case WM_DESTROY:
        {
            OnMainWindowDestroy();
            return 0;
        }
        
        /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */
        /* window controls                                                              */

        case WM_COMMAND:
            /* Load recent file */
            if(LOWORD(wParam) >= ID_FILE_RECENT_1 &&
               LOWORD(wParam) <= ID_FILE_RECENT_5)
            {
                LoadRecentFile(LOWORD(wParam) - ID_FILE_RECENT_1 + 1);
                EMUClose();
                EMUOpen();
            }
            else switch(LOWORD(wParam))
            {
                /* Load DVD/executable (START) */
                case ID_FILE_LOAD:
                {
                    if (name = UI::FileOpen(); !name.empty())
                    {
                    loadFile:
                        LoadFile(name.c_str());
                        EMUClose();
                        EMUOpen();
                    }

                    return 0;
                }
                /* Reload last opened file (RESET) */
                case ID_FILE_RELOAD:
                {
                    recent = GetConfigInt(USER_RECENT_NUM, USER_UI);
                    if (recent > 0)
                    {
                        LoadRecentFile(1);
                        EMUClose();
                        EMUOpen();
                    }

                    return 0;
                }
                /* Unload file (STOP) */
                case ID_FILE_UNLOAD:
                {
                    EMUClose();

                    return 0;
                }
                /* Load bootrom */
                case ID_FILE_IPLMENU:
                {
                    LoadFile(L"Bootrom");
                    EMUClose();
                    EMUOpen();

                    return 0;
                }
                /* Open/close DVD lid */
                case ID_FILE_COVER:
                {
                    if (DVD::DDU->GetCoverStatus() == DVD::CoverStatus::Open)   /* Close lid */
                    {
                        DVD::DDU->CloseCover();
                        ModifySwapControls(false);
                    }
                    else /* Open lid */
                    {
                        DVD::DDU->OpenCover();
                        ModifySwapControls(true);
                    }

                    return 0;
                }
                /* Set new current DVD image. */
                case ID_FILE_CHANGEDVD:
                {
                    if (name = UI::FileOpen(UI::FileType::Dvd); !name.empty() && DVD::DDU->GetCoverStatus() == DVD::CoverStatus::Open)
                    {
                        /* Same */
                        if (name == ldat.currentFile) 
                        { 
                            return 0; 
                        }
                        
                        /* Bad */
                        if (!DVD::MountFile(name))
                        {
                            return 0;
                        }

                        /* Close lid */
                        DVD::DDU->CloseCover();
                        ModifySwapControls(false);
                    }

                    return 0;
                }
                /* Exit to OS */
                case ID_FILE_EXIT:
                {
                    DestroyWindow(hwnd);
                    return 0;
                }
                /* Settings dialog */
                case ID_OPTIONS_SETTINGS:
                {
                    OpenSettingsDialog(hwnd, GetModuleHandle(NULL));
                    return 0;
                }
                /* Always on top */
                case ID_OPTIONS_ALWAYSONTOP:
                {
                    wnd.ontop = wnd.ontop ? false : true;
                    SetConfigBool(USER_ONTOP, wnd.ontop, USER_UI);
                    
                    auto flags = (wnd.ontop ? MF_BYCOMMAND | MF_CHECKED : MF_BYCOMMAND | MF_UNCHECKED);
                    CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_ALWAYSONTOP, flags);

                    SetAlwaysOnTop(hwnd, wnd.ontop);
                    return 0;
                }
                /* Refresh view */
                case ID_FILE_REFRESH:
                {
                    UpdateSelector();
                    return 0;
                }
                /* Disable the view */
                case ID_OPTIONS_VIEW_DISABLE:
                {
                    if (usel.active)
                    {
                        CloseSelector();
                        ResetStatusBar();
                        ModifySelectorControls(false);
                        usel.active = false;
                        SetConfigBool(USER_SELECTOR, false, USER_UI);
                    }
                    else
                    {
                        ModifySelectorControls(true);
                        usel.active = true;
                        SetConfigBool(USER_SELECTOR, true, USER_UI);
                        CreateSelector();
                    }

                    return 0;
                }
                /* Change icon size */
                case ID_OPTIONS_VIEW_SMALLICONS:
                {
                    CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_LARGEICONS, MF_BYCOMMAND | MF_UNCHECKED);
                    CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SMALLICONS, MF_BYCOMMAND | MF_CHECKED);
                    SetSelectorIconSize(TRUE);
                    return 0;
                }
                case ID_OPTIONS_VIEW_LARGEICONS:
                {
                    CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_SMALLICONS, MF_BYCOMMAND | MF_UNCHECKED);
                    CheckMenuItem(wnd.hMainMenu, ID_OPTIONS_VIEW_LARGEICONS, MF_BYCOMMAND | MF_CHECKED);
                    SetSelectorIconSize(FALSE);
                    return 0;
                }
                /* Sort files */
                case ID_OPTIONS_VIEW_SORTBY_1:
                {
                    SortSelector(SELECTOR_SORT::Default);
                    SelectSort();

                    return 0;
                }
                case ID_OPTIONS_VIEW_SORTBY_2:
                {
                    SortSelector(SELECTOR_SORT::Filename);
                    SelectSort();
                    
                    return 0;
                }
                case ID_OPTIONS_VIEW_SORTBY_3:
                {
                    SortSelector(SELECTOR_SORT::Title);
                    SelectSort();
                    
                    return 0;
                }
                case ID_OPTIONS_VIEW_SORTBY_4:
                {
                    SortSelector(SELECTOR_SORT::Size);
                    SelectSort();
                    
                    return 0;
                }
                case ID_OPTIONS_VIEW_SORTBY_5:
                {
                    SortSelector(SELECTOR_SORT::ID);
                    SelectSort();
                    
                    return 0;
                }
                case ID_OPTIONS_VIEW_SORTBY_6:
                {
                    SortSelector(SELECTOR_SORT::Comment);
                    SelectSort();
                    
                    return 0;
                }
                case ID_OPTIONS_VIEW_SORTBY_7:
                {
                    SortSelector(SELECTOR_SORT::Unsorted);
                    SelectSort();
                    
                    return 0;
                }

                /* Edit file filter */
                case ID_OPTIONS_VIEW_FILEFILTER:
                {
                    EditFileFilter(hwnd);
                    return 0;
                }

        /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */
        /* debug controls                                                               */

                // multiple instancies on/off
                case ID_RUN_ONCE:
                {
                    if (GetConfigBool(USER_RUNONCE, USER_UI))
                    {   /* Off */
                        CheckMenuItem(wnd.hMainMenu, ID_RUN_ONCE, MF_BYCOMMAND | MF_UNCHECKED);
                        SetConfigBool(USER_RUNONCE, false, USER_UI);
                    }
                    else
                    {   /* On */
                        CheckMenuItem(wnd.hMainMenu, ID_RUN_ONCE, MF_BYCOMMAND | MF_CHECKED);
                        SetConfigBool(USER_RUNONCE, true, USER_UI);
                    }
                    
                    return 0;
                }
                /* Enable patches */
                case ID_ALLOW_PATCHES:
                {
                    if (GetConfigBool(USER_PATCH, USER_LOADER))
                    {   /* Off */
                        CheckMenuItem(wnd.hMainMenu, ID_ALLOW_PATCHES, MF_BYCOMMAND | MF_UNCHECKED);
                        ldat.enablePatch = false;
                    }
                    else
                    {   /* On */
                        CheckMenuItem(wnd.hMainMenu, ID_ALLOW_PATCHES, MF_BYCOMMAND | MF_CHECKED);
                        ldat.enablePatch = true;
                    }
                    SetConfigBool(USER_PATCH, ldat.enablePatch, USER_LOADER);
                    return 0;
                }
                /* Load patch data */
                case ID_LOAD_PATCH:
                {
                    if (name = UI::FileOpen(UI::FileType::Patch); !name.empty())
                    {
                        UnloadPatch();
                        LoadPatch(name.data(), false);
                    }

                    return 0;
                }
                /* Add new patch data */
                case ID_ADD_PATCH:
                {
                    if (name = UI::FileOpen(UI::FileType::Patch); !name.empty())
                    {
                        LoadPatch(name.data(), true);
                    }
                    
                    return 0;
                }
                // open/close debugger
                case ID_DEBUG_CONSOLE:

                    if (dspDebug)
                    {
                        CheckMenuItem(wnd.hMainMenu, ID_DSP_DEBUG, MF_BYCOMMAND | MF_UNCHECKED);
                        delete dspDebug;
                        dspDebug = nullptr;
                    }

                    Sleep(100);

                    if(!emu.doldebug)
                    {   // open
                        CheckMenuItem(wnd.hMainMenu, ID_DEBUG_CONSOLE, MF_BYCOMMAND | MF_CHECKED);
                        DBOpen();
                        emu.doldebug = true;
                        SetStatusText(STATUS_ENUM::Progress, _T("Debugger opened"));
                    }
                    else
                    {   // close
                        CheckMenuItem(wnd.hMainMenu, ID_DEBUG_CONSOLE, MF_BYCOMMAND | MF_UNCHECKED);
                        DBClose();
                        emu.doldebug = false;
                        SetStatusText(STATUS_ENUM::Progress, _T("Debugger closed"));
                    }
                    SetConfigBool(USER_DOLDEBUG, emu.doldebug, USER_UI);
                    return 0;

                // Open/close DSP Debug
                case ID_DSP_DEBUG:
                    if (emu.doldebug)
                    {
                        CheckMenuItem(wnd.hMainMenu, ID_DEBUG_CONSOLE, MF_BYCOMMAND | MF_UNCHECKED);
                        DBClose();
                        emu.doldebug = false;
                    }

                    Sleep(100);

                    if (dspDebug == nullptr)
                    {
                        CheckMenuItem(wnd.hMainMenu, ID_DSP_DEBUG, MF_BYCOMMAND | MF_CHECKED);
                        dspDebug = new Debug::DspDebug("DSP Debug", 80, 60);
                        SetStatusText(STATUS_ENUM::Progress, _T("DSP Debugger opened"));
                    }
                    else
                    {
                        CheckMenuItem(wnd.hMainMenu, ID_DSP_DEBUG, MF_BYCOMMAND | MF_UNCHECKED);
                        delete dspDebug;
                        dspDebug = nullptr;
                        SetStatusText(STATUS_ENUM::Progress, _T("DSP Debugger closed"));
                    }
                    return 0;

                /* Mount Dolphin SDK as DVD */
                case ID_DEVELOPMENT_MOUNTSDK:
                {
                    auto dolphinSdkDir = UI::FileOpen(UI::FileType::Directory);
                    if (!dolphinSdkDir.empty())
                    {
                        std::vector<std::string> cmd1 = { "MountSDK", Util::convert<char>(dolphinSdkDir) };
                        Json::Value* output = Debug::Hub.Execute(cmd1);
                        
                        if (output != nullptr)
                        {
                            delete output;
                        }
                    }
                }

                return 0;

        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
        // options dialogs

                case ID_OPTIONS_CONTROLLERS_PORT1:
                    PADConfigure(0, wnd.hMainWindow);
                    return 0;
                case ID_OPTIONS_CONTROLLERS_PORT2:
                    PADConfigure(1, wnd.hMainWindow);
                    return 0;
                case ID_OPTIONS_CONTROLLERS_PORT3:
                    PADConfigure(2, wnd.hMainWindow);
                    return 0;
                case ID_OPTIONS_CONTROLLERS_PORT4:
                    PADConfigure(3, wnd.hMainWindow);
                    return 0;

                // configure memcard in slot A
                case ID_OPTIONS_MEMCARDS_SLOTA:
                    MemcardConfigure(0, hwnd);
                    return 0;

                // configure memcard in slot B
                case ID_OPTIONS_MEMCARDS_SLOTB:
                    MemcardConfigure(1, hwnd);
                    return 0;

                // configure fonts
                case ID_OPTIONS_BOOTROMFONT:
                    FontConfigure(hwnd);
                    break;

                case ID_HELP_ABOUT:
                    AboutDialog(hwnd);
                    return 0;
            }
            break;

        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
        // file drag & drop operation

        case WM_DROPFILES:
        {
            TCHAR fileName[MAX_PATH] = { 0 };
            DragQueryFile((HDROP)wParam, 0, fileName, sizeof(fileName));
            DragFinish((HDROP)wParam);

            // extension filter
            if(_tcsicmp(_T(".dol"), _tcsrchr(fileName, _T('.'))) &&
               _tcsicmp(_T(".elf"), _tcsrchr(fileName, _T('.'))) &&
               _tcsicmp(_T(".bin"), _tcsrchr(fileName, _T('.'))) &&
               _tcsicmp(_T(".iso"), _tcsrchr(fileName, _T('.'))) &&
               _tcsicmp(_T(".gcm"), _tcsrchr(fileName, _T('.'))) ) break;

            name = fileName;
            goto loadFile;
        }
        return 0;

        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
        // resizing of main window

        case WM_SIZE:
        {
            // resize status bar window
            if(IsWindow(wnd.hStatusWindow))
            { 
                RECT rm, rs;
                GetWindowRect(hwnd, &rm);
                GetWindowRect(wnd.hStatusWindow, &rs); 
                long sbh = (WORD)(rs.bottom - rs.top);
                MoveWindow(wnd.hStatusWindow, rm.left, rm.top, rm.right - rm.left, rm.bottom - sbh, TRUE);
            }

            // resize selector
            {
                RECT rc;
                GetClientRect(hwnd, &rc);
                ResizeSelector(
                    (WORD)(rc.right - rc.left),
                    (WORD)(rc.bottom - rc.top)
                );
            }

            return 0;
        }

        // - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
        // selector notification messages

        case WM_NOTIFY:
        {
            if(wParam == ID_SELECTOR)
            {
                NotifySelector((LPNMHDR)lParam);

                // sort rules may change, by clicking on column
                SelectSort();
            }
            return 0;
        }

        case WM_DRAWITEM:
        {
            if(wParam == ID_SELECTOR)
            {
                DrawSelectorItem((LPDRAWITEMSTRUCT)lParam);
            }
            return 0;
        }        
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// self-explanatory.. creates main window and all child windows.
// window size will be set to default 400x300.
HWND CreateMainWindow(HINSTANCE hInstance)
{
    WNDCLASS wc = { 0 };
    const TCHAR CLASS_NAME[] = _T("GAMECUBECLASS");

    assert(wnd.hMainWindow == nullptr);

    wc.cbClsExtra    = wc.cbWndExtra = 0;
    wc.hbrBackground = (HBRUSH)GetStockObject(DKGRAY_BRUSH);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DOLWIN_ICON));
    wc.hInstance     = hInstance;
    wc.lpfnWndProc   = WindowProc;
    wc.lpszClassName = CLASS_NAME;
    wc.lpszMenuName  = MAKEINTRESOURCE(IDR_MAIN_MENU);
    wc.style         = 0;

    ATOM classAtom = RegisterClass(&wc);
    assert(classAtom != 0);

    auto win_name = fmt::format(L"{:s} - {:s} ({:s})", APPNAME, APPDESC, APPVER);
    wnd.hMainWindow = CreateWindowEx(
        0,
        CLASS_NAME,
        win_name.c_str(),
        WIN_STYLE, 
        20, 30,
        400, 300,
        NULL, NULL,
        hInstance, NULL);

    assert(wnd.hMainWindow);

    ShowWindow(wnd.hMainWindow, SW_NORMAL);
    UpdateWindow(wnd.hMainWindow);

    return wnd.hMainWindow;
}

// ---------------------------------------------------------------------------
// utilities

// change main window on-top state
void SetAlwaysOnTop(HWND hwnd, BOOL state)
{
    RECT rect;
    HWND ontop[] = { HWND_NOTOPMOST, HWND_TOPMOST };
    GetWindowRect(hwnd, &rect);
    SetWindowPos(
        hwnd, 
        ontop[state], 
        rect.left, rect.top, 
        rect.right - rect.left, rect.bottom - rect.top, 
        SWP_SHOWWINDOW
    );
    UpdateWindow(hwnd);
}

// center the child window into the parent window
void CenterChildWindow(HWND hParent, HWND hChild)
{
   if(IsWindow(hParent) && IsWindow(hChild))
   {
        RECT rp, rc;

        GetWindowRect(hParent, &rp);
        GetWindowRect(hChild, &rc);

        MoveWindow(hChild, 
            rp.left + (rp.right - rp.left - rc.right + rc.left) / 2,
            rp.top + (rp.bottom - rp.top - rc.bottom + rc.top) / 2,
            rc.right - rc.left, rc.bottom - rc.top, TRUE
        );
    }
}

void SetMenuItemText(HMENU hmenu, UINT id, std::wstring_view text)
{
    MENUITEMINFO info = { 0 };
       
    info.cbSize = sizeof(MENUITEMINFO);
    info.fMask  = MIIM_TYPE;
    info.fType  = MFT_STRING;
    info.dwTypeData = (LPTSTR)text.data();

    SetMenuItemInfo(hmenu, id, FALSE, &info);
}
