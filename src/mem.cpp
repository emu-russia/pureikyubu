// MI - memory interface.
//
// MI is used in __OSInitMemoryProtection and by few games for debug.
#include "pch.h"

using namespace Debug;

MIState mi;

// stubs for MI registers
static void no_write(uint32_t addr, uint32_t data, void *ctx) {}
static void no_read(uint32_t addr, uint32_t* reg, void* ctx) { *reg = 0; }

static void MEM_WriteMarrStart(uint32_t addr, uint32_t data, void* ctx)
{
	switch (addr & 0xFF)
	{
		case MEM_MARR0_START: mi.marr_start[0] = data; break;
		case MEM_MARR1_START: mi.marr_start[1] = data; break;
		case MEM_MARR2_START: mi.marr_start[2] = data; break;
		case MEM_MARR3_START: mi.marr_start[3] = data; break;
		default: break;
	}
}

static void MEM_ReadMarrStart(uint32_t addr, uint32_t* reg, void* ctx)
{
	switch (addr & 0xFF)
	{
		case MEM_MARR0_START: *reg = mi.marr_start[0]; break;
		case MEM_MARR1_START: *reg = mi.marr_start[1]; break;
		case MEM_MARR2_START: *reg = mi.marr_start[2]; break;
		case MEM_MARR3_START: *reg = mi.marr_start[3]; break;
		default: break;
	}
}

static void MEM_WriteMarrEnd(uint32_t addr, uint32_t data, void* ctx)
{
	switch (addr & 0xFF)
	{
		case MEM_MARR0_END: mi.marr_end[0] = data; break;
		case MEM_MARR1_END: mi.marr_end[1] = data; break;
		case MEM_MARR2_END: mi.marr_end[2] = data; break;
		case MEM_MARR3_END: mi.marr_end[3] = data; break;
		default: break;
	}
}

static void MEM_ReadMarrEnd(uint32_t addr, uint32_t* reg, void* ctx)
{
	switch (addr & 0xFF)
	{
		case MEM_MARR0_END: *reg = mi.marr_end[0]; break;
		case MEM_MARR1_END: *reg = mi.marr_end[1]; break;
		case MEM_MARR2_END: *reg = mi.marr_end[2]; break;
		case MEM_MARR3_END: *reg = mi.marr_end[3]; break;
		default: break;
	}
}

static void MEM_WriteMarrControl(uint32_t addr, uint32_t data, void* ctx)
{
	mi.marr_control.bits = data;

	Report(Channel::MI, "MARR Control (MARR0-3 read/write enabled): %d/%d, %d/%d, %d/%d, %d/%d\n",
		mi.marr_control.marr0_read_enable, mi.marr_control.marr0_write_enable,
		mi.marr_control.marr1_read_enable, mi.marr_control.marr1_write_enable,
		mi.marr_control.marr2_read_enable, mi.marr_control.marr2_write_enable,
		mi.marr_control.marr3_read_enable, mi.marr_control.marr3_write_enable);
}

static void MEM_ReadMarrControl(uint32_t addr, uint32_t* reg, void* ctx)
{
	*reg = mi.marr_control.bits;
}

static void MEM_WriteIntEnable(uint32_t addr, uint32_t data, void* ctx)
{
	mi.int_enable.bits = data;
}

static void MEM_ReadIntEnable(uint32_t addr, uint32_t* reg, void* ctx)
{
	*reg = mi.int_enable.bits;
}

static void MEM_ReadIntStatus(uint32_t addr, uint32_t* reg, void* ctx)
{
	*reg = mi.int_status.bits;
}

static void MEM_WriteIntClear(uint32_t addr, uint32_t data, void* ctx)
{
	mi.int_status.bits &= ~(data);

	if ((mi.int_status.bits & mi.int_enable.bits) != 0) {
		PIAssertInt(PI_INTERRUPT_MEM);
	}
	else {
		PIClearInt(PI_INTERRUPT_MEM);
	}
}

