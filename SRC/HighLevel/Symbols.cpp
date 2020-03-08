// symbolic information API.
#include "dolphin.h"

// IMPORTANT : EXE loading base must be 0x00400000, for correct HLE.
// MSVC : Project/Settings/Link/Output/Base Address
// CW : Edit/** Win32 x86 Settings/Linker/x86 COFF/Base address

// all important variables are here
SYMControl sym;                 // default workspace
static SYMControl *work = &sym; // current workspace (in use)

// ---------------------------------------------------------------------------

// find first occurency of symbol in list
static SYM * symfind(char *symName)
{
    SYM *symbol = NULL;

    // walk all
    for(int tag=0; tag<61; tag++)
    for(int i=0; i<work->symcount[tag]; i++)
    {
        if(!strcmp(work->symhash[tag][i].savedName, symName))
        {
            symbol = &work->symhash[tag][i]; // found!
            return symbol;
        }
    }

    return NULL;
}

// calculate tag - sum of all ciphers in lower word of address
static int gettag(uint32_t addr)
{
    uint32_t lo = (uint16_t)addr;
    return ((lo >>  0) & 0xf) +         // value = 0...60
           ((lo >>  4) & 0xf) +
           ((lo >>  8) & 0xf) +
           ((lo >> 12) & 0xf);
}

void SYMSetWorkspace(SYMControl *useIt)
{
    work = useIt;
}

void SYMCompareWorkspaces (
    SYMControl      *source,
    SYMControl      *dest,
    void (*DiffCallback)(uint32_t ea, char * name)
)
{
    // walk all
    for(int tag=0; tag<61; tag++)
    for(int i=0; i<source->symcount[tag]; i++)
    {
        BOOL found = FALSE;
        SYM *symbol = &source->symhash[tag][i];
        if( !symbol->emuSymbol && symbol )
        {
            for(int tag2=0; tag2<61; tag2++)
            {
                for(int i2=0; i2<dest->symcount[tag2]; i2++)
                {
                    SYM *symbol2 = &dest->symhash[tag][i];
                    if(symbol2->emuSymbol || symbol2 == NULL) continue;
                    if(!strcmp(symbol->savedName, symbol2->savedName))
                    {
                        found = TRUE;
                        break;
                    }
                }
                if(found) break;
            }
            if(!found)
                DiffCallback(symbol->eaddr, symbol->savedName);
        }
    }
}

// get address of symbolic label
// if label is not specified, return 0
uint32_t SYMAddress(char *symName)
{
    // try to find specified symbol
    SYM *symbol = symfind(symName);

    if(symbol) return symbol->eaddr;
    else return 0;
}

// get symbolic label by given address
// if label is not specified, return NULL
char * SYMName(uint32_t symAddr)
{
    // calculate tag
    int tag = gettag(symAddr);

    // walk all
    for(int i=0; i<work->symcount[tag]; i++)
    {
        if(work->symhash[tag][i].eaddr == symAddr)
        {
            return work->symhash[tag][i].savedName;
        }
    }

    // symbol not found
    return NULL;
}

// associate high-level call with symbol
// (if CPU reaches label, it jumps to HLE call)
void SYMSetHighlevel(char *symName, void (*routine)())
{
    // try to find specified symbol
    SYM *symbol = symfind(symName);

    // check address
    VERIFY((uint32_t)routine & ~0x03ffffff, "High-level call is too high in memory.");

    // leave, if symbol is not found. add otherwise.
    if(symbol)
    {
        symbol->routine = routine;      // overwrite

        // if first opcode is 'BLR', then just leave it
        uint32_t op;
        MEMReadWord(symbol->eaddr, &op);
        if(op != 0x4e800020)
        {
            MEMWriteWord(
                symbol->eaddr,          // add patch
                (uint32_t)routine            // 000: high-level opcode
            );
            if(!stricmp(symName, "OSLoadContext"))
            {
                MEMWriteWord(
                    symbol->eaddr + 4,  // return to caller
                    0x4c000064          // rfi
                );
            }
            else
            {
                MEMWriteWord(
                    symbol->eaddr + 4,  // return to caller
                    0x4e800020          // blr
                );
            }
        }
        DBReport(YEL "patched API call: %08X %s\n", symbol->eaddr, symName);
    }
    else return;
}

