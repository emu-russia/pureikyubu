
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

void PADConfigure(long padnum, HWND hwndParent);
