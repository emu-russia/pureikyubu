#include "pch.h"

static HWND hStatusWindow = NULL;

void SetStatusHwnd(HWND hwnd)
{

}

// change text in specified statusbar part
void SetStatusText(int sbPart, const char* text, bool post)
{
    if (hStatusWindow == NULL) return;
    if (post)
    {
        PostMessage(hStatusWindow, SB_SETTEXT, (WPARAM)(sbPart), (LPARAM)text);
    }
    else
    {
        SendMessage(hStatusWindow, SB_SETTEXT, (WPARAM)(sbPart), (LPARAM)text);
    }
}

// get text of statusbar part
char* GetStatusText(int sbPart)
{
    static char sbText[256];

    if (hStatusWindow == NULL) return NULL;

    sbText[0] = 0;
    SendMessage(hStatusWindow, SB_GETTEXT, (WPARAM)(sbPart | 0), (LPARAM)sbText);
    return sbText;
}
