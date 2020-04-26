// This module handles all the magic that occurs when writing to GX FIFO.

#include "pch.h"

void GXFifoWriteBurst(uint8_t data[32])
{
    // PI FIFO

    pi.wrptr &= ~PI_WRPTR_WRAP;

    memcpy(&mi.ram[pi.wrptr & RAMMASK], data, 32);
    pi.wrptr += 32;

    if (pi.wrptr == pi.top)
    {
        pi.wrptr = pi.base;
        pi.wrptr |= PI_WRPTR_WRAP;
    }
    
    // CP FIFO

    if (fifo.cp.cr & CP_CR_WPINC)
    {
        fifo.cp.wrptr += 32;

        if (fifo.cp.wrptr == fifo.cp.top)
        {
            fifo.cp.wrptr = fifo.cp.base;
        }

        // All other work is done by CPThread.
    }
}
