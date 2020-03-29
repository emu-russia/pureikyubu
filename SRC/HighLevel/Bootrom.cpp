// BS and BS2 (IPL) simulation.
#include "pch.h"

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

    // read BB2
    DVD::Seek(0x420);
    DVD::Read((uint8_t *)bb2, 32);

    // rounding is not important, but present in new apploaders.
    // FST memory address is calculated, by adjusting bb[4] with "DOL LIMIT";
    // DOL limit is fixed to 4 mb, for most apploaders (in release date range
    // from AnimalCrossing to Zelda: Wind Waker).
    fstOffs = _byteswap_ulong(bb2[1]);
    fstSize = ROUND32(_byteswap_ulong(bb2[2]));
    fstMaxSize = ROUND32(_byteswap_ulong(bb2[3]));
    fstAddr = _byteswap_ulong(bb2[4]) + RAMSIZE - DOL_LIMIT;

    // load FST into memory
    DVD::Seek(fstOffs);
    DVD::Read(&mi.ram[fstAddr & RAMMASK], fstSize);

    // save fst configuration in lomem
    CPUWriteWord(0x80000038, fstAddr);
    CPUWriteWord(0x8000003c, fstMaxSize);

    // adjust arenaHi (OSInit will override it anyway, but not home demos)
    // arenaLo set to 0
    //CPUWriteWord(0x80000030, 0);
    //CPUWriteWord(0x80000034, fstAddr);
}

// execute apploader (apploader base is 0x81200000)
// this is exact apploader emulation. it is safe and checked.
static void BootApploader(Gekko::GekkoCore * core)
{
    uint32_t     appHeader[8];           // apploader header information
    uint32_t     appSize;                // size of apploader image
    uint32_t     appEntryPoint;
    uint32_t     _prolog, _main, _epilog;
    uint32_t     offs, size, addr;       // return of apploader main

    // I use prolog/epilog terms here, but Nintendo is using 
    // something weird, like : appLoaderFunc1 (see Zelda dump - it 
    // has some compilation garbage parts from bootrom, hehe).

    DBReport2( DbgChannel::HLE, "booting apploader..\n");

    // set OSReport dummy
    CPUWriteWord(0x81300000, 0x4e800020 /* blr opcode */);

    DVD::Seek(0x2440);                // apploader offset
    DVD::Read((uint8_t *)appHeader, 32);   // read apploader header
    Gekko::GekkoCore::SwapArea(appHeader, 32);     // and swap it

    // save apploader info
    appEntryPoint = appHeader[4];
    appSize = appHeader[5];

    // load apploader image
    DVD::Seek(0x2460);
    DVD::Read(&mi.ram[0x81200000 & RAMMASK], appSize);

    // set parameters for apploader entrypoint
    GPR[3] = 0x81300004;            // save apploader _prolog offset
    GPR[4] = 0x81300008;            // main
    GPR[5] = 0x8130000c;            // _epilog

    // execute entrypoint
    PC = appEntryPoint;
    PPC_LR = 0;
    while(PC) IPTExecuteOpcode(core);

    // get apploader interface offsets
    CPUReadWord(0x81300004, &_prolog);
    CPUReadWord(0x81300008, &_main);
    CPUReadWord(0x8130000c, &_epilog);

    DBReport2(DbgChannel::HLE, "apploader interface : init : %08X main : %08X close : %08X\n",
              _prolog, _main, _epilog );

    // execute apploader prolog
    GPR[3] = 0x81300000;            // OSReport callback as parameter
    PC = _prolog;
    PPC_LR = 0;
    while(PC) IPTExecuteOpcode(core);

    // execute apploader main
    do
    {
        // apploader main parameters
        GPR[3] = 0x81300004;        // memory address
        GPR[4] = 0x81300008;        // size
        GPR[5] = 0x8130000c;        // disk offset

        PC = _main;
        PPC_LR = 0;
        while(PC) IPTExecuteOpcode(core);

        CPUReadWord(0x81300004, &addr);
        CPUReadWord(0x81300008, &size);
        CPUReadWord(0x8130000c, &offs);

        if(size)
        {
            DVD::Seek(offs);
            DVD::Read(&mi.ram[addr & RAMMASK], size);

            DBReport2(DbgChannel::HLE, "apploader read : offs : %08X size : %08X addr : %08X\n",
                      offs, size, addr );
        }

    } while(GPR[3] != 0);

    // execute apploader epilog
    PC = _epilog;
    PPC_LR = 0;
    while(PC) IPTExecuteOpcode(core);

    PC = GPR[3];
    DBReport("\n");
}

// RTC -> TBR
static void __SyncTime(bool rtc)
{
    if(!rtc)
    {
        TBR = 0;
        return;
    }

    RTCUpdate();

    DBReport2(DbgChannel::HLE, "updating timer value..\n");

    int32_t counterBias = (int32_t)_byteswap_ulong(exi.sram.counterBias);
    int32_t rtcValue = exi.rtcVal + counterBias;
    DBReport2(DbgChannel::HLE, "counter bias: %i, real-time clock: %i\n", counterBias, exi.rtcVal);

    int64_t newTime = (int64_t)rtcValue * CPU_TIMER_CLOCK;
    int64_t systemTime;
    CPUReadDouble(0x800030d8, (uint64_t *)&systemTime);
    systemTime += newTime - TBR;
    CPUWriteDouble(0x800030d8, (uint64_t *)&systemTime);
    TBR = newTime;
    DBReport2(DbgChannel::HLE, "new timer: %08X%08X\n\n", cpu.tb.Part.u, cpu.tb.Part.l);
}

void BootROM(bool dvd, bool rtc, uint32_t consoleVer, Gekko::GekkoCore* core)
{
    // set initial MMU state, according with BS2/Dolphin OS
    for(int sr=0; sr<16; sr++)              // unmounted
    {
        PPC_SR[sr] = 0x80000000;
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

    MSR &= ~MSR_EE;                         // disable interrupts/DEC
    MSR |= MSR_FP;                          // enable FP

    // from gc-linux dev mailing list
    PVR = 0x00083214;

    // RTC -> TBR
    __SyncTime(rtc);

    // modify important OS low memory variables (lomem) (BS)
    CPUWriteWord(0x8000002c, consoleVer);   // console type
    CPUWriteWord(0x80000028, RAMSIZE);      // memsize
    CPUWriteWord(0x800000f0, RAMSIZE);      // simmemsize
    CPUWriteWord(0x800000f8, CPU_BUS_CLOCK);
    CPUWriteWord(0x800000fc, CPU_CORE_CLOCK);

    // install default syscall. not important for Dolphin OS,
    // but should be installed to avoid crash on SC opcode.
    memcpy( &mi.ram[CPU_EXCEPTION_SYSCALL],
            default_syscall, 
            sizeof(default_syscall) );

    // set stack
    SP = 0x816ffffc;
    SDA1 = 0x81100000;      // Fake sda1

    // simulate or boot apploader, if dvd
    if(dvd)
    {
        // read disk ID information to 0x80000000
        DVD::Seek(0);
        DVD::Read(mi.ram, 32);

        // additional PAL/NTSC selection hack for old VIConfigure()
        char *id = (char *)mi.ram;
        if(id[3] == 'P') CPUWriteWord(0x800000CC, 1);   // set to PAL
        else CPUWriteWord(0x800000CC, 0);

        BootApploader(core);
    }
    else
    {
        CPUWriteWord(0x80000034, SP);

        ReadFST(); // load FST, for demos
    }
}
