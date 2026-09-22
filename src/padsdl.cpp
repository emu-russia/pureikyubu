/*

SDL2 host input (the front end side of the peripheral subsystem).

This is the only file of the emulator that talks to SDL about input. It implements `HostInput` (see
peripherals.h), so the emulated devices - the standard controller of peripherals.cpp - never make an
SDL call: the subsystem resolves their bindings against this backend and hands them the values of
their actuators.

The backend owns the SDL game controllers and a snapshot of their state. The SDL objects belong to
the thread that pumps the SDL events (the UI thread): `Update` opens and closes them and takes a
plain snapshot, which is what the emulation thread reads. That keeps a controller from being closed
while the emulation is reading it, and it is also why the rumble motor is a request that the next
`Update` applies rather than a direct SDL call.

The bindings are SDL scancodes for the keyboard and an SDL game controller button or axis for a
game controller, both stored as one integer (see PERIPH_HOST_* in peripherals.h). The Nth connected
game controller drives the pad in Port N, and that mapping is the subsystem's (see HostPadOf in
peripherals.cpp); the backend only reports what is connected.

*/

#include "pch.h"

using namespace Debug;

// An axis bound to a digital control is pressed at 50% of its travel.
#define PAD_AXIS_PRESS_THRESHOLD    16384

// ---------------------------------------------------------------------------
// The snapshot of the SDL game controllers
//
// The pointers to the SDL objects and the device list belong to the event thread; this structure is
// what the emulation thread is allowed to see, and it is only ever read under the lock.

struct PADGAMEPAD
{
	bool    present;
	char    name[0x80];
	Uint8   buttons[SDL_CONTROLLER_BUTTON_MAX];
	Sint16  axes[SDL_CONTROLLER_AXIS_MAX];
};

//! The rumble command the emulation asked for and the next Update has to apply.
struct PADRUMBLE
{
	bool    dirty;
	int     cmd;
};

class SdlHostInput : public HostInput
{
	static const int MaxPads = 4;

	// The event thread side
	SDL_GameController* devices[MaxPads] = { nullptr };
	SDL_JoystickID      device_ids[MaxPads] = { 0 };
	SDL_JoystickID      failed_ids[MaxPads] = { 0 };      // devices that could not be opened (do not retry every frame)
	bool                sdl_inited = false;

	// The shared side
	PADGAMEPAD  snapshot[MaxPads];
	PADRUMBLE   rumble[MaxPads];
	SpinLock    snapshot_lock;

public:
	void Update() override;
	bool KeyDown(int scancode) override;
	int GamepadCount() override;
	std::string GamepadName(int pad) override;
	bool GamepadButton(int pad, int button) override;
	int GamepadAxis(int pad, int axis) override;
	std::string BindingName(int binding) override;
	void DefaultBindings(uint32_t type, const char* actuator, int& keyboard, int& gamepad) override;
	bool Rumble(int pad, int cmd) override;
};

