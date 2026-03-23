// PI - processor interface (interrupts and console control regs, FIFO)
#include "pch.h"

using namespace Debug;

namespace Flipper
{
	struct pi_reg_trap
	{
		void (*read)(uint32_t, uint32_t*, void*);
		void (*write)(uint32_t, uint32_t, void*);
		void* context;
	};

	// hardware traps tables.
	static pi_reg_trap hw_reg_traps[0x10000];

#pragma region "Processor Memory Interface"

	// Interface for accessing memory and memory-mapped registers from the processor side.
	// For more details see Docs/HW/ProcessorInterface.md

	// Actually, all memory errors should generate a PI interrupt, but we keep it simple and just output debug messages.

	void ProcessorInterface::PIReadByte(uint32_t pa, uint32_t* reg)
	{
		uint8_t* ptr;

		if (pa >= PI_MEMSPACE_BOOTROM)
		{
			if (HW->exi->exi.BootromPresent)
			{
				ptr = &HW->exi->exi.bootrom[pa - PI_MEMSPACE_BOOTROM];
				*reg = (uint32_t)*ptr;
			}
			else
			{
				*reg = 0xFF;
			}
			return;
		}

		// bus load byte
		ptr = (uint8_t*)HW->mem->MIGetMemoryPointerForPI(pa);
		if (ptr)
		{
			*reg = (uint32_t)*ptr;
		}
		else
		{
			Report(Channel::PI, "Unmapped memory read byte: 0x%08X\n", pa);
			*reg = 0;
		}
	}

	void ProcessorInterface::PIWriteByte(uint32_t pa, uint32_t data)
	{
		uint8_t* ptr;

		if (pa >= PI_MEMSPACE_BOOTROM)
		{
			return;
		}

		// bus store byte
		ptr = (uint8_t*)HW->mem->MIGetMemoryPointerForPI(pa);
		if (ptr)
		{
			*ptr = (uint8_t)data;
		}
		else
		{
			Report(Channel::PI, "Unmapped memory write byte: 0x%08X\n", pa);
		}
	}

	void ProcessorInterface::PIReadHalf(uint32_t pa, uint32_t* reg)
	{
		uint8_t* ptr;

		if (pa >= PI_MEMSPACE_BOOTROM)
		{
			if (HW->exi->exi.BootromPresent)
			{
				ptr = &HW->exi->exi.bootrom[pa - PI_MEMSPACE_BOOTROM];
				*reg = (uint32_t)_BYTESWAP_UINT16(*(uint16_t*)ptr);
			}
			else
			{
				*reg = 0xFFFF;
			}
			return;
		}

		// hardware trap
		if (pa >= HW_BASE)
		{
			pi_reg_trap* trap = &hw_reg_traps[pa & 0xfffe];
			trap->read(pa, reg, trap->context);
			return;
		}

		// bus load halfword
		ptr = (uint8_t*)HW->mem->MIGetMemoryPointerForPI(pa);
		if (ptr)
		{
			*reg = (uint32_t)_BYTESWAP_UINT16(*(uint16_t*)ptr);
		}
		else
		{
			Report(Channel::PI, "Unmapped memory read uint16: 0x%08X\n", pa);
			*reg = 0;
		}
	}

	void ProcessorInterface::PIWriteHalf(uint32_t pa, uint32_t data)
	{
		uint8_t* ptr;

		if (pa >= PI_MEMSPACE_BOOTROM)
		{
			return;
		}

		// hardware trap
		if (pa >= HW_BASE)
		{
			pi_reg_trap* trap = &hw_reg_traps[pa & 0xfffe];
			trap->write(pa, data, trap->context);
			return;
		}

		// bus store halfword
		ptr = (uint8_t*)HW->mem->MIGetMemoryPointerForPI(pa);
		if (ptr)
		{
			*(uint16_t*)ptr = _BYTESWAP_UINT16((uint16_t)data);
		}
		else
		{
			Report(Channel::PI, "Unmapped memory write uint16: 0x%08X\n", pa);
		}
	}

