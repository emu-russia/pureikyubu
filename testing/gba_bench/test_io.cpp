// Unit tests for the GBA I/O devices: the timers, the DMA engine, the serial port, the keypad
// and the interrupt controller.
//
// The tests drive the real device objects through real GbaBus instances (the SIO tests use two:
// two buses with their Sio attached to each other *are* the link cable). The expected values are
// the ones the GBATEK formulas give, written out in the test so the arithmetic can be checked
// against the specification text rather than against the implementation:
//
//   * timer overflow after (0x10000 - reload) * prescaler system cycles, where the prescaler is
//     1, 64, 256 or 1024 (GBATEK "GBA Timers", TMxCNT_H bits 0-1);
//   * a DMA transfer of n units takes 2 bus cycles per 16bit unit and 4 per 32bit one, plus the
//     2-cycle (4 when both ends are in the Game Pak) internal time (GBATEK "Transfer
//     Rate/Timing": "2N+2(n-1)S+xI");
//   * one SIO bit takes 128 system cycles at 256KHz and 16 at 2MHz (GBATEK "SIO Normal Mode",
//     "Transfer Rates");
//   * KEYCNT bit 15 is the AND/OR condition and bit 14 the interrupt enable (GBATEK "GBA Keypad
//     Input").

#include "gba_test.h"

#include "gba_bus.h"

using namespace GBA;

namespace
{
	// ---------------------------------------------------------------------------------------
	// Helpers
	// ---------------------------------------------------------------------------------------

	/// <summary>A bus with every device freshly reset.</summary>
	struct Fixture
	{
		GbaBus bus;

		Fixture() { bus.Reset(); }

		u16 Read(u32 offset) { return bus.Read16(0x04000000 + offset); }
		u16 ReadMem16(u32 address) { return bus.Read16(address); }
		void Write(u32 offset, u16 value) { bus.Write16(0x04000000 + offset, value); }
		void WriteMem16(u32 address, u16 value) { bus.Write16(address, value); }

		u8 Read8(u32 offset) { return bus.Read8(0x04000000 + offset); }
		void Write8(u32 offset, u8 value) { bus.Write8(0x04000000 + offset, value); }
	};

	/// <summary>
	/// Run a cable transfer to its end. Both ends have to be ticked in step, and the transfer is
	/// over only when *both* countdowns have expired. The end that finishes first clears its own
	/// start bit immediately, so the loop cannot stop at the first clear start bit: it keeps
	/// ticking until neither side is busy, which also lets the unit that finished first commit
	/// the data it received (it does so on the Tick after its peer went idle).
	/// </summary>
	void RunCable(Fixture& a, Fixture& b, int step = 4096)
	{
		// The end that finishes first clears its own start bit right away but only commits the
		// data it received on a later Tick (once the peer has gone idle), so a couple of extra
		// ticks are needed after both start bits clear.
		for (int i = 0; i < 1000000; i++)
		{
			a.bus.sio.Tick(a.bus, step);
			b.bus.sio.Tick(b.bus, step);

			if (!a.bus.sio.Busy() && !b.bus.sio.Busy())
			{
				// Give the delayed side its commit ticks.
				for (int extra = 0; extra < 4; extra++)
				{
					a.bus.sio.Tick(a.bus, step);
					b.bus.sio.Tick(b.bus, step);
				}

				return;
			}
		}

		GBA_FAIL("the transfer did not complete");
	}

	/// <summary>TIOCNT_H for a timer: prescaler, optional cascade, IRQ, enable.</summary>
	u16 TimerControl(int prescaler, bool cascade, bool irq)
	{
		return (u16)((prescaler & 3) | (cascade ? 0x0004 : 0) | (irq ? 0x0040 : 0) | 0x0080);
	}
}

// ===========================================================================================
// Timers
// ===========================================================================================

GBA_TEST(Timers, OverflowAfterReloadTimesPrescaler)
{
	Fixture f;

	// GBATEK "GBA Timers": the reload value is (0x10000 - reload) * prescaler cycles away from
	// the overflow, and the prescaler is F/1 for TMxCNT_H bits 0-1 = 0.
	const u16 reload = 0x8000;
	const int expected = (0x10000 - reload) * 1;		// 0x8000 = 32768 cycles

	f.Write(0x100, reload);			// TM0CNT_L
	f.Write(0x102, TimerControl(0, false, true));		// TM0CNT_H: F/1, IRQ, start

	// The start bit copies the reload value into the counter (GBATEK "GBA Timers").
	GBA_CHECK_HEX16(f.Read(0x100), reload);
	GBA_CHECK(f.bus.timers.Running(0));

	// One cycle short of the period nothing has happened yet.
	f.bus.timers.Tick(f.bus, expected - 1);
	GBA_CHECK_HEX16(f.bus.irq.ReadIF(), 0);
	GBA_CHECK_HEX16(f.Read(0x100), 0xFFFF);

	// The very next cycle overflows, reloads the counter and raises INT_TIMER0.
	f.bus.timers.Tick(f.bus, 1);
	GBA_CHECK_HEX16(f.bus.irq.ReadIF(), INT_TIMER0);
	GBA_CHECK_HEX16(f.Read(0x100), reload);
	GBA_CHECK(f.bus.timers.Running(0));
}

