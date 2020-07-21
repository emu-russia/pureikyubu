#include "pch.h"

int um_num;
BOOL um_filechanged;

static TCHAR *NewMemcardFileProc(HWND hwnd, TCHAR * lastDir)
{
    TCHAR prevc[MAX_PATH];
    OPENFILENAME ofn;
    TCHAR szFileName[120];
    TCHAR szFileTitle[120];
    static TCHAR LoadedFile[MAX_PATH];

    GetCurrentDirectory(sizeof(prevc), prevc);

    memset(&szFileName, 0, sizeof(szFileName));
    memset(&szFileTitle, 0, sizeof(szFileTitle));

    ofn.lStructSize         = sizeof(OPENFILENAME);
    ofn.hwndOwner           = hwnd;
    ofn.lpstrFilter         = 
        _T("GameCube Memcard Files (*.mci)\0*.mci\0")
        _T("All Files (*.*)\0*.*\0");
    ofn.lpstrCustomFilter   = NULL;
    ofn.nMaxCustFilter      = 0;
    ofn.nFilterIndex        = 1;
    ofn.lpstrFile           = szFileName;
    ofn.nMaxFile            = 120;
    ofn.lpstrInitialDir     = lastDir;
    ofn.lpstrFileTitle      = szFileTitle;
    ofn.nMaxFileTitle       = 120;
    ofn.lpstrTitle          = _T("Create Memcard File\0");
    ofn.lpstrDefExt         = _T("");
    ofn.Flags               = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
         
    if(GetSaveFileName(&ofn))
    {
        int i;

        for(i=0; i<120; i++) 
        {
            LoadedFile[i] = szFileName[i];
        }
            
        LoadedFile[i] = '\0';       // terminate
        
        SetCurrentDirectory(prevc);
        return LoadedFile;
    }
    else
    {
        SetCurrentDirectory(prevc);
        return NULL;
    }
}

static TCHAR *ChooseMemcardFileProc(HWND hwnd, TCHAR * lastDir)
{
    TCHAR prevc[MAX_PATH];
    OPENFILENAME ofn;
    TCHAR szFileName[120];
    TCHAR szFileTitle[120];
    static TCHAR LoadedFile[MAX_PATH];

    GetCurrentDirectory(sizeof(prevc), prevc);

    memset(&szFileName, 0, sizeof(szFileName));
    memset(&szFileTitle, 0, sizeof(szFileTitle));

    ofn.lStructSize         = sizeof(OPENFILENAME);
    ofn.hwndOwner           = hwnd;
    ofn.lpstrFilter         = 
        _T("GameCube Memcard Files (*.mci)\0*.mci\0")
        _T("All Files (*.*)\0*.*\0");
    ofn.lpstrCustomFilter   = NULL;
    ofn.nMaxCustFilter      = 0;
    ofn.nFilterIndex        = 1;
    ofn.lpstrFile           = szFileName;
    ofn.nMaxFile            = 120;
    ofn.lpstrInitialDir     = lastDir;
    ofn.lpstrFileTitle      = szFileTitle;
    ofn.nMaxFileTitle       = 120;
    ofn.lpstrTitle          = _T("Open Memcard File\0");
    ofn.lpstrDefExt         = _T("");
    ofn.Flags               = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
         
    if(GetOpenFileName(&ofn))
    {
        int i;

        for(i=0; i<120; i++) 
        {
            LoadedFile[i] = szFileName[i];
        }
            
        LoadedFile[i] = '\0';       // terminate
          
        SetCurrentDirectory(prevc);
        return LoadedFile;
    }
    else
    {
        SetCurrentDirectory(prevc);
        return NULL;
    }
}

/*
 * Callback procedure for the choose size (of a new memcard) dialog
 */