	void ProcessorInterface::PIReadWord(uint32_t pa, uint32_t* reg)
	{
		uint8_t* ptr;

		// bus load word
		ptr = (uint8_t*)HW->mem->MIGetMemoryPointerForPI(pa);
		if (ptr)
		{
			*reg = _BYTESWAP_UINT32(*(uint32_t*)ptr);
			return;
		}

		if (pa >= PI_MEMSPACE_BOOTROM)
		{
			if (HW->exi->exi.BootromPresent)
			{
				ptr = &HW->exi->exi.bootrom[pa - PI_MEMSPACE_BOOTROM];
				*reg = _BYTESWAP_UINT32(*(uint32_t*)ptr);
			}
			else
			{
				*reg = 0xFFFFFFFF;
			}
			return;
		}

		// hardware trap
		if (pa >= HW_BASE)
		{
			pi_reg_trap* trap;
			uint32_t temp_hi, temp_lo;
			trap = &hw_reg_traps[pa & 0xffff];
			trap->read(pa, &temp_hi, trap->context);
			trap = &hw_reg_traps[(pa + 2) & 0xffff];
			trap->read(pa + 2, &temp_lo, trap->context);
			*reg = (uint32_t)(temp_hi << 16) | (uint16_t)temp_lo;
			return;
		}

		// embedded frame buffer
		if ((pa & PI_EFB_ADDRESS_MASK) == PI_MEMSPACE_EFB)
		{
			*reg = HW->gfx->pe->EfbPeek(pa);
			return;
		}

		Report(Channel::PI, "Unmapped memory read word: 0x%08X\n", pa);
		*reg = 0;
	}

	void ProcessorInterface::PIWriteWord(uint32_t pa, uint32_t data)
	{
		uint8_t* ptr;

		if (pa >= PI_MEMSPACE_BOOTROM)
		{
			return;
		}

		// hardware trap
		if (pa >= HW_BASE)
		{
			pi_reg_trap* trap = &hw_reg_traps[pa & 0xffff];
			trap->write(pa, data >> 16, trap->context);
			trap = &hw_reg_traps[(pa + 2) & 0xffff];
			trap->write(pa + 2, (uint16_t)data, trap->context);
			return;
		}

		// embedded frame buffer
		if ((pa & PI_EFB_ADDRESS_MASK) == PI_MEMSPACE_EFB)
		{
			HW->gfx->pe->EfbPoke(pa, data);
			return;
		}

		// bus store word
		ptr = (uint8_t*)HW->mem->MIGetMemoryPointerForPI(pa);
		if (ptr)
		{
			*(uint32_t*)ptr = _BYTESWAP_UINT32(data);
		}
		else
		{
			Report(Channel::PI, "Unmapped memory write word: 0x%08X\n", pa);
		}
	}

	//
	// longlongs are never used in GC hardware access
	//

	void ProcessorInterface::PIReadDouble(uint32_t pa, uint64_t* reg)
	{
		if (pa >= PI_MEMSPACE_BOOTROM)
		{
			Halt("PI: Attempting to read uint64_t from BootROM\n");
			return;
		}

		uint8_t* ptr = (uint8_t*)HW->mem->MIGetMemoryPointerForPI(pa);
		if (!ptr)
		{
			Report(Channel::PI, "Unmapped memory read uint64: 0x%08X\n", pa);
			*reg = 0;
			return;
		}

		uint8_t* buf = ptr;

		// bus load doubleword
		*reg = _BYTESWAP_UINT64(*(uint64_t*)buf);
	}

	void ProcessorInterface::PIWriteDouble(uint32_t pa, uint64_t* data)
	{
		if (pa >= PI_MEMSPACE_BOOTROM)
		{
			Halt("PI: Attempting to write uint64_t to BootROM\n");
			return;
		}

		uint8_t* ptr = (uint8_t*)HW->mem->MIGetMemoryPointerForPI(pa);
		if (!ptr)
		{
			Report(Channel::PI, "Unmapped memory write uint64: 0x%08X\n", pa);
			return;
		}

		uint8_t* buf = ptr;

		// bus store doubleword
		*(uint64_t*)buf = _BYTESWAP_UINT64(*data);
	}

