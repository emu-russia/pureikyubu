// text output and refresh
#include "dolphin.h"

WINDControl wind;
ROLLControl roll;

static char *logcol[] = { 
    "<font color=#000000>", 
    "<font color=#000080>", 
    "<font color=#008000>", 
    "<font color=#008080>", 
    "<font color=#800000>",
    "<font color=#800080>",
    "<font color=#808000>",
    "<font color=#C0C0C0>",
    "<font color=#808080>",
    "<font color=#0000FF>",
    "<font color=#00FF00>",
    "<font color=#00FFFF>",
    "<font color=#FF0000>",
    "<font color=#FF00FF>",
    "<font color=#FFFF00>",
    "<font color=#FFFFFF>"
};
static char *logcurcol;

// ---------------------------------------------------------------------------

int con_wraproll(int roll, int value)
{
    roll += value;
    if(roll >= CON_LINES) return roll - CON_LINES;
    else if(roll < 0) return roll + CON_LINES;
    else return roll;
}

void con_set_disa_cur(uint32_t addr)
{
    con.disa_cursor = addr & ~3;
    con.text = con.disa_cursor - (wind.disa_h - 1) / 2 * 4;
    con.update |= CON_UPDATE_DISA;
}

void con_recalc_wnds()
{
    wind.regs_y = 0;

    if(wind.visible && CON_UPDATE_REGS)
        wind.data_y = wind.regs_y + wind.regs_h;
    else
        wind.data_y = wind.regs_y;

    if(wind.visible && CON_UPDATE_DATA)
        wind.disa_y = wind.data_y + wind.data_h;
    else
        wind.disa_y = wind.data_y;

    if(wind.visible && CON_UPDATE_DISA)
        wind.roll_y = wind.disa_y + wind.disa_h;
    else
        wind.roll_y = wind.disa_y;

    wind.roll_h = CON_HEIGHT - wind.roll_y - 2; // - statusline - editline
    wind.stat_h = wind.edit_h = 1;
    wind.edit_y = wind.roll_y + wind.roll_h;
    wind.stat_y = wind.edit_y + 1;
}

void con_blt_region(int regY, int regH)
{
    COORD       pos = { 0, (SHORT)regY };
    COORD       sz = { CON_WIDTH, CON_HEIGHT };
    SMALL_RECT  rgn = { 0, (SHORT)regY, 79, (SHORT)(regY + regH - 1) };
    BOOL        success = WriteConsoleOutput(con.output, *con.buf, sz, pos, &rgn);
}

void con_nextline()
{
    con.X = 0;
    con.Y++;
    if(con.Y >= CON_HEIGHT) con.Y = CON_HEIGHT - 1;
}

void con_printchar(char ch)
{
    con.buf[con.Y][con.X].Attributes = con.attr;
    con.buf[con.Y][con.X].Char.AsciiChar = ch;
    con.X++;
    if(con.X >= CON_WIDTH) con_nextline();
}

void con_printline(char *text)
{
    if(!text) return;

    while(*text)
    {
        if(*text == 1)
        {
            con.attr = (con.attr & 0xfff0) | text[1];
            text += 2;
        }
        else if(*text == 2)
        {
            con.attr &= 0xff0f;
            con.attr |= (text[1] & 0xf) << 4;
            text += 2;
        }
        else if(*text == '\n')
        {
            con_nextline();
            text++;
        }
        else if(*text == '\t')
        {
            int tbend = (con.X % 4) + 4;
            while(tbend--) con_printchar(' ');
            text++;
        }
        else con_printchar(*text++);
    }
}

void con_gotoxy(int X, int Y)
{
    con.X = X;
    con.Y = Y;
}

void con_print_at(int X, int Y, char *text)
{
    con_gotoxy(X, Y);
    con_printline(text);
}

void con_status(char *txt)
{
    sprintf_s (roll.statusline, sizeof(roll.statusline), " %s\n", txt);
    con_update(CON_UPDATE_STAT);
}

void con_cursorxy(int x, int y)
{
    COORD   cr = { (SHORT)x, (SHORT)y };
    SetConsoleCursorPosition(con.output, cr);
}

void con_fill_line(int y)
{
    for(int i = 0; i < CON_WIDTH; i++)
    {
        con.buf[y][i].Attributes = 3 << 4;
        con.buf[y][i].Char.AsciiChar = (char)0xC4;
    }
}

void con_clear_line(int y, uint16_t attr)
{
    for(int i = 0; i < CON_WIDTH; i++)
    {
        con.buf[y][i].Attributes = attr;
        con.buf[y][i].Char.AsciiChar = ' ';
    }
}

void con_printf_at(int x, int y, char *txt, ...)
{
    va_list     varg;
    static char buf[256];

    va_start(varg, txt);
    vsprintf_s (buf, sizeof(buf), txt, varg);
    con_print_at(x, y, buf);
    va_end(varg);
}

