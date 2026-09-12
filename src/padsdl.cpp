/*

GameCube controllers emulation backend (SDL2).

This is the SDL counterpart of the Win32 backend (pad.cpp). A pad can be driven by the keyboard
(polled with SDL_GetKeyboardState) and by an SDL game controller, both at the same time.

The key bindings are stored in the configuration (SettingsSdl.json, "controllers" section) as SDL
scancodes, and the game controller bindings as SDL game controller button or axis identifiers (see
PAD_GCKEY_* in pad.h). The settings dialog is a part of the SDL UI and captures the SDL key and
controller events (see the "Controller settings" section in uisdl.cpp).

Unlike the Win32 backend, the key bindings are SDL scancodes (SDL_Scancode), not virtual-key codes.
The SDL and the Win32 builds keep their settings in separate files (SettingsSdl.json /
SettingsWin.json, see config.h), so the same VKEY_FOR_* variable names may hold the two different
encodings without clashing. The game controller bindings live in the GCKEY_FOR_* variables.

The Nth SDL game controller that is currently connected drives the Nth pad, so a single gamepad
always works with Port 1, two gamepads with Port 1 and Port 2, and so on.

The backend consumer is the `si.cpp` module.

*/

// PAD API for emulator
#include "pch.h"

using namespace Debug;

// The names of the VKEY_FOR_* / GCKEY_FOR_* variables in the "controllers" section (in the enum order).
static const char* vkey_suffix[VKEY_FOR_MAX] =
{
	"UP",
	"DOWN",
	"LEFT",
	"RIGHT",
	"XUP50",
	"XUP100",
	"XDOWN50",
	"XDOWN100",
	"XLEFT50",
	"XLEFT100",
	"XRIGHT50",
	"XRIGHT100",
	"CXUP",
	"CXDOWN",
	"CXLEFT",
	"CXRIGHT",
	"TRIGGERL",
	"TRIGGERR",
	"TRIGGERZ",
	"A",
	"B",
	"X",
	"Y",
	"START",
};

static PADCONF pad[4];

// ---------------------------------------------------------------------------
// SDL game controllers
//
// The SDL objects are owned by the thread that pumps the SDL events: PADUpdateControllers()
// opens/closes the devices and stores a plain snapshot of their state, which is what the emulation
// thread reads in PADReadButtons(). Keeping the SDL pointers on the event thread avoids closing a
// controller while another thread is reading it.

#define PAD_AXIS_PRESS_THRESHOLD    16384   // an axis bound to a digital control is pressed at 50%

struct PADGAMEPAD
{
	bool    present;
	Uint8   buttons[SDL_CONTROLLER_BUTTON_MAX];
	Sint16  axes[SDL_CONTROLLER_AXIS_MAX];
};

static PADGAMEPAD pad_gamepads[4];
static SpinLock pad_gamepads_lock;

void PADUpdateControllers()
{
	static bool sdl_inited = false;
	static SDL_GameController* devices[4] = { nullptr };
	static SDL_JoystickID device_ids[4] = { 0 };
	static SDL_JoystickID failed_ids[4] = { 0 };      // devices that could not be opened (do not retry every frame)

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
	int connected[4];
	int connectedCount = 0;

	for (int i = 0; i < numJoysticks && connectedCount < 4; i++)
	{
		if (SDL_IsGameController(i))
		{
			connected[connectedCount++] = i;
		}
	}

	// Open the controllers of the connected devices and close the devices that are gone.
	// A device is reopened when its position in the list changes, so that the pad assignment follows
	// the order of the connected game controllers.

	for (int slot = 0; slot < 4; slot++)
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

	// Snapshot the state

	PADGAMEPAD snapshot[4];
	memset(snapshot, 0, sizeof(snapshot));

	for (int slot = 0; slot < 4; slot++)
	{
		if (devices[slot] == nullptr)
		{
			continue;
		}

		snapshot[slot].present = true;

		for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; b++)
		{
			snapshot[slot].buttons[b] = SDL_GameControllerGetButton(devices[slot], (SDL_GameControllerButton)b);
		}

		for (int a = 0; a < SDL_CONTROLLER_AXIS_MAX; a++)
		{
			snapshot[slot].axes[a] = SDL_GameControllerGetAxis(devices[slot], (SDL_GameControllerAxis)a);
		}
	}

	pad_gamepads_lock.Lock();
	memcpy(pad_gamepads, snapshot, sizeof(pad_gamepads));
	pad_gamepads_lock.Unlock();
}

// ---------------------------------------------------------------------------
// configuration

void PADLoadConfig(int padToConfigure)
{
	char parm[256] = { 0 };

	if (padToConfigure < 0 || padToConfigure > 3)
	{
		return;
	}

	// Plugged or not
	sprintf(parm, "PluggedIn_%i", padToConfigure);
	pad[padToConfigure].plugged = GetConfigBool(parm, USER_PADS);

	// Buttons
	for (int i = 0; i < VKEY_FOR_MAX; i++)
	{
		sprintf(parm, "VKEY_FOR_%s_%i", vkey_suffix[i], padToConfigure);
		pad[padToConfigure].vkeys[i] = GetConfigInt(parm, USER_PADS);

		sprintf(parm, "GCKEY_FOR_%s_%i", vkey_suffix[i], padToConfigure);
		pad[padToConfigure].gckeys[i] = GetConfigInt(parm, USER_PADS);
	}
}

