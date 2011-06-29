// PAD plugin configure and about dialogs
#include "PAD.h"

static char *vkeys[256] = { // default keyboard virtual codes description (? - not used)
 "?", "?", "?", "?", "?", "?", "?", "?",  "Bkspace", "Tab", "?", "?", "?", "Enter", "?", "?", // 00-0F
 "Shift", "Control", "Alt", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", // 10-1F
 "Space","PgUp","PgDown","End","Home","Left","Up","Right", "Down", "?","?","?","?","Ins","Del","?", // 20-2F
 "0", "1", "2", "3", "4", "5", "6", "7",  "8", "9", "?", "?", "?", "?", "?", "?", // 30-3F
 "", "A", "B", "C", "D", "E", "F", "G", "H",  "I", "J", "K", "L", "M", "N", "O", // 40-4F
 "P", "Q", "R", "S", "T", "U", "V", "W",  "X", "Y", "Z", "?", "?", "?", "?", "?", // 50-5F
 "Num 0","Num 1","Num 2","Num 3","Num 4","Num 5","Num 6","Num 7",
 "Num 8","Num 9","Mult","Plus","Bkslash","Minus","Decimal","Slash", // 60-6F
 "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "?", "?", "?", "?", // 70-7F
};

static char *GetVKDesc(int vkey)
{
    if(vkey >= 0x80) return "?";
    else return vkeys[vkey];
}