	void ProcessorInterface::PIReadBurst(uint32_t phys_addr, uint8_t burstData[32])
	{
		uint8_t* ptr = (uint8_t*)HW->mem->MIGetMemoryPointerForPI(phys_addr);
		if (!ptr) {
			Halt("PI: Unmapped read burst 0x%08X\n", phys_addr);
			return;
		}

		HW->mem->MIReadBurst(phys_addr, burstData);
	}

	void ProcessorInterface::PIWriteBurst(uint32_t phys_addr, uint8_t burstData[32])
	{
		// You can actually write anywhere on the page
		if ((phys_addr & ~0xfff) == PI_REGSPACE_GFX_FIFO)
		{
			// PI FIFO

			HW->mem->MIWriteBurst(pi.cp_wrptr, burstData);
			pi.cp_wrptr += 32;

			if (pi.cp_wrptr == pi.cp_top)
			{
				pi.cp_wrptr = pi.cp_base;
				pi.wrap_bit = 1;
			}

			// Notify CP
			HW->cp->FifoWriteBurst();
			return;
		}

		uint8_t* ptr = (uint8_t*)HW->mem->MIGetMemoryPointerForPI(phys_addr);
		if (!ptr) {
			Halt("PI: Unmapped write burst\n");
			return;
		}

		HW->mem->MIWriteBurst(phys_addr, burstData);
	}

	// ---------------------------------------------------------------------------
	// default hardware R/W operations.
	// emulation is halted on unknown register access.

	void ProcessorInterface::pi_def_hw_read16(uint32_t addr, uint32_t* reg, void* context)
	{
		Halt("PI: Unhandled HW access: R16 %08X\n", addr);
	}

	void ProcessorInterface::pi_def_hw_write16(uint32_t addr, uint32_t data, void* context)
	{
		Halt("PI: Unhandled HW access: W16 %08X = %04X\n", addr, (uint16_t)data);
	}

	// ---------------------------------------------------------------------------
	// traps API

	void ProcessorInterface::PISetTrap(
		uint32_t addr,
		void (*rdTrap)(uint32_t, uint32_t*, void*),
		void (*wrTrap)(uint32_t, uint32_t, void*),
		void* context)
	{
		// address must be in correct range
		if (!((addr >= HW_BASE) && (addr < (HW_BASE + HW_MAX_KNOWN))))
		{
			Halt(
				"PI: Trap address is out of GAMECUBE registers range.\n"
				"address : %08X\n", addr
			);
		}

		if (rdTrap == NULL) rdTrap = pi_def_hw_read16;
		if (wrTrap == NULL) wrTrap = pi_def_hw_write16;

		pi_reg_trap* trap = &hw_reg_traps[addr & 0xfffe];
		trap->read = rdTrap;
		trap->write = wrTrap;
		trap->context = context;
	}

	// Set default traps for any access, called every time when emu restarted
	void ProcessorInterface::PIClearTraps()
	{
		uint32_t addr;

		// possible errors, if greater 0xffff
		assert(HW_MAX_KNOWN < 0x10000);

		// for 16-bit registers
		for (addr = HW_BASE; addr < (HW_BASE + HW_MAX_KNOWN); addr += 2)
		{
			PISetTrap(addr);
		}
	}

#pragma endregion "Processor Memory Interface"

	// ---------------------------------------------------------------------------
	// interrupts

	// return short interrupt description
	const char* ProcessorInterface::intdesc(uint32_t mask)
	{
		switch (mask & 0xffff)
		{
			case PI_INTERRUPT_HSP: return "HSP";
			case PI_INTERRUPT_DEBUG: return "DEBUG";
			case PI_INTERRUPT_CP: return "CP";
			case PI_INTERRUPT_PE_FINISH: return "PE_FINISH";
			case PI_INTERRUPT_PE_TOKEN: return "PE_TOKEN";
			case PI_INTERRUPT_VI: return "VI";
			case PI_INTERRUPT_MEM: return "MEM";
			case PI_INTERRUPT_DSP: return "DSP";
			case PI_INTERRUPT_AI: return "AI";
			case PI_INTERRUPT_EXI: return "EXI";
			case PI_INTERRUPT_SI: return "SI";
			case PI_INTERRUPT_DI: return "DI";
			case PI_INTERRUPT_RSW: return "RSW";
			case PI_INTERRUPT_PI: return "PI_ERROR";
		}

		// default
		return "UNKNOWN";
	}

