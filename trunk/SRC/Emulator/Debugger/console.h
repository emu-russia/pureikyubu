// other console includes
#include "Debugger/output.h"    // text output and refresh
#include "Debugger/input.h"     // keyboard input
#include "Debugger/cmd.h"       // command processor
#include "Debugger/break.h"     // breakpoints control
#include "Debugger/regs.h"      // registers view
#include "Debugger/cpuview.h"   // cpu view (disassembly)
#include "Debugger/memview.h"   // memory (data) view

// console controls
void    con_open();
void    con_close();
void    con_start();
void    con_break(char *reason=NULL);

// all console important variables are here
typedef struct CONControl
{
    u32                 update;         // see CON_UPDATE_* in output.h
    int                 X, Y;
    u16                 attr;
    HWND                hwnd;           // console window handler
    jmp_buf             loop;           // console loop
    HANDLE              input, output;  // stdin/stdout
    CONSOLE_CURSOR_INFO curinfo;
    CHAR_INFO           buf[CON_HEIGHT][CON_WIDTH];
    u32                 text, data;     // effective address for cpu/mem view
    u32                 disa_cursor;    // cpuview cursor
    BOOL                active;         // TRUE, whenever console is active
    BOOL                running;        // TRUE, if running
    BOOL                log;            // flush messages into log-file
    char                logfile[256];   // HTML file for log output
    FILE*               logf;           // file descriptor
    DBPoint*            brks;           // breakpoint list
    s32                 brknum;         // number of breakpoints
    NOPHistory*         nopHist;
    int                 nopNum;
} CONControl;

extern  CONControl con;
