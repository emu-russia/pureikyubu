// Stubs to Dolwin debugger
#include "dolphin.h"

void (*DBHalt)(const char* text, ...);    // always breaks emulation
void (*DBReport)(const char* text, ...);  // do debugger output

// exception trap
void DBException(uint32_t code)
{ }

// start debugger loop, from PC
void DBStart()
{ }
