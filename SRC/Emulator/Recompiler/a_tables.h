// recompiler opcode tables
extern void (__fastcall *a_1[64])(u32 op, u32 pc);     // main
extern void (__fastcall *a_19[2048])(u32 op, u32 pc);  // 19
extern void (__fastcall *a_31[2048])(u32 op, u32 pc);  // 31
extern void (__fastcall *a_59[64])(u32 op, u32 pc);    // 59
extern void (__fastcall *a_63[2048])(u32 op, u32 pc);  // 63
extern void (__fastcall *a_4 [2048])(u32 op, u32 pc);  // 4

// setup recompiler tables 
void    RECInitTables();
