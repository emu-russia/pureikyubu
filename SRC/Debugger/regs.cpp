// registers view
#include "dolphin.h"

// register memory
static uint32_t gpr_old[32];
static FPREG ps0_old[32], ps1_old[32];

static char *gprnames[] = {
 "r0" , "sp" , "sd2", "r3" , "r4" , "r5" , "r6" , "r7" , 
 "r8" , "r9" , "r10", "r11", "r12", "sd1", "r14", "r15", 
 "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23", 
 "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31"
};

// ---------------------------------------------------------------------------

void con_memorize_cpu_regs()
{
    for(int i=0; i<32; i++)
    {
        gpr_old[i] = GPR[i];
        ps0_old[i].uval = cpu.fpr[i].uval;
        ps1_old[i].uval = cpu.ps1[i].uval;
    }
}

static void con_print_other_regs()
{
    con_printf_at(28, 1, CYAN "cr  " NORM "%08X", PPC_CR);
    con_printf_at(28, 2, CYAN "xer " NORM "%08X", XER);
    con_printf_at(28, 4, CYAN "ctr " NORM "%08X", CTR);
    con_printf_at(28, 5, CYAN "dec " NORM "%08X", PPC_DEC);
    con_printf_at(28, 8, CYAN "pc  " NORM "%08X", PC);
    con_printf_at(28, 9, CYAN "lr  " NORM "%08X", PPC_LR);
    con_printf_at(28,14, CYAN "tbr " NORM "%08X:%08X", cpu.tb.Part.u, cpu.tb.Part.l);

    con_printf_at(42, 1, CYAN "msr   " NORM "%08X", MSR);
    con_printf_at(42, 2, CYAN "fpscr " NORM "%08X", FPSCR);
    con_printf_at(42, 4, CYAN "hid0  " NORM "%08X", HID0);
    con_printf_at(42, 5, CYAN "hid1  " NORM "%08X", HID1);
    con_printf_at(42, 6, CYAN "hid2  " NORM "%08X", HID2);
    con_printf_at(42, 8, CYAN "wpar  " NORM "%08X", WPAR);
    con_printf_at(42, 9, CYAN "dmau  " NORM "%08X", DMAU);
    con_printf_at(42,10, CYAN "dmal  " NORM "%08X", DMAL);

    con_printf_at(58, 1, CYAN "dsisr " NORM "%08X", DSISR);
    con_printf_at(58, 2, CYAN "dar   " NORM "%08X", PPC_DAR);
    con_printf_at(58, 4, CYAN "srr0  " NORM "%08X", SRR0);
    con_printf_at(58, 5, CYAN "srr1  " NORM "%08X", SRR1);
    con_printf_at(58, 8, CYAN "sprg0 " NORM "%08X", SPRG0);
    con_printf_at(58, 9, CYAN "sprg1 " NORM "%08X", SPRG1);
    con_printf_at(58,10, CYAN "sprg2 " NORM "%08X", SPRG2);
    con_printf_at(58,11, CYAN "sprg3 " NORM "%08X", SPRG3);
    con_printf_at(58,13, CYAN "ear   " NORM "%08X", EAR);
    con_printf_at(58,14, CYAN "pvr   " NORM "%08X", PVR);

    // Some cpu flags.
    con_printf_at(74, 1, CYAN "%s", (MSR & MSR_PR) ? "UISA" : "OEA");       // Supervisor?
    con_printf_at(74, 2, CYAN "%s", (MSR & MSR_EE) ? "EE" : "NE");          // Interrupts enabled?
    con_printf_at(74, 4, CYAN "PSE " NORM "%i", (HID2 & HID2_PSE) ? 1 : 0); // Paired Single mode?
    con_printf_at(74, 5, CYAN "LSQ " NORM "%i", (HID2 & HID2_LSQE)? 1 : 0); // Load/Store Quantization?
    con_printf_at(74, 6, CYAN "WPE " NORM "%i", (HID2 & HID2_WPE) ? 1 : 0); // Gather buffer?
    con_printf_at(74, 7, CYAN "LC  " NORM "%i", (HID2 & HID2_LCE) ? 1 : 0); // Cache locked?
}

static void con_print_gprreg(int x, int y, int num)
{
    if(GPR[num] != gpr_old[num])
    {
        con_printf_at(x, y, CYAN "%-3s " GREEN "%.8X", gprnames[num], GPR[num]);
        gpr_old[num] = GPR[num];
    }
    else
    {
        con_printf_at(x, y, CYAN "%-3s " NORM "%.8X", gprnames[num], GPR[num]);
    }
}

