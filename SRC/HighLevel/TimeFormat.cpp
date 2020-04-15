#include "pch.h"

namespace HLE
{

    // Convert GC time to human-usable time string;
    // example output : "30 Jun 2004 3:06:14:127"
    void OSTimeFormat(TCHAR gcTime[256], uint64_t tbr, bool noDate)
    {
        // FILETIME - number of 1/10000000 intervals, since Jan 1 1601
        // GC time  - number of 1/40500000 sec intervals, since Jan 1 2000
        // To convert GCTIME -> FILETIME :
        //      1: adjust GCTIME by number of 1/10000000 intervals
        //         between Jan 1 1601 and Jan 1 2000.
        //      2: assume X - 1/10000000 sec, Y - 1/40500000 sec,
        //         FILETIME = (GCTIME * Y) / X

        // coversion GCTIME -> FILETIME
        #define MAGIK 0x0713AD7857941000
        double x = 1.0 / 10000000.0, y = 1.0 / 40500000.0;
        tbr += MAGIK;
        uint64_t ft = (uint64_t)(((double)(int64_t)tbr * y) / x);
        FILETIME fileTime; SYSTEMTIME sysTime;
        fileTime.dwHighDateTime = (uint32_t)(ft >> 32);
        fileTime.dwLowDateTime = (uint32_t)(ft & 0x00000000ffffffff);
        FileTimeToSystemTime(&fileTime, &sysTime);

        // format string
        static const TCHAR* mnstr[12] =
        { _T("Jan"), _T("Feb"), _T("Mar"), _T("Apr"),
          _T("May"), _T("Jun"), _T("Jul"), _T("Aug"),
          _T("Sep"), _T("Oct"), _T("Nov"), _T("Dec")
        };
        if (noDate)
        {
            _stprintf_s(gcTime, 255, _T("%02i:%02i:%02i:%03i"),
                sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds);
        }
        else
        {
            _stprintf_s(gcTime, 255, _T("%i %s %i %02i:%02i:%02i:%03i"),
                sysTime.wDay, mnstr[sysTime.wMonth - 1], sysTime.wYear,
                sysTime.wHour, sysTime.wMinute, sysTime.wSecond, sysTime.wMilliseconds);
        }
    }

}
