// MI - memory interface.
//
// MI is used in __OSInitMemoryProtection and by few games for debug.
#include "pch.h"

using namespace Debug;

MIControl mi;

// stubs for MI registers
static void no_write(uint32_t addr, uint32_t data) {}
static void no_read(uint32_t addr, uint32_t* reg) { *reg = 0; }

static void MEM_WriteMarrStart(uint32_t addr, uint32_t data)
{
	switch ((addr & 0xFF) >> 1)
	{
		case MEM_MARR0_START_ID: mi.marr_start[0] = data; break;
		case MEM_MARR1_START_ID: mi.marr_start[1] = data; break;
		case MEM_MARR2_START_ID: mi.marr_start[2] = data; break;
		case MEM_MARR3_START_ID: mi.marr_start[3] = data; break;
		default: break;
	}
}

static void MEM_ReadMarrStart(uint32_t addr, uint32_t* reg)
{
	switch ((addr & 0xFF) >> 1)
	{
		case MEM_MARR0_START_ID: *reg = mi.marr_start[0]; break;
		case MEM_MARR1_START_ID: *reg = mi.marr_start[1]; break;
		case MEM_MARR2_START_ID: *reg = mi.marr_start[2]; break;
		case MEM_MARR3_START_ID: *reg = mi.marr_start[3]; break;
		default: break;
	}
}

static void MEM_WriteMarrEnd(uint32_t addr, uint32_t data)
{
	switch ((addr & 0xFF) >> 1)
	{
		case MEM_MARR0_END_ID: mi.marr_end[0] = data; break;
		case MEM_MARR1_END_ID: mi.marr_end[1] = data; break;
		case MEM_MARR2_END_ID: mi.marr_end[2] = data; break;
		case MEM_MARR3_END_ID: mi.marr_end[3] = data; break;
		default: break;
	}
}

static void MEM_ReadMarrEnd(uint32_t addr, uint32_t* reg)
{
	switch ((addr & 0xFF) >> 1)
	{
		case MEM_MARR0_END_ID: *reg = mi.marr_end[0]; break;
		case MEM_MARR1_END_ID: *reg = mi.marr_end[1]; break;
		case MEM_MARR2_END_ID: *reg = mi.marr_end[2]; break;
		case MEM_MARR3_END_ID: *reg = mi.marr_end[3]; break;
		default: break;
	}
}

static void MEM_WriteMarrControl(uint32_t addr, uint32_t data)
{
	mi.marr_control.bits = data;

	Report(Channel::MI, "MARR Control (MARR0-3 read/write enabled): %d/%d, %d/%d, %d/%d, %d/%d\n",
		mi.marr_control.marr0_read_enable, mi.marr_control.marr0_write_enable,
		mi.marr_control.marr1_read_enable, mi.marr_control.marr1_write_enable,
		mi.marr_control.marr2_read_enable, mi.marr_control.marr2_write_enable,
		mi.marr_control.marr3_read_enable, mi.marr_control.marr3_write_enable);
}

static void MEM_ReadMarrControl(uint32_t addr, uint32_t* reg)
{
	*reg = mi.marr_control.bits;
}

static void MEM_WriteIntEnable(uint32_t addr, uint32_t data)
{
	mi.int_enable.bits = data;
}

static void MEM_ReadIntEnable(uint32_t addr, uint32_t* reg)
{
	*reg = mi.int_enable.bits;
}

static void MEM_ReadIntStatus(uint32_t addr, uint32_t* reg)
{
	*reg = mi.int_status.bits;
}

static void MEM_WriteIntClear(uint32_t addr, uint32_t data)
{
	mi.int_status.bits &= ~(data);

	if ((mi.int_status.bits & mi.int_enable.bits) != 0) {
		PIAssertInt(PI_INTERRUPT_MEM);
	}
	else {
		PIClearInt(PI_INTERRUPT_MEM);
	}
}

