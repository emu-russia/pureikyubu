#include "pch.h"

// bootrom descrambler reversed by segher
// Copyright 2008 Segher Boessenkool <segher@kernel.crashing.org>
void IPLDescrambler(uint8_t* data, size_t size)
{
    uint8_t acc = 0;
    uint8_t nacc = 0;

    uint16_t t = 0x2953;
    uint16_t u = 0xd9c2;
    uint16_t v = 0x3ff1;

    uint8_t x = 1;

    for (size_t it = 0; it < size;)
    {
        int t0 = t & 1;
        int t1 = (t >> 1) & 1;
        int u0 = u & 1;
        int u1 = (u >> 1) & 1;
        int v0 = v & 1;

        x ^= t1 ^ v0;
        x ^= (u0 | u1);
        x ^= (t0 ^ u1 ^ v0) & (t0 ^ u0);

        if (t0 == u0)
        {
            v >>= 1;
            if (v0)
            {
                v ^= 0xb3d0;
            }
        }

        if (t0 == 0)
        {
            u >>= 1;
            if (u0)
            {
                u ^= 0xfb10;
            }
        }

        t >>= 1;
        if (t0)
        {
            t ^= 0xa740;
        }

        nacc++;
        acc = 2 * acc + x;
        if (nacc == 8)
        {
            data[it++] ^= acc;
            nacc = 0;
        }
    }
}
