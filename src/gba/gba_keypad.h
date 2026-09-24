// GBA keypad: KEYINPUT (0x04000130) and KEYCNT (0x04000132).
//
// The keys are active low in KEYINPUT. KEYCNT selects the keys that raise the keypad interrupt
// (bit 14 = the interrupt is enabled, bit 15 = all selected keys have to be down, otherwise any
// one of them is enough).

#pragma once

#include "gba_types.h"

namespace GBA
{
	class StateWriter;
	class StateReader;

	// The bits of KEYINPUT. The frontend fills a mask of KEY_* bits with 1 = pressed; the
	// register is the complement of that mask.
	enum KeyBit : uint16_t
	{
		KEY_A = 0x0001,
		KEY_B = 0x0002,
		KEY_SELECT = 0x0004,
		KEY_START = 0x0008,
		KEY_RIGHT = 0x0010,
		KEY_LEFT = 0x0020,
		KEY_UP = 0x0040,
		KEY_DOWN = 0x0080,
		KEY_R = 0x0100,
		KEY_L = 0x0200,
	};

	class Keypad
	{
		uint16_t pressed = 0;		// 1 = the key is held down
		uint16_t control = 0;		// 0x04000132 KEYCNT

	public:
		void Reset();

		/// <summary>Write the held keys and KEYCNT into a save state. The keys are in it as well
		/// as KEYCNT: a state loaded while a key is held has to keep it held, or the machine would
		/// see a release the user never made.</summary>
		void SaveState(StateWriter& writer) const;
		void LoadState(StateReader& reader);

		/// <summary>Replace the pressed-key mask (the frontend calls this once per frame).</summary>
		void SetPressed(uint16_t mask) { pressed = mask; }

		uint16_t Pressed() const { return pressed; }

		/// <summary>KEYINPUT: the complement of the pressed mask, bits 10-15 read as ones.</summary>
		uint16_t ReadKeyInput() const { return (uint16_t)(~pressed & 0x03FF) | 0xFC00; }

		uint16_t ReadKeyCnt() const { return control; }
		void WriteKeyCnt(uint16_t value) { control = value; }

		/// <summary>True when the keypad condition of KEYCNT is met right now.</summary>
		bool ConditionMet() const;

		/// <summary>True when KEYCNT enables the keypad interrupt and its condition is met.</summary>
		bool IrqRequested() const { return (control & 0x4000) != 0 && ConditionMet(); }
	};
}