static void MEM_WriteCounter(uint32_t addr, uint32_t data)
{
	switch ((addr & 0xFF) >> 1)
	{
		case MEM_CP_COUNTERH_ID: mi.cp_counter.hi = data; break;
		case MEM_CP_COUNTERL_ID: mi.cp_counter.lo = data; break;
		case MEM_TC_COUNTERH_ID: mi.tc_counter.hi = data; break;
		case MEM_TC_COUNTERL_ID: mi.tc_counter.lo = data; break;
		case MEM_PI_READ_COUNTERH_ID: mi.pi_read_counter.hi = data; break;
		case MEM_PI_READ_COUNTERL_ID: mi.pi_read_counter.lo = data; break;
		case MEM_PI_WRITE_COUNTERH_ID: mi.pi_write_counter.hi = data; break;
		case MEM_PI_WRITE_COUNTERL_ID: mi.pi_write_counter.lo = data; break;
		case MEM_DSP_COUNTERH_ID: mi.dsp_counter.hi = data; break;
		case MEM_DSP_COUNTERL_ID: mi.dsp_counter.lo = data; break;
		case MEM_IO_COUNTERH_ID: mi.io_counter.hi = data; break;
		case MEM_IO_COUNTERL_ID: mi.io_counter.lo = data; break;
		case MEM_VI_COUNTERH_ID: mi.vi_counter.hi = data; break;
		case MEM_VI_COUNTERL_ID: mi.vi_counter.lo = data; break;
		case MEM_PE_COUNTERH_ID: mi.pe_counter.hi = data; break;
		case MEM_PE_COUNTERL_ID: mi.pe_counter.lo = data; break;
		default:
			break;
	}
}

static void MEM_ReadCounter(uint32_t addr, uint32_t* reg)
{
	switch ((addr & 0xFF) >> 1)
	{
		case MEM_CP_COUNTERH_ID: *reg = mi.cp_counter.hi; break;
		case MEM_CP_COUNTERL_ID: *reg = mi.cp_counter.lo; break;
		case MEM_TC_COUNTERH_ID: *reg = mi.tc_counter.hi; break;
		case MEM_TC_COUNTERL_ID: *reg = mi.tc_counter.lo; break;
		case MEM_PI_READ_COUNTERH_ID: *reg = mi.pi_read_counter.hi; break;
		case MEM_PI_READ_COUNTERL_ID: *reg = mi.pi_read_counter.lo; break;
		case MEM_PI_WRITE_COUNTERH_ID: *reg = mi.pi_write_counter.hi; break;
		case MEM_PI_WRITE_COUNTERL_ID: *reg = mi.pi_write_counter.lo; break;
		case MEM_DSP_COUNTERH_ID: *reg = mi.dsp_counter.hi; break;
		case MEM_DSP_COUNTERL_ID: *reg = mi.dsp_counter.lo; break;
		case MEM_IO_COUNTERH_ID: *reg = mi.io_counter.hi; break;
		case MEM_IO_COUNTERL_ID: *reg = mi.io_counter.lo; break;
		case MEM_VI_COUNTERH_ID: *reg = mi.vi_counter.hi; break;
		case MEM_VI_COUNTERL_ID: *reg = mi.vi_counter.lo; break;
		case MEM_PE_COUNTERH_ID: *reg = mi.pe_counter.hi; break;
		case MEM_PE_COUNTERL_ID: *reg = mi.pe_counter.lo; break;
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

	for (uint32_t ofs = 0; ofs < 0x100; ofs += 2)
	{
		PISetTrap(16, 0x0C004000 | ofs, no_read, no_write);
	}

	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_MARR0_START_ID), MEM_ReadMarrStart, MEM_WriteMarrStart);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_MARR1_START_ID), MEM_ReadMarrStart, MEM_WriteMarrStart);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_MARR2_START_ID), MEM_ReadMarrStart, MEM_WriteMarrStart);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_MARR3_START_ID), MEM_ReadMarrStart, MEM_WriteMarrStart);

	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_MARR0_END_ID), MEM_ReadMarrEnd, MEM_WriteMarrEnd);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_MARR1_END_ID), MEM_ReadMarrEnd, MEM_WriteMarrEnd);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_MARR2_END_ID), MEM_ReadMarrEnd, MEM_WriteMarrEnd);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_MARR3_END_ID), MEM_ReadMarrEnd, MEM_WriteMarrEnd);

	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_MARR_CONTROL_ID), MEM_ReadMarrControl, MEM_WriteMarrControl);

	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_INT_ENABLE_ID), MEM_ReadIntEnable, MEM_WriteIntEnable);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_INT_STATUS_ID), MEM_ReadIntStatus, nullptr);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_INT_CLR_ID), nullptr, MEM_WriteIntClear);

	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_MARR0_START_ID), MEM_ReadMarrStart, MEM_WriteMarrStart);

	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_CP_COUNTERH_ID), MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_CP_COUNTERL_ID), MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_TC_COUNTERH_ID), MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_TC_COUNTERL_ID), MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_PI_READ_COUNTERH_ID), MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_PI_READ_COUNTERL_ID), MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_PI_WRITE_COUNTERH_ID), MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_PI_WRITE_COUNTERL_ID), MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_DSP_COUNTERH_ID), MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_DSP_COUNTERL_ID), MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_IO_COUNTERH_ID), MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_IO_COUNTERL_ID), MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_VI_COUNTERH_ID), MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_VI_COUNTERL_ID), MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_PE_COUNTERH_ID), MEM_ReadCounter, MEM_WriteCounter);
	PISetTrap(16, PI_REG16_TO_SPACE(PI_REGSPACE_MEM, MEM_PE_COUNTERL_ID), MEM_ReadCounter, MEM_WriteCounter);

	LoadBootrom(config);
}