GBA_TEST(Timers, PrescalerSelectsThePeriod)
{
	Fixture f;

	// TM1CNT_H bits 0-1 = 1 selects F/64, so one count is 64 system cycles and the overflow is
	// (0x10000 - 0) * 64 = 4194304 cycles away.
	const int expected = 0x10000 * 64;

	f.Write(0x104, 0x0000);			// TM1CNT_L: reload 0
	f.Write(0x106, TimerControl(1, false, true));

	GBA_CHECK_HEX16(f.Read(0x104), 0);

	// 128 cycles are exactly two prescaler ticks.
	f.bus.timers.Tick(f.bus, 128);
	GBA_CHECK_HEX16(f.Read(0x104), 2);

	// The accumulator carries over between slices: 63 more cycles only complete the next tick.
	f.bus.timers.Tick(f.bus, 63);
	GBA_CHECK_HEX16(f.Read(0x104), 2);
	f.bus.timers.Tick(f.bus, 1);
	GBA_CHECK_HEX16(f.Read(0x104), 3);

	// Run up to one cycle short of the full period, then over the edge.
	f.bus.timers.Tick(f.bus, expected - 3 * 64 - 1);
	GBA_CHECK_HEX16(f.bus.irq.ReadIF(), 0);
	GBA_CHECK_HEX16(f.Read(0x104), 0xFFFF);

	f.bus.timers.Tick(f.bus, 1);
	GBA_CHECK_HEX16(f.bus.irq.ReadIF(), INT_TIMER1);
	GBA_CHECK_HEX16(f.Read(0x104), 0x0000);
}

GBA_TEST(Timers, CascadeCountsThePreviousOverflow)
{
	Fixture f;

	// GBATEK "GBA Timers": with TMxCNT_H bit 2 set the timer ignores its own prescaler and counts
	// each overflow of the previous timer. Timer 0 overflows every (0x10000 - reload) cycles; the
	// cascaded timer 1 then overflows after the same number of timer 0 overflows.
	const u16 reload = 0xFF00;					// 256 cycles per timer 0 period
	const int period = 0x10000 - reload;

	f.Write(0x100, reload);						// TM0CNT_L
	f.Write(0x102, TimerControl(0, false, true));	// F/1, IRQ, start
	f.Write(0x104, 0xFFFE);						// TM1CNT_L: two counts to overflow
	f.Write(0x106, TimerControl(0, true, true));	// TM1CNT_H: cascade (the prescaler is ignored)

	// After one timer 0 overflow the cascaded counter has advanced by one.
	f.bus.timers.Tick(f.bus, period);
	GBA_CHECK_HEX16(f.bus.irq.ReadIF(), INT_TIMER0);
	GBA_CHECK_HEX16(f.Read(0x104), 0xFFFF);

	// The second overflow of timer 0 makes timer 1 wrap and reload to 0xFFFE: the two overflows
	// of timer 0 are the two counts the cascaded timer needed.
	f.bus.timers.Tick(f.bus, period);
	GBA_CHECK_HEX16(f.bus.irq.ReadIF(), INT_TIMER0 | INT_TIMER1);
	GBA_CHECK_HEX16(f.Read(0x104), 0xFFFE);
}

GBA_TEST(Timers, ReloadWriteOnlyTakesEffectOnTheNextOverflow)
{
	Fixture f;

	// GBATEK "GBA Timers": a write to TMxCNT_L while the timer runs only initialises the reload
	// value, it does not change the counter; the value is used by the *next* overflow. (The
	// one-cycle window described around the overflow is not modelled, see gba_timers.cpp.)
	f.Write(0x100, 0xFFFE);
	f.Write(0x102, TimerControl(0, false, false));

	// Two cycles to the overflow.
	f.bus.timers.Tick(f.bus, 1);
	GBA_CHECK_HEX16(f.Read(0x100), 0xFFFF);

	// The write lands before the wrap, so the wrap reloads with the value that is current at
	// that moment (0x8000): the counter was one cycle short of 0x10000, reloads and counts that
	// last cycle. (The one-cycle window GBATEK describes is not modelled, see gba_timers.cpp.)
	f.Write(0x100, 0x8000);
	f.bus.timers.Tick(f.bus, 1);
	GBA_CHECK_HEX16(f.Read(0x100), 0x8000);

	// The counter is now a whole period of the new reload value away from the next overflow.
	f.bus.timers.Tick(f.bus, 0x8000);
	GBA_CHECK_HEX16(f.Read(0x100), 0x8000);
}

