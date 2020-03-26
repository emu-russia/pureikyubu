// registers view
#include "pch.h"

// register memory
static uint32_t gpr_old[32];
static FPREG ps0_old[32], ps1_old[32];

static const char *gprnames[] = {
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
    con_printf_at(28, 1, "\x1%ccr  \x1%c%08X", ConColor::CYAN, ConColor::NORM, PPC_CR);
    con_printf_at(28, 2, "\x1%cxer \x1%c%08X", ConColor::CYAN, ConColor::NORM, XER);
    con_printf_at(28, 4, "\x1%cctr \x1%c%08X", ConColor::CYAN, ConColor::NORM, CTR);
    con_printf_at(28, 5, "\x1%cdec \x1%c%08X", ConColor::CYAN, ConColor::NORM, PPC_DEC);
    con_printf_at(28, 8, "\x1%cpc  \x1%c%08X", ConColor::CYAN, ConColor::NORM, PC);
    con_printf_at(28, 9, "\x1%clr  \x1%c%08X", ConColor::CYAN, ConColor::NORM, PPC_LR);
    con_printf_at(28,14, "\x1%ctbr \x1%c%08X:%08X", ConColor::CYAN, ConColor::NORM, cpu.tb.Part.u, cpu.tb.Part.l);

    con_printf_at(42, 1, "\x1%cmsr   \x1%c%08X", ConColor::CYAN, ConColor::NORM, MSR);
    con_printf_at(42, 2, "\x1%cfpscr \x1%c%08X", ConColor::CYAN, ConColor::NORM, FPSCR);
    con_printf_at(42, 4, "\x1%chid0  \x1%c%08X", ConColor::CYAN, ConColor::NORM, HID0);
    con_printf_at(42, 5, "\x1%chid1  \x1%c%08X", ConColor::CYAN, ConColor::NORM, HID1);
    con_printf_at(42, 6, "\x1%chid2  \x1%c%08X", ConColor::CYAN, ConColor::NORM, HID2);
    con_printf_at(42, 8, "\x1%cwpar  \x1%c%08X", ConColor::CYAN, ConColor::NORM, WPAR);
    con_printf_at(42, 9, "\x1%cdmau  \x1%c%08X", ConColor::CYAN, ConColor::NORM, DMAU);
    con_printf_at(42,10, "\x1%cdmal  \x1%c%08X", ConColor::CYAN, ConColor::NORM, DMAL);

    con_printf_at(58, 1, "\x1%cdsisr \x1%c%08X", ConColor::CYAN, ConColor::NORM, DSISR);
    con_printf_at(58, 2, "\x1%cdar   \x1%c%08X", ConColor::CYAN, ConColor::NORM, PPC_DAR);
    con_printf_at(58, 4, "\x1%csrr0  \x1%c%08X", ConColor::CYAN, ConColor::NORM, SRR0);
    con_printf_at(58, 5, "\x1%csrr1  \x1%c%08X", ConColor::CYAN, ConColor::NORM, SRR1);
    con_printf_at(58, 8, "\x1%csprg0 \x1%c%08X", ConColor::CYAN, ConColor::NORM, SPRG0);
    con_printf_at(58, 9, "\x1%csprg1 \x1%c%08X", ConColor::CYAN, ConColor::NORM, SPRG1);
    con_printf_at(58,10, "\x1%csprg2 \x1%c%08X", ConColor::CYAN, ConColor::NORM, SPRG2);
    con_printf_at(58,11, "\x1%csprg3 \x1%c%08X", ConColor::CYAN, ConColor::NORM, SPRG3);
    con_printf_at(58,13, "\x1%cear   \x1%c%08X", ConColor::CYAN, ConColor::NORM, EAR);
    con_printf_at(58,14, "\x1%cpvr   \x1%c%08X", ConColor::CYAN, ConColor::NORM, PVR);

    // Some cpu flags.
    con_printf_at(74, 1, "\x1%c%s", ConColor::CYAN, (MSR & MSR_PR) ? "UISA" : "OEA");       // Supervisor?
    con_printf_at(74, 2, "\x1%c%s", ConColor::CYAN, (MSR & MSR_EE) ? "EE" : "NE");          // Interrupts enabled?
    con_printf_at(74, 4, "\x1%cPSE \x1%c%i", ConColor::CYAN, ConColor::NORM, (HID2 & HID2_PSE) ? 1 : 0); // Paired Single mode?
    con_printf_at(74, 5, "\x1%cLSQ \x1%c%i", ConColor::CYAN, ConColor::NORM, (HID2 & HID2_LSQE)? 1 : 0); // Load/Store Quantization?
    con_printf_at(74, 6, "\x1%cWPE \x1%c%i", ConColor::CYAN, ConColor::NORM, (HID2 & HID2_WPE) ? 1 : 0); // Gather buffer?
    con_printf_at(74, 7, "\x1%cLC  \x1%c%i", ConColor::CYAN, ConColor::NORM, (HID2 & HID2_LCE) ? 1 : 0); // Cache locked?
}

