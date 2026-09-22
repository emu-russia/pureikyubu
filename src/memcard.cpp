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

const uint32_t Memcard_BytesMask[5] = { 0x00000000, 0xFF000000, 0xFFFF0000, 0xFFFFFF00, 0xFFFFFFFF };

/*
 * These functions just execute the readed command.
 */

 /* 0x83 0x00
  * Returns the status byte of the memcard.
  */
static void MCGetStatusProc(Memcard* memcard, EXIRegs* exi);

/* 0x89
 * Clear errors bits on the status byte of the memcard.
 */
static void MCClearStatusProc(Memcard* memcard, EXIRegs* exi);

/* 0xF2 SA2 SA1 PN BA
 * Reads a specified number of bytes from the specified pointer
 * and writes them to the memcard, at the specified sector, page and byte address
 */
static void MCPageProgramProc(Memcard* memcard, EXIRegs* exi);

/* 0x52 SA2 SA1 PN BA 0x00 0x00 0x00 0x00
 * Reads a specified number of bytes from the memcard
 * at the specified sector, page and byte address,
 * and writes them to the specified pointer
 */
static void MCReadArrayProc(Memcard* memcard, EXIRegs* exi);

/* 0xF1 SA2 SA1
 * Erases the specified sector
 */
static void MCSectorEraseProc(Memcard* memcard, EXIRegs* exi);

/* 0x00 0x00
 * Returns the Id of this EXI device ( the size of the memcards in MBits in this case )
 * Returns 0 on error
 */
static void MCGetEXIDeviceIdProc(Memcard* memcard, EXIRegs* exi);

/* 0xF4 0x00 0x00
 * Erases all the memcard data
 */
static void MCCardEraseProc(Memcard* memcard, EXIRegs* exi);

/* 0x81 EN
 * Enables/disables interrupts
 */
static void MCEnableInterruptsProc(Memcard* memcard, EXIRegs* exi);

/* 0X85 0x00
 * Returns the memcard id (Manufacturer and device id).
 */
static void MCReadIdProc(Memcard* memcard, EXIRegs* exi);

/* 0X88
 * Puts the memcard in sleep mode
 */
static void MCSleepProc(Memcard* memcard, EXIRegs* exi);

/* 0X87
 * Puts the memcard in non-sleep mode
 */
static void MCWakeUpProc(Memcard* memcard, EXIRegs* exi);

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

Memcard memcard[2];

static uint32_t MCCalculateOffset(uint32_t mc_address) {
	// Fail closed: Halt() only logs, so returning an offset with the extra-bytes bit
	// silently masked off would let the malformed request reach the copy below.
	if (mc_address & MEMCARD_BA_EXTRABYTES) {
		Report(Channel::MC, "MC :: Extra bytes are not supported\n");
		return UINT32_MAX;		// no caller's range check accepts this
	}
	return        (mc_address & 0x0000007F) |
		((mc_address & 0x00000300) >> 1) |
		((mc_address & 0x7FFF0000) >> 7);
}

