// Dolwin debugger interface;
// currently console debugger, migrated from 0.08+ is in use, 
// but you may add GUI aswell. see "DOCS\EMU\debug.txt" for details of 
// debugger replacement.
#include "dolphin.h"

// message output
static void dummy(const char *text, ...) {}
void (*DBHalt)(const char *text, ...)   = dummy;
void (*DBReport)(const char *text, ...) = dummy;

void DBOpen()           // open debugger window
{
    DBHalt = con_error;
    DBReport = con_print;
    con_open();
    DBRedraw();
}

void DBClose()          // close debugger window
{
    con_close();
    DBHalt = dummy;
    DBReport = dummy;

    // suspend, until OS is freeing resources
    Sleep(100);
}

void DBRedraw()         // redraw whole debug UI (may be slow)
{
    con.update |= CON_UPDATE_ALL;
    con_refresh();
}

void DBStart()          // start debugger loop
{
    con.update |= CON_UPDATE_ALL;
    con_refresh(1);
    con_start();
}
