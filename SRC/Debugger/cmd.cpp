// command processor
#include "pch.h"

// ---------------------------------------------------------------------------

void cmd_init_handlers()
{
    con.cmds["."] = cmd_showpc;
    con.cmds["*"] = cmd_showldst;
    con.cmds["blr"] = cmd_blr;
    con.cmds["boot"] = cmd_boot;
    con.cmds["d"] = cmd_d;
    con.cmds["denop"] = cmd_denop;
    con.cmds["disa"] = cmd_disa;
    con.cmds["dop"] = cmd_dop;
    con.cmds["dvdopen"] = cmd_dvdopen;
    con.cmds["full"] = cmd_full;
    con.cmds["help"] = cmd_help;
    con.cmds["log"] = cmd_log;
    con.cmds["logfile"] = cmd_logfile;
    con.cmds["lr"] = cmd_lr;
    con.cmds["name"] = cmd_name;
    con.cmds["nop"] = cmd_nop;
    con.cmds["ostest"] = cmd_ostest;
    con.cmds["plist"] = cmd_plist;
    con.cmds["r"] = cmd_r;
    con.cmds["savemap"] = cmd_savemap;
    con.cmds["script"] = cmd_script;
    con.cmds["sd1"] = cmd_sd1;
    con.cmds["sd2"] = cmd_sd2;
    con.cmds["sop"] = cmd_sop;
    con.cmds["stat"] = cmd_stat;
    con.cmds["syms"] = cmd_syms;
    con.cmds["top10"] = cmd_top10;
    con.cmds["tree"] = cmd_tree;
    con.cmds["u"] = cmd_u;
    con.cmds["unload"] = cmd_unload;
    con.cmds["sleep"] = cmd_sleep;
    con.cmds["exit"] = cmd_exit;
    con.cmds["quit"] = cmd_exit;
    con.cmds["q"] = cmd_exit;
    con.cmds["x"] = cmd_exit;

    Debug::gekko_init_handlers();
    Debug::dsp_init_handlers();
    Debug::hw_init_handlers();
}

// ---------------------------------------------------------------------------
// help

void cmd_help(std::vector<std::string>& args)
{
    DBReport2(DbgChannel::Header, "## cpu debug commands\n");
    DBReport( "    .                    - view code at pc\n");
    DBReport( "    *                    - view data, pointed by load/store opcode\n");
    DBReport( "    u                    - view code at specified address\n");
    DBReport( "    d                    - view data at specified address\n");
    DBReport( "    sd1                  - quick form of d-command, using SDA1\n");
    DBReport( "    sd2                  - quick form of d-command, using SDA2\n");
    DBReport( "    r                    - show / change CPU register\n");
    DBReport( "    ps                   - show / change paired-single register\n");
    DBReport( "    pc                   - set program counter\n");

    DBReport( "    nop                  - insert NOP opcode at cursor\n");
    DBReport( "    denop                - restore old NOP'ed value\n");
    DBReport( "    blr [value]          - insert BLR opcode at cursor (with value)\n");
    //  DBReport( "    b <addr>             - toggle code breakpoint at <addr> (max=%i)\n", MAX_BPNUM);
    //  DBReport( "    bm [8|16|32] <addr>  - toggle data breakpoint at <addr> (max=%i)\n", MAX_BPNUM);
    DBReport( "    bc                   - clear all code breakpoints\n");
    //  DBReport( "    bmc                  - clear all data breakpoints\n");
    DBReport( "    run                  - execute until next breakpoint\n");
    DBReport( "    stop                 - stop debugging\n");
    DBReport( "    reset                - reset emulator\n");
    DBReport("\n");

    Debug::gekko_help();
    Debug::hw_help();
    Debug::dsp_help();

    DBReport2(DbgChannel::Header, "## high-level commands\n");
    DBReport( "    stat                 - show hardware state/statistics\n");
    DBReport( "    syms                 - list symbolic information\n");
    DBReport( "    name                 - name function (add symbol)\n");
    DBReport( "    dvdopen              - get file position (use DVD plugin)\n");
    DBReport( "    ostest               - test OS internals\n");
    DBReport( "    top10                - show HLE calls toplist\n");
    //  DBReport( "    alarm                - generate decrementer exception\n");
    //  DBReport( "    bcb                  - list all registered branch callbacks\n");    
    //  DBReport( "    hlestats             - show HLE-subsystem state\n");
    DBReport( "    savemap              - save symbolic map into file\n");
    DBReport("\n");

    DBReport2(DbgChannel::Header, "## Patch controls\n");
    DBReport( "    dop                  - apply patches immediately (only with freeze=1)\n");
    DBReport( "    plist                - list all patch data\n");
    DBReport( "    pload                - load patch file (unload previous)\n");
    DBReport( "    padd                 - add patch file (do not unload previous)\n");
    DBReport( "    patch                - insert memory patch\n");
    DBReport("\n");

    DBReport2(DbgChannel::Header, "## Misc commands\n");
    DBReport( "    boot                 - boot DVD/executable (from file or list)\n");
    DBReport( "    reboot               - reload last file\n");
    DBReport( "    unload               - unload current file\n");
    DBReport( "    sop                  - search opcode (forward) from cursor address\n");
    DBReport( "    lr                   - show LR back chain (\"branch history\")\n");
    DBReport( "    script               - execute batch script\n");
    //  DBReport( "    mapmem               - add memory mapper\n");
    DBReport( "    log                  - enable/disable log output)\n");
    DBReport( "    logfile              - choose HTML log-file for ouput\n");
    DBReport( "    full                 - set full screen console mode\n");
    DBReport( "    shot                 - take screenshot in ????.bmp\n");
    DBReport( "    memst                - memory allocation stats\n");
    DBReport( "    memtst               - memory test (WARNING)\n");
    DBReport( "    cls                  - clear message buffer\n");
    DBReport( "    colors               - colored output test\n");
    DBReport( "    disa                 - disassemble code into text file\n");
    DBReport( "    tree                 - show call tree\n");
    DBReport( "    sleep                - Sleep specified number of milliseconds\n");
    DBReport( "    [q]uit, e[x]it       - exit to OS\n");
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
}

