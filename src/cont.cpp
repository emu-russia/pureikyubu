/*

# The standard controller (DOL-003)

This is the implementation of the pad the header describes: the actuator table, the way the bindings
of the device are kept in the configuration, the state the SI register file polls and the command
set a communication transfer is answered with.

*/

#include "pch.h"
#include "cont.h"

using namespace Debug;

//! The actuators of the pad (see PAD_ACT_* in cont.h). `max` is the value a fully pressed or
//! deflected control has: a button says 0 or 1, a direction of a stick carries the size of its
//! deflection, and the two triggers have the range of the analog channel of the pad.
static const PeriphActuator pad_actuators[PAD_ACT_MAX] =
{
	{ "Buttons",       "Up",          "UP",         1 },
	{ "Buttons",       "Down",        "DOWN",       1 },
	{ "Buttons",       "Left",        "LEFT",       1 },
	{ "Buttons",       "Right",       "RIGHT",      1 },
	{ "Control Stick", "Up 50%",      "XUP50",      63 },
	{ "Control Stick", "Up 100%",     "XUP100",     127 },
	{ "Control Stick", "Down 50%",    "XDOWN50",    63 },
	{ "Control Stick", "Down 100%",   "XDOWN100",   127 },
	{ "Control Stick", "Left 50%",    "XLEFT50",    63 },
	{ "Control Stick", "Left 100%",   "XLEFT100",   127 },
	{ "Control Stick", "Right 50%",   "XRIGHT50",   63 },
	{ "Control Stick", "Right 100%",  "XRIGHT100",  127 },
	{ "C Stick",       "C Up",        "CXUP",       127 },
	{ "C Stick",       "C Down",      "CXDOWN",     127 },
	{ "C Stick",       "C Left",      "CXLEFT",     127 },
	{ "C Stick",       "C Right",     "CXRIGHT",    127 },
	{ "Triggers",      "L",           "TRIGGERL",   255 },
	{ "Triggers",      "R",           "TRIGGERR",   255 },
	{ "Triggers",      "Z",           "TRIGGERZ",   1 },
	{ "Buttons",       "A",           "A",          1 },
	{ "Buttons",       "B",           "B",          1 },
	{ "Buttons",       "X",           "X",          1 },
	{ "Buttons",       "Y",           "Y",          1 },
	{ "Buttons",       "Start",       "START",      1 },
};

//! The analog travel at which the trigger's click switch closes. The L and R controls are analog,
//! and the digital L / R bits of the poll answer come from the mechanical click at the end of
//! their travel (standard-controller.md 1, 4.2), not from "the trigger is touched at all": the
//! pad reports the click as a separate bit and the console's own clamping treats 0..30 as the
//! dead zone and 180..255 as the outer one (standard-controller.md 6.3), so a value past the
//! middle of the travel is where a pressed trigger is. The exact point is not documented; this one
//! sits above the dead zone of the console's own clamp, so a trigger that is only brushed is
//! analog-only and a trigger that is pressed reports both.
#define PAD_TRIGGER_CLICK   176


// ---------------------------------------------------------------------------
// The configuration of a pad
//
// A binding is one member per host control ("VKEY_FOR_A" is the key of A, "GCKEY_FOR_A" the game
// controller control of it) of the entry the pool gave this device (see config.h), so a pad that is
// added is the only one the settings belong to.

namespace
{
	int LoadBinding(const ConfigEntry* config, const char* kind, const char* suffix, bool& found)
	{
		char key[0x80];
		sprintf(key, "%s_FOR_%s", kind, suffix);

		if (!ConfigEntryValueExists(config, key))
		{
			found = false;
			return 0;
		}

		found = true;
		return GetConfigEntryInt(config, key, 0);
	}

	void SaveBinding(ConfigEntry* config, const char* kind, const char* suffix, int value)
	{
		char key[0x80];
		sprintf(key, "%s_FOR_%s", kind, suffix);
		SetConfigEntryInt(config, key, value);
	}
}

// ---------------------------------------------------------------------------
// The device

class ContPad : public PeripheralDevice
{
	int             port = -1;
	int             state[PAD_ACT_MAX] = { 0 };     //!< the value of every actuator
	PeriphBindings  bindings[PAD_ACT_MAX];
	int             motor = PAD_MOTOR_STOP;

	//! The value of a control, as the host drives it.
	int Act(int actuator) const { return state[actuator]; }

	//! The pressure of a direction of a stick: the half and the full control of the same direction
	//! may both be driven at once, and the larger of the two is what the stick sees.
	int Dir(int half, int full) const { return my_max(state[half], state[full]); }

	//! One axis of a stick from the two directions that drive it. The actuator keeps the deviation
	//! from the middle of the channel, which is what the guest reads (see si.cpp).
	static int8_t StickAxis(int negative, int positive)
	{
		int value = positive - negative;

		if (value > 127) value = 127;
		if (value < -127) value = -127;

		return (int8_t)value;
	}

public:
	ContPad()
	{
		for (int i = 0; i < PAD_ACT_MAX; i++)
		{
			bindings[i].keyboard = 0;
			bindings[i].gamepad = 0;
		}
	}

