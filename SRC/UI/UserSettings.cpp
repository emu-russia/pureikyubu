// Dolwin settings dialog (to configure user variables)
#include "pch.h"

/*/
    Settings control behaviours are : normal, sticky and locked. 
    Normal: Can be changed anytime, emulator state also modified with control
    modifying.
    Sticky: Can be changed anytime, but emulator state will be modified after
    game Reload.
    Locked: Cannot be changed while emu running. 
/*/

// all user variables (except memory cards vars) are placed in UserConfig.h

// parent window and instance
static HWND         hParentWnd, hChildDlg[4];
static HINSTANCE    hParentInst;
static BOOL         settingsLoaded[4];
static BOOL         needSelUpdate;

static const TCHAR * tabs[] = 
{
    _T("Emulator"),
    _T("GUI/Selector"),
    _T("GCN Hardware"),
    _T("GCN HLE")
};

static struct ConsoleVersion
{
    uint32_t ver;
    const TCHAR*   info;
} consoleVersion[] = {
    { 0x00000001, _T("0x00000001: Retail 1") },
    { 0x00000002, _T("0x00000002: HW2 production board") },
    { 0x00000003, _T("0x00000003: The latest production board") },
    { 0x10000004, _T("0x10000004: 1st Devkit HW") },
    { 0x10000005, _T("0x10000005: 2nd Devkit HW") },
    { 0x10000006, _T("0x10000006: The latest Devkit HW") },
    { 0xffffffff, _T("0x%08X: User defined") }
};

static char * int2str(int i)
{
    static char str[16];
    sprintf_s (str, sizeof(str), "%i", i);
    return str;
}

// ---------------------------------------------------------------------------

static void LoadSettings(int n)         // dialogs created
{
    HWND hDlg = hChildDlg[n];

    // Emulator
    if(n == 0)
    {
        CheckDlgButton(hDlg, IDC_ENSURE_WINDALL, BST_UNCHECKED);
        EnableWindow(GetDlgItem(hDlg, IDC_WINDALL), 0);

        SendDlgItemMessage(hDlg, IDC_CPU_CORE, CB_RESETCONTENT, 0, 0);
        SendDlgItemMessage(hDlg, IDC_CPU_CORE, CB_INSERTSTRING, -1, (LPARAM)_T("Interpreter"));
        SendDlgItemMessage(hDlg, IDC_CPU_CORE, CB_SETCURSEL, 0, 0);

        int selected = GetConfigInt(USER_MMU, USER_CORE);
        SendDlgItemMessage(hDlg, IDC_MEMORY_MODE, CB_RESETCONTENT, 0, 0);
        SendDlgItemMessage(hDlg, IDC_MEMORY_MODE, CB_INSERTSTRING, -1, (LPARAM)_T("Simple translation"));
        SendDlgItemMessage(hDlg, IDC_MEMORY_MODE, CB_INSERTSTRING, -1, (LPARAM)_T("Advanced (Mmu)"));
        SendDlgItemMessage(hDlg, IDC_MEMORY_MODE, CB_SETCURSEL, selected, 0);

        settingsLoaded[0] = TRUE;
    }

    // GUI/Selector
    if(n == 1)
    {
        for (auto it = usel.paths.begin(); it != usel.paths.end(); ++it)
        {
            TCHAR* path = *it;
            SendDlgItemMessage(hDlg, IDC_PATHLIST, LB_ADDSTRING, 0, (LPARAM)path);
        }

        needSelUpdate = FALSE;
        settingsLoaded[1] = TRUE;
    }

    // GCN Hardware
    if(n == 2)
    {
        uint32_t ver = GetConfigInt(USER_CONSOLE, USER_HW);
        int i=0, selected = sizeof(consoleVersion) / 8 - 1;
        while(consoleVersion[i].ver != 0xffffffff)
        {
            if(consoleVersion[i].ver == ver)
            {
                selected = i;
                break;
            }
            i++;
        }
        if(consoleVersion[i].ver == 0xffffffff) selected = sizeof(consoleVersion)/8 - 1;
        i=0;
        SendDlgItemMessage(hDlg, IDC_CONSOLE_VER, CB_RESETCONTENT, 0, 0);
        do
        {
            SendDlgItemMessage(hDlg, IDC_CONSOLE_VER, CB_INSERTSTRING, -1, (LPARAM)consoleVersion[i].info);
        } while(consoleVersion[++i].ver != 0xffffffff);
        if(selected == sizeof(consoleVersion)/8 - 1)
        {
            TCHAR buf[100];
            _stprintf_s(buf, _countof(buf) - 1, consoleVersion[selected].info, ver);
            SendDlgItemMessage(hDlg, IDC_CONSOLE_VER, CB_INSERTSTRING, -1, (LPARAM)buf);
        }
        SendDlgItemMessage(hDlg, IDC_CONSOLE_VER, CB_SETCURSEL, selected, 0);

        SetDlgItemText(hDlg, IDC_BOOTROM_FILE, GetConfigString(USER_BOOTROM, USER_HW));
        SetDlgItemText(hDlg, IDC_DSPDROM_FILE, GetConfigString(USER_DSP_DROM, USER_HW));
        SetDlgItemText(hDlg, IDC_DSPIROM_FILE, GetConfigString(USER_DSP_IROM, USER_HW));

        settingsLoaded[2] = TRUE;
    }

    // GCN High Level
    if(n == 3)
    {
        CheckDlgButton(hDlg, IDC_MTXHLE, BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_DSP_FAKE, BST_UNCHECKED);
        BOOL flag = GetConfigBool(USER_HLE_MTX, USER_HLE);
        if(flag) CheckDlgButton(hDlg, IDC_MTXHLE, BST_CHECKED);

        settingsLoaded[3] = TRUE;
    }
}

