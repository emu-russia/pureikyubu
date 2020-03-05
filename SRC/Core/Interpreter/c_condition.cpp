// Condition Register Logical Instructions
#include "dolphin.h"

#define OP(name) void __fastcall c_##name##(uint32_t op)

// CR[crbd] = CR[crba] & CR[crbb]
OP(CRAND)
{
    uint32_t crbd = CRBD, crba = CRBA, crbb = CRBB;

    uint32_t a = (CR >> (31 - crba)) & 1;
    uint32_t b = (CR >> (31 - crbb)) & 1;
    uint32_t d = (a & b) << (31 - crbd);     // <- crop is here
    uint32_t m = ~(1 << (31 - crbd));
    CR = (CR & m) | d;
}

// CR[crbd] = CR[crba] | CR[crbb]
OP(CROR)
{
    uint32_t crbd = CRBD, crba = CRBA, crbb = CRBB;

    uint32_t a = (CR >> (31 - crba)) & 1;
    uint32_t b = (CR >> (31 - crbb)) & 1;
    uint32_t d = (a | b) << (31 - crbd);     // <- crop is here
    uint32_t m = ~(1 << (31 - crbd));
    CR = (CR & m) | d;
}

// CR[crbd] = CR[crba] ^ CR[crbb]
OP(CRXOR)
{
    uint32_t crbd = CRBD, crba = CRBA, crbb = CRBB;

    uint32_t a = (CR >> (31 - crba)) & 1;
    uint32_t b = (CR >> (31 - crbb)) & 1;
    uint32_t d = (a ^ b) << (31 - crbd);     // <- crop is here
    uint32_t m = ~(1 << (31 - crbd));
    CR = (CR & m) | d;
}

// CR[crbd] = !(CR[crba] & CR[crbb])
OP(CRNAND)
{
    uint32_t crbd = CRBD, crba = CRBA, crbb = CRBB;

    uint32_t a = (CR >> (31 - crba)) & 1;
    uint32_t b = (CR >> (31 - crbb)) & 1;
    uint32_t d = (!(a & b)) << (31 - crbd);     // <- crop is here
    uint32_t m = ~(1 << (31 - crbd));
    CR = (CR & m) | d;
}

// CR[crbd] = !(CR[crba] | CR[crbb])
OP(CRNOR)
{
    uint32_t crbd = CRBD, crba = CRBA, crbb = CRBB;

    uint32_t a = (CR >> (31 - crba)) & 1;
    uint32_t b = (CR >> (31 - crbb)) & 1;
    uint32_t d = (!(a | b)) << (31 - crbd);     // <- crop is here
    uint32_t m = ~(1 << (31 - crbd));
    CR = (CR & m) | d;
}

// CR[crbd] = CR[crba] EQV CR[crbb]
OP(CREQV)
{
    uint32_t crbd = CRBD, crba = CRBA, crbb = CRBB;

    uint32_t a = (CR >> (31 - crba)) & 1;
    uint32_t b = (CR >> (31 - crbb)) & 1;
    uint32_t d = (!(a ^ b)) << (31 - crbd);     // <- crop is here
    uint32_t m = ~(1 << (31 - crbd));
    CR = (CR & m) | d;
}

// CR[crbd] = CR[crba] & ~CR[crbb]
OP(CRANDC)
{
    uint32_t crbd = CRBD, crba = CRBA, crbb = CRBB;

    uint32_t a = (CR >> (31 - crba)) & 1;
    uint32_t b = (CR >> (31 - crbb)) & 1;
    uint32_t d = (a & (~b)) << (31 - crbd);     // <- crop is here
    uint32_t m = ~(1 << (31 - crbd));
    CR = (CR & m) | d;
}

// CR[crbd] = CR[crba] | ~CR[crbb]
OP(CRORC)
{
    uint32_t crbd = CRBD, crba = CRBA, crbb = CRBB;

    uint32_t a = (CR >> (31 - crba)) & 1;
    uint32_t b = (CR >> (31 - crbb)) & 1;
    uint32_t d = (a | (~b)) << (31 - crbd);     // <- crop is here
    uint32_t m = ~(1 << (31 - crbd));
    CR = (CR & m) | d;
}

// CR[4*crfd .. 4*crfd + 3] = CR[4*crfs .. 4*crfs + 3]
OP(MCRF)
{
    int32_t crfd = 4 * (7 - CRFD), crfs = 4 * (7 - CRFS);
    CR = (CR & (~(0xf << crfd))) | (((CR >> crfs) & 0xf) << crfd);
}