void PADLoadConfig(HWND hwndDlg)
{
    char parm[256];
    int vkey;

    // Plugged or not
    sprintf(parm, "PluggedIn""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].plugged = GetConfigInt(parm, 0);

    //
    // Buttons 8|
    //

    sprintf(parm, "VKEY_FOR_UP""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_UP] = GetConfigInt(parm, -1);
    sprintf(parm, "VKEY_FOR_DOWN""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_DOWN] = GetConfigInt(parm, -1);
    sprintf(parm, "VKEY_FOR_LEFT""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_LEFT] = GetConfigInt(parm, -1);
    sprintf(parm, "VKEY_FOR_RIGHT""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_RIGHT] = GetConfigInt(parm, -1);

    sprintf(parm, "VKEY_FOR_XUP50""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP50] = GetConfigInt(parm, -1);
    sprintf(parm, "VKEY_FOR_XUP100""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP100] = GetConfigInt(parm, -1);
    sprintf(parm, "VKEY_FOR_XDOWN50""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN50] = GetConfigInt(parm, -1);
    sprintf(parm, "VKEY_FOR_XDOWN100""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN100] = GetConfigInt(parm, -1);
    sprintf(parm, "VKEY_FOR_XLEFT50""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT50] = GetConfigInt(parm, -1);
    sprintf(parm, "VKEY_FOR_XLEFT100""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT100] = GetConfigInt(parm, -1);
    sprintf(parm, "VKEY_FOR_XRIGHT50""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT50] = GetConfigInt(parm, -1);
    sprintf(parm, "VKEY_FOR_XRIGHT100""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT100] = GetConfigInt(parm, -1);

    sprintf(parm, "VKEY_FOR_CXUP""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXUP] = GetConfigInt(parm, -1);
    sprintf(parm, "VKEY_FOR_CXDOWN""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXDOWN] = GetConfigInt(parm, -1);
    sprintf(parm, "VKEY_FOR_CXLEFT""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXLEFT] = GetConfigInt(parm, -1);
    sprintf(parm, "VKEY_FOR_CXRIGHT""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXRIGHT] = GetConfigInt(parm, -1);

    sprintf(parm, "VKEY_FOR_TRIGGERL""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERL] = GetConfigInt(parm, -1);
    sprintf(parm, "VKEY_FOR_TRIGGERR""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERR] = GetConfigInt(parm, -1);
    sprintf(parm, "VKEY_FOR_TRIGGERZ""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERZ] = GetConfigInt(parm, -1);

    sprintf(parm, "VKEY_FOR_A""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_A] = GetConfigInt(parm, -1);
    sprintf(parm, "VKEY_FOR_B""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_B] = GetConfigInt(parm, -1);
    sprintf(parm, "VKEY_FOR_X""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_X] = GetConfigInt(parm, -1);
    sprintf(parm, "VKEY_FOR_Y""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_Y] = GetConfigInt(parm, -1);
    sprintf(parm, "VKEY_FOR_START""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_START] = GetConfigInt(parm, -1);

    //
    // Enable buttons
    //

    if(hwndDlg)
    {
        EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_UP), TRUE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_DOWN), TRUE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_LEFT), TRUE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_RIGHT), TRUE);

        EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XUP50), TRUE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XUP100), TRUE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XDOWN50), TRUE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XDOWN100), TRUE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XLEFT50), TRUE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XLEFT100), TRUE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XRIGHT50), TRUE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XRIGHT100), TRUE);

        EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXUP), TRUE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXDOWN), TRUE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXLEFT), TRUE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXRIGHT), TRUE);

        EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_TRIGGERL), TRUE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_TRIGGERR), TRUE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_TRIGGERZ), TRUE);

        EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_A), TRUE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_B), TRUE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_X), TRUE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_Y), TRUE);
        EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_START), TRUE);
    }

    //
    // Set buttons
    //

    if(hwndDlg)
    {
        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_UP];
        if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_UP, "...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_UP, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_DOWN];
        if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_DOWN, "...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_DOWN, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_RIGHT];
        if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_RIGHT, "...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_RIGHT, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_LEFT];
        if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_LEFT, "...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_LEFT, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP50];
        if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_XUP50, "...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_XUP50, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP100];
        if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_XUP100, "...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_XUP100, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN50];
        if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN50, "...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN50, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN100];
        if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN100, "...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN100, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT50];
        if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT50, "...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT50, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT100];
        if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT100, "...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT100, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT50];
        if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT50, "...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT50, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT100];
        if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT100, "...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT100, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXUP];
        if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_CXUP, "...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_CXUP, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXDOWN];
        if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_CXDOWN, "...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_CXDOWN, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXRIGHT];
        if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_CXRIGHT, "...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_CXRIGHT, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXLEFT];
        if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_CXLEFT, "...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_CXLEFT, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERL];
        if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERL, "...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERL, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERR];
        if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERR, "...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERR, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERZ];
        if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERZ, "...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERZ, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_A];
        if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_A, "...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_A, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_B];
        if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_B, "...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_B, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_X];
        if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_X, "...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_X, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_Y];
        if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_Y, "...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_Y, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_START];
        if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_START, "...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_START, GetVKDesc(vkey));
    }

    if(pad.config[pad.padToConfigure].plugged == 0)
    {
        CheckDlgButton(hwndDlg, IDC_CHECK_PLUG, BST_UNCHECKED);

        //
        // Disable buttons
        //

        if(hwndDlg)
        {
            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_UP), FALSE);
            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_DOWN), FALSE);
            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_LEFT), FALSE);
            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_RIGHT), FALSE);

            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XUP50), FALSE);
            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XUP100), FALSE);
            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XDOWN50), FALSE);
            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XDOWN100), FALSE);
            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XLEFT50), FALSE);
            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XLEFT100), FALSE);
            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XRIGHT50), FALSE);
            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XRIGHT100), FALSE);

            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXUP), FALSE);
            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXDOWN), FALSE);
            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXLEFT), FALSE);
            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXRIGHT), FALSE);

            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_TRIGGERL), FALSE);
            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_TRIGGERR), FALSE);
            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_TRIGGERZ), FALSE);

            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_A), FALSE);
            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_B), FALSE);
            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_X), FALSE);
            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_Y), FALSE);
            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_START), FALSE);
        }

        return;
    }
    else CheckDlgButton(hwndDlg, IDC_CHECK_PLUG, BST_CHECKED);
}

