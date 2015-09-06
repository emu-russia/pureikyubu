// Gamecube DVD Low-level interface.
// $root$/include/dolphin/dvd/DVDLow.h
//

// DVD cover status.
#define DVD_COVER_UNKNOWN   0       // DVD reset still continues. Cover state cannot be obtained during drive reset.
#define DVD_COVER_OPEN      1
#define DVD_COVER_CLOSED    2

// DVD-drive manufacturer information.
typedef struct DVDDriveInfo
{
    u16     revisionLevel;
    u16     deviceCode;
    u32     releaseDate;
    u8      padding[24];
} DVDDriveInfo;

// Low-level callback cause.
#define DVDLOW_CAUSE_TRANSFER       0x01    // DMA Transfer complete
#define DVDLOW_CAUSE_ERROR          0x02    // Disk Error
#define DVDLOW_CAUSE_RESET_COVER    0x04    // Cover signal status reset
#define DVDLOW_CAUSE_BREAK          0x08    // Break request
#define DVDLOW_CAUSE_TIMEOUT        0x10    // Operation timeout

// Callback for Low-level operations.
typedef void (*DVDLowCallback)(int cause);

BOOL    DVDLowRead (void *addr, s32 length, s32 offset, DVDLowCallback callback);
BOOL    DVDLowSeek (s32 offset, DVDLowCallback callback);
BOOL    DVDLowWaitCoverClose (DVDLowCallback callback);
BOOL    DVDLowReadDiskID (DVDDiskID *diskID, DVDLowCallback callback);
BOOL    DVDLowStopMotor (DVDLowCallback callback);
BOOL    DVDLowRequestError (DVDLowCallback callback);
BOOL    DVDLowInquiry (DVDDriveInfo *info, DVDLowCallback callback);
BOOL    DVDLowAudioStream (u32 subcmd, s32 length, s32 offset, DVDLowCallback callback);
BOOL    DVDLowRequestAudioStatus (u32 subcmd, DVDLowCallback callback);
BOOL    DVDLowAudioBufferConfig (BOOL enable, s32 size, DVDLowCallback callback);
void    DVDLowReset (void);
DVDLowCallback DVDLowSetResetCoverCallback (DVDLowCallback callback);
BOOL    DVDLowBreak (void);
DVDLowCallback DVDLowClearCallback (void);
int     DVDLowGetCoverStatus (void);