// ---------------------------------------------------------------------------
// special

void cmd_showpc(std::vector<std::string>& args)
{
    con_set_disa_cur(PC);
}

void cmd_showldst(std::vector<std::string>& args)
{
    if (wind.ldst)
    {
        con.data = wind.ldst_disp;
        con.update |= CON_UPDATE_DATA;
    }
}

void cmd_unload(std::vector<std::string>& args)
{
    if (emu.running)
    {
        EMUClose();
    }
    else DBReport("not loaded.\n");
}

void cmd_exit(std::vector<std::string>& args)
{
    DBReport(": exiting...\n");
    con_refresh();
    Sleep(10);
    EMUClose();
    EMUDie();
    exit(0);
}

// ---------------------------------------------------------------------------
// blr

void cmd_blr(std::vector<std::string>& args)
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
        if(emu.running) return;

        uint32_t ea = con.disa_cursor;
        uint32_t pa = MEMEffectiveToPhysical(ea, 0);
        if(pa == -1) return;

        uint32_t op = MEMSwap(*(uint32_t*)(&mi.ram[pa]));
        if(op == 0x4e800020) return;

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
}

// ---------------------------------------------------------------------------
// boot

void cmd_boot(std::vector<std::string>& args)
{
    if(args.size() < 2)
    {
        DBReport("syntax : boot <file>\n");
        DBReport("path can be relative\n");
        DBReport("examples of use : boot c:\\luigimansion.gcm\n");
        DBReport("                  boot PONG.dol\n");
    }
    else
    {
        char filepath[0x1000];
        
        strncpy_s(filepath, sizeof(filepath), args[1].c_str(), 255);

        FILE* f = nullptr;
        fopen_s(&f, filepath, "rb");
        if(!f)
        {
            DBReport("file not exist! filepath=%s\n", filepath);
            return;
        }
        else fclose(f);

        LoadFile(filepath);
        EMUClose();
        EMUOpen();
    }
}

// ---------------------------------------------------------------------------
// d

void cmd_d(std::vector<std::string>& args)
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
            reg = GPR[n];
            if(args.size() >= 3) ofs = strtoul(args[2].c_str(), NULL, 0);
            ofs &= 0xffff;
            if(ofs & 0x8000) ofs |= 0xffff0000;
            addr = reg + (int32_t)ofs;
            con.data = addr;
            return;
        }

        // now check for symbol
        addr = SYMAddress(args[1].c_str());
        if(addr)
        {
            con.data = addr;
            return;
        }

        // simply address
        con.data = strtoul(args[1].c_str(), NULL, 0);
    }
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

void cmd_disa(std::vector<std::string>& args)
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
        if(!emu.running)
        {
            DBReport("not loaded\n");
        }

        start_addr = strtoul ( args[1].c_str(), NULL, 0 );
        sa = start_addr;
        end_addr = strtoul ( args[2].c_str(), NULL, 0 );

        f = nullptr;
        fopen_s ( &f, "Data\\disa.txt", "wt" );
        if (!f)
        {
            DBReport( "Cannot open output file!\n");
            return;
        }

        for (start_addr; start_addr<end_addr; start_addr+=4)
        {
            uint32_t opcode;
            MEMFetch(start_addr, &opcode);
            disa_line ( f, opcode, start_addr );
        }

        DBReport( "Disassembling from 0x%08X to 0x%08X... done\n", sa, end_addr );
        fclose (f);
    }
}

// ---------------------------------------------------------------------------
// dop

void cmd_dop(std::vector<std::string>& args)
{
    if(ldat.patchNum == 0)
    {
        DBReport("no patch data loaded.\n");
        return;
    }
    else ApplyPatches();
}

// ---------------------------------------------------------------------------
// dvdopen

