// TODO : TEST WHEN DSP IS WORKING
// TODO : EXIINT and 0x8101 !!!!    
// TODO : figure out what 0x81 code is supposed to do on a real memcard

#include "pch.h"

using namespace Debug;

/************************** Commands ***************************************/ 

/*
Commands for MX25L4004
ReadArray           0x52 SA2 SA1 PN BA 0x00 0x00 0x00 0x00
ArrayToBuffer       0x53 SA2 SA1 PN                         (not implemented yet)
ReadBuffer          0x81 BA
WriteBuffer         0x82 BA                                 (not implemented yet)
StatusRead          0x83 0x00
ClearStatus         0x89
ReadId              0x85 0x00
ReadErrorBuffer     0x86 0x00                               (not implemented yet)
SectorErase         0xF1 SA2 SA1
PageProgram         0xF2 SA2 SA1 PN BA
ExtraByteProgram    0xF3 SA2 SA1 PN BA                      (not implemented yet)
Sleep               0X88
WakeUp              0x87

Parameters:
    (SA2 << 8) | SA1  =  sector
    PN  =  Page number
    BA  =  Bytes Address
*/

#define MEMCARD_COMMAND_UNDEFINED   0xFF
#define MEMCARD_COMMAND_GETEXIID    0x00
#define MEMCARD_COMMAND_READID      0x85
#define MEMCARD_COMMAND_GETSTATUS   0x83
#define MEMCARD_COMMAND_CLEARSTATUS 0x89
#define MEMCARD_COMMAND_READARRAY   0x52
#define MEMCARD_COMMAND_PAGEPROGRAM 0xF2
#define MEMCARD_COMMAND_ERASESECTOR 0xF1
#define MEMCARD_COMMAND_ERASECARD   0xF4
#define MEMCARD_COMMAND_SLEEP       0x88
#define MEMCARD_COMMAND_WAKEUP      0x87
#define MEMCARD_COMMAND_ENABLEINTER 0x81

#define MEMCARD_VALID_ADDRESS   0x03FF037F
#define MEMCARD_BA_EXTRABYTES   0x80 // when a BA is passed as arg, MEMCARD_BA_EXTRABYTES defines wheter the arg are regular bytes or extra bytes
/******************************************************************************************************/

const uint32_t Memcard_BytesMask [5] = { 0x00000000, 0xFF000000, 0xFFFF0000, 0xFFFFFF00, 0xFFFFFFFF };

/*
 * These functions just execute the readed command.
 */

/* 0x83 0x00
 * Returns the status byte of the memcard.
 */
static void MCGetStatusProc (Memcard * memcard);

/* 0x89
 * Clear errors bits on the status byte of the memcard.
 */
static void MCClearStatusProc (Memcard * memcard);

/* 0xF2 SA2 SA1 PN BA
 * Reads a specified number of bytes from the specified pointer 
 * and writes them to the memcard, at the specified sector, page and byte address
 */
static void MCPageProgramProc (Memcard * memcard);

/* 0x52 SA2 SA1 PN BA 0x00 0x00 0x00 0x00
 * Reads a specified number of bytes from the memcard
 * at the specified sector, page and byte address,
 * and writes them to the specified pointer
 */
static void MCReadArrayProc (Memcard * memcard);

/* 0xF1 SA2 SA1
 * Erases the specified sector
 */
static void MCSectorEraseProc (Memcard * memcard);

/* 0x00 0x00
 * Returns the Id of this EXI device ( the size of the memcards in MBits in this case )
 * Returns 0 on error
 */
static void MCGetEXIDeviceIdProc (Memcard * memcard);

/* 0xF4 0x00 0x00
 * Erases all the memcard data
 */
static void MCCardEraseProc (Memcard * memcard);

/* 0x81 EN
 * Enables/disables interrupts
 */
static void MCEnableInterruptsProc (Memcard * memcard);

/* 0X85 0x00
 * Returns the memcard id (Manufacturer and device id).
 */
static void MCReadIdProc (Memcard * memcard);

/* 0X88
 * Puts the memcard in sleep mode
 */
static void MCSleepProc (Memcard * memcard);

