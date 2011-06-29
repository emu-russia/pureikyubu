// execute another application
BOOL    DolwinExecute(char *appName, char *cmdLine="");

// fall-back into main loop
void    DolwinMainLoop();

// basic message output
void    DolwinError(char *title, char *fmt, ...);
BOOL    DolwinQuestion(char *title, char *fmt, ...);
void    DolwinReport(char *fmt, ...);

// Dolwin assertion macro. note : assertion is fired, when condition is TRUE
// I dont know how it works : (void) ((expr) && (error, 0)), but it does =:)
#define ASSERT(expr, msg)                                                   \
    (void) ((expr) &&                                                       \
    (                                                                       \
       DolwinError(                                                         \
            APPNAME " Assertion Failed!",                                   \
            "expr\t: %s\n"                                                  \
            "file\t: %s\n"                                                  \
            "line\t: %i\n"                                                  \
            "note\t: %s\n\n",                                               \
            #expr   ,                                                       \
            FileShortName(__FILE__),                                        \
            __LINE__,                                                       \
            msg)                                                            \
    , 0))
