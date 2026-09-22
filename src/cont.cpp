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

// ---------------------------------------------------------------------------
// The configuration of a pad
//
// A binding is one variable per host control ("VKEY_FOR_A" is the key of A, "GCKEY_FOR_A" the game
// controller control of it), and the device index is the position of the device in the pool, which
// is the name of the variables of that device (see peripherals.h).
//
// A configuration that was written before the device pool existed keeps the bindings of a socket
// under the name of the socket ("VKEY_FOR_A_0" for the first one), which is where a pad that has no
// binding of its own looks for one.

namespace
{
	int LoadBinding(int index, const char* kind, const char* suffix, bool& found)
	{
		char key[0x80];

		sprintf(key, "%s_FOR_%s", kind, suffix);

		if (PeriphConfigExists(index, key))
		{
			found = true;
			return PeriphConfigInt(index, key, 0);
		}

		sprintf(key, "%s_FOR_%s_%i", kind, suffix, index);

		if (ConfigValueExists(key, USER_PADS))
		{
			found = true;
			return GetConfigInt(key, USER_PADS);
		}

		found = false;
		return 0;
	}

	void SaveBinding(int index, const char* kind, const char* suffix, int value)
	{
		char key[0x80];
		sprintf(key, "%s_FOR_%s", kind, suffix);
		PeriphConfigSetInt(index, key, value);
	}
}

// ---------------------------------------------------------------------------
// The device

class ContPad : public PeripheralDevice
{
	int             index = -1;                     //!< the pool index (names the configuration)
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
	ContPad(int index) : index(index)
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

	void LoadConfig(int index) override
	{
		this->index = index;

		bool configured = false;

		for (int i = 0; i < PAD_ACT_MAX; i++)
		{
			bool found = false;

			bindings[i].keyboard = LoadBinding(index, "VKEY", pad_actuators[i].id, found);
			configured |= found;

			bindings[i].gamepad = LoadBinding(index, "GCKEY", pad_actuators[i].id, found);
			configured |= found;
		}

		// A pad that has never been configured gets the standard layout of its model right away, so
		// that a pad which is plugged into a socket works without a trip to the settings window. The
		// keyboard half of it only goes to the first pad of the pool (see FirstOfModel): one set of
		// keys cannot drive the pads of four sockets at once. Nothing is written back here: the
		// layout is what this run uses, and building the pool must not change the configuration.
		if (!configured)
		{
			Peripherals::Instance().ApplyDefaultBindings(index, Peripherals::Instance().FirstOfModel(index));
		}
	}

	void SaveConfig(int index) override
	{
		for (int i = 0; i < PAD_ACT_MAX; i++)
		{
			SaveBinding(index, "VKEY", pad_actuators[i].id, bindings[i].keyboard);
			SaveBinding(index, "GCKEY", pad_actuators[i].id, bindings[i].gamepad);
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

			// The standard poll: the data the guest reads comes from the poll (see Poll), so a
			// communication transfer of it has nothing to answer.
			case 0x40:
			case 0x42:
				return;

			// Read the calibration origins
			case 0x41:
			{
				buf[0] = 0x41;
				buf[1] = 0;
				buf[2] = buf[3] = buf[4] = buf[5] = 0x80;
				buf[6] = buf[7] = 0x1f;
				break;
			}

			default:
			{
				Debug::Halt(
					"Unknown SI command. chan:%i, cmd:%02X, out:%i, in:%i\n",
					port >= PERIPH_PORT_SI0 ? port - PERIPH_PORT_SI0 : 0, cmd, outlen, inlen);
			}
		}
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

		// The digital L and R are only reported when their analog control is pressed all the way
		// down, which is what the pad does.
		if (Act(PAD_ACT_TRIGGERL) > 0)
		{
			button |= PAD_TRIGGER_L;
		}

		if (Act(PAD_ACT_TRIGGERR) > 0)
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
	PeripheralDevice* CreateStandardPad(int index)
	{
		return new ContPad(index);
	}

	//! The pad registers its own factory with the peripheral subsystem, so that the pool can create
	//! one. A build that does not compile this file has no standard controllers.
	struct ContRegistrar
	{
		ContRegistrar()
		{
			Peripherals::RegisterFactory(PERIPH_DEVICE_STANDARD_PAD, "Standard Controller",
				"DOL-003, the pad on one of the four SI sockets", PERIPH_BUS_SI, CreateStandardPad);
		}
	};

	ContRegistrar cont_registrar;
}
