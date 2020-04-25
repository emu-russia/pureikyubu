
#pragma once 

// GX fifo buffer
#define GX_FIFO         0x0C008000

void GXFifoWriteBurst(uint8_t data[32]);
