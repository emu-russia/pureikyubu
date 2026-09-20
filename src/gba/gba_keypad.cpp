// GBA keypad: KEYINPUT (0x04000130) and KEYCNT (0x04000132).
//
// Implemented from GBATEK "GBA Keypad Input":
//
//   4000130h KEYINPUT (R): bit 0-9 = A B Select Start Right Left Up Down R L, 0 = pressed,
//   1 = released; bits 10-15 are not used (the class reads them back as ones, the value an
//   unconnected input pulls to).
//
//   4000132h KEYCNT (R/W):
//     bit 0-9  the keys the interrupt watches (0 = ignore, 1 = select)
//     bit 14   keypad interrupt enable
//     bit 15   condition: 0 = logical OR (any selected key pressed),
//                         1 = logical AND (all selected keys pressed)
//
// The condition uses the *selected* keys only. With nothing selected there is no condition to
// meet, so ConditionMet() is false - an interrupt that watches no key never fires.

#include "gba_keypad.h"

namespace GBA
{
	void Keypad::Reset()
	{
		pressed = 0;
		control = 0;
	}

	bool Keypad::ConditionMet() const
	{
		// GBATEK "GBA Keypad Input": KEYCNT bits 0-9 select the keys the interrupt watches.
		// The selection is compared against the pressed mask (1 = held down), which is the
		// complement of the KEYINPUT register the software reads.
		uint16_t selected = (uint16_t)(control & 0x03FF);

		if (control & 0x8000)
		{
			// Logical AND: "an interrupt is requested when ALL of the selected buttons are
			// pressed" (GBATEK 4000132h). With no button selected that is vacuously true, so the
			// request is up the moment the register is written - which is how the AGB aging
			// cartridge raises the keypad interrupt with nothing held down: it puts KEYCNT at
			// C000h (logical AND, empty selection) and expects the flag. The request is level
			// driven, so it stays up until the condition is cleared.
			return (pressed & selected) == selected;
		}

		if (selected == 0)
			return false;								// logical OR: nothing to press

		return (pressed & selected) != 0;				// logical OR: any selected key down
	}
}
