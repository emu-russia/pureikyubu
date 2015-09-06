// Gamecube DVD drive low-level commands. Version 0.02
// Reversed by org. Compile with CodeWarrior.
// $root$/build/libraries/dvd/src/DVDLow.c
#include <dolphin.h>
#include <dvd/DVDLow.h>

// DVD Interface register index.
//
#define DI_SR               0       // Status Register
#define DI_CVR              1       // Cover Register
#define DI_CMDBUF0          2       // Command Buffer 0
#define DI_CMDBUF1          3       // Command Buffer 1
#define DI_CMDBUF2          4       // Command Buffer 2
#define DI_MAR              5       // DMA Memory Address Register
#define DI_LEN              6       // DMA Transfer Length Register
#define DI_CR               7       // Control Register
#define DI_IMMBUF           8       // Immediate Data Buffer
#define DI_CFG              9       // Configuration Register
#define DI_MAX              10

// DI Status Register mask
#define DI_SR_BRKINT     (1 << 6)
#define DI_SR_BRKINTMSK  (1 << 5)
#define DI_SR_TCINT      (1 << 4)
#define DI_SR_TCINTMSK   (1 << 3)
#define DI_SR_DEINT      (1 << 2)
#define DI_SR_DEINTMSK   (1 << 1)
#define DI_SR_BRK        (1 << 0)

// DI Cover Register mask
#define DI_CVR_CVRINT    (1 << 2)
#define DI_CVR_CVRINTMSK (1 << 1)
#define DI_CVR_CVR       (1 << 0)

// DI Control Register mask
#define DI_CR_RW         (1 << 2)
#define DI_CR_DMA        (1 << 1)
#define DI_CR_TSTART     (1 << 0)

// DVD "workaround" is Nintendo hack for missing DVD reads in old revisions.

#define DVD_WATYPE_JUSTREAD     0
#define DVD_WATYPE_CACHE        1   // Fixes DVD cache read issues.
#define DVD_WATYPE_MAX          2

// Prev and Curr index names.
#define WA_ADDR     0
#define WA_LEN      1
#define WA_OFFS     2

// Internal variables.

static vu32             dvdReg[DI_MAX] AT_ADDRESS (OS_BASE_UNCACHED | 0x6000);  // DVD registers
static vu32             resetReg AT_ADDRESS (OS_BASE_UNCACHED | 0x3024);
static DVDLowCallback   Callback;   // low-level operation callback
static DVDLowCallback   ResetCoverCallback;
static OSTime           LastResetEnd;   // last reset completed checkout
static BOOL             ResetOccurred;
static int              WorkAroundType;
static s32              WorkAroundSeekLocation;
static BOOL             StopAtNextInt;
static BOOL             Breaking;   // Break request flag
static OSAlarm          AlarmForWA;
static OSAlarm          AlarmForTimeout;
static OSAlarm          AlarmForBreak;
static s32              LastLength; // last read length
static BOOL             WaitingCoverClose;
static BOOL             LastCommandWasRead;
static BOOL             FirstRead = TRUE;
static OSTime           LastReadIssued, LastReadFinished;
static u32              Prev[3], Curr[3]; // used for read operation. 0:Address, 1:Length, 2:Offset

// Seek/read command queue for cache issue workaround.
//

#define DVDLOW_CMD_READ     1
#define DVDLOW_CMD_SEEK     2
#define DVDLOW_CMD_END      -1  // terminate queue execution.

static struct
{
    u32             command;
    void*           address;
    u32             length;
    u32             offset;
    DVDLowCallback  callback;
} CommandList[3];
static  u32 NextCommandNumber;

// Internal use.