static INT_PTR CALLBACK MemcardChooseSizeProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    int index;
    TCHAR buf[256] = { 0 };

    switch(uMsg)
    {
        case WM_INITDIALOG:
            CenterChildWindow(wnd.hMainWindow, hwndDlg);
            SendMessage(hwndDlg, WM_SETICON,(WPARAM)ICON_BIG, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_DOLWIN_ICON)) );

            for (index = 0; index < Num_Memcard_ValidSizes; index ++) {
                int blocks, kb;
                blocks = Memcard_ValidSizes[index] / Memcard_BlockSize;
                kb = Memcard_ValidSizes[index] / 1024;
                _stprintf_s (buf, _countof(buf) - 1, _T("%d blocks  (%d Kb)"), blocks, kb);
                SendDlgItemMessage(hwndDlg, IDC_MEMCARD_SIZES, CB_INSERTSTRING, (WPARAM)index, (LPARAM)buf);
            }
            SendDlgItemMessage(hwndDlg, IDC_MEMCARD_SIZES, CB_SETCURSEL, (WPARAM)0,  (LPARAM)0);

            return TRUE;
        case WM_CLOSE:
            EndDialog(hwndDlg, -1);
            return TRUE;
        case WM_COMMAND:
            switch(wParam) {
            case IDCANCEL:
                EndDialog(hwndDlg, -1);
                return TRUE;
            case IDOK:
                index = (int)SendDlgItemMessage(hwndDlg, IDC_MEMCARD_SIZES, CB_GETCURSEL, 0, 0);
                EndDialog(hwndDlg, index);

                return TRUE;
            }
            return FALSE;
        default:
            return FALSE;
    }
    return FALSE;
}

/*
 * Callback procedure for the memcard settings dialog
 */
