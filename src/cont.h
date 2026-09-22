/*

# The standard controller (DOL-003)

The pad is an intelligent Joybus device on one of the four SI channels: the console polls it, and it
answers with its buttons and its six analog channels. It runs a state machine of its own, owns the
factory calibration EEPROM of its analog channels, and carries a rumble motor the host programs.

The console polls it with command 0x40 and reads the answer out of SICnINBUFH/L, which is what the
SI register file does from the state this device publishes (`Poll`, see standard-controller.md 4.2).
The other commands are the communication ones: 0x00 is the identification (the type of the device
and its status, including the latched motor state), 0x41 reads the calibration origins and 0x42
writes them, and 0xFF resets the device.

The actuators are the controls the host drives. A keyboard key can only say "pressed", so the two
sticks publish the half and the full deflection of every direction as separate controls: that is
what gives a key-driven stick its magnitude. A game controller drives the same controls with its
buttons and its axes, and the deflection of an axis is scaled into the range of the control it is
bound to. The device turns the values of all of them into the state the SI register file exposes.

The module registers its model with the peripheral subsystem from a constructor of its own (see the
end of cont.cpp): a build that does not compile this file simply has no controllers, and the pool
says so when a configuration asks for one.

*/

#pragma once

//! The actuators of the pad, in the order of ContPad::Actuator. The id of an actuator (which is the
//! name of its binding in the configuration, "Device0_VKEY_FOR_A") is given by the descriptor; the
//! order is what a consumer that drives a pad programmatically (the unit tests, a future scripting
//! or netplay layer) addresses a control by.
enum
{
	PAD_ACT_UP = 0,
	PAD_ACT_DOWN,
	PAD_ACT_LEFT,
	PAD_ACT_RIGHT,
	PAD_ACT_XUP50,
	PAD_ACT_XUP100,
	PAD_ACT_XDOWN50,
	PAD_ACT_XDOWN100,
	PAD_ACT_XLEFT50,
	PAD_ACT_XLEFT100,
	PAD_ACT_XRIGHT50,
	PAD_ACT_XRIGHT100,
	PAD_ACT_CXUP,
	PAD_ACT_CXDOWN,
	PAD_ACT_CXLEFT,
	PAD_ACT_CXRIGHT,
	PAD_ACT_TRIGGERL,
	PAD_ACT_TRIGGERR,
	PAD_ACT_TRIGGERZ,
	PAD_ACT_A,
	PAD_ACT_B,
	PAD_ACT_X,
	PAD_ACT_Y,
	PAD_ACT_START,

	PAD_ACT_MAX
};