// ---------------------------------------------------------------------------
// called when emulation started/stopped (pad controls)

bool PADOpen()
{
	for (int i = 0; i < 4; i++)
	{
		PADLoadConfig(i);
	}

	return true;    // ok
}

void PADClose()
{
	pad[0].plugged =
	pad[1].plugged =
	pad[2].plugged =
	pad[3].plugged = false;
}

// ---------------------------------------------------------------------------
// process input

#define THRESOLD    127
#define STICK_INITIAL	0

static void pad_reset_chan(PADState* state)
{
	memset(state, 0, sizeof(PADState));

	// The analog control values can range from -127 to +127.
	// The pad.a library further filters them by setting the lower cap to 15 and the upper cap to 87 for stick and 74 for cstick.
	// But these are purely programmatic fiddles that don't concern us.

	state->stickX = state->stickY = STICK_INITIAL;
	state->substickX = state->substickY = STICK_INITIAL;
}

// A key binding is a scancode; 0 (SDL_SCANCODE_UNKNOWN) and -1 mean "not assigned".
static bool pad_key_pressed(const Uint8* keys, int scancode)
{
	return scancode > 0 && scancode < SDL_NUM_SCANCODES && keys[scancode] != 0;
}

// A game controller button, or an axis deflected in the bound direction past the threshold.
static bool pad_gc_pressed(const PADGAMEPAD* gp, int binding)
{
	if (!gp->present)
	{
		return false;
	}

	if (PAD_GCKEY_IS_BUTTON(binding))
	{
		int button = PAD_GCKEY_BUTTON(binding);
		return button >= 0 && button < SDL_CONTROLLER_BUTTON_MAX && gp->buttons[button] != 0;
	}

	if (PAD_GCKEY_IS_AXIS(binding))
	{
		int axis = PAD_GCKEY_AXIS(binding);
		if (axis < 0 || axis >= SDL_CONTROLLER_AXIS_MAX) return false;
		int value = gp->axes[axis];
		return PAD_GCKEY_AXIS_POS(binding) ? (value > PAD_AXIS_PRESS_THRESHOLD) : (value < -PAD_AXIS_PRESS_THRESHOLD);
	}

	return false;
}

// The magnitude (0...127) of the deflection of the axis bound to the control. The caller applies
// the sign of the direction, the same way as for the keyboard bindings.
static int pad_gc_stick(const PADGAMEPAD* gp, int binding)
{
	if (!gp->present || !PAD_GCKEY_IS_AXIS(binding))
	{
		return 0;
	}

	int axis = PAD_GCKEY_AXIS(binding);
	if (axis < 0 || axis >= SDL_CONTROLLER_AXIS_MAX)
	{
		return 0;
	}

	int value = gp->axes[axis];
	bool positive = PAD_GCKEY_AXIS_POS(binding);

	if (positive && value <= 0) return 0;
	if (!positive && value >= 0) return 0;

	int scaled = (value < 0 ? -value : value) * 127 / 32767;
	return scaled > 127 ? 127 : scaled;
}

// The value (0...255) of the game controller trigger bound to the control. The trigger axes are
// unipolar (0...32767), so the direction of the binding is ignored.
static int pad_gc_trigger(const PADGAMEPAD* gp, int binding)
{
	if (!gp->present || !PAD_GCKEY_IS_AXIS(binding))
	{
		return 0;
	}

	int axis = PAD_GCKEY_AXIS(binding);
	if (axis < 0 || axis >= SDL_CONTROLLER_AXIS_MAX)
	{
		return 0;
	}

	int value = gp->axes[axis];
	if (value <= 0)
	{
		return 0;
	}

	int scaled = value * 255 / 32767;
	return scaled > 255 ? 255 : scaled;
}

// Both the 50% and the 100% binding of the same direction may be pressed at once,
// so the sum is clamped instead of overflowing int8_t.
static int8_t pad_stick_value(int value)
{
	if (value > 127) return 127;
	if (value < -127) return -127;
	return (int8_t)value;
}

