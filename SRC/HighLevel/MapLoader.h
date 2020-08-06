#pragma once

enum class MAP_FORMAT : int
{
    BAD = 0,
    RAW,             // MAP format, invented by org
    CW,              // CodeWarrior
    GCC,             // GCC
};

MAP_FORMAT LoadMAP(const wchar_t *mapname, bool add=false);
MAP_FORMAT LoadMAP(const char* mapname, bool add = false);