static void MEM_WriteCounter(uint32_t addr, uint32_t data, void* ctx)
{
	switch (addr & 0xFF)
	{
		case MEM_CP_COUNTERH: mi.cp_counter.hi = data; break;
		case MEM_CP_COUNTERL: mi.cp_counter.lo = data; break;
		case MEM_TC_COUNTERH: mi.tc_counter.hi = data; break;
		case MEM_TC_COUNTERL: mi.tc_counter.lo = data; break;
		case MEM_PI_READ_COUNTERH: mi.pi_read_counter.hi = data; break;
		case MEM_PI_READ_COUNTERL: mi.pi_read_counter.lo = data; break;
		case MEM_PI_WRITE_COUNTERH: mi.pi_write_counter.hi = data; break;
		case MEM_PI_WRITE_COUNTERL: mi.pi_write_counter.lo = data; break;
		case MEM_DSP_COUNTERH: mi.dsp_counter.hi = data; break;
		case MEM_DSP_COUNTERL: mi.dsp_counter.lo = data; break;
		case MEM_IO_COUNTERH: mi.io_counter.hi = data; break;
		case MEM_IO_COUNTERL: mi.io_counter.lo = data; break;
		case MEM_VI_COUNTERH: mi.vi_counter.hi = data; break;
		case MEM_VI_COUNTERL: mi.vi_counter.lo = data; break;
		case MEM_PE_COUNTERH: mi.pe_counter.hi = data; break;
		case MEM_PE_COUNTERL: mi.pe_counter.lo = data; break;
		default:
			break;
	}
}

static void MEM_ReadCounter(uint32_t addr, uint32_t* reg, void* ctx)
{
	switch (addr & 0xFF)
	{
		case MEM_CP_COUNTERH: *reg = mi.cp_counter.hi; break;
		case MEM_CP_COUNTERL: *reg = mi.cp_counter.lo; break;
		case MEM_TC_COUNTERH: *reg = mi.tc_counter.hi; break;
		case MEM_TC_COUNTERL: *reg = mi.tc_counter.lo; break;
		case MEM_PI_READ_COUNTERH: *reg = mi.pi_read_counter.hi; break;
		case MEM_PI_READ_COUNTERL: *reg = mi.pi_read_counter.lo; break;
		case MEM_PI_WRITE_COUNTERH: *reg = mi.pi_write_counter.hi; break;
		case MEM_PI_WRITE_COUNTERL: *reg = mi.pi_write_counter.lo; break;
		case MEM_DSP_COUNTERH: *reg = mi.dsp_counter.hi; break;
		case MEM_DSP_COUNTERL: *reg = mi.dsp_counter.lo; break;
		case MEM_IO_COUNTERH: *reg = mi.io_counter.hi; break;
		case MEM_IO_COUNTERL: *reg = mi.io_counter.lo; break;
		case MEM_VI_COUNTERH: *reg = mi.vi_counter.hi; break;
		case MEM_VI_COUNTERL: *reg = mi.vi_counter.lo; break;
		case MEM_PE_COUNTERH: *reg = mi.pe_counter.hi; break;
		case MEM_PE_COUNTERL: *reg = mi.pe_counter.lo; break;
		default:
			*reg = 0;
			break;
	}
}

