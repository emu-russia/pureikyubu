// PAD plugin configure and about dialogs
#include "pch.h"

typedef struct
{
    int         padToConfigure;
    PADCONF     config[4];
} PAD;

PAD pad;

static const wchar_t *vkeys[256] = { // default keyboard virtual codes description (? - not used)
 L"?", L"?", L"?", L"?", L"?", L"?", L"?", L"?", L"Bkspace", L"Tab", L"?", L"?", L"?", L"Enter", L"?", L"?", // 00-0F
 L"Shift", L"Control", L"Alt", L"?", L"?", L"?", L"?", L"?", L"?", L"?", L"?", L"?", L"?", L"?", L"?", L"?", // 10-1F
 L"Space", L"PgUp", L"PgDown", L"End", L"Home", L"Left", L"Up", L"Right", L"Down", L"?", L"?", L"?", L"?", L"Ins", L"Del", L"?", // 20-2F
 L"0", L"1", L"2", L"3", L"4", L"5", L"6", L"7", L"8", L"9", L"?", L"?", L"?", L"?", L"?", L"?", // 30-3F
 L"", L"A", L"B", L"C", L"D", L"E", L"F", L"G", L"H",  L"I", L"J", L"K", L"L", L"M", L"N", L"O", // 40-4F
 L"P", L"Q", L"R", L"S", L"T", L"U", L"V", L"W", L"X", L"Y", L"Z", L"?", L"?", L"?", L"?", L"?", // 50-5F
 L"Num 0", L"Num 1", L"Num 2", L"Num 3", L"Num 4", L"Num 5", L"Num 6", L"Num 7",
 L"Num 8", L"Num 9", L"Mult", L"Plus", L"Bkslash", L"Minus", L"Decimal", L"Slash", // 60-6F
 L"F1", L"F2", L"F3", L"F4", L"F5", L"F6", L"F7", L"F8", L"F9", L"F10", L"F11", L"F12", L"?", L"?", L"?", L"?", // 70-7F
};

static const wchar_t* GetVKDesc(int vkey)
{
    if(vkey >= 0x80) return L"?";
    else return vkeys[vkey];
}

