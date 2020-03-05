// Integer Compare Instructions
#include "dolphin.h"

#define OP(name) void __fastcall c_##name##(uint32_t op)

#define SET_CR_LT(n)    (CR |=  (1 << (3 + 4 * (7 - n))))
#define SET_CR_GT(n)    (CR |=  (1 << (2 + 4 * (7 - n))))
#define SET_CR_EQ(n)    (CR |=  (1 << (1 + 4 * (7 - n))))
#define SET_CR_SO(n)    (CR |=  (1 << (    4 * (7 - n))))
#define RESET_CR_LT(n)  (CR &= ~(1 << (3 + 4 * (7 - n))))
#define RESET_CR_GT(n)  (CR &= ~(1 << (2 + 4 * (7 - n))))
#define RESET_CR_EQ(n)  (CR &= ~(1 << (1 + 4 * (7 - n))))
#define RESET_CR_SO(n)  (CR &= ~(1 << (    4 * (7 - n))))

#define IS_XER_SO       (XER & (1 << 31))
#define IS_XER_OV       (XER & (1 << 30))
#define IS_XER_CA       (XER & (1 << 29))

// a = ra (signed)
// b = SIMM
// if a < b
//      then c = 0b100
//      else if a > b
//          then c = 0b010
//          else c = 0b001
// CR[4*crf..4*crf+3] = c || XER[SO]
OP(CMPI)
{
    int32_t a = RRA, b = SIMM;
    int crfd = CRFD;
    if(a < b) SET_CR_LT(crfd); else RESET_CR_LT(crfd);
    if(a > b) SET_CR_GT(crfd); else RESET_CR_GT(crfd);
    if(a == b) SET_CR_EQ(crfd); else RESET_CR_EQ(crfd);
    if(IS_XER_SO) SET_CR_SO(crfd); else RESET_CR_SO(crfd);
}

// a = ra (signed)
// b = rb (signed)
// if a < b
//      then c = 0b100
//      else if a > b
//          then c = 0b010
//          else c = 0b001
// CR[4*crf..4*crf+3] = c || XER[SO]
OP(CMP)
{
    int32_t a = RRA, b = RRB;
    int crfd = CRFD;
    if(a < b) SET_CR_LT(crfd); else RESET_CR_LT(crfd);
    if(a > b) SET_CR_GT(crfd); else RESET_CR_GT(crfd);
    if(a == b) SET_CR_EQ(crfd); else RESET_CR_EQ(crfd);
    if(IS_XER_SO) SET_CR_SO(crfd); else RESET_CR_SO(crfd);
}

// a = ra (unsigned)
// b = 0x0000 || UIMM
// if a < b
//      then c = 0b100
//      else if a > b
//          then c = 0b010
//          else c = 0b001
// CR[4*crf..4*crf+3] = c || XER[SO]
OP(CMPLI)
{
    uint32_t a = RRA, b = UIMM;
    int crfd = CRFD;
    if(a < b) SET_CR_LT(crfd); else RESET_CR_LT(crfd);
    if(a > b) SET_CR_GT(crfd); else RESET_CR_GT(crfd);
    if(a == b) SET_CR_EQ(crfd); else RESET_CR_EQ(crfd);
    if(IS_XER_SO) SET_CR_SO(crfd); else RESET_CR_SO(crfd);
}

// a = ra (unsigned)
// b = rb (unsigned)
// if a < b
//      then c = 0b100
//      else if a > b
//          then c = 0b010
//          else c = 0b001
// CR[4*crf..4*crf+3] = c || XER[SO]
OP(CMPL)
{
    uint32_t a = RRA;
    uint32_t b = RRB;
    int crfd = CRFD;
    if(a < b) SET_CR_LT(crfd); else RESET_CR_LT(crfd);
    if(a > b) SET_CR_GT(crfd); else RESET_CR_GT(crfd);
    if(a == b) SET_CR_EQ(crfd); else RESET_CR_EQ(crfd);
    if(IS_XER_SO) SET_CR_SO(crfd); else RESET_CR_SO(crfd);
}