	void ProcessorInterface::printOut(uint32_t mask, const char* fix)
	{
		std::string buf;
		for (uint32_t m = 1; m <= PI_INTERRUPT_MSB; m <<= 1)
		{
			if (mask & m)
			{
				buf += intdesc(m);
				buf += "INT ";
			}
		}
		Report(Channel::PI, "%s%s (pc: %08X, time: 0x%llx)\n", buf.c_str(), fix, Core->regs.pc, Core->GetTicks());
	}

	// assert interrupt
	void ProcessorInterface::PIAssertInt(uint32_t mask)
	{
		pi.intsr |= mask;
		if (pi.intmr & mask)
		{
			if (pi.log)
			{
				printOut(mask, "asserted");
			}
		}

		PIInterruptSource intSource = (PIInterruptSource)(31 - CNTLZ(mask));
		pi.intCounters[(size_t)intSource]++;

		if (pi.intsr & pi.intmr)
		{
			if (pi.log) {
				int64_t ticks_now = Core->GetTicks();
				int64_t ticks_passed = ticks_now - pi.last_int_ticks;
				Report(Channel::PI, "Ticks passed: %lld (%d us)\n", ticks_passed, ticks_passed / pi.one_microsecond);
				pi.last_int_ticks = ticks_now;
			}

			Core->AssertInterrupt();
		}
		else
		{
			Core->ClearInterrupt();
		}

		if (pi.intbrk != 0) {
			if ((pi.intbrk & mask) != 0) {
				pi.intbrk &= ~mask;
				Halt("A one-time interrupt breakpoint has been triggered %s\n", intdesc(mask));
			}
		}
	}

	// clear interrupt
	// Also used by external modules to clear read-only INTSR bits
	void ProcessorInterface::PIClearInt(uint32_t mask)
	{
		if (pi.intsr & mask)
		{
			if (pi.log)
			{
				printOut(mask, "cleared");
			}
		}
		pi.intsr &= ~mask;

		if (pi.intsr & pi.intmr)
		{
			Core->AssertInterrupt();
		}
		else
		{
			Core->ClearInterrupt();
		}
	}

	// ---------------------------------------------------------------------------
	// pi reg access

	//
	// CONFIG register
	//
	void ProcessorInterface::pi_write_config(uint32_t data)
	{
		// It is not yet clear what may or may not be affected by the support for system reset, for now we will leave only debug messages.

		if ((data & PI_CONFIG_SYSRSTB) == 0)
		{
			Report(Channel::PI, "System Reset requested.\n");
			// reset emulator
			//EMUClose();
			//EMUOpen();
		}

		if ((data & PI_CONFIG_MEMRSTB) == 0)
		{
			// BS1 clears memory so that VI does not produce nasty garbage when XFB loads (really? but it looks like the memory reset is done BEFORE filling Splash with test patterns).

			Report(Channel::PI, "MEM Reset requested.\n");
			HW->mem->MemRst();
		}

		if ((data & PI_CONFIG_DIRSTB) == 0)
		{
			Report(Channel::PI, "DVD Reset requested.\n");
			DVD::DDU->Reset();
		}
	}