static void SaveSettings()              // OK pressed
{
    int i;
    TCHAR buf[256] = { 0, };

    // Emulator
    if(settingsLoaded[0])
    {
        HWND hDlg = hChildDlg[0];
        int selected = (int)SendDlgItemMessage(hDlg, IDC_MEMORY_MODE, CB_GETCURSEL, 0, 0);
        SetConfigInt(USER_MMU, selected, USER_CORE);
    }

    // GUI/Selector
    if(settingsLoaded[1])
    {
        HWND hDlg = hChildDlg[1];

        TCHAR text[0x1000] = { 0, };
        int max = (int)SendDlgItemMessage(hDlg, IDC_PATHLIST, LB_GETCOUNT, 0, 0);

        // delete all dirs
        usel.paths.clear();
        SetConfigString(USER_PATH, (TCHAR *)_T(""), USER_UI);

        // add dirs again
        for(i=0; i<max; i++)
        {
            SendDlgItemMessage(hDlg, IDC_PATHLIST, LB_GETTEXT, i, (LPARAM)text);
            AddSelectorPath(text);
        }

        // update selector layout, if PATH has changed
        if(needSelUpdate)
        {
            UpdateSelector();
            needSelUpdate = FALSE;
        }
    }

    // GCN Hardwre
    if(settingsLoaded[2])
    {
        HWND hDlg = hChildDlg[2];
        int selected = (int)SendDlgItemMessage(hDlg, IDC_CONSOLE_VER, CB_GETCURSEL, 0, 0);
        if(selected == sizeof(consoleVersion)/8 - 1)
        {
            SendDlgItemMessage(hDlg, IDC_CONSOLE_VER, CB_GETLBTEXT, selected, (LPARAM)buf);
            uint32_t ver = _tcstoul(buf, NULL, 0);
            SetConfigInt(USER_CONSOLE, ver, USER_HW);
        }
        else SetConfigInt(USER_CONSOLE, consoleVersion[selected].ver, USER_HW);

        GetDlgItemText(hDlg, IDC_BOOTROM_FILE, buf, sizeof(buf));
        SetConfigString(USER_BOOTROM, buf, USER_HW);
        GetDlgItemText(hDlg, IDC_DSPDROM_FILE, buf, sizeof(buf));
        SetConfigString(USER_DSP_DROM, buf, USER_HW);
        GetDlgItemText(hDlg, IDC_DSPIROM_FILE, buf, sizeof(buf));
        SetConfigString(USER_DSP_IROM, buf, USER_HW);
    }

    // GCN High Level
    if(settingsLoaded[3])
    {
        HWND hDlg = hChildDlg[3];
        BOOL flag = IsDlgButtonChecked(hDlg, IDC_MTXHLE);
        SetConfigBool(USER_HLE_MTX, flag, USER_HLE);
    }
}

void ResetAllSettings()
{
    // Danger zone
}