void    __DVDInitWA (void);
BOOL    ProcessNextCommand (void);
void    __DVDInterruptHandler (__OSInterrupt interrupt, OSContext *context);
void    AlarmHandler (OSAlarm *alarm, OSContext *context);
void    AlarmHandlerForTimeout(OSAlarm *alarm, OSContext *context);
void    SetTimeoutAlarm (OSTime timeout);
void    Read (void *addr, s32 length, s32 offset, DVDLowCallback callback);
BOOL    AudioBufferOn (void);
BOOL    HitCache (u32 *curr, u32 *prev);
void    DoJustRead (void *addr, s32 length, s32 offset, DVDLowCallback callback);
void    SeekTwiceBeforeRead (void *addr, s32 length, s32 offset, DVDLowCallback callback);
void    WaitBeforeRead (void *addr, s32 length, s32 offset, DVDLowCallback callback, OSTime wait);
void    DoBreak (void);
void    AlarmHandlerForBreak (OSAlarm *alarm, OSContext *context);
void    SetBreakAlarm (OSTime timeout);
void    __DVDLowSetWAType (int type, s32 seekLoc);

// Private OS declarations. Should be in <os/OSPrivate.h> header actually.
OSTime  __OSGetSystemTime (void);

// ---------------------------------------------------------------------------------

void __DVDInitWA (void)
{
    NextCommandNumber = 0;
    CommandList[0].command = DVDLOW_CMD_END;
    __DVDLowSetWAType (DVD_WATYPE_JUSTREAD, 0);
    OSInitAlarm ();
}

// ---------------------------------------------------------------------------------
// Process next command from queue.
// Return true if there are still some commands in queue.
// Return false if queue terminated.

BOOL ProcessNextCommand (void)
{
    int n = NextCommandNumber;

    ASSERT (n >= 3);

    switch (CommandList[n].command)
    {
        case DVDLOW_CMD_READ:
            NextCommandNumber++;
            Read( CommandList[n].address,
                  CommandList[n].length,
                  CommandList[n].offset,
                  CommandList[n].callback );
            return TRUE;
    
        case DVDLOW_CMD_SEEK:
            NextCommandNumber++;
            DVDLowSeek( CommandList[n].offset,
                        CommandList[n].callback );
            return TRUE;
    }

    return FALSE;   // Queue terminated.
}

// ---------------------------------------------------------------------------------
// Handler for DVD interrupt.

