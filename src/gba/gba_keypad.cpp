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
		u16 selected = (u16)(control & 0x03FF);

		if (selected == 0)
			return false;

		if (control & 0x8000)
			return (pressed & selected) == selected;	// logical AND: all selected keys down

		return (pressed & selected) != 0;				// logical OR: any selected key down
	}
}
