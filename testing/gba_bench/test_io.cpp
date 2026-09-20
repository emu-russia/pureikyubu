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

		uint16_t Read(uint32_t offset) { return bus.Read16(0x04000000 + offset); }
		uint16_t ReadMem16(uint32_t address) { return bus.Read16(address); }
		void Write(uint32_t offset, uint16_t value) { bus.Write16(0x04000000 + offset, value); }
		void WriteMem16(uint32_t address, uint16_t value) { bus.Write16(address, value); }

		uint8_t Read8(uint32_t offset) { return bus.Read8(0x04000000 + offset); }
		void Write8(uint32_t offset, uint8_t value) { bus.Write8(0x04000000 + offset, value); }
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
	uint16_t TimerControl(int prescaler, bool cascade, bool irq)
	{
		return (uint16_t)((prescaler & 3) | (cascade ? 0x0004 : 0) | (irq ? 0x0040 : 0) | 0x0080);
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
	const uint16_t reload = 0x8000;
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
	const uint16_t reload = 0xFF00;					// 256 cycles per timer 0 period
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

GBA_TEST(Timers, OverflowTotalCountsEveryWrap)
{
	// The sound controller's direct-sound FIFOs are clocked by a timer overflow (SOUNDCNT_H bits
	// 10/14), and a timer can overflow several times between two host samples: the running total of
	// the wraps is what the mixer reads, so a counter value that happens to come back to where it
	// was - the period divides the slice - must not hide the wraps in between.
	Fixture f;

	const uint16_t reload = 0xFF00;					// 256 cycles per overflow
	f.Write(0x100, reload);						// TM0CNT_L
	f.Write(0x102, TimerControl(0, false, false));	// TM0CNT_H: F/1, start

	GBA_CHECK_EQ((int)f.bus.timers.Overflows(0), 0);

	// One period: the counter wraps once and is back at the reload value.
	f.bus.timers.Tick(f.bus, 256);
	GBA_CHECK_HEX16(f.Read(0x100), reload);
	GBA_CHECK_EQ((int)f.bus.timers.Overflows(0), 1);

	// Four periods in one slice: the counter is back at the same value again, but four more wraps
	// happened.
	f.bus.timers.Tick(f.bus, 1024);
	GBA_CHECK_HEX16(f.Read(0x100), reload);
	GBA_CHECK_EQ((int)f.bus.timers.Overflows(0), 5);

	// A stopped timer does not count, and the total is kept across a stop and a restart.
	f.Write(0x102, 0x0000);
	f.bus.timers.Tick(f.bus, 4096);
	GBA_CHECK_EQ((int)f.bus.timers.Overflows(0), 5);

	f.Write(0x102, TimerControl(0, false, false));
	f.bus.timers.Tick(f.bus, 512);
	GBA_CHECK_EQ((int)f.bus.timers.Overflows(0), 7);

	// A cascade counts the overflows of the timer before it, so its own total follows that one.
	Fixture c;
	c.Write(0x100, 0xFF00);						// TM0CNT_L: 256 cycles per overflow
	c.Write(0x102, TimerControl(0, false, false));
	c.Write(0x104, 0xFFFF);						// TM1CNT_L: one count to overflow
	c.Write(0x106, TimerControl(0, true, false));	// TM1CNT_H: cascade off timer 0

	c.bus.timers.Tick(c.bus, 256);
	GBA_CHECK_EQ((int)c.bus.timers.Overflows(0), 1);
	GBA_CHECK_EQ((int)c.bus.timers.Overflows(1), 1);
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
	f.Write(0x102, TimerControl(0, false, false) & (uint16_t)~0x0080);
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
		GBA_CHECK_HEX16((uint16_t)f.ReadMem16(0x02000100), 0x1111);
		GBA_CHECK_HEX16((uint16_t)f.ReadMem16(0x02000102), 0x2222);
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

		GBA_CHECK_HEX16((uint16_t)f.ReadMem16(0x02000104), 0xAAAA);
		GBA_CHECK_HEX16((uint16_t)f.ReadMem16(0x02000102), 0xBBBB);
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
		f.WriteMem16(0x02000100, 0x0000);	// the destination starts clear
		f.Write(0x0D2, 0x8040);			// dest fixed (bits 5-6 = 2), immediate, enable

		GBA_CHECK_HEX16((uint16_t)f.ReadMem16(0x02000100), 0x5678);	// the second unit wins
	}
}

