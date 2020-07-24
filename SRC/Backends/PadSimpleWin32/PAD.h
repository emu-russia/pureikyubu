
#pragma once

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

typedef struct
{
    bool    plugged;
    int     vkeys[VKEY_FOR_MAX];    // -1 - undefined
} PADCONF;

// all important data is placed here
typedef struct
{
    int         padToConfigure;
    PADCONF     config[4];
} PAD;

// PAD (input) interface
// (padnum = 0...3)

struct PADState
{
    unsigned short  button;         // combination of PAD_BUTTON*
    signed char     stickX;         // -127...127
    signed char     stickY;
    signed char     substickX;      // -127...127
    signed char     substickY;
    unsigned char   triggerLeft;    // 0...255
    unsigned char   triggerRight;
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

// read controller buttons state. returns 1, if ok, and 0, if PAD not connected
long PADReadButtons(long padnum, PADState* state);

// controller motor. 0 returned, if rumble is not supported by PAD.
// see one of PAD_MOTOR* for allowed commands.
long PADSetRumble(long padnum, long cmd);
