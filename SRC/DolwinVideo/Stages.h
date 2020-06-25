#pragma once

// define fifo pipeline callbacks

void pos_idx(GX::FifoProcessor* fifo);
void t0_idx(GX::FifoProcessor* fifo);
void t1_idx(GX::FifoProcessor* fifo);
void t2_idx(GX::FifoProcessor* fifo);
void t3_idx(GX::FifoProcessor* fifo);
void t4_idx(GX::FifoProcessor* fifo);
void t5_idx(GX::FifoProcessor* fifo);
void t6_idx(GX::FifoProcessor* fifo);
void t7_idx(GX::FifoProcessor* fifo);

extern void (*posattr[4][2][5])(GX::FifoProcessor* fifo);
extern void (*nrmattr[4][3][5])(GX::FifoProcessor* fifo);
extern void (*col0attr[4][2][6])(GX::FifoProcessor* fifo);
extern void (*col1attr[4][2][6])(GX::FifoProcessor* fifo);
extern void (*tex0attr[4][2][5])(GX::FifoProcessor* fifo);
extern void (*tex1attr[4][2][5])(GX::FifoProcessor* fifo);
extern void (*tex2attr[4][2][5])(GX::FifoProcessor* fifo);
extern void (*tex3attr[4][2][5])(GX::FifoProcessor* fifo);
extern void (*tex4attr[4][2][5])(GX::FifoProcessor* fifo);
extern void (*tex5attr[4][2][5])(GX::FifoProcessor* fifo);
extern void (*tex6attr[4][2][5])(GX::FifoProcessor* fifo);
extern void (*tex7attr[4][2][5])(GX::FifoProcessor* fifo);
