// BS and BS2 (IPL) simulation.
#include "dolphin.h"

static uint32_t default_syscall[] = {    // default exception handler
    0x2c01004c,     // isync
    0xac04007c,     // sync
    0x6400004c,     // rfi
};

// load FST
static void ReadFST()
{
    #define DOL_LIMIT   (4*1024*1024)
    #define ROUND32(x)  (((uint32_t)(x)+32-1)&~(32-1))

    uint32_t     bb2[8];         // space for BB2
    uint32_t     fstAddr, fstOffs, fstSize, fstMaxSize;

    // plugin safe test (to run something without plugins at all)
    if(DVDSetCurrent == NULL) return;

    // read BB2
    DVDSeek(0x420);
    DVDRead((uint8_t *)bb2, 32);

    // rounding is not important, but present in new apploaders.
    // FST memory address is calculated, by adjusting bb[4] with "DOL LIMIT";
    // DOL limit is fixed to 4 mb, for most apploaders (in release date range
    // from AnimalCrossing to Zelda: Wind Waker).
    fstOffs = MEMSwap(bb2[1]);
    fstSize = ROUND32(MEMSwap(bb2[2]));
    fstMaxSize = ROUND32(MEMSwap(bb2[3]));
    fstAddr = MEMSwap(bb2[4]) + RAMSIZE - DOL_LIMIT;

    // load FST into memory
    DVDSeek(fstOffs);
    DVDRead(&RAM[fstAddr & RAMMASK], fstSize);

    // save fst configuration in lomem
    MEMWriteWord(0x80000038, fstAddr);
    MEMWriteWord(0x8000003c, fstMaxSize);

    // adjust arenaHi (OSInit will override it anyway, but not home demos)
    // arenaLo set to 0
    //MEMWriteWord(0x80000030, 0);
    //MEMWriteWord(0x80000034, fstAddr);
}

// execute apploader (apploader base is 0x81200000)
// this is exact apploader emulation. it is safe and checked.
static void BootApploader()
{
    uint32_t     appHeader[8];           // apploader header information
    uint32_t     appSize;                // size of apploader image
    uint32_t     appEntryPoint;
    uint32_t     _prolog, _main, _epilog;
    uint32_t     offs, size, addr;       // return of apploader main

    // disable hardware update
    HWEnableUpdate(0);

    // I use prolog/epilog terms here, but Nintendo is using 
    // something weird, like : appLoaderFunc1 (see Zelda dump - it 
    // has some compilation garbage parts from bootrom, hehe).

    DBReport( YEL "booting apploader..\n");

    // set OSReport dummy
    MEMWriteWord(0x81300000, 0x4e800020 /* blr opcode */);

    DVDSeek(0x2440);                // apploader offset
    DVDRead((uint8_t *)appHeader, 32);   // read apploader header
    MEMSwapArea(appHeader, 32);     // and swap it

    // save apploader info
    appEntryPoint = appHeader[4];
    appSize = appHeader[5];

    // load apploader image
    DVDSeek(0x2460);
    DVDRead(&RAM[0x81200000 & RAMMASK], appSize);

    // set parameters for apploader entrypoint
    GPR[3] = 0x81300004;            // save apploader _prolog offset
    GPR[4] = 0x81300008;            // main
    GPR[5] = 0x8130000c;            // _epilog

    // set stack
    SP = 0x816ffffc;
    SDA1 = 0x81100000;      // Fake sda1

    // execute entrypoint
    PC = appEntryPoint;
    LR = 0;
    while(PC) IPTExecuteOpcode();

    // get apploader interface offsets
    MEMReadWord(0x81300004, &_prolog);
    MEMReadWord(0x81300008, &_main);
    MEMReadWord(0x8130000c, &_epilog);

    DBReport( YEL "apploader interface : init : %08X main : %08X close : %08X\n", 
              _prolog, _main, _epilog );

    // execute apploader prolog
    GPR[3] = 0x81300000;            // OSReport callback as parameter
    PC = _prolog;
    LR = 0;
    while(PC) IPTExecuteOpcode();

    // execute apploader main
    do
    {
        // apploader main parameters
        GPR[3] = 0x81300004;        // memory address
        GPR[4] = 0x81300008;        // size
        GPR[5] = 0x8130000c;        // disk offset

        PC = _main;
        LR = 0;
        while(PC) IPTExecuteOpcode();

        MEMReadWord(0x81300004, &addr);
        MEMReadWord(0x81300008, &size);
        MEMReadWord(0x8130000c, &offs);

        if(size)
        {
            DVDSeek(offs);
            DVDRead(&RAM[addr & RAMMASK], size);

            DBReport( YEL "apploader read : offs : %08X size : %08X addr : %08X\n",
                      offs, size, addr );
        }

    } while(GPR[3] != 0);

    // execute apploader epilog
    PC = _epilog;
    LR = 0;
    while(PC) IPTExecuteOpcode();

    // enable hardware update
    HWEnableUpdate(1);

    PC = GPR[3];
    DBReport( NORM "\n");
}