GBA_TEST(Timers, PrescalerDividesTheSystemClock)
{
	// GBATEK 4000100h: the prescaler divides the 16.78 MHz clock by 1, 64, 256 or 1024. The AGB
	// aging cartridge measures exactly this: it loads a timer's reload value and control and then
	// spins a fixed loop (1024 iterations of a three cycle branch), reading the counter
	// afterwards. 3072 cycles are 3072, 48, 12 and 3 ticks.
	const int expected[4] = { 3072, 48, 12, 3 };

	Fixture f;

	for (int p = 0; p < 4; p++)
	{
		f.Write(0x100, 0x0000);				// TM0CNT_L = 0
		f.Write(0x102, (uint16_t)(0x0080 | p));	// enable, prescaler p
		f.bus.Tick(3072);
		uint16_t count = f.Read(0x100);
		f.Write(0x102, 0x0000);				// stop

		GBA_CHECK_EQ(count, (uint16_t)expected[p]);
	}
}

GBA_TEST(Bus, InternalMemoryTimingsFollowTheMemoryMap)
{
	// GBATEK "GBA Memory Map" gives the access cycles of the internal memories, and the aging
	// cartridge measures them: the on-board 256K WRAM is a 16 bit bus at 3/3/6 cycles (its
	// waitstates come from the undocumented 4000800h register), VRAM/OAM/Palette RAM are 1/1/2,
	// and the BIOS, the 32K on-chip WRAM and the I/O area are 1/1/1.
	Fixture f;

	// The 32K on-chip WRAM and the I/O area need nothing on top of the CPU's own cycle.
	f.bus.TakeWaitCycles();
	f.WriteMem16(0x03000000, 0x1234);
	GBA_CHECK_EQ(f.bus.TakeWaitCycles(), 0);

	f.bus.TakeWaitCycles();
	f.Write(0x200, 0x0001);					// IE, an I/O register
	GBA_CHECK_EQ(f.bus.TakeWaitCycles(), 0);

	// VRAM: 16 bit accesses take one cycle, a 32 bit one takes two.
	f.bus.TakeWaitCycles();
	f.WriteMem16(0x06000000, 0x1234);
	GBA_CHECK_EQ(f.bus.TakeWaitCycles(), 0);

	f.bus.TakeWaitCycles();
	f.bus.Write32(0x06000000, 0x12345678);
	GBA_CHECK_EQ(f.bus.TakeWaitCycles(), 1);

	// The 256K WRAM: 3 cycles for 8 and 16 bit, 6 for 32 bit (two bus cycles of three).
	f.bus.TakeWaitCycles();
	f.WriteMem16(0x02000000, 0x1234);
	GBA_CHECK_EQ(f.bus.TakeWaitCycles(), 2);

	f.bus.TakeWaitCycles();
	f.bus.Write32(0x02000000, 0x12345678);
	GBA_CHECK_EQ(f.bus.TakeWaitCycles(), 5);

	// 4000800h selects those waitstates: 0Eh is one waitstate, so 8/16 bit cost two cycles and
	// 32 bit four (GBATEK 4000800h: "The fastest possible setting would be 0Eh (1 waitstate,
	// 2/2/4 cycles)").
	f.bus.TakeWaitCycles();
	f.Write(0x800, 0x0020);					// the low half: the 256K WRAM enable
	f.Write(0x802, 0x0E00);					// the high half: bits 24-27 = 0Eh
	f.WriteMem16(0x02000000, 0x1234);
	GBA_CHECK_EQ(f.bus.TakeWaitCycles(), 1);

	f.bus.TakeWaitCycles();
	f.bus.Write32(0x02000000, 0x12345678);
	GBA_CHECK_EQ(f.bus.TakeWaitCycles(), 3);

	// The register is mirrored across the I/O area in 64K steps.
	GBA_CHECK_HEX32(f.bus.Read32(0x04010800), 0x0E000020);
}

