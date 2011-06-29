// DVD Configure dialog
#include "DVD.h"

void DVDConfigure()
{
    MessageBox(
        NULL, 
        "Nothing to configure.",
        "Configure DVD Plugin", 
        MB_ICONINFORMATION | MB_OK | MB_TOPMOST
    );
}

void DVDAbout()
{
    MessageBox(
        NULL, 
        "DVD Plugin (ver. " DVD_VER ")" "\n"
        "Based on Dolwin Plugin Specs (ver. " DOL_PLUG_VER ")",
        "About DVD", 
        MB_ICONINFORMATION | MB_OK | MB_TOPMOST
    );
}