/* 0X87
 * Puts the memcard in non-sleep mode
 */
static void MCWakeUpProc (Memcard * memcard);

#define Num_Memcard_ValidCommands 11
const MCCommand Memcard_ValidCommands[Num_Memcard_ValidCommands] = {
    { 0, 1, MCImmRead, MCGetStatusProc,
        MEMCARD_COMMAND_GETSTATUS }, //#define MEMCARD_COMMAND_GETSTATUS     0x83
    { 0, 0, MCImmWrite, MCClearStatusProc,
        MEMCARD_COMMAND_CLEARSTATUS }, //#define MEMCARD_COMMAND_CLEARSTATUS   0x89
    { 4, 0, MCDmaWrite, MCPageProgramProc,
        MEMCARD_COMMAND_PAGEPROGRAM }, //#define MEMCARD_COMMAND_PAGEPROGRAM   0xF2
    { 4, 4, MCImmRead | MCDmaRead, MCReadArrayProc,
        MEMCARD_COMMAND_READARRAY }, //#define MEMCARD_COMMAND_READARRAY     0x52
    { 2, 0, MCImmWrite, MCSectorEraseProc,
        MEMCARD_COMMAND_ERASESECTOR }, //#define MEMCARD_COMMAND_ERASESECTOR   0xF1
    { 0, 1, MCImmRead, MCGetEXIDeviceIdProc,
        MEMCARD_COMMAND_GETEXIID }, //#define MEMCARD_COMMAND_GETEXIID      0x00
    { 0, 0, MCImmWrite, MCCardEraseProc,
        MEMCARD_COMMAND_ERASECARD }, //#define MEMCARD_COMMAND_ERASECARD     0xF4
    { 1, 0, MCImmWrite,  MCEnableInterruptsProc,
        MEMCARD_COMMAND_ENABLEINTER }, //#define MEMCARD_COMMAND_ENABLEINTER   0x81
    { 0, 1, MCImmRead, MCReadIdProc,
        MEMCARD_COMMAND_READID }, //#define MEMCARD_COMMAND_READID        0x85
    { 0, 0, MCImmWrite, MCSleepProc,
        MEMCARD_COMMAND_SLEEP }, //#define MEMCARD_COMMAND_SLEEP         0x88
    { 0, 0, MCImmWrite, MCWakeUpProc, 
        MEMCARD_COMMAND_WAKEUP }  //#define MEMCARD_COMMAND_WAKEUP        0x87
};

const uint32_t Memcard_ValidSizes[Num_Memcard_ValidSizes] = { 
    0x00080000, //524288 bytes , // Memory Card 59
    0x00100000, //1048576 bytes , // Memory Card 123
    0x00200000, //2097152 bytes , // Memory Card 251
    0x00400000, //4194304 bytes , // Memory Card 507
    0x00800000, //8388608 bytes , // Memory Card 1019
    0x01000000  //16777216 bytes , // Memory Card 2043
};

bool Memcard_Connected[2] = { false, false };
bool SyncSave = false;
bool MCOpened = false;

Memcard memcard[2];

static uint32_t MCCalculateOffset (uint32_t mc_address) {
	if (mc_address & MEMCARD_BA_EXTRABYTES)
		Halt ("MC :: Extra bytes are not supported\n");
    return        (mc_address & 0x0000007F) |
                 ((mc_address & 0x00000300) >> 1) |
                 ((mc_address & 0x7FFF0000) >> 7);
}

static void MCSyncSave (Memcard * memcard, uint32_t offset , uint32_t size) {
    if (SyncSave == true) // Bad idea!!
    {
        if (fseek(memcard->file, offset, SEEK_SET) != 0) {
            Halt("MC :: Error at seeking the memcard file.\n");
			return;
		}
        if (fwrite(&memcard->data[offset], size, 1, memcard->file) != 1) {
            Halt("MC :: Error at writing the memcard file.\n");
		}
    }
}
/*
 * All the following procedures asume that (memcard->connected == TRUE)
 */
