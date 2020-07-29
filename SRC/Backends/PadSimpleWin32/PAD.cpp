// PAD API for emulator
#include "pch.h"

static PADCONF pad[4];

void PADLoadConfig(int padToConfigure)
{
    char parm[256] = { 0 };

    // Plugged or not
    sprintf_s(parm, sizeof(parm), "PluggedIn""_%i", padToConfigure);
    pad[padToConfigure].plugged = GetConfigBool(parm, USER_PADS);

    //
    // Buttons 8|
    //

    sprintf_s(parm, sizeof(parm), "VKEY_FOR_UP""_%i", padToConfigure);
    pad[padToConfigure].vkeys[VKEY_FOR_UP] = GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_DOWN""_%i", padToConfigure);
    pad[padToConfigure].vkeys[VKEY_FOR_DOWN] = GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_LEFT""_%i", padToConfigure);
    pad[padToConfigure].vkeys[VKEY_FOR_LEFT] = GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_RIGHT""_%i", padToConfigure);
    pad[padToConfigure].vkeys[VKEY_FOR_RIGHT] = GetConfigInt(parm, USER_PADS);

    sprintf_s(parm, sizeof(parm), "VKEY_FOR_XUP50""_%i", padToConfigure);
    pad[padToConfigure].vkeys[VKEY_FOR_XUP50] = GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_XUP100""_%i", padToConfigure);
    pad[padToConfigure].vkeys[VKEY_FOR_XUP100] = GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_XDOWN50""_%i", padToConfigure);
    pad[padToConfigure].vkeys[VKEY_FOR_XDOWN50] = GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_XDOWN100""_%i", padToConfigure);
    pad[padToConfigure].vkeys[VKEY_FOR_XDOWN100] = GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_XLEFT50""_%i", padToConfigure);
    pad[padToConfigure].vkeys[VKEY_FOR_XLEFT50] = GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_XLEFT100""_%i", padToConfigure);
    pad[padToConfigure].vkeys[VKEY_FOR_XLEFT100] = GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_XRIGHT50""_%i", padToConfigure);
    pad[padToConfigure].vkeys[VKEY_FOR_XRIGHT50] = GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_XRIGHT100""_%i", padToConfigure);
    pad[padToConfigure].vkeys[VKEY_FOR_XRIGHT100] = GetConfigInt(parm, USER_PADS);

    sprintf_s(parm, sizeof(parm), "VKEY_FOR_CXUP""_%i", padToConfigure);
    pad[padToConfigure].vkeys[VKEY_FOR_CXUP] = GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_CXDOWN""_%i", padToConfigure);
    pad[padToConfigure].vkeys[VKEY_FOR_CXDOWN] = GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_CXLEFT""_%i", padToConfigure);
    pad[padToConfigure].vkeys[VKEY_FOR_CXLEFT] = GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_CXRIGHT""_%i", padToConfigure);
    pad[padToConfigure].vkeys[VKEY_FOR_CXRIGHT] = GetConfigInt(parm, USER_PADS);

    sprintf_s(parm, sizeof(parm), "VKEY_FOR_TRIGGERL""_%i", padToConfigure);
    pad[padToConfigure].vkeys[VKEY_FOR_TRIGGERL] = GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_TRIGGERR""_%i", padToConfigure);
    pad[padToConfigure].vkeys[VKEY_FOR_TRIGGERR] = GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_TRIGGERZ""_%i", padToConfigure);
    pad[padToConfigure].vkeys[VKEY_FOR_TRIGGERZ] = GetConfigInt(parm, USER_PADS);

    sprintf_s(parm, sizeof(parm), "VKEY_FOR_A""_%i", padToConfigure);
    pad[padToConfigure].vkeys[VKEY_FOR_A] = GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_B""_%i", padToConfigure);
    pad[padToConfigure].vkeys[VKEY_FOR_B] = GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_X""_%i", padToConfigure);
    pad[padToConfigure].vkeys[VKEY_FOR_X] = GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_Y""_%i", padToConfigure);
    pad[padToConfigure].vkeys[VKEY_FOR_Y] = GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_START""_%i", padToConfigure);
    pad[padToConfigure].vkeys[VKEY_FOR_START] = GetConfigInt(parm, USER_PADS);
}

