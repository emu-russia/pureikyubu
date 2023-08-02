// Dolwin about dialog
#include "pch.h"

static bool opened = false;
static HWND dlgAbout;

// dialog procedure
static INT_PTR CALLBACK AboutProc(
    HWND    hwndDlg,    // handle to dialog box
    UINT    uMsg,       // message
    WPARAM  wParam,     // first message parameter
    LPARAM  lParam      // second message parameter
)
{
#ifdef _DEBUG
    auto version = L"Debug";
#else
    auto version = L"Release";
#endif

#if _M_X64
    auto platform = L"x64";
#else
    auto platform = L"x86";
#endif

    auto jitc = UI::Jdi->JitcEnabled() ? L"JITC" : L"";

    UNREFERENCED_PARAMETER(lParam);
    switch(uMsg)
    {
        // prepare swap dialog
        case WM_INITDIALOG:
        {
            dlgAbout = hwndDlg;
            ShowWindow(dlgAbout, SW_NORMAL);
            SendMessage(dlgAbout, WM_SETICON, (WPARAM)ICON_BIG, (LPARAM)LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_DOLWIN_ICON)));
            CenterChildWindow(GetParent(dlgAbout), dlgAbout);

            std::string dateStamp = __DATE__;
            std::string timeStamp = __TIME__;

            auto buffer = fmt::format(L"{:s} - {:s}\n{:s}\n{:s} {:s} {:s} {:s} {:s} ({:s} {:s})\n",
                    APPNAME, APPDESC,
                    L"Copyright 2003,2004,2020,2021 Dolwin team",
                    L"Build version",
                    Util::StringToWstring(UI::Jdi->GetVersion()),
                    version, platform, jitc, 
                    Util::StringToWstring(dateStamp),
                    Util::StringToWstring(timeStamp) );

            SetDlgItemText(dlgAbout, IDC_ABOUT_RELEASE, buffer.c_str());
            return true;
        }

        // close button -> kill about
        case WM_CLOSE:
        {
            DestroyWindow(dlgAbout);
            dlgAbout = NULL;
            opened = false;
            break;
        }

        case WM_COMMAND:
        {
            if (wParam == IDCANCEL)
            {
                DestroyWindow(dlgAbout);
                dlgAbout = NULL;
                opened = false;
                return TRUE;
            }

            if (wParam == IDOK)
            {
                DestroyWindow(dlgAbout);
                dlgAbout = NULL;
                opened = false;
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

    opened = true;
}
