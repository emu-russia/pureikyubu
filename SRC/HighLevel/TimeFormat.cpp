#include "pch.h"

namespace HLE
{

    // Convert GC time to human-usable time string;
    // Example output: "30 Jun 2004 3:06:14:127"
    std::string OSTimeFormat(uint64_t tbr, bool noDate)
    {
        char timeStr[0x100];

        static const char* mnstr[12] = {
            "Jan", "Feb", "Mar", "Apr",
            "May", "Jun", "Jul", "Aug",
            "Sep", "Oct", "Nov", "Dec"
        };

        static const int dayMon[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        static const int dayMonLeap[] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        #define IsLeapYear(y)  ((y % 1000) == 0 || (y % 4) == 0)

        int64_t ticksPerMs = Core->OneSecond() / 1000;
        int64_t ms = tbr / ticksPerMs;
        int64_t msPerDay = 24 * 60 * 60 * 1000;
        int64_t days = ms / msPerDay;
        int64_t msDay = ms - days * msPerDay;

        // Hour, Minute, Second

        int h = (int)(msDay / (60 * 60 * 1000));
        msDay -= h * (60 * 60 * 1000);
        int m = (int)(msDay / (60 * 1000));
        msDay -= m * (60 * 1000);
        int s = (int)(msDay / 1000);
        msDay -= s * 1000;

        int year = 2000;
        int mon = 0;

        // Year, Month, Day

        while (days >= (IsLeapYear(year) ? 366 : 365))
        {
            days -= IsLeapYear(year) ? 366 : 365;
            year++;
        }

        while (days >= (IsLeapYear(year) ? dayMonLeap[mon] : dayMon[mon]))
        {
            days -= IsLeapYear(year) ? dayMonLeap[mon] : dayMon[mon];
            mon++;
        }

        int day = (int)(days + 1);

        if (noDate)
        {
            sprintf(timeStr, "%02i:%02i:%02i:%03i",
                h, m, s, (int)msDay);
        }
        else
        {
            sprintf(timeStr, "%i %s %i %02i:%02i:%02i:%03i",
                day, mnstr[mon], year,
                h, m, s, (int)msDay);
        }

        return timeStr;
    }

}
