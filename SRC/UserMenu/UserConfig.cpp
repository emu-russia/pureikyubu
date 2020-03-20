// Dolwin user variables access
#include "dolphin.h"

static HKEY RegKey;

// ---------------------------------------------------------------------------
// windows registry API for higher level

static void RegClose()
{
    if(!RegKey) return;

    RegCloseKey(RegKey);
    RegKey = NULL;
}

static int RegOpen(LPSTR subKey = USER_KEY_NAME)
{
    if(RegKey) RegClose();

    if(RegOpenKeyExA(
        USER_REG_HKEY,
        subKey,
        0, KEY_ALL_ACCESS,
        &RegKey) == ERROR_SUCCESS)
    {
        return 1;
    }
    else return 0;
}

static void RegCreate(LPSTR subKey = USER_KEY_NAME)
{
    DWORD RegDisp;

    if(RegKey) RegClose();

    RegCreateKeyExA(
        USER_REG_HKEY,
        subKey,
        0, NULL,
        REG_OPTION_NON_VOLATILE,
        KEY_ALL_ACCESS,
        NULL,
        &RegKey, &RegDisp);
}

static void RegDelete(LPSTR subKey = USER_KEY_NAME)
{
    if(RegKey) RegClose();

    RegDeleteKeyA(USER_REG_HKEY, subKey);
    RegKey = NULL;
}

static void RegQuery(int s, char *name, LPBYTE var)
{
    DWORD type, size = s;

    if(!RegKey) return;

    RegQueryValueExA(RegKey, name, 0, &type, var, &size);
}

static void RegSet(char *name, LPBYTE var, int size, DWORD type)
{
    if(!RegKey) return;

    RegSetValueExA(RegKey, name, 0, type, var, size);
}

// ---------------------------------------------------------------------------
// application layer, used by emu (user variables access)

char *GetConfigString(char *var, char *defVal, char *path)
{
    static char buf[256];

    if(RegOpen(path) == 0)
    {
        SetConfigString(var, defVal, path);
        return defVal;
    }

    memset(buf, 0, sizeof(buf));
    RegQuery(256, var, (LPBYTE)buf);

    if(strlen(buf) == 0)
    {
        SetConfigString(var, defVal, path);
        return defVal;
    }

    RegClose();

    return buf;
}

void SetConfigString(char *var, char *newVal, char *path)
{
    if(RegOpen(path) == 0)
    {
        RegCreate(path);
    }

    RegSet(var, (LPBYTE)newVal, (int)strlen(newVal), REG_SZ);

    RegClose();
}

int GetConfigInt(char *var, int defVal, char *path)
{
    char buf[256], *notused;

    if(RegOpen(path) == 0)
    {
        SetConfigInt(var, defVal, path);
        return defVal;
    }

    memset(buf, 0, sizeof(buf));
    RegQuery(256, var, (LPBYTE)buf);

    if(strlen(buf) == 0)
    {
        SetConfigInt(var, defVal, path);
        return defVal;
    }

    RegClose();

    return strtoul(buf, &notused, 16);
}

void SetConfigInt(char *var, int newVal, char *path)
{
    char buf[256];

    if(RegOpen(path) == 0)
    {
        RegCreate(path);
    }

    sprintf_s(buf, sizeof(buf), "0x%.8X", newVal);

    RegSet(var, (LPBYTE)buf, (int)strlen(buf), REG_SZ);
    RegClose();
}

void KillConfigInt(char *var, char *path)
{
    char key[256];
    sprintf_s(key, sizeof(key), "%s\\%s", path, var);
    RegDelete(key);
}