// make sure path have ending '\\'
static void fix_path(TCHAR *path)
{
    size_t n = _tcslen(path);
    if(path[n-1] != _T('\\'))
    {
        path[n]   = _T('\\');
        path[n+1] = 0;
    }
}

// ---------------------------------------------------------------------------
// Emulator

static INT_PTR CALLBACK EmulatorSettingsProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    HWND propSheet;

    switch(message)
    {
        case WM_INITDIALOG:
            // seems propsheet callback is shit :)
            // this trick do the same job
            propSheet = GetParent(hDlg);
            CenterChildWindow(hParentWnd, propSheet);

            hChildDlg[0] = hDlg;
            LoadSettings(0);
            return TRUE;

        case WM_COMMAND:
            switch(wParam)
            {
                case IDC_WINDALL:
                {
                    ResetAllSettings();
                    UI::DolwinReport(
                        _T("All Settings have been deleted.\n")
                        _T("Default values will be restored after first run.")
                    );
                    for(int i=0; i<4; i++) LoadSettings(i);
                }
                break;
                
                case IDC_ENSURE_WINDALL:
                {
                    if(IsDlgButtonChecked(hDlg, IDC_ENSURE_WINDALL))
                        EnableWindow(GetDlgItem(hDlg, IDC_WINDALL), 1);
                    else
                        EnableWindow(GetDlgItem(hDlg, IDC_WINDALL), 0);
                }
                break;
            }
            break;

        case WM_NOTIFY:
            if(((NMHDR FAR *)lParam)->code == PSN_APPLY) SaveSettings();
            break;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// UserMenu

static INT_PTR CALLBACK UserMenuSettingsProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    int i;
    int curSel, max;
    TCHAR * path, text[1024];

    switch(message)
    {
        case WM_INITDIALOG:
            hChildDlg[1] = hDlg;
            LoadSettings(1);
            return TRUE;

        case WM_COMMAND:
            switch(wParam)
            {
                case IDC_FILEFILTER:
                    EditFileFilter(hDlg);
                    break;
                case IDC_ADDPATH:
                    if((path = UI::FileOpen(hDlg, UI::FileType::Directory)) != nullptr)
                    {
                        fix_path(path);

                        // check if already present
                        max = (int)SendDlgItemMessage(hDlg, IDC_PATHLIST, LB_GETCOUNT, 0, 0);
                        for(i=0; i<max; i++)
                        {
                            SendDlgItemMessage(hDlg, IDC_PATHLIST, LB_GETTEXT, i, (LPARAM)text);
                            if(!_tcsicmp(path, text)) break;
                        }

                        // add new path
                        if(i == max)
                        {
                            SendDlgItemMessage( hDlg, IDC_PATHLIST, LB_ADDSTRING,
                                                0, (LPARAM)path );
                            needSelUpdate = TRUE;
                        }
                    }
                    break;
                case IDC_KILLPATH:
                    curSel = (int)SendDlgItemMessage(hDlg, IDC_PATHLIST, LB_GETCURSEL, 0, 0);
                    SendDlgItemMessage(hDlg, IDC_PATHLIST, LB_DELETESTRING, (WPARAM)curSel, 0);
                    needSelUpdate = TRUE;
                    break;
            }
            break;
 
        case WM_NOTIFY:
            if(((NMHDR FAR *)lParam)->code == PSN_APPLY) SaveSettings();
            break;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// GCN Hardware

static INT_PTR CALLBACK HardwareSettingsProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    TCHAR * file = nullptr;

    switch(message)
    {
        case WM_INITDIALOG:
            hChildDlg[2] = hDlg;
            LoadSettings(2);
            return TRUE;

        case WM_NOTIFY:
            if(((NMHDR FAR *)lParam)->code == PSN_APPLY) SaveSettings();
            break;

        case WM_COMMAND:
            switch (wParam)
            {
                case IDC_CHOOSE_BOOTROM:
                    file = UI::FileOpen(wnd.hMainWindow, UI::FileType::All);
                    if (file != nullptr)
                    {
                        SetDlgItemText(hDlg, IDC_BOOTROM_FILE, file);
                    }
                    else
                    {
                        SetDlgItemText(hDlg, IDC_BOOTROM_FILE, _T(""));
                    }
                    break;
                case IDC_CHOOSE_DSPDROM:
                    file = UI::FileOpen(wnd.hMainWindow, UI::FileType::All);
                    if (file != nullptr)
                    {
                        SetDlgItemText(hDlg, IDC_DSPDROM_FILE, file);
                    }
                    else
                    {
                        SetDlgItemText(hDlg, IDC_DSPDROM_FILE, _T(""));
                    }
                    break;
                case IDC_CHOOSE_DSPIROM:
                    file = UI::FileOpen(wnd.hMainWindow, UI::FileType::All);
                    if (file != nullptr)
                    {
                        SetDlgItemText(hDlg, IDC_DSPIROM_FILE, file);
                    }
                    else
                    {
                        SetDlgItemText(hDlg, IDC_DSPIROM_FILE, _T(""));
                    }
                    break;
            }
            break;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// GCN High Level

static INT_PTR CALLBACK HighLevelSettingsProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            hChildDlg[3] = hDlg;
            LoadSettings(3);
            return TRUE;

        case WM_NOTIFY:
            if(((NMHDR FAR *)lParam)->code == PSN_APPLY) SaveSettings();
            break;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------

void OpenSettingsDialog(HWND hParent, HINSTANCE hInst)
{
    hParentWnd  = hParent;
    hParentInst = hInst;

    PROPSHEETPAGE psp[4] = { 0 };
    PROPSHEETHEADER psh = { 0 };

    // Emulator page
    psp[0].dwSize = sizeof(PROPSHEETPAGE);
    psp[0].dwFlags = PSP_USETITLE;
    psp[0].hInstance = hParentInst;
    psp[0].pszTemplate = MAKEINTRESOURCE(IDD_SETTINGS_EMU);
    psp[0].pfnDlgProc = EmulatorSettingsProc;
    psp[0].pszTitle = tabs[0];
    psp[0].lParam = 0;
    psp[0].pfnCallback = NULL;

    // UserMenu page
    psp[1].dwSize = sizeof(PROPSHEETPAGE);
    psp[1].dwFlags = PSP_USETITLE;
    psp[1].hInstance = hParentInst;
    psp[1].pszTemplate = MAKEINTRESOURCE(IDD_SETTINGS_GUI);
    psp[1].pfnDlgProc = UserMenuSettingsProc;
    psp[1].pszTitle = tabs[1];
    psp[1].lParam = 0;
    psp[1].pfnCallback = NULL;
    settingsLoaded[0] = FALSE;

    // Hardware page
    psp[2].dwSize = sizeof(PROPSHEETPAGE);
    psp[2].dwFlags = PSP_USETITLE;
    psp[2].hInstance = hParentInst;
    psp[2].pszTemplate = MAKEINTRESOURCE(IDD_SETTINGS_HW);
    psp[2].pfnDlgProc = HardwareSettingsProc;
    psp[2].pszTitle = tabs[2];
    psp[2].lParam = 0;
    psp[2].pfnCallback = NULL;
    settingsLoaded[1] = FALSE;

    // High Level page
    psp[3].dwSize = sizeof(PROPSHEETPAGE);
    psp[3].dwFlags = PSP_USETITLE;
    psp[3].hInstance = hParentInst;
    psp[3].pszTemplate = MAKEINTRESOURCE(IDD_SETTINGS_HLE);
    psp[3].pfnDlgProc = HighLevelSettingsProc;
    psp[3].pszTitle = tabs[3];
    psp[3].lParam = 0;
    psp[3].pfnCallback = NULL;
    settingsLoaded[2] = FALSE;

    // property sheet
    psh.dwSize = sizeof(PROPSHEETHEADER);
    psh.dwFlags = PSH_USEHICON | /*PSH_PROPTITLE |*/ PSH_NOAPPLYNOW | PSH_PROPSHEETPAGE;
    psh.hwndParent = hParentWnd;
    psh.hInstance = hParentInst;
    psh.hIcon = LoadIcon(hParentInst, MAKEINTRESOURCE(IDI_DOLWIN_ICON));
    psh.pszCaption = _T("Configure ") APPNAME;
    psh.nPages = sizeof(psp) / sizeof(PROPSHEETPAGE);
    psh.nStartPage = 0;
    psh.ppsp = (LPCPROPSHEETPAGE)&psp;
    psh.pfnCallback = NULL;
    settingsLoaded[3] = FALSE;

    PropertySheet(&psh);    // blocking call
}



/* ---------------------------------------------------------------------------
    Misc config section.
--------------------------------------------------------------------------- */


// ---------------------------------------------------------------------------
// file filter dialog

static void filter_string(HWND hDlg, uint32_t filter)
{
    TCHAR buf[64], *ptr = buf;
    const TCHAR * mask[] = { _T("*.dol"), _T("*.elf"), _T("*.gcm"), _T("*.iso") };

    filter = _byteswap_ulong(filter);

    for(int i=0; i<4; i++)
    {
        if(filter & 0xff) ptr += _stprintf_s(ptr, (ptr - buf), _T("%s;"), mask[i]);
        filter >>= 8;
    }
    ptr[-1] = 0;

    SetDlgItemText(hDlg, IDC_FILE_FILTER, buf);
}

static void check_filter(HWND hDlg, uint32_t filter)
{
    // DOL
    if(filter & 0xff000000) CheckDlgButton(hDlg, IDC_DOL_FILTER, BST_CHECKED);
    else CheckDlgButton(hDlg, IDC_DOL_FILTER, BST_UNCHECKED);
    // ELF
    if(filter & 0x00ff0000) CheckDlgButton(hDlg, IDC_ELF_FILTER, BST_CHECKED);
    else CheckDlgButton(hDlg, IDC_ELF_FILTER, BST_UNCHECKED);
    // GCM
    if(filter & 0x0000ff00) CheckDlgButton(hDlg, IDC_GCM_FILTER, BST_CHECKED);
    else CheckDlgButton(hDlg, IDC_GCM_FILTER, BST_UNCHECKED);
    // ISO
    if(filter & 0x000000ff) CheckDlgButton(hDlg, IDC_GMP_FILTER, BST_CHECKED);
    else CheckDlgButton(hDlg, IDC_GMP_FILTER, BST_UNCHECKED);
}

// dialog procedure
static INT_PTR CALLBACK FileFilterProc(
    HWND    hwndDlg,    // handle to dialog box
    UINT    uMsg,       // message
    WPARAM  wParam,     // first message parameter
    LPARAM  lParam      // second message parameter
)
{
    switch(uMsg)
    {
        // prepare dialog
        case WM_INITDIALOG:
        {
            CenterChildWindow(GetParent(hwndDlg), hwndDlg);

            // set dialog appearance
            ShowWindow(hwndDlg, SW_NORMAL);
            SendMessage(hwndDlg, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_DOLWIN_ICON)));

            // fill by default info
            usel.filter = GetConfigInt(USER_FILTER, USER_UI);
            filter_string(hwndDlg, usel.filter);
            check_filter(hwndDlg, usel.filter);
            return TRUE;
        }

        // close button
        case WM_CLOSE:
        {
            EndDialog(hwndDlg, 0);
            return TRUE;
        }

        // dialog controls
        case WM_COMMAND:
        {
            if(wParam == IDOK)
            {
                EndDialog(hwndDlg, 0);

                // save information and update selector (if filter was changed)
                if((uint32_t)GetConfigInt(USER_FILTER, USER_UI) != usel.filter)
                {
                    SetConfigInt(USER_FILTER, usel.filter, USER_UI);
                    UpdateSelector();
                }
                return TRUE;
            }
            switch(LOWORD(wParam))
            {
                case IDC_DOL_FILTER:                    // DOL
                    usel.filter ^= 0xff000000;
                    filter_string(hwndDlg, usel.filter);
                    check_filter(hwndDlg, usel.filter);
                    return TRUE;
                case IDC_ELF_FILTER:                    // ELF
                    usel.filter ^= 0x00ff0000;
                    filter_string(hwndDlg, usel.filter);
                    check_filter(hwndDlg, usel.filter);
                    return TRUE;
                case IDC_GCM_FILTER:                    // GCM
                    usel.filter ^= 0x0000ff00;
                    filter_string(hwndDlg, usel.filter);
                    check_filter(hwndDlg, usel.filter);
                    return TRUE;
                case IDC_GMP_FILTER:                    // ISO
                    usel.filter ^= 0x000000ff;
                    filter_string(hwndDlg, usel.filter);
                    check_filter(hwndDlg, usel.filter);
                    return TRUE;
            }
        }
    }

    return FALSE;
}

void EditFileFilter(HWND hwnd)
{
    DialogBox(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_FILE_FILTER),
        hwnd,
        FileFilterProc);
}
