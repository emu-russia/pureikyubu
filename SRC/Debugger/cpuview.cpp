// cpu view (disassembly)
#include "pch.h"

// ---------------------------------------------------------------------------

static int con_disa_line(int line, uint32_t opcode, uint32_t addr)
{
    PPCD_CB    disa;
    int bg, fg, bgcur, bgbp;
    char *symbol;
    int addend = 1;

    bgcur = (addr == con.disa_cursor) ? (8) : (0);
    //bgbp = (con_is_code_bp(addr)) ? (4) : (0);
    bgbp = 0;
    bg = (addr == PC) ? (1) : (0);
    bg = bg ^ bgcur ^ bgbp;
    fg = 7;
    con_attr(fg, bg);

    con_clear_line(line, con.attr);

    if((symbol = SYMName(addr)) != nullptr)
    {
        con_printf_at(0, line, GREEN "%s\n", symbol);
        line++;
        addend++;

        con_clear_line(line, con.attr);
    }

    if(opcode == 1 /* no memory */)
    {
        con_printf_at( 0, line, NORM "%08X  ", addr);
        con_printf_at(10, line, CYAN "%08X  ", 0);
        con_printf_at(20, line, NORM "???");
        return addend;
    }

    con_printf_at( 0, line, NORM "%08X  ", addr);
    con_printf_at(10, line, CYAN "%08X  ", opcode);

    disa.instr = opcode;
    disa.pc = addr;

    PPCDisasm (&disa);

    if(opcode == 0x4e800020 /* blr */)
    {
        // ignore other bclr/bcctr opcodes,
        // to easily locate end of function
        con_printf_at(20, line, GREEN "blr");
    }
    
    else if(disa.iclass & PPC_DISA_BRANCH)
    {
        con_printf_at(20, line, GREEN "%-12s%s", disa.mnemonic, disa.operands);
        if(disa.target > addr) con_printline(CYAN " \x19");
        else if (disa.target < addr) con_printline(CYAN " \x18");
        else con_printline(CYAN " \x1b");

        if((symbol = SYMName((uint32_t)disa.target)) != nullptr)
        {
            con_printf_at(47, line, BROWN " ; %s", symbol);
        }
    }
    
    else con_printf_at(20, line, NORM "%-12s%s", disa.mnemonic, disa.operands);

    if ((disa.iclass & PPC_DISA_INTEGER) && disa.mnemonic[0] == 'r' && disa.mnemonic[1] == 'l')
    {
        con_printf_at (60, line, NORM "mask:0x%08X", (uint32_t)disa.target);
    }

    return addend;
}

// show pointer to data
void con_ldst_info()
{
    PPCD_CB    disa;
    wind.ldst = FALSE;

    {
        uint32_t op;
        MEMFetch(con.disa_cursor & ~3, &op);

        disa.pc = con.disa_cursor;
        disa.instr = op;
        PPCDisasm (&disa);

        if(disa.iclass & PPC_DISA_LDST)
        {
            int ra = ((op >> 16) & 0x1f);
            int32_t simm = ((int32_t)(int16_t)(uint16_t)op);
            wind.ldst_disp = GPR[ra] + simm;
            con_attr(0, 3);
            con_fill_line(wind.disa_y);
            if(wind.focus == WDISA) con_print_at(0, wind.disa_y, WHITE "\x1f");
            con_attr(0, 3);
            con_print_at(2, wind.disa_y, "F3");
            con_printf_at(6, wind.disa_y, "<%08X> %s",
                wind.ldst_disp, SYMName(wind.ldst_disp));
            con_attr(7, 0);
            wind.ldst = TRUE;
        }
    }
}

void con_update_disa_window()
{
    int line;
    con_attr(0, 3);
    con_fill_line(wind.disa_y);
    if(wind.focus == WDISA) con_print_at(0, wind.disa_y, WHITE "\x1f");
    con_attr(0, 3);
    con_print_at(2, wind.disa_y, "F3");
    con_printf_at(
        6, wind.disa_y, 
        " cursor:%08X phys:%08X pc:%08X", 
        con.disa_cursor, MEMEffectiveToPhysical(con.disa_cursor, 0), PC);
    con_attr(7, 0);

    con_ldst_info();

    uint32_t op, addr = con.text & ~3;
    wind.disa_sub_h = 0;

    for(int line=wind.disa_y+1; line<wind.disa_y+wind.disa_h; line++, addr+=4)
    {
        MEMFetch(addr, &op);

        int n = con_disa_line(line, op, addr);
        if(n > 1) wind.disa_sub_h += n - 1;
        line += n - 1;
    }
}
