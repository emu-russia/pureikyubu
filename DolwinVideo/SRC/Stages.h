// define fifo pipeline callbacks

uint8_t* __fastcall pos_idx(uint8_t* ptr);
uint8_t* __fastcall t0_idx(uint8_t* ptr);
uint8_t* __fastcall t1_idx(uint8_t* ptr);
uint8_t* __fastcall t2_idx(uint8_t* ptr);
uint8_t* __fastcall t3_idx(uint8_t* ptr);
uint8_t* __fastcall t4_idx(uint8_t* ptr);
uint8_t* __fastcall t5_idx(uint8_t* ptr);
uint8_t* __fastcall t6_idx(uint8_t* ptr);
uint8_t* __fastcall t7_idx(uint8_t* ptr);

extern uint8_t* (__fastcall* posattr[4][2][5])(uint8_t*);
extern uint8_t* (__fastcall* nrmattr[4][3][5])(uint8_t*);
extern uint8_t* (__fastcall* col0attr[4][2][6])(uint8_t*);
extern uint8_t* (__fastcall* col1attr[4][2][6])(uint8_t*);
extern uint8_t* (__fastcall* tex0attr[4][2][5])(uint8_t*);
extern uint8_t* (__fastcall* tex1attr[4][2][5])(uint8_t*);
extern uint8_t* (__fastcall* tex2attr[4][2][5])(uint8_t*);
extern uint8_t* (__fastcall* tex3attr[4][2][5])(uint8_t*);
extern uint8_t* (__fastcall* tex4attr[4][2][5])(uint8_t*);
extern uint8_t* (__fastcall* tex5attr[4][2][5])(uint8_t*);
extern uint8_t* (__fastcall* tex6attr[4][2][5])(uint8_t*);
extern uint8_t* (__fastcall* tex7attr[4][2][5])(uint8_t*);