static void con_print_gprs()
{
    int y;

    for(y=1; y<=16; y++)
    {
        con_print_gprreg(0, y, y - 1);
        con_print_gprreg(14, y, y - 1 + 16);
    }
    
    con_print_other_regs();
}

static void con_print_fpreg(int x, int y, int num)
{
    ULARGE_INTEGER li;

    if(cpu.fpr[num].uval != ps0_old[num].uval)
    {
        if(FPRD(num) >= 0.0) con_printf_at(x, y, CYAN "f%-2i  " GREEN "%e", num, FPRD(num));
        else con_printf_at(x, y, CYAN "f%-2i " GREEN "%e", num, FPRD(num));
    
        li.QuadPart = FPRU(num);
        con_printf_at(x + 20, y, GREEN "%.8X %.8X", li.HighPart, li.LowPart);

        ps0_old[num].uval = cpu.fpr[num].uval;
    }
    else
    {
        if(FPRD(num) >= 0.0) con_printf_at(x, y, CYAN "f%-2i  " NORM "%e", num, FPRD(num));
        else con_printf_at(x, y, CYAN "f%-2i " NORM "%e", num, FPRD(num));
    
        li.QuadPart = FPRU(num);
        con_printf_at(x + 20, y, "%.8X %.8X", li.HighPart, li.LowPart);
    }
}

static void con_print_ps(int x, int y, int num)
{
    if(cpu.fpr[num].uval != ps0_old[num].uval)
    {
        if(PS0(num) >= 0.0f) con_printf_at(x, y, CYAN "ps%-2i  " GREEN "%.4e", num, PS0(num));
        else con_printf_at(x, y, CYAN "ps%-2i " GREEN "%.4e", num, PS0(num));
        
        ps0_old[num].uval = cpu.fpr[num].uval;
    }
    else
    {
        if(PS0(num) >= 0.0f) con_printf_at(x, y, CYAN "ps%-2i  " NORM "%.4e", num, PS0(num));
        else con_printf_at(x, y, CYAN "ps%-2i " NORM "%.4e", num, PS0(num));
    }

    if(cpu.ps1[num].uval != ps1_old[num].uval)
    {
        if(PS1(num) >= 0.0f) con_printf_at(x + 18, y, GREEN " %.4e", PS1(num));
        else con_printf_at(x + 18, y, GREEN "%.4e", PS1(num));        
        
        ps1_old[num].uval = cpu.ps1[num].uval;
    }
    else
    {
        if(PS1(num) >= 0.0f) con_printf_at(x + 18, y, " %.4e", PS1(num));
        else con_printf_at(x + 18, y, "%.4e", PS1(num));
    }
}

static void con_print_fprs()
{
    int y;

    for(y=1; y<=16; y++)
    {
        con_print_fpreg(0, y, y - 1);
        con_print_fpreg(39, y, y - 1 + 16);
    }
}

static void con_print_psrs()
{
    int y;

    for(y=1; y<=16; y++)
    {
        con_print_ps(0, y, y - 1);
        con_print_ps(32, y, y - 1 + 16);
    }

    for(y=1; y<=8; y++)
    {
        con_printf_at(64, y, CYAN "gqr%i " NORM "%08X", y - 1, GQR[y - 1]);
    }

    con_printf_at (64, 10, CYAN "PSE   " NORM "%i", (HID2 & HID2_PSE) ? 1 : 0); // Paired Single mode?
    con_printf_at (64, 11, CYAN "LSQ   " NORM "%i", (HID2 & HID2_LSQE)? 1 : 0); // Load/Store Quantization?
}

// -----------------------------------------------------------------------------------------
// MMU

static int cntlzw(uint32_t val)
{
    int i;
    for(i=0; i<32; i++)
    {
        if(val & (1 << (31-i))) break;
    }
    return ((i == 32) ? 31 : i);
}