void cmd_dvdopen(std::vector<std::string>& args)
{
    if(args.size() < 2)
    {
        DBReport("syntax : dvdopen <file>\n");
        DBReport("path must be absolute, including root prefix '/'\n");
        DBReport("examples of use : dvdopen \"/opening.bnr\"\n");
        DBReport("                  dvdopen \"/gxTests/tex-02/ia8_odd.tpl\"\n");
    }
    else
    {
        uint32_t ofs = DVDOpenFile(args[1].c_str());
        if(ofs) DBReport("0x%08X : %s\n", ofs, args[1].c_str());
        else DBReport("not found : %s\n", args[1].c_str());
    }
}

// ---------------------------------------------------------------------------
// full

void cmd_full(std::vector<std::string>& args)
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
}

// ---------------------------------------------------------------------------
// log

void cmd_log(std::vector<std::string>& args)
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
}

// ---------------------------------------------------------------------------
// logfile

void cmd_logfile(std::vector<std::string>& args)
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
}

// ---------------------------------------------------------------------------
// lr

void cmd_lr(std::vector<std::string>& args)
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

        if(!emu.running || !SP)
        {
            DBReport("not running, or no calls.\n");
            return;
        }

        int level = atoi(args[1].c_str());
        if(args[1].c_str()[0] == '*' || level > MAX_LEVEL) level = MAX_LEVEL;
        MEMReadWord(SP, &sp);
        if(level == MAX_LEVEL) DBReport( "LR Back Chain (max levels) :\n");
        else DBReport( "LR Back Chain (%i levels) :\n", level);

        for(int i=0; i<level; i++)
        {
            uint32_t read_pc;
            MEMReadWord(sp+4, &read_pc);    // read LR value from stack
            disa.pc = read_pc;
            disa.pc -= 4;                   // set to branch opcode
            MEMReadWord((uint32_t)disa.pc, &disa.instr); // read branch
            PPCDisasm (&disa);                    // disasm
            if(disa.iclass & PPC_DISA_BRANCH)
            {
                char * symbol = SYMName((uint32_t)disa.target);
                if(symbol) DBReport("%-3i: %-12s%-12s (%s)\n",
                                      i+1, disa.mnemonic, disa.operands, symbol );
                else       DBReport("%-3i: %-12s%-12s " "\n",
                                      i+1, disa.mnemonic, disa.operands );
            }
            MEMReadWord(sp, &sp);           // walk stack
            if(!sp || MEMEffectiveToPhysical(sp, 0) == -1) break;
        }
        #undef MAX_LEVEL
    }
}

// ---------------------------------------------------------------------------
// name

