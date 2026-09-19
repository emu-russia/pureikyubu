// GBA timers: TM0CNT_L/H .. TM3CNT_L/H (0x04000100 .. 0x0400010F).
//
// Each timer has a 16-bit reload value (CNT_L), a prescaler and a cascade flag (CNT_H). Timers 0
// and 1 can also raise the timer interrupts. Timer 0 additionally drives the "HBlank interval"
// free-running counter the sound hardware uses in some games (a documented behaviour: when
// SOUNDCNT_H bit 10 is set, timer 0 is stepped once per scanline instead of once per prescaler
// tick).
//
// The counters are driven from the same 16.78 MHz clock as everything else, so the class is a
// consumer of Tick().

#pragma once

#include "gba_types.h"

namespace GBA
{
	class GbaBus;

	class Timers
	{
	public:
		void Reset();

		/// <summary>Read a timer register (offset is relative to 0x04000000, 0x100..0x10F).</summary>
		uint16_t Read16(uint32_t offset) const;

		/// <summary>Write a timer register. The caller passes the bus so a cascade reload and the
		/// interrupt flags can be handled in one place.</summary>
		void Write16(GbaBus& bus, uint32_t offset, uint16_t value);

		/// <summary>Advance every running timer by `cycles` of the 16.78 MHz clock.</summary>
		void Tick(GbaBus& bus, int cycles);

		/// <summary>The current counter value (for tests and the debugger).</summary>
		uint16_t Counter(int index) const { return counter[index]; }

		/// <summary>
		/// How many times the counter has wrapped since the last Reset. The sound controller's
		/// direct-sound FIFOs are clocked by a timer overflow (GBATEK "Sound Channel A and B"), and
		/// a timer can overflow several times between two host samples: the running total is what
		/// lets the mixer move as many bytes as there were overflows instead of one per sample.
		/// </summary>
		uint32_t Overflows(int index) const { return overflows[index]; }

		/// <summary>True when the timer is enabled (for tests and the debugger).</summary>
		bool Running(int index) const { return (control[index] & 0x80) != 0; }

		/// <summary>The prescaler of a TMxCNT_H value as the shift it applies to the clock (the
		/// debugger prints the divider, which is `1 &lt;&lt; PrescaleShift`).</summary>
		static int PrescaleShift(uint16_t control);

	private:
		uint16_t reload[4]{};		// TMxCNT_L
		uint16_t control[4]{};		// TMxCNT_H
		uint16_t counter[4]{};
		int prescaleAccum[4]{};	// cycles accumulated towards the next prescaler tick
		uint32_t overflows[4]{};	// wraps since the last Reset (the FIFO sample clock)
	};
}
