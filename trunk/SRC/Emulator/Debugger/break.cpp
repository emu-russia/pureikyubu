// breakpoints
#include "dolphin.h"

// ---------------------------------------------------------------------------
// breakpoints controls

void con_bp_list()
{
    for(s32 i=0; i<con.brknum; i++)
    {
        con_print("\t%i: %c %08X %i\n",
            i,
            (con.brks[i].mem) ? ('D') : ('I'),
            con.brks[i].ea,
            con.brks[i].dlen
        );
    }
}

DBPoint * con_allocate_bp()
{
    if(con.brknum) con.brks = (DBPoint *)realloc(con.brks, (con.brknum + 1) * sizeof(DBPoint));
    else con.brks = (DBPoint *)malloc(sizeof(DBPoint));
    ASSERT(con.brks == NULL, "No space for new breakpoint!");

    return &con.brks[con.brknum];
}

void con_rem_bp(s32 num)
{
    int i;
    if(num >= con.brknum) return;

    DBPoint * ptr = (DBPoint *)malloc((con.brknum - 1) * sizeof(DBPoint));
    ASSERT(ptr == NULL, "Cannot kill breakpoint.");
    for(i=0; i<num; i++)
    {
        ptr[i] = con.brks[i];
    }
    for(i=num+1; i<con.brknum; i++)
    {
        ptr[i-1] = con.brks[i];
    }

    free(con.brks);
    con.brks = ptr;
    con.brknum--;
}

DBPoint * con_is_code_bp(u32 addr)
{
    for(s32 i=0; i<con.brknum; i++)
    {
        if((con.brks[i].ea == addr) && !con.brks[i].mem)
        {
            return &con.brks[i];
        }
    }
    return NULL;
}

void con_add_code_bp(u32 addr)
{
    DBPoint * brk = con_is_code_bp(addr);
    if(brk) return;
    else brk = con_allocate_bp();

    brk->mem = 0;
    brk->ea = addr;

    con.brknum++;
}

void con_rem_code_bp(u32 addr)
{
    for(s32 i=0; i<con.brknum; i++)
    {
        if((con.brks[i].ea == addr) && !con.brks[i].mem)
        {
            con_rem_bp(i);
        }
    }
}

DBPoint * con_is_data_bp(u32 addr, s32 dlen)
{
    for(s32 i=0; i<con.brknum; i++)
    {
        if((con.brks[i].dlen == dlen) && (con.brks[i].ea == addr) && con.brks[i].mem)
        {
            return &con.brks[i];
        }
    }
    return NULL;
}

void con_add_data_bp(u32 addr, s32 dlen)
{
    DBPoint * brk = con_is_data_bp(addr, dlen);
    if(brk) return;
    else brk = con_allocate_bp();

    brk->mem = 1;
    brk->ea = addr;
    brk->dlen = dlen;

    con.brknum++;
}

void con_rem_data_bp(u32 addr, s32 dlen)
{
    for(s32 i=0; i<con.brknum; i++)
    {
        if((con.brks[i].dlen == dlen) && (con.brks[i].ea == addr) && con.brks[i].mem)
        {
            con_rem_bp(i);
        }
    }
}

void con_rem_all_bp()
{
    if(con.brks)
    {
        free(con.brks);
        con.brks = NULL;
    }
    con.brknum = 0;
}

// ---------------------------------------------------------------------------
// execution

// start execute (F5)
void con_run_execute()
{
    s64 old = TBR;
    con.running = TRUE;

    while(1)
    {
        IPTExecuteOpcode();

        if((TBR - old) >= 30000)
        {
            old = TBR;

            con_read_input(1);
            con_set_disa_cur(PC);
            con_update(CON_UPDATE_ALL);
            con_refresh();
        }

        if(con_is_code_bp(PC)) con_break(" (PC breakpoint)");
    }
}

// step over function
void con_step_over()
{
    s64 old = TBR;
    u32 stop_addr = PC + 4;

    while(1)
    {
        IPTExecuteOpcode();

        if(PC == stop_addr)
        {
            con_set_disa_cur(PC);
            con_update(CON_UPDATE_ALL);
            con_refresh();
            break;
        }

        if((TBR - old) >= 30000)
        {
            old = TBR;

            con_read_input(1);
            con_set_disa_cur(PC);
            con_update(CON_UPDATE_ALL);
            con_refresh();
        }

        if(con_is_code_bp(PC)) con_break(" (PC breakpoint)");
    }
}

// ---------------------------------------------------------------------------
// exception trap

void DBException(u32 code)
{
    IPTException(code);
}

// ---------------------------------------------------------------------------
// memory traps

void __fastcall DBReadByte(u32 addr, u32 *reg)
{
    if(con.running)
    {
    }
    MEMReadByte(addr, reg);
}

void __fastcall DBWriteByte(u32 addr, u32 data)
{
    if(con.running)
    {
    }
    MEMWriteByte(addr, data);
}

void __fastcall DBReadHalf(u32 addr, u32 *reg)
{
    if(con.running)
    {
    }
    MEMReadHalf(addr, reg);
}

void __fastcall DBReadHalfS(u32 addr, u32 *reg)
{
    if(con.running)
    {
    }
    MEMReadHalfS(addr, reg);
}

void __fastcall DBWriteHalf(u32 addr, u32 data)
{
    if(con.running)
    {
    }
    MEMWriteHalf(addr, data);
}

void __fastcall DBReadWord(u32 addr, u32 *reg)
{
    if(con.running)
    {
    }
    MEMReadWord(addr, reg);
}

void __fastcall DBWriteWord(u32 addr, u32 data)
{
    if(con.running)
    {
    }
    MEMWriteWord(addr, data);
}

void __fastcall DBReadDouble(u32 addr, u64 *reg)
{
    if(con.running)
    {
    }
    MEMReadDouble(addr, reg);
}

void __fastcall DBWriteDouble(u32 addr, u64 *data)
{
    if(con.running)
    {
    }
    MEMWriteDouble(addr, data);
}
