
#pragma once

#define SI_POLLING_INTERVAL     0x10000      // In Gekko ticks

// SI registers (all registers are 32-bit from the software side)

#define SI_CHAN0_OUTBUF     0x00      // Channel 0 Output Buffer
#define SI_CHAN0_INBUFH     0x04      // Channel 0 Input Buffer High
#define SI_CHAN0_INBUFL     0x08      // Channel 0 Input Buffer Low
#define SI_CHAN1_OUTBUF     0x0C      // Channel 1 Output Buffer
#define SI_CHAN1_INBUFH     0x10      // Channel 1 Input Buffer High
#define SI_CHAN1_INBUFL     0x14      // Channel 1 Input Buffer Low
#define SI_CHAN2_OUTBUF     0x18      // Channel 2 Output Buffer
#define SI_CHAN2_INBUFH     0x1C      // Channel 2 Input Buffer High
#define SI_CHAN2_INBUFL     0x20      // Channel 2 Input Buffer Low
#define SI_CHAN3_OUTBUF     0x24      // Channel 3 Output Buffer
#define SI_CHAN3_INBUFH     0x28      // Channel 3 Input Buffer High
#define SI_CHAN3_INBUFL     0x2C      // Channel 3 Input Buffer Low
#define SI_POLL             0x30      // Poll Register
#define SI_COMCSR           0x34      // Communication Control Status Register
#define SI_SR               0x38      // Status Register
#define SI_EXILK            0x3C      // EXI Clock Lock (unused)
#define SI_REG_MAX			0x40
#define SI_COMBUF           0x80      // Communication Buffer (128 bytes)

#define SI_POLL_REG         si.poll
#define SI_COMCSR_REG       si.comcsr
#define SI_SR_REG           si.sr

// SI Poll Register mask
#define SI_POLL_X(reg)          ((reg >>16) & 0x3ff)
#define SI_POLL_Y(reg)          ((reg >> 8) & 0xff)
#define SI_POLL_EN0             (1 << 7)
#define SI_POLL_EN1             (1 << 6)
#define SI_POLL_EN2             (1 << 5)
#define SI_POLL_EN3             (1 << 4)
#define SI_POLL_VBCPY0          (1 << 3)
#define SI_POLL_VBCPY1          (1 << 2)
#define SI_POLL_VBCPY2          (1 << 1)
#define SI_POLL_VBCPY3          (1 << 0)

// SI Communication Control Status Register mask
#define SI_COMCSR_TCINT         (1 << 31)
#define SI_COMCSR_TCINTMSK      (1 << 30)
#define SI_COMCSR_COMERR        (1 << 29)
#define SI_COMCSR_RDSTINT       (1 << 28)
#define SI_COMCSR_RDSTINTMSK    (1 << 27)
#define SI_COMCSR_OUTLEN(reg)   ((reg >> 16) & 0x7f)
#define SI_COMCSR_OUTLEN_MASK	0x007f0000
#define SI_COMCSR_INLEN(reg)    ((reg >>  8) & 0x7f)
#define SI_COMCSR_INLEN_MASK	0x00007f00
#define SI_COMCSR_CHAN(reg)     ((reg >> 1) & 3)
#define SI_COMCSR_CHAN_MASK		0x00000006
#define SI_COMCSR_TSTART        (1)

// SI Status Register mask
#define SI_SR_WR                (1 << 31)
#define SI_SR_RDST0             (1 << 29)
#define SI_SR_WRST0             (1 << 28)
#define SI_SR_NOREP0            (1 << 27)
#define SI_SR_COLL0             (1 << 26)
#define SI_SR_OVRUN0            (1 << 25)
#define SI_SR_UNRUN0            (1 << 24)
#define SI_SR_RDST1             (1 << 21)
#define SI_SR_WRST1             (1 << 20)
#define SI_SR_NOREP1            (1 << 19)
#define SI_SR_COLL1             (1 << 18)
#define SI_SR_OVRUN1            (1 << 17)
#define SI_SR_UNRUN1            (1 << 16)
#define SI_SR_RDST2             (1 << 13)
#define SI_SR_WRST2             (1 << 12)
#define SI_SR_NOREP2            (1 << 11)
#define SI_SR_COLL2             (1 << 10)
#define SI_SR_OVRUN2            (1 <<  9)
#define SI_SR_UNRUN2            (1 <<  8)
#define SI_SR_RDST3             (1 <<  5)
#define SI_SR_WRST3             (1 <<  4)
#define SI_SR_NOREP3            (1 <<  3)
#define SI_SR_COLL3             (1 <<  2)
#define SI_SR_OVRUN3            (1 <<  1)
#define SI_SR_UNRUN3            (1 <<  0)

// ---------------------------------------------------------------------------
// hardware API

namespace Flipper
{
	// SI state (registers and other data)
	struct SIState
	{
		volatile uint32_t            out[4], shdw[4];// out + shadows
		volatile uint32_t            poll;           // poll control
		volatile uint32_t            comcsr;         // CSR
		volatile uint32_t            sr;             // status
		volatile uint32_t            exilk;          // EXILK dummy
		uint8_t             combuf[128 + 32]; // communication buffer (+ overrun protection)

		PADState            pad[4];         // PAD state (inbuf replacement)
		bool                rumble[4];      // rumble support flags for every controller
		// filled when SI is inited, by checking PADSetRumble
		bool                log;            // do debugger log output
		int64_t             pollingTime;    // Saved Gekko TBR for polling
	};