static void MCSyncSave(Memcard* memcard, uint32_t offset, uint32_t size) {
	if (memcard->syncSave == true) // Bad idea!!
	{
		// The callers validate their window too, but a save must never fwrite outside
		// the card image even if one of them forgets.
		if (memcard->file == nullptr || memcard->data == nullptr ||
			!Verify::MemcardWindow(memcard->size, offset, size)) {
			Report(Channel::MC, "MC :: SyncSave offset is out of range\n");
			return;
		}
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
static void MCGetStatusProc(Memcard* memcard, EXIRegs* exi) {

	int auxbytes = (EXI_CR_TLEN(exi->cr) + 1);
	exi->data = (exi->data & ~Memcard_BytesMask[auxbytes]) |
		(((uint32_t)memcard->status << 24) & Memcard_BytesMask[auxbytes]);
}
/**********************************MCClearStatusProc*********************************************/
static void MCClearStatusProc(Memcard* memcard, EXIRegs* exi) {
	// TODO : Verify which bits are cleared
		//memcard[cardnum].status &= (~MEMCARD_STATUS_ERASEERROR | ~MEMCARD_STATUS_PROGRAMEERROR);
	memcard->status &= (~MEMCARD_STATUS_ERASEERROR |
		~MEMCARD_STATUS_PROGRAMEERROR |
		~MEMCARD_STATUS_ARRAYTOBUFFER);
}
/**********************************MCPageProgramProc*********************************************/
static void MCPageProgramProc(Memcard* memcard, EXIRegs* exi) {

	uint32_t offset;
	uint8_t auxbyte = (uint8_t)(memcard->commandData & 0x000000FF);
	uint32_t auxdata = memcard->commandData;
	uint8_t* abuf;
	uint32_t size;
	if (exi->cr & EXI_CR_DMA) {
		size = exi->len;
		// exi->len is a raw guest register: the length-aware accessor rejects both a
		// MADR outside main memory and a transfer that runs past the end of it.
		abuf = (uint8_t*)Flipper::HW->mem->MIGetMemoryPointerForIO(exi->madr & EXI_MADR_MASK, size);
		if (abuf == nullptr) {
			Report(Channel::MC, "PageProgram DMA source is out of main memory\n");
			return;
		}
	}
	else {
		Halt("MC : Unhandled Imm Page Program.\n");
		return;
	}


	offset = MCCalculateOffset(auxdata);

	if (!Verify::MemcardWindow(memcard->size, offset, size)) {
		Report(Channel::MC, "PageProgram offset is out of range\n");
		return;
	}

	/* memcard->status |= MEMCARD_STATUS_BUSY; */

	memcpy(&memcard->data[offset], abuf, size);

	MCSyncSave(memcard, offset, size);

	/* memcard->status &= ~MEMCARD_STATUS_BUSY; */

	exi->csr |= EXI_CSR_EXIINT;
}
/**********************************MCReadArrayProc*********************************************/
static void MCReadArrayProc(Memcard* memcard, EXIRegs* exi) {

	uint32_t offset;
	uint8_t auxbyte = (uint8_t)(memcard->commandData & 0x000000FF);
	int auxbytes = (EXI_CR_TLEN(exi->cr) + 1);
	uint32_t auxdata = memcard->commandData;
	uint8_t* abuf;
	uint32_t size;

	if (exi->cr & EXI_CR_DMA) {
		size = exi->len;
		// Length-aware: the destination has to hold the whole transfer, not just its start.
		abuf = (uint8_t*)Flipper::HW->mem->MIGetMemoryPointerForIO(exi->madr & EXI_MADR_MASK, size);
		if (abuf == nullptr) {
			Report(Channel::MC, "ReadArray DMA destination is out of main memory\n");
			return;
		}
	}
	else {
		// The immediate branch copies into the 4-byte data register, so the length
		// (the TLEN field, 1..4) can never exceed it.
		if (auxbytes < 1 || auxbytes > (int)sizeof(exi->data)) {
			Report(Channel::MC, "ReadArray immediate length is out of range\n");
			return;
		}
		abuf = (uint8_t*)&exi->data;
		size = auxbytes;
	}

	offset = MCCalculateOffset(auxdata);

	if (!Verify::MemcardWindow(memcard->size, offset, size)) {
		Report(Channel::MC, "ReadArray offset is out of range\n");
		return;
	}

	/* memcard->status |= MEMCARD_STATUS_BUSY; */

	memcpy(abuf, &memcard->data[offset], size);

	/* memcard->status &= ~MEMCARD_STATUS_BUSY; */

	memcard->status |= MEMCARD_STATUS_ARRAYTOBUFFER;


	if (exi->cr & EXI_CR_DMA) {
	}
	else {
		// auxbytes is 1..4: shift the byte the guest receives down without the
		// negative shift count the old `<< (auxbytes - 4)` produced.
		exi->data = _BYTESWAP_UINT32(exi->data) >> (8 * (4 - auxbytes));
	}
}
/**********************************MCSectorEraseProc*********************************************/
static void MCSectorEraseProc(Memcard* memcard, EXIRegs* exi) {
	uint32_t offset;

	offset = MCCalculateOffset(memcard->commandData);

	// The whole erased sector has to fit, not just its first byte.
	if (!Verify::MemcardWindow(memcard->size, offset, Memcard_BlockSize)) {
		Report(Channel::MC, "MC :: Erase sector is out of range\n");
		return;
	}

	/* memcard->status |= MEMCARD_STATUS_BUSY; */

	memset(&memcard->data[offset], MEMCARD_ERASEBYTE, Memcard_BlockSize);

	MCSyncSave(memcard, offset, Memcard_BlockSize);

	/* memcard->status &= ~MEMCARD_STATUS_BUSY; */

	exi->csr |= EXI_CSR_EXIINT;
}
/**********************************MCSectorEraseProc*********************************************/
static void MCGetEXIDeviceIdProc(Memcard* memcard, EXIRegs* exi) {

	int auxbytes = (EXI_CR_TLEN(exi->cr) + 1);
	// The mask keeps the most significant byte, so the capacity has to be shifted
	// there the way MCGetStatusProc/MCReadIdProc do it (was always zero).
	exi->data = (exi->data & ~Memcard_BytesMask[auxbytes]) |
		((((uint32_t)(memcard->size >> 17)) << 24) & Memcard_BytesMask[auxbytes]);
}
/**********************************MCCardEraseProc*********************************************/
static void MCCardEraseProc(Memcard* memcard, EXIRegs* exi) {
	uint32_t offset = 0;

	/* memcard->status |= MEMCARD_STATUS_BUSY; */

	memset(&memcard->data[offset], MEMCARD_ERASEBYTE, memcard->size);

	MCSyncSave(memcard, offset, memcard->size);

	/* memcard->status &= ~MEMCARD_STATUS_BUSY; */
}
/**********************************MCEnableInterruptsProc*********************************************/
static void MCEnableInterruptsProc(Memcard* memcard, EXIRegs* exi) {

	if (memcard->commandData & (0x01 << 24))
		Report(Channel::MC, "Enable Interrupts\n");
	else
		Report(Channel::MC, "Disable Interrupts\n");
}
/**********************************MCReadIdProc*********************************************/
static void MCReadIdProc(Memcard* memcard, EXIRegs* exi) {

	int auxbytes = (EXI_CR_TLEN(exi->cr) + 1);
	exi->data = (exi->data & ~Memcard_BytesMask[auxbytes]) |
		(((uint32_t)memcard->ID << 16) & Memcard_BytesMask[auxbytes]);
}
/**********************************MCSleepProc*********************************************/
static void MCSleepProc(Memcard* memcard, EXIRegs* exi) {
	memcard->status |= MEMCARD_STATUS_SLEEP;
}
/**********************************MCWakeUpProc*********************************************/
static void MCWakeUpProc(Memcard* memcard, EXIRegs* exi) {
	memcard->status &= ~MEMCARD_STATUS_SLEEP;
}

/********************************************************************************************/
void MCTransfer(Flipper::ExternalInterface* exi) {
	uint32_t auxdata, auxdma;
	int auxbytes, i;
	Memcard* auxmc;
	EXIRegs* auxexi;

	if ((exi->exi.regs[MEMCARD_SLOTA].cr & EXI_CR_TSTART) &&
		(exi->exi.regs[MEMCARD_SLOTA].csr & EXI_CSR_CS0B)) {
		auxmc = &memcard[MEMCARD_SLOTA];
		auxexi = &Flipper::HW->exi->exi.regs[MEMCARD_SLOTA];
	}
	else if ((exi->exi.regs[MEMCARD_SLOTB].cr & EXI_CR_TSTART) &&
		(exi->exi.regs[MEMCARD_SLOTB].csr & EXI_CSR_CS0B)) {
		auxmc = &memcard[MEMCARD_SLOTB];
		auxexi = &Flipper::HW->exi->exi.regs[MEMCARD_SLOTB];
	}
	else return;

	if (auxmc->connected == false) return;

	auxdma = auxexi->cr & EXI_CR_DMA;

	switch (EXI_CR_RW(auxexi->cr)) {
		case 0:  // Read Transfer
			if (auxmc->ready) {
				if (auxdma && (auxmc->executionFlags & MCDmaRead))
					auxmc->procedure(auxmc, auxexi);
				else if (!auxdma && (auxmc->executionFlags & MCImmRead))
					auxmc->procedure(auxmc, auxexi);
			}
			break;
		case 1:  // Write Transfer
			auxdata = auxexi->data;
			auxbytes = (EXI_CR_TLEN(auxexi->cr) + 1);
			if (!auxdma) {
				while (auxbytes > 0) {
					if (auxmc->Command == MEMCARD_COMMAND_UNDEFINED || auxmc->ready) {
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
							Report(Channel::MC, "Recognized Memcard Command %02x\n", auxmc->Command);
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
				if (auxdma && (auxmc->executionFlags & MCDmaWrite))
					auxmc->procedure(auxmc, auxexi);
				else if (!auxdma && (auxmc->executionFlags & MCImmWrite))
					auxmc->procedure(auxmc, auxexi);
			}
			break;
		default:
			Halt("MC: Unknown memcard transfer type\n");
	}
}

/*
 * Checks if the memcard is connected.
 */
bool    MCIsConnected(int cardnum) {
	// Invalid memcard number. assert() is gone in Release, so this has to be a real
	// check: a bad slot number would index the memcard[] array out of bounds.
	assert((cardnum == MEMCARD_SLOTA) || (cardnum == MEMCARD_SLOTB));
	if (cardnum != MEMCARD_SLOTA && cardnum != MEMCARD_SLOTB) {
		Report(Channel::MC, "MC :: Invalid memcard slot %d\n", cardnum);
		return false;
	}
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
bool    MCCreateMemcardFile(const wchar_t* path, uint16_t memcard_id) {
	FILE* newfile;
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
			Halt("MC: Wrong card id for creating file.\n");
			return false;
	}

	newfile = nullptr;
	newfile = Util::FileOpen(path, "wb");

	if (newfile == NULL) {
		Halt("MC: Error while trying to create memcard file.\n");
		return false;
	}

	memset(newfile_buffer, MEMCARD_ERASEBYTE, Memcard_BlockSize);
	for (b = 0; b < blocks; b++) {
		if (fwrite(newfile_buffer, Memcard_BlockSize, 1, newfile) != 1) {
			Halt("MC: Error while trying to write memcard file.\n");

			fclose(newfile);
			return false;
		}
	}

	fclose(newfile);
	return true;
}

/*
 * Sets the memcard to use the specified file. If the memcard is connected,
 * it will be first disconnected (to ensure that changes are saved)
 * if param connect is TRUE, then the memcard will be connected to the new file
 */
void    MCUseFile(int cardnum, const wchar_t* path, bool connect) {

	// Invalid memcard number
	assert((cardnum == MEMCARD_SLOTA) || (cardnum == MEMCARD_SLOTB));
	if (cardnum != MEMCARD_SLOTA && cardnum != MEMCARD_SLOTB) {
		Report(Channel::MC, "MC :: Invalid memcard slot %d\n", cardnum);
		return;
	}
	if (memcard[cardnum].connected == true) MCDisconnect(cardnum);

	// Bounded copy: the path comes from the caller and the buffer is fixed size.
	memset(memcard[cardnum].filename, 0, sizeof(memcard[cardnum].filename));
	wcsncpy(memcard[cardnum].filename, path, _countof(memcard[cardnum].filename) - 1);

	if (connect == true) MCConnect(cardnum);
}

/*
 * Connects the choosen memcard to the file it was pointed at
 */
bool MCConnect(int cardnum) {
	bool ret = true;
	int i;
	switch (cardnum) {
		case MEMCARD_SLOTA:
		case MEMCARD_SLOTB:
			if (memcard[cardnum].connected /*== TRUE*/) MCDisconnect(cardnum);

			size_t memcardSize = Util::FileSize(memcard[cardnum].filename);

			memcard[cardnum].file = nullptr;
			memcard[cardnum].file = Util::FileOpen(memcard[cardnum].filename, "r+b");
			if (memcard[cardnum].file == nullptr) {
				static char slt[2] = { 'A', 'B' };

				// TODO: redirect user to memcard configure dialog ?
				Report(
					Channel::MC,
					"Couldnt open memcard (slot %c),\n"
					"location : %s\n\n"
					"Check path or file attributes.",
					slt[cardnum], Util::WstringToString(memcard[cardnum].filename).c_str()
				);
				return false;
			}

			// The file size is 64-bit: comparing it after a cast to uint32_t accepted a
			// 4 GiB + 512 KiB file as a 512 KiB card.
			for (i = 0; i < Num_Memcard_ValidSizes && (uint64_t)Memcard_ValidSizes[i] != (uint64_t)memcardSize; i++);

			if (i >= Num_Memcard_ValidSizes) {
				//          DBReport(YEL "memcard file doesnt have a valid size\n");
				Halt("Memcard Error: memcard file doesnt have a valid size\n");
				fclose(memcard[cardnum].file);
				memcard[cardnum].file = nullptr;
				return false;
			}

			// Store the validated entry, never the raw (possibly truncated) file size.
			memcard[cardnum].size = Memcard_ValidSizes[i];
			memcard[cardnum].data = (uint8_t*)malloc(memcard[cardnum].size);

			if (memcard[cardnum].data == nullptr) {
				//          DBReport(YEL "couldnt allocate enough memory for memcard\n");
				Halt("Memcard Error: couldnt allocate enough memory for memcard\n");
				fclose(memcard[cardnum].file);
				memcard[cardnum].file = nullptr;
				return false;
			}

			if (fseek(memcard[cardnum].file, 0, SEEK_SET) != 0) {
				//          DBReport(YEL "error at locating file cursor\n");
				Halt("Memcard Error: error at locating file cursor\n");
				free(memcard[cardnum].data);
				memcard[cardnum].data = nullptr;
				fclose(memcard[cardnum].file);
				memcard[cardnum].file = nullptr;
				return false;
			}

			if (fread(memcard[cardnum].data, memcard[cardnum].size, 1, memcard[cardnum].file) != 1) {
				//          DBReport(YEL "error at reading the memcard file\n");
				Halt("Memcard Error: error at reading the memcard file\n");
				free(memcard[cardnum].data);
				memcard[cardnum].data = nullptr;
				fclose(memcard[cardnum].file);
				memcard[cardnum].file = nullptr;
				return false;
			}

			/* if nothing fails... */
			memcard[cardnum].ID = ((uint16_t)0xC2) << 8 | (uint16_t)0x42; // Datel's code just for now
			memcard[cardnum].status = MEMCARD_STATUS_READY;
			memcard[cardnum].connected = true;
			Flipper::HW->exi->EXIAttach(cardnum);        // connect device

			return true;
	}
	return false;
}

/*
 * Saves the data from the memcard to disk and disconnects the choosen memcard
 */
bool MCDisconnect(int cardnum) {
	bool ret = true;
	switch (cardnum) {
		case MEMCARD_SLOTA:
		case MEMCARD_SLOTB:
			if (!memcard[cardnum].connected) break;

			// A connected card always has both, but never fseek/fwrite a null FILE*
			// or touch a null buffer: drop the state instead.
			if (memcard[cardnum].file == nullptr || memcard[cardnum].data == nullptr) {
				Report(Channel::MC, "MC :: Slot %d has no file or buffer, disconnecting\n", cardnum);
				free(memcard[cardnum].data);
				memcard[cardnum].data = nullptr;
				memcard[cardnum].ID = 0;
				memcard[cardnum].size = 0;
				memcard[cardnum].file = nullptr;
				memcard[cardnum].status = 0;
				memcard[cardnum].connected = false;
				Flipper::HW->exi->EXIDetach(cardnum);
				ret = false;
				break;
			}

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
			Flipper::HW->exi->EXIDetach(cardnum);        // disconnect device

			break;
	}
	return ret;
}

// ---------------------------------------------------------------------------
// The memory card as a device of the peripheral pool
//
// The card is an EXI device of a card slot. It has no actuators - it is storage, not an input
// device - and what the settings dialog edits about it is the file that holds the card image and
// the policy of writing it back. The protocol itself is the code above, which the card device
// simply hands to the EXI channel it is plugged into.

namespace
{
	//! The properties of the card device, as the settings dialog numbers them.
	enum
	{
		MC_PROP_FILE = 0,       //!< the file that holds the card image
		MC_PROP_SYNC_SAVE,      //!< write through to the disk on every write
		MC_PROP_SIZE,           //!< the size of the card in the file (read only)

		MC_PROP_MAX
	};

	const PeriphProperty card_properties[MC_PROP_MAX] =
	{
		{ "Card file",     PERIPH_PROP_FILE, MC_PROP_FILE },
		{ "Save policy",   PERIPH_PROP_BOOL, MC_PROP_SYNC_SAVE },
		{ "Card",          PERIPH_PROP_INFO, MC_PROP_SIZE },
	};
}

class MemoryCardDevice : public PeripheralDevice
{
	int             index = -1;
	int             port = -1;          //!< PERIPH_PORT_SLOTA / SLOTB, -1: the card is out of its slot
	std::wstring    file;
	bool            syncSave = false;
	bool            connectTried = false;   //!< the card was already handed to its slot once

	//! The slot the card is in (see MEMCARD_SLOTA / MEMCARD_SLOTB).
	int Slot() const { return port >= PERIPH_PORT_SLOTA ? port - PERIPH_PORT_SLOTA : -1; }

public:
	MemoryCardDevice(int index) : index(index) {}

	uint32_t Type() override { return PERIPH_DEVICE_MEMCARD; }

	// -----------------------------------------------------------------------

	int PropertyCount() override { return MC_PROP_MAX; }

	const PeriphProperty* Property(int index) override
	{
		return (index >= 0 && index < MC_PROP_MAX) ? &card_properties[index] : nullptr;
	}

	std::string GetProperty(int id) override
	{
		switch (id)
		{
			case MC_PROP_FILE:
				return Util::WstringToString(file);

			case MC_PROP_SYNC_SAVE:
				return syncSave ? "1" : "0";

			case MC_PROP_SIZE:
			{
				if (file.empty() || !Util::FileExists(file))
				{
					return "No card file";
				}

				// The five blocks of the card directory are not usable by a game.
				size_t size = Util::FileSize(file);
				int blocks = (int)(size / Memcard_BlockSize) - 5;
				if (blocks < 0) blocks = 0;

				char text[0x80];
				sprintf(text, "%i usable blocks (%i Kb)", blocks, (int)(size / 1024));
				return text;
			}
		}

		return "";
	}

	void SetProperty(int id, const std::string& value) override
	{
		switch (id)
		{
			case MC_PROP_FILE:
			{
				if (Util::WstringToString(file) == value)
				{
					return;
				}

				file = Util::StringToWstring(value);
				PeriphConfigSetString(index, "File", value);

				// The card that is in the slot has to be replaced at once: the new image is what the
				// guest reads from now on. MCUseFile flushes the old one first.
				if (Slot() >= 0)
				{
					memcard[Slot()].syncSave = syncSave;
					MCUseFile(Slot(), file.c_str(), true);
				}
				break;
			}

			case MC_PROP_SYNC_SAVE:
			{
				syncSave = value == "1" || value == "true";
				PeriphConfigSetInt(index, "SyncSave", syncSave ? 1 : 0);

				if (Slot() >= 0)
				{
					memcard[Slot()].syncSave = syncSave;
				}
				break;
			}
		}
	}

	// -----------------------------------------------------------------------

	void LoadConfig(int index) override
	{
		this->index = index;

		// A configuration written before the pool kept the two cards in the "memcards" section,
		// under the name of the slot (see the comment at the top of peripherals.cpp).
		const char* legacyFile = index == PERIPH_DEFAULT_SLOTB ? MemcardB_Filename_Key : MemcardA_Filename_Key;

		file = Util::StringToWstring(PeriphConfigString(index, "File",
			Util::WstringToString(GetConfigString(legacyFile, USER_MEMCARDS))));

		syncSave = PeriphConfigInt(index, "SyncSave",
			GetConfigBool(Memcard_SyncSave_Key, USER_MEMCARDS) ? 1 : 0) != 0;
	}

	// -----------------------------------------------------------------------

	void Attach(int port) override
	{
		this->port = port;
		connectTried = false;

		if (file.empty() || !Util::FileExists(file))
		{
			Report(Channel::MC, "No card file for %s\n", Peripherals::Instance().PortName(port));
			return;
		}

		// The card is pointed at its file here but it goes in when the guest first talks to the slot
		// (see ExiTransfer): the pool is built while the machine is still being put together, and a
		// card that connects to the EXI channel of a half built console has nothing to connect to.
		memcard[Slot()].syncSave = syncSave;
		MCUseFile(Slot(), file.c_str(), false);
	}

	void Detach() override
	{
		if (Slot() >= 0)
		{
			MCDisconnect(Slot());
		}

		port = -1;
		connectTried = false;
	}

	// -----------------------------------------------------------------------

	void ExiTransfer(Flipper::ExternalInterface* exi) override
	{
		// The card goes in the first time the guest addresses the slot. That is also when the OS is
		// told about it: a card that was in the slot from power-on does not raise the insertion
		// interrupt the OS watches for, but the probe of a card that is not there is a transfer too,
		// so the answer of the probe is what decides what the guest sees.
		if (Slot() >= 0 && !memcard[Slot()].connected && !connectTried)
		{
			connectTried = true;
			MCConnect(Slot());      // it reports a file it cannot open by itself
		}

		MCTransfer(exi);
	}
};

// ---------------------------------------------------------------------------
// The registration

namespace
{
	PeripheralDevice* CreateMemoryCard(int index)
	{
		return new MemoryCardDevice(index);
	}

	//! The card registers its own factory with the peripheral subsystem, so that the pool can create
	//! it. A build that does not compile this file (the unit test harness) has no memory cards.
	struct MemcardRegistrar
	{
		MemcardRegistrar()
		{
			Peripherals::RegisterFactory(PERIPH_DEVICE_MEMCARD, "Memory Card",
				"DOL-008 / DOL-014 / DOL-020, an EXI device of a card slot",
				PERIPH_BUS_EXI, CreateMemoryCard);
		}
	};

	MemcardRegistrar memcard_registrar;
}