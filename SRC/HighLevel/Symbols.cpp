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
    for (auto it = work->symmap.begin(); it != work->symmap.end(); ++it)
    {
        if (!strcmp(it->second->savedName, symName))
        {
            return it->second;
        }
    }
    return nullptr;
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
    for (auto sourceId = source->symmap.begin(); sourceId != source->symmap.end(); ++sourceId)
    {
        bool found = false;

        for (auto destIt = dest->symmap.begin(); destIt != dest->symmap.end(); ++destIt)
        {
            if (!strcmp(sourceId->second->savedName, destIt->second->savedName))
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            DiffCallback(sourceId->second->eaddr, sourceId->second->savedName);
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
    auto it = work->symmap.find(symAddr);

    if (it == work->symmap.end())
    {
        return nullptr;
    }

    return it->second->savedName;
}

// associate high-level call with symbol
// (if CPU reaches label, it jumps to HLE call)
void SYMSetHighlevel(const char *symName, void (*routine)())
{
    // try to find specified symbol
    SYM *symbol = symfind(symName);

    // check address
    // High-level call is too high in memory.
    assert(((uint64_t)routine & ~0x03ffffff) == 0);

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
}

// save string in memory
static char * strsave(const char *str)
{
    size_t len = strlen(str) + 1;
    char *saved = new char[len];
    assert(saved);
    strcpy(saved, str);
    return saved;
}

// add new symbol
void SYMAddNew(uint32_t addr, const char *name)
{
    SYM* symbol = symfind(name);

    if (symbol != nullptr)
    {
        // Replace name
        if (symbol->savedName)
        {
            delete[] symbol->savedName;
            symbol->savedName = nullptr;
        }
        symbol->savedName = strsave(name);
    }
    else
    {
        // Add new
        SYM* sym = new SYM;

        sym->eaddr = addr;
        sym->savedName = strsave(name);
        sym->routine = nullptr;

        work->symmap[addr] = sym;
    }
}

// Remove all symbols
void SYMKill()
{
    for (auto it = work->symmap.begin(); it != work->symmap.end(); ++it)
    {
        if (it->second->savedName)
        {
            delete[] it->second->savedName;
        }
        delete it->second;
    }

    work->symmap.clear();
}

// list symbols, matching first occurence of "str".
// * - all symbols (warning! Zelda has about 20000 symbols).
void SYMList(const char *str)
{
    size_t len = strlen(str), cnt = 0;
    DBReport("<address> symbol\n\n");

    for (auto it = work->symmap.begin(); it != work->symmap.end(); ++it)
    {
        SYM* symbol = it->second;
        if (((*str == '*') || !_strnicmp(str, symbol->savedName, len)))
        {
            DBReport("<%08X> %s\n", symbol->eaddr, symbol->savedName);
            cnt++;
        }
    }

    DBReport("%i match\n\n", cnt);
}