void con_set_autoscroll(bool value)
{
    roll.autoscroll = value;
    if(value) con_status("Ready. Press PgUp to look behind.");
    else con_status("Scroll Mode - Press PgUp, PgDown, Up, Down to scroll.");
}

// generate HTML text
static char * string_to_HTML_string(char *txt)
{
    static char html[0x1000];
    char *ptr = html;
    size_t len = strlen(txt);

    logcurcol = NULL;
    ptr += sprintf_s (ptr, sizeof(html) - (ptr - html), "%s", logcol[7]);

    for(int n=0; n<len;)
    {
        char c = txt[n];
        if(c == 1)
        {
            if(logcurcol) ptr += sprintf_s(ptr, sizeof(html) - (ptr - html), "</font>");
            logcurcol = logcol[txt[n+1]];
            ptr += sprintf_s(ptr, sizeof(html) - (ptr - html), "%s", logcurcol);
            n+=2;
        }
        else if(c == 2)
        {
            n+=2;
        }
        else if(c == '<')
        {
            ptr += sprintf_s(ptr, sizeof(html) - (ptr - html), "&lt;");
            n++;
        }
        else if(c == '>')
        {
            ptr += sprintf_s (ptr, sizeof(html) - (ptr - html), "&gt;");
            n++;
        }
        else
        {
            *ptr++ = c;
            n++;
        }
    }

    if(logcurcol) ptr += sprintf_s (ptr, sizeof(html) - (ptr - html), "</font>");
    ptr += sprintf_s (ptr, sizeof(html) - (ptr - html), "\n");
    *ptr++ = 0;
    return html;
}

static void log_console_output(char *txt)
{
    static int nwrites = 10;

    if(con.log == TRUE)
    {
        if(!con.logf)
        {
            con.logf = nullptr;
            fopen_s (&con.logf, con.logfile, "w");
            if (con.logf)
            {
                fprintf(con.logf, "<html>\n");
                fprintf(con.logf, "<style>pre { font-family: Small; font-size: 8pt; }</style>\n");
                fprintf(con.logf, "<body bgcolor=#000000>\n");
                fprintf(con.logf, "<pre>\n");
            }
        }

        if(con.logf)
        {
            fprintf(con.logf, "%s", txt);
        }

        if(!nwrites-- && con.logf)
        {
            nwrites = 10;
            fflush(con.logf);
        }
    }
}

void con_add_roller_line(char *txt, int err)
{
    char line[0x1000], *ptr = txt;

    // insert error color
    if(err)
    {
        sprintf_s(line, sizeof(line), BRED "%s", txt);
        ptr = line;
    }

    // roll console "roller" 1 line up
    roll.rollpos = con_wraproll(roll.rollpos, 1);
    strncpy_s(roll.data[roll.rollpos], sizeof(roll.data[roll.rollpos]), ptr, CON_LINELEN-1);
    log_console_output(string_to_HTML_string(ptr));
    con_update(CON_UPDATE_MSGS);
}

void con_change_focus(FOCUSWND newfocus)
{
    FOCUSWND oldfocus = wind.focus;

    if(oldfocus == newfocus)
        return; // we dont need any repaint

    wind.focus = newfocus;
    
    switch(oldfocus)
    {           // switch focus from?
        case WREGS:     con.update |= (CON_UPDATE_REGS); break;
        case WDATA:     con.update |= (CON_UPDATE_DATA); break;
        case WDISA:     con.update |= (CON_UPDATE_DISA); break;
        case WCONSOLE:  con.update |= (CON_UPDATE_MSGS); break;
    }

    switch(newfocus)
    {           // switch focus to?
        case WREGS:     con.update |= (CON_UPDATE_REGS); break;
        case WDATA:     con.update |= (CON_UPDATE_DATA); break;
        case WDISA:     con.update |= (CON_UPDATE_DISA); break;
        case WCONSOLE:  con.update |= (CON_UPDATE_MSGS); break;
    }
}

static void con_update_scroll_window()
{
    int i, y, back, line;

    // cleanup window and skip header line
    memset(con.buf[wind.roll_y + 1], 0, (sizeof(CHAR_INFO) * (wind.roll_h - 1) * CON_WIDTH));
    con_attr(0, 3);
    con_fill_line(wind.roll_y);
    if(wind.focus == WCONSOLE) con_print_at(0, wind.roll_y, WHITE "\x1f");
    con_attr(0, 3);
    con_print_at(2, wind.roll_y, "F4");
    con_print_at(6, wind.roll_y, " console output");
    con_attr(7, 0);

    // printing coord in buffer (skip header)
    y = wind.roll_y + 1;

    // shift backward
    back = wind.roll_h - 1;

    // where to get buffer line
    line = (roll.autoscroll) ? (roll.rollpos - back) : (roll.viewpos - back);
    line += 1;
    for(i=1; i<wind.roll_h; i++)
    {
        if(line >= 0) con_print_at(0, y, roll.data[line]);
        line++;
        y++;
    }
}

