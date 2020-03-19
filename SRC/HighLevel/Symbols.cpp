// symbolic information API.
#include "pch.h"

// IMPORTANT : EXE loading base must be 0x00400000, for correct HLE.
// MSVC : Project/Settings/Link/Output/Base Address
// CW : Edit/** Win32 x86 Settings/Linker/x86 COFF/Base address

// all important variables are here
SYMControl sym;                 // default workspace
static SYMControl *work = &sym; // current workspace (in use)

// ---------------------------------------------------------------------------

// find first occurency of symbol in list
static SYM * symfind(const char *symName)
{
    SYM *symbol = NULL;

    // walk all
    for(int tag=0; tag<61; tag++)
    for(int i=0; i<work->symcount[tag]; i++)
    {
        if(!strcmp(work->symhash[tag][i].savedName, symName))
        {
            symbol = &work->symhash[tag][i]; // found!
            return symbol;
        }
    }

    return NULL;
}

// calculate tag - sum of all ciphers in lower word of address
static int gettag(uint32_t addr)
{
    uint32_t lo = (uint16_t)addr;
    return ((lo >>  0) & 0xf) +         // value = 0...60
           ((lo >>  4) & 0xf) +
           ((lo >>  8) & 0xf) +
           ((lo >> 12) & 0xf);
}

void SYMSetWorkspace(SYMControl *useIt)
{
    work = useIt;
}

void SYMCompareWorkspaces (
    SYMControl      *source,
    SYMControl      *dest,
    void (*DiffCallback)(uint32_t ea, char * name)
)
{
    // walk all
    for(int tag=0; tag<61; tag++)
    for(int i=0; i<source->symcount[tag]; i++)
    {
        BOOL found = FALSE;
        SYM *symbol = &source->symhash[tag][i];
        if( !symbol->emuSymbol && symbol )
        {
            for(int tag2=0; tag2<61; tag2++)
            {
                for(int i2=0; i2<dest->symcount[tag2]; i2++)
                {
                    SYM *symbol2 = &dest->symhash[tag][i];
                    if(symbol2->emuSymbol || symbol2 == NULL) continue;
                    if(!strcmp(symbol->savedName, symbol2->savedName))
                    {
                        found = TRUE;
                        break;
                    }
                }
                if(found) break;
            }
            if(!found)
                DiffCallback(symbol->eaddr, symbol->savedName);
        }
    }
}

// get address of symbolic label
// if label is not specified, return 0
uint32_t SYMAddress(const char *symName)
{
    // try to find specified symbol
    SYM *symbol = symfind(symName);

    if(symbol) return symbol->eaddr;
    else return 0;
}

// get symbolic label by given address
// if label is not specified, return NULL
char * SYMName(uint32_t symAddr)
{
    // calculate tag
    int tag = gettag(symAddr);

    // walk all
    for(int i=0; i<work->symcount[tag]; i++)
    {
        if(work->symhash[tag][i].eaddr == symAddr)
        {
            return work->symhash[tag][i].savedName;
        }
    }

    // symbol not found
    return NULL;
}

// associate high-level call with symbol
// (if CPU reaches label, it jumps to HLE call)
void SYMSetHighlevel(const char *symName, void (*routine)())
{
    // try to find specified symbol
    SYM *symbol = symfind(symName);

    // check address
    VERIFY((uint64_t)routine & ~0x03ffffff, "High-level call is too high in memory.");

    // leave, if symbol is not found. add otherwise.
    if(symbol)
    {
        symbol->routine = routine;      // overwrite

        // if first opcode is 'BLR', then just leave it
        uint32_t op;
        CPUReadWord(symbol->eaddr, &op);
        if(op != 0x4e800020)
        {
            CPUWriteWord(
                symbol->eaddr,          // add patch
                (uint32_t)((uint64_t)routine & 0x03ffffff)  // 000: high-level opcode
            );
            if(!_stricmp(symName, "OSLoadContext"))
            {
                CPUWriteWord(
                    symbol->eaddr + 4,  // return to caller
                    0x4c000064          // rfi
                );
            }
            else
            {
                CPUWriteWord(
                    symbol->eaddr + 4,  // return to caller
                    0x4e800020          // blr
                );
            }
        }
        DBReport2(DbgChannel::HLE, "patched API call: %08X %s\n", symbol->eaddr, symName);
    }
    else return;
}

// save string in memory
static char * strsave(const char *str)
{
    size_t len = strlen(str) + 1;
    char *saved = (char *)malloc(len);
    if(saved == NULL)
    {
        DolwinError( "strsave in HighLevel\\Symbols.cpp",
                     "Not enough memory for new string : %s\n\n", str );
        // halt !
    }
    strcpy(saved, str);
    return saved;
}

// add new symbol
void SYMAddNew(uint32_t addr, const char *name, bool emuSymbol /* false */)
{
    int i;
    // calculate tag
    int tag = gettag(addr);

    // ignore NULL address
    if(addr == 0) return;

    // check if already present
    for(i=0; i<work->symcount[tag]; i++)
    {
        // if yes, then replace
        if(work->symhash[tag][i].eaddr == addr)
        {
            if(work->symhash[tag][i].savedName)
            {
                free(work->symhash[tag][i].savedName);
                work->symhash[tag][i].savedName = NULL;
            }
            work->symhash[tag][i].savedName = strsave(name);
            return;
        }
    }

    // if not, then add new
    i = work->symcount[tag];
    work->symhash[tag] = (work->symhash[tag] != NULL) ?
        ( (SYM *)realloc(work->symhash[tag], sizeof(SYM) * (i + 1)) ) :
        ( (SYM *)malloc(sizeof(SYM)) );
    assert(work->symhash[tag]);
    work->symhash[tag][i].eaddr = addr;
    work->symhash[tag][i].savedName = strsave(name);
    work->symhash[tag][i].routine = NULL;
    work->symhash[tag][i].emuSymbol = emuSymbol;
    work->symcount[tag]++;
}

// remove all symbols (delete list)
void SYMKill()
{
    // kill 'em all
    for(int tag=0; tag<61; tag++)
    {
        for(int i=0; i<work->symcount[tag]; i++)
        {
            if(work->symhash[tag][i].savedName)
            {
                free(work->symhash[tag][i].savedName);
                work->symhash[tag][i].savedName = NULL;
            }
        }
        if(work->symhash[tag])
        {
            free(work->symhash[tag]);
            work->symhash[tag] = NULL;
        }
        work->symcount[tag] = 0;
    }
}

// list symbols, matching first occurence of "str".
// * - all symbols (warning! Zelda has about 20000 symbols).
void SYMList(const char *str)
{
    size_t len = strlen(str), cnt = 0;
    DBReport("tag:id <address> symbol\n\n");

    // walk all
    for(int tag=0; tag<61; tag++)
    for(int i=0; i<work->symcount[tag]; i++)
    {
        SYM *symbol = &work->symhash[tag][i];
        if( ((*str == '*') || !_strnicmp(str, symbol->savedName, len)) && !symbol->emuSymbol )
        {
            DBReport(
                "%02i:%03i <%08X> %s\n", 
                tag, i, symbol->eaddr, symbol->savedName
            );
            cnt++;
        }
    }

    DBReport("%i match\n\n", cnt);
}