// save string in memory
static char * strsave(const char *str)
{
    int len = strlen(str) + 1;
    char *saved = (char *)malloc(len);
    if(saved == NULL)
    {
        DolwinError( "strsave in HighLevel\\Symbols.cpp",
                     "Not enough memory for new string : %s\n\n", str );
        // halt !
    }
    strcpy(saved, str);
    return saved;
}

// add new symbol
void SYMAddNew(uint32_t addr, const char *name, bool emuSymbol /* false */)
{
    int i;
    // calculate tag
    int tag = gettag(addr);

    // ignore NULL address
    if(addr == 0) return;

    // check if already present
    for(i=0; i<work->symcount[tag]; i++)
    {
        // if yes, then replace
        if(work->symhash[tag][i].eaddr == addr)
        {
            if(work->symhash[tag][i].savedName)
            {
                free(work->symhash[tag][i].savedName);
                work->symhash[tag][i].savedName = NULL;
            }
            work->symhash[tag][i].savedName = strsave(name);
            return;
        }
    }

    // if not, then add new
    i = work->symcount[tag];
    work->symhash[tag] = (work->symhash[tag] != NULL) ?
        ( (SYM *)realloc(work->symhash[tag], sizeof(SYM) * (i + 1)) ) :
        ( (SYM *)malloc(sizeof(SYM)) );
    if(work->symhash[tag] == NULL)
    {
        DolwinError( "SYMAddNew in HighLevel\\Symbols.cpp",
                     "Not enough memory for new symbol (n=%i).", i );
    }
    work->symhash[tag][i].eaddr = addr;
    work->symhash[tag][i].savedName = strsave(name);
    work->symhash[tag][i].routine = NULL;
    work->symhash[tag][i].emuSymbol = emuSymbol;
    work->symcount[tag]++;
}

// remove all symbols (delete list)
void SYMKill()
{
    // kill 'em all
    for(int tag=0; tag<61; tag++)
    {
        for(int i=0; i<work->symcount[tag]; i++)
        {
            if(work->symhash[tag][i].savedName)
            {
                free(work->symhash[tag][i].savedName);
                work->symhash[tag][i].savedName = NULL;
            }
        }
        if(work->symhash[tag])
        {
            free(work->symhash[tag]);
            work->symhash[tag] = NULL;
        }
        work->symcount[tag] = 0;
    }
}

// list symbols, matching first occurence of "str".
// * - all symbols (warning! Zelda has about 20000 symbols).
void SYMList(const char *str)
{
    if(!emu.doldebug) return;
    int len = strlen(str), cnt = 0;
    DBReport("tag:id <address> symbol\n\n");

    // walk all
    for(int tag=0; tag<61; tag++)
    for(int i=0; i<work->symcount[tag]; i++)
    {
        SYM *symbol = &work->symhash[tag][i];
        if( ((*str == '*') || !strnicmp(str, symbol->savedName, len)) && !symbol->emuSymbol )
        {
            DBReport(
                "%02i:%03i " CYAN "<%08X> " GREEN "%s\n", 
                tag, i, symbol->eaddr, symbol->savedName
            );
            cnt++;
        }
    }

    DBReport("%i match\n\n", cnt);
}

