// command processor
#include "dolphin.h"

// ---------------------------------------------------------------------------

#define ifname(name)        (stricmp(name, argv[0]) == 0)
#define iftoken(name, n)    (stricmp(name, argv[n]) == 0)
#define required(req)       if(req != (argc - 1)) { print("%u parameter(s) required\n", req); return; }
#define minrequired(req)    if(req > (argc - 1)) { print("at least %u parameter(s) required\n", req); return; }

void con_command(int argc, char argv[][CON_LINELEN], int lnum)
{
    // a-z order
    if(ifname("."))                         // set disassembly cursor to PC
    {
        con_set_disa_cur(PC);
        return;
    }
    else if(ifname("*"))                    // show data pointed by LD/ST-opcode
    {
        if(wind.ldst)
        {
            con.data = wind.ldst_disp;
            con.update |= CON_UPDATE_DATA;
        }
        return;
    }
    else if(ifname("blr"))                  // insert BLR instruction at cursor (with value)
    {
        cmd_blr(argc, argv);
        return;
    }
    else if(ifname("boot"))                 // load file
    {
        cmd_boot(argc, argv);
        return;
    }
    else if(ifname("d"))                    // show data at address
    {
        cmd_d(argc, argv);
        con.update |= CON_UPDATE_DATA;
        return;
    }
    else if(ifname("denop"))                // show data at address
    {
        cmd_denop();
        return;
    }
    else if(ifname("dop"))                  // apply patches
    {
        cmd_dop();
        return;
    }
    else if(ifname("dvdopen"))              // get file position (use DVD plugin)
    {
        cmd_dvdopen(argc, argv);
        return;
    }
    else if(ifname("foo"))                  // http://www.adom.de
    {
        switch(GetTickCount() & 1)
        {
            case 0: con_print("What do you wish for? phase daggers? potions of quickling blood?\n"); break;
            case 1: con_print("I punish you, weaker! Mwahaha-hhha-haaa!!!\n"); break;
        }
        return;
    }
    else if(ifname("full"))                 // full screen mode
    {
        cmd_full(argc, argv);
        return;
    }
    else if(ifname("help"))                 // help
    {
        cmd_help();
        return;
    }
    else if(ifname("log"))                  // log control
    {
        cmd_log(argc, argv);
        return;
    }
    else if(ifname("logfile"))              // set log file
    {
        cmd_logfile(argc, argv);
        return;
    }
    else if(ifname("lr"))                   // show LR back chain
    {
        cmd_lr(argc, argv);
        return;
    }
    else if(ifname("name"))                 // add symbol
    {
        cmd_name(argc, argv);
        return;
    }
    else if(ifname("nop"))                  // insert NOP instruction at cursor
    {
        cmd_nop();
        return;
    }
    else if(ifname("ostest"))               // test OS internals
    {
        cmd_ostest();
        return;
    }
    else if(ifname("plist"))                // list all patch data
    {
        cmd_plist();
        return;
    }
    else if(ifname("r"))                    // register operations (GPR)
    {
        cmd_r(argc, argv);
        con.update |= CON_UPDATE_REGS;
        return;
    }
    else if(ifname("savemap"))              // save symbolic map into file
    {
        cmd_savemap(argc, argv);
        return;
    }
    else if(ifname("script"))               // execute batch script
    {
        if(argc < 2)
        {
            con_print("syntax : script <file>\n");
            con_print("path can be relative\n");
            con_print("examples of use : " GREEN "script data\\zelda.s\n");
            con_print("                  " GREEN "script c:\\luigi.s\n");
        }
        else cmd_script(argv[1]);
        return;
    }
    else if(ifname("sd1"))                  // show data at "small data #1" register
    {
        cmd_sd(1, argc, argv);
        con.update |= CON_UPDATE_DATA;
        return;
    }
    else if(ifname("sd2"))                  // show data at "small data #2" register
    {
        cmd_sd(2, argc, argv);
        con.update |= CON_UPDATE_DATA;
        return;
    }
    else if(ifname("sop"))                  // search opcodes (down)
    {
        cmd_sop(argc, argv);
        con.update |= CON_UPDATE_DISA;
        return;
    }
    else if(ifname("stat"))                 // show hardware state/stats
    {
        cmd_stat(argc, argv);
        return;
    }
    else if(ifname("syms"))                 // show symbolic info
    {
        cmd_syms(argc, argv);
        return;
    }
    else if(ifname("top10"))                // show HLE calls toplist
    {
        cmd_top10();
        return;
    }
    else if(ifname("u"))                    // set disassembly address
    {
        cmd_u(argc, argv);
        return;
    }
    else if(ifname("unload"))               // unload file
    {
        if(emu.running)
        {
            if(wind.disamode == DISAMOD_X86)
            {
                wind.disamode = DISAMOD_PPC;
                con.update |= CON_UPDATE_DISA;
                con_refresh();
                Sleep(10);
            }
            SendMessage(wnd.hMainWindow, WM_COMMAND, ID_FILE_UNLOAD, 0);
        }
        else con_print("not loaded.\n");
        return;
    }
    else if(ifname("exit") || ifname("quit") || ifname("q") || ifname("x"))
    {
        if(wind.disamode == DISAMOD_X86)
        {
            wind.disamode = DISAMOD_PPC;
            con.update |= CON_UPDATE_DISA;
        }
        con_print(GREEN ": exiting...\n"), con_refresh(), Sleep(10);
        EMUClose();
        EMUDie();
        exit(1);
    }
    else
    {
        if(lnum) con_print("unknown script command in line %i, see \'" GREEN "help" NORM "\'", lnum);
        else con_print("unknown command, try \'" GREEN "help" NORM "\'");
    }
}

