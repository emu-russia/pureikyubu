// Dolwin debugger interface;
#include "pch.h"

static HANDLE consoleThreadHandle = INVALID_HANDLE_VALUE;
static DWORD consoleThreadId;

static void db_report2(Debug::Channel chan, const char* text, ...)
{
    ConColor col = ConColor::NORM;
    char    buf[0x1000];
    va_list arg;
    const char* prefix = "";

    if (chan == Debug::Channel::Void)
        return;

    switch (chan)
    {
        case Debug::Channel::Norm:
            col = ConColor::NORM;
            break;
        case Debug::Channel::Info:
            col = ConColor::GREEN;
            break;
        case Debug::Channel::Error:
            col = ConColor::BRED;
            break;
        case Debug::Channel::Header:
            col = ConColor::CYAN;
            break;

        case Debug::Channel::CP:
            prefix = "CP : ";
            col = ConColor::CYAN;
            break;
        case Debug::Channel::PE:
            prefix = "PE : ";
            col = ConColor::CYAN;
            break;
        case Debug::Channel::VI:
            prefix = "VI : ";
            col = ConColor::CYAN;
            break;
        case Debug::Channel::GP:
            prefix = "GP : ";
            col = ConColor::CYAN;
            break;
        case Debug::Channel::PI:
            prefix = "PI : ";
            col = ConColor::CYAN;
            break;
        case Debug::Channel::CPU:
            prefix = "CPU: ";
            col = ConColor::CYAN;
            break;
        case Debug::Channel::MI:
            prefix = "MI : ";
            col = ConColor::CYAN;
            break;
        case Debug::Channel::DSP:
            prefix = "DSP: ";
            col = ConColor::BPUR;
            break;
        case Debug::Channel::DI:
            prefix = "DI : ";
            col = ConColor::CYAN;
            break;
        case Debug::Channel::AR:
            prefix = "AR : ";
            col = ConColor::CYAN;
            break;
        case Debug::Channel::AI:
            prefix = "AI : ";
            col = ConColor::CYAN;
            break;
        case Debug::Channel::AIS:
            prefix = "AIS: ";
            col = ConColor::CYAN;
            break;
        case Debug::Channel::SI:
            prefix = "SI : ";
            col = ConColor::CYAN;
            break;
        case Debug::Channel::EXI:
            prefix = "EXI: ";
            col = ConColor::CYAN;
            break;
        case Debug::Channel::MC:
            prefix = "MC : ";
            col = ConColor::CYAN;
            break;
        case Debug::Channel::DVD:
            prefix = "DVD: ";
            col = ConColor::BGREEN;
            break;
        case Debug::Channel::AX:
            prefix = "AX: ";
            col = ConColor::BGREEN;
            break;

        case Debug::Channel::Loader:
            col = ConColor::YEL;
            break;
        case Debug::Channel::HLE:
            col = ConColor::GREEN;
            break;
    }

    va_start(arg, text);
    int size = vsprintf_s(buf, sizeof(buf) - 1, text, arg);
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

    db_report2(Debug::Channel::Norm, buf);
}

static DWORD WINAPI DBThreadProc(LPVOID lpParameter)
{
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

    JDI::Hub.AddNode(DEBUGGER_JDI_JSON, Debug::Reflector);
}

// Close debugger window
void DBClose()
{
    con.exitPending = true;

    JDI::Hub.RemoveNode(DEBUGGER_JDI_JSON);
}
