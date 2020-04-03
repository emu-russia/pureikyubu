// Condition Register Logical Instructions
#include "../pch.h"
#include "interpreter.h"

namespace Gekko
{

    // CR[crbd] = CR[crba] & CR[crbb]
    OP(CRAND)
    {
        uint32_t crbd = CRBD, crba = CRBA, crbb = CRBB;

        uint32_t a = (PPC_CR >> (31 - crba)) & 1;
        uint32_t b = (PPC_CR >> (31 - crbb)) & 1;
        uint32_t d = (a & b) << (31 - crbd);     // <- crop is here
        uint32_t m = ~(1 << (31 - crbd));
        PPC_CR = (PPC_CR & m) | d;
    }

    // CR[crbd] = CR[crba] | CR[crbb]
    OP(CROR)
    {
        uint32_t crbd = CRBD, crba = CRBA, crbb = CRBB;

        uint32_t a = (PPC_CR >> (31 - crba)) & 1;
        uint32_t b = (PPC_CR >> (31 - crbb)) & 1;
        uint32_t d = (a | b) << (31 - crbd);     // <- crop is here
        uint32_t m = ~(1 << (31 - crbd));
        PPC_CR = (PPC_CR & m) | d;
    }

    // CR[crbd] = CR[crba] ^ CR[crbb]
    OP(CRXOR)
    {
        uint32_t crbd = CRBD, crba = CRBA, crbb = CRBB;

        uint32_t a = (PPC_CR >> (31 - crba)) & 1;
        uint32_t b = (PPC_CR >> (31 - crbb)) & 1;
        uint32_t d = (a ^ b) << (31 - crbd);     // <- crop is here
        uint32_t m = ~(1 << (31 - crbd));
        PPC_CR = (PPC_CR & m) | d;
    }

    // CR[crbd] = !(CR[crba] & CR[crbb])
    OP(CRNAND)
    {
        uint32_t crbd = CRBD, crba = CRBA, crbb = CRBB;

        uint32_t a = (PPC_CR >> (31 - crba)) & 1;
        uint32_t b = (PPC_CR >> (31 - crbb)) & 1;
        uint32_t d = (!(a & b)) << (31 - crbd);     // <- crop is here
        uint32_t m = ~(1 << (31 - crbd));
        PPC_CR = (PPC_CR & m) | d;
    }

    // CR[crbd] = !(CR[crba] | CR[crbb])
    OP(CRNOR)
    {
        uint32_t crbd = CRBD, crba = CRBA, crbb = CRBB;

        uint32_t a = (PPC_CR >> (31 - crba)) & 1;
        uint32_t b = (PPC_CR >> (31 - crbb)) & 1;
        uint32_t d = (!(a | b)) << (31 - crbd);     // <- crop is here
        uint32_t m = ~(1 << (31 - crbd));
        PPC_CR = (PPC_CR & m) | d;
    }

    // CR[crbd] = CR[crba] EQV CR[crbb]
    OP(CREQV)
    {
        uint32_t crbd = CRBD, crba = CRBA, crbb = CRBB;

        uint32_t a = (PPC_CR >> (31 - crba)) & 1;
        uint32_t b = (PPC_CR >> (31 - crbb)) & 1;
        uint32_t d = (!(a ^ b)) << (31 - crbd);     // <- crop is here
        uint32_t m = ~(1 << (31 - crbd));
        PPC_CR = (PPC_CR & m) | d;
    }

    // CR[crbd] = CR[crba] & ~CR[crbb]
    OP(CRANDC)
    {
        uint32_t crbd = CRBD, crba = CRBA, crbb = CRBB;

        uint32_t a = (PPC_CR >> (31 - crba)) & 1;
        uint32_t b = (PPC_CR >> (31 - crbb)) & 1;
        uint32_t d = (a & (~b)) << (31 - crbd);     // <- crop is here
        uint32_t m = ~(1 << (31 - crbd));
        PPC_CR = (PPC_CR & m) | d;
    }

    // CR[crbd] = CR[crba] | ~CR[crbb]
    OP(CRORC)
    {
        uint32_t crbd = CRBD, crba = CRBA, crbb = CRBB;

        uint32_t a = (PPC_CR >> (31 - crba)) & 1;
        uint32_t b = (PPC_CR >> (31 - crbb)) & 1;
        uint32_t d = (a | (~b)) << (31 - crbd);     // <- crop is here
        uint32_t m = ~(1 << (31 - crbd));
        PPC_CR = (PPC_CR & m) | d;
    }

    // CR[4*crfd .. 4*crfd + 3] = CR[4*crfs .. 4*crfs + 3]
    OP(MCRF)
    {
        int32_t crfd = 4 * (7 - CRFD), crfs = 4 * (7 - CRFS);
        PPC_CR = (PPC_CR & (~(0xf << crfd))) | (((PPC_CR >> crfs) & 0xf) << crfd);
    }

}
