// interpreter opcode tables
extern void (__fastcall *c_1[64])(u32 op);          // main
extern void (__fastcall *c_19[2048])(u32 op);       // 19
extern void (__fastcall *c_31[2048])(u32 op);       // 31
extern void (__fastcall *c_59[64])(u32 op);         // 59
extern void (__fastcall *c_63[2048])(u32 op);       // 63
extern void (__fastcall *c_4 [2048])(u32 op);       // 4

// setup extension tables 
void    IPTInitTables();
