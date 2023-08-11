// This component deals with emulation of the external Macronix chip where Bootrom and RTC are located.

#pragma once

// SRAM structure layout. see YAGCD for details.
struct SRAM
{
	uint16_t     checkSum;
	uint16_t     checkSumInv;
	uint32_t     ead0;
	uint32_t     ead1;
	uint32_t     counterBias;
	int8_t       displayOffsetH;
	uint8_t      ntd;
	uint8_t      language;
	uint8_t      flags;
	uint8_t      dummy[44];          // reserved for future        
};

// bootrom encoded font sizes
#define ANSI_SIZE   0x3000
#define SJIS_SIZE   0x4D000

// location of SRAM dump in filesystem 
#define SRAM_FILE   L"./Data/sram.bin"

void SRAMLoad(SRAM* s);
void SRAMSave(SRAM* s);

void RTCUpdate();

void FontLoad(uint8_t** font, uint32_t fontsize, wchar_t* filename);
void FontUnload(uint8_t** font);

void MXTransfer();		// to EXI

void IPLDescrambler(uint8_t* data, size_t size);

void BootROM(bool dvd, bool rtc, uint32_t consoleVer);

/// <summary>
/// Checks that the bootstrap (if present) is of PAL revision. This is determined by the "PAL" substring in the first unencoded 0x100 bytes with copyright.
/// </summary>
bool IsBootromPALRevision();

void LoadBootrom(HWConfig* config);