void cmd_name(std::vector<std::string>& args)
{
    if(args.size() < 3)
    {
        DBReport("syntax : name <addr> <symbol> OR name . <symbol> OR name * <symbol>\n");
        DBReport("give name to function or memory variable (add symbol).\n");
        DBReport("when using . (dot), symbol will be added by cursor address position.\n");
        DBReport("when using * , symbol will be added by branch relocation.\n");
        DBReport("examples of use : name 0x80003100 __start\n");
        DBReport("                  name . main\n");
        DBReport("                  name * OSCall\n");
        DBReport("see also        : syms savemap loadmap addmap\n");
    }
    else
    {
        uint32_t address = 0;
        if(args[1].c_str()[0] == '*')
        {
            uint32_t branchAddr = con.disa_cursor, op;
            uint32_t pa = MEMEffectiveToPhysical(branchAddr, 0);
            if(pa != -1)
            {
                PPCD_CB disa;
                MEMReadWord(branchAddr, &op);
                disa.pc = branchAddr;
                disa.instr = op;
                PPCDisasm (&disa);
                if(disa.iclass & PPC_DISA_BRANCH)
                    address = (uint32_t)disa.target;
            }
            else address = 0;
        }
        else if(args[1].c_str()[0] == '.') address = con.disa_cursor;
        else address = strtoul(args[1].c_str(), NULL, 0);
        if(address != 0)
        {
            DBReport("new symbol: %08X %s\n", address, args[2].c_str());
            SYMAddNew(address, args[2].c_str());
            con.update |= CON_UPDATE_ALL;
        }
        else DBReport("wrong address!\n");
    }
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

void cmd_nop(std::vector<std::string>& args)
{
    if(emu.running) return;

    uint32_t ea = con.disa_cursor;
    uint32_t pa = MEMEffectiveToPhysical(ea, 0);
    if(pa == -1) return;
    
    uint32_t old = MEMSwap(*(uint32_t*)(&mi.ram[pa]));
    mi.ram[pa] = 0x60;
    mi.ram[pa+1] = mi.ram[pa+2] = mi.ram[pa+3] = 0;
    add_nop(ea, old);

    con.update |= (CON_UPDATE_DISA | CON_UPDATE_DATA);
}

void cmd_denop(std::vector<std::string>& args)
{
    if(emu.running) return;

    uint32_t ea = con.disa_cursor;
    uint32_t pa = MEMEffectiveToPhysical(ea, 0);
    if(pa == -1) return;

    uint32_t old = get_nop(ea);
    if(old == 0) return;
    mi.ram[pa+0] = (uint8_t)(old >> 24);
    mi.ram[pa+1] = (uint8_t)(old >> 16);
    mi.ram[pa+2] = (uint8_t)(old >>  8);
    mi.ram[pa+3] = (uint8_t)(old >>  0);

    con.update |= (CON_UPDATE_DISA | CON_UPDATE_DATA);
}

// ---------------------------------------------------------------------------
// ostest

void cmd_ostest(std::vector<std::string>& args)
{
    OSCheckContextStruct();
}

// ---------------------------------------------------------------------------
// plist

void cmd_plist(std::vector<std::string>& args)
{
    if(ldat.patchNum == 0)
    {
        DBReport("no patch data loaded.\n");
        return;
    }

    DBReport("i----addr-----data-------------s-f-\n");
    for(uint32_t i=0; i<ldat.patchNum; i++)
    {
        Patch * p = &ldat.patches[i];
        uint8_t * data = (uint8_t *)&p->data;
        const char * fmt = "%.3i: %08X %02X%02X%02X%02X%02X%02X%02X%02X %i %i\n";

        switch(p->dataSize)
        {
            case PATCH_SIZE_8:
                fmt = "%.3i: %08X %02X %02X%02X%02X%02X%02X%02X%02X %i %i\n";
                break;
            case PATCH_SIZE_16:
                fmt = "%.3i: %08X %02X%02X %02X%02X%02X%02X%02X%02X %i %i\n";
                break;
            case PATCH_SIZE_32:
                fmt = "%.3i: %08X %02X%02X%02X%02X %02X%02X%02X%02X %i %i\n";
                break;
            case PATCH_SIZE_64:
                fmt = "%.3i: %08X %02X%02X%02X%02X%02X%02X%02X%02X %i %i\n";
                break;
            default:
                fmt = "PATCH DAMAGED!";
        }

        DBReport(
            fmt,
            i+1,
            MEMSwap(p->effectiveAddress),
            data[0], data[1], data[2], data[3],
            data[4], data[5], data[6], data[7],
            MEMSwapHalf(p->dataSize), MEMSwapHalf(p->freeze) & 1
        );
    }
    DBReport("-----------------------------------\n");
}

// ---------------------------------------------------------------------------
// r

// Get pointer to Gekko register.
static uint32_t *getreg (const char *name)
{
    if (!_stricmp(name, "r0")) return &GPR[0];
    else if (!_stricmp(name, "r1")) return &GPR[1];
    else if (!_stricmp(name, "r2")) return &GPR[2];
    else if (!_stricmp(name, "r3")) return &GPR[3];
    else if (!_stricmp(name, "r4")) return &GPR[4];
    else if (!_stricmp(name, "r5")) return &GPR[5];
    else if (!_stricmp(name, "r6")) return &GPR[6];
    else if (!_stricmp(name, "r7")) return &GPR[7];
    else if (!_stricmp(name, "r8")) return &GPR[8];
    else if (!_stricmp(name, "r9")) return &GPR[9];
    else if (!_stricmp(name, "r10")) return &GPR[10];
    else if (!_stricmp(name, "r11")) return &GPR[11];
    else if (!_stricmp(name, "r12")) return &GPR[12];
    else if (!_stricmp(name, "r13")) return &GPR[13];
    else if (!_stricmp(name, "r14")) return &GPR[14];
    else if (!_stricmp(name, "r15")) return &GPR[15];
    else if (!_stricmp(name, "r16")) return &GPR[16];
    else if (!_stricmp(name, "r17")) return &GPR[17];
    else if (!_stricmp(name, "r18")) return &GPR[18];
    else if (!_stricmp(name, "r19")) return &GPR[19];
    else if (!_stricmp(name, "r20")) return &GPR[20];
    else if (!_stricmp(name, "r21")) return &GPR[21];
    else if (!_stricmp(name, "r22")) return &GPR[22];
    else if (!_stricmp(name, "r23")) return &GPR[23];
    else if (!_stricmp(name, "r24")) return &GPR[24];
    else if (!_stricmp(name, "r25")) return &GPR[25];
    else if (!_stricmp(name, "r26")) return &GPR[26];
    else if (!_stricmp(name, "r27")) return &GPR[27];
    else if (!_stricmp(name, "r28")) return &GPR[28];
    else if (!_stricmp(name, "r29")) return &GPR[29];
    else if (!_stricmp(name, "r30")) return &GPR[30];
    else if (!_stricmp(name, "r31")) return &GPR[31];

    else if (!_stricmp(name, "sp")) return &GPR[1];
    else if (!_stricmp(name, "sd1")) return &GPR[13];
    else if (!_stricmp(name, "sd2")) return &GPR[2];

    else if (!_stricmp(name, "cr")) return &PPC_CR;
    else if (!_stricmp(name, "fpscr")) return &FPSCR;
    else if (!_stricmp(name, "xer")) return &XER;
    else if (!_stricmp(name, "lr")) return &PPC_LR;
    else if (!_stricmp(name, "ctr")) return &CTR;
    else if (!_stricmp(name, "msr")) return &MSR;

    else if (!_stricmp(name, "sr0")) return &PPC_SR[0];
    else if (!_stricmp(name, "sr1")) return &PPC_SR[1];
    else if (!_stricmp(name, "sr2")) return &PPC_SR[2];
    else if (!_stricmp(name, "sr3")) return &PPC_SR[3];
    else if (!_stricmp(name, "sr4")) return &PPC_SR[4];
    else if (!_stricmp(name, "sr5")) return &PPC_SR[5];
    else if (!_stricmp(name, "sr6")) return &PPC_SR[6];
    else if (!_stricmp(name, "sr7")) return &PPC_SR[7];
    else if (!_stricmp(name, "sr8")) return &PPC_SR[8];
    else if (!_stricmp(name, "sr9")) return &PPC_SR[9];
    else if (!_stricmp(name, "sr10")) return &PPC_SR[10];
    else if (!_stricmp(name, "sr11")) return &PPC_SR[11];
    else if (!_stricmp(name, "sr12")) return &PPC_SR[12];
    else if (!_stricmp(name, "sr13")) return &PPC_SR[13];
    else if (!_stricmp(name, "sr14")) return &PPC_SR[14];
    else if (!_stricmp(name, "sr15")) return &PPC_SR[15];

    else if (!_stricmp(name, "ibat0u")) return &IBAT0U;
    else if (!_stricmp(name, "ibat1u")) return &IBAT1U;
    else if (!_stricmp(name, "ibat2u")) return &IBAT2U;
    else if (!_stricmp(name, "ibat3u")) return &IBAT3U;
    else if (!_stricmp(name, "ibat0l")) return &IBAT0L;
    else if (!_stricmp(name, "ibat1l")) return &IBAT1L;
    else if (!_stricmp(name, "ibat2l")) return &IBAT2L;
    else if (!_stricmp(name, "ibat3l")) return &IBAT3L;
    else if (!_stricmp(name, "dbat0u")) return &DBAT0U;
    else if (!_stricmp(name, "dbat1u")) return &DBAT1U;
    else if (!_stricmp(name, "dbat2u")) return &DBAT2U;
    else if (!_stricmp(name, "dbat3u")) return &DBAT3U;
    else if (!_stricmp(name, "dbat0l")) return &DBAT0L;
    else if (!_stricmp(name, "dbat1l")) return &DBAT1L;
    else if (!_stricmp(name, "dbat2l")) return &DBAT2L;
    else if (!_stricmp(name, "dbat3l")) return &DBAT3L;

    else if (!_stricmp(name, "sdr1")) return &SDR1;
    else if (!_stricmp(name, "sprg0")) return &SPRG0;
    else if (!_stricmp(name, "sprg1")) return &SPRG1;
    else if (!_stricmp(name, "sprg2")) return &SPRG2;
    else if (!_stricmp(name, "sprg3")) return &SPRG3;
    else if (!_stricmp(name, "dar")) return &PPC_DAR;
    else if (!_stricmp(name, "dsisr")) return &DSISR;
    else if (!_stricmp(name, "srr0")) return &SRR0;
    else if (!_stricmp(name, "srr1")) return &SRR1;
    else if (!_stricmp(name, "pmc1")) return &SPR[953];
    else if (!_stricmp(name, "pmc2")) return &SPR[954];
    else if (!_stricmp(name, "pmc3")) return &SPR[957];
    else if (!_stricmp(name, "pmc4")) return &SPR[958];
    else if (!_stricmp(name, "mmcr0")) return &SPR[952];
    else if (!_stricmp(name, "mmcr1")) return &SPR[956];
    else if (!_stricmp(name, "sia")) return &SPR[955];
    else if (!_stricmp(name, "sda")) return &SPR[959];

    else if (!_stricmp(name, "gqr0")) return &GQR[0];
    else if (!_stricmp(name, "gqr1")) return &GQR[1];
    else if (!_stricmp(name, "gqr2")) return &GQR[2];
    else if (!_stricmp(name, "gqr3")) return &GQR[3];
    else if (!_stricmp(name, "gqr4")) return &GQR[4];
    else if (!_stricmp(name, "gqr5")) return &GQR[5];
    else if (!_stricmp(name, "gqr6")) return &GQR[6];
    else if (!_stricmp(name, "gqr7")) return &GQR[7];

    else if (!_stricmp(name, "hid0")) return &HID0;
    else if (!_stricmp(name, "hid1")) return &HID1;
    else if (!_stricmp(name, "hid2")) return &HID2;

    else if (!_stricmp(name, "dabr")) return &DABR;
    else if (!_stricmp(name, "iabr")) return &IABR;
    else if (!_stricmp(name, "wpar")) return &WPAR;
    else if (!_stricmp(name, "l2cr")) return &SPR[1017];
    else if (!_stricmp(name, "dmau")) return &DMAU;
    else if (!_stricmp(name, "dmal")) return &DMAL;
    else if (!_stricmp(name, "thrm1")) return &SPR[1020];
    else if (!_stricmp(name, "thrm2")) return &SPR[1021];
    else if (!_stricmp(name, "thrm2")) return &SPR[1022];
    else if (!_stricmp(name, "ictc")) return &SPR[1019];

    else if (!_stricmp(name, "pc")) return &PC;   // Wow !

    return NULL;
}

// Operations.
static uint32_t op_replace (uint32_t a, uint32_t b) { return b; }
static uint32_t op_add (uint32_t a, uint32_t b) { return a+b; }
static uint32_t op_sub (uint32_t a, uint32_t b) { return a-b; }
static uint32_t op_mul (uint32_t a, uint32_t b) { return a*b; }
static uint32_t op_div (uint32_t a, uint32_t b) { return b?(a/b):0; }
static uint32_t op_or (uint32_t a, uint32_t b) { return a|b; }
static uint32_t op_and (uint32_t a, uint32_t b) { return a&b; }
static uint32_t op_xor (uint32_t a, uint32_t b) { return a^b; }
static uint32_t op_shl (uint32_t a, uint32_t b) { return a<<b; }
static uint32_t op_shr (uint32_t a, uint32_t b) { return a>>b; }

// Special handling for MSR register.
static void describe_msr (uint32_t msr_val)
{
    static const char *fpmod[4] = 
    {
        "exceptions disabled",
        "imprecise nonrecoverable",
        "imprecise recoverable",
        "precise mode",                        
    };
    int f, fe[2];

    DBReport("MSR: 0x%08X\n", msr_val);
    
    if(msr_val & MSR_POW) DBReport("MSR[POW]: 1, power management enabled\n");
    else DBReport("MSR[POW]: 0, power management disabled\n");
    if(msr_val & MSR_ILE) DBReport("MSR[ILE]: 1\n");
    else DBReport("MSR[ILE]: 0\n");
    if(msr_val & MSR_EE) DBReport("MSR[EE] : 1, external interrupts and decrementer exception are enabled\n");
    else DBReport("MSR[EE] : 0, external interrupts and decrementer exception are disabled\n");
    if(msr_val & MSR_PR) DBReport("MSR[PR] : 1, processor execute in user mode (UISA)\n");
    else DBReport("MSR[PR] : 0, processor execute in supervisor mode (OEA)\n");
    if(msr_val & MSR_FP) DBReport("MSR[FP] : 1, floating-point is available\n");
    else DBReport("MSR[FP] : 0, floating-point unavailable\n");
    if(msr_val & MSR_ME) DBReport("MSR[ME] : 1, machine check exceptions are enabled\n");
    else DBReport("MSR[ME] : 0, machine check exceptions are disabled\n");
    
    fe[0] = msr_val & MSR_FE0 ? 1 : 0;
    fe[1] = msr_val & MSR_FE1 ? 1 : 0;
    f = (fe[0] << 1) | (fe[1]);
    DBReport("MSR[FE] : %i, floating-point %s\n", f, fpmod[f]);
    
    if(msr_val & MSR_SE) DBReport("MSR[SE] : 1, single-step tracing is enabled\n");
    else DBReport("MSR[SE] : 0, single-step tracing is disabled\n");
    if(msr_val & MSR_BE) DBReport("MSR[BE] : 1, branch tracing is enabled\n");
    else DBReport("MSR[BE] : 0, branch tracing is disabled\n");
    if(msr_val & MSR_IP) DBReport("MSR[IP] : 1, exception prefix to physical address is 0xFFFn_nnnn\n");
    else DBReport("MSR[IP] : 0, exception prefix to physical address is 0x000n_nnnn\n");
    if(msr_val & MSR_IR) DBReport("MSR[IR] : 1, instruction address translation is enabled\n");
    else DBReport("MSR[IR] : 0, instruction address translation is disabled\n");
    if(msr_val & MSR_DR) DBReport("MSR[DR] : 1, data address translation is enabled\n");
    else DBReport("MSR[DR] : 0, data address translation is disabled\n");
    if(msr_val & MSR_PM) DBReport("MSR[PM] : 1, performance monitoring is enabled for this thread\n");
    else DBReport("MSR[PM] : 0, performance monitoring is disabled for this thread\n");
    if(msr_val & MSR_RI) DBReport("MSR[RI] : 1\n");
    else DBReport("MSR[RI] : 0\n");
    if(msr_val & MSR_LE) DBReport("MSR[LE] : 1, processor runs in little-endian mode\n");
    else DBReport("MSR[LE] : 0, processor runs in big-endian mode\n");
}

void cmd_r (std::vector<std::string>& args)
{
    if(args.size() < 2)
    {
        DBReport("Syntax : r <reg> OR r <reg> <op> <val> OR r <reg> <op> <reg>\n");
        DBReport("sp, sd1, sd2 semantics are supported for reg name.\n");
        DBReport("Value can be decimal, or hex with \'0x\' prefix.\n");
        DBReport("Possible operations are: = + - * / | & ^ << >>\n");
        DBReport("Examples of use : r sp\n");
        DBReport("                  r r3 = 12\n");
        DBReport("                  r r7 | 0x8020\n");
        DBReport("                  r msr\n");
        DBReport("                  r hid2 | 2\n");
        DBReport("                  r r7 = sd1\n");
        DBReport("See also        : fr\n");
    }
    else
    {
        uint32_t (*op)(uint32_t a, uint32_t b) = NULL;

        uint32_t *n = getreg (args[1].c_str());
        if(n == NULL)
        {
            DBReport("unknown register : %s\n", args[1].c_str());
            return;
        }

        // show register
        if(args.size() <= 3)
        {
            if (!_stricmp (args[1].c_str(), "msr")) describe_msr (*n);
            else DBReport("%s = %i (0x%X)\n", args[1].c_str(), *n, *n);
            return;
        }

        // Get operation.
        if (!strcmp (args[2].c_str(), "=")) op = op_replace;
        else if (!strcmp (args[2].c_str(), "+")) op = op_add;
        else if (!strcmp (args[2].c_str(), "-")) op = op_sub;
        else if (!strcmp (args[2].c_str(), "*")) op = op_mul;
        else if (!strcmp (args[2].c_str(), "/")) op = op_div;
        else if (!strcmp (args[2].c_str(), "|")) op = op_or;
        else if (!strcmp (args[2].c_str(), "&")) op = op_and;
        else if (!strcmp (args[2].c_str(), "^")) op = op_xor;
        else if (!strcmp (args[2].c_str(), "<<")) op = op_shl;
        else if (!strcmp (args[2].c_str(), ">>")) op = op_shr;
        if (op == NULL)
        {
            DBReport("Unknown operation: %s\n", args[2].c_str());
            return;
        }

        // New value
        uint32_t *m = getreg (args[3].c_str());
        if (m == NULL)
        {
            int i = strtoul (args[3].c_str(), NULL, 0);
            DBReport("%s %s %i (0x%X)\n", args[1].c_str(), args[2].c_str(), i, i);
            *n = op(*n, i);
        }
        else
        {
            DBReport("%s %s %s\n", args[1].c_str(), args[2].c_str(), args[3].c_str());
            *n = op(*n, *m);
        }
        con_update(CON_UPDATE_REGS | CON_UPDATE_DISA);
    }
}

// ---------------------------------------------------------------------------
// sop

void cmd_sop(std::vector<std::string>& args)
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
            uint32_t pa = MEMEffectiveToPhysical(saddr, 0);
            if(pa != -1) MEMFetch(pa, &op);
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
}

