extern  HINSTANCE   hPlugin;
extern  HWND        hwndMain;

void    GFXError(const char *fmt, ...);

#define VERIFY(expr)                                        \
{                                                           \
    if(expr)                                                \
    {                                                       \
       GFXError(                                            \
            "file\t: %s\nline\t: %i\nexpr\t: %s\n",         \
            __FILE__,                                       \
            __LINE__,                                       \
            #expr);                                         \
    }                                                       \
}