/**********************************MCGetStatusProc*********************************************/
static void MCGetStatusProc (Memcard * memcard){
    EXIRegs * exi = memcard->exi;

    int auxbytes = (EXI_CR_TLEN(exi->cr) + 1);
    exi->data = (exi->data & ~Memcard_BytesMask[auxbytes]) |
        (((uint32_t)memcard->status << 24) & Memcard_BytesMask[auxbytes]);
}
/**********************************MCClearStatusProc*********************************************/
static void MCClearStatusProc (Memcard * memcard) {
// TODO : Verify which bits are cleared
    //memcard[cardnum].status &= (~MEMCARD_STATUS_ERASEERROR | ~MEMCARD_STATUS_PROGRAMEERROR);
    memcard->status &= (~MEMCARD_STATUS_ERASEERROR |
                                ~MEMCARD_STATUS_PROGRAMEERROR |
                                ~MEMCARD_STATUS_ARRAYTOBUFFER);
}
/**********************************MCPageProgramProc*********************************************/
static void MCPageProgramProc (Memcard * memcard){
    EXIRegs * exi = memcard->exi;

    uint32_t offset;
    uint8_t auxbyte = (uint8_t)(memcard->commandData & 0x000000FF);
    uint32_t auxdata = memcard->commandData;
    uint8_t *abuf;
    uint32_t size;
    if (exi->cr & EXI_CR_DMA) {
        abuf = &mi.ram[exi->madr & RAMMASK];
        size = exi->len;
    }
    else {
        Halt("MC : Unhandled Imm Page Program.\n");
		return;
    }


    offset = MCCalculateOffset(auxdata);

	if (offset >=  memcard->size + size) {
        Halt("MC :: PageProgram offset is out of range\n");
		return;
	}

    /* memcard->status |= MEMCARD_STATUS_BUSY; */

    memcpy(&memcard->data[offset], abuf, size);

    MCSyncSave (memcard, offset, size);

    /* memcard->status &= ~MEMCARD_STATUS_BUSY; */

    exi->csr |= EXI_CSR_EXIINT;
}
/**********************************MCReadArrayProc*********************************************/
static void MCReadArrayProc (Memcard * memcard){
    EXIRegs * exi = memcard->exi;

    uint32_t offset;
    uint8_t auxbyte = (uint8_t)(memcard->commandData & 0x000000FF);
    int auxbytes = (EXI_CR_TLEN(exi->cr) + 1);
    uint32_t auxdata = memcard->commandData;
    uint8_t *abuf;
    uint32_t size;

    if (exi->cr & EXI_CR_DMA) {
        abuf = &mi.ram[exi->madr & RAMMASK];
        size = exi->len;
    }
    else {
        abuf = (uint8_t *)&exi->data;
        size = auxbytes;
    }

    offset = MCCalculateOffset(auxdata);

    if (offset >=  memcard->size + size) {
        Halt("MC :: ReadArray offset is out of range\n");
        return;
    }

    /* memcard->status |= MEMCARD_STATUS_BUSY; */

    memcpy(abuf, &memcard->data[offset], size);

    /* memcard->status &= ~MEMCARD_STATUS_BUSY; */

    memcard->status |= MEMCARD_STATUS_ARRAYTOBUFFER;

    
    if (exi->cr & EXI_CR_DMA) {
    }
    else {
        exi->data = _byteswap_ulong( exi->data ) << (auxbytes - 4);
    }
}
/**********************************MCSectorEraseProc*********************************************/
static void MCSectorEraseProc (Memcard * memcard){
    EXIRegs * exi = memcard->exi;
    uint32_t offset;


    offset = MCCalculateOffset(memcard->commandData);

    if (offset >=  memcard->size) {
        Halt("MC :: Erase sector is out of range\n");
        return;
    }

	/* memcard->status |= MEMCARD_STATUS_BUSY; */

    memset(&memcard->data[offset], MEMCARD_ERASEBYTE, Memcard_BlockSize);

    MCSyncSave (memcard, offset, Memcard_BlockSize);

	/* memcard->status &= ~MEMCARD_STATUS_BUSY; */

    exi->csr |= EXI_CSR_EXIINT;
}
/**********************************MCSectorEraseProc*********************************************/
static void MCGetEXIDeviceIdProc (Memcard * memcard) {
    EXIRegs * exi = memcard->exi;

    int auxbytes = (EXI_CR_TLEN(exi->cr) + 1);
    exi->data = (exi->data & ~Memcard_BytesMask[auxbytes]) |
        ((uint32_t)(memcard->size >> 17) & Memcard_BytesMask[auxbytes]);
}
/**********************************MCCardEraseProc*********************************************/
static void MCCardEraseProc (Memcard * memcard) {
    uint32_t offset = 0;

	/* memcard->status |= MEMCARD_STATUS_BUSY; */

    memset(&memcard->data[offset], MEMCARD_ERASEBYTE, memcard->size);

    MCSyncSave (memcard, offset, memcard->size);

	/* memcard->status &= ~MEMCARD_STATUS_BUSY; */
}
/**********************************MCEnableInterruptsProc*********************************************/
static void MCEnableInterruptsProc (Memcard * memcard){
    EXIRegs * exi = memcard->exi;

    if ( memcard->commandData & (0x01 << 24) ) 
        Report (Channel::MC, "Enable Interrupts\n");
    else
        Report (Channel::MC, "Disable Interrupts\n");
}
/**********************************MCReadIdProc*********************************************/
static void MCReadIdProc (Memcard * memcard){
    EXIRegs * exi = memcard->exi;

    int auxbytes = (EXI_CR_TLEN(exi->cr) + 1);
    exi->data = (exi->data & ~Memcard_BytesMask[auxbytes]) |
        (((uint32_t)memcard->ID << 16) & Memcard_BytesMask[auxbytes]);
}
/**********************************MCSleepProc*********************************************/
static void MCSleepProc (Memcard * memcard) {
    memcard->status |= MEMCARD_STATUS_SLEEP;
}
/**********************************MCWakeUpProc*********************************************/
static void MCWakeUpProc (Memcard * memcard) {
    memcard->status &= ~MEMCARD_STATUS_SLEEP;
}

