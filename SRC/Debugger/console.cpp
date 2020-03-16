// console init code.
//
// recommended console font size is 6x9.
#include "pch.h"

// all console important variables are here
CONControl con;

// ---------------------------------------------------------------------------

static BOOL CALLBACK WndEnumProc(HWND hWnd, LPARAM lpParam)
{
    TCHAR szClassName[1024];
    DWORD dwPid = 0;

    ZeroMemory(szClassName, sizeof(szClassName));

    if(!GetClassName(hWnd, szClassName, sizeof(szClassName))) return TRUE;
    if(lstrcmp(szClassName, TEXT("tty")) != 0) return TRUE;

    GetWindowThreadProcessId(hWnd, &dwPid);
    if(dwPid == GetCurrentProcessId())
    {
        *(HWND*)lpParam = hWnd;
        return FALSE;
    }

    return TRUE;
}

static HWND MyGetConsoleWindow(void)
{
    HWND hwnd = NULL;
    EnumWindows((WNDENUMPROC)WndEnumProc, (LPARAM)&hwnd);
    return hwnd;
}

void con_open()
{
    DWORD flags;
    COORD coord;
    SMALL_RECT rect;
    RECT wndrect;

    if(con.active) return;

    // clear internal structures
    memset(&roll, 0, sizeof(ROLLControl));
    memset(&wind, 0, sizeof(WINDControl));

    cmd_init_handlers();

    // prepare console structures
    con_set_autoscroll(TRUE);
    roll.rollpos = con_wraproll(0, -1);
    con_memorize_cpu_regs();

    wind.full = 0;
    wind.visible |= CON_UPDATE_ALL;
    wind.focus = WCONSOLE;
    wind.regs_h = 17;
    wind.data_h = 8;
    wind.disa_h = 18; // 16
    wind.disa_sub_h = 0;
    con_recalc_wnds();

    con.data = 0x80000000;
    con.text = PC;
    con_set_disa_cur(con.text);
    strcpy_s (con.logfile, sizeof(con.logfile), CON_LOG_FILE);

    // create console 
    AllocConsole();
    con.hwnd = MyGetConsoleWindow();

    // get input/ouput handles
    con.input = GetStdHandle(STD_INPUT_HANDLE);
    assert(con.input != INVALID_HANDLE_VALUE);
    con.output = GetStdHandle(STD_OUTPUT_HANDLE);
    assert(con.output != INVALID_HANDLE_VALUE);

    // setup console window
    GetConsoleCursorInfo(con.output, &con.curinfo);
    GetConsoleMode(con.input, &flags);
    flags &= ~ENABLE_MOUSE_INPUT;
    SetConsoleMode(con.input, flags);

    rect.Top = rect.Left = 0; 
    rect.Right = CON_WIDTH - 1; 
    rect.Bottom = CON_HEIGHT - 1;

    coord.X = CON_WIDTH;
    coord.Y = CON_HEIGHT;

    SetConsoleWindowInfo(con.output, TRUE, &rect);
    SetConsoleScreenBufferSize(con.output, coord);

    // change window layout
    if(con.hwnd)
    while(1)
    {
        GetWindowRect(con.hwnd, &wndrect);
        if(wndrect.right >= GetSystemMetrics(SM_CXSCREEN)) break;
        SetWindowPos(
            con.hwnd, 
            HWND_TOP, 
            GetSystemMetrics(SM_CXSCREEN) - (wndrect.right - wndrect.left), 
            0, 0, 0, SWP_NOSIZE);
    }
    SetConsoleTitleA("Dolwin Debug Console");

    con.active = TRUE;
    con.update |= CON_UPDATE_ALL;

    DBReport("Debugger is running. Type help for quick reference.\n");

    con_refresh();
}

void con_close()
{
    if(!con.active) return;

    con.cmds.clear();

    // clear NOP history
    if(con.nopHist)
    {
        free(con.nopHist);
        con.nopHist = NULL;
        con.nopNum = 0;
    }

    // close console
    CloseHandle(con.input);
    CloseHandle(con.output);
    FreeConsole();

    // close log file
    if(con.logf)
    {
        fclose(con.logf);
        con.logf = NULL;
    }

    con.active = FALSE;
}

void con_start()
{
    uint32_t main = SYMAddress("main");
    if(main) con_set_disa_cur(main);
    else con_set_disa_cur(PC);

    con.update = CON_UPDATE_ALL;
    con_refresh();

    for(;;)
    {
        con_read_input(1);
        con_refresh();
        Sleep(10);
    }
}

void con_break(const char *reason)
{
    if(reason) con_print("\n" GREEN "debugger breaks%s. press F5 to continue.\n", reason);
    if(emu.running) emu.running = false;
    con_set_disa_cur(PC);
}

void con_command(std::vector<std::string>& args, int lnum)
{
    auto it = con.cmds.find(args[0]);

    if (it != con.cmds.end())
    {
        it->second(args);
    }
    else
    {
        if (lnum) con_print("unknown script command in line %i, see \'" GREEN "help" NORM "\'", lnum);
        else con_print("unknown command, try \'" GREEN "help" NORM "\'");
    }
}
