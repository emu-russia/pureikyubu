// Flipper main memory (1T-SRAM) controller

#pragma once

// amount of main memory (in bytes)
#define RAMSIZE     0x01800000      // 24 mb

// Bootrom size (in bytes)
#define BOOTROM_SIZE    (2*1024*1024)

// Bootrom start address
#define BOOTROM_START_ADDRESS 0xfff00000

// physical memory mask (for simple translation mode).
// 0x0fffffff, because GC architecture is limited by 256 mb of RAM
#define RAMMASK     0x0fffffff

struct MIControl
{
    uint8_t* ram;
    size_t ramSize;

    uint8_t* bootrom;       ///< Descrambled (Thank you segher, you already have a place in heaven)
    size_t bootromSize;

    bool    BootromPresent;     ///< loaded and descrambled valid bootrom

};

extern	MIControl mi;

void    MIOpen(HWConfig* config);
void	MIClose();

uint8_t* MITranslatePhysicalAddress(uint32_t physAddr, size_t bytes);