	uint32_t Type() override { return PERIPH_DEVICE_STANDARD_PAD; }

	int ActuatorCount() override { return PAD_ACT_MAX; }
	const PeriphActuator* Actuator(int index) override
	{
		return (index >= 0 && index < PAD_ACT_MAX) ? &pad_actuators[index] : nullptr;
	}
	PeriphBindings* ActuatorBindings(int index) override
	{
		return (index >= 0 && index < PAD_ACT_MAX) ? &bindings[index] : nullptr;
	}

	void SetState(int actuator, int value) override
	{
		if (actuator >= 0 && actuator < PAD_ACT_MAX)
		{
			if (value < 0) value = 0;
			if (value > pad_actuators[actuator].max) value = pad_actuators[actuator].max;

			state[actuator] = value;
		}
	}

	// -----------------------------------------------------------------------

	void LoadConfig() override
	{
		bool configured = false;

		for (int i = 0; i < PAD_ACT_MAX; i++)
		{
			bool found = false;

			bindings[i].keyboard = LoadBinding(config, "VKEY", pad_actuators[i].id, found);
			configured |= found;

			bindings[i].gamepad = LoadBinding(config, "GCKEY", pad_actuators[i].id, found);
			configured |= found;
		}

		// A pad that has never been configured gets the standard layout of its model right away, so
		// that a pad which is plugged into a socket works without a trip to the settings window. The
		// keyboard half of it only goes to the first pad of the pool (see FirstOfModel): one set of
		// keys cannot drive the pads of four sockets at once. Nothing is written back here: the
		// layout is what this run uses, and building the pool must not change the configuration.
		if (!configured)
		{
			Peripherals& pool = Peripherals::Instance();
			pool.ApplyDefaultBindings(this, pool.FirstOfModel(this));
		}
	}

	void SaveConfig() override
	{
		for (int i = 0; i < PAD_ACT_MAX; i++)
		{
			SaveBinding(config, "VKEY", pad_actuators[i].id, bindings[i].keyboard);
			SaveBinding(config, "GCKEY", pad_actuators[i].id, bindings[i].gamepad);
		}
	}

	void Attach(int port) override
	{
		this->port = port;
	}

	void Detach() override
	{
		// A motor that is still running must be stopped on the host when the pad goes away.
		HostInput* host = Peripherals::Instance().Host();

		if (motor != PAD_MOTOR_STOP && host != nullptr)
		{
			host->Rumble(HostPadOfPort(port), PAD_MOTOR_STOP);
		}

		motor = PAD_MOTOR_STOP;
		port = -1;
	}

	// -----------------------------------------------------------------------

	//! The answer to the Joybus command set (standard-controller.md 3).
	void Transfer(int outlen, int inlen, uint8_t* buf) override
	{
		uint8_t cmd = buf[0];

		switch (cmd)
		{
			// Get type and status
			case 0x00:
			{
				// 0 : use sub-type
				// 2 : n64 mouse
				// 5 : n64 controller
				// 9 : default gc controller
				buf[0] = 9;
				buf[1] = 0;         // sub-type
				buf[2] = (uint8_t)(motor << 4);     // STAT: the latched motor state is bits 5:4
				break;
			}

			// The standard poll. Over the automatic poll the guest reads the state out of
			// SICnINBUFH/L (see Poll and si.cpp), but a communication transfer of 0x40 asks the same
			// question over the wire and has to be answered with the same eight bytes: the console
			// builds its answer here from the state of its own controls.
			case 0x40:
			{
				PADState state;
				Poll(&state);
				PackResponse(&state, buf);
				break;
			}

			// Write Joy Port: 34 data bytes and a CRC go out, one byte comes back. The emulated pad
			// keeps no writable EEPROM, so the write is acknowledged and dropped (the origins it
			// answers 0x41 with are the ones its analog channels are calibrated at).
			case 0x42:
				buf[0] = 0;
				break;

			// Read the calibration origins
			case 0x41:
			{
				buf[0] = 0x41;
				buf[1] = 0;
				buf[2] = buf[3] = buf[4] = buf[5] = 0x80;
				buf[6] = buf[7] = 0x1f;
				break;
			}

			// Reset: the state machine goes back to its power-on state and the latched errors are
			// cleared, and the answer is the identification - the same three bytes 0x00 gives
			// (standard-controller.md 3). "Clears the error latches" reaches the host as well: the
			// latched motor command is dropped, so a motor that was rumbling stops.
			case 0xFF:
			{
				motor = PAD_MOTOR_STOP;

				HostInput* host = Peripherals::Instance().Host();

				if (host != nullptr)
				{
					host->Rumble(HostPadOfPort(port), PAD_MOTOR_STOP);
				}

				buf[0] = 9;
				buf[1] = 0;
				buf[2] = (uint8_t)(motor << 4);
				break;
			}

			// A command the controller does not know: it answers with its type byte once and
			// abandons the transfer (standard-controller.md 3). The emulation reports it instead of
			// stopping, because a guest that probes an unknown opcode is not a broken machine.
			default:
			{
				Report(Channel::SI,
					"Unknown SI command. chan:%i, cmd:%02X, out:%i, in:%i\n",
					port >= PERIPH_PORT_SI0 ? port - PERIPH_PORT_SI0 : 0, cmd, outlen, inlen);

				buf[0] = 9;
				break;
			}
		}
	}

