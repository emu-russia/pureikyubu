/*++

Copyright (c)

Module Name:

    GuiMain.h

Abstract:

    Dolwin entrypoint (WinMain) and fail-safe application messages.

--*/

VOID DolwinError ( PTCHAR Title, PTCHAR Format, ... );
BOOLEAN DolwinQuestion ( PTCHAR Title, PTCHAR Format, ... );
VOID DolwinReport ( PTCHAR Format, ... );

//
// Dolwin assertion macro.
// Note: assertion is fired, when condition is FALSE
//

#define ASSERTMSG(expr, msg)                                                \
    (void) (!(expr) &&                                                      \
    (                                                                       \
       DolwinError(                                                         \
            APPNAME _T(" Assertion Failed!"),                               \
            _T("expr\t: %s\n")                                              \
            _T("file\t: %s\n")                                              \
            _T("line\t: %i\n")                                              \
            _T("note\t: %s\n\n"),                                           \
            _T(#expr)   ,                                                   \
            _T(__FILE__),                                                   \
            __LINE__,                                                       \
            msg)                                                            \
    , 0))
