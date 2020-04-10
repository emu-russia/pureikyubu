
#pragma once

#include <intrin.h>
#include <Windows.h>

class SpinLock
{
    volatile LONG _lock = 0;

public:
    void Lock();
    void Unlock();
};
