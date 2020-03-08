
#pragma once

// color codes for console
// format: control byte, color byte
// control byte:
//   0x1 - set foreground color 
//   0x2 - set background color
#define DBLUE   "\x1\x1"
#define GREEN   "\x1\x2"
#define CYAN    "\x1\x3"
#define RED     "\x1\x4"
#define PUR     "\x1\x5"
#define BROWN   "\x1\x6"
#define NORM    "\x1\x7"
#define GRAY    "\x1\x8"
#define BLUE    "\x1\x9"
#define BGREEN  "\x1\xa"
#define BCYAN   "\x1\xb"
#define BRED    "\x1\xc"
#define BPUR    "\x1\xd"
#define YEL     "\x1\xe"
#define YELLOW  "\x1\xe"
#define WHITE   "\x1\xf"

// hardware messages output prefix (GC hardware)
#define CP      CYAN "CP : "
#define PE      CYAN "PE : "
#define VI      CYAN "VI : "
#define GP      CYAN "GP : "
#define PI      CYAN "PI : "
#define CPU     CYAN "CPU: "
#define MI      CYAN "MI : "
#define DSP     CYAN "DSP: "
#define DI      CYAN "DI : "
#define AR      CYAN "AR : "
#define AI      CYAN "AI : "
#define AIS     CYAN "AIS: "
#define SI      CYAN "SI : "
#define EXI     CYAN "EXI: "
#define MC      CYAN "MC : "

// '\n' is supported (no auto-linefeed!)
extern  void (*DBHalt)(const char* text, ...);    // always breaks emulation
extern  void (*DBReport)(const char* text, ...);  // do debugger output

// exception trap
void    DBException(uint32_t code);

// start debugger loop, from PC
void    DBStart();
