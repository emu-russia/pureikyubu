#pragma once

// symbolic entry
struct SYM
{
    uint32_t eaddr;             // effective address
    char* savedName;          // symbolic description
    void    (*routine)();       // associated high-level call
};

// all important variables are here
struct SYMControl
{
    std::map<uint32_t, SYM*> symmap;
};

extern  SYMControl sym;

// API for emulator
void    SYMAddNew(uint32_t addr, const char* name);
void    SYMSetHighlevel(const char* symName, void (*routine)());
uint32_t SYMAddress(const char* symName);
char* SYMName(uint32_t symAddr);
char* SYMGetNearestName(uint32_t address, size_t& offset);
void    SYMKill();
void    SYMList(const char* str = "*");

// advanced stuff (dont use it, if you dont know how)
void    SYMSetWorkspace(SYMControl* useIt);
void    SYMCompareWorkspaces(
    SYMControl* source,
    SYMControl* dest,
    // callback is called when symbol in source wasnt found in dest
    void (*DiffCallback)(uint32_t ea, char* name)
);



enum class MAP_FORMAT : int
{
    BAD = 0,
    RAW,             // MAP format, invented by org
    CW,              // CodeWarrior
    GCC,             // GCC
};

MAP_FORMAT LoadMAP(const wchar_t* mapname, bool add = false);
MAP_FORMAT LoadMAP(const char* mapname, bool add = false);



/*
 * Starts the creation of a new map
 */
void MAPInit(const wchar_t* mapname);

/*
 * Adds a mark to the opcode at the specified offset.
 * if blr is FALSE, the mark is considerated an entrypoint to a function
 * if blr is not FALSE, the mark is considerated an exitpoint from the function
 * Use carefully!!!
 */
void MAPAddMark(uint32_t offset, bool blr);

/*
 * Checks the specified range, and automatically adds marks to entry and exit points to functions.
 */
void MAPAddRange(uint32_t offsetStart, uint32_t offsetEnd);

/*
 * Finishes the creation of the current map
 */
void MAPFinish();


void	SaveMAP(const char* mapname);
