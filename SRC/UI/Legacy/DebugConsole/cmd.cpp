// Debug commands processor
// This module is a legacy from version 0.10. Will gradually be replaced by a more advanced JDI system.

#include "pch.h"

// ---------------------------------------------------------------------------

void cmd_init_handlers()
{
    con.cmds["."] = cmd_showpc;
    con.cmds["*"] = cmd_showldst;
    con.cmds["blr"] = cmd_blr;
    con.cmds["d"] = cmd_d;
    con.cmds["denop"] = cmd_denop;
    con.cmds["disa"] = cmd_disa;
    con.cmds["full"] = cmd_full;
    con.cmds["help"] = cmd_help;
    con.cmds["log"] = cmd_log;
    con.cmds["logfile"] = cmd_logfile;
    con.cmds["lr"] = cmd_lr;
    con.cmds["nop"] = cmd_nop;
    con.cmds["sd1"] = cmd_sd1;
    con.cmds["sd2"] = cmd_sd2;
    con.cmds["sop"] = cmd_sop;
    con.cmds["tree"] = cmd_tree;
    con.cmds["u"] = cmd_u;
}

// ---------------------------------------------------------------------------
// help

Json::Value* cmd_help(std::vector<std::string>& args)
{
    DBReport2(DbgChannel::Header, "## cpu debug commands\n");
    DBReport( "    .                    - view code at pc\n");
    DBReport( "    *                    - view data, pointed by load/store opcode\n");
    DBReport( "    u                    - view code at specified address\n");
    DBReport( "    d                    - view data at specified address\n");
    DBReport( "    sd1                  - quick form of d-command, using SDA1\n");
    DBReport( "    sd2                  - quick form of d-command, using SDA2\n");
    DBReport( "    pc                   - set program counter\n");

    DBReport( "    nop                  - insert NOP opcode at cursor\n");
    DBReport( "    denop                - restore old NOP'ed value\n");
    DBReport( "    blr [value]          - insert BLR opcode at cursor (with value)\n");
    //  DBReport( "    b <addr>             - toggle code breakpoint at <addr> (max=%i)\n", MAX_BPNUM);
    //  DBReport( "    bm [8|16|32] <addr>  - toggle data breakpoint at <addr> (max=%i)\n", MAX_BPNUM);
    DBReport( "    bc                   - clear all code breakpoints\n");
    //  DBReport( "    bmc                  - clear all data breakpoints\n");
    DBReport( "    reset                - reset emulator\n");
    DBReport("\n");

    JDI::Hub.Help();

    DBReport2(DbgChannel::Header, "## Misc commands\n");
    DBReport( "    reboot               - reload last file\n");
    DBReport( "    sop                  - search opcode (forward) from cursor address\n");
    DBReport( "    lr                   - show LR back chain (\"branch history\")\n");
    DBReport( "    log                  - enable/disable log output)\n");
    DBReport( "    logfile              - choose HTML log-file for ouput\n");
    DBReport( "    full                 - set full screen console mode\n");
    DBReport( "    cls                  - clear message buffer\n");
    DBReport( "    disa                 - disassemble code into text file\n");
    DBReport( "    tree                 - show call tree\n");
    DBReport("\n");

    DBReport2(DbgChannel::Header, "## Functional keys\n");
    DBReport( "    F1                   - update registers\n");
    DBReport( "    F2                   - memory view\n");
    DBReport( "    F3                   - disassembly\n");
    DBReport( "    F4                   - command string\n");
    DBReport( "    F5                   - run, stop\n");
    DBReport( "    F6, ^F6              - switch registers\n");
    DBReport( "    F9                   - toggle autokill breakpoint\n");
    DBReport( "    ^F9                  - toggle breakpoint\n");
    DBReport( "    F10                  - step over\n");
    DBReport( "    F11                  - single step (Google: disable f11 windows 10 console)");
    DBReport( "    F12                  - skip instruction\n");
    DBReport("\n");

    DBReport2(DbgChannel::Header, "## Misc keys\n");
    DBReport( "    PGUP, PGDN           - scroll windows\n");
    DBReport( "    ENTER, ESC           - follow/return branch (in disasm window)\n");
    DBReport( "    ENTER                - memory edit (in memview window)\n");
    DBReport("\n");
    return nullptr;
}

