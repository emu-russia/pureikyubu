// MI - memory interface.
//
// MI is used in __OSInitMemoryProtection and by few games for debug.
#include "pch.h"

using namespace Debug;

#define RAMSIZE     0x01800000      //!< amount of main memory (in bytes), 24 MBytes

#define RAMMASK     0x0fffffff		//!< physical memory mask (GC architecture is limited by 256 mb of RAM max)

namespace Flipper
{
	// stubs for MI registers
	void MemoryInterface::mi_no_write(uint32_t addr, uint32_t data, void* ctx) {}
	void MemoryInterface::mi_no_read(uint32_t addr, uint32_t* reg, void* ctx) { *reg = 0; }

	void MemoryInterface::MEM_WriteMarrStart(uint32_t addr, uint32_t data, void* ctx)
	{
		MemoryInterface* mi = (MemoryInterface*)ctx;
		switch (addr & 0xFF)
		{
			case MEM_MARR0_START: mi->mi.marr_start[0] = data; break;
			case MEM_MARR1_START: mi->mi.marr_start[1] = data; break;
			case MEM_MARR2_START: mi->mi.marr_start[2] = data; break;
			case MEM_MARR3_START: mi->mi.marr_start[3] = data; break;
			default: break;
		}
	}

	void MemoryInterface::MEM_ReadMarrStart(uint32_t addr, uint32_t* reg, void* ctx)
	{
		MemoryInterface* mi = (MemoryInterface*)ctx;
		switch (addr & 0xFF)
		{
			case MEM_MARR0_START: *reg = mi->mi.marr_start[0]; break;
			case MEM_MARR1_START: *reg = mi->mi.marr_start[1]; break;
			case MEM_MARR2_START: *reg = mi->mi.marr_start[2]; break;
			case MEM_MARR3_START: *reg = mi->mi.marr_start[3]; break;
			default: break;
		}
	}

	void MemoryInterface::MEM_WriteMarrEnd(uint32_t addr, uint32_t data, void* ctx)
	{
		MemoryInterface* mi = (MemoryInterface*)ctx;
		switch (addr & 0xFF)
		{
			case MEM_MARR0_END: mi->mi.marr_end[0] = data; break;
			case MEM_MARR1_END: mi->mi.marr_end[1] = data; break;
			case MEM_MARR2_END: mi->mi.marr_end[2] = data; break;
			case MEM_MARR3_END: mi->mi.marr_end[3] = data; break;
			default: break;
		}
	}

	void MemoryInterface::MEM_ReadMarrEnd(uint32_t addr, uint32_t* reg, void* ctx)
	{
		MemoryInterface* mi = (MemoryInterface*)ctx;
		switch (addr & 0xFF)
		{
			case MEM_MARR0_END: *reg = mi->mi.marr_end[0]; break;
			case MEM_MARR1_END: *reg = mi->mi.marr_end[1]; break;
			case MEM_MARR2_END: *reg = mi->mi.marr_end[2]; break;
			case MEM_MARR3_END: *reg = mi->mi.marr_end[3]; break;
			default: break;
		}
	}

	void MemoryInterface::MEM_WriteMarrControl(uint32_t addr, uint32_t data, void* ctx)
	{
		MemoryInterface* mi = (MemoryInterface*)ctx;
		mi->mi.marr_control.bits = data;

		Report(Channel::MI, "MARR Control (MARR0-3 read/write enabled): %d/%d, %d/%d, %d/%d, %d/%d\n",
			mi->mi.marr_control.marr0_read_enable, mi->mi.marr_control.marr0_write_enable,
			mi->mi.marr_control.marr1_read_enable, mi->mi.marr_control.marr1_write_enable,
			mi->mi.marr_control.marr2_read_enable, mi->mi.marr_control.marr2_write_enable,
			mi->mi.marr_control.marr3_read_enable, mi->mi.marr_control.marr3_write_enable);
	}

	void MemoryInterface::MEM_ReadMarrControl(uint32_t addr, uint32_t* reg, void* ctx)
	{
		MemoryInterface* mi = (MemoryInterface*)ctx;
		*reg = mi->mi.marr_control.bits;
	}

	void MemoryInterface::MEM_WriteIntEnable(uint32_t addr, uint32_t data, void* ctx)
	{
		MemoryInterface* mi = (MemoryInterface*)ctx;
		mi->mi.int_enable.bits = data;
	}

