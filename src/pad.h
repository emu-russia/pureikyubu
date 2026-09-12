
#pragma once

// The index of a GameCube controller control. Every pad has two bindings per control: a keyboard
// key (vkeys) and a game controller button or axis (gckeys, the SDL build only).
enum
{
	VKEY_FOR_UP = 0,
	VKEY_FOR_DOWN,
	VKEY_FOR_LEFT,
	VKEY_FOR_RIGHT,
	VKEY_FOR_XUP50,
	VKEY_FOR_XUP100,
	VKEY_FOR_XDOWN50,
	VKEY_FOR_XDOWN100,
	VKEY_FOR_XLEFT50,
	VKEY_FOR_XLEFT100,
	VKEY_FOR_XRIGHT50,
	VKEY_FOR_XRIGHT100,
	VKEY_FOR_CXUP,
	VKEY_FOR_CXDOWN,
	VKEY_FOR_CXLEFT,
	VKEY_FOR_CXRIGHT,
	VKEY_FOR_TRIGGERL,
	VKEY_FOR_TRIGGERR,
	VKEY_FOR_TRIGGERZ,
	VKEY_FOR_A,
	VKEY_FOR_B,
	VKEY_FOR_X,
	VKEY_FOR_Y,
	VKEY_FOR_START,

	VKEY_FOR_MAX
};

struct PADCONF
{
	bool    plugged;
	int     vkeys[VKEY_FOR_MAX];    // keyboard binding (-1 or 0 - undefined)
	int     gckeys[VKEY_FOR_MAX];   // game controller binding (SDL build only), see PAD_GCKEY_*
};

// The game controller binding of a control is stored as a single integer, so that it fits the
// existing configuration variables (GCKEY_FOR_*_N in the "controllers" section). It is either an
// SDL game controller button or an SDL game controller axis with the direction that drives
// the control.
#define PAD_GCKEY_BUTTON_BASE   0x00010000
#define PAD_GCKEY_AXIS_BASE     0x00020000

#define PAD_GCKEY_IS_BUTTON(v)  ((v) >= PAD_GCKEY_BUTTON_BASE && (v) < PAD_GCKEY_AXIS_BASE)
#define PAD_GCKEY_IS_AXIS(v)    ((v) >= PAD_GCKEY_AXIS_BASE)

#define PAD_GCKEY_BUTTON(v)     ((v) - PAD_GCKEY_BUTTON_BASE)
#define PAD_GCKEY_AXIS(v)       (((v) - PAD_GCKEY_AXIS_BASE) >> 1)
#define PAD_GCKEY_AXIS_POS(v)   (((v) & 1) != 0)        // 1: positive direction, 0: negative

#define PAD_GCKEY_MAKE_BUTTON(b)     (PAD_GCKEY_BUTTON_BASE + (b))
#define PAD_GCKEY_MAKE_AXIS(a, pos)  (PAD_GCKEY_AXIS_BASE + ((a) << 1) + ((pos) ? 1 : 0))

// PAD (input) interface
// (padnum = 0...3)

struct PADState
{
	uint16_t    button;         // combination of PAD_BUTTON*
	int8_t      stickX;         // -127...127
	int8_t      stickY;
	int8_t      substickX;      // -127...127
	int8_t      substickY;
	uint8_t     triggerLeft;    // 0...255
	uint8_t     triggerRight;
};

// controller buttons
#define PAD_BUTTON_LEFT         (0x0001)
#define PAD_BUTTON_RIGHT        (0x0002)
#define PAD_BUTTON_DOWN         (0x0004)
#define PAD_BUTTON_UP           (0x0008)
#define PAD_TRIGGER_Z           (0x0010)
#define PAD_TRIGGER_R           (0x0020)
#define PAD_TRIGGER_L           (0x0040)
#define PAD_BUTTON_A            (0x0100)
#define PAD_BUTTON_B            (0x0200)
#define PAD_BUTTON_X            (0x0400)
#define PAD_BUTTON_Y            (0x0800)
#define PAD_BUTTON_START        (0x1000)

// controller motor commands
#define PAD_MOTOR_STOP          0
#define PAD_MOTOR_RUMBLE        1
#define PAD_MOTOR_STOP_HARD     2

// PADOpen() should be called before emulation started, to initialize
// plugin. PADClose() is called, when emulation is stopped, to shutdown plugin.
bool PADOpen();
void PADClose();

// (Re)read the bindings of the specified pad from the configuration. Called by PADOpen(), and by
// the controller settings dialog, so that the new bindings are applied without restarting the emulation.
void PADLoadConfig(int padToConfigure);

// SDL build only. Keep the SDL game controllers of the ports open and refresh their cached state.
// Must be called from the thread that pumps the SDL events (the UI thread), see padsdl.cpp.
void PADUpdateControllers();

// read controller buttons state. returns 1, if ok, and 0, if PAD not connected
bool PADReadButtons(long padnum, PADState* state);

// controller motor. 0 returned, if rumble is not supported by PAD.
// see one of PAD_MOTOR* for allowed commands.
bool PADSetRumble(long padnum, long cmd);
