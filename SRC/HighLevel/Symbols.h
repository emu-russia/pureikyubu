// symbols can be identified by tag. tag is sum of all ciphers
// of lower word of effective address. max tag value = 60, so
// we have hash for 61 entries (tag=0..60).

// symbolic entry
typedef struct SYM
{
    uint32_t eaddr;             // effective address
    char*   savedName;          // symbolic description
    void    (*routine)();       // associated high-level call
    BOOL    emuSymbol;          // 1, when symbol from Dolwin.exe
} SYM;

// all important variables are here
typedef struct SYMControl
{
    SYM*        symhash[61];    // symbol list
    int         symcount[61];
} SYMControl;

extern  SYMControl sym;

// API for emulator
void    SYMAddNew(uint32_t addr, char *name, BOOL emuSymbol=FALSE);
void    SYMSetHighlevel(char *symName, void (*routine)());
uint32_t SYMAddress(char *symName);
char*   SYMName(uint32_t symAddr);
void    SYMKill();
void    SYMList(char *str="*");
void    SYMAddEmulatorSymbols();

// advanced stuff (dont use it, if you dont know how)
void    SYMSetWorkspace(SYMControl *useIt);
void    SYMCompareWorkspaces (
    SYMControl      *source,
    SYMControl      *dest,
    // callback is called when symbol in source wasnt found in dest
    void (*DiffCallback)(uint32_t ea, char * name)
);