// collect keyboard and gamepad buttons in PADState
bool PADReadButtons(long padnum, PADState* state)
{
	uint16_t button = 0;
	int stickX = 0, stickY = 0;
	int substickX = 0, substickY = 0;
	int triggerLeft = 0, triggerRight = 0;

	pad_reset_chan(state);

	if (padnum < 0 || padnum > 3) return false;

	if (!pad[padnum].plugged) return false;

	const Uint8* keys = SDL_GetKeyboardState(NULL);
	if (keys == nullptr) return false;

	PADGAMEPAD gp;
	pad_gamepads_lock.Lock();
	gp = pad_gamepads[padnum];
	pad_gamepads_lock.Unlock();

	PADCONF* conf = &pad[padnum];

	auto key = [&](int vkey) { return pad_key_pressed(keys, conf->vkeys[vkey]); };
	auto gc = [&](int vkey) { return pad_gc_pressed(&gp, conf->gckeys[vkey]); };
	auto pressed = [&](int vkey) { return key(vkey) || gc(vkey); };

	//
	// ===== PAD n =====
	//

	if (pressed(VKEY_FOR_UP)) button |= PAD_BUTTON_UP;
	if (pressed(VKEY_FOR_DOWN)) button |= PAD_BUTTON_DOWN;
	if (pressed(VKEY_FOR_LEFT)) button |= PAD_BUTTON_LEFT;
	if (pressed(VKEY_FOR_RIGHT)) button |= PAD_BUTTON_RIGHT;

	if (pressed(VKEY_FOR_A)) button |= PAD_BUTTON_A;
	if (pressed(VKEY_FOR_B)) button |= PAD_BUTTON_B;
	if (pressed(VKEY_FOR_X)) button |= PAD_BUTTON_X;
	if (pressed(VKEY_FOR_Y)) button |= PAD_BUTTON_Y;
	if (pressed(VKEY_FOR_START)) button |= PAD_BUTTON_START;

	// Note : digital L and R are only set when its analog key is pressed all the way down;
	// this plugin is only supporting the fact, that L/R are pressed.

	if (pressed(VKEY_FOR_TRIGGERL))
	{
		button |= PAD_TRIGGER_L;
		triggerLeft = 255;
	}
	if (pressed(VKEY_FOR_TRIGGERR))
	{
		button |= PAD_TRIGGER_R;
		triggerRight = 255;
	}

	if (pressed(VKEY_FOR_TRIGGERZ))
	{
		button |= PAD_TRIGGER_Z;
	}

	// The analog L/R triggers are the only controls with an analog value.

	int analog = pad_gc_trigger(&gp, conf->gckeys[VKEY_FOR_TRIGGERL]);
	if (analog > triggerLeft) triggerLeft = analog;

	analog = pad_gc_trigger(&gp, conf->gckeys[VKEY_FOR_TRIGGERR]);
	if (analog > triggerRight) triggerRight = analog;

	// The main stick: the keyboard binds the 50%/100% keys, the gamepad binds the stick axes.
	// The 50% and the 100% controls of a direction may share the same axis, so the largest value wins.

	if (key(VKEY_FOR_XUP50))    stickY += THRESOLD / 2;
	if (key(VKEY_FOR_XUP100))   stickY += THRESOLD;
	if (key(VKEY_FOR_XDOWN50))  stickY += -THRESOLD / 2;
	if (key(VKEY_FOR_XDOWN100)) stickY += -THRESOLD;
	if (key(VKEY_FOR_XRIGHT50))  stickX += THRESOLD / 2;
	if (key(VKEY_FOR_XRIGHT100)) stickX += THRESOLD;
	if (key(VKEY_FOR_XLEFT50))   stickX += -THRESOLD / 2;
	if (key(VKEY_FOR_XLEFT100))  stickX += -THRESOLD;

	stickY += my_max(pad_gc_stick(&gp, conf->gckeys[VKEY_FOR_XUP50]), pad_gc_stick(&gp, conf->gckeys[VKEY_FOR_XUP100]));
	stickY -= my_max(pad_gc_stick(&gp, conf->gckeys[VKEY_FOR_XDOWN50]), pad_gc_stick(&gp, conf->gckeys[VKEY_FOR_XDOWN100]));
	stickX += my_max(pad_gc_stick(&gp, conf->gckeys[VKEY_FOR_XRIGHT50]), pad_gc_stick(&gp, conf->gckeys[VKEY_FOR_XRIGHT100]));
	stickX -= my_max(pad_gc_stick(&gp, conf->gckeys[VKEY_FOR_XLEFT50]), pad_gc_stick(&gp, conf->gckeys[VKEY_FOR_XLEFT100]));

	// The C stick

	if (key(VKEY_FOR_CXUP))    substickY += THRESOLD;
	if (key(VKEY_FOR_CXDOWN))  substickY += -THRESOLD;
	if (key(VKEY_FOR_CXRIGHT)) substickX += THRESOLD;
	if (key(VKEY_FOR_CXLEFT))  substickX += -THRESOLD;

	substickY += pad_gc_stick(&gp, conf->gckeys[VKEY_FOR_CXUP]);
	substickY -= pad_gc_stick(&gp, conf->gckeys[VKEY_FOR_CXDOWN]);
	substickX += pad_gc_stick(&gp, conf->gckeys[VKEY_FOR_CXRIGHT]);
	substickX -= pad_gc_stick(&gp, conf->gckeys[VKEY_FOR_CXLEFT]);

	state->stickX = pad_stick_value(stickX);
	state->stickY = pad_stick_value(stickY);
	state->substickX = pad_stick_value(substickX);
	state->substickY = pad_stick_value(substickY);
	state->triggerLeft = (uint8_t)triggerLeft;
	state->triggerRight = (uint8_t)triggerRight;

	state->button = button;

	return true;
}

// controller motor. 0 returned, if rumble is not supported by PAD.
// see one of PAD_MOTOR* for allowed commands.
bool PADSetRumble(long padnum, long cmd)
{
	return false;
}
