// registry settings
#define USER_REG_HKEY    HKEY_CURRENT_USER
#define USER_KEY_NAME    "Software\\Dolwin Emulator\\DVD Plugin"

// user variables API
char *  GetConfigString(char *var, char *defVal, char *path = USER_KEY_NAME);
void    SetConfigString(char *var, char *newVal, char *path = USER_KEY_NAME);
int     GetConfigInt(char *var, int defVal, char *path = USER_KEY_NAME);
void    SetConfigInt(char *var, int newVal, char *path = USER_KEY_NAME);

// ---------------------------------------------------------------------------
// DVD Plugin Variables list (a-z).
// ---------------------------------------------------------------------------