	void MemoryInterface::MEM_ReadIntEnable(uint32_t addr, uint32_t* reg, void* ctx)
	{
		MemoryInterface* mi = (MemoryInterface*)ctx;
		*reg = mi->mi.int_enable.bits;
	}

	void MemoryInterface::MEM_ReadIntStatus(uint32_t addr, uint32_t* reg, void* ctx)
	{
		MemoryInterface* mi = (MemoryInterface*)ctx;
		*reg = mi->mi.int_status.bits;
	}

	void MemoryInterface::MEM_WriteIntClear(uint32_t addr, uint32_t data, void* ctx)
	{
		MemoryInterface* mi = (MemoryInterface*)ctx;
		mi->mi.int_status.bits &= ~(data);

		if ((mi->mi.int_status.bits & mi->mi.int_enable.bits) != 0) {
			PIAssertInt(PI_INTERRUPT_MEM);
		}
		else {
			PIClearInt(PI_INTERRUPT_MEM);
		}
	}

	void MemoryInterface::MEM_WriteCounter(uint32_t addr, uint32_t data, void* ctx)
	{
		MemoryInterface* mi = (MemoryInterface*)ctx;
		switch (addr & 0xFF)
		{
			case MEM_CP_COUNTERH: mi->mi.cp_counter.hi = data; break;
			case MEM_CP_COUNTERL: mi->mi.cp_counter.lo = data; break;
			case MEM_TC_COUNTERH: mi->mi.tc_counter.hi = data; break;
			case MEM_TC_COUNTERL: mi->mi.tc_counter.lo = data; break;
			case MEM_PI_READ_COUNTERH: mi->mi.pi_read_counter.hi = data; break;
			case MEM_PI_READ_COUNTERL: mi->mi.pi_read_counter.lo = data; break;
			case MEM_PI_WRITE_COUNTERH: mi->mi.pi_write_counter.hi = data; break;
			case MEM_PI_WRITE_COUNTERL: mi->mi.pi_write_counter.lo = data; break;
			case MEM_DSP_COUNTERH: mi->mi.dsp_counter.hi = data; break;
			case MEM_DSP_COUNTERL: mi->mi.dsp_counter.lo = data; break;
			case MEM_IO_COUNTERH: mi->mi.io_counter.hi = data; break;
			case MEM_IO_COUNTERL: mi->mi.io_counter.lo = data; break;
			case MEM_VI_COUNTERH: mi->mi.vi_counter.hi = data; break;
			case MEM_VI_COUNTERL: mi->mi.vi_counter.lo = data; break;
			case MEM_PE_COUNTERH: mi->mi.pe_counter.hi = data; break;
			case MEM_PE_COUNTERL: mi->mi.pe_counter.lo = data; break;
			default:
				break;
		}
	}

	void MemoryInterface::MEM_ReadCounter(uint32_t addr, uint32_t* reg, void* ctx)
	{
		MemoryInterface* mi = (MemoryInterface*)ctx;
		switch (addr & 0xFF)
		{
			case MEM_CP_COUNTERH: *reg = mi->mi.cp_counter.hi; break;
			case MEM_CP_COUNTERL: *reg = mi->mi.cp_counter.lo; break;
			case MEM_TC_COUNTERH: *reg = mi->mi.tc_counter.hi; break;
			case MEM_TC_COUNTERL: *reg = mi->mi.tc_counter.lo; break;
			case MEM_PI_READ_COUNTERH: *reg = mi->mi.pi_read_counter.hi; break;
			case MEM_PI_READ_COUNTERL: *reg = mi->mi.pi_read_counter.lo; break;
			case MEM_PI_WRITE_COUNTERH: *reg = mi->mi.pi_write_counter.hi; break;
			case MEM_PI_WRITE_COUNTERL: *reg = mi->mi.pi_write_counter.lo; break;
			case MEM_DSP_COUNTERH: *reg = mi->mi.dsp_counter.hi; break;
			case MEM_DSP_COUNTERL: *reg = mi->mi.dsp_counter.lo; break;
			case MEM_IO_COUNTERH: *reg = mi->mi.io_counter.hi; break;
			case MEM_IO_COUNTERL: *reg = mi->mi.io_counter.lo; break;
			case MEM_VI_COUNTERH: *reg = mi->mi.vi_counter.hi; break;
			case MEM_VI_COUNTERL: *reg = mi->mi.vi_counter.lo; break;
			case MEM_PE_COUNTERH: *reg = mi->mi.pe_counter.hi; break;
			case MEM_PE_COUNTERL: *reg = mi->mi.pe_counter.lo; break;
			default:
				*reg = 0;
				break;
		}
	}

