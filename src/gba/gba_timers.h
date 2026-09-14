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
		u16 Read16(u32 offset) const;

		/// <summary>Write a timer register. The caller passes the bus so a cascade reload and the
		/// interrupt flags can be handled in one place.</summary>
		void Write16(GbaBus& bus, u32 offset, u16 value);

		/// <summary>Advance every running timer by `cycles` of the 16.78 MHz clock.</summary>
		void Tick(GbaBus& bus, int cycles);

		/// <summary>The current counter value (for tests and the debugger).</summary>
		u16 Counter(int index) const { return counter[index]; }

		/// <summary>True when the timer is enabled (for tests and the debugger).</summary>
		bool Running(int index) const { return (control[index] & 0x80) != 0; }

	private:
		u16 reload[4]{};		// TMxCNT_L
		u16 control[4]{};		// TMxCNT_H
		u16 counter[4]{};
		int prescaleAccum[4]{};	// cycles accumulated towards the next prescaler tick

		static int PrescaleShift(u16 control);
	};
}