void PADSaveConfig(HWND hwndDlg)
{
    char parm[256];

    // Plugged or not
    sprintf(parm, "PluggedIn""_%i", pad.padToConfigure);
    if(pad.config[pad.padToConfigure].plugged) SetConfigInt(parm, 1);
    else SetConfigInt(parm, 0);

    //
    // Buttons
    //

    sprintf(parm, "VKEY_FOR_UP""_%i", pad.padToConfigure);
    SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_UP]);
    sprintf(parm, "VKEY_FOR_DOWN""_%i", pad.padToConfigure);
    SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_DOWN]);
    sprintf(parm, "VKEY_FOR_LEFT""_%i", pad.padToConfigure);
    SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_LEFT]);
    sprintf(parm, "VKEY_FOR_RIGHT""_%i", pad.padToConfigure);
    SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_RIGHT]);

    sprintf(parm, "VKEY_FOR_XUP50""_%i", pad.padToConfigure);
    SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP50]);
    sprintf(parm, "VKEY_FOR_XUP100""_%i", pad.padToConfigure);
    SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP100]);
    sprintf(parm, "VKEY_FOR_XDOWN50""_%i", pad.padToConfigure);
    SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN50]);
    sprintf(parm, "VKEY_FOR_XDOWN100""_%i", pad.padToConfigure);
    SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN100]);
    sprintf(parm, "VKEY_FOR_XLEFT50""_%i", pad.padToConfigure);
    SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT50]);
    sprintf(parm, "VKEY_FOR_XLEFT100""_%i", pad.padToConfigure);
    SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT100]);
    sprintf(parm, "VKEY_FOR_XRIGHT50""_%i", pad.padToConfigure);
    SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT50]);
    sprintf(parm, "VKEY_FOR_XRIGHT100""_%i", pad.padToConfigure);
    SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT100]);

    sprintf(parm, "VKEY_FOR_CXUP""_%i", pad.padToConfigure);
    SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXUP]);
    sprintf(parm, "VKEY_FOR_CXDOWN""_%i", pad.padToConfigure);
    SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXDOWN]);
    sprintf(parm, "VKEY_FOR_CXLEFT""_%i", pad.padToConfigure);
    SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXLEFT]);
    sprintf(parm, "VKEY_FOR_CXRIGHT""_%i", pad.padToConfigure);
    SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXRIGHT]);

    sprintf(parm, "VKEY_FOR_TRIGGERL""_%i", pad.padToConfigure);
    SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERL]);
    sprintf(parm, "VKEY_FOR_TRIGGERR""_%i", pad.padToConfigure);
    SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERR]);
    sprintf(parm, "VKEY_FOR_TRIGGERZ""_%i", pad.padToConfigure);
    SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERZ]);

    sprintf(parm, "VKEY_FOR_A""_%i", pad.padToConfigure);
    SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_A]);
    sprintf(parm, "VKEY_FOR_B""_%i", pad.padToConfigure);
    SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_B]);
    sprintf(parm, "VKEY_FOR_X""_%i", pad.padToConfigure);
    SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_X]);
    sprintf(parm, "VKEY_FOR_Y""_%i", pad.padToConfigure);
    SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_Y]);
    sprintf(parm, "VKEY_FOR_START""_%i", pad.padToConfigure);
    SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_START]);
}

// reset all butons and unplug pad
void PADClearConfig(HWND hwndDlg)
{
    int i;

    pad.config[pad.padToConfigure].plugged = 0;

    for(i=0; i<VKEY_FOR_MAX; i++)
    {
        pad.config[pad.padToConfigure].vkeys[i] = -1;
    }

    // reload
    PADSaveConfig(hwndDlg);
    PADLoadConfig(hwndDlg);
}

// set default buttons (only first pad supported)
void PADDefaultConfig(HWND hwndDlg)
{
    switch(pad.padToConfigure)
    {
        case 0:
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_UP] = 0x24;        // Home
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_DOWN] = 0x23;      // End
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_LEFT] = 0x2e;      // Del
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_RIGHT] = 0x22;     // PgDn
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP50] = -1;
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP100] = 0x26;    // Up
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN50] = -1;
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN100] = 0x28;  // Down
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT50] = -1;
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT100] = 0x25;  // Left
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT50] = -1;
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT100] = 0x27; // Right
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXUP] = 0x68;      // Num 8
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXDOWN] = 0x62;    // Num 2
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXLEFT] = 0x64;    // Num 4
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXRIGHT] = 0x66;   // Num 6
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERL] = 0x51;  // Q
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERR] = 0x57;  // W
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERZ] = 0x45;  // E
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_A] = 0x58;         // X
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_B] = 0x5a;         // Z
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_X] = 0x53;         // S
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_Y] = 0x41;         // A
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_START] = 0xd;      // Enter
            break;
    }

    // reload
    PADSaveConfig(hwndDlg);
    PADLoadConfig(hwndDlg);
}

