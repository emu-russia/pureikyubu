// Stubs to Dolwin debugger
#include "pch.h"

void (*DBHalt)(const char* text, ...);    // always breaks emulation
void (*DBReport)(const char* text, ...);  // do debugger output

// start debugger loop, from PC
void DBStart()
{ }