static void con_print_gprreg(int x, int y, int num)
{
    if(GPR[num] != gpr_old[num])
    {
        con_printf_at(x, y, "\x1%c%-3s \x1%c%.8X", ConColor::CYAN, gprnames[num], ConColor::GREEN, GPR[num]);
        gpr_old[num] = GPR[num];
    }
    else
    {
        con_printf_at(x, y, "\x1%c%-3s \x1%c%.8X", ConColor::CYAN, gprnames[num], ConColor::NORM, GPR[num]);
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
        if(FPRD(num) >= 0.0) con_printf_at(x, y, "\x1%cf%-2i  \x1%c%e", ConColor::CYAN, num, ConColor::GREEN, FPRD(num));
        else con_printf_at(x, y, "\x1%cf%-2i \x1%c%e", ConColor::CYAN, num, ConColor::GREEN, FPRD(num));
    
        li.QuadPart = FPRU(num);
        con_printf_at(x + 20, y, "\x1%c%.8X %.8X", ConColor::GREEN, li.HighPart, li.LowPart);

        ps0_old[num].uval = cpu.fpr[num].uval;
    }
    else
    {
        if(FPRD(num) >= 0.0) con_printf_at(x, y, "\x1%cf%-2i  \x1%c%e", ConColor::CYAN, num, ConColor::NORM, FPRD(num));
        else con_printf_at(x, y, "\x1%cf%-2i \x1%c%e", ConColor::CYAN, num, ConColor::NORM, FPRD(num));
    
        li.QuadPart = FPRU(num);
        con_printf_at(x + 20, y, "%.8X %.8X", li.HighPart, li.LowPart);
    }
}

static void con_print_ps(int x, int y, int num)
{
    if(cpu.fpr[num].uval != ps0_old[num].uval)
    {
        if(PS0(num) >= 0.0f) con_printf_at(x, y, "\x1%cps%-2i  \x1%c%.4e", ConColor::CYAN, num, ConColor::GREEN, PS0(num));
        else con_printf_at(x, y, "\x1%cps%-2i \x1%c%.4e", ConColor::CYAN, num, ConColor::GREEN, PS0(num));
        
        ps0_old[num].uval = cpu.fpr[num].uval;
    }
    else
    {
        if(PS0(num) >= 0.0f) con_printf_at(x, y, "\x1%cps%-2i  \x1%c%.4e", ConColor::CYAN, num, ConColor::NORM, PS0(num));
        else con_printf_at(x, y, "\x1%cps%-2i \x1%c%.4e", ConColor::CYAN, num, ConColor::NORM, PS0(num));
    }

    if(cpu.ps1[num].uval != ps1_old[num].uval)
    {
        if(PS1(num) >= 0.0f) con_printf_at(x + 18, y, "\x1%c %.4e", ConColor::GREEN, PS1(num));
        else con_printf_at(x + 18, y, "\x1%c%.4e", ConColor::GREEN, PS1(num));
        
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
        con_printf_at(64, y, "\x1%cgqr%i \x1%c%08X", ConColor::CYAN, y - 1, ConColor::NORM, GQR[y - 1]);
    }

    con_printf_at (64, 10, "\x1%cPSE   \x1%c%i", ConColor::CYAN, ConColor::NORM, (HID2 & HID2_PSE) ? 1 : 0); // Paired Single mode?
    con_printf_at (64, 11, "\x1%cLSQ   \x1%c%i", ConColor::CYAN, ConColor::NORM, (HID2 & HID2_LSQE)? 1 : 0); // Load/Store Quantization?
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

    const char *ppstr = "NA";
    if(pp)
    {
        if(instr) { ppstr = ((pp & 1) ? (char *)("X") : (char *)("XW")); }
        else      { ppstr = ((pp & 1) ? (char *)("R") : (char *)("RW")); }
    }

    con_printf_at (x, y, "\x1%c%08X->%08X" " %-6s" " %c%c%c%c" " %s %s" " \x1%c%s" ,
        ConColor::NORM,
        EStart, PStart, UI::FileSmartSize(blkSize), 
        w ? 'W' : '-',
        i ? 'I' : '-',
        m ? 'M' : '-',
        g ? 'G' : '-',
        vs ? "Vs" : "Ns",
        vp ? "Vp" : "Np",
        ConColor::NORM, ppstr
    );
}

