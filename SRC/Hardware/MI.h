// Flipper memory controller

// GC physical memory map. This is physical addresses, not effective.
/*/
    00000000  24MB  Main Memory (RAM)
    08000000   2MB  Embedded Framebuffer (EFB)
    0C000000        Command Processor (CP)
    0C001000        Pixel Engine (PE)
    0C002000        Video Interface (VI)
    0C003000        Processor Interface (PI)
    0C004000        Memory Interface (MI)
    0C005000        DSP and DMA Audio Interface (AID)
    0C006000        DVD Interface (DI)
    0C006400        Serial Interface (SI)
    0C006800        External Interface (EXI)
    0C006C00        Audio Streaming Interface (AIS)
    0C008000        PI FIFO (GX)
    FFF00000   2MB  Boot ROM

    EFB - this is not straight "direct" access. reads and writes
    are passing through some Flipper logic, so its just simulation of
    direct access.

    Hardware Registers (HW) are located above 0x0C000000. Dolwin
    memory engine is using hardware traps, which are handling all
    registers operations. traps are abstracting HW from Emulator,
    so basically any Hardware will work with Dolwin, with minimal
    modifications of Emulator core.

    Boot ROM is available only during CPU reset. after reset,
    execution will begin from 0xFFF00100 reset vector, with
    enabled bootrom EXI reading logic. small program, called
    "BS" (Bootstrap?) will run and load IPL menu up to
    0x81300000 address (already effective!). then IPL menu (or "BS2")
    will run, with disabled EXI scrambler.
    Dolwin is simulating all BS and BS2 activities before running
    any DVD/executable. see HighLevel\Bootrom.cpp for details.
/*/

#pragma once

// amount of main memory (in bytes)
#define RAMSIZE     0x01800000      // 24 mb

// physical memory mask (for simple translation mode).
// 0x0fffffff, because GC architecture is limited by 256 mb of RAM
#define RAMMASK     0x0fffffff

// max known GC HW address is 0x0C008004 (fifo), so 0x8010 will be enough.
// note : it must not be greater 0xffff, unless you need to change code.
#define HW_MAX_KNOWN    0x8010

void __fastcall MIReadByte(uint32_t phys_addr, uint32_t* reg);
void __fastcall MIWriteByte(uint32_t phys_addr, uint32_t data);
void __fastcall MIReadHalf(uint32_t phys_addr, uint32_t* reg);
void __fastcall MIWriteHalf(uint32_t phys_addr, uint32_t data);
void __fastcall MIReadWord(uint32_t phys_addr, uint32_t* reg);
void __fastcall MIWriteWord(uint32_t phys_addr, uint32_t data);
void __fastcall MIReadDouble(uint32_t phys_addr, uint64_t* reg);
void __fastcall MIWriteDouble(uint32_t phys_addr, uint64_t* data);

typedef struct _MIControl
{
	uint8_t* ram;
	size_t ramSize;
} MIControl;

extern	MIControl mi;

void    MISetTrap(
    uint32_t type,                                       // 8, 16 or 32
    uint32_t addr,                                       // physical address of trap
    void (__fastcall* rdTrap)(uint32_t, uint32_t*) = NULL,  // register read trap
    void (__fastcall* wrTrap)(uint32_t, uint32_t) = NULL);  // register write trap
void    MIOpen(HWConfig * config);
void	MIClose();
