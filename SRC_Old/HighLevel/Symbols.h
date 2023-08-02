#pragma once

#include <map>

// symbolic entry
struct SYM
{
    uint32_t eaddr;             // effective address
    char*   savedName;          // symbolic description
    void    (*routine)();       // associated high-level call
};

// all important variables are here
struct SYMControl
{
    std::map<uint32_t, SYM*> symmap;
};

extern  SYMControl sym;

// API for emulator
void    SYMAddNew(uint32_t addr, const char *name);
void    SYMSetHighlevel(const char *symName, void (*routine)());
uint32_t SYMAddress(const char *symName);
char*   SYMName(uint32_t symAddr);
char* SYMGetNearestName(uint32_t address, size_t& offset);
void    SYMKill();
void    SYMList(const char *str="*");

// advanced stuff (dont use it, if you dont know how)
void    SYMSetWorkspace(SYMControl *useIt);
void    SYMCompareWorkspaces (
    SYMControl      *source,
    SYMControl      *dest,
    // callback is called when symbol in source wasnt found in dest
    void (*DiffCallback)(uint32_t ea, char * name)
);