static void con_print_mmu()
{
    con_printf_at (0, 11,"\x1%csdr1  \x1%c%08X", ConColor::CYAN, ConColor::NORM, SDR1);

    con_printf_at (0, 13,"\x1%cIR    \x1%c%i", ConColor::CYAN, ConColor::NORM, (MSR & MSR_IR) ? 1 : 0);
    con_printf_at (0, 14,"\x1%cDR    \x1%c%i", ConColor::CYAN, ConColor::NORM, (MSR & MSR_DR) ? 1 : 0);
    
    con_printf_at (0, 1, "\x1%cdbat0 \x1%c%08X:%08X", ConColor::CYAN, ConColor::NORM, DBAT0U, DBAT0L);
    con_printf_at (0, 2, "\x1%cdbat1 \x1%c%08X:%08X", ConColor::CYAN, ConColor::NORM, DBAT1U, DBAT1L);
    con_printf_at (0, 3, "\x1%cdbat2 \x1%c%08X:%08X", ConColor::CYAN, ConColor::NORM, DBAT2U, DBAT2L);
    con_printf_at (0, 4, "\x1%cdbat3 \x1%c%08X:%08X", ConColor::CYAN, ConColor::NORM, DBAT3U, DBAT3L);

    con_printf_at (0, 6, "\x1%cibat0 \x1%c%08X:%08X", ConColor::CYAN, ConColor::NORM, IBAT0U, IBAT0L);
    con_printf_at (0, 7, "\x1%cibat1 \x1%c%08X:%08X", ConColor::CYAN, ConColor::NORM, IBAT1U, IBAT1L);
    con_printf_at (0, 8, "\x1%cibat2 \x1%c%08X:%08X", ConColor::CYAN, ConColor::NORM, IBAT2U, IBAT2L);
    con_printf_at (0, 9, "\x1%cibat3 \x1%c%08X:%08X", ConColor::CYAN, ConColor::NORM, IBAT3U, IBAT3L);

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
        const ConColor prefix = PPC_SR[y-1] & 0x80000000 ? ConColor::BRED : ConColor::NORM;
        con_printf_at (64, y, "\x1%csr%-2i  " "\x1%c" "%08X", ConColor::CYAN, y-1, prefix, PPC_SR[y-1]);
    }
}

void con_update_registers()
{
    con_attr(7, 0);

    for (int i = 0; i < wind.regs_h; i++)
    {
        con_fill_line(wind.regs_y + i, ' ');
    }

    con_attr(0, 3);
    con_fill_line(wind.regs_y, 0xc4);
    con_attr(0, 3);
    if(wind.focus == WREGS) con_printf_at(0, wind.regs_y, "\x1%c\x1f", ConColor::WHITE);
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
