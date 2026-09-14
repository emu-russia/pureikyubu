// GBA timers: TM0CNT_L/H .. TM3CNT_L/H (0x04000100 .. 0x0400010F).
//
// Implemented from GBATEK "GBA Timers":
//
//   4000100h/104h/108h/10Ch TM0..TM3CNT_L (R/W)
//     Writing it initialises the <reload> value, it does not directly change the <counter>.
//     Reading returns the current counter (the frozen counter while the timer is stopped).
//     The reload value is copied into the counter in two situations: automatically on overflow,
//     and when the start bit (TMxCNT_H bit 7) changes from 0 to 1.
//     GBATEK note: when the start bit and the reload value are written by one 32bit access, the
//     value just written is used as the counter as well - that falls out of "a write to CNT_L
//     followed by a write to CNT_H" because the reload is applied when the start bit rises.
//
//   4000102h/106h/10Ah/10Eh TM0..TM3CNT_H (R/W)
//     bit 0-1  prescaler: 0 = F/1, 1 = F/64, 2 = F/256, 3 = F/1024
//     bit 2    count-up timing (cascade): count the overflows of the previous timer, ignoring
//              the prescaler (not available on timer 0, which has no previous timer)
//     bit 6    raise the timer's interrupt on overflow
//     bit 7    start/stop
//
// The counters are driven from the 16.78MHz system clock, so Tick() consumes the cycles the bus
// hands it in exactly the amounts the prescaler and the reload value dictate:
//
//     overflow after (0x10000 - reload) * prescaler system cycles
//
// The reload value is applied when the timer starts, so that is a complete period.
//
// Two documented details this file does *not* model (the rest of the emulator does not depend on
// them, and guessing them would be worse than leaving them out):
//
//   1. GBATEK "GBA Timers" describes a one-cycle window around an overflow in which a write to
//      TMxCNT_L still changes the value the *current* overflow reloads; which of the two values
//      wins depends on the exact cycle the store lands in. Here a write to TMxCNT_L while the
//      timer runs only changes the reload value the *next* overflow uses, and an overflow always
//      reloads with the value that was in the reload register when the counter wrapped.
//   2. The "HBlank interval free" mode of timer 0 (SOUNDCNT_H bit 10), where timer 0 is stepped
//      once per scanline instead of once per prescaler tick. That needs the sound hardware, so it
//      belongs to the APU/PPU integration rather than here.

#include "gba_timers.h"
#include "gba_bus.h"

namespace GBA
{
	namespace
	{
		// TMxCNT_H
		const u16 TmPrescalerMask = 0x0003;		// bits 0-1: F/1, F/64, F/256, F/1024
		const u16 TmCascade = 0x0004;			// bit 2: count the previous timer's overflows
		const u16 TmIrqEnable = 0x0040;			// bit 6: interrupt on overflow
		const u16 TmEnable = 0x0080;			// bit 7: run

		// The prescaler divides the 16.78MHz clock by 1, 64, 256 or 1024 (GBATEK: F/1..F/1024),
		// so bits 0-1 select a divisor of 1 << (0, 6, 8, 10).
		int Prescaler(u16 control)
		{
			switch (control & TmPrescalerMask)
			{
			case 0: return 1;
			case 1: return 64;
			case 2: return 256;
			default: return 1024;
			}
		}

		// -------------------------------------------------------------------------------------
		// The per-timer helpers. They are file local free functions because the frozen Timers
		// header declares no private member for them; the state they need is passed in.
		// -------------------------------------------------------------------------------------

		void TimerOverflow(GbaBus& bus, u16* reload, u16* control, u16* counter, int index)
		{
			// GBATEK: "The reload value is copied into the counter [...] automatically upon
			// timer overflows", and with TMxCNT_H bit 6 set the overflow also requests that
			// timer's interrupt (IF bits 3-6 are timer 0-3, see the InterruptBit enum).
			counter[index] = reload[index];

			if (control[index] & TmIrqEnable)
				bus.irq.Raise((u16)(INT_TIMER0 << index));

			// Cascade (TMxCNT_H bit 2 of the next timer, GBATEK "GBA Timers"): the next timer
			// counts the overflows of this one instead of its own prescaler, so it advances
			// now and may overflow in turn. The carry ripples through the four timers in one
			// system cycle, so this happens inside the same Tick.
			int next = index + 1;
			if (next < 4 && (control[next] & TmEnable) && (control[next] & TmCascade))
			{
				if (counter[next] == 0xFFFF)
					TimerOverflow(bus, reload, control, counter, next);
				else
					counter[next]++;
			}
		}

