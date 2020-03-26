// Dolwin about dialog
#include "dolphin.h"

static BOOL opened = FALSE;
static HWND dlgAbout;

// dialog procedure
static INT_PTR CALLBACK AboutProc(
    HWND    hwndDlg,    // handle to dialog box
    UINT    uMsg,       // message
    WPARAM  wParam,     // first message parameter
    LPARAM  lParam      // second message parameter
)
{
    UNREFERENCED_PARAMETER(lParam);

    TCHAR tmpbuf[256];    

    switch(uMsg)
    {
        // prepare swap dialog
        case WM_INITDIALOG:
        {
            dlgAbout = hwndDlg;
            ShowWindow(dlgAbout, SW_NORMAL);
            SendMessage(dlgAbout, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_DOLWIN_ICON)));
            CenterChildWindow(GetParent(dlgAbout), dlgAbout);

            _stprintf_s(
                tmpbuf,
                _countof(tmpbuf) - 1,
                APPNAME _T(" - ") APPDESC _T("\n")
                _T("Copyright 2003-2020, Dolwin team\n")
                _T("Build version %s (%s, %s) ")
#ifdef      _DEBUG
                _T("debug ")
#else
                _T("release ")
#endif

#if         _M_X64
                _T("X64")
#else
                _T("X86")
#endif

                ,
                APPVER,
                __DATE__,
                __TIME__
            );
            SetDlgItemText(dlgAbout, IDC_ABOUT_RELEASE, tmpbuf);
            return TRUE;
        }

        // close button -> kill about
        case WM_CLOSE:
        {
            DestroyWindow(dlgAbout);
            dlgAbout = NULL;
            opened = FALSE;
            break;
        }
        case WM_COMMAND:
        {
            if(wParam == IDCANCEL)
            {
                DestroyWindow(dlgAbout);
                dlgAbout = NULL;
                opened = FALSE;
                return TRUE;
            }
            if(wParam == IDOK)
            {
                DestroyWindow(dlgAbout);
                dlgAbout = NULL;
                opened = FALSE;
                return TRUE;
            }
            break;
        }
    }

    return FALSE;
}

// non-blocking call
void AboutDialog(HWND hwndParent)
{
    if(opened) return;

    // create modeless dialog
    CreateDialog(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_ABOUT),
        hwndParent,
        AboutProc
    );

    opened = TRUE;
}
