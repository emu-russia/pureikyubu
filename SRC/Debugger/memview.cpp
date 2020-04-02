// memory (data) view
#include "pch.h"

static const char *hexbyte(uint32_t addr)
{
    static char buf[0x10];

    // check address
    uint32_t pa = -1;
        
    if (Gekko::Gekko)
    {
        pa = Gekko::Gekko->EffectiveToPhysical(addr, 0);
    }

    if (mi.ram)
    {
        if (pa != -1)
        {
            if (pa < RAMSIZE)
            {
                sprintf_s (buf, sizeof(buf), "%02X", mi.ram[pa]);
                return buf;
            }
        }
    }
    return "??";
}

static const char *charbyte(uint32_t addr)
{
    static char buf[0x10];
    buf[0] = '.'; buf[1] = 0;

    // check address
    uint32_t pa = -1;
    
    if (Gekko::Gekko)
    {
        pa = Gekko::Gekko->EffectiveToPhysical(addr, 0);
    }

    if (mi.ram && pa != -1)
    {
        if (pa < RAMSIZE)
        {
            uint8_t data = mi.ram[pa];
            if ((data >= 32) && (data <= 255)) sprintf_s(buf, sizeof(buf), "%c\0", data);
            return buf;
        }
    }
    return "?";
}

void con_update_dump_window()
{
    con_attr(7, 0);
    for (int i = 0; i < wind.data_h; i++)
    {
        con_fill_line(wind.data_y + i, ' ');
    }

    uint32_t pa = -1;
    if (Gekko::Gekko)
    {
        pa = Gekko::Gekko->EffectiveToPhysical(con.data, 0);
    }

    con_attr(0, 3);
    con_fill_line(wind.data_y, 0xc4);
    con_attr(0, 3);
    if(wind.focus == WDATA) con_printf_at(0, wind.data_y, "\x1%c\x1f", ConColor::WHITE);
    con_attr(0, 3);
    con_print_at(2, wind.data_y, "F2");
    con_printf_at(6, wind.data_y, 
        " phys:%08X stack:%08X sda1:%08X sda2:%08X", 
        pa, SP, SDA1, SDA2
    );
    con_attr(7, 0);

    for(int row=0; row<wind.data_h-1; row++)
    {
        int col;
        static char buf[16];

        con_printf_at(0, wind.data_y + row + 1, "%08X", con.data + row * 16);

        for(col=0; col<8; col++)
            con_print_at(10 + col * 3, wind.data_y + row + 1, hexbyte(con.data + row * 16 + col));

        for(col=0; col<8; col++)
            con_print_at(35 + col * 3, wind.data_y + row + 1, hexbyte(con.data + row * 16 + col + 8));

        for(col=0; col<16; col++)
            con_print_at(60 + col, wind.data_y + row + 1, charbyte(con.data + row * 16 + col));
    }
}

void con_data_key(char ascii, int vkey, int ctrl)
{
    UNREFERENCED_PARAMETER(ascii);
    UNREFERENCED_PARAMETER(ctrl);

    switch(vkey)
    {
        case VK_HOME:
            con.data = (uint32_t)(1<<31);
            break;
        case VK_END:
            con.data = (RAMSIZE | (1<<31)) - ((wind.data_h - 1) * 16);
            break;
        case VK_NEXT:
            con.data += ((wind.data_h - 1) * 16);
            break;
        case VK_PRIOR:
            con.data -= ((wind.data_h - 1) * 16);
            break;
        case VK_UP:
            con.data -= 16;
            break;
        case VK_DOWN:
            con.data += 16;
            break;
    }

    con.update |= CON_UPDATE_DATA;
}