		void TimerCountUp(GbaBus& bus, u16* reload, u16* control, u16* counter, int index, int counts)
		{
			if (counts <= 0)
				return;

			int period = 0x10000 - reload[index];
			if (period <= 0)
				period = 1;

			// The counter keeps its value across a Tick, so how many wraps fit is measured from
			// where it stands: the counter has to reach 0x10000, not merely the period, before
			// the first overflow. Writing the distance to the *end* of the counter (0x10000)
			// keeps the arithmetic in one place:
			//
			//   distance = counts + counter        (the value the counter would hold)
			//   wraps    = (distance - 0x10000 + period) / period ... only once distance has
			//              passed 0x10000
			u64 distance = (u64)counts + counter[index];

			if (distance < 0x10000)
			{
				counter[index] = (u16)distance;
				return;
			}

			u64 past = distance - 0x10000;			// counts beyond the first wrap
			u64 wraps = 1 + past / (u64)period;
			u64 rest = past % (u64)period;

			for (u64 wrap = 0; wrap < wraps; wrap++)
				TimerOverflow(bus, reload, control, counter, index);

			counter[index] = (u16)(reload[index] + rest);
		}
	}

	void Timers::Reset()
	{
		// A power-on reset leaves the reload registers at 0 and stops every timer, so each
		// counter starts at 0 (the reload value copied on the 0 -> 1 edge of the start bit).
		for (int i = 0; i < 4; i++)
		{
			reload[i] = 0;
			control[i] = 0;
			counter[i] = 0;
			prescaleAccum[i] = 0;
		}
	}

	int Timers::PrescaleShift(u16 control)
	{
		switch (control & TmPrescalerMask)
		{
		case 0: return 0;
		case 1: return 6;
		case 2: return 8;
		default: return 10;
		}
	}

	u16 Timers::Read16(u32 offset) const
	{
		// The eight registers are laid out as four CNT_L/CNT_H pairs (0x100/0x102, 0x104/0x106,
		// 0x108/0x10A, 0x10C/0x10E). Reading CNT_L returns the current counter, not the reload
		// value (GBATEK "GBA Timers": "Reading returns the current <counter> value").
		u32 index = (offset - 0x100) >> 2;
		if (index > 3)
			return 0;

		if (offset & 2)
			return control[index];

		return counter[index];
	}

	void Timers::Write16(GbaBus& bus, u32 offset, u16 value)
	{
		(void)bus;

		u32 index = (offset - 0x100) >> 2;
		if (index > 3)
			return;

		if ((offset & 2) == 0)
		{
			// TMxCNT_L. GBATEK: a write only initialises the reload value. While the timer is
			// stopped it also loads the (frozen) counter, so a read of CNT_L returns what was
			// written - that is the "recent/frozen counter value" of the spec.
			reload[index] = value;

			if ((control[index] & TmEnable) == 0)
				counter[index] = value;

			return;
		}

		// TMxCNT_H. Only the documented bits exist; the rest of the halfword reads back as 0.
		u16 old = control[index];
		u16 next = (u16)(value & (TmPrescalerMask | TmCascade | TmIrqEnable | TmEnable));
		control[index] = next;

		bool wasRunning = (old & TmEnable) != 0;
		bool nowRunning = (next & TmEnable) != 0;

		if (!wasRunning && nowRunning)
		{
			// "The reload value is copied into the counter [...] when the timer start bit
			// becomes changed from 0 to 1" (GBATEK "GBA Timers"), so the first overflow
			// happens (0x10000 - reload) * prescaler cycles after this write. Timer 0 cannot
			// cascade (it has no previous timer), so the bit is forced off there.
			if (index == 0)
				control[index] &= (u16)~TmCascade;

			counter[index] = reload[index];
			prescaleAccum[index] = 0;
		}
		else if (wasRunning && !nowRunning)
		{
			// Stopping freezes the counter; it keeps its value until the next start reloads it.
			prescaleAccum[index] = 0;
		}
	}

	void Timers::Tick(GbaBus& bus, int cycles)
	{
		if (cycles <= 0)
			return;

		// Pass 1: how many times each free-running timer counts in this slice. The prescaler
		// accumulator carries the remainder over to the next Tick, so the counter advances by
		// exactly the prescaled number of cycles no matter how the slices are cut.
		int ticks[4] = { 0, 0, 0, 0 };

		for (int i = 0; i < 4; i++)
		{
			if ((control[i] & TmEnable) == 0 || (control[i] & TmCascade))
				continue;

			int prescale = Prescaler(control[i]);

			if (prescale == 1)
			{
				prescaleAccum[i] = 0;
				ticks[i] = cycles;
			}
			else
			{
				int divisor = PrescaleShift(control[i]);
				int accumulator = prescaleAccum[i] + cycles;
				ticks[i] = accumulator >> divisor;
				prescaleAccum[i] = accumulator & (prescale - 1);
			}
		}

		// Pass 2: advance the counters in index order. A cascade timer is advanced in the same
		// slice as the overflow of the timer before it (see TimerOverflow), which is what the
		// hardware does - the carry ripples through the four timers in one system cycle.
		for (int i = 0; i < 4; i++)
		{
			if ((control[i] & TmEnable) == 0)
				continue;

			if (control[i] & TmCascade)
				continue;		// counted by the overflows of timer i-1, not by the prescaler

			TimerCountUp(bus, reload, control, counter, i, ticks[i]);
		}
	}
}