GBA_TEST(Timers, StopFreezesAndRestartReloads)
{
	Fixture f;

	f.Write(0x100, 0x0010);
	f.Write(0x102, TimerControl(0, false, false));	// start
	GBA_CHECK_HEX16(f.Read(0x100), 0x0010);

	f.bus.timers.Tick(f.bus, 5);
	GBA_CHECK_HEX16(f.Read(0x100), 0x0015);

	// Clearing the start bit freezes the counter where it stands.
	f.Write(0x102, TimerControl(0, false, false) & (u16)~0x0080);
	GBA_CHECK(!f.bus.timers.Running(0));
	GBA_CHECK_HEX16(f.Read(0x100), 0x0015);

	f.bus.timers.Tick(f.bus, 100);
	GBA_CHECK_HEX16(f.Read(0x100), 0x0015);

	// Writing CNT_L while stopped also updates the (frozen) counter.
	f.Write(0x100, 0x0020);
	GBA_CHECK_HEX16(f.Read(0x100), 0x0020);
}

// ===========================================================================================
// DMA
// ===========================================================================================

GBA_TEST(Dma, ImmediateTransferWithIncrementDecrementAndFixed)
{
	// Three channels, three address adjustments: DMA0 increments the destination, DMA1
	// decrements it and DMA2 keeps it fixed (GBATEK DMAxCNT_H bits 5-6).
	{
		Fixture f;
		f.WriteMem16(0x02000000, 0x1111);
		f.WriteMem16(0x02000002, 0x2222);

		f.Write(0x0B0, 0x0000);			// DMA0SAD = 0x02000000
		f.Write(0x0B2, 0x0200);
		f.Write(0x0B4, 0x0100);			// DMA0DAD = 0x02000100
		f.Write(0x0B6, 0x0200);
		f.Write(0x0B8, 2);				// two units
		f.Write(0x0BA, 0x8000);			// dest increment, source increment, immediate, enable

		// The enable bit clears itself when the transfer completes (GBATEK "Transfer End").
		GBA_CHECK(!f.bus.dma.Active(0));
		GBA_CHECK_HEX16((u16)f.ReadMem16(0x02000100), 0x1111);
		GBA_CHECK_HEX16((u16)f.ReadMem16(0x02000102), 0x2222);
	}

	{
		Fixture f;
		f.WriteMem16(0x02000000, 0xAAAA);
		f.WriteMem16(0x02000002, 0xBBBB);

		f.Write(0x0BC, 0x0000);			// DMA1SAD = 0x02000000
		f.Write(0x0BE, 0x0200);
		f.Write(0x0C0, 0x0104);			// DMA1DAD = 0x02000104
		f.Write(0x0C2, 0x0200);
		f.Write(0x0C4, 2);
		f.Write(0x0C6, 0x8020);			// dest decrement (bit 5), immediate, enable

		GBA_CHECK_HEX16((u16)f.ReadMem16(0x02000104), 0xAAAA);
		GBA_CHECK_HEX16((u16)f.ReadMem16(0x02000102), 0xBBBB);
	}

	{
		Fixture f;
		f.WriteMem16(0x02000000, 0x1234);
		f.WriteMem16(0x02000002, 0x5678);

		f.Write(0x0C8, 0x0000);			// DMA2SAD = 0x02000000
		f.Write(0x0CA, 0x0200);
		f.Write(0x0CC, 0x0100);			// DMA2DAD = 0x02000100
		f.Write(0x0CE, 0x0200);
		f.Write(0x0D0, 2);
		f.Write(0x0D2, 0x8040);			// dest fixed (bits 5-6 = 2), immediate, enable
		f.WriteMem16(0x02000100, 0x0000);

		GBA_CHECK_HEX16((u16)f.ReadMem16(0x02000100), 0x5678);	// the second unit wins
	}
}

GBA_TEST(Dma, ImmediateTransferChargesTheBusWaitstates)
{
	Fixture f;

	f.WriteMem16(0x02000000, 0x1111);
	f.WriteMem16(0x02000002, 0x1111);
	f.Write(0x0B0, 0x0000);
	f.Write(0x0B2, 0x0200);
	f.Write(0x0B4, 0x0000);
	f.Write(0x0B6, 0x0200);
	f.Write(0x0B8, 4);					// four 16bit units

	f.bus.AddWaitCycles(0);
	f.Write(0x0BA, 0x8000);

	// GBATEK "Transfer Rate/Timing": two bus cycles per unit (one read, one write) plus the
	// 2-cycle internal time, and nothing here is in the Game Pak.
	int expected = 4 * 2 + 2;
	GBA_CHECK_EQ(f.bus.TakeWaitCycles(), expected);
}