void SdlHostInput::Update()
{
	if (!sdl_inited)
	{
		if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0)
		{
			Report(Channel::SI, "SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) failed: %s\n", SDL_GetError());
		}
		sdl_inited = true;
	}

	// The connected game controllers, in the SDL enumeration order

	int numJoysticks = SDL_NumJoysticks();
	int connected[MaxPads];
	int connectedCount = 0;

	for (int i = 0; i < numJoysticks && connectedCount < MaxPads; i++)
	{
		if (SDL_IsGameController(i))
		{
			connected[connectedCount++] = i;
		}
	}

	// Open the controllers of the connected devices and close the devices that are gone.
	// A device is reopened when its position in the list changes, so that the pad assignment follows
	// the order of the connected game controllers.

	for (int slot = 0; slot < MaxPads; slot++)
	{
		SDL_JoystickID wanted = (slot < connectedCount) ? SDL_JoystickGetDeviceInstanceID(connected[slot]) : -1;

		if (devices[slot] != nullptr && device_ids[slot] != wanted)
		{
			SDL_GameControllerClose(devices[slot]);
			devices[slot] = nullptr;
		}

		if (devices[slot] == nullptr && wanted != -1 && failed_ids[slot] != wanted)
		{
			devices[slot] = SDL_GameControllerOpen(connected[slot]);

			if (devices[slot] != nullptr)
			{
				failed_ids[slot] = 0;
				device_ids[slot] = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(devices[slot]));
				Report(Channel::SI, "Gamepad %i: %s\n", slot + 1, SDL_GameControllerName(devices[slot]));
			}
			else
			{
				failed_ids[slot] = wanted;
				Report(Channel::SI, "Failed to open gamepad %i: %s\n", slot + 1, SDL_GetError());
			}
		}
	}

	// Apply the rumble the emulation asked for while the SDL objects are ours to touch.

	snapshot_lock.Lock();

	for (int slot = 0; slot < MaxPads; slot++)
	{
		if (!rumble[slot].dirty)
		{
			continue;
		}

		rumble[slot].dirty = false;

		if (devices[slot] == nullptr)
		{
			continue;
		}

		// "Stop hard" is the motor being braked; a host controller can only be told to stop.
		Uint16 strength = (rumble[slot].cmd == PAD_MOTOR_RUMBLE) ? 0x8000 : 0;

		// A duration of "forever": the motor runs until the emulation asks for something else.
		if (SDL_GameControllerRumble(devices[slot], strength, strength, 0xffffffffu) < 0)
		{
			Report(Channel::SI, "Gamepad %i: cannot rumble (%s)\n", slot + 1, SDL_GetError());
		}
	}

	snapshot_lock.Unlock();

	// Take the snapshot the emulation thread reads.

	PADGAMEPAD fresh[MaxPads];
	memset(fresh, 0, sizeof(fresh));

	for (int slot = 0; slot < MaxPads; slot++)
	{
		if (devices[slot] == nullptr)
		{
			continue;
		}

		fresh[slot].present = true;

		const char* name = SDL_GameControllerName(devices[slot]);
		snprintf(fresh[slot].name, sizeof(fresh[slot].name), "%s", name != nullptr ? name : "?");

		for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; b++)
		{
			fresh[slot].buttons[b] = SDL_GameControllerGetButton(devices[slot], (SDL_GameControllerButton)b);
		}

		for (int a = 0; a < SDL_CONTROLLER_AXIS_MAX; a++)
		{
			fresh[slot].axes[a] = SDL_GameControllerGetAxis(devices[slot], (SDL_GameControllerAxis)a);
		}
	}

	snapshot_lock.Lock();
	memcpy(snapshot, fresh, sizeof(snapshot));
	snapshot_lock.Unlock();
}

bool SdlHostInput::KeyDown(int scancode)
{
	if (scancode <= 0 || scancode >= SDL_NUM_SCANCODES)
	{
		return false;
	}

	const Uint8* keys = SDL_GetKeyboardState(NULL);

	return keys != nullptr && keys[scancode] != 0;
}

int SdlHostInput::GamepadCount()
{
	snapshot_lock.Lock();

	int count = 0;

	for (int i = 0; i < MaxPads; i++)
	{
		if (snapshot[i].present)
		{
			count++;
		}
	}

	snapshot_lock.Unlock();

	return count;
}

std::string SdlHostInput::GamepadName(int pad)
{
	if (pad < 0 || pad >= MaxPads)
	{
		return "";
	}

	snapshot_lock.Lock();
	std::string name = snapshot[pad].present ? snapshot[pad].name : "";
	snapshot_lock.Unlock();

	return name;
}

bool SdlHostInput::GamepadButton(int pad, int button)
{
	if (pad < 0 || pad >= MaxPads || button < 0 || button >= SDL_CONTROLLER_BUTTON_MAX)
	{
		return false;
	}

	snapshot_lock.Lock();
	bool pressed = snapshot[pad].present && snapshot[pad].buttons[button] != 0;
	snapshot_lock.Unlock();

	return pressed;
}