void MIOpen(HWConfig* config)
{
	Report(Channel::MI, "Flipper memory interface\n");

	memset(&mi, 0, sizeof(mi));

	mi.ramSize = config->ramsize;
	mi.ram = new uint8_t[mi.ramSize];

	memset(mi.ram, 0, mi.ramSize);

	mi.log = config->mi_log;

	for (uint32_t ofs = 0; ofs < 0x100; ofs += 2)
	{
		PISetTrap(PI_REGSPACE_MEM | ofs, no_read, no_write);
	}

	PISetTrap(PI_REGSPACE_MEM | MEM_MARR0_START, MEM_ReadMarrStart, MEM_WriteMarrStart);
	PISetTrap(PI_REGSPACE_MEM | MEM_MARR1_START, MEM_ReadMarrStart, MEM_WriteMarrStart);
	PISetTrap(PI_REGSPACE_MEM | MEM_MARR2_START, MEM_ReadMarrStart, MEM_WriteMarrStart);
	PISetTrap(PI_REGSPACE_MEM | MEM_MARR3_START, MEM_ReadMarrStart, MEM_WriteMarrStart);

	PISetTrap(PI_REGSPACE_MEM | MEM_MARR0_END, MEM_ReadMarrEnd, MEM_WriteMarrEnd);
	PISetTrap(PI_REGSPACE_MEM | MEM_MARR1_END, MEM_ReadMarrEnd, MEM_WriteMarrEnd);
	PISetTrap(PI_REGSPACE_MEM | MEM_MARR2_END, MEM_ReadMarrEnd, MEM_WriteMarrEnd);
	PISetTrap(PI_REGSPACE_MEM | MEM_MARR3_END, MEM_ReadMarrEnd, MEM_WriteMarrEnd);

	PISetTrap(PI_REGSPACE_MEM | MEM_MARR_CONTROL, MEM_ReadMarrControl, MEM_WriteMarrControl);

	PISetTrap(PI_REGSPACE_MEM | MEM_INT_ENABLE, MEM_ReadIntEnable, MEM_WriteIntEnable);
	PISetTrap(PI_REGSPACE_MEM | MEM_INT_STATUS, MEM_ReadIntStatus, nullptr);
	PISetTrap(PI_REGSPACE_MEM | MEM_INT_CLR, nullptr, MEM_WriteIntClear);

	PISetTrap(PI_REGSPACE_MEM | MEM_MARR0_START, MEM_ReadMarrStart, MEM_WriteMarrStart);

	PISetTrap(PI_REGSPACE_MEM | MEM_CP_COUNTERH, MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(PI_REGSPACE_MEM | MEM_CP_COUNTERL, MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(PI_REGSPACE_MEM | MEM_TC_COUNTERH, MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(PI_REGSPACE_MEM | MEM_TC_COUNTERL, MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(PI_REGSPACE_MEM | MEM_PI_READ_COUNTERH, MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(PI_REGSPACE_MEM | MEM_PI_READ_COUNTERL, MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(PI_REGSPACE_MEM | MEM_PI_WRITE_COUNTERH, MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(PI_REGSPACE_MEM | MEM_PI_WRITE_COUNTERL, MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(PI_REGSPACE_MEM | MEM_DSP_COUNTERH, MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(PI_REGSPACE_MEM | MEM_DSP_COUNTERL, MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(PI_REGSPACE_MEM | MEM_IO_COUNTERH, MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(PI_REGSPACE_MEM | MEM_IO_COUNTERL, MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(PI_REGSPACE_MEM | MEM_VI_COUNTERH, MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(PI_REGSPACE_MEM | MEM_VI_COUNTERL, MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(PI_REGSPACE_MEM | MEM_PE_COUNTERH, MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(PI_REGSPACE_MEM | MEM_PE_COUNTERL, MEM_ReadCounter, MEM_WriteCounter);
}

void MIClose()
{
	if (mi.ram)
	{
		delete[] mi.ram;
		mi.ram = nullptr;
	}
}

// The counters are not emulated accurately, but this is not required.

void* MIGetMemoryPointerForCP(uint32_t phys_addr)
{
	mi.cp_counter.cnt++;
	return &mi.ram[phys_addr & RAMMASK];
}

void* MIGetMemoryPointerForTX(uint32_t phys_addr)
{
	mi.tc_counter.cnt++;
	return &mi.ram[phys_addr & RAMMASK];
}

void* MIGetMemoryPointerForVI(uint32_t phys_addr)
{
	mi.vi_counter.cnt++;
	if (phys_addr >= mi.ramSize) return nullptr;
	else return &mi.ram[phys_addr & RAMMASK];
}

void* MIGetMemoryPointerForIO(uint32_t phys_addr)
{
	mi.io_counter.cnt++;
	if (phys_addr >= mi.ramSize) return nullptr;
	else return &mi.ram[phys_addr & RAMMASK];
}

void MIReadBurst(uint32_t mem_addr, uint8_t burstData[32])
{
	memcpy(burstData, &mi.ram[mem_addr], 32);
	mi.pi_read_counter.cnt++;
}

void MIWriteBurst(uint32_t mem_addr, uint8_t burstData[32])
{
	memcpy(&mi.ram[mem_addr], burstData, 32);
	mi.pi_write_counter.cnt++;
}

void MemRst()
{
	// Let's clear the contents of Splash... I guess that will count as a MEM reset :)
	memset(mi.ram, 0, mi.ramSize);
}