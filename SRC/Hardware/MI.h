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

// hardware registers base (physical address)
#define HW_BASE         0x0C000000

// max known GC HW address is 0x0C008004 (fifo), so 0x8010 will be enough.
// note : it must not be greater 0xffff, unless you need to change code.
#define HW_MAX_KNOWN    0x8010

// TODO: While exploring the Flipper architecture, I misunderstood the purpose of the PI and MEM (MI) components. 
// In fact, PI is used to access Flipper's memory and registers from the Gekko side. MEM is used by various Flipper subsystems to access main memory (1T-SRAM). 
// Now all memory access handlers are in the MI.cpp module, but in theory they should be in PI.cpp. Let's leave it as it is for now.

void MIReadByte(uint32_t phys_addr, uint32_t* reg);
void MIWriteByte(uint32_t phys_addr, uint32_t data);
void MIReadHalf(uint32_t phys_addr, uint32_t* reg);
void MIWriteHalf(uint32_t phys_addr, uint32_t data);
void MIReadWord(uint32_t phys_addr, uint32_t* reg);
void MIWriteWord(uint32_t phys_addr, uint32_t data);
void MIReadDouble(uint32_t phys_addr, uint64_t* reg);
void MIWriteDouble(uint32_t phys_addr, uint64_t* data);
void MIReadBurst(uint32_t phys_addr, uint8_t burstData[32]);
void MIWriteBurst(uint32_t phys_addr, uint8_t burstData[32]);

struct MIControl
{
	uint8_t* ram;
	size_t ramSize;

    uint8_t* bootrom;       ///< Descrambled (Thank you segher, you already have a place in heaven)
    size_t bootromSize;

    bool    BootromPresent;     ///< loaded and descrambled valid bootrom

};

extern	MIControl mi;

void    MISetTrap(
    uint32_t type,                                       // 8, 16 or 32
    uint32_t addr,                                       // physical address of trap
    void (*rdTrap)(uint32_t, uint32_t*) = NULL,  // register read trap
    void (*wrTrap)(uint32_t, uint32_t) = NULL);  // register write trap
void    MIOpen(HWConfig * config);
void	MIClose();

uint8_t* MITranslatePhysicalAddress(uint32_t physAddr, size_t bytes);
