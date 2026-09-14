// GBA interrupt controller: the IE (0x04000200), IF (0x04000202) and IME (0x04000208) registers.
//
// The controller is trivial - the interesting part is *when* the devices raise their bits and
// that writing a 1 to an IF bit acknowledges it - but it is shared by every device, so it gets
// its own tiny class instead of living inside the bus.

#pragma once

#include "gba_types.h"

namespace GBA
{
	class Irq
	{
		u16 enable = 0;			// 0x04000200 IE
		u16 request = 0;		// 0x04000202 IF
		bool masterEnable = false;	// 0x04000208 IME

	public:
		void Reset();

		/// <summary>Raise one or more causes (the devices call this).</summary>
		void Raise(u16 bits) { request |= bits; }

		/// <summary>Drop one or more causes (used when a device is switched off).</summary>
		void Clear(u16 bits) { request &= (u16)~bits; }

		/// <summary>Acknowledge the causes named by `bits` (a write to IF).</summary>
		void Acknowledge(u16 bits) { request &= (u16)~bits; }

		/// <summary>True when the CPU must take the IRQ exception.</summary>
		bool Pending() const { return masterEnable && (request & enable) != 0; }

		u16 ReadIE() const { return enable; }
		u16 ReadIF() const { return request; }
		bool ReadIME() const { return masterEnable; }

		void WriteIE(u16 value) { enable = value & 0x3FFF; }
		void WriteIF(u16 value) { Acknowledge(value & 0x3FFF); }
		void WriteIME(bool value) { masterEnable = value; }
	};
}
