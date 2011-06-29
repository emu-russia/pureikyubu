#ifndef __MC_H__
#define __MC_H__

/* Memcard ids by number of blocks */
#define MEMCARD_ID_64       (0x0004)
#define MEMCARD_ID_128      (0x0008)
#define MEMCARD_ID_256      (0x0010)
#define MEMCARD_ID_512      (0x0020)
#define MEMCARD_ID_1024     (0x0040)
#define MEMCARD_ID_2048     (0x0080)

/* Memcard ids by number of usable blocks */
#define MEMCARD_ID_59       MEMCARD_ID_64
#define MEMCARD_ID_123      MEMCARD_ID_128
#define MEMCARD_ID_251      MEMCARD_ID_256
#define MEMCARD_ID_507      MEMCARD_ID_512
#define MEMCARD_ID_1019     MEMCARD_ID_1024 
#define MEMCARD_ID_2043     MEMCARD_ID_2048

#define MEMCARD_ERASEBYTE 0x00
//#define MEMCARD_ERASEBYTE 0xFF

#define MEMCARD_STATUS_BUSY             0x80 // PROGRAM or ERASE OPERATION      
#define MEMCARD_STATUS_ARRAYTOBUFFER    0x40
#define MEMCARD_STATUS_SLEEP            0x20
#define MEMCARD_STATUS_ERASEERROR       0x10
#define MEMCARD_STATUS_PROGRAMEERROR    0x08
#define MEMCARD_STATUS_READY		    0x01 // ?????

#define MEMCARD_SLOTA         0
#define MEMCARD_SLOTB         1

/* Names of the keys used to store to configuration in the windows register */
#define HKEY_MEMCARD   USER_KEY_NAME "\\Memcard"
#define MemcardA_Connected_Key "MemcardA_Connected"
#define MemcardB_Connected_Key "MemcardB_Connected"
#define MemcardA_Filename_Key "MemcardA_Filename"
#define MemcardB_Filename_Key "MemcardB_Filename"
#define Memcard_SyncSave_Key "Memcard_SyncSave"

/* Execution flags for commands */
#define MCImmRead       (1 << 0)
#define MCImmWrite      (1 << 1)
#define MCDmaRead       (1 << 2)
#define MCDmaWrite      (1 << 3)

/* structure of a memcard buffer */
typedef struct Memcard {
    char filename[256]; // filename where the memcard data is stores 
    FILE * file;        // pointer to that file
    u32 size;           // size of the memcard in bytes
    u8 * data;          // pointer to the memcard raw data (stored in little endian order)
    BOOL connected;     // indicates if the memcard is actually 'connected', meaning all data in the structure is valid
    u16 ID;             // manufacturer and device code
    u8 status;          // current status

//  Command Reader
    u8 Command;
    int databytes;
    int dummybytes;
    u32 executionFlags;
    void (__fastcall *procedure)(Memcard *);
    int databytesread;
    int dummybytesread;
    u32 commandData;
    BOOL ready;
    EXIRegs * exi;
} Memcard;

typedef struct MCCommand {
    int databytes;
    int dummybytes;
    u32 executionFlags;
    void (__fastcall *procedure)(Memcard *);
    u8 Command;
} MCCommand ;

#define Memcard_BlockSize       8192
#define Memcard_BlockSize_log2  13 // just to make shifts instead of mult. or div.
#define Memcard_SectorSize      512
#define Memcard_SectorSize_log2 9 // just to make shifts instead of mult. or div.
#define Memcard_PageSize        128
#define Memcard_PageSize_log2   7 // just to make shifts instead of mult. or div.


/* definition of valid sizes for a memcard in bytes */
#define Num_Memcard_ValidSizes 6
extern const u32 Memcard_ValidSizes[Num_Memcard_ValidSizes];

/* defines if the memcard should be tried to connect, (not if the memcard is actually connected!!) */
extern BOOL Memcard_Connected[2];

/*
 * if SyncSave is TRUE, all write operations on the memcard will be instantaneusly saved to disk
 * if not, the memcard will only be saved to disk when it is disconnected
 */
extern BOOL SyncSave;

/* Memcards vars */
extern Memcard memcard[2];

/* defines if the Memcard system has been opened */
extern BOOL MCOpened;

/***************************************************************/

void MCTransfer () ;

/*
 * Checks if the memcard is connected.
 */
BOOL    MCIsConnected(int cardnum);

/*
 * Creates a new memcard file
 * memcard_id should be one of the following:
 * MEMCARD_ID_64       (0x0004)
 * MEMCARD_ID_128      (0x0008)
 * MEMCARD_ID_256      (0x0010)
 * MEMCARD_ID_512      (0x0020)
 * MEMCARD_ID_1024     (0x0040)
 * MEMCARD_ID_2048     (0x0080)
 */
BOOL    MCCreateMemcardFile(char *path, u16 memcard_id);

/*
 * Sets the memcard to use the specified file. If the memcard is connected, 
 * it will be first disconnected (to ensure that changes are saved)
 * if param connect is TRUE, then the memcard will be connected to the new file
 */ 
void    MCUseFile(int cardnum, char *path, BOOL connect);

/* 
 * Starts the memcard system and loads the saved settings.
 * If no settings are found, default memcards are created.
 * Then both memcards are connected (based on settings)
 */ 
void    MCOpen ();

/* 
 * Disconnects both Memcard. Closes the memcard system and saves the current settings
 */ 
void    MCClose ();

/* 
 * Connects the choosen memcard
 * 
 * cardnum = -1 for both (based on the Memcard_Connected setting)
 */
BOOL    MCConnect (int cardnum = -1);

/* 
 * Saves the data from the memcard to disk and disconnects the choosen memcard
 * 
 * cardnum = -1 for both
 */
BOOL    MCDisconnect (int cardnum = -1);

#endif //__MC_H__