	void ProcessorInterface::PIRegRead(uint32_t addr, uint32_t* reg, void* ctx)
	{
		ProcessorInterface* pi = (ProcessorInterface*)ctx;
		switch (addr & 0xFF) {

			case PI_INTSR + 2:
				*reg = pi->pi.intsr /* | PI_INTSR_RSTSWB */;
				break;
			case PI_INTMR + 2:
				*reg = pi->pi.intmr;
				break;
			case PI_CPBAS:
				*reg = (pi->pi.cp_base >> 16) & PI_FIFO_ADDRESS_MASK_HI;
				break;
			case PI_CPBAS + 2:
				*reg = (uint16_t)pi->pi.cp_base & PI_FIFO_ADDRESS_MASK_LO;
				break;
			case PI_CPTOP:
				*reg = (pi->pi.cp_top >> 16) & PI_FIFO_ADDRESS_MASK_HI;
				break;
			case PI_CPTOP + 2:
				*reg = (uint16_t)pi->pi.cp_top & PI_FIFO_ADDRESS_MASK_LO;
				break;
			case PI_CPWRT:
				*reg = (pi->pi.cp_wrptr >> 16) & PI_FIFO_ADDRESS_MASK_HI | (pi->pi.wrap_bit ? PI_CPWRT_WRAP : 0);
				break;
			case PI_CPWRT + 2:
				*reg = (uint16_t)pi->pi.cp_wrptr & PI_FIFO_ADDRESS_MASK_LO;
				break;
			case PI_CPABT + 2:
				Report(Channel::PI, "PI CP Abort read not implemented!\n");
				*reg = 0;
				break;
			case PI_CONFIG + 2:
				// Return the state that there is no reset. We process the reset immediately.
				*reg = PI_CONFIG_MEMRSTB | PI_CONFIG_DIRSTB;
				break;
			case PI_CHIPID:
				*reg = pi->pi.chipid >> 16;
				break;
			case PI_CHIPID + 2:
				*reg = (uint16_t)pi->pi.chipid;
				break;

			default:
				*reg = 0;
				break;
		}
	}

	void ProcessorInterface::PIRegWrite(uint32_t addr, uint32_t data, void* ctx)
	{
		ProcessorInterface* pi = (ProcessorInterface*)ctx;
		switch (addr & 0xFF) {

			case PI_INTSR + 2:
				// Write 1 clears only HSP, DEBUG, RSW and PI interrupts. The rest is cleared in a tricky way in the corresponding modules.
				data &= (PI_INTERRUPT_HSP | PI_INTERRUPT_DEBUG | PI_INTERRUPT_RSW | PI_INTERRUPT_PI);
				pi->PIClearInt(data);
				break;
			case PI_INTMR + 2:
				pi->pi.intmr = data;

				// print out list of masked interrupts
				if (pi->pi.intmr && pi->pi.log)
				{
					std::string buf;
					for (uint32_t m = 1; m <= PI_INTERRUPT_MSB; m <<= 1)
					{
						if (pi->pi.intmr & m)
						{
							buf += pi->intdesc(m);
							buf += " ";
						}
					}

					Report(Channel::PI, "unmasked : %s\n", buf.c_str());
				}
				break;
			case PI_CPBAS:
				pi->pi.cp_base &= 0x0000ffff;
				pi->pi.cp_base |= (data & PI_FIFO_ADDRESS_MASK_HI) << 16;
				break;
			case PI_CPBAS + 2:
				pi->pi.cp_base &= 0xffff0000;
				pi->pi.cp_base |= (uint16_t)data & PI_FIFO_ADDRESS_MASK_LO;
				if (pi->pi.log) {
					pi->DumpPIFIFO();
				}
				break;
			case PI_CPTOP:
				pi->pi.cp_top &= 0x0000ffff;
				pi->pi.cp_top |= (data & PI_FIFO_ADDRESS_MASK_HI) << 16;
				break;
			case PI_CPTOP + 2:
				pi->pi.cp_top &= 0xffff0000;
				pi->pi.cp_top |= (uint16_t)data & PI_FIFO_ADDRESS_MASK_LO;
				if (pi->pi.log) {
					pi->DumpPIFIFO();
				}
				break;
			case PI_CPWRT:
				pi->pi.cp_wrptr &= 0x0000ffff;
				pi->pi.cp_wrptr |= (data & PI_FIFO_ADDRESS_MASK_HI) << 16;
				pi->pi.wrap_bit = 0;
				break;
			case PI_CPWRT + 2:
				pi->pi.cp_wrptr &= 0xffff0000;
				pi->pi.cp_wrptr |= (uint16_t)data & PI_FIFO_ADDRESS_MASK_LO;
				pi->pi.wrap_bit = 0;
				if (pi->pi.log) {
					pi->DumpPIFIFO();
				}
				break;
			case PI_CPABT + 2:
				if ((data & 1) != 0) {
					HW->cp->CPAbortFifo();
				}
				break;
			case PI_CONFIG + 2:
				pi->pi_write_config(data);
				break;
			case PI_STRGTH + 2:
				// Just so the BS doesn't give an error on unmapped HW.
				Report(Channel::PI, "Strength: 0x%08x\n", data);
				break;

			default:
				break;
		}
	}