void PADLoadConfig(HWND hwndDlg)
{
    char parm[256] = { 0 };
    int vkey;

    // Plugged or not
    sprintf_s(parm, sizeof(parm), "PluggedIn""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].plugged = UI::Jdi.GetConfigBool(parm, USER_PADS);

    //
    // Buttons 8|
    //

    sprintf_s(parm, sizeof(parm), "VKEY_FOR_UP""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_UP] = UI::Jdi.GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_DOWN""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_DOWN] = UI::Jdi.GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_LEFT""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_LEFT] = UI::Jdi.GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_RIGHT""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_RIGHT] = UI::Jdi.GetConfigInt(parm, USER_PADS);

    sprintf_s(parm, sizeof(parm), "VKEY_FOR_XUP50""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP50] = UI::Jdi.GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_XUP100""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP100] = UI::Jdi.GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_XDOWN50""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN50] = UI::Jdi.GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_XDOWN100""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN100] = UI::Jdi.GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_XLEFT50""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT50] = UI::Jdi.GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_XLEFT100""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT100] = UI::Jdi.GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_XRIGHT50""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT50] = UI::Jdi.GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_XRIGHT100""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT100] = UI::Jdi.GetConfigInt(parm, USER_PADS);

    sprintf_s(parm, sizeof(parm), "VKEY_FOR_CXUP""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXUP] = UI::Jdi.GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_CXDOWN""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXDOWN] = UI::Jdi.GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_CXLEFT""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXLEFT] = UI::Jdi.GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_CXRIGHT""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXRIGHT] = UI::Jdi.GetConfigInt(parm, USER_PADS);

    sprintf_s(parm, sizeof(parm), "VKEY_FOR_TRIGGERL""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERL] = UI::Jdi.GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_TRIGGERR""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERR] = UI::Jdi.GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_TRIGGERZ""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERZ] = UI::Jdi.GetConfigInt(parm, USER_PADS);

    sprintf_s(parm, sizeof(parm), "VKEY_FOR_A""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_A] = UI::Jdi.GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_B""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_B] = UI::Jdi.GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_X""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_X] = UI::Jdi.GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_Y""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_Y] = UI::Jdi.GetConfigInt(parm, USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_START""_%i", pad.padToConfigure);
    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_START] = UI::Jdi.GetConfigInt(parm, USER_PADS);

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
        if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_UP, L"...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_UP, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_DOWN];
        if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_DOWN, L"...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_DOWN, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_RIGHT];
        if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_RIGHT, L"...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_RIGHT, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_LEFT];
        if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_LEFT, L"...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_LEFT, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP50];
        if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XUP50, L"...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_XUP50, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP100];
        if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XUP100, L"...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_XUP100, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN50];
        if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN50, L"...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN50, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN100];
        if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN100, L"...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN100, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT50];
        if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT50, L"...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT50, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT100];
        if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT100, L"...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT100, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT50];
        if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT50, L"...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT50, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT100];
        if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT100, L"...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT100, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXUP];
        if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_CXUP, L"...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_CXUP, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXDOWN];
        if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_CXDOWN, L"...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_CXDOWN, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXRIGHT];
        if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_CXRIGHT, L"...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_CXRIGHT, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXLEFT];
        if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_CXLEFT, L"...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_CXLEFT, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERL];
        if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERL, L"...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERL, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERR];
        if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERR, L"...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERR, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERZ];
        if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERZ, L"...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERZ, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_A];
        if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_A, L"...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_A, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_B];
        if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_B, L"...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_B, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_X];
        if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_X, L"...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_X, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_Y];
        if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_Y, L"...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_Y, GetVKDesc(vkey));

        vkey = pad.config[pad.padToConfigure].vkeys[VKEY_FOR_START];
        if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_START, L"...");
        else SetDlgItemText(hwndDlg, IDC_BUTTON_START, GetVKDesc(vkey));
    }

    if(!pad.config[pad.padToConfigure].plugged)
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
    sprintf_s(parm, sizeof(parm), "PluggedIn""_%i", pad.padToConfigure);
    if(pad.config[pad.padToConfigure].plugged) UI::Jdi.SetConfigBool(parm, true, USER_PADS);
    else UI::Jdi.SetConfigBool(parm, false, USER_PADS);

    //
    // Buttons
    //

    sprintf_s(parm, sizeof(parm), "VKEY_FOR_UP""_%i", pad.padToConfigure);
    UI::Jdi.SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_UP], USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_DOWN""_%i", pad.padToConfigure);
    UI::Jdi.SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_DOWN], USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_LEFT""_%i", pad.padToConfigure);
    UI::Jdi.SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_LEFT], USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_RIGHT""_%i", pad.padToConfigure);
    UI::Jdi.SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_RIGHT], USER_PADS);

    sprintf_s(parm, sizeof(parm), "VKEY_FOR_XUP50""_%i", pad.padToConfigure);
    UI::Jdi.SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP50], USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_XUP100""_%i", pad.padToConfigure);
    UI::Jdi.SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP100], USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_XDOWN50""_%i", pad.padToConfigure);
    UI::Jdi.SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN50], USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_XDOWN100""_%i", pad.padToConfigure);
    UI::Jdi.SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN100], USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_XLEFT50""_%i", pad.padToConfigure);
    UI::Jdi.SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT50], USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_XLEFT100""_%i", pad.padToConfigure);
    UI::Jdi.SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT100], USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_XRIGHT50""_%i", pad.padToConfigure);
    UI::Jdi.SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT50], USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_XRIGHT100""_%i", pad.padToConfigure);
    UI::Jdi.SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT100], USER_PADS);

    sprintf_s(parm, sizeof(parm), "VKEY_FOR_CXUP""_%i", pad.padToConfigure);
    UI::Jdi.SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXUP], USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_CXDOWN""_%i", pad.padToConfigure);
    UI::Jdi.SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXDOWN], USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_CXLEFT""_%i", pad.padToConfigure);
    UI::Jdi.SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXLEFT], USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_CXRIGHT""_%i", pad.padToConfigure);
    UI::Jdi.SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXRIGHT], USER_PADS);

    sprintf_s(parm, sizeof(parm), "VKEY_FOR_TRIGGERL""_%i", pad.padToConfigure);
    UI::Jdi.SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERL], USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_TRIGGERR""_%i", pad.padToConfigure);
    UI::Jdi.SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERR], USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_TRIGGERZ""_%i", pad.padToConfigure);
    UI::Jdi.SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERZ], USER_PADS);

    sprintf_s(parm, sizeof(parm), "VKEY_FOR_A""_%i", pad.padToConfigure);
    UI::Jdi.SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_A], USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_B""_%i", pad.padToConfigure);
    UI::Jdi.SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_B], USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_X""_%i", pad.padToConfigure);
    UI::Jdi.SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_X], USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_Y""_%i", pad.padToConfigure);
    UI::Jdi.SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_Y], USER_PADS);
    sprintf_s(parm, sizeof(parm), "VKEY_FOR_START""_%i", pad.padToConfigure);
    UI::Jdi.SetConfigInt(parm, pad.config[pad.padToConfigure].vkeys[VKEY_FOR_START], USER_PADS);
}

