#pragma once

// DI registers (32-bit from the software side)
#define DI_SR            0x00     // Status Register
#define DI_CVR           0x04     // Cover Register
#define DI_CMDBUF0       0x08     // Command Buffer 0
#define DI_CMDBUF1       0x0C     // Command Buffer 1
#define DI_CMDBUF2       0x10     // Command Buffer 2
#define DI_MAR           0x14     // DMA Memory Address Register
#define DI_LEN           0x18     // DMA Transfer Length Register
#define DI_CR            0x1C     // Control Register
#define DI_IMMBUF        0x20     // Immediate Data Buffer
#define DI_CFG           0x24     // Configuration Register
#define DI_REG_MAX	     0x28

#define DISR             di.sr
#define DICVR            di.cvr
#define DIMAR            di.mar
#define DILEN            di.len
#define DICR             di.cr

// DI Status Register mask
#define DI_SR_BRKINT     (1 << 6)
#define DI_SR_BRKINTMSK  (1 << 5)
#define DI_SR_TCINT      (1 << 4)
#define DI_SR_TCINTMSK   (1 << 3)
#define DI_SR_DEINT      (1 << 2)
#define DI_SR_DEINTMSK   (1 << 1)
#define DI_SR_BRK        (1 << 0)

// DI Cover Register mask
#define DI_CVR_CVRINT    (1 << 2)
#define DI_CVR_CVRINTMSK (1 << 1)
#define DI_CVR_CVR       (1 << 0)           // 0 = Cover is closed, 1 = Cover is open

// DI Control Register mask
#define DI_CR_RW         (1 << 2)           // 0 = Read Command (DDU->Host), 1 = Write Command (Host->DDU)
#define DI_CR_DMA        (1 << 1)           // 0 = Immediate Mode, 1 = DMA Mode
#define DI_CR_TSTART     (1 << 0)

#define DI_DIMAR_MASK    0x03ff'ffe0        // Valid bits of DIMAR
#define DI_DIMAR_MASK_HI 0x03ff
#define DI_DIMAR_MASK_LO 0xffe0

// ---------------------------------------------------------------------------
// hardware API

namespace Flipper
{
	class StateWriter;
	class StateReader;

	// DI state (registers and other data)
	struct DIState
	{
		// DI registers
		volatile uint16_t        sr, cvr, cr;
		volatile uint32_t        mar, len;
		volatile uint8_t         cmdbuf[12];
		volatile uint8_t         immbuf[4];
		volatile uint16_t        cfg;
		uint8_t         dmaFifo[32];

		int             dduToHostByteCounter;
		int             hostToDduByteCounter;

		bool            log;
	};

	class DiskInterface
	{
		DIState di{};		//!< DI state (registers and other data)

		static void DIOpenCover(void* ctx);
		static void DICloseCover(void* ctx);
		static void DIErrorCallback(void* ctx);
		void DIBreak();
		void DITransferComplete();
		static uint8_t DIHostToDduCallbackCommand(void* ctx);
		static uint8_t DIHostToDduCallbackData(void* ctx);
		static void DIDduToHostCallback(uint8_t data, void* ctx);
		/// <summary>
		/// Re-evaluate the aggregate Processor Interface line (PI_INTERRUPT_DI) from the three
		/// internal DISR causes and their masks. Called wherever a cause or a mask changes.
		/// </summary>
		void DIUpdateInt();

		void write_sr(uint16_t data);
		void write_cr(uint16_t data);
		void write_cvr(uint16_t data);
		void read_cvr(uint32_t* reg);
		static void DIRegRead(uint32_t addr, uint32_t* reg, void* context);
		static void DIRegWrite(uint32_t addr, uint32_t data, void* context);

	public:
		DiskInterface(Flipper* flipper, HWConfig* config);
		~DiskInterface();

		// -- save states -------------------------------------------------------------------

		/// <summary>
		/// Write the disk interface into the save state section: every register the guest can
		/// see (the status, cover and control registers, the DMA address and length, the
		/// command and immediate buffers and the configuration word) and the two pieces of the
		/// transfer protocol that outlive a single register write - the 32-byte DMA FIFO, its
		/// position taken from the two byte counters the drive's callbacks advance. The disc
		/// itself, its file and the drive's own transfer state belong to the frontend and to
		/// the DVD block, not here.
		/// </summary>
		void SaveState(SaveStates::StateWriter& writer) const;

		/// <summary>
		/// Read it back. The registers are decoded state, so they go back as they are - in
		/// particular the control register is not replayed through the write path, which would
		/// start a command the drive is not being asked to run. The aggregate interrupt line the
		/// CPU sees is derived from the restored causes and masks rather than stored, so it is
		/// re-derived here (DIUpdateInt) once everything is in; nothing else has to be called
		/// after the load.
		/// </summary>
		void LoadState(SaveStates::StateReader& reader);

		//! The DI state, for the debug interface (`diregs` and the debugui2 "Disk" panel).
		const DIState& State() const { return di; }
	};
}