	MemoryInterface::MemoryInterface(HWConfig* config)
	{
		Report(Channel::MI, "Flipper memory interface\n");

		memset(&mi, 0, sizeof(mi));

		mi.ramSize = config->ramsize;

		// Check that the memory size is 24 or 48 MB. If not, set the default to 24.
		// Technically the memory is limited to 256 MByte, but it is known that there were only 24 and 48 MByte configurations.
		if (!(mi.ramSize == RAMSIZE || mi.ramSize == 2 * RAMSIZE)) {
			mi.ramSize = RAMSIZE;
		}

		mi.ram = new uint8_t[mi.ramSize];

		memset(mi.ram, 0, mi.ramSize);

		mi.log = config->mi_log;

		for (uint32_t ofs = 0; ofs < MEM_REG_MAX; ofs += 2)
		{
			PISetTrap(PI_REGSPACE_MEM | ofs, mi_no_read, mi_no_write, this);
		}

		PISetTrap(PI_REGSPACE_MEM | MEM_MARR0_START, MEM_ReadMarrStart, MEM_WriteMarrStart, this);
		PISetTrap(PI_REGSPACE_MEM | MEM_MARR1_START, MEM_ReadMarrStart, MEM_WriteMarrStart, this);
		PISetTrap(PI_REGSPACE_MEM | MEM_MARR2_START, MEM_ReadMarrStart, MEM_WriteMarrStart, this);
		PISetTrap(PI_REGSPACE_MEM | MEM_MARR3_START, MEM_ReadMarrStart, MEM_WriteMarrStart, this);

		PISetTrap(PI_REGSPACE_MEM | MEM_MARR0_END, MEM_ReadMarrEnd, MEM_WriteMarrEnd, this);
		PISetTrap(PI_REGSPACE_MEM | MEM_MARR1_END, MEM_ReadMarrEnd, MEM_WriteMarrEnd, this);
		PISetTrap(PI_REGSPACE_MEM | MEM_MARR2_END, MEM_ReadMarrEnd, MEM_WriteMarrEnd, this);
		PISetTrap(PI_REGSPACE_MEM | MEM_MARR3_END, MEM_ReadMarrEnd, MEM_WriteMarrEnd, this);

		PISetTrap(PI_REGSPACE_MEM | MEM_MARR_CONTROL, MEM_ReadMarrControl, MEM_WriteMarrControl, this);

		PISetTrap(PI_REGSPACE_MEM | MEM_INT_ENABLE, MEM_ReadIntEnable, MEM_WriteIntEnable, this);
		PISetTrap(PI_REGSPACE_MEM | MEM_INT_STATUS, MEM_ReadIntStatus, nullptr, this);
		PISetTrap(PI_REGSPACE_MEM | MEM_INT_CLR, nullptr, MEM_WriteIntClear, this);

		PISetTrap(PI_REGSPACE_MEM | MEM_CP_COUNTERH, MEM_ReadCounter, MEM_WriteCounter, this);
		PISetTrap(PI_REGSPACE_MEM | MEM_CP_COUNTERL, MEM_ReadCounter, MEM_WriteCounter, this);
		PISetTrap(PI_REGSPACE_MEM | MEM_TC_COUNTERH, MEM_ReadCounter, MEM_WriteCounter, this);
		PISetTrap(PI_REGSPACE_MEM | MEM_TC_COUNTERL, MEM_ReadCounter, MEM_WriteCounter, this);
		PISetTrap(PI_REGSPACE_MEM | MEM_PI_READ_COUNTERH, MEM_ReadCounter, MEM_WriteCounter, this);
		PISetTrap(PI_REGSPACE_MEM | MEM_PI_READ_COUNTERL, MEM_ReadCounter, MEM_WriteCounter, this);
		PISetTrap(PI_REGSPACE_MEM | MEM_PI_WRITE_COUNTERH, MEM_ReadCounter, MEM_WriteCounter, this);
		PISetTrap(PI_REGSPACE_MEM | MEM_PI_WRITE_COUNTERL, MEM_ReadCounter, MEM_WriteCounter, this);
		PISetTrap(PI_REGSPACE_MEM | MEM_DSP_COUNTERH, MEM_ReadCounter, MEM_WriteCounter, this);
		PISetTrap(PI_REGSPACE_MEM | MEM_DSP_COUNTERL, MEM_ReadCounter, MEM_WriteCounter, this);
		PISetTrap(PI_REGSPACE_MEM | MEM_IO_COUNTERH, MEM_ReadCounter, MEM_WriteCounter, this);
		PISetTrap(PI_REGSPACE_MEM | MEM_IO_COUNTERL, MEM_ReadCounter, MEM_WriteCounter, this);
		PISetTrap(PI_REGSPACE_MEM | MEM_VI_COUNTERH, MEM_ReadCounter, MEM_WriteCounter, this);
		PISetTrap(PI_REGSPACE_MEM | MEM_VI_COUNTERL, MEM_ReadCounter, MEM_WriteCounter, this);
		PISetTrap(PI_REGSPACE_MEM | MEM_PE_COUNTERH, MEM_ReadCounter, MEM_WriteCounter, this);
		PISetTrap(PI_REGSPACE_MEM | MEM_PE_COUNTERL, MEM_ReadCounter, MEM_WriteCounter, this);
	}