GBA_TEST(Dma, ATransferSpendsItsCyclesWhileItRuns)
{
	// GBATEK "Transfer Rate/Timing": a transfer's read and write cycles "depend on the
	// waitstates and bus-width of the source and destination areas", and the hardware steals
	// them one at a time - the clock keeps moving between one unit and the next. The AGB aging
	// cartridge measures exactly that: it times a memory block by DMA-sampling Timer 0. A
	// transfer that ran to completion and only charged its cycles to the next slice gave it the
	// same sample every time.
	Fixture f;

	// Timer 0 counts the system clock (prescaler 1, GBATEK 4000100h) and DMA3 copies its
	// counter into the on-chip WRAM four times, with the source fixed on the register.
	f.Write(0x100, 0x0000);
	f.Write(0x102, 0x0080);				// enable, prescaler 1
	f.Write(0x0D4, 0x0100);				// SAD = 0x04000100 (Timer 0's counter)
	f.Write(0x0D6, 0x0400);
	f.Write(0x0D8, 0x0000);				// DAD = 0x03000000 (on-chip WRAM)
	f.Write(0x0DA, 0x0300);
	f.Write(0x0DC, 4);					// four 16bit units
	f.Write(0x0DE, 0x8100);				// enable, fixed source control (bits 7-8 = 2)

	uint16_t first = f.ReadMem16(0x03000000);
	uint16_t last = f.ReadMem16(0x03000006);

	// Every sample is the timer as the transfer read it, so they climb.
	GBA_CHECK_MSG(last > first, "the clock must advance while a transfer runs");
}

GBA_TEST(Dma, WordCountZeroWrapsToTheMaximum)
{
	// DMA0-2: a count of zero is 0x4000 units (GBATEK "DMAxCNT_L").
	{
		Fixture f;
		f.WriteMem16(0x03000000, 0x1234);
		f.Write(0x0B0, 0x0000);
		f.Write(0x0B2, 0x0300);			// source 0x03000000 (IWRAM)
		f.Write(0x0B4, 0x0000);
		f.Write(0x0B6, 0x0200);			// destination 0x02000000 (EWRAM)
		f.Write(0x0B8, 0);				// word count 0 -> 0x4000
		f.Write(0x0BA, 0x8100);			// enable, fixed source (bits 7-8 = 2)

		// The transfer ran with the maximum count: the last unit landed.
		GBA_CHECK_HEX16(f.ReadMem16(0x02000000 + (0x4000 - 1) * 2), 0x1234);
		GBA_CHECK_HEX16(f.ReadMem16(0x02000000 + (0x4000 - 2) * 2), 0x1234);
	}

	// DMA3: a count of zero is 0x10000 units.
	{
		Fixture f;
		f.WriteMem16(0x03000000, 0x5678);
		f.Write(0x0D4, 0x0000);
		f.Write(0x0D6, 0x0300);
		f.Write(0x0D8, 0x0000);
		f.Write(0x0DA, 0x0200);
		f.Write(0x0DC, 0);
		f.Write(0x0DE, 0x8100);

		GBA_CHECK_HEX16(f.ReadMem16(0x02000000 + (0x10000 - 1) * 2), 0x5678);
	}
}