void __DVDInterruptHandler (__OSInterrupt interrupt, OSContext *context)
{
    #pragma unused(interrupt)

    OSContext           exceptionContext;
    DVDLowCallback      cb;
    u32                 reg, cause, mask, intr;

    cause = 0;       // Clear callback source.

    OSCancelAlarm (&AlarmForTimeout);

    if (LastCommandWasRead)
    {
        LastReadFinished = __OSGetSystemTime ();
        FirstRead = FALSE;

        Prev[WA_ADDR] = Curr[WA_ADDR];  // replace previous read parameters
        Prev[WA_LEN]  = Curr[WA_LEN];
        Prev[WA_OFFS] = Curr[WA_OFFS];

        if (StopAtNextInt) cause |= DVDLOW_CAUSE_BREAK;
    }

    LastCommandWasRead = FALSE;
    StopAtNextInt = FALSE;

    // Check regular interrupts cause.

    reg  = dvdReg[DI_SR];
    mask = reg & (DI_SR_BRKINTMSK | DI_SR_TCINTMSK | DI_SR_DEINTMSK);
    intr = reg & (DI_SR_BRKINT | DI_SR_TCINT | DI_SR_DEINT);
    intr &= (mask << 1);

    if (intr & DI_SR_BRKINT) cause |= DVDLOW_CAUSE_BREAK;
    if (intr & DI_SR_TCINT) cause |= DVDLOW_CAUSE_TRANSFER;
    if (intr & DI_SR_DEINT) cause |= DVDLOW_CAUSE_ERROR;

    if (cause) ResetOccurred = FALSE;   // DVD reset can be detected only this way :(

    dvdReg[DI_SR] = intr | mask;    // clear pending interrupts

    if ( ResetOccurred &&
        ((__OSGetSystemTime() - LastResetEnd) < OSTicksToMilliseconds(200)) )
    {
        // Check cover interrupt cause.
        reg  = dvdReg[DI_CVR];
        mask = reg & DI_CVR_CVRINTMSK;
        intr = reg & DI_CVR_CVRINT;
        intr &= (mask << 1);

        if (intr)
        {
            if (ResetCoverCallback) ResetCoverCallback (DVDLOW_CAUSE_RESET_COVER);
            ResetCoverCallback = NULL;
        }

        dvdReg[DI_CVR] = dvdReg[DI_CVR];    // Clear pending cover interrupt
    }
    else
    {
        if (WaitingCoverClose)
        {
            reg  = dvdReg[DI_CVR];
            mask = reg & DI_CVR_CVRINTMSK;
            intr = reg & DI_CVR_CVRINT;
            intr &= (mask << 1);

            if (intr) cause |= DVDLOW_CAUSE_RESET_COVER;
            dvdReg[DI_CVR] = intr | mask;
            WaitingCoverClose = FALSE;
        }
        else dvdReg[DI_CVR] = 0;
    }

    if ( Breaking && (cause & DVDLOW_CAUSE_BREAK) )
    {
        cause &= ~DVDLOW_CAUSE_BREAK;
    }

    if (cause & DVDLOW_CAUSE_TRANSFER)
    {
        if (ProcessNextCommand()) return;
    }
    else
    {
        CommandList[0].command = DVDLOW_CMD_END;
        NextCommandNumber = 0;
    }

    OSClearContext (&exceptionContext);
    OSSetCurrentContext (&exceptionContext);

    if (cause)
    {
        cb = Callback;
        Callback = NULL;
        if (cb) cb (cause);     // Execute operation callback.
        Breaking = FALSE;
    }

    OSClearContext (&exceptionContext);
    OSSetCurrentContext (context);
}

// ---------------------------------------------------------------------------------

void AlarmHandler (OSAlarm *alarm, OSContext *context)
{
    #pragma unused(alarm)
    #pragma unused(context)

    BOOL processed = ProcessNextCommand ();
    ASSERT (processed);
}

// ---------------------------------------------------------------------------------

void AlarmHandlerForTimeout (OSAlarm *alarm, OSContext *context)
{
    #pragma unused(alarm)

    OSContext exceptionContext; // alarm-runtime context
    DVDLowCallback cb;

    __OSMaskInterrupts (OS_INTERRUPTMASK_PI_DI);

    OSClearContext (&exceptionContext);
    OSSetCurrentContext (&exceptionContext);

    // Execute (and clear) low-level operation callback.
    cb = Callback;
    Callback = NULL;
    if (cb) cb (DVDLOW_CAUSE_TIMEOUT);

    OSClearContext (&exceptionContext);
    OSSetCurrentContext (context);
}

// ---------------------------------------------------------------------------------
// Set timeout for low-level operations.

void SetTimeoutAlarm (OSTime timeout)
{
    OSCreateAlarm (&AlarmForTimeout);
    OSSetAlarm (&AlarmForTimeout, timeout, AlarmHandlerForTimeout);
}

// ---------------------------------------------------------------------------------
// Send actual DVD read command.

void Read (void *addr, s32 length, s32 offset, DVDLowCallback callback)
{
    Callback = callback;
    StopAtNextInt = FALSE;
    LastCommandWasRead = TRUE;
    LastReadIssued = __OSGetSystemTime ();

    dvdReg[DI_CMDBUF0] = 0xA8000000 | 0x00;     // subcmd = 0x00
    dvdReg[DI_CMDBUF1] = offset >> 2;
    dvdReg[DI_CMDBUF2] = length;
    dvdReg[DI_MAR] = (u32)addr;
    dvdReg[DI_LEN] = length;
    LastLength = length;
    dvdReg[DI_CR] = DI_CR_DMA | DI_CR_TSTART;

    if (length > 10*1024*1024) SetTimeoutAlarm (OSSecondsToTicks(20));
    else SetTimeoutAlarm (OSSecondsToTicks(10));
}