	class SerialInterface
	{
		SIState si{};		//!< SI state (registers and other data)

		void SICommand(int chan, int outlen, int inlen, uint8_t* ptr);
		void SIClearInterrupt();

		void si_wr_out_hi(int chan, uint32_t data);
		void si_wr_out_lo(int chan, uint32_t mask, uint32_t data);
		static void si_wr_out0_hi(uint32_t addr, uint32_t data, void* ctx);
		static void si_wr_out0_lo(uint32_t addr, uint32_t data, void* ctx);
		static void si_rd_out0_hi(uint32_t addr, uint32_t* reg, void* ctx);
		static void si_rd_out0_lo(uint32_t addr, uint32_t* reg, void* ctx);
		static void si_wr_out1_hi(uint32_t addr, uint32_t data, void* ctx);
		static void si_wr_out1_lo(uint32_t addr, uint32_t data, void* ctx);
		static void si_rd_out1_hi(uint32_t addr, uint32_t* reg, void* ctx);
		static void si_rd_out1_lo(uint32_t addr, uint32_t* reg, void* ctx);
		static void si_wr_out2_hi(uint32_t addr, uint32_t data, void* ctx);
		static void si_wr_out2_lo(uint32_t addr, uint32_t data, void* ctx);
		static void si_rd_out2_hi(uint32_t addr, uint32_t* reg, void* ctx);
		static void si_rd_out2_lo(uint32_t addr, uint32_t* reg, void* ctx);
		static void si_wr_out3_hi(uint32_t addr, uint32_t data, void* ctx);
		static void si_wr_out3_lo(uint32_t addr, uint32_t data, void* ctx);
		static void si_rd_out3_hi(uint32_t addr, uint32_t* reg, void* ctx);
		static void si_rd_out3_lo(uint32_t addr, uint32_t* reg, void* ctx);

		void si_inh_hi(int chan, uint32_t* reg);
		void si_inh_lo(int chan, uint32_t mask, uint32_t* reg);
		void si_inl_hi(int chan, uint32_t* reg);
		void si_inl_lo(int chan, uint32_t* reg);
		static void si_inh0_hi(uint32_t addr, uint32_t* reg, void* ctx);
		static void si_inh0_lo(uint32_t addr, uint32_t* reg, void* ctx);
		static void si_inl0_hi(uint32_t addr, uint32_t* reg, void* ctx);
		static void si_inl0_lo(uint32_t addr, uint32_t* reg, void* ctx);
		static void si_inh1_hi(uint32_t addr, uint32_t* reg, void* ctx);
		static void si_inh1_lo(uint32_t addr, uint32_t* reg, void* ctx);
		static void si_inl1_hi(uint32_t addr, uint32_t* reg, void* ctx);
		static void si_inl1_lo(uint32_t addr, uint32_t* reg, void* ctx);
		static void si_inh2_hi(uint32_t addr, uint32_t* reg, void* ctx);
		static void si_inh2_lo(uint32_t addr, uint32_t* reg, void* ctx);
		static void si_inl2_hi(uint32_t addr, uint32_t* reg, void* ctx);
		static void si_inl2_lo(uint32_t addr, uint32_t* reg, void* ctx);
		static void si_inh3_hi(uint32_t addr, uint32_t* reg, void* ctx);
		static void si_inh3_lo(uint32_t addr, uint32_t* reg, void* ctx);
		static void si_inl3_hi(uint32_t addr, uint32_t* reg, void* ctx);
		static void si_inl3_lo(uint32_t addr, uint32_t* reg, void* ctx);

		static void write_sicom(uint32_t addr, uint32_t data, void* ctx);
		static void read_sicom(uint32_t addr, uint32_t* reg, void* ctx);

		static void write_poll_hi(uint32_t addr, uint32_t data, void* ctx);
		static void write_poll_lo(uint32_t addr, uint32_t data, void* ctx);
		static void read_poll_hi(uint32_t addr, uint32_t* reg, void* ctx);
		static void read_poll_lo(uint32_t addr, uint32_t* reg, void* ctx);

		static void write_commcsr_hi(uint32_t addr, uint32_t data, void* ctx);
		static void write_commcsr_lo(uint32_t addr, uint32_t data, void* ctx);
		static void read_commcsr_hi(uint32_t addr, uint32_t* reg, void* ctx);
		static void read_commcsr_lo(uint32_t addr, uint32_t* reg, void* ctx);

		static void write_sisr_hi(uint32_t addr, uint32_t data, void* ctx);
		static void write_sisr_lo(uint32_t addr, uint32_t data, void* ctx);
		static void read_sisr_hi(uint32_t addr, uint32_t* reg, void* ctx);
		static void read_sisr_lo(uint32_t addr, uint32_t* reg, void* ctx);

		static void write_exilk_hi(uint32_t addr, uint32_t data, void* ctx);
		static void write_exilk_lo(uint32_t addr, uint32_t data, void* ctx);
		static void read_exilk_hi(uint32_t addr, uint32_t* reg, void* ctx);
		static void read_exilk_lo(uint32_t addr, uint32_t* reg, void* ctx);

	public:
		SerialInterface(HWConfig* config);
		~SerialInterface();

		void SIPoll();
	};
}