/********************************************************************************************/
void MCTransfer () {
    uint32_t auxdata, auxdma;
    int auxbytes, i;
    Memcard *auxmc;
    EXIRegs *auxexi;

    if ((exi.regs[MEMCARD_SLOTA].cr & EXI_CR_TSTART) &&
		(exi.regs[MEMCARD_SLOTA].csr & EXI_CSR_CS0B))
        auxmc = &memcard[MEMCARD_SLOTA];
    else if ((exi.regs[MEMCARD_SLOTB].cr & EXI_CR_TSTART) &&
			 (exi.regs[MEMCARD_SLOTB].csr & EXI_CSR_CS0B))
        auxmc = &memcard[MEMCARD_SLOTB];
    else return;

    if (auxmc->connected == false) return;

    auxexi = auxmc->exi;
    auxdma = auxexi->cr & EXI_CR_DMA;

    switch (EXI_CR_RW(auxexi->cr)) {
    case 0:  // Read Transfer
        if (auxmc->ready) {
            if ( auxdma && (auxmc->executionFlags & MCDmaRead ) )
                auxmc->procedure (auxmc);
            else if ( !auxdma && (auxmc->executionFlags & MCImmRead ) )
                auxmc->procedure (auxmc);
        }
        break;
    case 1:  // Write Transfer
        auxdata = auxexi->data;
        auxbytes = (EXI_CR_TLEN(auxexi->cr) + 1);
        if (!auxdma) {
            while (auxbytes > 0) {
                if (auxmc->Command == MEMCARD_COMMAND_UNDEFINED || auxmc->ready ) {
                    auxmc->Command = (uint8_t)(auxdata >> 24);
                    for (i = 0; i < Num_Memcard_ValidCommands; i++)
                        if (auxmc->Command == Memcard_ValidCommands[i].Command) {
                            auxmc->databytes = Memcard_ValidCommands[i].databytes;
                            auxmc->dummybytes = Memcard_ValidCommands[i].dummybytes;
                            auxmc->executionFlags = Memcard_ValidCommands[i].executionFlags;
                            auxmc->procedure = Memcard_ValidCommands[i].procedure;
                            auxmc->databytesread = 0;
                            auxmc->dummybytesread = 0;
                            auxmc->commandData = 0x00000000;
                            auxmc->ready = false;
                            break;
                        }

                    if (i >= Num_Memcard_ValidCommands) {
                        Halt("MC :: Unrecognized Memcard Command %02x\n", auxmc->Command);
                        auxmc->Command = MEMCARD_COMMAND_UNDEFINED;
                    }
                    else {
                        Report (Channel::MC, "Recognized Memcard Command %02x\n", auxmc->Command);
                    }
                }
                else if (auxmc->databytesread < auxmc->databytes) {
                    auxmc->commandData |= ((auxdata & 0xFF000000) >> (auxmc->databytesread * 8));
                    auxmc->databytesread++;
                }
                else if (auxmc->dummybytesread < auxmc->dummybytes) {
                    auxmc->dummybytesread++;
                }
                else 
                    Halt("MC :: Extra bytes at transfer , data : %02x\n", (uint8_t)(auxdata >> 24));
                auxdata = auxdata << 8;
                auxbytes--;
            }

            if (auxmc->Command != MEMCARD_COMMAND_UNDEFINED &&
                auxmc->databytesread == auxmc->databytes &&
                auxmc->dummybytesread == auxmc->dummybytes)
                auxmc->ready = true;
        }

        if (auxmc->ready) {
            if ( auxdma && (auxmc->executionFlags & MCDmaWrite ) ) 
                auxmc->procedure (auxmc);
            else if ( !auxdma && (auxmc->executionFlags & MCImmWrite ) ) 
                auxmc->procedure (auxmc);
        }
        break;
    default:
        Halt ("MC: Unknown memcard transfer type");
    }
}

