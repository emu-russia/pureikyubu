// Dolwin debugger interface;
#include "pch.h"

// message output
static void dummy(const char *text, ...) {}
static void dummy2(DbgChannel chan, const char* text, ...) {}
void (*DBHalt)(const char *text, ...)   = dummy;
void (*DBReport)(const char* text, ...) = dummy;
void (*DBReport2)(DbgChannel chan, const char *text, ...) = dummy2;

static HANDLE consoleThreadHandle = INVALID_HANDLE_VALUE;
static DWORD consoleThreadId;

static void db_report2(DbgChannel chan, const char* text, ...)
{
    ConColor col = ConColor::NORM;
    char    buf[0x1000];
    va_list arg;
    const char* prefix = "";

    if (chan == DbgChannel::Void)
        return;

    switch (chan)
    {
        case DbgChannel::Norm:
            col = ConColor::NORM;
            break;
        case DbgChannel::Info:
            col = ConColor::GREEN;
            break;
        case DbgChannel::Error:
            col = ConColor::BRED;
            break;
        case DbgChannel::Header:
            col = ConColor::CYAN;
            break;

        case DbgChannel::CP:
            prefix = "CP : ";
            col = ConColor::CYAN;
            break;
        case DbgChannel::PE:
            prefix = "PE : ";
            col = ConColor::CYAN;
            break;
        case DbgChannel::VI:
            prefix = "VI : ";
            col = ConColor::CYAN;
            break;
        case DbgChannel::GP:
            prefix = "GP : ";
            col = ConColor::CYAN;
            break;
        case DbgChannel::PI:
            prefix = "PI : ";
            col = ConColor::CYAN;
            break;
        case DbgChannel::CPU:
            prefix = "CPU: ";
            col = ConColor::CYAN;
            break;
        case DbgChannel::MI:
            prefix = "MI : ";
            col = ConColor::CYAN;
            break;
        case DbgChannel::DSP:
            prefix = "DSP: ";
            col = ConColor::BPUR;
            break;
        case DbgChannel::DI:
            prefix = "DI : ";
            col = ConColor::CYAN;
            break;
        case DbgChannel::AR:
            prefix = "AR : ";
            col = ConColor::CYAN;
            break;
        case DbgChannel::AI:
            prefix = "AI : ";
            col = ConColor::CYAN;
            break;
        case DbgChannel::AIS:
            prefix = "AIS: ";
            col = ConColor::CYAN;
            break;
        case DbgChannel::SI:
            prefix = "SI : ";
            col = ConColor::CYAN;
            break;
        case DbgChannel::EXI:
            prefix = "EXI: ";
            col = ConColor::CYAN;
            break;
        case DbgChannel::MC:
            prefix = "MC : ";
            col = ConColor::CYAN;
            break;
        case DbgChannel::DVD:
            prefix = "DVD: ";
            col = ConColor::BGREEN;
            break;

        case DbgChannel::Loader:
            col = ConColor::YEL;
            break;
        case DbgChannel::HLE:
            col = ConColor::GREEN;
            break;
    }

    va_start(arg, text);
    vsprintf_s(buf, sizeof(buf) - 1, text, arg);
    va_end(arg);

    con_print(col, "%s%s", prefix, buf);
}

static void db_report(const char* text, ...)
{
    char    buf[0x1000];
    va_list arg;

    va_start(arg, text);
    vsprintf_s(buf, sizeof(buf) - 1, text, arg);
    va_end(arg);

    db_report2(DbgChannel::Norm, buf);
}

static DWORD WINAPI DBThreadProc(LPVOID lpParameter)
{
    DBHalt = con_error;
    DBReport = db_report;
    DBReport2 = db_report2;
    con_open();

    con.update |= CON_UPDATE_ALL;
    con_refresh(1);
    
    con_start();

    return 0;
}

// Open debugger window
void DBOpen()
{
    // start debugger thread
    consoleThreadHandle = CreateThread(NULL, 0, DBThreadProc, &con, 0, &consoleThreadId);
    assert(consoleThreadHandle != INVALID_HANDLE_VALUE);
}

// Close debugger window
void DBClose()
{
    con.exitPending = true;
}