static void describe_bat_reg (int x, int y, uint32_t up, uint32_t lo, int instr)
{
    // Use plain numbers, no definitions (for best compatibility).
    uint32_t bepi = (up >> 17) & 0x7fff;
    uint32_t bl = (up >> 2) & 0x7ff;
    uint32_t vs = (up >> 1) & 1;
    uint32_t vp = up & 1;
    uint32_t brpn = (lo >> 17) & 0x7fff;
    uint32_t w = (lo >> 5) & 1;
    uint32_t i = (lo >> 4) & 1;
    uint32_t m = (lo >> 3) & 1;
    uint32_t g = (lo >> 2) & 1;
    uint32_t pp = lo & 3;

    uint32_t EStart = bepi << 17, PStart = brpn << 17;
    uint32_t blkSize = 1 << (17 + 11 - cntlzw((bl << (32-11)) | 0x00100000));

    char *ppstr = BRED "NA";
    if(pp)
    {
        if(instr) { ppstr = ((pp & 1) ? (char *)(NORM "X") : (char *)(NORM "XW")); }
        else      { ppstr = ((pp & 1) ? (char *)(NORM "R") : (char *)(NORM "RW")); }
    }

    con_printf_at (x, y, NORM "%08X->%08X" " %-6s" " %c%c%c%c" " %s %s" " %s" ,
        EStart, PStart, FileSmartSize(blkSize), 
        w ? 'W' : '-',
        i ? 'I' : '-',
        m ? 'M' : '-',
        g ? 'G' : '-',
        vs ? NORM "Vs" : BRED "Ns",
        vp ? NORM "Vp" : BRED "Np",
        ppstr
    );
}

static void con_print_mmu()
{
    con_printf_at (0, 11,CYAN "sdr1  " NORM "%08X", SDR1);

    con_printf_at (0, 13,CYAN "IR    " NORM "%i", (MSR & MSR_IR) ? 1 : 0);
    con_printf_at (0, 14,CYAN "DR    " NORM "%i", (MSR & MSR_DR) ? 1 : 0);
    
    con_printf_at (0, 1, CYAN "dbat0 " NORM "%08X:%08X", DBAT0U, DBAT0L);
    con_printf_at (0, 2, CYAN "dbat1 " NORM "%08X:%08X", DBAT1U, DBAT1L);
    con_printf_at (0, 3, CYAN "dbat2 " NORM "%08X:%08X", DBAT2U, DBAT2L);
    con_printf_at (0, 4, CYAN "dbat3 " NORM "%08X:%08X", DBAT3U, DBAT3L);        

    con_printf_at (0, 6, CYAN "ibat0 " NORM "%08X:%08X", IBAT0U, IBAT0L);
    con_printf_at (0, 7, CYAN "ibat1 " NORM "%08X:%08X", IBAT1U, IBAT1L);
    con_printf_at (0, 8, CYAN "ibat2 " NORM "%08X:%08X", IBAT2U, IBAT2L);
    con_printf_at (0, 9, CYAN "ibat3 " NORM "%08X:%08X", IBAT3U, IBAT3L);

    describe_bat_reg(24, 1, DBAT0U, DBAT0L, 0);
    describe_bat_reg(24, 2, DBAT1U, DBAT1L, 0);
    describe_bat_reg(24, 3, DBAT2U, DBAT2L, 0);
    describe_bat_reg(24, 4, DBAT3U, DBAT3L, 0);

    describe_bat_reg(24, 6, IBAT0U, IBAT0L, 1);
    describe_bat_reg(24, 7, IBAT1U, IBAT1L, 1);
    describe_bat_reg(24, 8, IBAT2U, IBAT2L, 1);
    describe_bat_reg(24, 9, IBAT3U, IBAT3L, 1);

    for(int n=0, y=1; n<16; n++, y++)
    {
        char * prefix = PPC_SR[y-1] & 0x80000000 ? BRED : NORM;
        con_printf_at (64, y, CYAN "sr%-2i  " "%s" "%08X", y-1, prefix, PPC_SR[y-1]);
    }
}

void con_update_registers()
{
    con_attr(7, 0);
    con_fill_line(wind.regs_y);
    con_attr(0, 3);
    if(wind.focus == WREGS) con_print_at(0, wind.regs_y, WHITE "\x1f");
    con_attr(0, 3);
    con_print_at(2, wind.regs_y, "F1");

    switch(wind.regmode)
    {
        case REGMOD_GPR: con_printf_at(6, wind.regs_y, " GPR, SPR"); break;
        case REGMOD_FPR: con_printf_at(6, wind.regs_y, " FPR"); break;
        case REGMOD_PSR: con_printf_at(6, wind.regs_y, " PSR"); break;
        case REGMOD_MMU: con_printf_at(6, wind.regs_y, " MMU"); break;
    }

    con_attr(7, 0);

    switch(wind.regmode)
    {
        case REGMOD_GPR: con_print_gprs(); break;
        case REGMOD_FPR: con_print_fprs(); break;
        case REGMOD_PSR: con_print_psrs(); break;
        case REGMOD_MMU: con_print_mmu(); break;
    }
}
