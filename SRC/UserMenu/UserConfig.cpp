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

    if(RegOpenKeyEx(
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

    RegCreateKeyEx(
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

    RegDeleteKey(USER_REG_HKEY, subKey);
    RegKey = NULL;
}

static void RegQuery(int s, char *name, LPBYTE var)
{
    DWORD type, size = s;

    if(!RegKey) return;

    RegQueryValueEx(RegKey, name, 0, &type, var, &size);
}

static void RegSet(char *name, LPBYTE var, int size, DWORD type)
{
    if(!RegKey) return;

    RegSetValueEx(RegKey, name, 0, type, var, size);
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

// ---------------------------------------------------------------------------
// reading of game info from INI-file

static BOOL SectionPresent(char *gameId)
{
    char sectionData[0x1000];

    int length = GetPrivateProfileSection(
        gameId,
        sectionData,
        sizeof(sectionData),
        USER_INI_FILE
    );

    return (BOOL)length;
}

BOOL GetGameInfo(char *gameId, char title[64], char comment[128])
{
    // there is no such section
    if(!SectionPresent(gameId))
    {
        return FALSE;
    }

    // read alternate game title
    GetPrivateProfileString(
        gameId,
        "alttitle",
        "Unknown",
        title,
        64,
        USER_INI_FILE
    );

    // read comments
    GetPrivateProfileString(
        gameId,
        "comment",
        "Get updated GAMES.INI",
        comment,
        128,
        USER_INI_FILE
    );

    return TRUE;
}

void SetGameInfo(char *gameId, char title[64], char comment[128])
{
    char newComment[150];
    BOOL present = SectionPresent(gameId);

    // insert line-feed to the end of string, 
    // to interleave INI sections for best view.
    if(!present)
    {
        sprintf_s(newComment, sizeof(newComment), "%s\0xd\n", comment);
    }

    // write alternate game title
    WritePrivateProfileString(
        gameId,
        "alttitle",
        title,
        USER_INI_FILE
    );

    // write comments
    WritePrivateProfileString(
        gameId,
        "comment",
        present ? comment : newComment,
        USER_INI_FILE
    );
}

char *GetIniVar(char *gameId, char *var)
{
    static char buf[256];

    GetPrivateProfileString(
        gameId,
        var,
        "ERROR",
        buf,
        sizeof(buf),
        USER_INI_FILE
    );

    if(!_stricmp(buf, "ERROR")) return NULL;
    else return buf;
}
