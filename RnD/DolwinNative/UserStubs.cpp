#include "pch.h"

// fatal error
void DolwinError(const char* title, const char* fmt, ...)
{
    va_list arg;
    char buf[0x1000];

    va_start(arg, fmt);
    vsprintf(buf, fmt, arg);
    va_end(arg);

    MessageBoxA(NULL, buf, title, MB_ICONHAND | MB_OK | MB_TOPMOST);

    exit(0);    // return bad
}

// fatal error, if user answers no
// return TRUE if "yes", and FALSE if "no"
BOOL DolwinQuestion(const char* title, const char* fmt, ...)
{
    va_list arg;
    char buf[0x1000];

    va_start(arg, fmt);
    vsprintf(buf, fmt, arg);
    va_end(arg);

    int btn = MessageBoxA(
        NULL,
        buf,
        title,
        MB_RETRYCANCEL | MB_ICONHAND | MB_TOPMOST
    );
    if (btn == IDCANCEL)
    {
        return FALSE;
    }
    else return TRUE;
}

// application message
void DolwinReport(const char* fmt, ...)
{
    va_list arg;
    char buf[0x1000];

    va_start(arg, fmt);
    vsprintf(buf, fmt, arg);
    va_end(arg);

    MessageBoxA(NULL, buf, "Dolwin Reports", MB_ICONINFORMATION | MB_OK | MB_TOPMOST);
}
