
#pragma once

// vertex attributes
typedef enum
{
    VTX_POSMATIDX = 0,      // Position Matrix Index
    VTX_TEX0MTXIDX,         // Texture Coordinate 0 Matrix Index
    VTX_TEX1MTXIDX,         // Texture Coordinate 1 Matrix Index
    VTX_TEX2MTXIDX,         // Texture Coordinate 2 Matrix Index
    VTX_TEX3MTXIDX,         // Texture Coordinate 3 Matrix Index
    VTX_TEX4MTXIDX,         // Texture Coordinate 4 Matrix Index
    VTX_TEX5MTXIDX,         // Texture Coordinate 5 Matrix Index
    VTX_TEX6MTXIDX,         // Texture Coordinate 6 Matrix Index
    VTX_TEX7MTXIDX,         // Texture Coordinate 7 Matrix Index
    VTX_POS,                // Position
    VTX_NRM,                // Normal or Normal/Binormal/Tangent
    VTX_COLOR0,             // Color 0
    VTX_COLOR1,             // Color 1
    VTX_TEXCOORD0,          // Texture Coordinate 0
    VTX_TEXCOORD1,          // Texture Coordinate 1
    VTX_TEXCOORD2,          // Texture Coordinate 2
    VTX_TEXCOORD3,          // Texture Coordinate 3
    VTX_TEXCOORD4,          // Texture Coordinate 4
    VTX_TEXCOORD5,          // Texture Coordinate 5
    VTX_TEXCOORD6,          // Texture Coordinate 6
    VTX_TEXCOORD7,          // Texture Coordinate 7
    VTX_MAX_ATTR
} VTX_ATTR;

// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// GP fifo commands
#define OP_CMD_NOP              0x00
#define OP_CMD_INV              0x48
#define OP_CMD_CALL_DL          0x40
#define OP_CMD_LOAD_BPREG       0x61
#define OP_CMD_LOAD_CPREG       0x08
#define OP_CMD_LOAD_XFREG       0x10
#define OP_CMD_LOAD_INDXA       0x20
#define OP_CMD_LOAD_INDXB       0x28
#define OP_CMD_LOAD_INDXC       0x30
#define OP_CMD_LOAD_INDXD       0x38
#define OP_CMD_DRAW_QUAD        0x80
#define OP_CMD_DRAW_TRIANGLE    0x90
#define OP_CMD_DRAW_STRIP       0x98
#define OP_CMD_DRAW_FAN         0xA0
#define OP_CMD_DRAW_LINE        0xA8
#define OP_CMD_DRAW_LINESTRIP   0xB0
#define OP_CMD_DRAW_POINT       0xB8

// -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

extern  uint32_t     lastFifoSize;

// for gpregs module
// called after any changes of VCD / VAT
void FifoReconfigure(
    VTX_ATTR    attr,       // stage attribute
    unsigned    vat,        // vat number
    unsigned    vcd,        // attribute description
    unsigned    cnt,        // attribute "cnt"
    unsigned    fmt,        // attribute "fmt"
    unsigned    frac);

extern uint8_t  accum[1024*1024+32];// primitive accumulation buffer
extern uint8_t  *accptr;             // current offset in accum
extern size_t   acclen;             // length of accumulated data
extern uint8_t  cmdidle;