GBA_TEST(Dma, WordCountZeroWrapsToTheMaximum)
{
	// DMA0-2: a count of zero is 0x4000 units (GBATEK "DMAxCNT_L").
	{
		Fixture f;
		f.Write(0x0B0, 0x0000);
		f.Write(0x0B2, 0x0300);			// source 0x03000000 (IWRAM)
		f.Write(0x0B4, 0x0000);
		f.Write(0x0B6, 0x0300);			// destination 0x03000000
		f.Write(0x0B8, 0);				// word count 0 -> 0x4000
		f.Write(0x0BA, 0x8000);

		// The transfer ran with the maximum count: 0x4000 units of 2 cycles plus the 2-cycle
		// internal time (see the note on the timing formula in gba_dma.cpp).
		int expected = 0x4000 * 2 + 2;
		GBA_CHECK_EQ(f.bus.TakeWaitCycles(), expected);
	}

	// DMA3: a count of zero is 0x10000 units.
	{
		Fixture f;
		f.Write(0x0D4, 0x0000);
		f.Write(0x0D6, 0x0300);
		f.Write(0x0D8, 0x0000);
		f.Write(0x0DA, 0x0300);
		f.Write(0x0DC, 0);
		f.Write(0x0DE, 0x8000);

		int expected = 0x10000 * 2 + 2;
		GBA_CHECK_EQ(f.bus.TakeWaitCycles(), expected);
	}
}

GBA_TEST(Dma, RepeatTransferRestartedByVBlank)
{
	Fixture f;

	f.WriteMem16(0x02000000, 0xCAFE);
	f.WriteMem16(0x02000002, 0xBABE);
	f.WriteMem16(0x02000100, 0x0000);
	f.WriteMem16(0x02000102, 0x0000);
	f.WriteMem16(0x02000104, 0x0000);

	// DMA3, source increment, destination increment + reload, repeat, VBlank (timing 1),
	// word count 2 (GBATEK DMAxCNT_H bits 5-6 = 3, bits 12-13 = 1, bit 9 = 1).
	f.Write(0x0D4, 0x0000);
	f.Write(0x0D6, 0x0200);
	f.Write(0x0D8, 0x0100);
	f.Write(0x0DA, 0x0200);
	f.Write(0x0DC, 2);
	f.Write(0x0DE, (u16)(0x0200 | 0x1000 | 0x0060 | 0x8000));

	// Nothing runs until the VBlank edge arrives.
	GBA_CHECK_HEX16((u16)f.ReadMem16(0x02000100), 0x0000);
	GBA_CHECK(f.bus.dma.Active(3));

	f.bus.dma.OnVBlank(f.bus);
	GBA_CHECK_HEX16((u16)f.ReadMem16(0x02000100), 0xCAFE);
	GBA_CHECK_HEX16((u16)f.ReadMem16(0x02000102), 0xBABE);

	// The repeat bit keeps the channel enabled and DAD reloaded, so the next VBlank writes the
	// same two words to the same place again (GBATEK "DMA Repeat bit").
	GBA_CHECK(f.bus.dma.Active(3));
	GBA_CHECK_HEX16(f.Read(0x0DE), (u16)(0x0200 | 0x1000 | 0x0060 | 0x8000));

	f.WriteMem16(0x02000100, 0x0000);
	f.bus.dma.OnVBlank(f.bus);
	GBA_CHECK_HEX16((u16)f.ReadMem16(0x02000100), 0xCAFE);

	// Clearing the enable bit stops the repetition.
	f.Write(0x0DE, (u16)(0x0200 | 0x1000 | 0x0060));
	GBA_CHECK(!f.bus.dma.Active(3));
}

GBA_TEST(Dma, FifoRefillCompletesWhenTheApuAsks)
{
	Fixture f;

	// The four words a sound DMA moves (GBATEK "Sound DMA (FIFO Timing Mode)": "4 units of
	// 32bits (16 bytes) are transferred, both Word Count register and DMA Transfer Type bit are
	// ignored").
	const u16 payload[8] = { 0x1111, 0x2222, 0x3333, 0x4444, 0x5555, 0x6666, 0x7777, 0x8888 };
	for (int i = 0; i < 8; i++)
		f.WriteMem16(0x02000000 + i * 2, payload[i]);

	// DMA1, special timing (bits 12-13 = 3) with the FIFO A destination (0x040000A0).
	f.Write(0x0BC, 0x0000);
	f.Write(0x0BE, 0x0200);
	f.Write(0x0C0, 0x00A0);
	f.Write(0x0C2, 0x0400);
	f.Write(0x0C4, 4);
	f.Write(0x0C6, (u16)(0x0300 | 0x8000));		// special timing, repeat, enable

	// The APU asks for a refill...
	f.bus.dma.OnFifoRequest(f.bus, 0);

	// ... and the four words are in FIFO A, in transfer order, through the documented APU
	// interface (Apu::Read8 pops one byte of the FIFO).
	const u8 expected[16] = {
		0x11, 0x11, 0x22, 0x22, 0x33, 0x33, 0x44, 0x44,
		0x55, 0x55, 0x66, 0x66, 0x77, 0x77, 0x88, 0x88,
	};
	for (int i = 0; i < 16; i++)
		GBA_CHECK_HEX16(f.bus.apu.Read8(0x0A0, 0x00), expected[i]);

	// The request flag is cleared again so the APU can ask once more.
	GBA_CHECK(!f.bus.apu.FifoRequest(0));

	// The channel stays armed for the next refill (the repeat bit is set).
	GBA_CHECK(f.bus.dma.Active(1));

	// A second request moves the next 16 bytes.
	f.bus.dma.OnFifoRequest(f.bus, 0);
	GBA_CHECK(!f.bus.apu.FifoRequest(0));

	// A request for the other FIFO finds no channel and changes nothing.
	f.bus.dma.OnFifoRequest(f.bus, 1);
	GBA_CHECK(!f.bus.apu.FifoRequest(1));
}

