// Dolphin SDK PowerPC EABI Init.

__init_user ()
{
    __init_cpp ();
}

__init_cpp ()
{
    while ( *_ctors )
    {
        *_ctors ();
        _ctors++;
    }
}

__fini_cpp ()
{
    while ( *_dtors )
    {
        *_dtors ();
        _dtors++;
    }
}

abort ()
{
    _ExitProcess ();
}

exit ()
{
    __fini_cpp ();

    _ExitProcess ();
}

_ExitProcess ()
{
    PPCHalt ();
}

__init_hardware ()
{
    MSR |= MSR_FP;  // floating point available

    __OSPSInit ();
    
    __OSCacheInit ();
}

__flush_cache (void *address, size_t size)
{
    r5 = r3 & 0xFFFFFFF1;
    r3 = r3 - r5;
    r4 = r4 + r3;

loc_150:
    dcbst     r0, r5
    sync
    icbi      r0, r5

    r5 += 8;
    r4 -= 8;
    if ( r4 >= 0) goto loc_150;

    isync;
}