// ---------------------------------------------------------------------------------
// Return whenever DVD streaming buffer is active or not.

BOOL AudioBufferOn (void)
{
    DVDDiskID *id = DVDGetCurrentDiskID ();
    return id->streaming ? TRUE : FALSE;
}

// ---------------------------------------------------------------------------------

BOOL HitCache (u32 *curr, u32 *prev)
{
    int blockNumOfPrevEnd, blockNumOfCurrStart, cacheBlockSize;

    blockNumOfPrevEnd = (prev[WA_OFFS] + prev[WA_LEN] - 1) / 32*1024;
    blockNumOfCurrStart = curr[WA_OFFS] / 32*1024;

    cacheBlockSize = AudioBufferOn() ? 5 : 15;

    if (blockNumOfCurrStart > blockNumOfPrevEnd - 2) return TRUE;

    if (blockNumOfCurrStart >= blockNumOfPrevEnd + cacheBlockSize + 3) return FALSE;
    else return TRUE;
}

// ---------------------------------------------------------------------------------

void DoJustRead (void *addr, s32 length, s32 offset, DVDLowCallback callback)
{
    CommandList[0].command = DVDLOW_CMD_END;
    NextCommandNumber = 0;
    Read (addr, length, offset, callback);
}

// ---------------------------------------------------------------------------------

void SeekTwiceBeforeRead (void *addr, s32 length, s32 offset, DVDLowCallback callback)
{
    s32 offsetToSeek;

    if (offset & ~0x7FFF)
    {
        offsetToSeek = (offset & ~0x7FFF) + WorkAroundSeekLocation;
    }
    else offsetToSeek = 0;

    CommandList[0].command = DVDLOW_CMD_SEEK;
    CommandList[0].offset  = offsetToSeek;
    CommandList[0].callback= callback;

    CommandList[1].command = DVDLOW_CMD_READ;
    CommandList[1].address = addr;
    CommandList[1].length  = length;
    CommandList[1].offset  = offset;
    CommandList[1].callback= callback;    

    CommandList[2].command = DVDLOW_CMD_END;    // terminate.

    NextCommandNumber = 0;

    DVDLowSeek (offsetToSeek, callback);
}

// ---------------------------------------------------------------------------------

void WaitBeforeRead (void *addr, s32 length, s32 offset, DVDLowCallback callback, OSTime wait)
{
    CommandList[0].command = DVDLOW_CMD_READ;
    CommandList[0].address = addr;
    CommandList[0].length  = length;
    CommandList[0].offset  = offset;
    CommandList[0].callback= callback;

    CommandList[1].command = DVDLOW_CMD_END;    // terminate.

    NextCommandNumber = 0;

    OSCreateAlarm (&AlarmForWA);
    OSSetAlarm (&AlarmForWA, wait, AlarmHandler);
}

// ---------------------------------------------------------------------------------
// Low-level DVD sector reading, using cache issue workaround (if enabled).
// This one was tough in reversing, but I like challenge.

/* 
 * Complete DVD cache issue workaround description (seems to be fixed in production boards):
 * Seek twice before very first read. Else:
 * 1. If reading doesnt overlap previous one in 32 KB cache, seek twice and read.
 * 2. If reading do overlap, but previous reading is completed less than 5 ms ago, do just read.
 * 3. If reading do overlap, and previous reading is completed more than 5 ms ago, wait 5ms+500us and read.
 * If reading doesnt hit the cache, do just read.
*/