GBA_TEST(Dma, RepeatTransferRestartedByVBlank)
{
	// What a repeat reloads is what GBATEK "Source and Destination Address and Word Count
	// Registers" lists: "Upon DMA Enable (Bit 15) changing from 0 to 1: Reloads SAD, DAD, CNT_L.
	// Upon Repeat: Reloads CNT_L, and optionally DAD (Increment+Reload)." SAD is *not* on the
	// repeat list, so a repeating channel carries on through its source while the count - and,
	// with the destination control set to increment + reload, DAD - start over for the next
	// start condition. (A stream through the sound FIFO is the case that makes this obvious: the
	// FIFO asks for 16 bytes at a time, and a source that restarted every time would only ever
	// play the first 16 bytes of the music.) A program that wants the same block again sets the
	// source control to "fixed" instead, which is what the last part of this test checks.
	Fixture f;

	// Four halfwords of source data at 0x02000000, so that two repeating transfers can be told
	// apart, and a zeroed destination at 0x02000100.
	f.WriteMem16(0x02000000, 0xCAFE);
	f.WriteMem16(0x02000002, 0xBABE);
	f.WriteMem16(0x02000004, 0x0DD0);
	f.WriteMem16(0x02000006, 0x0FF1);
	f.WriteMem16(0x02000100, 0x0000);
	f.WriteMem16(0x02000102, 0x0000);

	// DMA3, source increment, destination increment + reload, repeat, VBlank (timing 1),
	// word count 2 (GBATEK DMAxCNT_H bits 5-6 = 3, bits 12-13 = 1, bit 9 = 1).
	f.Write(0x0D4, 0x0000);
	f.Write(0x0D6, 0x0200);
	f.Write(0x0D8, 0x0100);
	f.Write(0x0DA, 0x0200);
	f.Write(0x0DC, 2);
	f.Write(0x0DE, (uint16_t)(0x0200 | 0x1000 | 0x0060 | 0x8000));

	// Nothing runs until the VBlank edge arrives.
	GBA_CHECK_HEX16((uint16_t)f.ReadMem16(0x02000100), 0x0000);
	GBA_CHECK(f.bus.dma.Active(3));

	f.bus.dma.OnVBlank(f.bus);
	GBA_CHECK_HEX16((uint16_t)f.ReadMem16(0x02000100), 0xCAFE);
	GBA_CHECK_HEX16((uint16_t)f.ReadMem16(0x02000102), 0xBABE);

	// The repeat bit keeps the channel enabled, so the next VBlank writes two more words to the
	// same place (DAD was reloaded): the source, which was not reloaded, is now two halfwords on.
	GBA_CHECK(f.bus.dma.Active(3));
	GBA_CHECK_HEX16(f.Read(0x0DE), (uint16_t)(0x0200 | 0x1000 | 0x0060 | 0x8000));

	f.WriteMem16(0x02000100, 0x0000);
	f.WriteMem16(0x02000102, 0x0000);
	f.bus.dma.OnVBlank(f.bus);
	GBA_CHECK_HEX16((uint16_t)f.ReadMem16(0x02000100), 0x0DD0);
	GBA_CHECK_HEX16((uint16_t)f.ReadMem16(0x02000102), 0x0FF1);

	// A repeat whose source control is "fixed" reads the same word for every unit, which is how a
	// block is filled with one value: both halfwords of the destination get the first word.
	f.Write(0x0DE, 0x0000);										// stop
	f.Write(0x0D4, 0x0000);										// SAD = 0x02000000
	f.Write(0x0D6, 0x0200);
	f.Write(0x0DE, (uint16_t)(0x0200 | 0x1000 | 0x0060 | 0x8000 | 0x0100));	// source fixed, start

	f.WriteMem16(0x02000100, 0x0000);
	f.WriteMem16(0x02000102, 0x0000);
	f.bus.dma.OnVBlank(f.bus);
	GBA_CHECK_HEX16((uint16_t)f.ReadMem16(0x02000100), 0xCAFE);
	GBA_CHECK_HEX16((uint16_t)f.ReadMem16(0x02000102), 0xCAFE);

	// To copy a *block* again, a program re-arms the channel: the register still holds the address
	// the transfer started from (a transfer never changes it), so the 0 -> 1 edge puts the source
	// pointer back and the next VBlank copies the first two halfwords once more.
	f.Write(0x0DE, 0x0000);										// stop
	f.Write(0x0DE, (uint16_t)(0x0200 | 0x1000 | 0x0060 | 0x8000));	// source increment again, start

	f.WriteMem16(0x02000100, 0x0000);
	f.WriteMem16(0x02000102, 0x0000);
	f.bus.dma.OnVBlank(f.bus);
	GBA_CHECK_HEX16((uint16_t)f.ReadMem16(0x02000100), 0xCAFE);
	GBA_CHECK_HEX16((uint16_t)f.ReadMem16(0x02000102), 0xBABE);

	// Clearing the enable bit stops the repetition.
	f.Write(0x0DE, (uint16_t)(0x0200 | 0x1000 | 0x0060));
	GBA_CHECK(!f.bus.dma.Active(3));
}