static INT_PTR CALLBACK MemcardSettingsProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    TCHAR buf[MAX_PATH] = { 0 }, buf2[MAX_PATH] = { 0 }, * filename;
    size_t newsize;

    switch(uMsg)
    {
        case WM_INITDIALOG:
            CenterChildWindow(wnd.hMainWindow, hwndDlg);
            SendMessage(hwndDlg, WM_SETICON,(WPARAM)ICON_BIG, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_DOLWIN_ICON)) );

            if (um_num == 0)
                SendMessage(hwndDlg, WM_SETTEXT, (WPARAM)0, lParam = (LPARAM)_T("Memcard A Settings"));
            else if (um_num == 1)
                SendMessage(hwndDlg, WM_SETTEXT, (WPARAM)0, lParam = (LPARAM)_T("Memcard B Settings"));

            if (SyncSave)
                CheckRadioButton(hwndDlg, IDC_MEMCARD_SYNCSAVE_FALSE,
                                IDC_MEMCARD_SYNCSAVE_TRUE, IDC_MEMCARD_SYNCSAVE_TRUE );
            else 
                CheckRadioButton(hwndDlg, IDC_MEMCARD_SYNCSAVE_FALSE,
                                IDC_MEMCARD_SYNCSAVE_TRUE, IDC_MEMCARD_SYNCSAVE_FALSE );

            if (emu.loaded) {
                EnableWindow(GetDlgItem(hwndDlg, IDC_MEMCARD_SYNCSAVE_FALSE), FALSE);
                EnableWindow(GetDlgItem(hwndDlg, IDC_MEMCARD_SYNCSAVE_TRUE), FALSE);
            }

            if (Memcard_Connected[um_num])
                CheckDlgButton(hwndDlg, IDC_MEMCARD_CONNECTED, BST_CHECKED);

            _tcscpy(buf, memcard[um_num].filename);
            filename = _tcsrchr(buf, _T('\\'));
            if (filename == NULL) {
                SendDlgItemMessage(hwndDlg, IDC_MEMCARD_FILE, WM_SETTEXT,  (WPARAM)0, (LPARAM)(LPCTSTR)buf);
            }
            else {
                *filename = _T('\0');
                filename++;
                SendDlgItemMessage(hwndDlg, IDC_MEMCARD_FILE, WM_SETTEXT,  (WPARAM)0, (LPARAM)(LPCTSTR)filename);
                SendDlgItemMessage(hwndDlg, IDC_MEMCARD_PATH, WM_SETTEXT,  (WPARAM)0, (LPARAM)(LPCTSTR)buf);
            }

            if (memcard[um_num].connected) {
                _stprintf_s (buf, _countof(buf) - 1, _T("Size: %d usable blocks (%d Kb)"),
                    (int)(memcard[um_num].size / Memcard_BlockSize - 5), (int)(memcard[um_num].size / 1024));
                SendDlgItemMessage(hwndDlg, IDC_MEMCARD_SIZEDESC, WM_SETTEXT,  (WPARAM)0, (LPARAM)(LPCTSTR)buf);
            }
            else {
                SendDlgItemMessage(hwndDlg, IDC_MEMCARD_SIZEDESC, WM_SETTEXT,  (WPARAM)0, (LPARAM)_T("Not connected"));
            }

            um_filechanged = FALSE;
            return TRUE;
        case WM_CLOSE:
            EndDialog(hwndDlg, 0);
            return TRUE;
        case WM_COMMAND:
            switch(wParam) {
            case IDC_MEMCARD_NEW:
                newsize = DialogBox(GetModuleHandle(NULL),
                        MAKEINTRESOURCE(IDD_MEMCARD_CHOOSESIZE),
                        hwndDlg,
                        MemcardChooseSizeProc);
                if (newsize == -1) return TRUE;
                newsize = Memcard_ValidSizes[newsize];

                SendDlgItemMessage(hwndDlg, IDC_MEMCARD_PATH, WM_GETTEXT,  (WPARAM)256, (LPARAM)(LPCTSTR)buf);
                filename = NewMemcardFileProc(hwndDlg, buf);
                if (filename == NULL) return TRUE;
                _tcscpy_s(buf, _countof(buf) - 1, filename );

                /* create the file */
                if (MCCreateMemcardFile(filename, (uint16_t)(newsize >> 17)) == FALSE) return TRUE ;

                filename = _tcsrchr(buf, _T('\\'));
                if (filename == NULL) {
                    SendDlgItemMessage(hwndDlg, IDC_MEMCARD_FILE, WM_SETTEXT,  (WPARAM)0, (LPARAM)(LPCTSTR)buf);
                }
                else {
                    *filename = '\0';
                    filename++;
                    SendDlgItemMessage(hwndDlg, IDC_MEMCARD_FILE, WM_SETTEXT,  (WPARAM)0, (LPARAM)(LPCTSTR)filename);
                    SendDlgItemMessage(hwndDlg, IDC_MEMCARD_PATH, WM_SETTEXT,  (WPARAM)0, (LPARAM)(LPCTSTR)buf);
                }

                SendDlgItemMessage(hwndDlg, IDC_MEMCARD_SIZEDESC, WM_SETTEXT,  (WPARAM)0, (LPARAM)_T("Not connected"));

                um_filechanged = TRUE;
                return TRUE;

            case IDC_MEMCARD_CHOOSEFILE:
                SendDlgItemMessage(hwndDlg, IDC_MEMCARD_PATH, WM_GETTEXT,  (WPARAM)256, (LPARAM)(LPCTSTR)buf);
                filename = ChooseMemcardFileProc(hwndDlg, buf);
                if (filename == NULL) return TRUE;
                _tcscpy_s (buf, _countof(buf) - 1, filename );

                filename = _tcsrchr(buf, _T('\\'));
                if (filename == NULL) {
                    SendDlgItemMessage(hwndDlg, IDC_MEMCARD_FILE, WM_SETTEXT,  (WPARAM)0, (LPARAM)(LPCTSTR)buf);
                }
                else {
                    *filename = '\0';
                    filename++;
                    SendDlgItemMessage(hwndDlg, IDC_MEMCARD_FILE, WM_SETTEXT,  (WPARAM)0, (LPARAM)(LPCTSTR)filename);
                    SendDlgItemMessage(hwndDlg, IDC_MEMCARD_PATH, WM_SETTEXT,  (WPARAM)0, (LPARAM)(LPCTSTR)buf);
                }
                SendDlgItemMessage(hwndDlg, IDC_MEMCARD_SIZEDESC, WM_SETTEXT,  (WPARAM)0, (LPARAM)_T("Not connected"));

                um_filechanged = TRUE;
                return TRUE;
            case IDCANCEL:
                EndDialog(hwndDlg, 0);
                return TRUE;
            case IDOK:
                if (um_filechanged == TRUE )
                {
                    size_t Fnsize, Pathsize;
                    Fnsize = SendDlgItemMessage(hwndDlg, IDC_MEMCARD_FILE, WM_GETTEXTLENGTH,  (WPARAM)0, (LPARAM)0);
                    Pathsize = SendDlgItemMessage(hwndDlg, IDC_MEMCARD_PATH, WM_GETTEXTLENGTH,  (WPARAM)0, (LPARAM)0);

                    if (Fnsize+1 + Pathsize+1 >= sizeof (memcard[um_num].filename)) {
                        _stprintf_s (buf, _countof(buf) - 1, _T("File full path must be less than %zi characters."), sizeof (memcard[um_num].filename) );
                        MessageBox(hwndDlg, buf, _T("Invalid filename"), 0);
                        return TRUE;
                    }

                    SendDlgItemMessage(hwndDlg, IDC_MEMCARD_PATH, WM_GETTEXT,  (WPARAM)(Pathsize+1), (LPARAM)(LPCTSTR)buf);
                    SendDlgItemMessage(hwndDlg, IDC_MEMCARD_FILE, WM_GETTEXT,  (WPARAM)(Fnsize+1), (LPARAM)(LPCTSTR)buf2);

                    _tcscat_s(buf, _countof(buf) - 1, _T("\\"));
                    _tcscat_s(buf, _countof(buf) - 1, buf2);
                }

                if (!emu.loaded) {
                    if (IsDlgButtonChecked(hwndDlg, IDC_MEMCARD_SYNCSAVE_FALSE) == BST_CHECKED  )
                        SyncSave = false;
                    else
                        SyncSave = true;
                }
                if (IsDlgButtonChecked(hwndDlg, IDC_MEMCARD_CONNECTED) == BST_CHECKED ) {
                    /* memcard is supposed to be connected */

                    if (um_filechanged == TRUE) /* file was changed */
                        MCUseFile(um_num, buf, true);
                    else if (Memcard_Connected[um_num] == FALSE || memcard[um_num].connected == FALSE )
                            if (MCConnect (um_num) == FALSE)
                                MessageBox(wnd.hMainWindow, _T("Error while trying to connect the memcards."), _T("Memcard Error"), 0) ;

                    Memcard_Connected[um_num] = true;
                }
                else {
                    /* memcard is supposed to be disconnected */

                    if (um_filechanged == TRUE) /* file was changed */
                        MCUseFile(um_num, buf, false);
                    else if (Memcard_Connected[um_num] || memcard[um_num].connected )
                                MCDisconnect(um_num);

                    Memcard_Connected[um_num] = false;
                }

                // Writeback settings

                if (um_num == 0)
                {
                    SetConfigBool(MemcardA_Connected_Key, Memcard_Connected[0], USER_MEMCARDS);
                    SetConfigString(MemcardA_Filename_Key, memcard[0].filename, USER_MEMCARDS);
                }
                else
                {
                    SetConfigBool(MemcardB_Connected_Key, Memcard_Connected[1], USER_MEMCARDS);
                    SetConfigString(MemcardB_Filename_Key, memcard[1].filename, USER_MEMCARDS);
                }
                SetConfigBool(Memcard_SyncSave_Key, SyncSave, USER_MEMCARDS);

                EndDialog(hwndDlg, 0);
                return TRUE;
            }
            return FALSE;
        default:
            return FALSE;
    }
    return FALSE;
}

/*
 * Calls the memcard settings dialog
 */ 
void MemcardConfigure(int num, HWND hParent) {
    bool openedBefore;
    if ((num != 0) && (num != 1)) return ;
    um_num = num;

    openedBefore = MCOpened;
    if (!openedBefore)
    {
        HWConfig confg = { 0 };
        EMUGetHwConfig(&confg);
        MCOpen(&confg); // This Dialog needs that the memcard are connected if they are supposed to be
    }
    DialogBox(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_MEMCARD_SETTINGS),
        hParent,
        MemcardSettingsProc);
    if (!openedBefore)
        MCClose ();
}