// ---------------------------------------------------------------------------
// special

Json::Value* cmd_showpc(std::vector<std::string>& args)
{
    con_set_disa_cur(Gekko::Gekko->regs.pc);
    return nullptr;
}

Json::Value* cmd_showldst(std::vector<std::string>& args)
{
    if (wind.ldst)
    {
        con.data = wind.ldst_disp;
        con.update |= CON_UPDATE_DATA;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// blr

Json::Value* cmd_blr(std::vector<std::string>& args)
{
    if(args.size() > 2)
    {
        DBReport("syntax : blr [value]\n");
        DBReport("when [value] is specified, pair of instructions is inserted : \n");
        DBReport("    li [value]\n");
        DBReport("    blr\n");
        DBReport("(with exception, if BLR is already present at cursor.\n");
        DBReport("16-bit value can be decimal, or hex with \'0x\' prefix.\n");
        DBReport("examples of use : blr\n");
        DBReport("                  blr 0\n");
        DBReport("                  blr 1\n");
        DBReport("see also        : nop\n");
    }
    else
    {
        if(emu.loaded) return nullptr;

        int WIMG;
        uint32_t ea = con.disa_cursor;
        uint32_t pa = Gekko::BadAddress;
        if (Gekko::Gekko)
        {
            pa = Gekko::Gekko->EffectiveToPhysical(ea, Gekko::MmuAccess::Execute, WIMG);
        }
        if(pa == Gekko::BadAddress) return nullptr;

        uint32_t op = _byteswap_ulong(*(uint32_t*)(&mi.ram[pa]));
        if(op == 0x4e800020) return nullptr;

        int ofs = 0;
        if(args.size() >= 2)           // value, to simulate "return X"
        {
            uint32_t iVal = strtoul(args[1].c_str(), NULL, 0) & 0xffff;
            mi.ram[pa+0] = 0x38;
            mi.ram[pa+1] = 0;
            mi.ram[pa+2] = (uint8_t)(iVal >> 8);
            mi.ram[pa+3] = (uint8_t)iVal;
            ofs = 4;
        }
        
        mi.ram[pa+ofs+0] = 0x4e;   // BLR
        mi.ram[pa+ofs+1] = 0x80;
        mi.ram[pa+ofs+2] = 0;
        mi.ram[pa+ofs+3] = 0x20;

        con.update |= (CON_UPDATE_DISA | CON_UPDATE_DATA);
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
// d

Json::Value* cmd_d(std::vector<std::string>& args)
{
    if(args.size() < 2)
    {
        DBReport("syntax : d <addr> OR d <symbol> OR d <reg> [ofs]\n");
        DBReport("numbers can be decimal, or hex with \'0x\' prefix.\n");
        DBReport("offset is a 16-bit signed dec/hex number.\n");
        DBReport("\'small data\' regs are used as r13 and r2.\n");
        DBReport("examples of use : d 0x8023bc00\n");
        DBReport("                  d main\n");
        DBReport("                  d r5\n");
        DBReport("                  d r9 0x8250\n");
        DBReport("see also        : sd1 sd2 *\n");
    }
    else
    {
        uint32_t addr = 0;

        con.update |= CON_UPDATE_DATA;

        // first check for register form
        if(args[1].c_str()[0] == 'r' && isdigit(args[1].c_str()[1]))
        {
            uint32_t reg, ofs = 0;
            int n = strtoul(&args[1].c_str()[1], NULL, 10);
            reg = Gekko::Gekko->regs.gpr[n];
            if(args.size() >= 3) ofs = strtoul(args[2].c_str(), NULL, 0);
            ofs &= 0xffff;
            if(ofs & 0x8000) ofs |= 0xffff0000;
            addr = reg + (int32_t)ofs;
            con.data = addr;
            return nullptr;
        }

        // now check for symbol
        addr = SYMAddress(args[1].c_str());
        if(addr)
        {
            con.data = addr;
            return nullptr;
        }

        // simply address
        con.data = strtoul(args[1].c_str(), NULL, 0);
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// disa

static void disa_line (FILE *f, uint32_t opcode, uint32_t addr)
{
    PPCD_CB    disa;
    char *symbol;

    if((symbol = SYMName(addr)) != nullptr)
    {
        fprintf (f, "\n%s\n", symbol);
    }

    fprintf ( f, "%08X  %08X  ", addr, opcode);

    disa.instr = opcode;
    disa.pc = addr;

    PPCDisasm (&disa);

    if(opcode == 0x4e800020  )  /// blr
    {
        // ignore other bclr/bcctr opcodes,
        // to easily locate end of function
        fprintf (f, "blr");
    }

    else if(disa.iclass & PPC_DISA_BRANCH)
    {
        if((symbol = SYMName((uint32_t)disa.target)) != nullptr) fprintf (f, "%-12s%s", disa.mnemonic, symbol);
        else fprintf (f, "%-12s%s", disa.mnemonic, disa.operands);

        if(disa.target > addr) fprintf (f, " \x19");
        else if (disa.target < addr) fprintf (f, " \x18");
        else fprintf (f, " \x1b");
    }
    
    else fprintf (f, "%-12s%s", disa.mnemonic, disa.operands);

    if ((disa.iclass & PPC_DISA_INTEGER) && disa.mnemonic[0] == 'r' && disa.mnemonic[1] == 'l')
    {
        fprintf (f, "\t\t\tmask:0x%08X", (uint32_t)disa.target);
    }
    fprintf (f, "\n");
}

Json::Value* cmd_disa(std::vector<std::string>& args)
{
    uint32_t start_addr, sa, end_addr;
    FILE *f;

    if (args.size() < 3)
    {
        DBReport("syntax : disa <start_addr> <end_addr>\n");
        DBReport("disassemble code between `start_addr` and `end_addr` and dump it into disa.txt\n");
        DBReport("example of use : disa 0x81300000 0x81350000\n");
    }
    else
    {
        if(!emu.loaded)
        {
            DBReport("not loaded\n");
            return nullptr;
        }

        start_addr = strtoul ( args[1].c_str(), NULL, 0 );
        sa = start_addr;
        end_addr = strtoul ( args[2].c_str(), NULL, 0 );

        f = nullptr;
        fopen_s ( &f, "Data\\disa.txt", "wt" );
        if (!f)
        {
            DBReport( "Cannot open output file!\n");
            return nullptr;
        }

        for (start_addr; start_addr<end_addr; start_addr+=4)
        {
            uint32_t opcode;
            Gekko::Gekko->ReadWord(start_addr, &opcode);
            disa_line ( f, opcode, start_addr );
        }

        DBReport( "Disassembling from 0x%08X to 0x%08X... done\n", sa, end_addr );
        fclose (f);
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
// full

Json::Value* cmd_full(std::vector<std::string>& args)
{
    if(args.size() < 2)
    {
        DBReport("syntax : full <0/1>\n");
        DBReport("set \"fullscreen\" console mode.\n");
        DBReport("examples of use : full 1\n");
    }
    else
    {
        con_fullscreen(atoi(args[1].c_str()) & 1);
        con.update |= CON_UPDATE_ALL;
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
// log

Json::Value* cmd_log(std::vector<std::string>& args)
{
    if(args.size() < 2)
    {
        DBReport("syntax : log [dev] <0/1>\n");
        DBReport("dev is specified device :\n");
        DBReport("    fifo : GX command processor\n");
        DBReport("    vi   : video interface\n");
        DBReport("    pi   : interrupts\n");
        DBReport("    mi   : Flipper memory protection\n");
        DBReport("    ax   : DSP, audio and streaming\n");
        DBReport("    di   : DVD\n");
        DBReport("    si   : serial interface (joypads)\n");
        DBReport("    exi  : EXI devices\n");
        DBReport("examples of use : log 1\n");
        DBReport("                : log 0\n");
        DBReport("                : log pi 0\n");
        DBReport("                : log ax 1\n");
        DBReport("see also        : logfile\n");
    }
    else if(args.size() < 3)
    {
        con.log = atoi(args[1].c_str());
        if(con.log) DBReport("log enabled (logfile: %s)", con.logfile);
        else
        {
            if(con.logf)
            {
                fprintf(con.logf, "</pre>\n");
                fprintf(con.logf, "</body>\n");
                fprintf(con.logf, "</html>\n");
                fclose(con.logf);
                con.logf = NULL;
            }
            DBReport("log disabled\n");
        }
    }
    else
    {
        #define IFDEV(n) if(!strncmp(argv[1], n, strlen(n)))
        DBReport("unknown device!\n");
        #undef IFDEV
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
// logfile

Json::Value* cmd_logfile(std::vector<std::string>& args)
{
    if(args.size() < 2)
    {
        DBReport("syntax : logfile <filename>\n");
        DBReport("filename can be relative. default filename is %s\n", CON_LOG_FILE);
        DBReport("examples of use : logfile log.htm\n");
        DBReport("see also        : log\n");
    }
    else
    {
        strncpy_s (con.logfile, sizeof(con.logfile), args[1].c_str(), sizeof(con.logfile));
        DBReport("logging into %s\n", con.logfile);
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
// lr

Json::Value* cmd_lr(std::vector<std::string>& args)
{
    if(args.size() < 2)
    {
        DBReport("syntax : lr <level>\n");
        DBReport("level - chain depth (number of calls). use \'*\' to show whole chain.\n");
        DBReport("examples of use : lr 10\n");
        DBReport("                : lr *\n");
    }
    else
    {
        #define MAX_LEVEL 0xfff     // back chain limit
        PPCD_CB disa;
        uint32_t sp;

        if(!emu.loaded || !Gekko::Gekko->regs.gpr[1])
        {
            DBReport("not running, or no calls.\n");
            return nullptr;
        }

        int level = atoi(args[1].c_str());
        if(args[1].c_str()[0] == '*' || level > MAX_LEVEL) level = MAX_LEVEL;
        Gekko::Gekko->ReadWord(Gekko::Gekko->regs.gpr[1], &sp);
        if(level == MAX_LEVEL) DBReport( "LR Back Chain (max levels) :\n");
        else DBReport( "LR Back Chain (%i levels) :\n", level);

        for(int i=0; i<level; i++)
        {
            uint32_t read_pc;
            Gekko::Gekko->ReadWord(sp+4, &read_pc);    // read LR value from stack
            disa.pc = read_pc;
            disa.pc -= 4;                   // set to branch opcode
            Gekko::Gekko->ReadWord((uint32_t)disa.pc, &disa.instr); // read branch
            PPCDisasm (&disa);                    // disasm
            if(disa.iclass & PPC_DISA_BRANCH)
            {
                char * symbol = SYMName((uint32_t)disa.target);
                if(symbol) DBReport("%-3i: %-12s%-12s (%s)\n",
                                      i+1, disa.mnemonic, disa.operands, symbol );
                else       DBReport("%-3i: %-12s%-12s " "\n",
                                      i+1, disa.mnemonic, disa.operands );
            }
            Gekko::Gekko->ReadWord(sp, &sp);           // walk stack

            uint32_t pa = Gekko::BadAddress;
            if (Gekko::Gekko)
            {
                int WIMG;
                pa = Gekko::Gekko->EffectiveToPhysical(sp, Gekko::MmuAccess::Execute, WIMG);
            }

            if(!sp || pa == Gekko::BadAddress) break;
        }
        #undef MAX_LEVEL
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
// nop

static void add_nop(uint32_t ea, uint32_t oldVal)
{
    int n = con.nopNum ++;
    con.nopHist = (NOPHistory *)realloc(con.nopHist, sizeof(NOPHistory) * con.nopNum);
    assert(con.nopHist);
    con.nopHist[n].ea = ea;
    con.nopHist[n].oldValue = oldVal;
}

static uint32_t get_nop(uint32_t ea)
{
    for(int i=0; i<con.nopNum; i++)
    {
        if(con.nopHist[i].ea == ea)
            return con.nopHist[i].oldValue;
    }
    return 0;   // not present
}

Json::Value* cmd_nop(std::vector<std::string>& args)
{
    if(!emu.loaded) return nullptr;

    uint32_t ea = con.disa_cursor;
    uint32_t pa = Gekko::BadAddress;
    if (Gekko::Gekko)
    {
        int WIMG;
        pa = Gekko::Gekko->EffectiveToPhysical(ea, Gekko::MmuAccess::Execute, WIMG);
    }
    if(pa == Gekko::BadAddress) return nullptr;
    
    uint32_t old = _byteswap_ulong(*(uint32_t*)(&mi.ram[pa]));
    mi.ram[pa] = 0x60;
    mi.ram[pa+1] = mi.ram[pa+2] = mi.ram[pa+3] = 0;
    add_nop(ea, old);

    con.update |= (CON_UPDATE_DISA | CON_UPDATE_DATA);
    return nullptr;
}

Json::Value* cmd_denop(std::vector<std::string>& args)
{
    if(!emu.loaded) return nullptr;

    uint32_t ea = con.disa_cursor;
    uint32_t pa = Gekko::BadAddress;
    if (Gekko::Gekko)
    {
        int WIMG;
        pa = Gekko::Gekko->EffectiveToPhysical(ea, Gekko::MmuAccess::Execute, WIMG);
    }
    if(pa == Gekko::BadAddress) return nullptr;

    uint32_t old = get_nop(ea);
    if(old == 0) return nullptr;
    mi.ram[pa+0] = (uint8_t)(old >> 24);
    mi.ram[pa+1] = (uint8_t)(old >> 16);
    mi.ram[pa+2] = (uint8_t)(old >>  8);
    mi.ram[pa+3] = (uint8_t)(old >>  0);

    con.update |= (CON_UPDATE_DISA | CON_UPDATE_DATA);
    return nullptr;
}

// ---------------------------------------------------------------------------
// sop

Json::Value* cmd_sop(std::vector<std::string>& args)
{
    uint32_t saddr;
    if(args.size() < 2)
    {
        DBReport("syntax : sop <opcode>\n");
        DBReport("search range is not greater 16384 bytes.\n");
        DBReport("examples of use : sop mtlr\n");
        DBReport("                  sop psq_l\n");
    }
    else
    {
        // search opcode
        uint32_t eaddr = con.disa_cursor + 16384;
        for(saddr=con.disa_cursor+4; saddr<eaddr; saddr+=4)
        {
            PPCD_CB disa;
            uint32_t op = 0;
            uint32_t pa = Gekko::BadAddress;
            if (Gekko::Gekko)
            {
                int WIMG;
                pa = Gekko::Gekko->EffectiveToPhysical(saddr, Gekko::MmuAccess::Execute, WIMG);
            }
            if(pa != Gekko::BadAddress) Gekko::Gekko->ReadWord(pa, &op);
            disa.instr = op;
            disa.pc = saddr;
            PPCDisasm (&disa);
            if(!_stricmp(disa.mnemonic, args[1].c_str())) break;
        }
        if(saddr == eaddr) DBReport("%s not found. last address : %08X\n", args[1].c_str(), saddr);
        else
        {
            DBReport("%s found at address : %08X\n", args[1].c_str(), saddr);
            con_set_disa_cur(saddr);
        }
        con.update |= CON_UPDATE_DISA;
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
// sd1, sd2

Json::Value* cmd_sdCommon(int sd, std::vector<std::string>& args)
{
    if(args.size() < 2)
    {
        DBReport("syntax : sd%i <ofs>\n", sd);
        DBReport("offset is a 16-bit signed dec/hex number.\n");
        DBReport("examples of use : sd%i 0x8250\n", sd);
        DBReport("see also        : d *\n");
    }
    else
    {
        uint32_t sda;
        if(sd == 1) sda = Gekko::Gekko->regs.gpr[13];
        else sda = Gekko::Gekko->regs.gpr[2];

        uint32_t ofs = strtoul(args[1].c_str(), NULL, 0);
        ofs &= 0xffff;
        if(ofs & 0x8000) ofs |= 0xffff0000;
        con.data = sda + (int32_t)ofs;
        con.update |= CON_UPDATE_DATA;
    }
    return nullptr;
}

Json::Value* cmd_sd1(std::vector<std::string>& args)
{
    return cmd_sdCommon(1, args);
}

Json::Value* cmd_sd2(std::vector<std::string>& args)
{
    return cmd_sdCommon(2, args);
}

// ---------------------------------------------------------------------------
// tree

typedef struct call_history {
    uint32_t address;
    int hits;
} call_history;

static call_history * callhist;
static int callhist_count;

static int get_call_history ( uint32_t address )
{
    for (int i=0; i<callhist_count; i++) {
        if ( callhist[i].address == address ) {
            return ++callhist[i].hits;
        }
    }

    callhist = (call_history *)realloc ( callhist, sizeof(call_history) * (callhist_count + 1) );
    callhist[callhist_count].address = address;
    callhist[callhist_count].hits = 1;
    callhist_count ++;

    return 1;
}

static void dump_subcalls ( uint32_t address, FILE * f, int level )
{
    uint32_t prev_address = address;
    PPCD_CB    disa;
    int bailout = 10000;

    int times = get_call_history (address);

    int cnt = level;
    while ( cnt--) fprintf ( f, "\t" );
    
    char * name = SYMName (address);
    if (name) fprintf ( f, "%s (%i)", name, times );
    else fprintf ( f, "0x%08X (%i)", address, times );

    if ( address > 0x8135b260 ) {
        fprintf ( f, "\n" );
        return;
    }

    if ( times > 1 ) {      // Eliminate repetitive walktrhough
        fprintf ( f, " ...\n");
        return;
    }
    else fprintf ( f, "\n");

    while ( bailout-- )
    {
        uint32_t opcode;
        Gekko::Gekko->ReadWord(address, &opcode);
        if ( opcode == 0x4e800020 || opcode == 0 ) break;

        disa.instr = opcode;
        disa.pc = address;

        PPCDisasm (&disa);

        if(disa.iclass & PPC_DISA_BRANCH)
        {
            if ( !_stricmp ( disa.mnemonic, "bl" ) )
            {
                uint32_t start_address = (uint32_t)disa.target;
                if ( prev_address != start_address ) dump_subcalls ( start_address, f, level+1 );
            }
        }

        if ( disa.iclass & PPC_DISA_ILLEGAL ) break;

        address += 4;
    }
}

Json::Value* cmd_tree (std::vector<std::string>& args)
{
    uint32_t start_addr;
    FILE * f;

    if (args.size() < 2)
    {
        DBReport("syntax: tree <start_addr> \n");
        DBReport("create call tree of function at `start_addr`, including subcalls and dump it into calltree.txt\n");
        DBReport("`start_addr` can be symbolic or direct address.\n");
        DBReport("example of use: tree main\n");
    }
    else
    {
        if(!emu.loaded)
        {
            DBReport("not loaded\n");
            return nullptr;
        }

        start_addr = SYMAddress(args[1].c_str());
        if ( start_addr == 0 ) start_addr = strtoul ( args[1].c_str(), NULL, 0 );

        DBReport( "Creating call tree from 0x%08X\n", start_addr );

        if ( callhist ) {
            free (callhist);
            callhist_count = 0;
        }

        f = nullptr;
        fopen_s ( &f, "Data\\calltree.txt", "wt" );
        dump_subcalls ( start_addr, f, 0 );
        fclose ( f );
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// u

Json::Value* cmd_u(std::vector<std::string>& args)
{
    if(args.size() < 2)
    {
        DBReport("syntax : u <addr> OR u <symbol> OR u lr OR u ctr\n");
        DBReport("numbers can be decimal, or hex with \'0x\' prefix.\n");
        DBReport("examples of use : u 0x8023bc00\n");
        DBReport("                  u main\n");
        DBReport("                  u lr\n");
        DBReport("see also        : . ENTER-key ESC-key\n");
    }
    else
    {
        uint32_t addr = 0;

        // first check for link/counter registers
        if(!_stricmp(args[1].c_str(), "lr"))
        {
            con_set_disa_cur(Gekko::Gekko->regs.spr[(int)Gekko::SPR::LR]);
            return nullptr;
        }
        if(!_stricmp(args[1].c_str(), "ctr"))
        {
            con_set_disa_cur(Gekko::Gekko->regs.spr[(int)Gekko::SPR::CTR]);
            return nullptr;
        }

        // now check for symbol
        addr = SYMAddress(args[1].c_str());
        if(addr)
        {
            con_set_disa_cur(addr);
            return nullptr;
        }

        // simply address
        con_set_disa_cur(strtoul(args[1].c_str(), NULL, 0));
    }

    return nullptr;
}
