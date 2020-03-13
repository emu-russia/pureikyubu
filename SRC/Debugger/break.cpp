// breakpoints
#include "pch.h"

// ---------------------------------------------------------------------------
// execution

// start execute (F5)
void con_run_execute()
{
    int64_t old = TBR;

    emu.running = true;

    while(emu.running)
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

        //if(con_is_code_bp(PC)) con_break(" (PC breakpoint)");
    }
}

// step into instruction
void con_step_into()
{
    IPTExecuteOpcode();
    con.text = PC - 4 * wind.disa_h / 2 + 4;
    con.update |= CON_UPDATE_ALL;
}

// step over function
void con_step_over()
{
    int64_t old = TBR;
    uint32_t stop_addr = PC + 4;

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

        //if(con_is_code_bp(PC)) con_break(" (PC breakpoint)");
    }
}