// ---------------------------------------------------------------------------
// blr

void cmd_blr_old()
{
    if(!emu.running) return;
    if(con.running) return;

    u32 ea = con.disa_cursor;
    u32 pa = MEMEffectiveToPhysical(ea, 0);
    if(pa == -1) return;
    RAM[pa+0] = 0x4e;   // BLR
    RAM[pa+1] = 0x80;
    RAM[pa+2] = 0;
    RAM[pa+3] = 0x20;

    con.update |= (CON_UPDATE_DISA | CON_UPDATE_DATA);
}

void cmd_blr(int argc, char argv[][CON_LINELEN])
{
    if(argc > 2)
    {
        con_print("syntax : blr [value]\n");
        con_print("when [value] is specified, pair of instructions is inserted : \n");
        con_print(GREEN "    " "li [value]\n");
        con_print(GREEN "    " "blr\n");
        con_print("(with exception, if BLR is already present at cursor.\n");
        con_print("16-bit value can be decimal, or hex with \'0x\' prefix.\n");
        con_print("examples of use : " GREEN "blr\n");
        con_print("                  " GREEN "blr 0\n");
        con_print("                  " GREEN "blr 1\n");
        con_print("see also        : " GREEN "nop\n");
    }
    else
    {
        if(!emu.running) return;
        if(con.running) return;

        u32 ea = con.disa_cursor;
        u32 pa = MEMEffectiveToPhysical(ea, 0);
        if(pa == -1) return;

        u32 op = MEMSwap(*(u32 *)(&RAM[pa]));
        if(op == 0x4e800020) return;

        int ofs = 0;
        if(argc >= 2)           // value, to simulate "return X"
        {
            u32 iVal = strtoul(argv[1], NULL, 0) & 0xffff;
            RAM[pa+0] = 0x38;
            RAM[pa+1] = 0;
            RAM[pa+2] = (u8)(iVal >> 8);
            RAM[pa+3] = (u8)iVal;
            ofs = 4;
        }
        
        RAM[pa+ofs+0] = 0x4e;   // BLR
        RAM[pa+ofs+1] = 0x80;
        RAM[pa+ofs+2] = 0;
        RAM[pa+ofs+3] = 0x20;

        con.update |= (CON_UPDATE_DISA | CON_UPDATE_DATA);
    }
}

// ---------------------------------------------------------------------------
// boot