BOOL DVDLowRead (void *addr, s32 length, s32 offset, DVDLowCallback callback)
{
    int blockNumOfPrevEnd, blockNumOfCurrStart;

    // Sanity checks.

    if ((u32)addr & 0x1f)
        OSHalt ("DVDLowRead(): address must be aligned with 32 byte boundary.");

    if (length & 0x1f)
        OSHalt ("DVDLowRead(): length must be a multiple of 32.");

    if (offset & 3)
        OSHalt ("DVDLowRead(): offset must be a multiple of 4.");

    if (length == 0)
        OSHalt ("DVD read: 0 was specified to length of the read\n");

    dvdReg[DI_LEN] = length;

    Curr[WA_ADDR] = (u32)addr;      // Save current read parameters.
    Curr[WA_LEN]  = length;
    Curr[WA_OFFS] = offset;

    switch (WorkAroundType)
    {
        case DVD_WATYPE_JUSTREAD:
            DoJustRead (addr, length, offset, callback);
            break;

        case DVD_WATYPE_CACHE:  // DVD-drive has 32K read-ahead(?) cache space inside.
            if (FirstRead)
            {
                SeekTwiceBeforeRead (addr, length, offset, callback);
            }
            else
            {
                if (HitCache(Curr, Prev) == FALSE)  // Reading doesnt hit the 32 KB cache.
                {
                    DoJustRead (addr, length, offset, callback);
                }
                else
                {
                    blockNumOfPrevEnd = (Prev[WA_OFFS] + Prev[WA_LEN] - 1) / 32*1024;
                    blockNumOfCurrStart = Curr[WA_OFFS] / 32*1024;

                    if ( (blockNumOfPrevEnd == blockNumOfCurrStart) || (blockNumOfPrevEnd+1 == blockNumOfCurrStart) )
                    { // Reading do overlap cache.

                        // If last read finished less than 5 milliseconds ago, do just read,
                        // else wait 5 ms + 500 us before read.
                        if ( (__OSGetSystemTime() - LastReadFinished) < OSTicksToMilliseconds(5) )
                        {
                            DoJustRead (addr, length, offset, callback);
                        }
                        else
                        {
                            WaitBeforeRead (addr, length, offset, callback, OSTicksToMilliseconds(5) + OSTicksToMicroseconds(500));
                        }
                    }
                    else SeekTwiceBeforeRead (addr, length, offset, callback);
                }
            }
            break;

        default:    // Unknown workaround type.
            ASSERT(FALSE);  // Smart ass eh.
    }

    return TRUE;
}

// ---------------------------------------------------------------------------------
// Move laser head at specified position. Offset must be multiple of 4 by hardware limitations.

BOOL DVDLowSeek (s32 offset, DVDLowCallback callback)
{
    if (offset & 3)
        OSHalt ("DVDLowSeek(): offset must be a multiple of 4.");

    Callback = callback;
    StopAtNextInt = FALSE;

    dvdReg[DI_CMDBUF0] = 0xAB000000;
    dvdReg[DI_CMDBUF1] = offset >> 2;
    dvdReg[DI_CR] = DI_CR_TSTART;

    SetTimeoutAlarm (OSSecondsToTicks(10));
    return TRUE;
}

// ---------------------------------------------------------------------------------

BOOL DVDLowWaitCoverClose (DVDLowCallback callback)
{
    Callback = callback;
    WaitingCoverClose = TRUE;
    StopAtNextInt = FALSE;
    dvdReg[DI_CVR] = DI_CVR_CVRINTMSK;  // Enable cover interrupt.
    return TRUE;
}

// ---------------------------------------------------------------------------------

BOOL DVDLowReadDiskID (DVDDiskID *diskID, DVDLowCallback callback)
{
    if ((u32)diskID & 0x1f)
        OSHalt ("DVDLowReadID(): id must be aligned with 32 byte boundary.");

    Callback = callback;
    StopAtNextInt = FALSE;

    dvdReg[DI_CMDBUF0] = 0xA8000000 | 0x40;     // subcmd = 0x40
    dvdReg[DI_CMDBUF1] = 0;
    dvdReg[DI_CMDBUF2] = sizeof(DVDDiskID);
    dvdReg[DI_MAR] = (u32)diskID;
    dvdReg[DI_LEN] = sizeof(DVDDiskID);
    dvdReg[DI_CR] = DI_CR_DMA | DI_CR_TSTART;

    SetTimeoutAlarm (OSSecondsToTicks(10));
    return TRUE;
}

