#pragma once

// CP registers. CPU accessing CP regs by 16-bit reads and writes
#define CP_SR           0x0C000000      // status register
#define CP_CR           0x0C000002      // control register
#define CP_CLR          0x0C000004      // clear register
#define CP_TEX          0x0C000006      // something used for TEX units setup
#define CP_BASE         0x0C000020      // GP fifo base
#define CP_TOP          0x0C000024      // GP fifo top
#define CP_HIWMARK      0x0C000028      // FIFO high water count
#define CP_LOWMARK      0x0C00002C      // FIFO low water count
#define CP_CNT          0x0C000030      // FIFO_COUNT (entries currently in FIFO)
#define CP_WRPTR        0x0C000034      // GP FIFO write pointer
#define CP_RDPTR        0x0C000038      // GP FIFO read pointer
#define CP_BPPTR        0x0C00003C      // GP FIFO read address break point

void CP_PEOpen();
void CP_PEClose();
