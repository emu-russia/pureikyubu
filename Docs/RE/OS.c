// OS Init.

struct OSBootInfo_s
{
    DVDDiskID diskID;
    magic
    version
    memorySize;         // 0x28
    u32 consoleType;    // 0x2C
    arenaLo;            // 0x30
    arenaHi;            // 0x34
    FSTLocation;
    FSTMaxLength;    
} ;

OSBootInfo_s * BootInfo;

double ZeroF = 0.0;

BOOLEAN AreWeInitialized;

OSTime __OSStartTime;

u32 __OSExceptionLocations[] = {
    0x100,
    0x200,
    0x300,
    0x400,
    0x500,
    0x600,
    0x700,
    0x800,
    0x900,
    0xC00,
    0xD00,
    0xF00,
    0x1300,
    0x1400,
    0x1700,
};

char * __OSExceptionNames[] = {
    "System reset",
    "MachineCheck",
    "DSI",
    "ISI",
    "External Int.",
    "Alignment",
    "Program",
    "FP Unavailable",
    "Decrementer",
    "System call",
    "Trace",
    "Perf mon",
    "IABR",
    "SMI",
    "Thermal Int.",
    "Protection error",
};

__OSExceptionHandler * OSExceptionTable;

u32 * BI2DebugFlag;

u32 BI2DebugFlagHolder;

BOOLEAN __OSInIPL;

//
// Procedures
//

__OSIsDebuggerPresent ()
{
    u32 * flag = OSPhysicalToCached (0x40);
    return *flag;
}

__OSInitFPRs ()
{
    f0 = ZeroF;
    f1...f31 = f0;
}

DisableWriteGatherPipe ()
{
    PPCMthid2 (
        PPCMfhid2 () & ~HID2_WPE );
}

u32 OSGetConsoleType ()
{
    return (BootInfo->consoleType ) ? BootInfo->consoleType : OS_CONSOLE_ARTHUR;
}

ClearArena ()
{
    void * regionStart
    void * regionEnd;

    if ( OSGetResetCode() == OS_RESETCODE_RESTART )
    {
        regionStart = *(0x81300000 - 0x2010);
        regionEnd = *(0x81300000 - 0x2014);

        if ( regionStart )
        {
            ASSERT ( regionEnd != NULL );

            if ( OSGetArenaLo() < regionStart )
            {
                if ( OSGetArenaHi() > regionStart )
                {
                    //
                    // Clear excluding Reset region.
                    //

                    memset ( OSGetArenaLo(), 
                             0,
                             regionStart - OSGetArenaLo() );

                    memset ( regionEnd, 
                             0, 
                             OSGetArenaHi() - regionEnd );
                    return;
                }
            }
            else return;
        }
    }

    //
    // Clear All
    //

ClearAll:

    memset ( OSGetArenaLo (),
             0,
             OSGetArenaHi () - OSGetArenaLo() );
}

