// console init code.
//
// recommended console font size is 6x9.
#include "dolphin.h"

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

    // set traps to CPU memory operations
    CPUReadByte    = DBReadByte;
    CPUWriteByte   = DBWriteByte;
    CPUReadHalf    = DBReadHalf;
    CPUReadHalfS   = DBReadHalfS;
    CPUWriteHalf   = DBWriteHalf;
    CPUReadWord    = DBReadWord;
    CPUWriteWord   = DBWriteWord;
    CPUReadDouble  = DBReadDouble;
    CPUWriteDouble = DBWriteDouble;

    // clear internal structures
    memset(&con, 0, sizeof(CONControl));
    memset(&roll, 0, sizeof(ROLLControl));
    memset(&wind, 0, sizeof(WINDControl));

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

    wind.disamode = DISAMOD_PPC;
    con.data = 0x80000000;
    con.text = PC;
    con_set_disa_cur(con.text);
    strcpy(con.logfile, CON_LOG_FILE);

    // create console 
    AllocConsole();
    con.hwnd = MyGetConsoleWindow();

    // get input/ouput handles
    con.input = GetStdHandle(STD_INPUT_HANDLE);
    ASSERT(con.input == INVALID_HANDLE_VALUE, "Cannot obtain input handler");
    con.output = GetStdHandle(STD_OUTPUT_HANDLE);
    ASSERT(con.output == INVALID_HANDLE_VALUE, "Cannot obtain output handler");

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
    SetConsoleTitle(APPNAME " Debug Console");
    SetAlwaysOnTop(con.hwnd, TRUE);

    con.active = TRUE;
    con.update |= CON_UPDATE_ALL;

    // show emulator version and copyright
    con_print(
        WHITE APPNAME " - " APPDESC "\n"
        WHITE "Build ver. " APPVER ", " __DATE__ ", " __TIME__ 
#ifdef  __MSVC__
        ", MSVC"
#endif
#ifdef  __VCNET__
        ", VCNET"
#endif
#ifdef  __MWERKS__
        ", CW"
#endif        
        "\n"
        WHITE "Copyright 2002-2004, " APPNAME " Team\n\n"
    );
}

void con_close()
{
    if(!con.active) return;

    // clear NOP history
    if(con.nopHist)
    {
        free(con.nopHist);
        con.nopHist = NULL;
        con.nopNum = 0;
    }

    // release temporary X86 text buffer
    if(wind.x86dasm)
    {
        free(wind.x86dasm);
        wind.x86dasm = NULL;
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

    // remove all breakpoints
    con_rem_all_bp();

    // restore CPU memory operations
    CPUReadByte    = MEMReadByte;
    CPUWriteByte   = MEMWriteByte;
    CPUReadHalf    = MEMReadHalf;
    CPUReadHalfS   = MEMReadHalfS;
    CPUWriteHalf   = MEMWriteHalf;
    CPUReadWord    = MEMReadWord;
    CPUWriteWord   = MEMWriteWord;
    CPUReadDouble  = MEMReadDouble;
    CPUWriteDouble = MEMWriteDouble;

    con.active = FALSE;
}

void con_start()
{
    u32 main = SYMAddress("main");
    if(main) con_set_disa_cur(main);
    else con_set_disa_cur(PC);

    setjmp(con.loop);
    con.update = CON_UPDATE_ALL;
    con_refresh();

    for(;;)
    {
        UpdateMainWindow(1);
        con_read_input(1);
        con_refresh();
    }
}

void con_break(char *reason)
{
    if(reason) con_print("\n" GREEN "debugger breaks%s. press F5 to continue.\n", reason);
    if(con.running) con.running = FALSE;
    con_set_disa_cur(PC);
    longjmp(con.loop, 0);
}