	// show PI fifo configuration
	void ProcessorInterface::DumpPIFIFO()
	{
		Report(Channel::Norm, "PI fifo configuration (pc=0x%08X)\n", CallJdi("GetPc")->value.AsUint32);
		Report(Channel::Norm, "   base :0x%08X\n", pi.cp_base);
		Report(Channel::Norm, "   top  :0x%08X\n", pi.cp_top);
		Report(Channel::Norm, "   wrptr:0x%08X\n", pi.cp_wrptr);
		Report(Channel::Norm, "   wrap :%d\n", pi.wrap_bit);
	}

	// ---------------------------------------------------------------------------
	// init

	ProcessorInterface::ProcessorInterface(HWConfig* config)
	{
		Report(Channel::PI, "Processor interface\n");

		memset(&pi, 0, sizeof(pi));

		pi.consoleVer = config->consoleVer;
		pi.log = config->pi_log;

		// bootrom using this register in following way:
		//
		//  [8000002C] =  1                     // set machine type to retail1
		//  [8000002C] += [CC00302C] >> 28;     // upgrade revision
		// set to HW2 final production board 
		// we need to set [8000002C] with value 3 (retail3)
		// so return '2'
		pi.chipid = ((pi.consoleVer & 0xf) - 1) << 28;

		// now any access will generate unhandled warning,
		// if emulator try to read or write register,
		// so we need to set traps for missing regs:

		// clear all traps
		PIClearTraps();

		// clear interrupt registers
		pi.intsr = pi.intmr = 0;

		pi.intbrk = 0;
		pi.last_int_ticks = 0;
		pi.one_microsecond = Core->OneSecond() / 1000000;

		// set registers hooks
		for (uint32_t i = 0; i < PI_REG_MAX; i += 2) {
			PISetTrap(PI_REGSPACE_PI + i, PIRegRead, PIRegWrite, this);
		}

		// Reset interrupt counters
		for (size_t i = 0; i < (size_t)PIInterruptSource::Max; i++)
		{
			pi.intCounters[i] = 0;
		}
	}

	ProcessorInterface::~ProcessorInterface()
	{
		PIClearTraps();
	}

	void ProcessorInterface::PIBreakOnNextInt(uint32_t mask)
	{
		pi.intbrk |= mask;
	}

	uint8_t* ProcessorInterface::PITranslatePhysicalAddress(uint32_t physAddr, size_t bytes)
	{
		if (bytes == 0)
			return nullptr;

		if (physAddr >= PI_MEMSPACE_BOOTROM && HW->exi->exi.BootromPresent)
		{
			return &HW->exi->exi.bootrom[physAddr - PI_MEMSPACE_BOOTROM];
		}

		uint8_t* ptr = (uint8_t*)HW->mem->MIGetMemoryPointerForPI(physAddr);
		if (ptr)
		{
			return ptr;
		}

		return nullptr;
	}

	int64_t ProcessorInterface::PIGetInterruptCounter(PIInterruptSource src)
	{
		return pi.intCounters[(size_t)src];
	}

	void ProcessorInterface::PIResetInterruptCounter(PIInterruptSource src)
	{
		pi.intCounters[(size_t)src] = 0;
	}
}