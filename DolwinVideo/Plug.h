extern  HINSTANCE   hPlugin;
extern  HWND*       hwndMain;

void    GFXError(char *fmt, ...);

// my assertion system
// note : assertion is fired, when condition is TRUE
#define ASSERT(expr)                                        \
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