OSInit ()
{
    void * arenaLo;
    void * arenaHi;

    if ( AreWeInitialized )
        return;

    AreWeInitialized = TRUE;

    __OSStartTime = __OSGetSystemTime ();

    OSDisableInterrupts ();

    BootInfo = OSPhysicalToCached (0);

    BI2DebugFlag = NULL;

    __DVDLongFileNameFlag = 0;

    //
    // BI2DebugFlag & __PADSpec
    //

    if ( *dword.OSPhysicalToCached(0xF4) == 0 )
    {
        if ( BootInfo->arenaHi )
        {
            BI2DebugFlagHolder = *byte.OSPhysicalToCached(0x30E8);

            BI2DebugFlag = &BI2DebugFlagHolder;

            __PADSpec = *byte.OSPhysicalToCached (0x30E9);
        }
    }
    else
    {
        r29 = * dword.OSPhysicalToCached(0xF4);
        BI2DebugFlag = r29 + 0xC;
        __PADSpec = dword.[r29 + 0x24];

        *byte.OSPhysicalToCached(0x30E8) = (*BI2DebugFlag) & 0xFF;
        *byte.OSPhysicalToCached(0x30E8) = __PADSpec & 0xFF;
    }

    __DVDLongFileNameFlag = TRUE;

    //
    // Arena Lo
    //

    if ( BootInfo->arenaLo ) arenaLo = BootInfo->arenaLo;
    else arenaLo = __ArenaLo;

    OSSetArenaLo ( arenaLo );

    if ( BI2DebugFlag )         // Set Arena Lo at stack
    {
        if ( *BI2DebugFlag < 2 )
            OSSetArenaLo ( OSRoundUp32B(_stack_addr) );
    }

    //
    // Arena Hi
    //

    if ( BootInfo->arenaHi ) arenaHi = BootInfo->arenaHi;
    else arenaHi = __ArenaHi;

    OSSetArenaHi ( arenaHi );

    //
    // OS Subsystems
    //

    OSExceptionInit ();

    __OSInitSystemCall ();

    OSInitAlarm ();

    __OSModuleInit ();

    __OSInterruptInit ();

    __OSSetInterruptHandler ( __OS_INTERRUPT_PI_RSW,
                              __OSResetSWInterruptHandler );

    __OSContextInit ();

    __OSCacheInit ();

    EXIInit ();

    SIInit ();

    __OSInitSram ();

    __OSThreadInit ();

    __OSInitAudioSystem ();

    DisableWriteGatherPipe ();

    //
    // Console Type
    //

    ASSERT ( BootInfo );

    if ( BootInfo->consoleType >> 28 )
        BootInfo->consoleType = OS_CONSOLE_DEVHW1;
    else 
        BootInfo->consoleType = OS_CONSOLE_RETAIL1;

    BootInfo->consoleType += (dword.0xCC00302C >> 28);

    if ( __OSInIPL == 0 )
        __OSInitMemoryProtection ();

    //
    // Report OS Version
    //

    OSReport ( "\n" 
               "Dolphin OS $Revision: 49 $.\n" );
    OSReport ( "Kernel built : %s %s\n", __DATE__, __TIME__ );

    //
    // Report console type
    //

    OSReport ( "Console Type : " );

    consoleType = OSGetConsoleType ();

    if ( consoleType & OS_CONSOLE_DEVELOPMENT )
    {
        switch (consoleType)
        {
            case OS_CONSOLE_EMULATOR:
                OSReport ( "Mac Emulator\n" );
                break;

            case OS_CONSOLE_PC_EMULATOR:
                OSReport ( "PC Emulator\n" );
                break;

            case OS_CONSOLE_ARTHUR:
                OSReport ( "EPPC Arthur\n" );
                break;

            case OS_CONSOLE_MINNOW:
                OSReport ( "EPPC Minnow\n" );
                break;

            default:
                OSReport ( "Development HW%d\n", consoleType & ~OS_CONSOLE_DEVELOPMENT );
                break;
        }
    }
    else OSReport ( "Retail %d\n", consoleType );

    OSReport ( "Memory %d MB\n", BootInfo->memorySize / 1024*1024 );

    OSReport ( "Arena : 0x%x - 0x%x\n", OSGetArenaLo(), OSGetArenaHi() );
    
    if ( BI2DebugFlag )
    {
        if ( *BI2DebugFlag < 2 )
            EnableMetroTRKInterrupts ();
    }

    ClearArena ();

    OSEnableInterrupts ();
}

OSExceptionInit ()
{
    __OSException exception;
    void * destAddr;
    u32 * opCodeAddr;
    u32 oldOpCode;
    void * handlerStart;
    int handlerSize;
    u32 * ops;
    int cb;

    handlerSize = __OSEVEnd - __OSEVStart;

    ASSERTMSG ( handlerSize <= 0x100 , "OSExceptionInit(): too big exception vector code." );

    opCodeAddr = __OSEVSetNumber;

    oldOpCode = *opCodeAddr;

    destAddr = OSPhysicalToCached ( 0x60 );

    if ( *destAddr == 0 )
    {
        DBPrintf ( "Installing OSDBIntegrator\n" );

        memcpy ( destAddr, __OSDBINTSTART, __OSDBJUMPSTART - __OSDBINTSTART );

        DCFlushRangeNoSync ( destAddr, __OSDBJUMPSTART - __OSDBINTSTART );

        __sync;

        ICInvalidateRange ( destAddr, __OSDBJUMPSTART - __OSDBINTSTART );
    }

    //
    // Redirect to Debugger.
    //

    for ( exception=0; exception<__OS_EXCEPTION_MAX; exception++ )
    {
        if ( BI2DebugFlag )
        {
            if ( ! ( *BI2DebugFlag < 2 && !__DBIsExceptionMarked(exception) ) )
            {
                DBPrintf ( ">>> OSINIT: exception %d commandeered by TRK\n", exception );
                continue;
            }
        }

        *opCodeAddr = oldOpCode | exception;

        if ( !__DBIsExceptionMarked (exception) )
        {
            cb = 0;
            ops = __DBVECTOR;

            while ( cb < (__OSDBJUMPEND - __OSDBJUMPSTART) )
            {
                *ops = 0x60000000;   // nop
                ops++;
                cb += 4;
            }
        }
        else
        {
            DBPrintf ( ">>> OSINIT: exception %d vectored to debugger\n", exception );

            memcpy ( __DBVECTOR, 
                     __OSDBJUMPSTART,
                     __OSDBJUMPEND - __OSDBJUMPSTART );
        }

        handlerStart = OSPhysicalToCached ( __OSExceptionLocations[exception] );

        memcpy ( handlerStart, __OSEVStart, handlerSize );

        DCFlushRangeNoSync ( handlerStart, handlerSize );

        __sync;

        ICInvalidateRange ( handlerStart, handlerSize );
    }    

    //
    // Install default exception handlers.
    //

    OSExceptionTable = OSPhysicalToCached ( 0x3000 );

    for ( exception=0; exception<0xF; exception++ )
    {
        __OSSetExceptionHandler ( exception, OSDefaultExceptionHandler );
    }

    *opCodeAddr = oldOpCode;

    DBPrintf ( "Exceptions initialized...\n" );
}