// ---------------------------------------------------------------------------
// stat

void cmd_stat(std::vector<std::string>& args)
{
    if(args.size() < 2)
    {
        DBReport("syntax : stat <dev>\n");
        DBReport("dev is specified device :\n");
        DBReport("    cpu  : Gekko processor\n");
        DBReport("    fifo : GX command processor\n");
        DBReport("    vi   : video interface\n");
        DBReport("    pi   : interrupts\n");
        DBReport("    mi   : Flipper memory protection\n");
        DBReport("    ax   : DSP, audio and streaming\n");
        DBReport("    di   : DVD\n");
        DBReport("    si   : serial interface (joypads)\n");
        DBReport("    exi  : EXI devices\n");
        DBReport("examples of use : stat cpu\n");
        DBReport("                : stat vi\n");
    }
    else
    {
        #define IFDEV(n) if(!strncmp(args[1].c_str(), n, strlen(n)))
        IFDEV("vi")
        {
            VIStats();
            return;
        }
        else DBReport("unknown device!\n");
        #undef IFDEV
    }
}

// ---------------------------------------------------------------------------
// syms

void cmd_syms(std::vector<std::string>& args)
{
    if(args.size() < 2)
    {
        DBReport("syntax : syms <string> OR syms *\n");
        DBReport("<string> is the first occurance of symbol to find.\n");
        DBReport("* - list all symbols (possible overflow of message buffer).\n");
        DBReport("examples of use : syms ma\n");
        DBReport("                  syms __z\n");
        DBReport("                  syms *\n");
        DBReport("see also        : name savemap loadmap addmap\n");
    }
    else
    {
        SYMList(args[1].c_str());
    }
}