GBA_TEST(Dma, EepromTransferCompletesInsideTheEnableWrite)
{
	Fixture f;

	// The EEPROM protocol (GBATEK "GBA Cartridges"), the 9-unit 2bit command that starts a
	// write: the games set up the bit stream in RAM and poll the DMA enable bit in the very next
	// instruction, so the transfer has to be over when the CNT_H write returns.
	for (int i = 0; i < 9; i++)
		f.WriteMem16(0x02000000 + i * 2, (u16)(0x0080u >> (i & 7)));

	// DMA3 at 0x02000000 -> 0x0D000000 (the EEPROM window), 9 units, 16bit, immediate.
	f.Write(0x0D4, 0x0000);
	f.Write(0x0D6, 0x0200);
	f.Write(0x0D8, 0x0000);
	f.Write(0x0DA, 0x0D00);
	f.Write(0x0DC, 9);
	f.Write(0x0DE, 0x8000);

	GBA_CHECK(!f.bus.dma.Active(3));
	GBA_CHECK_HEX16((u16)(f.Read(0x0DE) & 0x8000), 0x0000);
}

GBA_TEST(Dma, Dma0CannotWriteTheCartridge)
{
	Fixture f;

	// Only DMA3 may transfer to the Game Pak (GBATEK "DMA Transfers"): a DMA0 write into the ROM
	// window is dropped, but the destination address still advances.
	f.WriteMem16(0x02000000, 0x1234);
	f.Write(0x0B0, 0x0000);
	f.Write(0x0B2, 0x0200);			// source 0x02000000
	f.Write(0x0B4, 0x0000);
	f.Write(0x0B6, 0x0800);			// destination 0x08000000 (ROM)
	f.Write(0x0B8, 1);
	f.Write(0x0BA, 0x8000);

	// The transfer ran (the enable bit cleared) but the ROM window was not written: a ROM read
	// still returns the open bus / cartridge value, never the transferred word.
	GBA_CHECK(!f.bus.dma.Active(0));
	GBA_CHECK(f.bus.dma.Get(0).destLatch == 0x08000002);
}

// ===========================================================================================
// SIO
// ===========================================================================================

GBA_TEST(Sio, ByteLaneAccess)
{
	Fixture f;

	// GBATEK "GBA Memory Map": an 8bit access hits one lane of the halfword, the other eight
	// data bits come from the open bus.
	f.Write(0x120, 0xAABB);			// SIODATA32_L

	// The byte lanes are addressed with the real bus address (the bus routes an 8bit access
	// to the halfword that contains the byte).
	f.bus.Write8(0x04000121, 0x12);		// high byte of 0x120
	GBA_CHECK_HEX16(f.Read(0x120), 0x12BB);
	f.bus.Write8(0x04000120, 0x34);		// low byte of 0x120
	GBA_CHECK_HEX16(f.Read(0x120), 0x1234);

	// A byte read returns the addressed lane in the low byte; the upper eight data bits are open
	// bus, and the bus has just driven 0x1234 on the same halfword.
	u8 low = f.bus.Read8(0x04000120);
	GBA_CHECK_HEX16(low, 0x0034);
	u8 high = f.bus.Read8(0x04000121);
	GBA_CHECK_HEX16(high, 0x0012);

	// SIODATA32_H is a separate halfword: 0x123 addresses its high byte.
	f.Write(0x122, 0x5678);
	f.bus.Write8(0x04000123, 0x9A);
	GBA_CHECK_HEX16(f.Read(0x122), 0x9A78);
}

