// interpreter opcode tables
extern void (__fastcall *c_1[64])(uint32_t op);          // main
extern void (__fastcall *c_19[2048])(uint32_t op);       // 19
extern void (__fastcall *c_31[2048])(uint32_t op);       // 31
extern void (__fastcall *c_59[64])(uint32_t op);         // 59
extern void (__fastcall *c_63[2048])(uint32_t op);       // 63
extern void (__fastcall *c_4 [2048])(uint32_t op);       // 4

// setup extension tables 
void    IPTInitTables();
