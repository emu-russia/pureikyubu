
#pragma once

// status bar parts enumerator
enum STATUS_ENUM
{
    STATUS_PROGRESS = 1,        // current emu state
    STATUS_FPS,                 // fps counter
    STATUS_TIMING,              // cpu timing (CF - Delay - Bailout)
    STATUS_TIME,                // time counter
};

void    SetStatusText(int sbPart, const char* text, bool post = false);
char*   GetStatusText(int sbPart);