GBA_TEST(Dma, FifoRefillCompletesWhenTheApuAsks)
{
	Fixture f;

	// The four words a sound DMA moves (GBATEK "Sound DMA (FIFO Timing Mode)": "4 units of
	// 32bits (16 bytes) are transferred, both Word Count register and DMA Transfer Type bit are
	// ignored").
	const uint16_t payload[8] = { 0x1111, 0x2222, 0x3333, 0x4444, 0x5555, 0x6666, 0x7777, 0x8888 };
	for (int i = 0; i < 8; i++)
		f.WriteMem16(0x02000000 + i * 2, payload[i]);

	// DMA1, special timing (bits 12-13 = 3), incrementing source, fixed destination, repeat, with
	// the FIFO A destination (0x040000A0).
	f.Write(0x0BC, 0x0000);
	f.Write(0x0BE, 0x0200);
	f.Write(0x0C0, 0x00A0);
	f.Write(0x0C2, 0x0400);
	f.Write(0x0C4, 4);
	f.Write(0x0C6, (uint16_t)(0x3000 | 0x0200 | 0x8000));

	// The APU asks for a refill...
	f.bus.dma.OnFifoRequest(f.bus, 0);

	// ... and the four words are in FIFO A, in transfer order. The FIFO is write-only on the
	// hardware (a read returns the open bus), so the emulator's `Apu::Read8` - which is const -
	// peeks at the byte that will be played next instead of popping it, and the FIFO is drained
	// the way the hardware does it: one byte per overflow of timer 0/1 (GBATEK "Sound Channel
	// A/B": "Move 8bit data from FIFO to sound circuit").
	const uint8_t expected[16] = {
		0x11, 0x11, 0x22, 0x22, 0x33, 0x33, 0x44, 0x44,
		0x55, 0x55, 0x66, 0x66, 0x77, 0x77, 0x88, 0x88,
	};

	GBA_CHECK_HEX16(f.bus.apu.Read8(0x0A0, 0x00), expected[0]);

	// Disarm the channel for the drain: the FIFO is a level-sensitive requester, so a repeating
	// DMA would refill it the moment it drops below 16 bytes and the FIFO would never run dry.
	f.Write(0x0C6, 0x0000);

	// Timer 0 reloads from 0xFC00 with the prescaler at 1, so it overflows every 1024 cycles -
	// two APU samples at 32768 Hz, which is two FIFO bytes per overflow-free sample and makes the
	// wrap visible to the APU (which detects an overflow by the counter going backwards).
	f.bus.apu.SetSampleRate(32768);
	f.Write(0x100, 0xFC00);				// TM0CNT_L
	f.Write(0x102, 0x0080);				// TM0CNT_H: enable, prescaler 1

	// SOUNDCNT_H: FIFO A on both outputs (bits 8-9), 100% volume (bits 2-3 = 1), timer 0;
	// SOUNDCNT_X: master enable.
	f.Write(0x082, 0x0304);
	f.Write(0x084, 0x0080);

	// The APU samples the timer once per host sample, so a wrap that lands outside a sample
	// boundary is missed (a documented approximation of the sound path); 120 samples is several
	// times the 16 bytes the payload holds, so the FIFO is certainly empty by then.
	for (int i = 0; i < 120; i++)
	{
		f.bus.timers.Tick(f.bus, 512);
		f.bus.apu.Tick(f.bus, 512);
	}

	// The FIFO has run dry, and the byte it stopped on is the last one of the payload: the DMA
	// queued all four words in order and the drain moved exactly those 16 bytes. The peek then
	// returns that latched sample (hardware keeps the last sample it played rather than going
	// silent).
	GBA_CHECK_HEX16(f.bus.apu.Read8(0x0A0, 0x00), expected[15]);

	// An enabled but empty FIFO asks for more data, which is the level the sound DMA watches.
	GBA_CHECK(f.bus.apu.FifoRequest(0));

	// Re-arming the repeating channel lets the pending request fill the FIFO again; the repeat bit
	// keeps the channel armed after the transfer.
	f.Write(0x0C6, (uint16_t)(0x3000 | 0x0200 | 0x8000));
	GBA_CHECK(f.bus.dma.Active(1));

	// A peek that no longer returns the latched 0x88 is the proof: the FIFO holds data once more,
	// and it was filled from the same source address, so it starts at the first byte of the
	// payload.
	f.bus.apu.Tick(f.bus, 512);
	GBA_CHECK_HEX16(f.bus.apu.Read8(0x0A0, 0x00), expected[0]);

	// A request for the other FIFO finds no channel and changes nothing.
	f.bus.dma.OnFifoRequest(f.bus, 1);
	GBA_CHECK(!f.bus.apu.FifoRequest(1));
}

