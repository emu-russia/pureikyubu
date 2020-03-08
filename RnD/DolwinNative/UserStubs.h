
// basic message output
void    DolwinError(const char* title, const char* fmt, ...);
BOOL    DolwinQuestion(const char* title, const char* fmt, ...);
void    DolwinReport(const char* fmt, ...);

void    OnMainWindowOpened();
void    OnMainWindowClosed();

#define VERIFY(expr, msg)