// ---------------------------------------------------------------------------
// savemap

void cmd_savemap(std::vector<std::string>& args)
{
    if(args.size() < 2)
    {
        DBReport("syntax : savemap <file> OR savemap .\n");
        DBReport(". is used to update current loaded map.\n");
        DBReport("path can be relative\n");
        DBReport("examples of use : savemap .\n");
        DBReport("                  savemap data\\my.map\n");
        DBReport("see also        : name loadmap addmap\n");
    }
    else
    {
        if(!strcmp(args[1].c_str(), ".")) SaveMAP();
        else SaveMAP(args[1].c_str());
    }
}

// ---------------------------------------------------------------------------
// script

static int testempty(char *str)
{
    int i, len = (int)strlen(str);

    for(i=0; i<len; i++)
    {
        if(str[i] != 0x20) return 0;
    }

    return 1;
}

void cmd_script(std::vector<std::string>& args)
{
    int i;
    const char* file;
    std::vector<std::string> commandArgs;

    if (args.size() < 2)
    {
        DBReport("syntax : script <file>\n");
        DBReport("path can be relative\n");
        DBReport("examples of use : script data\\zelda.cmd\n");
        DBReport("                  script c:\\luigi.cmd\n");
        return;
    }

    file = args[1].c_str();

    DBReport("loading script: %s\n", file);

    // following code is copied from MAPLoad :)

    int size = FileSize(file);
    FILE* f = nullptr;
    fopen_s (&f, file, "rt");
    if(!f)
    {
        DBReport("cannot open script file!\n");
        return ;
    }

    // allocate memory
    char *sbuf = (char *)malloc(size + 1);
    if(sbuf == NULL)
    {
        fclose(f);
        DBReport(
            "Not enough memory to load script.\n"
            "file size : %ib\n\n",
            size
        );
        return;
    }

    // load from file
    fread(sbuf, size, 1, f);
    fclose(f);
    sbuf[size] = 0;

    // remove all garbage, like tabs
    for(i=0; i<size; i++)
    {
        if(sbuf[i] < ' ') sbuf[i] = '\n';
    }

    DBReport("executing script...\n");

    int cnt = 1;
    char *ptr = sbuf;
    while(*ptr)
    {
        char line[1000];
        line[i = 0] = 0;

        // cut string
        while(*ptr == '\n') ptr++;
        if(!*ptr) break;
        while(*ptr != '\n') line[i++] = *ptr++;
        line[i++] = 0;

        // remove comments
        char *p = line;
        while(*p)
        {
            if(p[0] == '/' && p[1] == '/')
            {
                *p = 0;
                break;
            }
            p++;
        }

        // remove spaces at the end
        p = &line[strlen(line) - 1];
        while(*p <= ' ') p--;
        if(*p) p[1] = 0;

        // remove spaces at the beginning
        p = line;
        while(*p <= ' ' && *p) p++;

        // empty string ?
        if(!*p) continue;

        // execute line
        if(testempty(line)) continue;
        DBReport("%i: %s", cnt++, line);
        con_tokenizing(line);
        line[0] = 0;
        con_update(CON_UPDATE_EDIT | CON_UPDATE_MSGS);

        commandArgs.clear();
        for (int i = 0; i < roll.tokencount; i++)
        {
            commandArgs.push_back(roll.tokens[i]);
        }

        con_command(commandArgs);
    }
    free(sbuf);

    DBReport( "\ndone execute script.\n");
    con.update |= CON_UPDATE_ALL;
}