int SdlHostInput::GamepadAxis(int pad, int axis)
{
	if (pad < 0 || pad >= MaxPads || axis < 0 || axis >= SDL_CONTROLLER_AXIS_MAX)
	{
		return 0;
	}

	snapshot_lock.Lock();
	int value = snapshot[pad].present ? snapshot[pad].axes[axis] : 0;
	snapshot_lock.Unlock();

	return value;
}

bool SdlHostInput::Rumble(int pad, int cmd)
{
	if (pad < 0 || pad >= MaxPads)
	{
		return false;
	}

	snapshot_lock.Lock();
	bool present = snapshot[pad].present;
	rumble[pad].cmd = cmd;
	rumble[pad].dirty = true;
	snapshot_lock.Unlock();

	return present;
}

// ---------------------------------------------------------------------------
// The names of the bindings

static const char* gamepad_button_name[SDL_CONTROLLER_BUTTON_MAX] =
{
	"A", "B", "X", "Y", "Back", "Guide", "Start", "L Stick", "R Stick",
	"L Shoulder", "R Shoulder", "DPad Up", "DPad Down", "DPad Left", "DPad Right",
	"Misc", "Paddle 1", "Paddle 2", "Paddle 3", "Paddle 4", "Touchpad",
};

static const char* gamepad_axis_name[SDL_CONTROLLER_AXIS_MAX] =
{
	"L Stick X", "L Stick Y", "R Stick X", "R Stick Y", "L Trigger", "R Trigger",
};

std::string SdlHostInput::BindingName(int binding)
{
	if (PERIPH_HOST_IS_BUTTON(binding))
	{
		int button = PERIPH_HOST_BUTTON(binding);

		if (button < 0 || button >= SDL_CONTROLLER_BUTTON_MAX)
		{
			return "?";
		}

		return gamepad_button_name[button];
	}

	if (PERIPH_HOST_IS_AXIS(binding))
	{
		int axis = PERIPH_HOST_AXIS(binding);

		if (axis < 0 || axis >= SDL_CONTROLLER_AXIS_MAX)
		{
			return "?";
		}

		return std::string(gamepad_axis_name[axis]) + (PERIPH_HOST_AXIS_POS(binding) ? " +" : " -");
	}

	if (binding <= 0 || binding >= SDL_NUM_SCANCODES)
	{
		return "...";
	}

	const char* name = SDL_GetScancodeName((SDL_Scancode)binding);

	return (name && *name) ? name : "?";
}

// ---------------------------------------------------------------------------
// The default bindings of a pad
//
// The usual Xbox-style layout: the main stick on the left stick, the C stick on the right stick and
// the L/R triggers on the analog triggers. The keys are the ones a GameCube pad has always had.

struct PadDefault
{
	const char* id;         //!< the PeriphActuator::id of the control
	int         keyboard;   //!< an SDL scancode
	int         gamepad;    //!< a PERIPH_HOST_* control
};

