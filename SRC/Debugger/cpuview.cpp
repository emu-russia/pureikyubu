// cpu view (disassembly)
#include "pch.h"

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

    symbol = SYMName(addr);
    if(symbol)
    {
        con_printf_at(0, line, "\x1%c%s\n", ConColor::GREEN, symbol);
        line++;
        addend++;

        con_clear_line(line, con.attr);
    }

    if(opcode == 1 /* no memory */)
    {
        con_printf_at( 0, line, "\x1%c%08X  ", ConColor::NORM, addr);
        con_printf_at(10, line, "\x1%c%08X  ", ConColor::CYAN, 0);
        con_printf_at(20, line, "\x1%c???", ConColor::NORM);
        return addend;
    }

    con_printf_at( 0, line, "\x1%c%08X  ", ConColor::NORM, addr);
    con_printf_at(10, line, "\x1%c%08X  ", ConColor::NORM, opcode);

    disa.instr = opcode;
    disa.pc = addr;

    PPCDisasm (&disa);

    if(opcode == 0x4e800020 /* blr */)
    {
        // ignore other bclr/bcctr opcodes,
        // to easily locate end of function
        con_printf_at(20, line, "\x1%cblr", ConColor::GREEN);
    }
    
    else if(disa.iclass & PPC_DISA_BRANCH)
    {
        const char* dir;

        if (disa.target > addr) dir = " \x19";
        else if (disa.target < addr) dir = " \x18";
        else dir = " \x1b";

        con_printf_at(20, line, "\x1%c%-12s%s\x1%c%s", 
            ConColor::GREEN, disa.mnemonic, disa.operands, ConColor::CYAN, dir);

        symbol = SYMName((uint32_t)disa.target);
        if(symbol)
        {
            con_printf_at(47, line, "\x1%c ; %s", ConColor::BROWN, symbol);
        }
    }
    
    else con_printf_at(20, line, "\x1%c%-12s%s", ConColor::NORM, disa.mnemonic, disa.operands);

    if ((disa.iclass & PPC_DISA_INTEGER) && disa.mnemonic[0] == 'r' && disa.mnemonic[1] == 'l')
    {
        con_printf_at (60, line, "\x1%cmask:0x%08X", ConColor::NORM, (uint32_t)disa.target);
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
            con_fill_line(wind.disa_y, 0xc4);
            if(wind.focus == WDISA) con_printf_at(0, wind.disa_y, "\x1%c\x1f", ConColor::WHITE);
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
    uint32_t pa = -1;
    if (emu.core)
    {
        pa = emu.core->EffectiveToPhysical(con.disa_cursor, true);
    }

    con_attr(0, 3);
    con_fill_line(wind.disa_y, 0xc4);
    if(wind.focus == WDISA) con_printf_at(0, wind.disa_y, "\x1%c\x1f", ConColor::WHITE);
    con_attr(0, 3);
    con_print_at(2, wind.disa_y, "F3");
    con_printf_at(
        6, wind.disa_y, 
        " cursor:%08X phys:%08X pc:%08X", 
        pa, PC);
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

static BOOL disa_cur_visible()
{
    DWORD   limit;
    limit = con.text + (wind.disa_h - 1) * 4;
    return ((con.disa_cursor < limit) && (con.disa_cursor >= con.text));
}

static void disa_goto(uint32_t addr)
{
    if(wind.disa_nav_last < 256)
    {
        wind.disa_nav_hist[++wind.disa_nav_last] = con.disa_cursor;
        con.text = addr - 4 * wind.disa_h / 2 + 4;
        con.disa_cursor = addr;
        con.update |= CON_UPDATE_DISA;
    }
}

static void disa_return()
{
    if(wind.disa_nav_last > 0)
    {
        con.disa_cursor = con.text = wind.disa_nav_hist[wind.disa_nav_last--];
        con.text -= 4 * wind.disa_h / 2;
        con.update |= CON_UPDATE_DISA;
    }
}

static void disa_navigate()
{
    PPCD_CB    disa;
    uint32_t op = 0, addr = con.disa_cursor;

    uint32_t pa = -1;
    if (emu.core)
    {
        pa = emu.core->EffectiveToPhysical(addr, true);
    }
    if(pa != -1) MEMFetch(pa, &op);
    if(op == 0) return;

    memset(&disa, 0, sizeof(PPCD_CB));
    disa.instr = op;
    disa.pc = addr;
    PPCDisasm (&disa);

    if(disa.iclass & PPC_DISA_BRANCH) disa_goto((uint32_t)disa.target);
}

void con_disa_key(char ascii, int vkey, int ctrl)
{
    UNREFERENCED_PARAMETER(ascii);
    UNREFERENCED_PARAMETER(ctrl);

    switch(vkey)
    {
        case VK_HOME:
            con_set_disa_cur(con.disa_cursor);
            break;
        case VK_END:
            break;
        case VK_UP:
            if(con.disa_cursor < con.text)
            {
                con.disa_cursor = con.text;
                break;
            }
            if(con.disa_cursor >= (con.text + 4 * wind.disa_h - 4))
            {
                con.disa_cursor = con.text + 4 * wind.disa_h - 8;
                break;
            }
            con.disa_cursor -= 4;
            if(con.disa_cursor < con.text) con.text -= 4;
            break;
        case VK_DOWN:
            if(con.disa_cursor < con.text)
            {
                con.disa_cursor = con.text;
                break;
            }
            if(con.disa_cursor >= (con.text + 4 * (wind.disa_h - wind.disa_sub_h) - 4))
            {
                con.disa_cursor = con.text + 4 * (wind.disa_h - wind.disa_sub_h) - 8;
                break;
            }
            con.disa_cursor += 4;
            if(con.disa_cursor >= (con.text + ((wind.disa_h - wind.disa_sub_h) - 1) * 4)) con.text += 4;
            break;
        case VK_PRIOR:
            con.text -= 4 * wind.disa_h - 4;
            if(!disa_cur_visible()) con.disa_cursor = con.text;
            break;
        case VK_NEXT:
            con.text += 4 * (wind.disa_h - wind.disa_sub_h) - 4;
            if(!disa_cur_visible()) con.disa_cursor = con.text + ((wind.disa_h - wind.disa_sub_h) - 2) * 4;
            break;
        case VK_RETURN:
            disa_navigate();    // browse functions
            break;
        case VK_ESCAPE:
            disa_return();
            break;
    }

    con_ldst_info();
    con.update |= CON_UPDATE_DISA;
}