void con_fullscreen(bool full)
{
    for(int i=0; i<CON_HEIGHT; i++)
        con_clear_line(i, 7);

    wind.full = full;

    if(wind.full)
    {
        wind.regs_h = 0;
        wind.data_h = 0;
        wind.disa_h = CON_HEIGHT - 3;
    }
    else
    {
        wind.regs_h = 17;
        wind.data_h = 8;
        wind.disa_h = 18; // 16
    }
    con_recalc_wnds();
}

// IMPORTANT : read carefully.
// this function actually will update windows not instantly,
// but sometimes, so if you really want to be sure, that
// window is updated, force "con.update |= mask" instead!
void con_update(uint32_t mask)
{
    if(!con.active) return;

    if(!emu.running)
    {
        con.update |= mask;
        return;
    }

    if(mask & CON_UPDATE_REGS)
    {
        con.update |= mask;
    }
    else if(mask & CON_UPDATE_DATA)
    {
        con.update |= mask;
    }
    else if(mask & CON_UPDATE_DISA)
    {
        con.update |= mask;
    }
    else con.update |= mask;
}

void con_refresh(bool showpc)
{
    if(con.active == FALSE) return;
    if(showpc)
    {
        con_set_disa_cur(PC);
    }
    if(con.update == 0) return;

    // registres
    if(con.update & CON_UPDATE_REGS)
    {
        con_update_registers();
        con_blt_region(wind.regs_y, wind.regs_h);
    }

    // data dump window
    if(con.update & CON_UPDATE_DATA)
    {
        con_update_dump_window();
        con_blt_region(wind.data_y, wind.data_h);
    }

    // disassembler window
    if(con.update & CON_UPDATE_DISA)
    {
        con_update_disa_window();
        con_blt_region(wind.disa_y, wind.disa_h);
    }

    // message history
    if(con.update & CON_UPDATE_MSGS)
    {
        con_update_scroll_window();
        con_blt_region(wind.roll_y, wind.roll_h);
    }

    // editline window
    if(con.update & CON_UPDATE_EDIT)
    {
        memset(&con.buf[wind.edit_y][0], 0, sizeof(CHAR_INFO) * 80 );
        con_attr(7, 0);
        con_print_at(0, wind.edit_y, "> ");
        con_printline(roll.editline);
        con_blt_region(wind.edit_y, 1);
        con_cursorxy(roll.editpos + 2, wind.edit_y);
    }

    // statusline window
    if(con.update & CON_UPDATE_STAT)
    {
        int i;

        con_attr(0, 3);
        for(i=0; i<CON_WIDTH; i++)
        {
            con.buf[CON_HEIGHT - 1][i].Char.AsciiChar = ' ';
            con.buf[CON_HEIGHT - 1][i].Attributes = con.attr;
        }

        con_print_at(1, wind.stat_y, roll.statusline);
        con_blt_region(wind.stat_y, 1);
    }

    con.update = 0;
}

void con_error(const char *txt, ...)
{
    char    buf[0x1000];
    va_list arg;

    // emulator can do output, even if console closed
    if(!con.active) return;

    sprintf_s(buf, sizeof(buf), BRED);
    va_start(arg, txt);
    vsprintf(buf+strlen(BRED), txt, arg);
    va_end(arg);
    buf[strlen(buf) + 1] = 0;

    char *s, *p = buf;
    while(*p)
    {
        while(*p && *p == '\n')
        {
            con_add_roller_line("", 0);
            p++;
        }

        s = p;
        while(*p && *p != '\n') p++;
        *p = 0;

        if(*s) con_add_roller_line(s, 1);
        p++;
    }

    // break
    con.update = CON_UPDATE_ALL;
    con_refresh();
    Sleep(10);
    con_break();
}

void con_print(const char *txt, ...)
{
    char    buf[0x1000];
    va_list arg;

    // emulator can do output, even if console closed
    if(!con.active) return;

    sprintf_s(buf, sizeof(buf), NORM);
    va_start(arg, txt);
    vsprintf(buf+strlen(NORM), txt, arg);
    va_end(arg);
    buf[strlen(buf) + 1] = 0;

    char *s, *p = buf;
    while(*p)
    {
        while(*p && *p == '\n')
        {
            con_add_roller_line("", 0);
            p++;
        }

        s = p;
        while(*p && *p != '\n') p++;
        *p = 0;

        if(*s) con_add_roller_line(s, 0);
        p++;
    }
}
