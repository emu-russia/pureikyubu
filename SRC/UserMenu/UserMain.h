// basic message output
void    DolwinError(const char *title, const char *fmt, ...);
BOOL    DolwinQuestion(const char *title, const char *fmt, ...);
void    DolwinReport(const char *fmt, ...);

#define VERIFY(expr, msg)                                                   \
    (void) ((expr) &&                                                       \
    (                                                                       \
       DolwinError(                                                         \
            "Assertion Failed!",                                            \
            "expr\t: %s\n"                                                  \
            "file\t: %s\n"                                                  \
            "line\t: %i\n"                                                  \
            "note\t: %s\n\n",                                               \
            #expr   ,                                                       \
            FileShortName(__FILE__),                                        \
            __LINE__,                                                       \
            msg)                                                            \
    , 0))

extern HACCEL  hAccel;
