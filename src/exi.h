#pragma once

// EXI registers (all registers are 32-bit from the software side)
//                      (chan 0)
#define EXI0_CSR        0x00          // Communication Status Register
#define EXI0_MADR       0x04          // DMA Memory Address Register
#define EXI0_LEN        0x08          // DMA Length Register
#define EXI0_CR         0x0C          // Control Register
#define EXI0_DATA       0x10          // Data Register
//                      (chan 1)
#define EXI1_CSR        0x14          // -"-
#define EXI1_MADR       0x18
#define EXI1_LEN        0x1C
#define EXI1_CR         0x20
#define EXI1_DATA       0x24
//                      (chan 2)
#define EXI2_CSR        0x28          // -"-
#define EXI2_MADR       0x2C
#define EXI2_LEN        0x30
#define EXI2_CR         0x34
#define EXI2_DATA       0x38

// EXI Communication Status Register mask
#define EXI_CSR_ROMDIS      (1 << 13)       // disable IPL decryption logic
#define EXI_CSR_EXT         (1 << 12)       // attached status
#define EXI_CSR_EXTINT      (1 << 11)       // attached / detached interrupt
#define EXI_CSR_EXTINTMSK   (1 << 10)
#define EXI_CSR_CS2B        (1 <<  9)       // device select bits
#define EXI_CSR_CS1B        (1 <<  8)
#define EXI_CSR_CS0B        (1 <<  7)
#define EXI_CSR_CLK(reg)    ((reg >> 4) & 7)// dont care (bus clock)
#define EXI_CSR_TCINT       (1 <<  3)       // transfer complete interrupt
#define EXI_CSR_TCINTMSK    (1 <<  2)
#define EXI_CSR_EXIINT      (1 <<  1)       // exi interrupt from devices (IRQ line)
#define EXI_CSR_EXIINTMSK   (1 <<  0)
#define EXI_CSR_INTERRUPTS  (EXI_CSR_EXTINT | EXI_CSR_TCINT | EXI_CSR_EXIINT)
#define EXI_CSR_READONLY    (EXI_CSR_EXT | EXI_CSR_EXTINT | EXI_CSR_TCINT | EXI_CSR_EXIINT)

// EXI Control Register mask
#define EXI_CR_TLEN(reg)    ((reg >> 4) & 3)// immediate data size
#define EXI_CR_RW(reg)      ((reg >> 2) & 3)// direction (read/write)
#define EXI_CR_DMA          (1 << 1)        // select dma transfer (dma/immediate)
#define EXI_CR_TSTART       (1 << 0)        // start transfer

#define EXI_MADR_MASK 0x3fff'ffe0

// EXI registers block
struct EXIRegs
{
	volatile uint16_t         csr;            // communication register 
	volatile uint32_t         madr;           // memory address (32 byte aligned)
	volatile uint32_t         len;            // size (32 bytes aligned)
	volatile uint16_t         cr;             // control register
	volatile uint32_t         data;           // immediate data register
};

// Bootrom size (in bytes)
#define BOOTROM_SIZE    (2*1024*1024)

// ---------------------------------------------------------------------------
// hardware API

// EXI state (registers and other data)
struct EXIState
{
	// hardware state
	EXIRegs     regs[3];        // exi registers
	SRAM        sram;           // battery-backed memory (misc console settings)
	uint8_t*	ansiFont;       // bootrom font (loaded from file)
	uint8_t*	sjisFont;
	uint32_t    rtcVal;         // last updated RTC value
	uint32_t    ad16;           // trace step
	char        uart[256];      // UART I/O buffer
	uint32_t    upos;           // UART buffer position (if > 0, UART buffer not empty)

	// helper variables used for EXI transfers
	int32_t     chan, sel;      // curent selected chan:device (sel=-1 no device)
	uint32_t    ad16_cmd;       // command for AD16
	bool        firstImm;       // first imm write is always command
	uint32_t    mxaddr;         // "address" inside MX chip for transfers
	bool        uartNE;

	bool        log;            // allow log EXI activities
	bool        osReport;       // allow UART debugger output (log not affecting this)

	uint8_t* bootrom;       // Descrambled (Thank you segher, you already have a place in heaven)
	size_t bootromSize;
	bool    BootromPresent;     // loaded and descrambled valid bootrom
};

typedef void (*EXITransferCallback)(void* ctx);

namespace Flipper
{
	class ExternalInterface
	{
		void exi_select(int chan);
		void write_csr(int chan, uint32_t data);

		static void exi_read_dummy(uint32_t addr, uint32_t* reg, void* ctx);
		static void exi_write_dummy(uint32_t addr, uint32_t data, void* ctx);

