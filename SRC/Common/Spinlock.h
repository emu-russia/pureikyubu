// https://docs.microsoft.com/ru-ru/cpp/intrinsics/interlockedcompareexchange-intrinsic-functions?view=vs-2019

#pragma once

#include <intrin.h>
#include <Windows.h>

class SpinLock
{
    volatile LONG _lock = 0;

public:
    void Lock()
    {
        while (_InterlockedCompareExchange(&_lock,
            1, // exchange
            0)  // comparand
            == 1)
        {
            // spin!
        }
    }

    void Unlock() {
        _InterlockedExchange(&_lock, 0);
    }
};