	MemoryInterface::~MemoryInterface()
	{
		if (mi.ram)
		{
			delete[] mi.ram;
			mi.ram = nullptr;
		}
	}

	// The counters are not emulated accurately, but this is not required.

	void* MemoryInterface::MIGetMemoryPointerForPI(uint32_t phys_addr)
	{
		// pi r/w counters are not emulated.
		if (phys_addr >= mi.ramSize) return nullptr;
		else return &mi.ram[phys_addr & RAMMASK];
	}

	void* MemoryInterface::MIGetMemoryPointerForCP(uint32_t phys_addr)
	{
		mi.cp_counter.cnt++;
		if (phys_addr >= mi.ramSize) return nullptr;
		else return &mi.ram[phys_addr & RAMMASK];
	}

	void* MemoryInterface::MIGetMemoryPointerForTX(uint32_t phys_addr)
	{
		mi.tc_counter.cnt++;
		if (phys_addr >= mi.ramSize) return nullptr;
		else return &mi.ram[phys_addr & RAMMASK];
	}

	void* MemoryInterface::MIGetMemoryPointerForVI(uint32_t phys_addr)
	{
		mi.vi_counter.cnt++;
		if (phys_addr >= mi.ramSize) return nullptr;
		else return &mi.ram[phys_addr & RAMMASK];
	}

	void* MemoryInterface::MIGetMemoryPointerForDSP(uint32_t phys_addr)
	{
		mi.dsp_counter.cnt++;
		if (phys_addr >= mi.ramSize) return nullptr;
		else return &mi.ram[phys_addr & RAMMASK];
	}

	void* MemoryInterface::MIGetMemoryPointerForIO(uint32_t phys_addr)
	{
		mi.io_counter.cnt++;
		if (phys_addr >= mi.ramSize) return nullptr;
		else return &mi.ram[phys_addr & RAMMASK];
	}

	void* MemoryInterface::MIGetMemoryPointerForDebug(uint32_t phys_addr)
	{
		if (phys_addr >= mi.ramSize) return nullptr;
		else return &mi.ram[phys_addr & RAMMASK];
	}

	void MemoryInterface::MIReadBurst(uint32_t mem_addr, uint8_t burstData[32])
	{
		memcpy(burstData, &mi.ram[mem_addr], 32);
		mi.pi_read_counter.cnt++;
	}

	void MemoryInterface::MIWriteBurst(uint32_t mem_addr, uint8_t burstData[32])
	{
		memcpy(&mi.ram[mem_addr], burstData, 32);
		mi.pi_write_counter.cnt++;
	}

	void MemoryInterface::MemRst()
	{
		// Let's clear the contents of Splash... I guess that will count as a MEM reset :)
		memset(mi.ram, 0, mi.ramSize);
	}
}