// ---------------------------------------------------------------------------
// called when emulation started/stopped (pad controls)

bool PADOpen()
{
    PADLoadConfig(0);
    PADLoadConfig(1);
    PADLoadConfig(2);
    PADLoadConfig(3);

    return true;    // ok
}

void PADClose()
{
    pad[0].plugged = 
    pad[1].plugged = 
    pad[2].plugged = 
    pad[3].plugged = 0;
}

// ---------------------------------------------------------------------------
// process input

#define THRESOLD    87

static void pad_reset_chan(PADState *state)
{
    memset(state, 0, sizeof(PADState));

    state->stickX = state->stickY = -128;
    state->substickX = state->substickY = -128;
}

// collect keyboard buttons in PADState
long PADReadButtons(long padnum, PADState *state)
{
    uint16_t button = 0;

    pad_reset_chan(state);

    if(!pad[padnum].plugged) return 0;

    if(padnum < 4)
    {
        //
        // ===== PAD n =====
        //

        if(pad[padnum].vkeys[VKEY_FOR_UP] != -1)
            if(GetAsyncKeyState(pad[padnum].vkeys[VKEY_FOR_UP]) & 0x80000000) button |= PAD_BUTTON_UP;
        if(pad[padnum].vkeys[VKEY_FOR_DOWN] != -1)
            if(GetAsyncKeyState(pad[padnum].vkeys[VKEY_FOR_DOWN]) & 0x80000000) button |= PAD_BUTTON_DOWN;
        if(pad[padnum].vkeys[VKEY_FOR_LEFT] != -1)
            if(GetAsyncKeyState(pad[padnum].vkeys[VKEY_FOR_LEFT]) & 0x80000000) button |= PAD_BUTTON_LEFT;
        if(pad[padnum].vkeys[VKEY_FOR_RIGHT] != -1)
            if(GetAsyncKeyState(pad[padnum].vkeys[VKEY_FOR_RIGHT]) & 0x80000000) button |= PAD_BUTTON_RIGHT;

        if(pad[padnum].vkeys[VKEY_FOR_A] != -1)
            if(GetAsyncKeyState(pad[padnum].vkeys[VKEY_FOR_A]) & 0x80000000) button |= PAD_BUTTON_A;
        if(pad[padnum].vkeys[VKEY_FOR_B] != -1)
            if(GetAsyncKeyState(pad[padnum].vkeys[VKEY_FOR_B]) & 0x80000000) button |= PAD_BUTTON_B;
        if(pad[padnum].vkeys[VKEY_FOR_X] != -1)
            if(GetAsyncKeyState(pad[padnum].vkeys[VKEY_FOR_X]) & 0x80000000) button |= PAD_BUTTON_X;
        if(pad[padnum].vkeys[VKEY_FOR_Y] != -1)
            if(GetAsyncKeyState(pad[padnum].vkeys[VKEY_FOR_Y]) & 0x80000000) button |= PAD_BUTTON_Y;
        if(pad[padnum].vkeys[VKEY_FOR_START] != -1)
            if(GetAsyncKeyState(pad[padnum].vkeys[VKEY_FOR_START]) & 0x80000000) button |= PAD_BUTTON_START;

        // Note : digital L and R are only set when its analog key is pressed all the way down;
        // this plugin is only supporting the fact, that L/R are pressed.

        if(pad[padnum].vkeys[VKEY_FOR_TRIGGERL] != -1)
        {
            if(GetAsyncKeyState(pad[padnum].vkeys[VKEY_FOR_TRIGGERL]) & 0x80000000)
            {
                button |= PAD_TRIGGER_L;
                state->triggerLeft = 255;
            }
        }
        if(pad[padnum].vkeys[VKEY_FOR_TRIGGERR] != -1)
        {
            if(GetAsyncKeyState(pad[padnum].vkeys[VKEY_FOR_TRIGGERR]) & 0x80000000)
            {
                button |= PAD_TRIGGER_R;
                state->triggerRight = 255;
            }
        }

        if(pad[padnum].vkeys[VKEY_FOR_TRIGGERZ] != -1)
        {
            if(GetAsyncKeyState(pad[padnum].vkeys[VKEY_FOR_TRIGGERZ]) & 0x80000000)
            {
                button |= PAD_TRIGGER_Z;
            }
        }

        if(pad[padnum].vkeys[VKEY_FOR_XUP50] != -1)
            if(GetAsyncKeyState(pad[padnum].vkeys[VKEY_FOR_XUP50]) & 0x80000000) state->stickY += THRESOLD / 2;
        if(pad[padnum].vkeys[VKEY_FOR_XUP100] != -1)
            if(GetAsyncKeyState(pad[padnum].vkeys[VKEY_FOR_XUP100]) & 0x80000000) state->stickY += THRESOLD;
        if(pad[padnum].vkeys[VKEY_FOR_XDOWN50] != -1)
            if(GetAsyncKeyState(pad[padnum].vkeys[VKEY_FOR_XDOWN50]) & 0x80000000) state->stickY += -THRESOLD / 2;
        if(pad[padnum].vkeys[VKEY_FOR_XDOWN100] != -1)
            if(GetAsyncKeyState(pad[padnum].vkeys[VKEY_FOR_XDOWN100]) & 0x80000000) state->stickY += -THRESOLD;
        if(pad[padnum].vkeys[VKEY_FOR_XRIGHT50] != -1)
            if(GetAsyncKeyState(pad[padnum].vkeys[VKEY_FOR_XRIGHT50]) & 0x80000000) state->stickX += THRESOLD / 2;
        if(pad[padnum].vkeys[VKEY_FOR_XRIGHT100] != -1)
            if(GetAsyncKeyState(pad[padnum].vkeys[VKEY_FOR_XRIGHT100]) & 0x80000000) state->stickX += THRESOLD;
        if(pad[padnum].vkeys[VKEY_FOR_XLEFT50] != -1)
            if(GetAsyncKeyState(pad[padnum].vkeys[VKEY_FOR_XLEFT50]) & 0x80000000) state->stickX += -THRESOLD / 2;
        if(pad[padnum].vkeys[VKEY_FOR_XLEFT100] != -1)
            if(GetAsyncKeyState(pad[padnum].vkeys[VKEY_FOR_XLEFT100]) & 0x80000000) state->stickX += -THRESOLD;

        if(pad[padnum].vkeys[VKEY_FOR_CXUP] != -1)
            if(GetAsyncKeyState(pad[padnum].vkeys[VKEY_FOR_CXUP]) & 0x80000000) state->substickY += THRESOLD;
        if(pad[padnum].vkeys[VKEY_FOR_CXDOWN] != -1)
            if(GetAsyncKeyState(pad[padnum].vkeys[VKEY_FOR_CXDOWN]) & 0x80000000) state->substickY += -THRESOLD;
        if(pad[padnum].vkeys[VKEY_FOR_CXRIGHT] != -1)
            if(GetAsyncKeyState(pad[padnum].vkeys[VKEY_FOR_CXRIGHT]) & 0x80000000) state->substickX += THRESOLD;
        if(pad[padnum].vkeys[VKEY_FOR_CXLEFT] != -1)
            if(GetAsyncKeyState(pad[padnum].vkeys[VKEY_FOR_CXLEFT]) & 0x80000000) state->substickX += -THRESOLD;
    } else return 0;

    state->button = button;

    return 1;
}

// controller motor. 0 returned, if rumble is not supported by PAD.
// see one of PAD_MOTOR* for allowed commands.
long PADSetRumble(long padnum, long cmd)
{
    return false;
}
