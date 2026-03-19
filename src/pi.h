// Flipper Processor Interface (for Gekko)

#pragma once

// Address spaces
#define PI_MEMSPACE_MAINMEM     0x0000'0000         // 1T-SRAM main memory ("Splash")
#define PI_MEMSPACE_EFB         0x0800'0000         // GFX eFB base address
#define PI_REGSPACE_CP          0x0C00'0000
#define PI_REGSPACE_PE          0x0C00'1000         // GFX Pixel Engine regs mapped to physical memory
#define PI_REGSPACE_VI          0x0C00'2000
#define PI_REGSPACE_PI          0x0C00'3000
#define PI_REGSPACE_MEM         0x0C00'4000
#define PI_REGSPACE_DSP         0x0C00'5000
#define PI_REGSPACE_DI          0x0C00'6000
#define PI_REGSPACE_SI          0x0C00'6400
#define PI_REGSPACE_EXI         0x0C00'6800
#define PI_REGSPACE_AI          0x0C00'6C00
#define PI_REGSPACE_GFX_FIFO    0x0C00'8000         // GFX streaming fifo
#define PI_MEMSPACE_BOOTROM     0xFFF0'0000			// Bootrom start address

// Efb Z-plane select
#define PI_EFB_ZPLANE       0x0040'0000 

// Mask EFB address
#define PI_EFB_ADDRESS_MASK 0xFF80'0000

#define PI_INTSR            0x00      // master interrupt reg
#define PI_INTMR            0x04      // master interrupt mask (a set bit means that the interrupt is enabled)
#define PI_CPBAS            0x0C      // PI CP fifo base
#define PI_CPTOP            0x10      // PI CP fifo top
#define PI_CPWRT            0x14      // PI CP fifo write pointer
#define PI_CPABT            0x18      // Abort PI CP FIFO
#define PI_PIESR            0x1C
#define PI_PIEAR            0x20
#define PI_CONFIG           0x24      // PI CFG + reset bits
#define PI_DURAR            0x28
#define PI_CHIPID           0x2C      // Flipper ID (console revision)
#define PI_STRGTH           0x30
#define PI_CPUDBB           0x34
#define PI_REG_MAX			0x38

#define PI_INTSR_RSTSWB		0x10000			// The state of the reset switch button. Non-maskable INTSR bit

// PI interrupt regs mask
#define PI_INTERRUPT_HSP		0x2000      // high-speed port (SDRAM expansion)
#define PI_INTERRUPT_DEBUG		0x1000      // debug hardware
#define PI_INTERRUPT_CP			0x0800      // command fifo
#define PI_INTERRUPT_PE_FINISH	0x0400      // PE finish command (draw done)
#define PI_INTERRUPT_PE_TOKEN	0x0200      // PE token parsed (draw sync)
#define PI_INTERRUPT_VI			0x0100      // 4 VI line ints
#define PI_INTERRUPT_MEM		0x0080      // memory protection failed
#define PI_INTERRUPT_DSP		0x0040      // various DSP (ARAM, AI FIFO, DSP)
#define PI_INTERRUPT_AI			0x0020      // DVD streaming trigger interrupt
#define PI_INTERRUPT_EXI		0x0010      // EXI transfer complete
#define PI_INTERRUPT_SI			0x0008      // serial interrupts
#define PI_INTERRUPT_DI			0x0004      // DVD cover, break, transfer complete
#define PI_INTERRUPT_RSW		0x0002      // Reset switch interrupt
#define PI_INTERRUPT_PI			0x0001      // Generated when something goes wrong inside Flipper
#define PI_INTERRUPT_MSB		PI_INTERRUPT_HSP

// PI CONFIG Reset control bits
#define PI_CONFIG_SYSRSTB 0x00000001
#define PI_CONFIG_MEMRSTB 0x00000002
#define PI_CONFIG_DIRSTB 0x00000004

// PI CP write pointer wrap bit (for the upper register)
#define PI_CPWRT_WRAP   0x0400

