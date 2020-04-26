#pragma once

// define fifo pipeline callbacks

void __fastcall pos_idx();
void __fastcall t0_idx();
void __fastcall t1_idx();
void __fastcall t2_idx();
void __fastcall t3_idx();
void __fastcall t4_idx();
void __fastcall t5_idx();
void __fastcall t6_idx();
void __fastcall t7_idx();

extern void (__fastcall* posattr[4][2][5])();
extern void(__fastcall* nrmattr[4][3][5])();
extern void(__fastcall* col0attr[4][2][6])();
extern void(__fastcall* col1attr[4][2][6])();
extern void(__fastcall* tex0attr[4][2][5])();
extern void(__fastcall* tex1attr[4][2][5])();
extern void(__fastcall* tex2attr[4][2][5])();
extern void(__fastcall* tex3attr[4][2][5])();
extern void(__fastcall* tex4attr[4][2][5])();
extern void(__fastcall* tex5attr[4][2][5])();
extern void(__fastcall* tex6attr[4][2][5])();
extern void(__fastcall* tex7attr[4][2][5])();