		static void exi0_read_csr(uint32_t addr, uint32_t* reg, void* ctx);
		static void exi1_read_csr(uint32_t addr, uint32_t* reg, void* ctx);
		static void exi2_read_csr(uint32_t addr, uint32_t* reg, void* ctx);
		static void exi0_write_csr(uint32_t addr, uint32_t data, void* ctx);
		static void exi1_write_csr(uint32_t addr, uint32_t data, void* ctx);
		static void exi2_write_csr(uint32_t addr, uint32_t data, void* ctx);

		static void exi0_read_madrh(uint32_t addr, uint32_t* reg, void* ctx);
		static void exi1_read_madrh(uint32_t addr, uint32_t* reg, void* ctx);
		static void exi2_read_madrh(uint32_t addr, uint32_t* reg, void* ctx);
		static void exi0_write_madrh(uint32_t addr, uint32_t data, void* ctx);
		static void exi1_write_madrh(uint32_t addr, uint32_t data, void* ctx);
		static void exi2_write_madrh(uint32_t addr, uint32_t data, void* ctx);
		static void exi0_read_madrl(uint32_t addr, uint32_t* reg, void* ctx);
		static void exi1_read_madrl(uint32_t addr, uint32_t* reg, void* ctx);
		static void exi2_read_madrl(uint32_t addr, uint32_t* reg, void* ctx);
		static void exi0_write_madrl(uint32_t addr, uint32_t data, void* ctx);
		static void exi1_write_madrl(uint32_t addr, uint32_t data, void* ctx);
		static void exi2_write_madrl(uint32_t addr, uint32_t data, void* ctx);

		static void exi0_read_lenh(uint32_t addr, uint32_t* reg, void* ctx);
		static void exi1_read_lenh(uint32_t addr, uint32_t* reg, void* ctx);
		static void exi2_read_lenh(uint32_t addr, uint32_t* reg, void* ctx);
		static void exi0_write_lenh(uint32_t addr, uint32_t data, void* ctx);
		static void exi1_write_lenh(uint32_t addr, uint32_t data, void* ctx);
		static void exi2_write_lenh(uint32_t addr, uint32_t data, void* ctx);
		static void exi0_read_lenl(uint32_t addr, uint32_t* reg, void* ctx);
		static void exi1_read_lenl(uint32_t addr, uint32_t* reg, void* ctx);
		static void exi2_read_lenl(uint32_t addr, uint32_t* reg, void* ctx);
		static void exi0_write_lenl(uint32_t addr, uint32_t data, void* ctx);
		static void exi1_write_lenl(uint32_t addr, uint32_t data, void* ctx);
		static void exi2_write_lenl(uint32_t addr, uint32_t data, void* ctx);

		void exi_write_cr(int chan, uint32_t data);

		static void exi0_read_cr(uint32_t addr, uint32_t* reg, void* ctx);
		static void exi1_read_cr(uint32_t addr, uint32_t* reg, void* ctx);
		static void exi2_read_cr(uint32_t addr, uint32_t* reg, void* ctx);
		static void exi0_write_cr(uint32_t addr, uint32_t data, void* ctx);
		static void exi1_write_cr(uint32_t addr, uint32_t data, void* ctx);
		static void exi2_write_cr(uint32_t addr, uint32_t data, void* ctx);

		static void exi0_read_datah(uint32_t addr, uint32_t* reg, void* ctx);
		static void exi1_read_datah(uint32_t addr, uint32_t* reg, void* ctx);
		static void exi2_read_datah(uint32_t addr, uint32_t* reg, void* ctx);
		static void exi0_write_datah(uint32_t addr, uint32_t data, void* ctx);
		static void exi1_write_datah(uint32_t addr, uint32_t data, void* ctx);
		static void exi2_write_datah(uint32_t addr, uint32_t data, void* ctx);
		static void exi0_read_datal(uint32_t addr, uint32_t* reg, void* ctx);
		static void exi1_read_datal(uint32_t addr, uint32_t* reg, void* ctx);
		static void exi2_read_datal(uint32_t addr, uint32_t* reg, void* ctx);
		static void exi0_write_datal(uint32_t addr, uint32_t data, void* ctx);
		static void exi1_write_datal(uint32_t addr, uint32_t data, void* ctx);
		static void exi2_write_datal(uint32_t addr, uint32_t data, void* ctx);

	public:
		ExternalInterface(HWConfig* config);
		~ExternalInterface();

		EXIState exi{};

		static void UnknownTransfer(void* ctx);
		static void ADTransfer(void* ctx);

		// for memcards and other external devices
		void EXIUpdateInterrupts();
		// connect device
		void EXIAttach(int chan);
		// disconnect device
		void EXIDetach(int chan);
	};
}