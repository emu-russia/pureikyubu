// Condition Register Logical Instructions
#include "dolphin.h"

#define OP(name) void __fastcall c_##name##(u32 op)

// CR[crbd] = CR[crba] & CR[crbb]
OP(CRAND)
{
    u32 crbd = CRBD, crba = CRBA, crbb = CRBB;

    u32 a = (CR >> (31 - crba)) & 1;
    u32 b = (CR >> (31 - crbb)) & 1;
    u32 d = (a & b) << (31 - crbd);     // <- crop is here
    u32 m = ~(1 << (31 - crbd));
    CR = (CR & m) | d;
}

// CR[crbd] = CR[crba] | CR[crbb]
OP(CROR)
{
    u32 crbd = CRBD, crba = CRBA, crbb = CRBB;

    u32 a = (CR >> (31 - crba)) & 1;
    u32 b = (CR >> (31 - crbb)) & 1;
    u32 d = (a | b) << (31 - crbd);     // <- crop is here
    u32 m = ~(1 << (31 - crbd));
    CR = (CR & m) | d;
}

// CR[crbd] = CR[crba] ^ CR[crbb]
OP(CRXOR)
{
    u32 crbd = CRBD, crba = CRBA, crbb = CRBB;

    u32 a = (CR >> (31 - crba)) & 1;
    u32 b = (CR >> (31 - crbb)) & 1;
    u32 d = (a ^ b) << (31 - crbd);     // <- crop is here
    u32 m = ~(1 << (31 - crbd));
    CR = (CR & m) | d;
}

// CR[crbd] = !(CR[crba] & CR[crbb])
OP(CRNAND)
{
    u32 crbd = CRBD, crba = CRBA, crbb = CRBB;

    u32 a = (CR >> (31 - crba)) & 1;
    u32 b = (CR >> (31 - crbb)) & 1;
    u32 d = (!(a & b)) << (31 - crbd);     // <- crop is here
    u32 m = ~(1 << (31 - crbd));
    CR = (CR & m) | d;
}

// CR[crbd] = !(CR[crba] | CR[crbb])
OP(CRNOR)
{
    u32 crbd = CRBD, crba = CRBA, crbb = CRBB;

    u32 a = (CR >> (31 - crba)) & 1;
    u32 b = (CR >> (31 - crbb)) & 1;
    u32 d = (!(a | b)) << (31 - crbd);     // <- crop is here
    u32 m = ~(1 << (31 - crbd));
    CR = (CR & m) | d;
}

// CR[crbd] = CR[crba] EQV CR[crbb]
OP(CREQV)
{
    u32 crbd = CRBD, crba = CRBA, crbb = CRBB;

    u32 a = (CR >> (31 - crba)) & 1;
    u32 b = (CR >> (31 - crbb)) & 1;
    u32 d = (!(a ^ b)) << (31 - crbd);     // <- crop is here
    u32 m = ~(1 << (31 - crbd));
    CR = (CR & m) | d;
}

// CR[crbd] = CR[crba] & ~CR[crbb]
OP(CRANDC)
{
    u32 crbd = CRBD, crba = CRBA, crbb = CRBB;

    u32 a = (CR >> (31 - crba)) & 1;
    u32 b = (CR >> (31 - crbb)) & 1;
    u32 d = (a & (~b)) << (31 - crbd);     // <- crop is here
    u32 m = ~(1 << (31 - crbd));
    CR = (CR & m) | d;
}

// CR[crbd] = CR[crba] | ~CR[crbb]
OP(CRORC)
{
    u32 crbd = CRBD, crba = CRBA, crbb = CRBB;

    u32 a = (CR >> (31 - crba)) & 1;
    u32 b = (CR >> (31 - crbb)) & 1;
    u32 d = (a | (~b)) << (31 - crbd);     // <- crop is here
    u32 m = ~(1 << (31 - crbd));
    CR = (CR & m) | d;
}

// CR[4*crfd .. 4*crfd + 3] = CR[4*crfs .. 4*crfs + 3]
OP(MCRF)
{
    s32 crfd = 4 * (7 - CRFD), crfs = 4 * (7 - CRFS);
    CR = (CR & (~(0xf << crfd))) | (((CR >> crfs) & 0xf) << crfd);
}
