// Dolphin OS - heap aka arena. Last modified: 27 Feb 2006.
#include "OSPrivate.h"

static void *__OSArenaLo = (void *)0xFFFFFFFF, *__OSArenaHi = NULL;

#define RoundUp(addr, align)    (((u32)(x) + ((u32)(align) - 1)) & ~((u32)(align) - 1))
#define RoundDown(addr, align)  (((u32)(x)) & ~((u32)(align) - 1))

void *OSGetArenaHi(void)
{
    return __OSArenaHi;
}

void *OSGetArenaLo(void)
{
    return __OSArenaLo;
}

void OSSetArenaHi(void *newHi)
{
    __OSArenaHi = newHi;
}

void OSSetArenaLo(void *newLo)
{
    __OSArenaLo = newLo;
}

void *OSAllocFromArenaLo(u32 size, u32 align)
{
    void *ptr;

    ptr = RoundUp(__OSArenaLo, align);
    __OSArenaLo = RoundUp(ptr + size, align);

    return ptr;
}

void *OSAllocFromArenaHi(u32 size, u32 align)
{
    void *ptr;

    ptr = RoundDown(__OSArenaHi, align) - size;
    __OSArenaHi = RoundDown(ptr);

    return __OSArenaHi;
}
