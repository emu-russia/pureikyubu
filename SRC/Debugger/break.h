// breakpoint
typedef struct DBPoint
{
    bool        mem;        // breakpoint type. 1:memory, 0:pc
    uint32_t    ea;         // effective address of breakpoint
    int         dlen;       // data length to break : 8, 16, 32 or 64
} DBPoint;

void    con_bp_list();
DBPoint* con_allocate_bp();
void    con_rem_bp(int num);
DBPoint* con_is_code_bp(uint32_t addr);
void    con_add_code_bp(uint32_t addr);
void    con_rem_code_bp(uint32_t addr);
DBPoint* con_is_data_bp(uint32_t addr, int dlen);
void    con_add_data_bp(uint32_t addr, int dlen);
void    con_rem_data_bp(uint32_t addr, int dlen);
void    con_rem_all_bp();

void    con_run_execute();
void    con_step_into();
void    con_step_over();

// exception trap
void    DBException(uint32_t code);

// debugger traps for CPU memory operations
void __fastcall DBReadByte(uint32_t addr, uint32_t*reg);
void __fastcall DBWriteByte(uint32_t addr, uint32_t data);
void __fastcall DBReadHalf(uint32_t addr, uint32_t*reg);
void __fastcall DBReadHalfS(uint32_t addr, uint32_t*reg);
void __fastcall DBWriteHalf(uint32_t addr, uint32_t data);
void __fastcall DBReadWord(uint32_t addr, uint32_t*reg);
void __fastcall DBWriteWord(uint32_t addr, uint32_t data);
void __fastcall DBReadDouble(uint32_t addr, uint64_t*reg);
void __fastcall DBWriteDouble(uint32_t addr, uint64_t*data);
uint32_t  __fastcall DBFetch(uint32_t addr);