__OSDBINTSTART:
    li        r5, 0x40
    mflr      r3
    stw       r3, 0xC(r5)
    lwz       r3, 8(r5)
    oris      r3, r3, 0x8000
    mtlr      r3
    li        r3, 0x30 # '0'
    mtmsr     r3
    blr
__OSDBINTEND:

__OSDBJUMPSTART:
__OSDBJump:
    bla       0x60
__OSDBJUMPEND:

//
// Updates OS exception table, NOT first-level exception vector.
//

__OSExceptionHandler __OSSetExceptionHandler(
    __OSException           exception,
    __OSExceptionHandler    handler )
{
    __OSExceptionHandler oldHandler;

    ASSERTMSG ( exception < __OS_EXCEPTION_MAX, "__OSSetExceptionHandler(): unknown exception." );

    oldHandler = OSExceptionTable[exception];

    OSExceptionTable[exception] = handler;

    return oldHandler;
}

__OSExceptionHandler __OSGetExceptionHandler(
    __OSException           exception
)
{
    ASSERTMSG ( exception < __OS_EXCEPTION_MAX, "__OSGetExceptionHandler(): unknown exception." );

    return OSExceptionTable[exception];
}

__OSEVStart:
OSExceptionVector:
    mtsprg0   r4
    lwz       r4, loc_C0
    stw       r3, 0xC(r4)
    mfsprg0   r3
    stw       r3, 0x10(r4)
    stw       r5, 0x14(r4)
    lhz       r3, 0x1A2(r4)
    ori       r3, r3, 2
    sth       r3, 0x1A2(r4)
    mfcr      r3
    stw       r3, 0x80(r4)
    mflr      r3
    stw       r3, 0x84(r4)
    mfctr     r3
    stw       r3, 0x88(r4)
    mfxer     r3
    stw       r3, 0x8C(r4)
    mfsrr0    r3
    stw       r3, 0x198(r4)
    mfsrr1    r3
    stw       r3, 0x19C(r4)
    mr        r5, r3

__DBVECTOR:
    nop
    mfmsr     r3
    ori       r3, r3, 0x30
    mtsrr1    r3

__OSEVSetNumber:
    li        r3, 0
    lwz       r4, loc_D4
    rlwinm.   r5, r5, 0,30,30
    bne       loc_A44
    lis       r5, OSDefaultExceptionHandler@ha
    addi      r5, r5, OSDefaultExceptionHandler@l
    mtsrr0    r5
    rfi
loc_A44:
    clrlslwi  r5, r3, 24,2
    lwz       r5, 0x3000(r5)
    mtsrr0    r5
    rfi

__OSEVEnd:
    nop


OSDefaultExceptionHandler:
    stw       r0, 0(r4)
    stw       r1, 4(r4)
    stw       r2, 8(r4)
    stmw      r6, 0x18(r4)
    mfspr     r0, 0x391     # GQR1
    stw       r0, 0x1A8(r4)
    mfspr     r0, 0x392
    stw       r0, 0x1AC(r4)
    mfspr     r0, 0x393
    stw       r0, 0x1B0(r4)
    mfspr     r0, 0x394
    stw       r0, 0x1B4(r4)
    mfspr     r0, 0x395
    stw       r0, 0x1B8(r4)
    mfspr     r0, 0x396
    stw       r0, 0x1BC(r4)
    mfspr     r0, 0x397     # GQR7
    stw       r0, 0x1C0(r4)
    mfdsisr   r5
    mfdar     r6
    b         __OSUnhandledException

__OSPSInit ()
{
    PPCMthid2 (
        PPCMfhid2 () | 0x80000000 | 0x20000000 );

    ICFlashInvalidate ();

    __sync;

    mtspr 0x390, 0      // GQR0 = 0
}

u32 __OSGetDIConfig ()
{
    return dword.0xCC006024 & 0xFF;
}