	//! The eight bytes of the 0x40 answer, in the order they go out on the wire (the response
	//! bytes of standard-controller.md 4.2). The automatic poll fills SICnINBUFH/L with the same
	//! bytes (see si_inh_*/si_inl_*), so the two paths cannot disagree about what a pad reports.
	static void PackResponse(const PADState* pad, uint8_t* buf)
	{
		buf[0] = (uint8_t)(pad->button >> 8);   // A, B, X, Y, Start and the EEPROM state
		buf[1] = (uint8_t)pad->button;          // the D-pad and L / R / Z
		// PADState keeps signed deflections; Joybus reports axes around the 0x80 origin.
		buf[2] = (uint8_t)(pad->stickX + 0x80);
		buf[3] = (uint8_t)(pad->stickY + 0x80);
		buf[4] = (uint8_t)(pad->substickX + 0x80);
		buf[5] = (uint8_t)(pad->substickY + 0x80);
		buf[6] = pad->triggerLeft;
		buf[7] = pad->triggerRight;
	}

	//! The state the guest polls: the buttons and the six analog channels.
	bool Poll(PADState* pad) override
	{
		memset(pad, 0, sizeof(PADState));

		uint16_t button = 0;

		if (Act(PAD_ACT_UP)) button |= PAD_BUTTON_UP;
		if (Act(PAD_ACT_DOWN)) button |= PAD_BUTTON_DOWN;
		if (Act(PAD_ACT_LEFT)) button |= PAD_BUTTON_LEFT;
		if (Act(PAD_ACT_RIGHT)) button |= PAD_BUTTON_RIGHT;

		if (Act(PAD_ACT_A)) button |= PAD_BUTTON_A;
		if (Act(PAD_ACT_B)) button |= PAD_BUTTON_B;
		if (Act(PAD_ACT_X)) button |= PAD_BUTTON_X;
		if (Act(PAD_ACT_Y)) button |= PAD_BUTTON_Y;
		if (Act(PAD_ACT_START)) button |= PAD_BUTTON_START;

		// The digital L and R come from the click switch at the end of the trigger's travel, so a
		// trigger that is only brushed is analog-only (see PAD_TRIGGER_CLICK).
		if (Act(PAD_ACT_TRIGGERL) >= PAD_TRIGGER_CLICK)
		{
			button |= PAD_TRIGGER_L;
		}

		if (Act(PAD_ACT_TRIGGERR) >= PAD_TRIGGER_CLICK)
		{
			button |= PAD_TRIGGER_R;
		}

		if (Act(PAD_ACT_TRIGGERZ))
		{
			button |= PAD_TRIGGER_Z;
		}

		pad->button = button;

		pad->stickX = StickAxis(Dir(PAD_ACT_XLEFT50, PAD_ACT_XLEFT100), Dir(PAD_ACT_XRIGHT50, PAD_ACT_XRIGHT100));
		pad->stickY = StickAxis(Dir(PAD_ACT_XDOWN50, PAD_ACT_XDOWN100), Dir(PAD_ACT_XUP50, PAD_ACT_XUP100));
		pad->substickX = StickAxis(state[PAD_ACT_CXLEFT], state[PAD_ACT_CXRIGHT]);
		pad->substickY = StickAxis(state[PAD_ACT_CXDOWN], state[PAD_ACT_CXUP]);
		pad->triggerLeft = (uint8_t)Act(PAD_ACT_TRIGGERL);
		pad->triggerRight = (uint8_t)Act(PAD_ACT_TRIGGERR);

		return true;
	}

	bool SetMotor(int cmd) override
	{
		HostInput* host = Peripherals::Instance().Host();

		if (host == nullptr)
		{
			return false;
		}

		motor = cmd;

		return host->Rumble(HostPadOfPort(port), cmd);
	}
};

// ---------------------------------------------------------------------------
// The registration

namespace
{
	PeripheralDevice* CreateStandardPad()
	{
		return new ContPad();
	}

	//! The pad registers its own factory with the peripheral subsystem, so that the pool can create
	//! one. A build that does not compile this file has no standard controllers.
	struct ContRegistrar
	{
		ContRegistrar()
		{
			Peripherals::RegisterFactory(PERIPH_DEVICE_STANDARD_PAD, "Standard Controller",
				"DOL-003, the pad on one of the four SI sockets", PERIPH_BUS_SI,
				PERIPH_PAGE_CONTROLLERS, CreateStandardPad);
		}
	};

	ContRegistrar cont_registrar;
}