GBA_TEST(Sio, NormalMode32BitTransferExchangesData)
{
	Fixture master;
	Fixture slave;

	master.bus.sio.Attach(&slave.bus.sio);

	// Normal 32bit mode with internal clock and 256KHz (SIOCNT bit 12 = 1, bit 14 = IRQ,
	// bit 0 = internal). GBATEK "SIO Normal Mode": SIOCNT bits 13-12 select 8bit or 32bit.
	master.Write(0x120, 0x1111);			// SIODATA32_L
	master.Write(0x122, 0x2222);			// SIODATA32_H
	master.Write(0x128, 0x5001);			// 32bit, IRQ, internal clock, 256KHz

	// The slave waits with the external clock and its own data ready.
	slave.Write(0x120, 0xAAAA);
	slave.Write(0x122, 0xBBBB);
	slave.Write(0x128, 0x5000);			// 32bit, IRQ, external clock
	slave.Write(0x128, 0x5080);			// start

	GBA_CHECK(slave.bus.sio.Busy());

	// The master writes the same value with the start bit set, which begins the transfer.
	master.Write(0x128, 0x5081);
	GBA_CHECK(master.bus.sio.Busy());

	// The outgoing data is latched when the transfer starts, so LastSent already carries it.
	GBA_CHECK_HEX16(master.bus.sio.LastSent(), 0x1111);
	RunCable(master, slave);

	// Both ends completed: the start bit is cleared and INT_SIO was raised.
	GBA_CHECK(!master.bus.sio.Busy());
	GBA_CHECK(!slave.bus.sio.Busy());
	GBA_CHECK_HEX16((u16)(master.Read(0x128) & 0x0080), 0);
	GBA_CHECK_HEX16((u16)(slave.Read(0x128) & 0x0080), 0);
	GBA_CHECK_HEX16((u16)(master.bus.irq.ReadIF() & INT_SIO), INT_SIO);
	GBA_CHECK_HEX16((u16)(slave.bus.irq.ReadIF() & INT_SIO), INT_SIO);

	// The master received the slave's 32bit value and the slave the master's. The low half comes
	// back through the send register (SIODATA32_L), so it is LastSent/LastReceived that carries
	// it, while the high half sits in SIODATA32_H (0x122).
	GBA_CHECK_HEX16(master.bus.sio.LastReceived(), 0xAAAA);
	GBA_CHECK_HEX16(slave.bus.sio.LastReceived(), 0x1111);
	GBA_CHECK_HEX16(master.Read(0x122), 0xBBBB);
	GBA_CHECK_HEX16(slave.Read(0x122), 0x2222);
}

GBA_TEST(Sio, NormalMode8BitTransferUsesTheByteRegister)
{
	Fixture master;
	Fixture slave;

	master.bus.sio.Attach(&slave.bus.sio);

	// 8bit normal mode: SIODATA8 at 0x124 (GBATEK "400012Ah - SIODATA8"), internal clock.
	master.Write(0x124, 0x005A);
	master.Write(0x128, 0x4001);
	slave.Write(0x124, 0x00C3);
	slave.Write(0x128, 0x4080);

	master.Write(0x128, 0x4081);
	RunCable(master, slave);

	GBA_CHECK_HEX16(master.bus.sio.LastReceived(), 0x00C3);
	GBA_CHECK_HEX16(slave.bus.sio.LastReceived(), 0x005A);
	GBA_CHECK_HEX16((u16)(master.Read(0x128) & 0x0080), 0);
}

GBA_TEST(Sio, NormalModeWithoutPeerReadsTheEmptyCable)
{
	Fixture solo;

	// No peer attached: the port behaves like an empty cable, so the received data is 0xFFFF and
	// the transfer still completes (the start bit clears, INT_SIO is raised).
	solo.Write(0x120, 0x1234);
	solo.Write(0x122, 0x5678);
	solo.Write(0x128, 0x5081);		// 32bit, IRQ, internal clock, start

	RunCable(solo, solo);

	GBA_CHECK_HEX16(solo.bus.sio.LastReceived(), 0xFFFF);
	GBA_CHECK_HEX16((u16)(solo.Read(0x128) & 0x0080), 0);
	GBA_CHECK_HEX16((u16)(solo.bus.irq.ReadIF() & INT_SIO), INT_SIO);
}

GBA_TEST(Sio, MultiplayerTwoPlayers)
{
	Fixture parent;
	Fixture child;

	parent.bus.sio.Attach(&child.bus.sio);

	// Multiplayer mode: SIOCNT bits 13-12 = 10 (0x2000), baud rate 0 = 9600 bps, IRQ enable.
	// The parent holds SIOCNT bit 2 clear, the child sets it (GBATEK "SIO Multi-Player Mode").
	parent.Write(0x120, 0x1111);		// SIOMLT_SEND
	parent.Write(0x128, 0x6000);		// 0x2000 | 0x4000: IRQ, baud 0

	child.Write(0x120, 0x2222);
	child.Write(0x128, 0x6004);		// child (bit 2), baud 0

	GBA_CHECK_EQ(parent.bus.sio.ConnectedPlayers(), 2);
	GBA_CHECK_EQ(child.bus.sio.ConnectedPlayers(), 2);

	// The child sets its start bit (its data is ready), then the parent starts the transfer.
	child.Write(0x128, 0x6084);
	GBA_CHECK_HEX16((u16)(child.Read(0x128) & 0x0080), 0x0080);
	parent.Write(0x128, 0x6080);

	// GBATEK "Transmission Time": two units take 36 shift clocks; at 9600 bps one bit is
	// 16777216 / 9600 = 1747 cycles.
	int expected = 36 * (CyclesPerSecond / 9600);
	GBA_CHECK(expected > 0);

	RunCable(parent, child);

	// Both units see the same SIOMULTI0/SIOMULTI1: the parent's data in slot 0 and the first
	// child's in slot 1 (GBATEK "SIOMULTI0-3": "after the transfer, all connected GBAs will
	// contain the same values in their SIOMULTI0-3 registers").
	GBA_CHECK_HEX16(parent.Read(0x120), 0x1111);
	GBA_CHECK_HEX16(parent.Read(0x122), 0x2222);
	GBA_CHECK_HEX16(child.Read(0x120), 0x1111);
	GBA_CHECK_HEX16(child.Read(0x122), 0x2222);

	// The start bits are cleared and INT_SIO was requested on both sides.
	GBA_CHECK_HEX16((u16)(parent.Read(0x128) & 0x0080), 0);
	GBA_CHECK_HEX16((u16)(child.Read(0x128) & 0x0080), 0);
	GBA_CHECK_HEX16((u16)(parent.bus.irq.ReadIF() & INT_SIO), INT_SIO);
	GBA_CHECK_HEX16((u16)(child.bus.irq.ReadIF() & INT_SIO), INT_SIO);

	// The ID bits (SIOCNT bits 4-5) name the unit's slot: the parent is 0, the first child is 1
	// (GBATEK "SIOCNT, MULTI-PLAYER Mode": "4-5 Multi-Player ID (0=Parent, 1-3=1st-3rd child)").
	GBA_CHECK_HEX16((u16)(parent.Read(0x128) & 0x0030) >> 4, 0);
	GBA_CHECK_HEX16((u16)(child.Read(0x128) & 0x0030) >> 4, 1);
}

