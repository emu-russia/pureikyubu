// GBA interrupt controller: the IE (0x04000200), IF (0x04000202) and IME (0x04000208) registers.
//
// The controller is trivial - the interesting part is *when* the devices raise their bits and
// that writing a 1 to an IF bit acknowledges it - but it is shared by every device, so it gets
// its own tiny class instead of living inside the bus.

#pragma once

#include "gba_types.h"

namespace GBA
{
	class StateWriter;
	class StateReader;

	class Irq
	{
		uint16_t enable = 0;		// 0x04000200 IE
		uint16_t request = 0;		// 0x04000202 IF
		bool masterEnable = false;	// 0x04000208 IME

	public:
		void Reset();

		/// <summary>Write the three registers into a save state.</summary>
		void SaveState(StateWriter& writer) const;

		/// <summary>Read them back (see gba_savestate.h).</summary>
		void LoadState(StateReader& reader);

		/// <summary>Raise one or more causes (the devices call this).</summary>
		void Raise(uint16_t bits) { request |= bits; }

		/// <summary>Drop one or more causes (used when a device is switched off).</summary>
		void Clear(uint16_t bits) { request &= (uint16_t)~bits; }

		/// <summary>Acknowledge the causes named by `bits` (a write to IF).</summary>
		void Acknowledge(uint16_t bits) { request &= (uint16_t)~bits; }

		/// <summary>True when the CPU must take the IRQ exception.</summary>
		bool Pending() const { return masterEnable && (request & enable) != 0; }

		uint16_t ReadIE() const { return enable; }
		uint16_t ReadIF() const { return request; }
		bool ReadIME() const { return masterEnable; }

		void WriteIE(uint16_t value) { enable = value & 0x3FFF; }
		void WriteIF(uint16_t value) { Acknowledge(value & 0x3FFF); }
		void WriteIME(bool value) { masterEnable = value; }
	};
}
