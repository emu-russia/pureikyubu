
#pragma once

#include <Windows.h>

#include <string>
#include <map>

// other console includes
#include "output.h"    // text output and refresh
#include "input.h"     // keyboard input
#include "cmd.h"       // command processor
#include "regs.h"      // registers view
#include "cpuview.h"   // cpu view (disassembly)
#include "memview.h"   // memory (data) view

// console controls
void    con_open();
void    con_close();
void    con_start();
void    con_break(const char *reason=NULL);
void    con_command(std::vector<std::string>& args, int lnum = 0);
void    con_step_into();

#pragma pack(push, 8)

// NOP command history
typedef struct NOPHistory
{
    uint32_t     ea;                 // effective address of NOP instruction
    uint32_t     oldValue;           // old value for "denop" command
} NOPHistory;

// all console important variables are here
class CONControl
{
public:
    uint32_t            update = 0;             // see CON_UPDATE_* in output.h
    int                 X = 0, Y = 0;
    uint16_t            attr = 0;
    HWND                hwnd = nullptr;         // console window handler
    HANDLE              input, output;          // stdin/stdout
    CONSOLE_CURSOR_INFO curinfo = { 0 };
    CHAR_INFO           buf[CON_HEIGHT][CON_WIDTH];
    uint32_t            text, data;             // effective address for cpu/mem view
    uint32_t            disa_cursor;            // cpuview cursor
    bool                active = false;         // TRUE, whenever console is active
    bool                log = false;            // flush messages into log-file
    char                logfile[256] = { 0 };   // HTML file for log output
    FILE*               logf = nullptr;         // file descriptor
    NOPHistory*         nopHist = nullptr;
    int                 nopNum = 0;
    std::map<std::string, Debug::CmdDelegate> cmds;
    MySpinLock::LOCK    reportLock = MySpinLock::LOCK_IS_FREE;
    std::atomic<bool>   exitPending = false;
};

#pragma pack(pop)

extern  CONControl con;
