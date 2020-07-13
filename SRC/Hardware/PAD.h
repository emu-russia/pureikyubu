// PAD (input) interface
// (padnum = 0...3)

#pragma once

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
bool PADOpen(HWND hwnd);
void PADClose();

// read controller buttons state. returns 1, if ok, and 0, if PAD not connected
long PADReadButtons(long padnum, PADState *state);

// controller motor. 0 returned, if rumble is not supported by PAD.
// see one of PAD_MOTOR* for allowed commands.
long PADSetRumble(long padnum, long cmd);

// config / about
void PADConfigure(long padnum, HWND hwndParent);
