// Dolphin OS application entrypoint.

typedef struct _ROM_COPY_INFO
{
    void   *rom;
    void   *addr;
    size_t size;
} ROM_COPY_INFO, *PROM_COPY_INFO;

typedef struct _BSS_INIT_INFO
{
    void * addr;
    size_t size;
} BSS_INIT_INFO, *PBSS_INIT_INFO;

__check_pad3 ()
{
    r0 = word.0x800030E4 & 0xEEF;

    if ( r0 == 0xEEF)
        OSResetSystem ( OS_RESET_RESTART, 0, FALSE );
}

__start ()
{
    int argc,
    char **argv;

    __init_registers ();

    __init_hardware ();

    __init_data ();

    //
    // Metrowerk TRK
    //

    dword.0x80000044 = 0;

    r6 = dword.0x800000F4;

    if ( r6 )
    {
        if ( dword.r6[0xC] == 2 )
            InitMetroTRK ( r5 = 0 );
        else if ( dword.r6[0xC] == 3 )
            InitMetroTRK ( r5 = 1 );
    }

    //
    // Command Line
    //

    r5 = dword.0x800000F4;

    if ( r5 && dword.r5[8] && (argc = *(r5 + dword.r5[8])) )
    {
        argv = (r5 + dword.r5[8] + 4);

        r6 = r5 + dword.r5[8] + 4;

        for ( i=0; i<argc; i++)
        {
            dword.r6[i] += r5;
        }

        dword.0x80000034 = argv & ~0x1F;
    }
    else
    {
        argc = 0;
        argv = NULL;
    }

    //
    // OS Stuff
    //

    DBInit ();

    OSInit ();

    r3 = word.0x800030E6;

    if ( !(r3 & 0x8000) || (r3 & 0x7FFF) == 1 )
        __check_pad3 ();

    __init_user ();

    main ( argc, argv );

    exit ();
}

__copy_rom_section(void *dst, void *src, size_t n)
{
    if ( n && dst != src )
    {
        memcpy ( dst, src, n );
        __flush_cache ( dst, n );
    }
}

__init_bss_section (void *dst, size_t size)
{
    if (size)
        memset ( dst, 0, size );
}

__init_registers ()
{
    r1 = _stack_addr;
    r2 = _SDA2_BASE_;
    r13 = _SDA_BASE_;
}

__init_data ()
{
    PROM_COPY_INFO RomCopy;
    PBSS_INIT_INFO BssInit;

    RomCopy = _rom_copy_info;

    while ( RomCopy->size )
    {
        __copy_rom_section ( RomCopy->addr, RomCopy->rom, RomCopy->size );
        RomCopy++;
    }

    BssInit = _bss_init_info;

    while ( BssInit->size )
    {
        __init_bss_section ( BssInit->addr, BssInit->size );
        BssInit++;
    }
}