// ---------------------------------------------------------------------------
// sd1, sd2

void cmd_sdCommon(int sd, std::vector<std::string>& args)
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
        if(sd == 1) sda = SDA1;
        else sda = SDA2;

        uint32_t ofs = strtoul(args[1].c_str(), NULL, 0);
        ofs &= 0xffff;
        if(ofs & 0x8000) ofs |= 0xffff0000;
        con.data = sda + (int32_t)ofs;
        con.update |= CON_UPDATE_DATA;
    }
}

void cmd_sd1(std::vector<std::string>& args)
{
    return cmd_sdCommon(1, args);
}

void cmd_sd2(std::vector<std::string>& args)
{
    return cmd_sdCommon(2, args);
}

// ---------------------------------------------------------------------------
// top10

void cmd_top10(std::vector<std::string>& args)
{
    HLEGetTop10(hle.top10);
    DBReport("HLE Greatest Hits!!\n");
    for(int i=0; i<10; i++)
    {
        DBReport(
            "%-2i: %s (%i calls)\n",
            i+1, HLEGetHitNameByIndex(hle.top10[i]), hle.hitrate[hle.top10[i]]
        );
    }
    HLEResetHitrate();
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
        MEMFetch(address, &opcode);
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

void cmd_tree (std::vector<std::string>& args)
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
        if(!emu.running)
        {
            DBReport("not loaded\n");
            return;
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
}

// ---------------------------------------------------------------------------
// u

void cmd_u(std::vector<std::string>& args)
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
            con_set_disa_cur(PPC_LR);
            return;
        }
        if(!_stricmp(args[1].c_str(), "ctr"))
        {
            con_set_disa_cur(CTR);
            return;
        }

        // now check for symbol
        addr = SYMAddress(args[1].c_str());
        if(addr)
        {
            con_set_disa_cur(addr);
            return;
        }

        // simply address
        con_set_disa_cur(strtoul(args[1].c_str(), NULL, 0));
    }
}

// Sleep specified number of milliseconds
void cmd_sleep(std::vector<std::string>& args)
{
    if (args.size() < 2)
    {
        DBReport("syntax : sleep <milliseconds>\n");
        DBReport("examples of use : sleep 1000\n");
        return;
    }

    Sleep(atoi(args[1].c_str()));
}
