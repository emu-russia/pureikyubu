
#pragma once

// '\n' is supported (no auto-linefeed!)
extern  void (*DBHalt)(const char* text, ...);    // always breaks emulation
extern  void (*DBReport)(const char* text, ...);  // do debugger output

// exception trap
void    DBException(uint32_t code);

// start debugger loop, from PC
void    DBStart();