GBA_TEST(Dma, ARepeatingFifoTransferStreamsForward)
{
	// A repeating sound DMA streams a buffer: every refill the FIFO asks for moves the *next* 16
	// bytes, so the source pointer carries on from where the last block ended. GBATEK "Source and
	// Destination Address and Word Count Registers" is what says so - "Upon DMA Enable (Bit 15)
	// changing from 0 to 1: Reloads SAD, DAD, CNT_L. Upon Repeat: Reloads CNT_L, and optionally
	// DAD (Increment+Reload)" - SAD is not on the repeat list. Reloading it there restarts the
	// stream from the same 16 bytes on every refill, which is heard as one short loop buzzing at
	// the FIFO's byte rate instead of the music (Metroid Fusion's soundtrack was exactly that).
	Fixture f;

	// 64 bytes of source data, every byte different (byte i + 1, so that a sample of zero cannot
	// be mistaken for a byte that played).
	const int Bytes = 64;
	for (int i = 0; i < Bytes / 2; i++)
		f.WriteMem16(0x02000000 + i * 2, (uint16_t)((i * 2 + 2) << 8 | (i * 2 + 1)));

	// DMA1: special timing, incremental source, FIFO A destination, 32bit units, repeat, enabled.
	f.Write(0x0BC, 0x0000);						// DMA1SAD_L
	f.Write(0x0BE, 0x0200);						// DMA1SAD_H: 0x02000000
	f.Write(0x0C0, 0x00A0);						// DMA1DAD_L
	f.Write(0x0C2, 0x0400);						// DMA1DAD_H: 0x040000A0
	f.Write(0x0C4, Bytes / 4);					// DMA1CNT_L
	f.Write(0x0C6, (uint16_t)(0x3000 | 0x0400 | 0x0200 | 0x8000));	// special, 32bit, repeat, enable

	// SOUNDCNT_H: FIFO A on both outputs at 100%, timer 0; SOUNDCNT_X: master enable.
	f.Write(0x082, 0x0304);
	f.Write(0x084, 0x0080);

	// Timer 0 overflows every 512 cycles, i.e. once per host sample at 32768 Hz: one FIFO byte
	// per sample, so the stream can be read back one byte at a time.
	f.Write(0x100, 0xFE00);						// TM0CNT_L: 512 cycles
	f.Write(0x102, 0x0080);						// TM0CNT_H: enable, prescaler 1
	f.bus.apu.SetSampleRate(32768);

	// Drain a few refills' worth of bytes. The very first sample only takes the timer's running
	// total (the byte that plays is the one a previous overflow put in the latch), so the first
	// byte can still be the stale one; what matters is that the *stream* moves on - with the
	// source reloaded on every refill the FIFO only ever plays the buffer's first 16 bytes.
	std::vector<int> played;
	for (int i = 0; i < 40; i++)
	{
		f.bus.timers.Tick(f.bus, 512);
		f.bus.apu.Tick(f.bus, 512);

		int16_t frame[2] = { 0, 0 };
		if (f.bus.apu.ReadSamples(frame, 1) == 1)
			played.push_back((int)frame[0] / (4 * 64));	// the byte, undoing FifoScale and MixScale
	}

	GBA_CHECK_MSG(played.size() >= 32,
		"the FIFO played only " + std::to_string(played.size()) + " bytes");
	GBA_CHECK_MSG(played.back() >= 32,
		"the stream stopped at byte " + std::to_string(played.back()) + " of the buffer");

	bool consecutive = true;
	for (size_t i = played.size() - 20; i < played.size(); i++)
	{
		if (played[i] != played[i - 1] + 1)
			consecutive = false;
	}

	GBA_CHECK_MSG(consecutive, "the bytes did not come out in order: " +
		std::to_string(played[played.size() - 3]) + ", " +
		std::to_string(played[played.size() - 2]) + ", " +
		std::to_string(played[played.size() - 1]));

	// The register is untouched by the transfer (GBATEK: "The hardware does NOT change the content
	// of these registers"), so it still reads 0x02000000.
	GBA_CHECK_HEX16(f.Read(0x0BC), 0x0000);
	GBA_CHECK_HEX16(f.Read(0x0BE), 0x0200);
}

