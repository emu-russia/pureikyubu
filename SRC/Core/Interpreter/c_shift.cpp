// Integer Shift Instructions
#include "../pch.h"
#include "interpreter.h"

namespace Gekko
{

    // n = rb[27-31]
    // r = ROTL(rs, n)
    // if rb[26] = 0
    // then m = MASK(0, 31-n)
    // else m = (32)0
    // ra = r & m
    // (simply : ra = rs << rb, or ra = 0, if rb[26] = 1)
    OP(SLW)
    {
        uint32_t n = RRB;

        if (n & 0x20) RRA = 0;
        else RRA = RRS << (n & 0x1f);
    }

    // n = rb[27-31]
    // r = ROTL(rs, n)
    // if rb[26] = 0
    // then m = MASK(0, 31-n)
    // else m = (32)0
    // ra = r & m
    // (simply : ra = rs << rb, or ra = 0, if rb[26] = 1)
    // CR0
    OP(SLWD)
    {
        uint32_t n = RRB;
        uint32_t res;

        if (n & 0x20) res = 0;
        else res = RRS << (n & 0x1f);

        RRA = res;
        COMPUTE_CR0(res);
    }

    // n = rb[27-31]
    // r = ROTL(rs, 32-n)
    // if rb[26] = 0
    // then m = MASK(n, 31)
    // else m = (32)0
    // ra = r & m
    // (simply : ra = rs >> rb, or ra = 0, if rb[26] = 1)
    OP(SRW)
    {
        uint32_t n = RRB;

        if (n & 0x20) RRA = 0;
        else RRA = RRS >> (n & 0x1f);
    }

    // n = rb[27-31]
    // r = ROTL(rs, 32-n)
    // if rb[26] = 0
    // then m = MASK(n, 31)
    // else m = (32)0
    // ra = r & m
    // (simply : ra = rs >> rb, or ra = 0, if rb[26] = 1)
    // CR0
    OP(SRWD)
    {
        uint32_t n = RRB;
        uint32_t res;

        if (n & 0x20) res = 0;
        else res = RRS >> (n & 0x1f);

        RRA = res;
        COMPUTE_CR0(res);
    }

    // n = SH
    // r = ROTL(rs, 32 - n)
    // m = MASK(n, 31)
    // sign = rs[0]
    // ra = r & m | (32)sign & ~m
    // XER[CA] = sign(0) & ((r & ~m) != 0)
    OP(SRAWI)
    {
        uint32_t n = SH;
        int32_t res;
        int32_t src = RRS;

        if (n == 0)
        {
            res = src;
            RESET_XER_CA;
        }
        else
        {
            res = src >> n;
            if (src < 0 && (src << (32 - n)) != 0) SET_XER_CA; else RESET_XER_CA;
        }

        RRA = res;
    }

    // n = SH
    // r = ROTL(rs, 32 - n)
    // m = MASK(n, 31)
    // sign = rs[0]
    // ra = r & m | (32)sign & ~m
    // XER[CA] = sign(0) & ((r & ~m) != 0)
    // CR0
    OP(SRAWID)
    {
        uint32_t n = SH;
        int32_t res;
        int32_t src = RRS;

        if (n == 0)
        {
            res = src;
            RESET_XER_CA;
        }
        else
        {
            res = src >> n;
            if (src < 0 && (src << (32 - n)) != 0) SET_XER_CA; else RESET_XER_CA;
        }

        RRA = res;
        COMPUTE_CR0(res);
    }

    // n = rb[27-31]
    // r = ROTL(rs, 32-n)
    // if rb[26] = 0
    // then m = MASK(n, 31)
    // else m = (32)0
    // S = rs(0)
    // ra = r & m | (32)S & ~m
    // XER[CA] = S & (r & ~m[0-31] != 0)
    OP(SRAW)
    {
        uint32_t n = RRB;
        int32_t res;
        int32_t src = RRS;

        if (n == 0)
        {
            res = src;
            RESET_XER_CA;
        }
        else if (n & 0x20)
        {
            if (src < 0)
            {
                res = 0xffffffff;
                if (src & 0x7fffffff) SET_XER_CA; else RESET_XER_CA;
            }
            else
            {
                res = 0;
                RESET_XER_CA;
            }
        }
        else
        {
            n = n & 0x1f;
            res = (int32_t)src >> n;
            if (src < 0 && (src << (32 - n)) != 0) SET_XER_CA; else RESET_XER_CA;
        }

        RRA = res;
    }

    // n = rb[27-31]
    // r = ROTL(rs, 32-n)
    // if rb[26] = 0
    // then m = MASK(n, 31)
    // else m = (32)0
    // S = rs(0)
    // ra = r & m | (32)S & ~m
    // XER[CA] = S & (r & ~m[0-31] != 0)
    // CR0
    OP(SRAWD)
    {
        uint32_t n = RRB;
        int32_t res;
        int32_t src = RRS;

        if (n == 0)
        {
            res = src;
            RESET_XER_CA;
        }
        else if (n & 0x20)
        {
            if (src < 0)
            {
                res = 0xffffffff;
                if (src & 0x7fffffff) SET_XER_CA; else RESET_XER_CA;
            }
            else
            {
                res = 0;
                RESET_XER_CA;
            }
        }
        else
        {
            n = n & 0x1f;
            res = (int32_t)src >> n;
            if (src < 0 && (src << (32 - n)) != 0) SET_XER_CA; else RESET_XER_CA;
        }

        RRA = res;
        COMPUTE_CR0(res);
    }

}