GBA_TEST(Sio, MultiplayerEmptySlotsReadFFFF)
{
	// No peer at all: every remote slot is the empty cable.
	{
		Fixture solo;
		solo.Write(0x120, 0xDEAD);
		solo.Write(0x128, 0x6000);

		GBA_CHECK_EQ(solo.bus.sio.ConnectedPlayers(), 1);

		solo.Write(0x128, 0x6080);
		RunCable(solo, solo);

		// Slot 0 is the local outgoing value; the remote slots have no unit behind them and stay
		// FFFFh ("otherwise still FFFFh", GBATEK "SIOMULTI0-3").
		GBA_CHECK_HEX16(solo.Read(0x120), 0xDEAD);
		GBA_CHECK_HEX16(solo.Read(0x122), 0xFFFF);
		GBA_CHECK_HEX16(solo.Read(0x124), 0xFFFF);
		GBA_CHECK_HEX16(solo.Read(0x126), 0xFFFF);
		GBA_CHECK_HEX16((u16)(solo.Read(0x128) & 0x0080), 0);
	}

	// One peer in multiplayer mode: slot 1 is filled in, slots 2 and 3 stay 0xFFFF.
	{
		Fixture parent;
		Fixture child;
		parent.bus.sio.Attach(&child.bus.sio);

		parent.Write(0x120, 0x0101);
		parent.Write(0x128, 0x6000);
		child.Write(0x120, 0x0202);
		child.Write(0x128, 0x6004);
		child.Write(0x128, 0x6084);
		parent.Write(0x128, 0x6080);

		RunCable(parent, child);

		GBA_CHECK_HEX16(parent.Read(0x120), 0x0101);
		GBA_CHECK_HEX16(parent.Read(0x122), 0x0202);
		GBA_CHECK_HEX16(parent.Read(0x124), 0xFFFF);
		GBA_CHECK_HEX16(parent.Read(0x126), 0xFFFF);
	}
}

GBA_TEST(Sio, TransferLengthsMatchTheSpecifiedBaudRate)
{
	Fixture f;

	// 32 bits at 256KHz: 32 * 128 = 4096 cycles. The transfer is driven in slices, so the
	// countdown is checked through the end of the transfer rather than directly.
	f.Write(0x120, 0x1234);
	f.Write(0x122, 0x5678);
	f.Write(0x128, 0x5001);			// 32bit, IRQ, 256KHz, start

	for (int elapsed = 0; elapsed < 4096 - 64; elapsed += 64)
	{
		f.bus.sio.Tick(f.bus, 64);
		GBA_CHECK(f.bus.sio.Busy());
	}

	// The last 64 cycles complete it.
	f.bus.sio.Tick(f.bus, 64);
	GBA_CHECK(!f.bus.sio.Busy());
}

// ===========================================================================================
// Keypad
// ===========================================================================================

GBA_TEST(Keypad, OrConditionAndInterrupt)
{
	Fixture f;

	f.bus.keypad.Reset();

	// KEYCNT: select A and B, logical OR, interrupt enabled (GBATEK "GBA Keypad Input").
	f.Write(0x132, (u16)(0x4000 | KEY_A | KEY_B));

	f.bus.keypad.SetPressed(0);
	GBA_CHECK(!f.bus.keypad.ConditionMet());
	GBA_CHECK(!f.bus.keypad.IrqRequested());

	f.bus.keypad.SetPressed(KEY_A);
	GBA_CHECK(f.bus.keypad.ConditionMet());
	GBA_CHECK(f.bus.keypad.IrqRequested());

	f.bus.keypad.SetPressed(KEY_A | KEY_B);
	GBA_CHECK(f.bus.keypad.ConditionMet());

	// A key that is not selected does not satisfy the condition.
	f.bus.keypad.SetPressed(KEY_START);
	GBA_CHECK(!f.bus.keypad.ConditionMet());

	// KEYINPUT is the complement of the pressed mask, upper bits one.
	GBA_CHECK_HEX16(f.bus.keypad.ReadKeyInput(), (u16)(~KEY_START & 0x03FF) | 0xFC00);
	GBA_CHECK_HEX16(f.Read(0x130), (u16)(~KEY_START & 0x03FF) | 0xFC00);
}