// Mask for FIFO registers (only bits 25:5 are used for memory addressing)
#define PI_FIFO_ADDRESS_MASK_HI  0x03ff
#define PI_FIFO_ADDRESS_MASK_LO  0xffe0

enum class PIInterruptSource
{
	PI,
	RSW,
	DI,
	SI,
	EXI,
	AI,
	DSP,
	MEM,
	VI,
	PE_TOKEN,
	PE_FINISH,
	CP,
	DEBUG,
	HSP,

	Max,
};

// hardware registers base (physical address)
#define HW_BASE         0x0C000000

// max known GC HW address is 0x0C008004 (fifo), so 0x8010 will be enough.
// note : it must not be greater 0xffff, unless you need to change code.
#define HW_MAX_KNOWN    0x8010

// Program interface that implements transactions over Gekko 60x Bus

void PIReadByte(uint32_t pa, uint32_t* reg);
void PIWriteByte(uint32_t pa, uint32_t data);
void PIReadHalf(uint32_t pa, uint32_t* reg);
void PIWriteHalf(uint32_t pa, uint32_t data);
void PIReadWord(uint32_t pa, uint32_t* reg);
void PIWriteWord(uint32_t pa, uint32_t data);
void PIReadDouble(uint32_t pa, uint64_t* reg);
void PIWriteDouble(uint32_t pa, uint64_t* data);
void PIReadBurst(uint32_t phys_addr, uint8_t burstData[32]);
void PIWriteBurst(uint32_t phys_addr, uint8_t burstData[32]);

uint8_t* PITranslatePhysicalAddress(uint32_t physAddr, size_t bytes);

// ---------------------------------------------------------------------------
// hardware API

// PI state (registers and other data)
struct PIControl
{
	volatile uint16_t intsr;	// interrupt cause
	volatile uint16_t intmr;	// interrupt mask
	volatile uint16_t intbrk;	// one-shot interrup breakpoint
	bool        log;			// log interrupts
	uint32_t    consoleVer;		// console version
	uint32_t	chipid;
	int64_t     intCounters[(size_t)PIInterruptSource::Max];	// interrupt counters
	int64_t last_int_ticks;		// Core TBR value since the last interrupt (for statistics)
	int64_t one_microsecond;	// one CPU microsecond in timer ticks

	// The PI contains its own part of the CP FIFO, intended for Burst transactions via the GFX FIFO Stream Pointer. The write event is simultaneously forwarded to the CP to track the WRPTR.
	volatile uint32_t cp_base;
	volatile uint32_t cp_top;
	volatile uint32_t cp_wrptr;		// also WRAP bit
	volatile uint32_t wrap_bit;		// When the CPU writes to the CPWRT register, this bit is cleared. When the FIFO writes by the CPWRT address, this bit remains asserted
};

extern  PIControl pi;

// The role of the /HRESET signal is performed by the PIOpen/PIClose pairing. The role of INT signal is performed by PIAssertInt/PIClearInt pairing

void PIAssertInt(uint32_t mask);  // set interrupt(s)
void PIClearInt(uint32_t mask);   // clear interrupt(s)
void PIOpen(HWConfig* config);
void PIClose();

/// <summary>
/// Set the breakpoint (Halt) to trigger once per interrupt.
/// Used by debug commands for step-by-step debugging of system runtime.
/// </summary>
/// <param name="mask">Mask of interrupts that require a one-shot break</param>
void PIBreakOnNextInt(uint32_t mask);

/// <summary>
/// Install a handler to access a register mapped to the CPU address space.
/// </summary>
/// <param name="addr">physical address of trap</param>
/// <param name="rdTrap">register read trap</param>
/// <param name="wrTrap">register write trap</param>
/// <param name="context">An arbitrary context that will be passed to the handler (e.g. `this`)</param>
void PISetTrap(
	uint32_t addr,
	void (*rdTrap)(uint32_t, uint32_t*, void*) = NULL,
	void (*wrTrap)(uint32_t, uint32_t, void*) = NULL,
	void *context = NULL);

void DumpPIFIFO();