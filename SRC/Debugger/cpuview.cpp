// cpu view (disassembly)
#include "pch.h"

static int con_disa_line(int line, uint32_t opcode, uint32_t addr)
{
    int bg, fg, bgcur, bgbp;
    char *symbol;
    int addend = 1;

    bgcur = (addr == con.disa_cursor) ? (8) : (0);
    //bgbp = (con_is_code_bp(addr)) ? (4) : (0);
    bgbp = 0;
    bg = (addr == Gekko::Gekko->regs.pc) ? (1) : (0);
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

    Gekko::AnalyzeInfo info = { 0 };

    Gekko::Analyzer::Analyze(addr, opcode, &info);

    std::string text = Gekko::GekkoDisasm::Disasm(addr, &info);

    if(info.flow && info.Imm.Address != 0)
    {
        const char* dir;

        if (info.Imm.Address > addr) dir = " \x19";
        else if (info.Imm.Address < addr) dir = " \x18";
        else dir = " \x1b";

        con_printf_at(0, line, "\x1%c%s\x1%c%s", 
            ConColor::GREEN, text.c_str(), ConColor::CYAN, dir);

        symbol = SYMName(info.Imm.Address);
        if(symbol)
        {
            con_printf_at(47, line, "\x1%c ; %s", ConColor::BROWN, symbol);
        }
    }
    
    else con_printf_at(0, line, "\x1%c%s", ConColor::NORM, text.c_str());

    if (text[0] == 'r' && text[1] == 'l')
    {
        int mb = info.paramBits[3];
        int me = info.paramBits[4];
        uint32_t mask = ((uint32_t)-1 >> mb) ^ ((me >= 31) ? 0 : ((uint32_t)-1) >> (me + 1));

        con_printf_at (60, line, "\x1%cmask:0x%08X", ConColor::NORM, mask);
    }

    return addend;
}

void con_update_disa_window()
{
    uint32_t pa = -1;
    if (Gekko::Gekko)
    {
        pa = Gekko::Gekko->EffectiveToPhysical(con.disa_cursor, true);
    }

    con_attr(0, 3);
    con_fill_line(wind.disa_y, 0xc4);
    if(wind.focus == WDISA) con_printf_at(0, wind.disa_y, "\x1%c\x1f", ConColor::WHITE);
    con_attr(0, 3);
    con_print_at(2, wind.disa_y, "F3");
    con_printf_at(
        6, wind.disa_y, 
        " cursor:%08X phys:%08X pc:%08X", 
        pa, Gekko::Gekko->regs.pc);
    con_attr(7, 0);

    uint32_t op, addr = con.text & ~3;
    wind.disa_sub_h = 0;

    for(int line=wind.disa_y+1; line<wind.disa_y+wind.disa_h; line++, addr+=4)
    {
        if (CPUReadWord != nullptr)
        {
            CPUReadWord(addr, &op);
        }
        else
        {
            op = 0;
        }

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
    uint32_t op = 0, addr = con.disa_cursor;

    uint32_t pa = -1;
    if (Gekko::Gekko)
    {
        pa = Gekko::Gekko->EffectiveToPhysical(addr, true);
    }
    if(pa != -1) CPUReadWord(pa, &op);
    if(op == 0) return;

    Gekko::AnalyzeInfo info = { 0 };

    Gekko::Analyzer::Analyze(addr, op, &info);

    std::string text = Gekko::GekkoDisasm::Disasm(addr, &info);

    if(info.flow && info.Imm.Address != 0) disa_goto(info.Imm.Address);
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

    con.update |= CON_UPDATE_DISA;
}
