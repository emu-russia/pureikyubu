// breakpoint
typedef struct DBPoint
{
    BOOL    mem;        // breakpoint type. 1:memory, 0:pc
    u32     ea;         // effective address of breakpoint
    s32     dlen;       // data length to break : 8, 16, 32 or 64
} DBPoint;

void    con_bp_list();
DBPoint* con_allocate_bp();
void    con_rem_bp(s32 num);
DBPoint* con_is_code_bp(u32 addr);
void    con_add_code_bp(u32 addr);
void    con_rem_code_bp(u32 addr);
DBPoint* con_is_data_bp(u32 addr, s32 dlen);
void    con_add_data_bp(u32 addr, s32 dlen);
void    con_rem_data_bp(u32 addr, s32 dlen);
void    con_rem_all_bp();

void    con_run_execute();
void    con_step_over();

// exception trap
void    DBException(u32 code);

// debugger traps for CPU memory operations
void __fastcall DBReadByte(u32 addr, u32 *reg);
void __fastcall DBWriteByte(u32 addr, u32 data);
void __fastcall DBReadHalf(u32 addr, u32 *reg);
void __fastcall DBReadHalfS(u32 addr, u32 *reg);
void __fastcall DBWriteHalf(u32 addr, u32 data);
void __fastcall DBReadWord(u32 addr, u32 *reg);
void __fastcall DBWriteWord(u32 addr, u32 data);
void __fastcall DBReadDouble(u32 addr, u64 *reg);
void __fastcall DBWriteDouble(u32 addr, u64 *data);
u32  __fastcall DBFetch(u32 addr);
