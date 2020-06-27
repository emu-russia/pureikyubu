#ifndef __AMCSTUBS_H__
#define __AMCSTUBS_H__

void EXI2_Init();
void EXI2_EnableInterrupts();
void EXI2_Poll();
void EXI2_ReadN();
void EXI2_WriteN();
void EXI2_Reserve();
void EXI2_Unreserve();
void OSRegisterVersion(int ver);

#endif  // __AMCSTUBS_H__
