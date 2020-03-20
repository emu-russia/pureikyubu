#include "dolphin.h"

int um_num;
BOOL um_filechanged;

static char *NewMemcardFileProc(HWND hwnd, char * lastDir)
{
    char prevc[256];
    OPENFILENAMEA ofn;
    char szFileName[120];
    char szFileTitle[120];
    static char LoadedFile[256];

    _getcwd(prevc, 255);

    memset(&szFileName, 0, sizeof(szFileName));
    memset(&szFileTitle, 0, sizeof(szFileTitle));

    ofn.lStructSize         = sizeof(OPENFILENAME);
    ofn.hwndOwner           = hwnd;
    ofn.lpstrFilter         = 
        "GameCube Memcard Files (*.mci)\0*.mci\0"
        "All Files (*.*)\0*.*\0";
    ofn.lpstrCustomFilter   = NULL;
    ofn.nMaxCustFilter      = 0;
    ofn.nFilterIndex        = 1;
    ofn.lpstrFile           = szFileName;
    ofn.nMaxFile            = 120;
    ofn.lpstrInitialDir     = lastDir;
    ofn.lpstrFileTitle      = szFileTitle;
    ofn.nMaxFileTitle       = 120;
    ofn.lpstrTitle          = "Create Memcard File\0";
    ofn.lpstrDefExt         = "";
    ofn.Flags               = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
         
    if(GetSaveFileNameA(&ofn))
    {
        int i;

        for(i=0; i<120; i++) 
        {
            LoadedFile[i] = szFileName[i];
        }
            
        LoadedFile[i] = '\0';       // terminate
        
        _chdir(prevc);
        return LoadedFile;
    }
    else
    {
        _chdir(prevc);
        return NULL;
    }
}