// reset all butons and unplug pad
void PADClearConfig(HWND hwndDlg)
{
    int i;

    pad.config[pad.padToConfigure].plugged = false;

    for(i=0; i<VKEY_FOR_MAX; i++)
    {
        pad.config[pad.padToConfigure].vkeys[i] = 0;
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
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP50] = 0;
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP100] = 0x26;    // Up
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN50] = 0;
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN100] = 0x28;  // Down
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT50] = 0;
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT100] = 0x25;  // Left
            pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT50] = 0;
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
                if(i == VK_ESCAPE) return 0;
                else return i;
            }
        }
    }
}

INT_PTR CALLBACK PADConfigDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    wchar_t buf[256];
    int vkey;

    switch(uMsg)
    {
        case WM_INITDIALOG:
            swprintf_s (buf, _countof(buf) - 1, L"Configure Controller %i", pad.padToConfigure + 1);
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
                    SetDlgItemText(hwndDlg, IDC_BUTTON_UP, L"?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_UP] = vkey;
                    if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_UP, L"...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_UP, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_DOWN:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_DOWN, L"?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_DOWN] = vkey;
                    if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_DOWN, L"...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_DOWN, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_LEFT:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_LEFT, L"?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_LEFT] = vkey;
                    if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_LEFT, L"...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_LEFT, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_RIGHT:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_RIGHT, L"?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_RIGHT] = vkey;
                    if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_RIGHT, L"...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_RIGHT, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_XUP50:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_XUP50, L"?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP50] = vkey;
                    if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XUP50, L"...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_XUP50, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_XUP100:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_XUP100, L"?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XUP100] = vkey;
                    if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XUP100, L"...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_XUP100, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_XDOWN50:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN50, L"?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN50] = vkey;
                    if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN50, L"...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN50, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_XDOWN100:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN100, L"?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XDOWN100] = vkey;
                    if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN100, L"...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_XDOWN100, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_XLEFT50:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT50, L"?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT50] = vkey;
                    if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT50, L"...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT50, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_XLEFT100:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT100, L"?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XLEFT100] = vkey;
                    if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT100, L"...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_XLEFT100, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_XRIGHT50:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT50, L"?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT50] = vkey;
                    if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT50, L"...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT50, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_XRIGHT100:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT100, L"?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_XRIGHT100] = vkey;
                    if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT100, L"...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_XRIGHT100, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_CXUP:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_CXUP, L"?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXUP] = vkey;
                    if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_CXUP, L"...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_CXUP, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_CXDOWN:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_CXDOWN, L"?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXDOWN] = vkey;
                    if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_CXDOWN, L"...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_CXDOWN, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_CXLEFT:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_CXLEFT, L"?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXLEFT] = vkey;
                    if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_CXLEFT, L"...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_CXLEFT, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_CXRIGHT:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_CXRIGHT, L"?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_CXRIGHT] = vkey;
                    if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_CXRIGHT, L"...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_CXRIGHT, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_TRIGGERL:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERL, L"?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERL] = vkey;
                    if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERL, L"...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERL, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_TRIGGERR:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERR, L"?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERR] = vkey;
                    if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERR, L"...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERR, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_TRIGGERZ:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERZ, L"?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_TRIGGERZ] = vkey;
                    if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERZ, L"...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_TRIGGERZ, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_A:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_A, L"?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_A] = vkey;
                    if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_A, L"...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_A, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_B:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_B, L"?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_B] = vkey;
                    if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_B, L"...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_B, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_X:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_X, L"?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_X] = vkey;
                    if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_X, L"...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_X, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_Y:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_Y, L"?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_Y] = vkey;
                    if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_Y, L"...");
                    else SetDlgItemText(hwndDlg, IDC_BUTTON_Y, GetVKDesc(vkey));
                    return FALSE;

                case IDC_BUTTON_START:
                    SetDlgItemText(hwndDlg, IDC_BUTTON_START, L"?");
                    vkey = GetVKey();
                    pad.config[pad.padToConfigure].vkeys[VKEY_FOR_START] = vkey;
                    if(vkey == 0) SetDlgItemText(hwndDlg, IDC_BUTTON_START, L"...");
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

                            pad.config[pad.padToConfigure].plugged = true;
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

                            pad.config[pad.padToConfigure].plugged = false;
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

void PADConfigure(long padnum, HWND hwndParent)
{
    pad.padToConfigure = padnum;

    DialogBox(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(IDD_DIALOG_PAD),
        hwndParent,
        PADConfigDialogProc);
}