// ---------------------------------------------------------------------------------

BOOL DVDLowStopMotor (DVDLowCallback callback)
{
    Callback = callback;
    StopAtNextInt = FALSE;

    dvdReg[DI_CMDBUF0] = 0xE3000000;
    dvdReg[DI_CR] = DI_CR_TSTART;

    SetTimeoutAlarm (OSSecondsToTicks(10));
    return TRUE;
}

// ---------------------------------------------------------------------------------

BOOL DVDLowRequestError (DVDLowCallback callback)
{
    Callback = callback;
    StopAtNextInt = FALSE;

    dvdReg[DI_CMDBUF0] = 0xE0000000;
    dvdReg[DI_CR] = DI_CR_TSTART;

    SetTimeoutAlarm (OSSecondsToTicks(10));
    return TRUE;
}

// ---------------------------------------------------------------------------------
// Read DVD manufacturer info.

BOOL DVDLowInquiry (DVDDriveInfo *info, DVDLowCallback callback)
{
    Callback = callback;
    StopAtNextInt = FALSE;

    dvdReg[DI_CMDBUF0] = 0x12000000;
    dvdReg[DI_CMDBUF2] = sizeof(DVDDriveInfo);
    dvdReg[DI_MAR] = (u32)info;
    dvdReg[DI_LEN] = sizeof(DVDDriveInfo);
    dvdReg[DI_CR] = DI_CR_DMA | DI_CR_TSTART;

    SetTimeoutAlarm (OSSecondsToTicks(10));
    return TRUE;
}

// ---------------------------------------------------------------------------------

BOOL DVDLowAudioStream (u32 subcmd, s32 length, s32 offset, DVDLowCallback callback)
{
    Callback = callback;
    StopAtNextInt = FALSE;

    dvdReg[DI_CMDBUF0] = 0xE1000000 | subcmd;
    dvdReg[DI_CMDBUF1] = offset >> 2;
    dvdReg[DI_CMDBUF2] = length;
    dvdReg[DI_CR] = DI_CR_TSTART;

    SetTimeoutAlarm (OSSecondsToTicks(10));
    return TRUE;
}

// ---------------------------------------------------------------------------------

BOOL DVDLowRequestAudioStatus (u32 subcmd, DVDLowCallback callback)
{
    Callback = callback;
    StopAtNextInt = FALSE;

    dvdReg[DI_CMDBUF0] = 0xE2000000 | subcmd;
    dvdReg[DI_CR] = DI_CR_TSTART;

    SetTimeoutAlarm (OSSecondsToTicks(10));
    return TRUE;
}

// ---------------------------------------------------------------------------------
// Set DVD streaming buffer configuration.

/*
 * DVD Audio Buffer Config Command explained:
 *
 *  -----------------------------------------------------------------------------------------------
 * |00|01|02|03|04|05|06|07|08|09|10|11|12|13|14|15|16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|
 * |-----------------------------------------------------------------------------------------------|
 * | 1  1  1  0  0  1| Trig| -  -  -  -  -  -  -  -| E| -  -  -  -  -  -  -  -  -  -  -|   Siz     |
 *  -----------------------------------------------------------------------------------------------
 *
 * Trig : Trigger. Allowed values are 0, 1 and 2.
 * E    : Enable flag.
 * Siz  : Buffer size. Must not exceed 16 bytes.
*/

BOOL DVDLowAudioBufferConfig (BOOL enable, s32 size, DVDLowCallback callback)
{
    int bufSize, trigger;

    Callback = callback;
    StopAtNextInt = FALSE;

    bufSize = size & 0xf;
    trigger = size >> 28;

    ASSERT (bufSize < 16);
    ASSERT (trigger <= 2);

    dvdReg[DI_CMDBUF0] = 0xE4000000 | (enable ? 0x1000 : 0) | size;
    dvdReg[DI_CR] = DI_CR_TSTART;

    SetTimeoutAlarm (OSSecondsToTicks(10));
    return TRUE;
}

