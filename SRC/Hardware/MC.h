#pragma once

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

/* Names of the keys used to store to configuration */
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
struct Memcard {
    TCHAR filename[0x1000]; // filename where the memcard data is stores 
    FILE * file;        // pointer to that file
    uint32_t size;           // size of the memcard in bytes
    uint8_t * data;          // pointer to the memcard raw data (stored in little endian order)
    bool connected;     // indicates if the memcard is actually 'connected', meaning all data in the structure is valid
    uint16_t ID;             // manufacturer and device code
    uint8_t status;          // current status

//  Command Reader
    uint8_t Command;
    int databytes;
    int dummybytes;
    uint32_t executionFlags;
    void (*procedure)(Memcard *);
    int databytesread;
    int dummybytesread;
    uint32_t commandData;
    bool ready;
    EXIRegs * exi;
};

struct MCCommand {
    int databytes;
    int dummybytes;
    uint32_t executionFlags;
    void (*procedure)(Memcard *);
    uint8_t Command;
};

#define Memcard_BlockSize       8192
#define Memcard_BlockSize_log2  13 // just to make shifts instead of mult. or div.
#define Memcard_SectorSize      512
#define Memcard_SectorSize_log2 9 // just to make shifts instead of mult. or div.
#define Memcard_PageSize        128
#define Memcard_PageSize_log2   7 // just to make shifts instead of mult. or div.


/* definition of valid sizes for a memcard in bytes */
#define Num_Memcard_ValidSizes 6
extern const uint32_t Memcard_ValidSizes[Num_Memcard_ValidSizes];

/* defines if the memcard should be tried to connect, (not if the memcard is actually connected!!) */
extern bool Memcard_Connected[2];

/*
 * if SyncSave is TRUE, all write operations on the memcard will be instantaneusly saved to disk
 * if not, the memcard will only be saved to disk when it is disconnected
 */
extern bool SyncSave;

/* Memcards vars */
extern Memcard memcard[2];

/* defines if the Memcard system has been opened */
extern bool MCOpened;

/***************************************************************/

void MCTransfer () ;

/*
 * Checks if the memcard is connected.
 */
bool    MCIsConnected(int cardnum);

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
bool    MCCreateMemcardFile(const TCHAR *path, uint16_t memcard_id);

/*
 * Sets the memcard to use the specified file. If the memcard is connected, 
 * it will be first disconnected (to ensure that changes are saved)
 * if param connect is TRUE, then the memcard will be connected to the new file
 */ 
void    MCUseFile(int cardnum, const TCHAR *path, bool connect);

/* 
 * Starts the memcard system and loads the saved settings.
 * If no settings are found, default memcards are created.
 * Then both memcards are connected (based on settings)
 */ 
void    MCOpen (HWConfig* config);

/* 
 * Disconnects both Memcard. Closes the memcard system and saves the current settings
 */ 
void    MCClose ();

/* 
 * Connects the choosen memcard
 * 
 * cardnum = -1 for both (based on the Memcard_Connected setting)
 */
bool    MCConnect (int cardnum = -1);

/* 
 * Saves the data from the memcard to disk and disconnects the choosen memcard
 * 
 * cardnum = -1 for both
 */
bool    MCDisconnect (int cardnum = -1);
