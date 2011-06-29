// Stupid PAD lib HLE
#include "dolphin.h"

#define PARAM(n)    GPR[3+n]
#define RET_VAL     GPR[3]
#define SWAP        MEMSwap

// u32 PADRead(PADStatus * status)
void _PADRead()
{
    HLEHit(HLE_PAD_READ);

    PADStatus * status = (PADStatus *)(&RAM[PARAM(0) & RAMMASK]);
    BOOL present;
    u32 retVal = 0, rumbleFlags[] = { 1<<31, 1<<30, 1<<29, 1<<28 };

    // read PAD state
    for(int i=0; i<4; i++)
    {
        PADState pad;  present = PADReadButtons(i, &pad);
        if(present)
        {
            status[i].err = 0;
            status[i].analogA = status[i].analogB = 0;
            status[i].button = MEMSwapHalf(pad.button);
            status[i].stickX = pad.stickX;
            status[i].stickY = pad.stickY;
            status[i].substickX = pad.substickX;
            status[i].substickY = pad.substickY;
            status[i].triggerLeft = pad.triggerLeft;
            status[i].triggerRight = pad.triggerRight;
        }
        else status[i].err = -1;

        retVal |= rumbleFlags[si.rumble[i]];
    }

    RET_VAL = retVal;
}

// init HLE driver
void PADOpenHLE()
{
    BOOL flag = GetConfigInt(USER_HLE_PAD, USER_HLE_PAD_DEFAULT);
    if(flag)
    {
        DBReport(GREEN "Highlevel PAD driver install..\n");

        HLESetCall("PADRead", _PADRead);

        DBReport("\n");
    }
}