// ---------------------------------------------------------------------------------
// Reset DVD controller.

void DVDLowReset (void)
{
    OSTime resetStart;
    u32 reg;

    dvdReg[DI_CVR] = DI_CVR_CVRINTMSK;  // Enable cover interrupt.

    reg = resetReg;
    resetReg = (reg & ~4) | 1;  // Clear bit 29, set bit 31

    // Wait some time (12 usec).
    resetStart = __OSGetSystemTime ();
    while ( (__OSGetSystemTime () - resetStart) < OSMicrosecondsToTicks(12)){};

    resetReg = (reg | 4) | 1;   // Set bit 29, set bit 31

    ResetOccurred = TRUE;
    LastResetEnd = __OSGetSystemTime ();
}

// ---------------------------------------------------------------------------------

DVDLowCallback DVDLowSetResetCoverCallback (DVDLowCallback callback)
{
    DVDLowCallback  old;
    BOOL            enabled;

    enabled = OSDisableInterrupts ();
    old = ResetCoverCallback;
    ResetCoverCallback = callback;
    OSRestoreInterrupts (enabled);
    return old;
}

// ---------------------------------------------------------------------------------
// Do actual break (low-level command).

void DoBreak (void)
{
    dvdReg[DI_SR] |= DI_SR_BRKINT | DI_SR_BRK;  // clear Break interrupt and request for break.
    Breaking = TRUE;
}

// ---------------------------------------------------------------------------------
// Timeout handler for break operation.

void AlarmHandlerForBreak (OSAlarm *alarm, OSContext *context)
{
    // Break operation if its actually started, else delay 20 ms.
    if (dvdReg[DI_LEN] < LastLength) DoBreak ();
    else SetBreakAlarm (OSMillisecondsToTicks(20));
}

// ---------------------------------------------------------------------------------
// Set timeout for break operation.

void SetBreakAlarm (OSTime timeout)
{
    OSCreateAlarm (&AlarmForBreak);
    OSSetAlarm (&AlarmForBreak, timeout, AlarmHandlerForBreak);
}

// ---------------------------------------------------------------------------------
// Break last DVD operation.

BOOL DVDLowBreak (void)
{
    StopAtNextInt = TRUE;
    Breaking = TRUE;
    return TRUE;
}

// ---------------------------------------------------------------------------------
// Clear low-level DVD callback. Return previous callback address.

DVDLowCallback DVDLowClearCallback (void)
{
    DVDLowCallback old;

    dvdReg[DI_CVR] = 0;     // Mask DVD Cover interrupt (disable). WTF?
    old = Callback;
    Callback = NULL;
    return old;
}

// ---------------------------------------------------------------------------------
// Return DVD cover signal state. Good result may appear after 100 milliseconds only.

int DVDLowGetCoverStatus (void)
{
    // DVD cover state cannot be detected properly during drive reset.
    // So good result may appear after some time (approximately 100 milliseconds)
    if ( (__OSGetSystemTime () - LastResetEnd) < OSMillisecondsToTicks (100))
        return DVD_COVER_UNKNOWN;   // try later :)

    return (dvdReg[DI_CVR] & DI_CVR_CVR) ? DVD_COVER_OPEN : DVD_COVER_CLOSED;
}

// ---------------------------------------------------------------------------------
// Set workaround hack parameters.

void __DVDLowSetWAType (int type, s32 seekLoc)
{
    BOOL enabled = OSDisableInterrupts ();

    ASSERT (type < DVD_WATYPE_MAX);

    WorkAroundType = type;
    WorkAroundSeekLocation = seekLoc;

    OSRestoreInterrupts (enabled);
}
