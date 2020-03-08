
#pragma once

#include <Windows.h>

// other console includes
#include "output.h"    // text output and refresh
#include "input.h"     // keyboard input
#include "cmd.h"       // command processor
#include "break.h"     // breakpoints control
#include "regs.h"      // registers view
#include "cpuview.h"   // cpu view (disassembly)
#include "memview.h"   // memory (data) view

// console controls
void    con_open();
void    con_close();
void    con_start();
void    con_break(char *reason=NULL);

#pragma pack(push, 8)

// all console important variables are here
typedef struct CONControl
{
    uint32_t            update;         // see CON_UPDATE_* in output.h
    int                 X, Y;
    uint16_t            attr;
    HWND                hwnd;           // console window handler
    jmp_buf             loop;           // console loop
    HANDLE              input, output;  // stdin/stdout
    CONSOLE_CURSOR_INFO curinfo;
    CHAR_INFO           buf[CON_HEIGHT][CON_WIDTH];
    uint32_t            text, data;     // effective address for cpu/mem view
    uint32_t            disa_cursor;    // cpuview cursor
    bool                active;         // TRUE, whenever console is active
    bool                running;        // TRUE, if running
    bool                log;            // flush messages into log-file
    char                logfile[256];   // HTML file for log output
    FILE*               logf;           // file descriptor
    DBPoint*            brks;           // breakpoint list
    int                 brknum;         // number of breakpoints
    NOPHistory*         nopHist;
    int                 nopNum;
} CONControl;

#pragma pack(pop)

extern  CONControl con;
