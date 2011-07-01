// cpu view (disassembly)
#include "dolphin.h"

// ---------------------------------------------------------------------------

static int con_disa_line(int line, u32 opcode, u32 addr)
{
    PPCD_CB    disa;
    int bg, fg, bgcur, bgbp;
    char *symbol;
    int addend = 1;

    bgcur = (addr == con.disa_cursor) ? (8) : (0);
    bgbp = (con_is_code_bp(addr)) ? (4) : (0);
    bg = (addr == PC) ? (1) : (0);
    bg = bg ^ bgcur ^ bgbp;
    fg = 7;
    con_attr(fg, bg);

    con_clear_line(line, con.attr);

    if(symbol = SYMName(addr))
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

        if(symbol = SYMName(disa.target))
        {
            con_printf_at(47, line, BROWN " ; %s", symbol);
        }
    }
    else con_printf_at(20, line, NORM "%-12s%s", disa.mnemonic, disa.operands);

    return addend;
}

// show pointer to data
void con_ldst_info()
{
    PPCD_CB    disa;
    wind.ldst = FALSE;

    {
        u32 op = MEMFetch(con.disa_cursor & ~3);

        disa.pc = con.disa_cursor;
        disa.instr = op;
        PPCDisasm (&disa);

        if(disa.iclass & PPC_DISA_LDST)
        {
            wind.ldst_disp = RRA + SIMM;
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
    if(wind.disamode == DISAMOD_PPC)
    {
        con_printf_at(
            6, wind.disa_y, 
            " cursor:%08X phys:%08X pc:%08X", 
            con.disa_cursor, MEMEffectiveToPhysical(con.disa_cursor, 0), PC);
    }
    else if(wind.disamode == DISAMOD_X86)
    {
        con_printf_at(6, wind.disa_y, " group:%08X size:%i (Press Esc, to back)", 
            wind.x86addr, RECGroupSize(wind.x86addr));
    }
    con_attr(7, 0);

    if(wind.disamode == DISAMOD_PPC) con_ldst_info();

    u32 op, addr = con.text & ~3;
    wind.disa_sub_h = 0;

    switch(wind.disamode)
    {
        case DISAMOD_PPC:           // PowerPC
        {
            for(int line=wind.disa_y+1; line<wind.disa_y+wind.disa_h; line++, addr+=4)
            {
                op = MEMFetch(addr);

                int n = con_disa_line(line, op, addr);
                if(n > 1) wind.disa_sub_h += n - 1;
                line += n - 1;
            }
            break;
        }

        case DISAMOD_X86:           // Intel X86
        {
            char *ptr = wind.x86dasm, linebuf[256], *lp;
            int crcnt = 0;

            // skip some lines
            while(crcnt != wind.x86line)
            {
                if(*ptr++ == '\n') crcnt++;
            }

            for(line=wind.disa_y+1; line<wind.disa_y+wind.disa_h; line++)
            {
                lp = linebuf;
                while(*ptr && *ptr != '\n') *lp++ = *ptr++;
                ptr++, *lp = 0;
                if(!*ptr) break;

                con_clear_line(line);
                con_printf_at(0, line, linebuf);
            }
            for(line; line<wind.disa_y+wind.disa_h; line++)
            {
                con_clear_line(line);
            }
            break;
        }
    }
}

void con_disa_show_compile()
{
    wind.x86addr = con.disa_cursor;
    wind.x86line = wind.x86lines = 0;
    u32 ea = wind.x86addr, pa = MEMEffectiveToPhysical(wind.x86addr, 0);

    // check if group is present
    u8 *code = cpu.groups[pa >> 2];
    if(code == (void *)RECDefaultGroup) // compile first
    {
        u32 old = PC;
        code = (u8 *)RECCompileGroup(wind.x86addr);
        PC = old;
    }

    // reallocate
    if(wind.x86dasm)
    {
        free(wind.x86dasm);
        wind.x86dasm = NULL;
    }
    wind.x86dasm = (char *)malloc(0x100000);
    ASSERT(wind.x86dasm == NULL, "Not enough memory for X86 text buffer.");

    // build text
    char *ptr = wind.x86dasm;
    int i = CPU_MAX_GROUP, len;
    while(i)
    {
        if(*code == 0xcc) break;    // int3
        if(*code == 0x90)           // nop - show PPC opcode
        {
            PPCD_CB disa;
            u32 op = MEMFetch(pa);

            ptr += sprintf(ptr, GREEN "%08X  ", ea);
            ptr += sprintf(ptr, "%08X  ", op);

            disa.instr = op;
            disa.pc = ea;

            PPCDisasm (&disa);

            if(disa.iclass & PPC_DISA_BRANCH)
            {
                char * symbol;

                ptr += sprintf(ptr, "%-12s%s", disa.mnemonic, disa.operands);

                if(symbol = SYMName(disa.target))
                {
                    ptr += sprintf(ptr, BROWN " ; %s", symbol);
                }
                ptr += sprintf(ptr, "\n");
            }
            else ptr += sprintf(ptr, "%-12s%s\n", disa.mnemonic, disa.operands);

            i--;
            code++;
            ea += 4;
            pa += 4;
            wind.x86lines++;
            continue;
        }
        char *d = dasm86(code, (int)code, &len);
        ptr += sprintf(ptr, NORM "%08X%s\n", (u32)code, d);
        code += len;
        i -= len;
        wind.x86lines++;
    }
    *ptr = 0;
}