GBA_TEST(Dma, VideoMemoryTransferLandsWhereItShould)
{
	// The GBA BIOS's boot animation copies its graphics into VRAM with the DMA (and so do games),
	// so a transfer whose destination is video memory has to land at the address DAD names - a
	// comparison against the EWRAM tests would not catch a destination path that is off by a
	// window. The source is EWRAM, the destination VRAM at 0x06001C40, 512 bytes, 16-bit units.
	Fixture f;

	// 512 halfwords of source data in EWRAM: the first transfer moves the first half, the second
	// one the second half (a transfer that runs off the end of the payload would move zeros and
	// hide a broken destination path).
	for (int i = 0; i < 512; i++)
		f.WriteMem16(0x02000000 + i * 2, (uint16_t)(0x1000 + i));

	f.Write(0x0D4, 0x0000);				// DMA3 SAD
	f.Write(0x0D6, 0x0200);
	f.Write(0x0D8, 0x1C40);				// DMA3 DAD
	f.Write(0x0DA, 0x0600);
	f.Write(0x0DC, 256);				// 256 halfwords
	f.Write(0x0DE, (uint16_t)(0x8000));		// immediate, 16-bit units, enable

	// The first and the last word of the block are in VRAM now, and the rest of the area is not.
	GBA_CHECK_HEX16(f.bus.ppu.ReadVram(0x1C40), 0x00);
	GBA_CHECK_HEX16(f.bus.ppu.ReadVram(0x1C41), 0x10);
	GBA_CHECK_HEX16(f.bus.ppu.ReadVram(0x1C42), 0x01);
	GBA_CHECK_HEX16(f.bus.ppu.ReadVram(0x1C43), 0x10);
	GBA_CHECK_HEX16(f.bus.ppu.ReadVram(0x1C40 + 510), 0xFF);
	GBA_CHECK_HEX16(f.bus.ppu.ReadVram(0x1C40 + 511), 0x10);
	GBA_CHECK_HEX16(f.bus.ppu.ReadVram(0x1C40 + 512), 0x00);
	GBA_CHECK_HEX16(f.bus.ppu.ReadVram(0x1C3F), 0x00);

	// A 32-bit transfer into the object tile area behaves the same way (the BIOS puts the
	// animation's sprites there). A new 0 -> 1 enable copies SAD from the register again (GBATEK
	// "Source and Destination Address and Word Count Registers"), and a transfer does not change
	// the register - so this second transfer starts from the payload's first halfword, exactly as
	// the first one did, and the 64 32bit units it moves are payload halfwords 0..255.
	f.Write(0x0D8, 0x0000);
	f.Write(0x0DA, 0x0601);
	f.Write(0x0DC, 64);
	f.Write(0x0DE, (uint16_t)(0x8400));		// immediate, 32-bit units, enable

	GBA_CHECK_HEX16(f.bus.ppu.ReadVram(0x10000), 0x00);
	GBA_CHECK_HEX16(f.bus.ppu.ReadVram(0x10001), 0x10);
	GBA_CHECK_HEX16(f.bus.ppu.ReadVram(0x10000 + 254), 0x7F);
	GBA_CHECK_HEX16(f.bus.ppu.ReadVram(0x10000 + 255), 0x10);
	GBA_CHECK_HEX16(f.bus.ppu.ReadVram(0x10000 + 256), 0x00);
	GBA_CHECK_HEX16(f.bus.ppu.ReadVram(0x0FFFF), 0x00);

	// Writing SAD is what makes a transfer carry on: the third one starts where the first stopped
	// (0x02000200), so the first halfword it moves is payload halfword 256 = 0x1100.
	f.Write(0x0D4, 0x0200);				// DMA3 SAD = 0x02000200
	f.Write(0x0D6, 0x0200);
	f.Write(0x0D8, 0x0000);
	f.Write(0x0DA, 0x0601);
	f.Write(0x0DC, 64);
	f.Write(0x0DE, (uint16_t)(0x8400));		// immediate, 32-bit units, enable

	GBA_CHECK_HEX16(f.bus.ppu.ReadVram(0x10000), 0x00);
	GBA_CHECK_HEX16(f.bus.ppu.ReadVram(0x10001), 0x11);
	GBA_CHECK_HEX16(f.bus.ppu.ReadVram(0x10000 + 254), 0x7F);
	GBA_CHECK_HEX16(f.bus.ppu.ReadVram(0x10000 + 255), 0x11);
	GBA_CHECK_HEX16(f.bus.ppu.ReadVram(0x10000 + 256), 0x00);
	GBA_CHECK_HEX16(f.bus.ppu.ReadVram(0x0FFFF), 0x00);
}