// ---------------------------------------------------------------------------

int GetVKey()
{
    int i;

    while(1)
    {
        for(i=0; i<0x80; i++)
        {
            if(GetAsyncKeyState(i) & 0x80000000)
            {
                if(i == VK_SHIFT || i == VK_CONTROL || i == VK_MENU) continue;  // rhyme :)
                if(i >= VK_F1 && i <= VK_F24) continue;
                if(i == VK_ESCAPE) return -1;
                else return i;
            }
        }
    }
}

BOOL CALLBACK PADConfigDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    char buf[256];
    int vkey;

    switch(uMsg)
    {
        case WM_INITDIALOG:
            sprintf(buf, "Configure Controller %i", pad.padToConfigure + 1);
            SetWindowText(hwndDlg, buf);
            if(pad.padToConfigure != 0)
            {
                EnableWindow(GetDlgItem(hwndDlg, IDC_PAD_CONFIG_DEFAULT), FALSE);
            }
            PADLoadConfig(hwndDlg);
            return TRUE;

        case WM_CLOSE:
            EndDialog(hwndDlg, 0);
            return TRUE;

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_BUTTON_UP:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_UP, "?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_UP] = vkey;
                    if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_UP, "...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_UP, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_DOWN:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_DOWN, "?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_DOWN] = vkey;
                    if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_DOWN, "...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_DOWN, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_LEFT:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_LEFT, "?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_LEFT] = vkey;
                    if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_LEFT, "...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_LEFT, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_RIGHT:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_RIGHT, "?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_RIGHT] = vkey;
                    if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_RIGHT, "...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_RIGHT, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_XUP50:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_XUP50, "?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP50] = vkey;
                    if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_XUP50, "...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_XUP50, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_XUP100:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_XUP100, "?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP100] = vkey;
                    if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_XUP100, "...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_XUP100, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_XDOWN50:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN50, "?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN50] = vkey;
                    if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN50, "...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN50, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_XDOWN100:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN100, "?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN100] = vkey;
                    if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN100, "...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN100, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_XLEFT50:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT50, "?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT50] = vkey;
                    if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT50, "...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT50, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_XLEFT100:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT100, "?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT100] = vkey;
                    if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT100, "...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT100, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_XRIGHT50:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT50, "?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT50] = vkey;
                    if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT50, "...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT50, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_XRIGHT100:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT100, "?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT100] = vkey;
                    if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT100, "...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT100, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_CXUP:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_CXUP, "?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXUP] = vkey;
                    if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_CXUP, "...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_CXUP, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_CXDOWN:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_CXDOWN, "?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXDOWN] = vkey;
                    if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_CXDOWN, "...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_CXDOWN, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_CXLEFT:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_CXLEFT, "?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXLEFT] = vkey;
                    if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_CXLEFT, "...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_CXLEFT, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_CXRIGHT:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_CXRIGHT, "?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXRIGHT] = vkey;
                    if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_CXRIGHT, "...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_CXRIGHT, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_TRIGGERL:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERL, "?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERL] = vkey;
                    if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERL, "...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERL, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_TRIGGERR:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERR, "?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERR] = vkey;
                    if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERR, "...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERR, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_TRIGGERZ:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERZ, "?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERZ] = vkey;
                    if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERZ, "...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERZ, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_A:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_A, "?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_A] = vkey;
                    if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_A, "...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_A, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_B:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_B, "?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_B] = vkey;
                    if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_B, "...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_B, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_X:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_X, "?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_X] = vkey;
                    if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_X, "...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_X, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_Y:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_Y, "?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_Y] = vkey;
                    if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_Y, "...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_Y, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_START:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_START, "?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_START] = vkey;
                    if(vkey == -1) SetDlgItemText(hwndDlg, IDC_BUTTON_START, "...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_START, GetVKDesc(vkey));
                    return FALSE;

                case IDC_CHECK_PLUG:
                        if(IsDlgButtonChecked(hwndDlg, IDC_CHECK_PLUG))
                        {
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_UP), TRUE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_DOWN), TRUE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_LEFT), TRUE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_RIGHT), TRUE);

                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XUP50), TRUE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XUP100), TRUE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XDOWN50), TRUE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XDOWN100), TRUE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XLEFT50), TRUE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XLEFT100), TRUE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XRIGHT50), TRUE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XRIGHT100), TRUE);

                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXUP), TRUE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXDOWN), TRUE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXLEFT), TRUE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXRIGHT), TRUE);

                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_TRIGGERL), TRUE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_TRIGGERR), TRUE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_TRIGGERZ), TRUE);

                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_A), TRUE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_B), TRUE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_X), TRUE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_Y), TRUE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_START), TRUE);

                            pad.config[pad.padToConfigure].plugged = 1;
                        }
                        else
                        {
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_UP), FALSE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_DOWN), FALSE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_LEFT), FALSE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_RIGHT), FALSE);

                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XUP50), FALSE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XUP100), FALSE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XDOWN50), FALSE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XDOWN100), FALSE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XLEFT50), FALSE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XLEFT100), FALSE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XRIGHT50), FALSE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_XRIGHT100), FALSE);

                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXUP), FALSE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXDOWN), FALSE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXLEFT), FALSE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_CXRIGHT), FALSE);

                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_TRIGGERL), FALSE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_TRIGGERR), FALSE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_TRIGGERZ), FALSE);

                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_A), FALSE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_B), FALSE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_X), FALSE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_Y), FALSE);
                            EnableWindow(GetDlgItem(hwndDlg, IDC_BUTTON_START), FALSE);

                            pad.config[pad.padToConfigure].plugged = 0;
                        }
                    return FALSE;

                case IDC_PAD_CONFIG_CANCEL:
                    EndDialog(hwndDlg, 0);
                    return TRUE;

                case IDC_PAD_CONFIG_OK:
                    PADSaveConfig(hwndDlg);
                    EndDialog(hwndDlg, 0);
                    return TRUE;

                case IDC_PAD_CONFIG_CLEAR:
                    PADClearConfig(hwndDlg);
                    return FALSE;

                case IDC_PAD_CONFIG_DEFAULT:
                    PADDefaultConfig(hwndDlg);
                    return FALSE;
            }
            break;

        default:
            return FALSE;
    }
    return FALSE;
}

