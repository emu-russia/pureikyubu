// Flipper main memory (1T-SRAM) controller

#pragma once

// amount of main memory (in bytes)
#define RAMSIZE     0x01800000      // 24 mb

// physical memory mask
// 0x0fffffff, because GC architecture is limited by 256 mb of RAM
#define RAMMASK     0x0fffffff

// Bootrom size (in bytes)
#define BOOTROM_SIZE    (2*1024*1024)
// Bootrom start address
#define BOOTROM_START_ADDRESS 0xfff00000

// MEM PI Mapped regs

enum MEM_PIMappedReg : size_t
{
	// Mem ranges (addr >> 10)
	MEM_MARR0_START_ID = 0,
	MEM_MARR0_END_ID,
	MEM_MARR1_START_ID,
	MEM_MARR1_END_ID,
	MEM_MARR2_START_ID,
	MEM_MARR2_END_ID,
	MEM_MARR3_START_ID,
	MEM_MARR3_END_ID,
	MEM_MARR_CONTROL_ID,
	MEM_CP_DIAL_ID,
	MEM_TC_DIAL_ID,
	MEM_PE_DIAL_ID,
	MEM_PI_READ_DIAL_ID,
	MEM_PI_WRITE_DIAL_ID,
	MEM_INT_ENABLE_ID,				// MI interrupt enable
	MEM_INT_STATUS_ID,				// MI interrupt status
	MEM_INT_CLR_ID,					// Clear pending MI interrupt
	MEM_INT_ADDRL_ID,				// Interrupted Mem address
	MEM_INT_ADDRH_ID,
	MEM_REFRESH_ID,					// Cycles between refresh
	MEM_CONFIG_ID,
	MEM_LATENCY_ID,					// Mem latency cycles (3-6)
	MEM_RDTORD_ID,					// Idle cycles
	MEM_RDTOWR_ID,
	MEM_WRTORD_ID,
	MEM_CP_COUNTERH_ID,
	MEM_CP_COUNTERL_ID,
	MEM_TC_COUNTERH_ID,
	MEM_TC_COUNTERL_ID,
	MEM_PI_READ_COUNTERH_ID,
	MEM_PI_READ_COUNTERL_ID,
	MEM_PI_WRITE_COUNTERH_ID,
	MEM_PI_WRITE_COUNTERL_ID,
	MEM_DSP_COUNTERH_ID,
	MEM_DSP_COUNTERL_ID,
	MEM_IO_COUNTERH_ID,
	MEM_IO_COUNTERL_ID,
	MEM_VI_COUNTERH_ID,
	MEM_VI_COUNTERL_ID,
	MEM_PE_COUNTERH_ID,
	MEM_PE_COUNTERL_ID,
	MEM_RF_COUNTERH_ID,				// Refresh requests
	MEM_RF_COUNTERL_ID,
	MEM_FI_COUNTERH_ID,				// Forced idle cycles
	MEM_FI_COUNTERL_ID,
	MEM_DRV_STRENGTH_ID,
	MEM_REFRESH_THRES_ID,
};

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

struct MIControl
{
	uint8_t* ram;
	size_t ramSize;

	// TODO: Move to bootrtc.cpp
	uint8_t* bootrom;       // Descrambled (Thank you segher, you already have a place in heaven)
	size_t bootromSize;
	bool    BootromPresent;     // loaded and descrambled valid bootrom

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

extern	MIControl mi;

void    MIOpen(HWConfig* config);
void	MIClose();

uint8_t* MITranslatePhysicalAddress(uint32_t physAddr, size_t bytes);

// These calls are specifically added to show the direct connection of the MEM block, with the rest of the Flipper modules (according to the architecture).

void* MIGetMemoryPointerForCP(uint32_t phys_addr);

void* MIGetMemoryPointerForTX(uint32_t phys_addr);

void* MIGetMemoryPointerForVI(uint32_t phys_addr);
