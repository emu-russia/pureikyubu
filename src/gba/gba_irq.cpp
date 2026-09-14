// GBA interrupt controller: IE (0x04000200), IF (0x04000202) and IME (0x04000208).
//
// GBATEK "GBA Interrupt Control" (4000200h IE, 4000202h IF, 4000208h IME):
//
//   * IE names, per bit, the causes the CPU is willing to take. The GBA has only the 14 causes
//     of the InterruptBit enum, so IE/IF bits 14-15 do not exist; the accessors in the header
//     already mask writes with 0x3FFF, and Raise()/Clear()/Acknowledge() keep the register at
//     that width because their callers only ever pass bits from the same enum.
//   * IF latches the causes a device raised. A cause is *not* dropped by taking the exception:
//     the handler has to acknowledge it by writing a 1 to the bit (writing a 0 leaves it set,
//     which is what IF's "write 1 to clear" semantics mean). That is Acknowledge().
//   * IME (bit 0 of 4000208h) gates every cause: Pending() is masterEnable && (enable & request).
//
// Nothing here is timing aware: a device raises its bit when its hardware event happens, and the
// CPU checks Pending() before executing the next instruction.

#include "gba_irq.h"

namespace GBA
{
	void Irq::Reset()
	{
		enable = 0;			// IE = 0000h after a reset (no cause enabled)
		request = 0;		// IF = 0000h (nothing latched)
		masterEnable = false;	// IME = 0 (all interrupts disabled)
	}
}