/*
 * Checks if the memcard is connected.
 */
bool    MCIsConnected(int cardnum) {
    // Invalid memcard number
    assert((cardnum == MEMCARD_SLOTA) || (cardnum == MEMCARD_SLOTB));
    return memcard[cardnum].connected;
}

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
bool    MCCreateMemcardFile(const TCHAR *path, uint16_t memcard_id) {
    FILE * newfile;
    uint32_t b, blocks;
    uint8_t newfile_buffer[Memcard_BlockSize];

    switch (memcard_id) {
    case MEMCARD_ID_64:
    case MEMCARD_ID_128:
    case MEMCARD_ID_256:
    case MEMCARD_ID_512:
    case MEMCARD_ID_1024:
    case MEMCARD_ID_2048:
        /* 17 = Mbits to byte conversion */
        blocks = ((uint32_t)memcard_id) << (17 - Memcard_BlockSize_log2); 
        break;
    default:
        Halt ("MC: Wrong card id for creating file.");
        return false;
    }

    newfile = nullptr;
    _tfopen_s(&newfile, path, _T("wb")) ;

	if (newfile == NULL) {
        Halt( "MC: Error while trying to create memcard file.");
		return false;
	}

    memset(newfile_buffer, MEMCARD_ERASEBYTE, Memcard_BlockSize);
    for (b = 0; b < blocks; b++) {
        if (fwrite (newfile_buffer, Memcard_BlockSize, 1, newfile) != 1) {
            Halt("MC: Error while trying to write memcard file.");

			fclose (newfile);
            return false;
        }
    }

    fclose (newfile);
    return true;
}

/*
 * Sets the memcard to use the specified file. If the memcard is connected, 
 * it will be first disconnected (to ensure that changes are saved)
 * if param connect is TRUE, then the memcard will be connected to the new file
 */ 
void    MCUseFile(int cardnum, const TCHAR *path, bool connect) {

    // Invalid memcard number
    assert((cardnum == MEMCARD_SLOTA) || (cardnum == MEMCARD_SLOTB));
    if (memcard[cardnum].connected == true) MCDisconnect(cardnum);

    memset(memcard[cardnum].filename, 0, sizeof (memcard[cardnum].filename));
    _tcscpy_s(memcard[cardnum].filename, _countof(memcard[cardnum].filename) - 1, path);

    if (connect == true) MCConnect(cardnum);
}

/* 
 * Starts the memcard system and loads the saved settings.
 * If no settings are found, default memcards are created.
 * Then both memcards are connected (based on settings)
 */ 