static char *ChooseMemcardFileProc(HWND hwnd, char * lastDir)
{
    char prevc[256];
    OPENFILENAMEA ofn;
    char szFileName[120];
    char szFileTitle[120];
    static char LoadedFile[256];

    _getcwd(prevc, 255);

    memset(&szFileName, 0, sizeof(szFileName));
    memset(&szFileTitle, 0, sizeof(szFileTitle));

    ofn.lStructSize         = sizeof(OPENFILENAME);
    ofn.hwndOwner           = hwnd;
    ofn.lpstrFilter         = 
        "GameCube Memcard Files (*.mci)\0*.mci\0"
        "All Files (*.*)\0*.*\0";
    ofn.lpstrCustomFilter   = NULL;
    ofn.nMaxCustFilter      = 0;
    ofn.nFilterIndex        = 1;
    ofn.lpstrFile           = szFileName;
    ofn.nMaxFile            = 120;
    ofn.lpstrInitialDir     = lastDir;
    ofn.lpstrFileTitle      = szFileTitle;
    ofn.nMaxFileTitle       = 120;
    ofn.lpstrTitle          = "Open Memcard File\0";
    ofn.lpstrDefExt         = "";
    ofn.Flags               = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
         
    if(GetOpenFileNameA(&ofn))
    {
        int i;

        for(i=0; i<120; i++) 
        {
            LoadedFile[i] = szFileName[i];
        }
            
        LoadedFile[i] = '\0';       // terminate
          
        _chdir(prevc);
        return LoadedFile;
    }
    else
    {
        _chdir(prevc);
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
    wchar_t buf[256];

    switch(uMsg)
    {
        case WM_INITDIALOG:
            CenterChildWindow(wnd.hMainWindow, hwndDlg);
            SendMessage(hwndDlg, WM_SETICON,(WPARAM)ICON_BIG, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_DOLWIN_ICON)) );

            for (index = 0; index < Num_Memcard_ValidSizes; index ++) {
                int blocks, kb;
                blocks = Memcard_ValidSizes[index] / Memcard_BlockSize;
                kb = Memcard_ValidSizes[index] / 1024;
                swprintf_s (buf, _countof(buf), L"%d blocks  (%d Kb)", blocks, kb);
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
    char buf[256], buf2[256], *filename;
    long newsize;

    switch(uMsg)
    {
        case WM_INITDIALOG:
            CenterChildWindow(wnd.hMainWindow, hwndDlg);
            SendMessage(hwndDlg, WM_SETICON,(WPARAM)ICON_BIG, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_DOLWIN_ICON)) );

            if (um_num == 0)
                SendMessage(hwndDlg, WM_SETTEXT, (WPARAM)0, lParam = (LPARAM)L"Memcard A Settings");
            else if (um_num == 1)
                SendMessage(hwndDlg, WM_SETTEXT, (WPARAM)0, lParam = (LPARAM)L"Memcard B Settings");

            if (SyncSave == TRUE)
                CheckRadioButton(hwndDlg, IDC_MEMCARD_SYNCSAVE_FALSE,
                                IDC_MEMCARD_SYNCSAVE_TRUE, IDC_MEMCARD_SYNCSAVE_TRUE );
            else 
                CheckRadioButton(hwndDlg, IDC_MEMCARD_SYNCSAVE_FALSE,
                                IDC_MEMCARD_SYNCSAVE_TRUE, IDC_MEMCARD_SYNCSAVE_FALSE );

            if (emu.running == TRUE) {
                EnableWindow(GetDlgItem(hwndDlg, IDC_MEMCARD_SYNCSAVE_FALSE), FALSE);
                EnableWindow(GetDlgItem(hwndDlg, IDC_MEMCARD_SYNCSAVE_TRUE), FALSE);
            }

            if (Memcard_Connected[um_num] == TRUE)
                CheckDlgButton(hwndDlg, IDC_MEMCARD_CONNECTED, BST_CHECKED);

            memcpy(buf, memcard[um_num].filename, sizeof(memcard[um_num].filename));
            filename = strrchr(buf, '\\');
            if (filename == NULL) {
                SendDlgItemMessageA(hwndDlg, IDC_MEMCARD_FILE, WM_SETTEXT,  (WPARAM)0, (LPARAM)(LPCTSTR)buf);
            }
            else {
                *filename = '\0';
                filename++;
                SendDlgItemMessageA(hwndDlg, IDC_MEMCARD_FILE, WM_SETTEXT,  (WPARAM)0, (LPARAM)(LPCTSTR)filename);
                SendDlgItemMessageA(hwndDlg, IDC_MEMCARD_PATH, WM_SETTEXT,  (WPARAM)0, (LPARAM)(LPCTSTR)buf);
            }

            if (memcard[um_num].connected == TRUE) {
                sprintf_s (buf, sizeof(buf), "Size: %d usable blocks (%d Kb)", (int)(memcard[um_num].size / Memcard_BlockSize - 5), (int)(memcard[um_num].size / 1024));
                SendDlgItemMessageA(hwndDlg, IDC_MEMCARD_SIZEDESC, WM_SETTEXT,  (WPARAM)0, (LPARAM)(LPCTSTR)buf);
            }
            else {
                SendDlgItemMessage(hwndDlg, IDC_MEMCARD_SIZEDESC, WM_SETTEXT,  (WPARAM)0, (LPARAM)L"Not connected");
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
                strcpy_s(buf, sizeof(buf), filename );

                /* create the file */
                if (MCCreateMemcardFile(filename, (uint16_t)(newsize >> 17)) == FALSE) return TRUE ;

                filename = strrchr(buf, '\\');
                if (filename == NULL) {
                    SendDlgItemMessageA(hwndDlg, IDC_MEMCARD_FILE, WM_SETTEXT,  (WPARAM)0, (LPARAM)(LPCTSTR)buf);
                }
                else {
                    *filename = '\0';
                    filename++;
                    SendDlgItemMessageA(hwndDlg, IDC_MEMCARD_FILE, WM_SETTEXT,  (WPARAM)0, (LPARAM)(LPCTSTR)filename);
                    SendDlgItemMessageA(hwndDlg, IDC_MEMCARD_PATH, WM_SETTEXT,  (WPARAM)0, (LPARAM)(LPCTSTR)buf);
                }

                SendDlgItemMessage(hwndDlg, IDC_MEMCARD_SIZEDESC, WM_SETTEXT,  (WPARAM)0, (LPARAM)L"Not connected");

                um_filechanged = TRUE;
                return TRUE;

            case IDC_MEMCARD_CHOOSEFILE:
                SendDlgItemMessageA(hwndDlg, IDC_MEMCARD_PATH, WM_GETTEXT,  (WPARAM)256, (LPARAM)(LPCTSTR)buf);
                filename = ChooseMemcardFileProc(hwndDlg, buf);
                if (filename == NULL) return TRUE;
                strcpy_s (buf, sizeof(buf), filename );

                filename = strrchr(buf, '\\');
                if (filename == NULL) {
                    SendDlgItemMessageA(hwndDlg, IDC_MEMCARD_FILE, WM_SETTEXT,  (WPARAM)0, (LPARAM)(LPCTSTR)buf);
                }
                else {
                    *filename = '\0';
                    filename++;
                    SendDlgItemMessageA(hwndDlg, IDC_MEMCARD_FILE, WM_SETTEXT,  (WPARAM)0, (LPARAM)(LPCTSTR)filename);
                    SendDlgItemMessageA(hwndDlg, IDC_MEMCARD_PATH, WM_SETTEXT,  (WPARAM)0, (LPARAM)(LPCTSTR)buf);
                }
                SendDlgItemMessage(hwndDlg, IDC_MEMCARD_SIZEDESC, WM_SETTEXT,  (WPARAM)0, (LPARAM)L"Not connected");

                um_filechanged = TRUE;
                return TRUE;
            case IDCANCEL:
                EndDialog(hwndDlg, 0);
                return TRUE;
            case IDOK:
                if (um_filechanged == TRUE )
                {
                    int Fnsize, Pathsize;
                    Fnsize = SendDlgItemMessage(hwndDlg, IDC_MEMCARD_FILE, WM_GETTEXTLENGTH,  (WPARAM)0, (LPARAM)0);
                    Pathsize = SendDlgItemMessage(hwndDlg, IDC_MEMCARD_PATH, WM_GETTEXTLENGTH,  (WPARAM)0, (LPARAM)0);

                    if (Fnsize+1 + Pathsize+1 >= sizeof (memcard[um_num].filename)) {
                        sprintf_s (buf, sizeof(buf), "File full path must be less than %i characters.", sizeof (memcard[um_num].filename) );
                        MessageBoxA(hwndDlg, buf, "Invalid filename", 0);
                        return TRUE;
                    }

                    SendDlgItemMessageA(hwndDlg, IDC_MEMCARD_PATH, WM_GETTEXT,  (WPARAM)(Pathsize+1), (LPARAM)(LPCTSTR)buf);
                    SendDlgItemMessageA(hwndDlg, IDC_MEMCARD_FILE, WM_GETTEXT,  (WPARAM)(Fnsize+1), (LPARAM)(LPCTSTR)buf2);

                    strcat_s(buf, sizeof(buf), "\\");
                    strcat_s(buf, sizeof(buf), buf2);
                }

                if (emu.running == FALSE) {
                    if (IsDlgButtonChecked(hwndDlg, IDC_MEMCARD_SYNCSAVE_FALSE) == BST_CHECKED  )
                        SyncSave = FALSE;
                    else
                        SyncSave = TRUE;
                }
                if (IsDlgButtonChecked(hwndDlg, IDC_MEMCARD_CONNECTED) == BST_CHECKED ) {
                    /* memcard is supposed to be connected */

                    if (um_filechanged == TRUE) /* file was changed */
                        MCUseFile(um_num, buf, TRUE);
                    else if (Memcard_Connected[um_num] == FALSE || memcard[um_num].connected == FALSE )
                            if (MCConnect (um_num) == FALSE)
                                MessageBoxA(wnd.hMainWindow, "Error while trying to connect the memcards.", "Memcard Error", 0) ;

                    Memcard_Connected[um_num] = TRUE;
                }
                else {
                    /* memcard is supposed to be disconnected */

                    if (um_filechanged == TRUE) /* file was changed */
                        MCUseFile(um_num, buf, FALSE);
                    else if (Memcard_Connected[um_num] == TRUE || memcard[um_num].connected == TRUE )
                                MCDisconnect(um_num);

                    Memcard_Connected[um_num] = FALSE;
                }

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
    BOOL opened;
    if ((num != 0) && (num != 1)) return ;
    um_num = num;

    opened = MCOpened;
    if (opened == FALSE)
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
    if (opened == FALSE)
        MCClose ();
}