// RTC -> TBR
static void __SyncTime()
{
    if(GetConfigInt(USER_RTC, USER_RTC_DEFAULT) == FALSE)
    {
        TBR = 0;
        return;
    }

    RTCUpdate();

    DBReport(GREEN "updating timer value..\n");

    int32_t counterBias = (int32_t)MEMSwap(exi.sram.counterBias);
    int32_t rtcValue = exi.rtcVal + counterBias;
    DBReport(GREEN "counter bias: %i, real-time clock: %i\n", counterBias, exi.rtcVal);

    int64_t newTime = (int64_t)rtcValue * CPU_TIMER_CLOCK;
    int64_t systemTime;
    MEMReadDouble(0x800030d8, (uint64_t *)&systemTime);
    systemTime += newTime - TBR;
    MEMWriteDouble(0x800030d8, (uint64_t *)&systemTime);
    TBR = newTime;
    DBReport(GREEN "new timer: %08X%08X\n\n", cpu.tb.Part.u, cpu.tb.Part.l);
}

void BootROM(BOOL dvd)
{
    // set initial MMU state, according with BS2/Dolphin OS
    for(int sr=0; sr<16; sr++)              // unmounted
    {
        SR[sr] = 0x80000000;
    }
    // DBATs
    DBAT0U = 0x80001fff; DBAT0L = 0x00000002;   // 0x80000000, 256mb, cached
    DBAT1U = 0xc0001fff; DBAT1L = 0x0000002a;   // 0xC0000000, 256mb, uncached
    DBAT2U = 0x00000000; DBAT2L = 0x00000000;   // undefined
    DBAT3U = 0x00000000; DBAT3L = 0x00000000;   // undefined
    // IBATs
    IBAT0U = DBAT0U; IBAT0L = DBAT0L;
    IBAT1U = DBAT1U; IBAT1L = DBAT1L;
    IBAT2U = DBAT2U; IBAT2L = DBAT2L;
    IBAT3U = DBAT3U; IBAT3L = DBAT3L;
    // MSR MMU bits
    MSR |= (MSR_IR | MSR_DR);               // enable translation
    // page table
    SDR1 = 0;

    // reset MMU
    if(mem.mmu) MEMRemapMemory(1, 1);

    MSR &= ~MSR_EE;                         // disable interrupts/DEC
    MSR |= MSR_FP;                          // enable FP

    // from gc-linux dev mailing list
    PVR = 0x00083214;

    // RTC -> TBR
    __SyncTime();

    // modify important OS low memory variables (lomem) (BS)
    uint32_t ver = GetConfigInt(USER_CONSOLE, USER_CONSOLE_DEFAULT);
    MEMWriteWord(0x8000002c, ver);          // console type
    MEMWriteWord(0x80000028, RAMSIZE);      // memsize
    MEMWriteWord(0x800000f0, RAMSIZE);      // simmemsize
    MEMWriteWord(0x800000f8, CPU_BUS_CLOCK);
    MEMWriteWord(0x800000fc, CPU_CORE_CLOCK);

    // install default syscall. not important for Dolphin OS,
    // but should be installed to avoid crash on SC opcode.
    memcpy( &RAM[CPU_EXCEPTION_SYSCALL], 
            default_syscall, 
            sizeof(default_syscall) );

    // simulate or boot apploader, if dvd
    if(dvd)
    {
        // read disk ID information to 0x80000000
        DVDSeek(0);
        DVDRead(RAM, 32);

        // additional PAL/NTSC selection hack for old VIConfigure()
        char *id = (char *)RAM;
        if(id[3] == 'P') MEMWriteWord(0x800000CC, 1);   // set to PAL
        else MEMWriteWord(0x800000CC, 0);

        BootApploader();
    }
    else ReadFST(); // load FST, for demos
}