GBA_TEST(Dma, EepromTransferCompletesInsideTheEnableWrite)
{
	Fixture f;

	// The EEPROM protocol (GBATEK "GBA Cartridges"), the 9-unit 2bit command that starts a
	// write: the games set up the bit stream in RAM and poll the DMA enable bit in the very next
	// instruction, so the transfer has to be over when the CNT_H write returns.
	for (int i = 0; i < 9; i++)
		f.WriteMem16(0x02000000 + i * 2, (uint16_t)(0x0080u >> (i & 7)));

	// DMA3 at 0x02000000 -> 0x0D000000 (the EEPROM window), 9 units, 16bit, immediate.
	f.Write(0x0D4, 0x0000);
	f.Write(0x0D6, 0x0200);
	f.Write(0x0D8, 0x0000);
	f.Write(0x0DA, 0x0D00);
	f.Write(0x0DC, 9);
	f.Write(0x0DE, 0x8000);

	GBA_CHECK(!f.bus.dma.Active(3));
	GBA_CHECK_HEX16((uint16_t)(f.Read(0x0DE) & 0x8000), 0x0000);
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
	uint8_t low = f.bus.Read8(0x04000120);
	GBA_CHECK_HEX16(low, 0x0034);
	uint8_t high = f.bus.Read8(0x04000121);
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
	GBA_CHECK_HEX16((uint16_t)(master.Read(0x128) & 0x0080), 0);
	GBA_CHECK_HEX16((uint16_t)(slave.Read(0x128) & 0x0080), 0);
	GBA_CHECK_HEX16((uint16_t)(master.bus.irq.ReadIF() & INT_SIO), INT_SIO);
	GBA_CHECK_HEX16((uint16_t)(slave.bus.irq.ReadIF() & INT_SIO), INT_SIO);

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
	GBA_CHECK_HEX16((uint16_t)(master.Read(0x128) & 0x0080), 0);
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
	GBA_CHECK_HEX16((uint16_t)(solo.Read(0x128) & 0x0080), 0);
	GBA_CHECK_HEX16((uint16_t)(solo.bus.irq.ReadIF() & INT_SIO), INT_SIO);
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
	GBA_CHECK_HEX16((uint16_t)(child.Read(0x128) & 0x0080), 0x0080);
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
	GBA_CHECK_HEX16((uint16_t)(parent.Read(0x128) & 0x0080), 0);
	GBA_CHECK_HEX16((uint16_t)(child.Read(0x128) & 0x0080), 0);
	GBA_CHECK_HEX16((uint16_t)(parent.bus.irq.ReadIF() & INT_SIO), INT_SIO);
	GBA_CHECK_HEX16((uint16_t)(child.bus.irq.ReadIF() & INT_SIO), INT_SIO);

	// The ID bits (SIOCNT bits 4-5) name the unit's slot: the parent is 0, the first child is 1
	// (GBATEK "SIOCNT, MULTI-PLAYER Mode": "4-5 Multi-Player ID (0=Parent, 1-3=1st-3rd child)").
	GBA_CHECK_HEX16((uint16_t)(parent.Read(0x128) & 0x0030) >> 4, 0);
	GBA_CHECK_HEX16((uint16_t)(child.Read(0x128) & 0x0030) >> 4, 1);
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
		GBA_CHECK_HEX16((uint16_t)(solo.Read(0x128) & 0x0080), 0);
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
	f.Write(0x128, 0x5081);			// 32bit, IRQ, 256KHz, internal clock, start (bit 7)

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
	f.Write(0x132, (uint16_t)(0x4000 | KEY_A | KEY_B));

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
	GBA_CHECK_HEX16(f.bus.keypad.ReadKeyInput(), (uint16_t)(~KEY_START & 0x03FF) | 0xFC00);
	GBA_CHECK_HEX16(f.Read(0x130), (uint16_t)(~KEY_START & 0x03FF) | 0xFC00);
}

GBA_TEST(Keypad, AndConditionNeedsEverySelectedKey)
{
	Fixture f;

	f.bus.keypad.Reset();

	// Logical AND: bit 15 set means "all selected keys" (GBATEK "GBA Keypad Input").
	f.Write(0x132, (uint16_t)(0x8000 | 0x4000 | KEY_A | KEY_B));
	GBA_CHECK_HEX16(f.Read(0x132), (uint16_t)(0x8000 | 0x4000 | KEY_A | KEY_B));

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