void MCOpen (HWConfig * config)
{
    Report (Channel::MC, "Memory cards\n");

    MCOpened = true;
    memset(memcard, 0 , 2 * sizeof (Memcard));
    memcard[MEMCARD_SLOTA].Command = MEMCARD_COMMAND_UNDEFINED;
    memcard[MEMCARD_SLOTB].Command = MEMCARD_COMMAND_UNDEFINED;
    memcard[MEMCARD_SLOTA].ready = true;
    memcard[MEMCARD_SLOTB].ready = true;
    memcard[MEMCARD_SLOTA].exi = &exi.regs[MEMCARD_SLOTA];
    memcard[MEMCARD_SLOTB].exi = &exi.regs[MEMCARD_SLOTB];

    /* load settings */
    Memcard_Connected[MEMCARD_SLOTA] = config->MemcardA_Connected;
    Memcard_Connected[MEMCARD_SLOTB] = config->MemcardB_Connected;
    _tcscpy_s(memcard[MEMCARD_SLOTA].filename, _countof(memcard[MEMCARD_SLOTA].filename) - 1, config->MemcardA_Filename);
    _tcscpy_s(memcard[MEMCARD_SLOTB].filename, _countof(memcard[MEMCARD_SLOTB].filename) - 1, config->MemcardB_Filename);
    SyncSave = config->Memcard_SyncSave;

    if (memcard[MEMCARD_SLOTA].filename[0] == 0) {
        /* there is no info in the registry. use a default memcard */

        const TCHAR * filename = _T(".\\Data\\MemCardA.mci");
        FILE * fileptr;

        fileptr = nullptr;
        _tfopen_s(&fileptr, filename, _T("rb"));
        if (fileptr == nullptr) {
            /* if default memcard doesn't exist, create it */
            if (MCCreateMemcardFile(filename, MEMCARD_ID_64) == true) {
                MCUseFile(MEMCARD_SLOTA, filename, false);
                Memcard_Connected[MEMCARD_SLOTA] = true;
            }
        }
        else {
            fclose(fileptr);
            MCUseFile(MEMCARD_SLOTA, filename, false);
            Memcard_Connected[MEMCARD_SLOTA] = true;
        }
    }
    if (memcard[MEMCARD_SLOTB].filename[0] == 0) {
        /* there is no info in the registry. use a default memcard */

        const TCHAR * filename = _T(".\\Data\\MemCardB.mci");
        FILE * fileptr;

        fileptr = nullptr;
        _tfopen_s (&fileptr, filename, _T("rb"));
        if (fileptr == nullptr) {
            /* if default memcard doesn't exist, create it */
            if (MCCreateMemcardFile(filename, MEMCARD_ID_64) == true) {
                MCUseFile(MEMCARD_SLOTB, filename, false);
                Memcard_Connected[MEMCARD_SLOTB] = true;
            }
        }
        else {
            fclose(fileptr);
            MCUseFile(MEMCARD_SLOTB, filename, false);
            Memcard_Connected[MEMCARD_SLOTB] = true;
        }
    }

    MCConnect();
}

/* 
 * Disconnects both Memcard. Closes the memcard system and saves the current settings
 */ 
void MCClose () {
    MCOpened = false;
    MCDisconnect();
}

/* 
 * Connects the choosen memcard
 * 
 * cardnum = -1 for both (based on the Memcard_Connected setting)
 */