// add Dolwin symbolic information (for x86 disassembly)
void SYMAddEmulatorSymbols()
{
    // hardware registers (Dolphin OS mapping!)
    // ---------------------------------------------------------------------------    

    #define PREFIX  0xc0000000

    // 0C000000        Command Processor (CP)
    SYMAddNew(CP_SR | PREFIX, "HW_BASE (CP_SR)", true);
    SYMAddNew(CP_CR | PREFIX, "CP_CR", true);
    SYMAddNew(CP_CLR | PREFIX, "CP_CLR", true);
    SYMAddNew(CP_BASE | PREFIX, "CP_BASE", true);
    SYMAddNew(CP_TOP | PREFIX, "CP_TOP", true);
    SYMAddNew(CP_HIWMARK | PREFIX, "CP_HIWMARK", true);
    SYMAddNew(CP_LOWMARK | PREFIX, "CP_LOWMARK", true);
    SYMAddNew(CP_CNT | PREFIX, "CP_CNT", true);
    SYMAddNew(CP_WRPTR | PREFIX, "CP_WRPTR", true);
    SYMAddNew(CP_RDPTR | PREFIX, "CP_RDPTR", true);
    SYMAddNew(CP_BPPTR | PREFIX, "CP_BPPTR", true);

    // 0C001000        Pixel Engine (PE)
    SYMAddNew(PE_SR | PREFIX, "PE_SR", true);
    SYMAddNew(PE_TOKEN | PREFIX, "PE_TOKEN", true);

    // 0C002000        Video Interface (VI)
    SYMAddNew(VI_VERT_TIMING | PREFIX, "VI_VERT_TIMING", true);
    SYMAddNew(VI_DISP_CR | PREFIX, "VI_DISP_CR", true);
    SYMAddNew(VI_HORZ_TIMING0 | PREFIX, "VI_HORZ_TIMING0", true);
    SYMAddNew(VI_HORZ_TIMING1 | PREFIX, "VI_HORZ_TIMING1", true);
    SYMAddNew(VI_VERT_TIMING_ODD | PREFIX, "VI_VERT_TIMING_ODD", true);
    SYMAddNew(VI_VERT_TIMING_EVEN | PREFIX, "VI_VERT_TIMING_EVEN", true);
    SYMAddNew(VI_BBINT_ODD | PREFIX, "VI_BBINT_ODD", true);
    SYMAddNew(VI_BBINT_EVEN | PREFIX, "VI_BBINT_EVEN", true);
    SYMAddNew(VI_TFBL | PREFIX, "VI_TFBL", true);
    SYMAddNew(VI_TFBR | PREFIX, "VI_TFBR", true);
    SYMAddNew(VI_BFBL | PREFIX, "VI_BFBL", true);
    SYMAddNew(VI_BFBR | PREFIX, "VI_BFBR", true);
    SYMAddNew(VI_DISP_POS | PREFIX, "VI_DISP_POS", true);
    SYMAddNew(VI_INT0 | PREFIX, "VI_INT0", true);
    SYMAddNew(VI_INT1 | PREFIX, "VI_INT1", true);
    SYMAddNew(VI_INT2 | PREFIX, "VI_INT2", true);
    SYMAddNew(VI_INT3 | PREFIX, "VI_INT3", true);
    SYMAddNew(VI_TAP0 | PREFIX, "VI_TAP0", true);
    SYMAddNew(VI_TAP1 | PREFIX, "VI_TAP1", true);
    SYMAddNew(VI_TAP2 | PREFIX, "VI_TAP2", true);
    SYMAddNew(VI_TAP3 | PREFIX, "VI_TAP3", true);
    SYMAddNew(VI_TAP4 | PREFIX, "VI_TAP4", true);
    SYMAddNew(VI_TAP5 | PREFIX, "VI_TAP5", true);
    SYMAddNew(VI_TAP6 | PREFIX, "VI_TAP6", true);
    SYMAddNew(VI_CLK_SEL | PREFIX, "VI_CLK_SEL", true);
    SYMAddNew(VI_DTV | PREFIX, "VI_DTV", true);
    SYMAddNew(VI_BRDR_HBE | PREFIX, "VI_BRDR_HBE", true);
    SYMAddNew(VI_BRDR_HBS | PREFIX, "VI_BRDR_HBS", true);

    // 0C003000        Processor Interface (PI)
    SYMAddNew(PI_INTSR | PREFIX, "PI_INTSR", true);
    SYMAddNew(PI_INTMR | PREFIX, "PI_INTMR", true);
    SYMAddNew(PI_BASE | PREFIX, "PI_BASE", true);
    SYMAddNew(PI_TOP | PREFIX, "PI_TOP", true);
    SYMAddNew(PI_WRPTR | PREFIX, "PI_WRPTR", true);
    SYMAddNew(PI_MB_REV | PREFIX, "PI_MB_REV", true);
    SYMAddNew(PI_RST_CODE | PREFIX, "PI_RST_CODE", true);

    // 0C004000        Memory Interface (MI)

    // 0C005000        DSP and DMA Audio Interface (AID), ARAM
    SYMAddNew(DSP_OUTMBOXH | PREFIX, "DSP_OUTMBOXH", true);
    SYMAddNew(DSP_OUTMBOXL | PREFIX, "DSP_OUTMBOXL", true);
    SYMAddNew(DSP_INMBOXH | PREFIX, "DSP_INMBOXH", true);
    SYMAddNew(DSP_INMBOXL | PREFIX, "DSP_INMBOXL", true);
    SYMAddNew(AI_DCR | PREFIX, "AI_DCR", true);
    SYMAddNew(AID_MADRH | PREFIX, "AID_MADRH", true);
    SYMAddNew(AID_MADRL | PREFIX, "AID_MADRL", true);
    SYMAddNew(AID_LEN | PREFIX, "AID_LEN", true);
    SYMAddNew(AID_CNT | PREFIX, "AID_CNT", true);
    SYMAddNew(AR_SIZE | PREFIX, "AR_SIZE", true);
    SYMAddNew(AR_MODE | PREFIX, "AR_MODE", true);
    SYMAddNew(AR_REFRESH | PREFIX, "AR_REFRESH", true);
    SYMAddNew(AR_DMA_MMADDR_H | PREFIX, "AR_DMA_MMADDR_H", true);
    SYMAddNew(AR_DMA_MMADDR_L | PREFIX, "AR_DMA_MMADDR_L", true);
    SYMAddNew(AR_DMA_ARADDR_H | PREFIX, "AR_DMA_ARADDR_H", true);
    SYMAddNew(AR_DMA_ARADDR_L | PREFIX, "AR_DMA_ARADDR_L", true);
    SYMAddNew(AR_DMA_CNT_H | PREFIX, "AR_DMA_CNT_H", true);
    SYMAddNew(AR_DMA_CNT_L | PREFIX, "AR_DMA_CNT_L", true);

    // 0C006000        DVD Interface (DI) (no 16-bit regs)
    SYMAddNew(DI_SR | PREFIX, "DI_SR", true);
    SYMAddNew(DI_CVR | PREFIX, "DI_CVR", true);
    SYMAddNew(DI_CMDBUF0 | PREFIX, "DI_CMDBUF0", true);
    SYMAddNew(DI_CMDBUF1 | PREFIX, "DI_CMDBUF1", true);
    SYMAddNew(DI_CMDBUF2 | PREFIX, "DI_CMDBUF2", true);
    SYMAddNew(DI_MAR | PREFIX, "DI_MAR", true);
    SYMAddNew(DI_LEN | PREFIX, "DI_LEN", true);
    SYMAddNew(DI_CR | PREFIX, "DI_CR", true);
    SYMAddNew(DI_IMMBUF | PREFIX, "DI_IMMBUF", true);
    SYMAddNew(DI_CFG | PREFIX, "DI_CFG", true);

    // 0C006400        Serial Interface (SI)
    SYMAddNew(SI_CHAN0_OUTBUF | PREFIX, "SI_CHAN0_OUTBUF", true);
    SYMAddNew(SI_CHAN0_INBUFH | PREFIX, "SI_CHAN0_INBUFH", true);
    SYMAddNew(SI_CHAN0_INBUFL | PREFIX, "SI_CHAN0_INBUFL", true);
    SYMAddNew(SI_CHAN1_OUTBUF | PREFIX, "SI_CHAN1_OUTBUF", true);
    SYMAddNew(SI_CHAN1_INBUFH | PREFIX, "SI_CHAN1_INBUFH", true);
    SYMAddNew(SI_CHAN1_INBUFL | PREFIX, "SI_CHAN1_INBUFL", true);
    SYMAddNew(SI_CHAN2_OUTBUF | PREFIX, "SI_CHAN2_OUTBUF", true);
    SYMAddNew(SI_CHAN2_INBUFH | PREFIX, "SI_CHAN2_INBUFH", true);
    SYMAddNew(SI_CHAN2_INBUFL | PREFIX, "SI_CHAN2_INBUFL", true);
    SYMAddNew(SI_CHAN3_OUTBUF | PREFIX, "SI_CHAN3_OUTBUF", true);
    SYMAddNew(SI_CHAN3_INBUFH | PREFIX, "SI_CHAN3_INBUFH", true);
    SYMAddNew(SI_CHAN3_INBUFL | PREFIX, "SI_CHAN3_INBUFL", true);
    SYMAddNew(SI_POLL | PREFIX, "SI_POLL", true);
    SYMAddNew(SI_COMCSR | PREFIX, "SI_COMCSR", true);
    SYMAddNew(SI_SR | PREFIX, "SI_SR", true);
    SYMAddNew(SI_EXILK | PREFIX, "SI_EXILK", true);
    SYMAddNew(SI_COMBUF | PREFIX, "SI_COMBUF", true);

    // 0C006800        External Interface (EXI)
    SYMAddNew(EXI0_CSR | PREFIX, "EXI0_CSR", true);
    SYMAddNew(EXI0_MADR | PREFIX, "EXI0_MADR", true);
    SYMAddNew(EXI0_LEN | PREFIX, "EXI0_LEN", true);
    SYMAddNew(EXI0_CR | PREFIX, "EXI0_CR", true);
    SYMAddNew(EXI0_DATA | PREFIX, "EXI0_DATA", true);
    SYMAddNew(EXI1_CSR | PREFIX, "EXI1_CSR", true);
    SYMAddNew(EXI1_MADR | PREFIX, "EXI1_MADR", true);
    SYMAddNew(EXI1_LEN | PREFIX, "EXI1_LEN", true);
    SYMAddNew(EXI1_CR | PREFIX, "EXI1_CR", true);
    SYMAddNew(EXI1_DATA | PREFIX, "EXI1_DATA", true);
    SYMAddNew(EXI2_CSR | PREFIX, "EXI2_CSR", true);
    SYMAddNew(EXI2_MADR | PREFIX, "EXI2_MADR", true);
    SYMAddNew(EXI2_LEN | PREFIX, "EXI2_LEN", true);
    SYMAddNew(EXI2_CR | PREFIX, "EXI2_CR", true);
    SYMAddNew(EXI2_DATA | PREFIX, "EXI2_DATA", true);

    // 0C006C00        Audio Streaming Interface (AIS)
    SYMAddNew(AIS_CR | PREFIX, "AIS_CR", true);
    SYMAddNew(AIS_VR | PREFIX, "AIS_VR", true);
    SYMAddNew(AIS_SCNT | PREFIX, "AIS_SCNT", true);
    SYMAddNew(AIS_IT | PREFIX, "AIS_IT", true);

    // 0C008000        PI FIFO (GX)
    SYMAddNew(GX_FIFO | PREFIX, "GX_FIFO", true);
    SYMAddNew((GX_FIFO + 4) | PREFIX, "GX_FIFO", true);
}