void cmd_boot(int argc, char argv[][CON_LINELEN])
{
    if(argc < 2)
    {
        con_print("syntax : boot <file> OR boot {n}\n");
        con_print("path can be relative, n = 1..MAX\n");
        con_print("examples of use : " GREEN "boot c:\\luigimansion.gcm\n");
        con_print("                  " GREEN "boot PONG.dol\n");
        con_print("                  " GREEN "boot {1}\n");
    }
    else
    {
        char filepath[256];
        
        if(argv[1][0] == '{' && !emu.running)
        {
            int n = strtoul(&argv[1][1], NULL, 0);
            if(usel.filenum <= 0)
            {
                con_print("selector is empty (no files).\n");
                return;
            }
            if(n <= 0)
            {
                con_print("n must be 1..%i\n", usel.filenum);
                return;
            }
            if(n >= (usel.filenum + 1))
            {
                con_print("out of file list. n must be 1..%i\n", usel.filenum);
                return;
            }
            strncpy(filepath, usel.files[n-1].name, 255);
        }
        else strncpy(filepath, argv[1], 255);

        FILE *f = fopen(filepath, "rb");
        if(!f)
        {
            con_print("file not exist! filepath=%s\n", filepath);
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

void cmd_d(int argc, char argv[][CON_LINELEN])
{
    if(argc < 2)
    {
        con_print("syntax : d <addr> OR d <symbol> OR d <reg> [ofs]\n");
        con_print("numbers can be decimal, or hex with \'0x\' prefix.\n");
        con_print("offset is a 16-bit signed dec/hex number.\n");
        con_print("\'small data\' regs are used as r13 and r2.\n");
        con_print("examples of use : " GREEN "d 0x8023bc00\n");
        con_print("                  " GREEN "d main\n");
        con_print("                  " GREEN "d r5\n");
        con_print("                  " GREEN "d r9 0x8250\n");
        con_print("see also        : " GREEN "sd1 sd2 *\n");
    }
    else
    {
        u32 addr = 0;

        // first check for register form
        if(argv[1][0] == 'r' && isdigit(argv[1][1]))
        {
            u32 reg, ofs = 0;
            int n = strtoul(&argv[1][1], NULL, 10);
            reg = GPR[n];
            if(argc >= 3) ofs = strtoul(argv[2], NULL, 0);
            ofs &= 0xffff;
            if(ofs & 0x8000) ofs |= 0xffff0000;
            addr = reg + (s32)ofs;
            con.data = addr;
            return;
        }

        // now check for symbol
        addr = SYMAddress(argv[1]);
        if(addr)
        {
            con.data = addr;
            return;
        }

        // simply address
        con.data = strtoul(argv[1], NULL, 0);
    }
}

// ---------------------------------------------------------------------------
// dop

void cmd_dop()
{
    if(ldat.patchNum == 0)
    {
        con_print(YEL "no patch data loaded.\n");
        return;
    }
    else ApplyPatches();
}

// ---------------------------------------------------------------------------
// dvdopen

void cmd_dvdopen(int argc, char argv[][CON_LINELEN])
{
    if(argc < 2)
    {
        con_print("syntax : dvdopen <file>\n");
        con_print("path must be absolute, including root prefix '/'\n");
        con_print("examples of use : " GREEN "dvdopen \"/opening.bnr\"\n");
        con_print("                  " GREEN "dvdopen \"/gxTests/tex-02/ia8_odd.tpl\"\n");
    }
    else
    {
        u32 ofs = DVDOpenFile(argv[1]);
        if(ofs) con_print(GREEN "0x%08X : %s\n", ofs, argv[1]);
        else con_print(BRED "not found : %s\n", argv[1]);
    }
}

// ---------------------------------------------------------------------------
// full

void cmd_full(int argc, char argv[][CON_LINELEN])
{
    if(argc < 2)
    {
        con_print("syntax : full <0/1>\n");
        con_print("set \"fullscreen\" console mode.\n");
        con_print("examples of use : " GREEN "full 1\n");
    }
    else
    {
        con_fullscreen(atoi(argv[1]) & 1);
        con.update |= CON_UPDATE_ALL;
    }
}

// ---------------------------------------------------------------------------
// log

void cmd_log(int argc, char argv[][CON_LINELEN])
{
    if(argc < 2)
    {
        con_print("syntax : log [dev] <0/1>\n");
        con_print(GREEN "dev" NORM " is specified device :\n");
        con_print("    cpu  : Gekko processor\n");
        con_print("    fifo : GX command processor\n");
        con_print("    vi   : video interface\n");
        con_print("    pi   : interrupts\n");
        con_print("    mi   : Flipper memory protection\n");
        con_print("    ax   : DSP, audio and streaming\n");
        con_print("    di   : DVD\n");
        con_print("    si   : serial interface (joypads)\n");
        con_print("    exi  : EXI devices\n");
        con_print("examples of use : " GREEN "log 1\n");
        con_print("                : " GREEN "log 0\n");
        con_print("                : " GREEN "log pi 0\n");
        con_print("                : " GREEN "log ax 1\n");
        con_print("see also        : " GREEN "logfile\n");
    }
    else if(argc < 3)
    {
        con.log = atoi(argv[1]);
        if(con.log) con_print(GREEN "log enabled (logfile: %s)", con.logfile);
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
            con_print(GREEN "log disabled");
        }
    }
    else
    {
        #define IFDEV(n) if(!strncmp(argv[1], n, strlen(n)))
        IFDEV("cpu")
        {
            cpu.log = atoi(argv[2]);
            if(cpu.log) con_print(CPU "Gekko processor log enabled");
            else con_print(CPU "Gekko processor log disabled");
        }
        else con_print("unknown device!\n");
        #undef IFDEV
    }
}

// ---------------------------------------------------------------------------
// logfile

void cmd_logfile(int argc, char argv[][CON_LINELEN])
{
    if(argc < 2)
    {
        con_print("syntax : logfile <filename>\n");
        con_print("filename can be relative. default filename is %s\n", CON_LOG_FILE);
        con_print("examples of use : " GREEN "logfile log.htm\n");
        con_print("see also        : " GREEN "log\n");
    }
    else
    {
        strncpy(con.logfile, argv[1], sizeof(con.logfile));
        con_print("logging into " GREEN "%s\n", con.logfile);
    }
}

// ---------------------------------------------------------------------------
// lr

void cmd_lr(int argc, char argv[][CON_LINELEN])
{
    if(argc < 2)
    {
        con_print("syntax : lr <level>\n");
        con_print("level - chain depth (number of calls). use \'*\' to show whole chain.\n");
        con_print("examples of use : " GREEN "lr 10\n");
        con_print("                : " GREEN "lr *\n");
    }
    else
    {
        #define MAX_LEVEL 0xfff     // back chain limit
        PPCD_CB disa;
        u32 sp;

        if(!emu.running || !SP)
        {
            con_print("not running, or no calls.\n");
            return;
        }

        int level = atoi(argv[1]);
        if(argv[1][0] == '*' || level > MAX_LEVEL) level = MAX_LEVEL;
        MEMReadWord(SP, &sp);
        if(level == MAX_LEVEL) con_print( "LR Back Chain (max levels) :\n");
        else con_print( "LR Back Chain (%i levels) :\n", level);

        for(int i=0; i<level; i++)
        {
            u32 read_pc;
            MEMReadWord(sp+4, &read_pc);    // read LR value from stack
            disa.pc = read_pc;
            disa.pc -= 4;                   // set to branch opcode
            MEMReadWord(disa.pc, &disa.instr); // read branch
            PPCDisasm (&disa);                    // disasm
            if(disa.iclass & PPC_DISA_BRANCH)
            {
                char * symbol = SYMName(disa.target);
                if(symbol) con_print( NORM "%-3i: " GREEN "%-12s%-12s " BROWN "(%s)\n", 
                                      i+1, disa.mnemonic, disa.operands, symbol );
                else       con_print( NORM "%-3i: " GREEN "%-12s%-12s " "\n", 
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

void cmd_name(int argc, char argv[][CON_LINELEN])
{
    if(argc < 3)
    {
        con_print("syntax : name <addr> <symbol> OR name . <symbol> OR name * <symbol>\n");
        con_print("give name to function or memory variable (add symbol).\n");
        con_print("when using . (dot), symbol will be added by cursor address position.\n");
        con_print("when using * , symbol will be added by branch relocation.\n");
        con_print("examples of use : " GREEN "name 0x80003100 __start\n");
        con_print("                  " GREEN "name . main\n");
        con_print("                  " GREEN "name * OSCall\n");
        con_print("see also        : " GREEN "syms savemap loadmap addmap\n");
    }
    else
    {
        u32 address;
        if(argv[1][0] == '*')
        {
            u32 branchAddr = con.disa_cursor, op;
            u32 pa = MEMEffectiveToPhysical(branchAddr, 0);
            if(pa != -1)
            {
                PPCD_CB disa;
                MEMReadWord(branchAddr, &op);
                disa.pc = branchAddr;
                disa.instr = op;
                PPCDisasm (&disa);
                if(disa.iclass & PPC_DISA_BRANCH)
                    address = disa.target;
            }
            else address = 0;
        }
        else if(argv[1][0] == '.') address = con.disa_cursor;
        else address = strtoul(argv[1], NULL, 0);
        if(address != 0)
        {
            con_print(YEL "new symbol: %08X %s\n", address, argv[2]);
            SYMAddNew(address, argv[2]);
            con.update |= CON_UPDATE_ALL;
        }
        else con_print(BRED "wrong address!\n");
    }
}

// ---------------------------------------------------------------------------
// nop

static void add_nop(u32 ea, u32 oldVal)
{
    int n = con.nopNum ++;
    con.nopHist = (NOPHistory *)realloc(con.nopHist, sizeof(NOPHistory) * con.nopNum);
    ASSERT(con.nopHist == NULL, "NOP history is full");
    con.nopHist[n].ea = ea;
    con.nopHist[n].oldValue = oldVal;
}

static u32 get_nop(u32 ea)
{
    for(int i=0; i<con.nopNum; i++)
    {
        if(con.nopHist[i].ea == ea)
            return con.nopHist[i].oldValue;
    }
    return 0;   // not present
}

void cmd_nop()
{
    if(!emu.running) return;
    if(con.running) return;

    u32 ea = con.disa_cursor;
    u32 pa = MEMEffectiveToPhysical(ea, 0);
    if(pa == -1) return;
    
    u32 old = MEMSwap(*(u32 *)(&RAM[pa]));
    RAM[pa] = 0x60;
    RAM[pa+1] = RAM[pa+2] = RAM[pa+3] = 0;
    add_nop(ea, old);

    con.update |= (CON_UPDATE_DISA | CON_UPDATE_DATA);
}

void cmd_denop()
{
    if(!emu.running) return;
    if(con.running) return;

    u32 ea = con.disa_cursor;
    u32 pa = MEMEffectiveToPhysical(ea, 0);
    if(pa == -1) return;

    u32 old = get_nop(ea);
    if(old == 0) return;
    RAM[pa+0] = (u8)(old >> 24);
    RAM[pa+1] = (u8)(old >> 16);
    RAM[pa+2] = (u8)(old >>  8);
    RAM[pa+3] = (u8)(old >>  0);

    con.update |= (CON_UPDATE_DISA | CON_UPDATE_DATA);
}

// ---------------------------------------------------------------------------
// ostest

void cmd_ostest()
{
    OSCheckContextStruct();
}

// ---------------------------------------------------------------------------
// plist

void cmd_plist()
{
    if(ldat.patchNum == 0)
    {
        con_print(YEL "no patch data loaded.\n");
        return;
    }

    con_print("i----addr-----data-------------s-f-\n");
    for(u32 i=0; i<ldat.patchNum; i++)
    {
        Patch * p = &ldat.patches[i];
        u8 * data = (u8 *)&p->data;
        char * fmt = "%.3i: %08X %02X%02X%02X%02X%02X%02X%02X%02X %i %i\n";

        switch(p->dataSize)
        {
            case PATCH_SIZE_8:
                fmt = "%.3i: %08X " YEL "%02X" NORM "%02X%02X%02X%02X%02X%02X%02X %i %i\n";
                break;
            case PATCH_SIZE_16:
                fmt = "%.3i: %08X " YEL "%02X%02X" NORM "%02X%02X%02X%02X%02X%02X %i %i\n";
                break;
            case PATCH_SIZE_32:
                fmt = "%.3i: %08X " YEL "%02X%02X%02X%02X" NORM "%02X%02X%02X%02X %i %i\n";
                break;
            case PATCH_SIZE_64:
                fmt = "%.3i: %08X " YEL "%02X%02X%02X%02X%02X%02X%02X%02X" NORM " %i %i\n";
                break;
            default:
                fmt = "PATCH DAMAGED!";
        }

        con_print(
            fmt,
            i+1,
            MEMSwap(p->effectiveAddress),
            data[0], data[1], data[2], data[3],
            data[4], data[5], data[6], data[7],
            MEMSwapHalf(p->dataSize), MEMSwapHalf(p->freeze) & 1
        );
    }
    con_print("-----------------------------------\n");
}

// ---------------------------------------------------------------------------
// r

// -1, if unknown register
static int getreg(char *str)
{
    if(str[0] == 'r')
    {
        if(!isdigit(str[1])) return -1;
        return atoi(str + 1);
    }
    else if(!stricmp(str, "sp")) return 1;
    else if(!stricmp(str, "sd1")) return 13;
    else if(!stricmp(str, "sd2")) return 2;
    else return -1;
}

void cmd_r(int argc, char argv[][CON_LINELEN])
{
    if(argc < 2)
    {
        con_print("syntax : r <reg> OR r <reg> <val> OR r <reg> <reg>\n");
        con_print("sp, sd1, sd2 semantics are supported for reg name.\n");
        con_print("value can be decimal, or hex with \'0x\' prefix.\n");
        con_print("examples of use : " GREEN "r sp\n");
        con_print("                  " GREEN "r r3 12\n");
        con_print("                  " GREEN "r r7 0xffff\n");
        con_print("                  " GREEN "r r7 sd1\n");
    }
    else
    {
        int n = getreg(argv[1]);
        if(n == -1)
        {
            con_print("unknown register : %s\n", argv[1]);
            return;
        }

        // show register
        if(argc < 3)
        {
            con_print("r%i = %i (0x%X)\n", n, GPR[n], GPR[n]);
            return;
        }

        // set new value
        int m = getreg(argv[2]);
        if(m == -1)
        {
            u32 i = strtoul(argv[2], NULL, 0);
            con_print("r%i = %i (0x%X)\n", n, i, i);
            GPR[n] = i;
            return;
        }
        else
        {
            con_print("r%i = r%i\n", n, m);
            GPR[n] = GPR[m];
        }
    }
}

// ---------------------------------------------------------------------------
// sop

void cmd_sop(int argc, char argv[][CON_LINELEN])
{
    u32 saddr;
    if(argc < 2)
    {
        con_print("syntax : sop <opcode>\n");
        con_print("search range is not greater 16384 bytes.\n");
        con_print("examples of use : " GREEN "sop mtlr\n");
        con_print("                  " GREEN "sop psq_l\n");
    }
    else
    {
        // search opcode
        u32 eaddr = con.disa_cursor + 16384;
        for(saddr=con.disa_cursor+4; saddr<eaddr; saddr+=4)
        {
            PPCD_CB disa;
            u32 op = 0;
            u32 pa = MEMEffectiveToPhysical(saddr, 0);
            if(pa != -1) op = MEMFetch(pa);
            disa.instr = op;
            disa.pc = saddr;
            PPCDisasm (&disa);
            if(!stricmp(disa.mnemonic, argv[1])) break;
        }
        if(saddr == eaddr) con_print(GREEN "%s " NORM "not found. last address : %08X\n", argv[1], saddr);
        else
        {
            con_print(GREEN "%s " NORM "found at address : %08X\n", argv[1], saddr);
            con_set_disa_cur(saddr);
        }
    }
}

// ---------------------------------------------------------------------------
// stat

void cmd_stat(int argc, char argv[][CON_LINELEN])
{
    if(argc < 2)
    {
        con_print("syntax : stat <dev>\n");
        con_print(GREEN "dev" NORM " is specified device :\n");
        con_print("    cpu  : Gekko processor\n");
        con_print("    fifo : GX command processor\n");
        con_print("    vi   : video interface\n");
        con_print("    pi   : interrupts\n");
        con_print("    mi   : Flipper memory protection\n");
        con_print("    ax   : DSP, audio and streaming\n");
        con_print("    di   : DVD\n");
        con_print("    si   : serial interface (joypads)\n");
        con_print("    exi  : EXI devices\n");
        con_print("examples of use : " GREEN "stat cpu\n");
        con_print("                : " GREEN "stat vi\n");
    }
    else
    {
        #define IFDEV(n) if(!strncmp(argv[1], n, strlen(n)))
        IFDEV("vi")
        {
            VIStats();
            return;
        }
        else con_print("unknown device!\n");
        #undef IFDEV
    }
}

// ---------------------------------------------------------------------------
// syms

void cmd_syms(int argc, char argv[][CON_LINELEN])
{
    if(argc < 2)
    {
        con_print("syntax : syms <string> OR syms *\n");
        con_print("<string> is the first occurance of symbol to find.\n");
        con_print("* - list all symbols (possible overflow of message buffer).\n");
        con_print("examples of use : " GREEN "syms ma\n");
        con_print("                  " GREEN "syms __z\n");
        con_print("                  " GREEN "syms *\n");
        con_print("see also        : " GREEN "name savemap loadmap addmap\n");
    }
    else
    {
        SYMList(argv[1]);
    }
}

// ---------------------------------------------------------------------------
// savemap

void cmd_savemap(int argc, char argv[][CON_LINELEN])
{
    if(argc < 2)
    {
        con_print("syntax : savemap <file> OR savemap .\n");
        con_print(". is used to update current loaded map.\n");
        con_print("path can be relative\n");
        con_print("examples of use : " GREEN "savemap .\n");
        con_print("                  " GREEN "savemap data\\my.map\n");
        con_print("see also        : " GREEN "name loadmap addmap\n");
    }
    else
    {
        if(!strcmp(argv[1], ".")) SaveMAP();
        else SaveMAP(argv[1]);
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

void cmd_script(char *file)
{
    int i;
    con_print(YEL "loading script: %s\n", file);

    // following code is copied from MAPLoad :)

    s32 size = FileSize(file);
    FILE *f = fopen(file, "rt");
    if(!f)
    {
        con_print(RED "cannot open script file!\n");
        return ;
    }

    // allocate memory
    char *sbuf = (char *)malloc(size + 1);
    if(sbuf == NULL)
    {
        fclose(f);
        con_print(
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

    con_print(YEL "executing script...\n");

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
        con_print("%i: %s", cnt++, line);
        con_tokenizing(line);
        line[0] = 0;
        con_update(CON_UPDATE_EDIT | CON_UPDATE_MSGS);
        con_command(roll.tokencount, roll.tokens);
    }
    free(sbuf);

    con_print(YEL "\ndone execute script.\n");
    con.update |= CON_UPDATE_ALL;
}

// ---------------------------------------------------------------------------
// sd1, sd2

void cmd_sd(int sd, int argc, char argv[][CON_LINELEN])
{
    if(argc < 2)
    {
        con_print("syntax : sd%i <ofs>\n", sd);
        con_print("offset is a 16-bit signed dec/hex number.\n");
        con_print("examples of use : " GREEN "sd%i 0x8250\n", sd);
        con_print("see also        : " GREEN "d *\n");
    }
    else
    {
        u32 sda;
        if(sd == 1) sda = SDA1;
        else sda = SDA2;

        u32 ofs = strtoul(argv[1], NULL, 0);
        ofs &= 0xffff;
        if(ofs & 0x8000) ofs |= 0xffff0000;
        con.data = sda + (s32)ofs;
    }
}

// ---------------------------------------------------------------------------
// top10

void cmd_top10()
{
    HLEGetTop10(hle.top10);
    con_print("HLE Greatest Hits!!\n");
    for(int i=0; i<10; i++)
    {
        con_print(
            "%-2i: " GREEN "%s" NORM " (%i calls)\n",
            i+1, HLEGetHitNameByIndex(hle.top10[i]), hle.hitrate[hle.top10[i]]
        );
    }
    HLEResetHitrate();
}

// ---------------------------------------------------------------------------
// u

void cmd_u(int argc, char argv[][CON_LINELEN])
{
    if(argc < 2)
    {
        con_print("syntax : u <addr> OR u <symbol> OR u lr OR u ctr\n");
        con_print("numbers can be decimal, or hex with \'0x\' prefix.\n");
        con_print("examples of use : " GREEN "u 0x8023bc00\n");
        con_print("                  " GREEN "u main\n");
        con_print("                  " GREEN "u lr\n");
        con_print("see also        : " GREEN ". ENTER-key ESC-key\n");
    }
    else
    {
        u32 addr = 0;

        // first check for link/counter registers
        if(!stricmp(argv[1], "lr"))
        {
            con_set_disa_cur(LR);
            return;
        }
        if(!stricmp(argv[1], "ctr"))
        {
            con_set_disa_cur(CTR);
            return;
        }

        // now check for symbol
        addr = SYMAddress(argv[1]);
        if(addr)
        {
            con_set_disa_cur(addr);
            return;
        }

        // simply address
        con_set_disa_cur(strtoul(argv[1], NULL, 0));
    }
}

// ---------------------------------------------------------------------------
// help

void cmd_help()
{
    con_print(
        WHITE APPNAME " - " APPDESC "\n"
        WHITE "Build ver. " APPVER ", " __DATE__ ", " __TIME__ 
#ifdef  __MSVC__
        ", MSVC"
#endif
#ifdef  __VCNET__
        ", VCNET"
#endif
#ifdef  __MWERKS__
        ", CW"
#endif        
        "\n"
        WHITE "Copyright 2002-2004, " APPNAME " Team\n\n"
    );

    con_print(CYAN  "--- cpu debug commands --------------------------------------------------------");
    con_print(WHITE "    .                    " NORM "- view code at pc");
    con_print(WHITE "    *                    " NORM "- view data, pointed by load/store opcode");
    con_print(WHITE "    u                    " NORM "- view code at specified address");
    con_print(WHITE "    d                    " NORM "- view data at specified address");
    con_print(WHITE "    sd1                  " NORM "- quick form of d-command, using SDA1");
    con_print(WHITE "    sd2                  " NORM "- quick form of d-command, using SDA2");
    con_print(WHITE "    r                    " NORM "- show / change CPU register");
    con_print(WHITE "    ps                   " NORM "- show / change paired-single register");
    con_print(WHITE "    pc                   " NORM "- set program counter");

    con_print(WHITE "    nop                  " NORM "- insert NOP opcode at cursor");
    con_print(WHITE "    denop                " NORM "- restore old NOP'ed value");
    con_print(WHITE "    blr [value]          " NORM "- insert BLR opcode at cursor (with value)");
//  con_print(WHITE "    b <addr>             " NORM "- toggle code breakpoint at <addr> (max=%i)", MAX_BPNUM);
//  con_print(WHITE "    bm [8|16|32] <addr>  " NORM "- toggle data breakpoint at <addr> (max=%i)", MAX_BPNUM);
    con_print(WHITE "    bc                   " NORM "- clear all code breakpoints");
//  con_print(WHITE "    bmc                  " NORM "- clear all data breakpoints");
    con_print(WHITE "    run                  " NORM "- execute until next breakpoint");
    con_print(WHITE "    stop                 " NORM "- stop debugging");
    con_print(WHITE "    reset                " NORM "- reset emulator");
    con_print("\n");
    
    con_print(CYAN  "--- high-level commands -------------------------------------------------------");
    con_print(WHITE "    stat                 " NORM "- show hardware state/statistics");
    con_print(WHITE "    syms                 " NORM "- list symbolic information");
    con_print(WHITE "    name                 " NORM "- name function (add symbol)");
    con_print(WHITE "    dvdopen              " NORM "- get file position (use DVD plugin)");
    con_print(WHITE "    ostest               " NORM "- test OS internals");
    con_print(WHITE "    top10                " NORM "- show HLE calls toplist");
//  con_print(WHITE "    alarm                " NORM "- generate decrementer exception");
//  con_print(WHITE "    bcb                  " NORM "- list all registered branch callbacks");    
//  con_print(WHITE "    hlestats             " NORM "- show HLE-subsystem state");
    con_print(WHITE "    savemap              " NORM "- save symbolic map into file");
    con_print("\n");

    con_print(CYAN  "--- patch controls ------------------------------------------------------------");
    con_print(WHITE "    dop                  " NORM "- apply patches immediately (only with freeze=1)");
    con_print(WHITE "    plist                " NORM "- list all patch data");
    con_print(WHITE "    pload                " NORM "- load patch file (unload previous)");
    con_print(WHITE "    padd                 " NORM "- add patch file (do not unload previous)");
    con_print(WHITE "    patch                " NORM "- insert memory patch");
    con_print("\n");

    con_print(CYAN  "--- misc commands -------------------------------------------------------------");
    con_print(WHITE "    boot                 " NORM "- boot DVD/executable (from file or list)");
    con_print(WHITE "    reboot               " NORM "- reload last file");
    con_print(WHITE "    unload               " NORM "- unload current file");
    con_print(WHITE "    sop                  " NORM "- search opcode (forward) from cursor address");
    con_print(WHITE "    lr                   " NORM "- show LR back chain (\"branch history\"");
    con_print(WHITE "    script               " NORM "- execute batch script");
//  con_print(WHITE "    mapmem               " NORM "- add memory mapper");
    con_print(WHITE "    log                  " NORM "- enable/disable log output)");
    con_print(WHITE "    logfile              " NORM "- choose HTML log-file for ouput");
    con_print(WHITE "    full                 " NORM "- set full screen console mode");
    con_print(WHITE "    shot                 " NORM "- take screenshot in ????.tga");
    con_print(WHITE "    memst                " NORM "- memory allocation stats");
    con_print(WHITE "    memtst               " NORM "- memory test (" YEL "WARNING" NORM ")");
    con_print(WHITE "    cls                  " NORM "- clear message buffer");
    con_print(WHITE "    colors               " NORM "- colored output test");
    con_print(WHITE "    [q]uit, e[x]it       " NORM "- exit to OS");
    con_print("\n");

    con_print(CYAN  "--- functional keys -----------------------------------------------------------");
    con_print(WHITE "    F1                   " NORM "- update registers");
    con_print(WHITE "    F2                   " NORM "- memory view");
    con_print(WHITE "    F3                   " NORM "- disassembly");
    con_print(WHITE "    F4                   " NORM "- command string");
    con_print(WHITE "    F5                   " NORM "- run, stop");
    con_print(WHITE "    F6, ^F6              " NORM "- switch registers");
    con_print(WHITE "    F7                   " NORM "- step in");
    con_print(WHITE "    F9                   " NORM "- toggle autokill breakpoint");
    con_print(WHITE "    ^F9                  " NORM "- toggle breakpoint");
    con_print("\n");

    con_print(CYAN  "--- misc keys -----------------------------------------------------------------");
    con_print(WHITE "    PGUP, PGDN           " NORM "- scroll windows");
    con_print(WHITE "    ENTER, ESC           " NORM "- follow/return branch (in disasm window)");
    con_print(WHITE "    ENTER                " NORM "- memory edit (in memview window)");
    con_print(WHITE "    ^ENTER               " NORM "- show X86 code, compile if need (disasm window)");
    con_print("\n");
}