bool MCConnect (int cardnum) {
    bool ret = true;
    int i;
    switch (cardnum) {
    case -1:
        if (Memcard_Connected[MEMCARD_SLOTA] /*== TRUE*/)   ret = MCConnect(MEMCARD_SLOTA);
        if (Memcard_Connected[MEMCARD_SLOTB] /*== TRUE*/)   ret = ret && MCConnect(MEMCARD_SLOTB);
        return ret;
        break;
    case MEMCARD_SLOTA:
    case MEMCARD_SLOTB:
        if (memcard[cardnum].connected /*== TRUE*/) MCDisconnect(cardnum) ;

        size_t memcardSize = Util::FileSize(memcard[cardnum].filename);

        memcard[cardnum].file = nullptr;
        _tfopen_s (&memcard[cardnum].file, memcard[cardnum].filename, _T("r+b"));
        if (memcard[cardnum].file == nullptr) {
            static char slt[2] = { 'A', 'B' };

            // TODO: redirect user to memcard configure dialog ?
            Report(
                Channel::MC,
                "Couldnt open memcard (slot %c),\n"
                "location : %s\n\n"
                "Check path or file attributes.",
                slt[cardnum], Util::TcharToString(memcard[cardnum].filename).c_str()
            );
            return false;
        }

        for (i = 0 ; i < Num_Memcard_ValidSizes && Memcard_ValidSizes[i] != (uint32_t)memcardSize; i++);

        if (i >= Num_Memcard_ValidSizes) {
//          DBReport(YEL "memcard file doesnt have a valid size\n");
            MessageBox (NULL, _T("memcard file doesnt have a valid size"), _T("Memcard Error"), 0);
            fclose(memcard[cardnum].file);
            memcard[cardnum].file = nullptr;
            return false;
        }

        memcard[cardnum].size = (uint32_t)memcardSize;
        memcard[cardnum].data = (uint8_t *)malloc(memcard[cardnum].size);

        if (memcard[cardnum].data == nullptr) {
//          DBReport(YEL "couldnt allocate enough memory for memcard\n");
            MessageBox (nullptr, _T("couldnt allocate enough memory for memcard"), _T("Memcard Error"), 0);
            fclose(memcard[cardnum].file);
            memcard[cardnum].file = nullptr;
            return false;
        }

        if (fseek(memcard[cardnum].file, 0, SEEK_SET) != 0) {
//          DBReport(YEL "error at locating file cursor\n");
            MessageBox (nullptr, _T("error at locating file cursor"), _T("Memcard Error"), 0);
            free (memcard[cardnum].data);
            memcard[cardnum].data = nullptr;
            fclose(memcard[cardnum].file);
            memcard[cardnum].file = nullptr;
            return false;
        }

        if (fread(memcard[cardnum].data, memcard[cardnum].size, 1, memcard[cardnum].file) != 1) {
//          DBReport(YEL "error at reading the memcard file\n");
            MessageBox (nullptr, _T("error at reading the memcard file"), _T("Memcard Error"), 0);
            free (memcard[cardnum].data);
            memcard[cardnum].data = nullptr;
            fclose(memcard[cardnum].file);
            memcard[cardnum].file = nullptr;
            return false;
        }

        /* if nothing fails... */
        memcard[cardnum].ID = ((uint16_t)0xC2) << 8 | (uint16_t)0x42; // Datel's code just for now
        memcard[cardnum].status = MEMCARD_STATUS_READY;
        memcard[cardnum].connected = true;
        EXIAttach(cardnum);        // connect device

        return true;
    }
    return false;
}

/* 
 * Saves the data from the memcard to disk and disconnects the choosen memcard
 * 
 * cardnum = -1 for both
 */
bool MCDisconnect (int cardnum) {
    bool ret = true;
    switch (cardnum) {
    case -1:
        ret = MCDisconnect(MEMCARD_SLOTA) && MCDisconnect(MEMCARD_SLOTB);
        break;
    case MEMCARD_SLOTA:
    case MEMCARD_SLOTB:
        if (!memcard[cardnum].connected) break ;

        if (fseek(memcard[cardnum].file, 0, SEEK_SET) != 0)
        {
            ret = false;
        }
        else
        {
            /* write to the file */
            if (fwrite(memcard[cardnum].data, memcard[cardnum].size, 1, memcard[cardnum].file) != 1)
            {
                ret = false;
            }

        }
        /* close the file */
        fclose(memcard[cardnum].file);

        free(memcard[cardnum].data);

        memcard[cardnum].ID = 0;
        memcard[cardnum].size = 0;
        memcard[cardnum].file = nullptr;
        memcard[cardnum].data = nullptr;
        memcard[cardnum].status = 0;
        memcard[cardnum].connected = false;
        EXIDetach(cardnum);        // disconnect device

        break;
    }
    return ret;
}