void MIClose()
{
	if (mi.ram)
	{
		delete[] mi.ram;
		mi.ram = nullptr;
	}

	if (mi.bootrom)
	{
		delete[] mi.bootrom;
		mi.bootrom = nullptr;
	}
}

uint8_t* MITranslatePhysicalAddress(uint32_t physAddr, size_t bytes)
{
	if (!mi.ram || bytes == 0)
		return nullptr;

	if (physAddr < (RAMSIZE - bytes))
	{
		return &mi.ram[physAddr];
	}

	if (physAddr >= BOOTROM_START_ADDRESS && mi.BootromPresent)
	{
		return &mi.bootrom[physAddr - BOOTROM_START_ADDRESS];
	}

	return nullptr;
}

// The counters are not emulated accurately, but this is not required.

/// <summary>
/// Used for memory access from the CP side, for Vertex Array.
/// </summary>
void* MIGetMemoryPointerForCP(uint32_t phys_addr)
{
	mi.cp_counter.cnt++;
	return &mi.ram[phys_addr & RAMMASK];
}

/// <summary>
/// The texture unit accesses MEM to sample textures in TMEM.
/// </summary>
void* MIGetMemoryPointerForTX(uint32_t phys_addr)
{
	mi.tc_counter.cnt++;
	return &mi.ram[phys_addr & RAMMASK];
}

/// <summary>
/// VI uses MEM to gain access to the XFB.
/// </summary>
void* MIGetMemoryPointerForVI(uint32_t phys_addr)
{
	mi.vi_counter.cnt++;
	if (phys_addr >= mi.ramSize) return nullptr;
	else return &mi.ram[phys_addr & RAMMASK];
}

/// <summary>
/// Used by various IO devices (AI, EXI, SI, DI) for DMA.
/// </summary>
void* MIGetMemoryPointerForIO(uint32_t phys_addr)
{
	mi.io_counter.cnt++;
	if (phys_addr >= mi.ramSize) return nullptr;
	else return &mi.ram[phys_addr & RAMMASK];
}