void PADConfigure(long padnum)
{
    pad.padToConfigure = padnum;

    DialogBox(
        pad.inst,
        MAKEINTRESOURCE(IDD_DIALOG1),
        *pad.hwndParent,
        PADConfigDialogProc);
}

BOOL CALLBACK PADAboutDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    char buf[256], *ptr;

    switch(uMsg)
    {
        case WM_INITDIALOG:
            ptr = buf;
            ptr += sprintf(ptr, "DATE\t: %s\n", __DATE__);
            ptr += sprintf(ptr, "TIME\t: %s\n", __TIME__);
            ptr += sprintf(ptr, "SPEC\t: %s  ", DOL_PLUG_VER);
            SetDlgItemText(hwndDlg, IDC_ABOUT_STAMP, buf);
            return TRUE;

        case WM_CLOSE:
            EndDialog(hwndDlg, 0);
            return TRUE;

        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDC_PAD_ABOUT_OK:
                    PADSaveConfig(hwndDlg);
                    EndDialog(hwndDlg, 0);
                    return TRUE;
            }
            break;

        default:
            return FALSE;
    }
    return FALSE;
}


void PADAbout()
{
    DialogBox(
        pad.inst,
        MAKEINTRESOURCE(IDD_DIALOG2),
        *pad.hwndParent,
        PADAboutDialogProc);
}