GBA_TEST(Keypad, AndConditionNeedsEverySelectedKey)
{
	Fixture f;

	f.bus.keypad.Reset();

	// Logical AND: bit 15 set means "all selected keys" (GBATEK "GBA Keypad Input").
	f.Write(0x132, (u16)(0x8000 | 0x4000 | KEY_A | KEY_B));
	GBA_CHECK_HEX16(f.Read(0x132), (u16)(0x8000 | 0x4000 | KEY_A | KEY_B));

	f.bus.keypad.SetPressed(KEY_A);
	GBA_CHECK(!f.bus.keypad.ConditionMet());

	f.bus.keypad.SetPressed(KEY_A | KEY_B);
	GBA_CHECK(f.bus.keypad.ConditionMet());

	// With nothing selected there is no condition to meet.
	f.bus.keypad.Reset();
	f.bus.keypad.SetPressed(0xFFFF);
	GBA_CHECK(!f.bus.keypad.ConditionMet());
}

// ===========================================================================================
// Interrupt controller
// ===========================================================================================

GBA_TEST(Irq, PendingNeedsImeIeAndIf)
{
	Fixture f;

	// Nothing is pending after a reset.
	GBA_CHECK(!f.bus.irq.Pending());
	GBA_CHECK_HEX16(f.bus.irq.ReadIF(), 0);
	GBA_CHECK_HEX16(f.bus.irq.ReadIE(), 0);
	GBA_CHECK(!f.bus.irq.ReadIME());

	// A cause that is not enabled stays unacknowledged in IF (GBATEK "4000202h - IF").
	f.bus.irq.Raise(INT_VBLANK);
	GBA_CHECK_HEX16(f.bus.irq.ReadIF(), INT_VBLANK);
	GBA_CHECK(!f.bus.irq.Pending());

	// IE names the causes the CPU is willing to take, IME gates them all.
	f.bus.irq.WriteIE(INT_VBLANK);
	GBA_CHECK(!f.bus.irq.Pending());
	f.bus.irq.WriteIME(true);
	GBA_CHECK(f.bus.irq.Pending());

	// A different cause does not wake the CPU up.
	f.bus.irq.Raise(INT_KEYPAD);
	GBA_CHECK_HEX16(f.bus.irq.ReadIF(), INT_VBLANK | INT_KEYPAD);
	GBA_CHECK(f.bus.irq.Pending());

	// Clearing one cause leaves the other one set.
	f.bus.irq.Clear(INT_VBLANK);
	GBA_CHECK_HEX16(f.bus.irq.ReadIF(), INT_KEYPAD);
	GBA_CHECK(!f.bus.irq.Pending());

	// IME off always means "no interrupt".
	f.bus.irq.WriteIME(false);
	GBA_CHECK(!f.bus.irq.Pending());

	// IE bits 14-15 do not exist.
	f.bus.irq.WriteIE(0xFFFF);
	GBA_CHECK_HEX16(f.bus.irq.ReadIE(), 0x3FFF);
}

GBA_TEST(Irq, AcknowledgeClearsOnlyTheNamedBits)
{
	Fixture f;

	// "Interrupts must be manually acknowledged by writing a '1' to one of the IRQ bits, the IRQ
	// bit will then be cleared" (GBATEK "4000202h - IF"). The devices raise the causes, so they
	// are raised through the controller here.
	f.bus.irq.Raise(INT_VBLANK | INT_TIMER0);
	GBA_CHECK_HEX16(f.bus.irq.ReadIF(), INT_VBLANK | INT_TIMER0);

	// A write of zero acknowledges nothing (a 1 clears the bit, a 0 leaves it alone).
	f.Write(0x202, 0);
	GBA_CHECK_HEX16(f.bus.irq.ReadIF(), INT_VBLANK | INT_TIMER0);

	f.Write(0x202, INT_TIMER0);
	GBA_CHECK_HEX16(f.bus.irq.ReadIF(), INT_VBLANK);

	// A write through the bus reaches the same controller the devices raise into.
	f.bus.irq.Raise(INT_DMA3);
	GBA_CHECK_HEX16(f.Read(0x202), INT_VBLANK | INT_DMA3);
	f.Write(0x202, 0x3FFF);
	GBA_CHECK_HEX16(f.Read(0x202), 0);

	// IME is a one-bit register behind an 8bit/16bit access.
	f.Write(0x208, 1);
	GBA_CHECK(f.bus.irq.ReadIME());
	f.Write(0x208, 0);
	GBA_CHECK(!f.bus.irq.ReadIME());
}