static const PadDefault pad_defaults[] =
{
	{ "UP",        SDL_SCANCODE_HOME,       PERIPH_HOST_MAKE_BUTTON(SDL_CONTROLLER_BUTTON_DPAD_UP) },
	{ "DOWN",      SDL_SCANCODE_END,        PERIPH_HOST_MAKE_BUTTON(SDL_CONTROLLER_BUTTON_DPAD_DOWN) },
	{ "LEFT",      SDL_SCANCODE_DELETE,     PERIPH_HOST_MAKE_BUTTON(SDL_CONTROLLER_BUTTON_DPAD_LEFT) },
	{ "RIGHT",     SDL_SCANCODE_PAGEDOWN,   PERIPH_HOST_MAKE_BUTTON(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) },
	{ "XUP50",     0,                       0 },
	{ "XUP100",    SDL_SCANCODE_UP,         PERIPH_HOST_MAKE_AXIS(SDL_CONTROLLER_AXIS_LEFTY, false) },
	{ "XDOWN50",   0,                       0 },
	{ "XDOWN100",  SDL_SCANCODE_DOWN,       PERIPH_HOST_MAKE_AXIS(SDL_CONTROLLER_AXIS_LEFTY, true) },
	{ "XLEFT50",   0,                       0 },
	{ "XLEFT100",  SDL_SCANCODE_LEFT,       PERIPH_HOST_MAKE_AXIS(SDL_CONTROLLER_AXIS_LEFTX, false) },
	{ "XRIGHT50",  0,                       0 },
	{ "XRIGHT100", SDL_SCANCODE_RIGHT,      PERIPH_HOST_MAKE_AXIS(SDL_CONTROLLER_AXIS_LEFTX, true) },
	{ "CXUP",      SDL_SCANCODE_KP_8,       PERIPH_HOST_MAKE_AXIS(SDL_CONTROLLER_AXIS_RIGHTY, false) },
	{ "CXDOWN",    SDL_SCANCODE_KP_2,       PERIPH_HOST_MAKE_AXIS(SDL_CONTROLLER_AXIS_RIGHTY, true) },
	{ "CXLEFT",    SDL_SCANCODE_KP_4,       PERIPH_HOST_MAKE_AXIS(SDL_CONTROLLER_AXIS_RIGHTX, false) },
	{ "CXRIGHT",   SDL_SCANCODE_KP_6,       PERIPH_HOST_MAKE_AXIS(SDL_CONTROLLER_AXIS_RIGHTX, true) },
	{ "TRIGGERL",  SDL_SCANCODE_Q,          PERIPH_HOST_MAKE_AXIS(SDL_CONTROLLER_AXIS_TRIGGERLEFT, true) },
	{ "TRIGGERR",  SDL_SCANCODE_W,          PERIPH_HOST_MAKE_AXIS(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, true) },
	{ "TRIGGERZ",  SDL_SCANCODE_E,          PERIPH_HOST_MAKE_BUTTON(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) },
	{ "A",         SDL_SCANCODE_X,          PERIPH_HOST_MAKE_BUTTON(SDL_CONTROLLER_BUTTON_A) },
	{ "B",         SDL_SCANCODE_Z,          PERIPH_HOST_MAKE_BUTTON(SDL_CONTROLLER_BUTTON_B) },
	{ "X",         SDL_SCANCODE_S,          PERIPH_HOST_MAKE_BUTTON(SDL_CONTROLLER_BUTTON_X) },
	{ "Y",         SDL_SCANCODE_A,          PERIPH_HOST_MAKE_BUTTON(SDL_CONTROLLER_BUTTON_Y) },
	{ "START",     SDL_SCANCODE_RETURN,     PERIPH_HOST_MAKE_BUTTON(SDL_CONTROLLER_BUTTON_START) },
};

void SdlHostInput::DefaultBindings(uint32_t type, const char* actuator, int& keyboard, int& gamepad)
{
	keyboard = 0;
	gamepad = 0;

	// The default layout of a pad; the other models will have their own (there is one device model
	// per bus for now, and a memory card has no actuators at all).
	if (type != PERIPH_DEVICE_STANDARD_PAD || actuator == nullptr)
	{
		return;
	}

	for (const auto& def : pad_defaults)
	{
		if (strcmp(def.id, actuator) == 0)
		{
			keyboard = def.keyboard;
			gamepad = def.gamepad;
			return;
		}
	}
}

// ---------------------------------------------------------------------------
// The backend of this build

static SdlHostInput* sdl_host_input = nullptr;

HostInput* HostInputCreate()
{
	sdl_host_input = new SdlHostInput();
	return sdl_host_input;
}

void HostInputDestroy()
{
	delete sdl_host_input;
	sdl_host_input = nullptr;
}

void HostInputUpdate()
{
	if (sdl_host_input != nullptr)
	{
		sdl_host_input->Update();
	}
}
