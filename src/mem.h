// Flipper main memory (1T-SRAM) controller

#pragma once

#define RAMSIZE     0x01800000      //!< amount of main memory (in bytes), 24 MBytes

#define RAMMASK     0x0fffffff		//!< physical memory mask (GC architecture is limited by 256 mb of RAM max)

// MEM PI Mapped regs

// Mem ranges (addr >> 10)
#define MEM_MARR0_START 0x00
#define MEM_MARR0_END 0x02
#define MEM_MARR1_START 0x04
#define MEM_MARR1_END 0x06
#define MEM_MARR2_START 0x08
#define MEM_MARR2_END 0x0a
#define MEM_MARR3_START 0x0c
#define MEM_MARR3_END 0x0e
#define MEM_MARR_CONTROL 0x10
#define MEM_CP_DIAL 0x12
#define MEM_TC_DIAL 0x14
#define MEM_PE_DIAL 0x16
#define MEM_PI_READ_DIAL 0x18
#define MEM_PI_WRITE_DIAL 0x1a
#define MEM_INT_ENABLE 0x1c				// MI interrupt enable
#define MEM_INT_STATUS 0x1e				// MI interrupt status
#define MEM_INT_CLR 0x20				// Clear pending MI interrupt
#define MEM_INT_ADDRL 0x22				// Interrupted Mem address
#define MEM_INT_ADDRH 0x24
#define MEM_REFRESH 0x26				// Cycles between refresh
#define MEM_CONFIG 0x28
#define MEM_LATENCY 0x2a				// Mem latency cycles (3-6)
#define MEM_RDTORD 0x2c					// Idle cycles
#define MEM_RDTOWR 0x2e
#define MEM_WRTORD 0x30
#define MEM_CP_COUNTERH 0x32
#define MEM_CP_COUNTERL 0x34
#define MEM_TC_COUNTERH 0x36
#define MEM_TC_COUNTERL 0x38
#define MEM_PI_READ_COUNTERH 0x3a
#define MEM_PI_READ_COUNTERL 0x3c
#define MEM_PI_WRITE_COUNTERH 0x3e
#define MEM_PI_WRITE_COUNTERL 0x40
#define MEM_DSP_COUNTERH 0x42
#define MEM_DSP_COUNTERL 0x44
#define MEM_IO_COUNTERH 0x46
#define MEM_IO_COUNTERL 0x48
#define MEM_VI_COUNTERH 0x4a
#define MEM_VI_COUNTERL 0x4c
#define MEM_PE_COUNTERH 0x4e
#define MEM_PE_COUNTERL 0x50
#define MEM_RF_COUNTERH 0x52			// Refresh requests
#define MEM_RF_COUNTERL 0x54
#define MEM_FI_COUNTERH 0x56			// Forced idle cycles
#define MEM_FI_COUNTERL 0x58
#define MEM_DRV_STRENGTH 0x5a
#define MEM_REFRESH_THRES 0x5c

#define MEM_MARR_SHIFT 10
#define MEM_MARR_MASK 0x03fffc00

union MEMMarrControl
{
	struct
	{
		unsigned marr0_read_enable : 1;
		unsigned marr0_write_enable : 1;
		unsigned marr1_read_enable : 1;
		unsigned marr1_write_enable : 1;
		unsigned marr2_read_enable : 1;
		unsigned marr2_write_enable : 1;
		unsigned marr3_read_enable : 1;
		unsigned marr3_write_enable : 1;
	};
	uint32_t bits;
};

union MEMIntReg
{
	struct
	{
		unsigned marr0 : 1;
		unsigned marr1 : 1;
		unsigned marr2 : 1;
		unsigned marr3 : 1;
		unsigned addr_err : 1;
	};
	uint32_t bits;
};

#pragma pack(push, 1)
union MEMCounter
{
	struct
	{
		uint16_t lo;
		uint16_t hi;
	};
	uint32_t cnt;
};
#pragma pack(pop)

struct MIState
{
	uint8_t* ram;
	size_t ramSize;
	bool log;

	uint32_t marr_start[4];
	uint32_t marr_end[4];
	MEMMarrControl marr_control;
	MEMIntReg int_enable;
	MEMIntReg int_status;

	MEMCounter cp_counter;
	MEMCounter tc_counter;
	MEMCounter pi_read_counter;
	MEMCounter pi_write_counter;
	MEMCounter dsp_counter;
	MEMCounter io_counter;
	MEMCounter vi_counter;
	MEMCounter pe_counter;
};

extern	MIState mi;

void    MIOpen(HWConfig* config);
void	MIClose();

// These calls are specifically added to show the direct connection of the MEM block, with the rest of the Flipper modules (according to the architecture).

/// <summary>
/// used by PI to read the cache line.
/// </summary>
void MIReadBurst(uint32_t mem_addr, uint8_t burstData[32]);

/// <summary>
/// used by PI to write data using GFX FIFO or for Cache Store (cache line write).
/// </summary>
void MIWriteBurst(uint32_t mem_addr, uint8_t burstData[32]);

/// <summary>
/// Used for memory access from the CP side, for Vertex Array.
/// </summary>
void* MIGetMemoryPointerForCP(uint32_t phys_addr);

/// <summary>
/// The texture unit accesses MEM to sample textures in TMEM.
/// </summary>
void* MIGetMemoryPointerForTX(uint32_t phys_addr);

/// <summary>
/// VI uses MEM to gain access to the XFB.
/// </summary>
void* MIGetMemoryPointerForVI(uint32_t phys_addr);

/// <summary>
/// Used by various IO devices (AI, EXI, SI, DI) for DMA.
/// </summary>
void* MIGetMemoryPointerForIO(uint32_t phys_addr);

/// <summary>
/// The PI requested a MEM subsystem reset by clearing the PI_CONFIG_MEMRSTB bit (active low). Do something similar to a MEM reset.
/// It's not yet clear exactly what happens when the MEM is reset, but it's clear that various FIFOs and the state machines in the MEM itself are cleared,
/// and RST is also forwarded to the 1T-SRAM chips to reset the rich internal world of Splash.
/// </summary>
void MemRst();