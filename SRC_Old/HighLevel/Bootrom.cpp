// BS and BS2 (IPL) simulation.
#include "pch.h"

using namespace Debug;

// The simulation of BS and BS2 is performed with the cache turned off virtually.

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
    DVD::Seek(DVD_BB2_OFFSET);
    DVD::Read((uint8_t *)bb2, 32);

    // rounding is not important, but present in new apploaders.
    // FST memory address is calculated, by adjusting bb[4] with "DOL LIMIT";
    // DOL limit is fixed to 4 mb, for most apploaders (in release date range
    // from AnimalCrossing to Zelda: Wind Waker).
    fstOffs = _BYTESWAP_UINT32(bb2[1]);
    fstSize = ROUND32(_BYTESWAP_UINT32(bb2[2]));
    fstMaxSize = ROUND32(_BYTESWAP_UINT32(bb2[3]));
    fstAddr = _BYTESWAP_UINT32(bb2[4]);      // Ignore this

    uint32_t ArenaHi = 0;
    Core->ReadWord(0x80000034, &ArenaHi);
    ArenaHi -= fstSize;
    Core->WriteWord(0x80000034, ArenaHi);

    // load FST into memory
    DVD::Seek(fstOffs);
    DVD::Read(&mi.ram[ArenaHi & RAMMASK], fstSize);

    // save fst configuration in lomem
    Core->WriteWord(0x80000038, ArenaHi);
    Core->WriteWord(0x8000003c, fstMaxSize);

    // adjust arenaHi (OSInit will override it anyway, but not home demos)
    // arenaLo set to 0
    //CPUWriteWord(0x80000030, 0);
    //CPUWriteWord(0x80000034, fstAddr);
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

    // I use prolog/epilog terms here, but Nintendo is using 
    // something weird, like : appLoaderFunc1 (see Zelda dump - it 
    // has some compilation garbage parts from bootrom, hehe).

    Report( Channel::HLE, "booting apploader..\n");

    // set OSReport dummy
    Core->WriteWord(0x81300000, 0x4e800020 /* blr opcode */);

    DVD::Seek(DVD_APPLDR_OFFSET);                // apploader offset
    DVD::Read((uint8_t *)appHeader, 32);   // read apploader header
    Gekko::GekkoCore::SwapArea(appHeader, 32);     // and swap it

    // save apploader info
    appEntryPoint = appHeader[4];
    appSize = appHeader[5];

    // load apploader image
    DVD::Seek(0x2460);
    DVD::Read(&mi.ram[0x81200000 & RAMMASK], appSize);

    // set parameters for apploader entrypoint
    Core->regs.gpr[3] = 0x81300004;            // save apploader _prolog offset
    Core->regs.gpr[4] = 0x81300008;            // main
    Core->regs.gpr[5] = 0x8130000c;            // _epilog

    // execute entrypoint
    Core->regs.pc = appEntryPoint;
    Core->regs.spr[(int)Gekko::SPR::LR] = 0;
    while (Core->regs.pc)
    {
        Core->Step();
    }

    // get apploader interface offsets
    Core->ReadWord(0x81300004, &_prolog);
    Core->ReadWord(0x81300008, &_main);
    Core->ReadWord(0x8130000c, &_epilog);

    Report( Channel::HLE, "apploader interface : init : %08X main : %08X close : %08X\n",
            _prolog, _main, _epilog );

    // execute apploader prolog
    Core->regs.gpr[3] = 0x81300000;            // OSReport callback as parameter
    Core->regs.pc = _prolog;
    Core->regs.spr[(int)Gekko::SPR::LR] = 0;
    while (Core->regs.pc)
    {
        Core->Step();
    }

    // execute apploader main
    do
    {
        // apploader main parameters
        Core->regs.gpr[3] = 0x81300004;        // memory address
        Core->regs.gpr[4] = 0x81300008;        // size
        Core->regs.gpr[5] = 0x8130000c;        // disk offset

        Core->regs.pc = _main;
        Core->regs.spr[(int)Gekko::SPR::LR] = 0;
        while (Core->regs.pc)
        {
            Core->Step();
        }

        Core->ReadWord(0x81300004, &addr);
        Core->ReadWord(0x81300008, &size);
        Core->ReadWord(0x8130000c, &offs);

        if(size)
        {
            DVD::Seek(offs);
            DVD::Read(&mi.ram[addr & RAMMASK], size);

            Report( Channel::HLE, "apploader read : offs : %08X size : %08X addr : %08X\n",
                    offs, size, addr );
        }

    } while(Core->regs.gpr[3] != 0);

    // execute apploader epilog
    Core->regs.pc = _epilog;
    Core->regs.spr[(int)Gekko::SPR::LR] = 0;
    while (Core->regs.pc)
    {
        Core->Step();
    }

    Core->regs.pc = Core->regs.gpr[3];
    Report(Channel::Norm, "\n");
}

// RTC -> TBR
static void __SyncTime(bool rtc)
{
    if(!rtc)
    {
        Core->regs.tb.uval = 0;
        return;
    }

    RTCUpdate();

    Report(Channel::HLE, "updating timer value..\n");

    int32_t counterBias = (int32_t)_BYTESWAP_UINT32(exi.sram.counterBias);
    int32_t rtcValue = exi.rtcVal + counterBias;
    Report(Channel::HLE, "counter bias: %i, real-time clock: %i\n", counterBias, exi.rtcVal);

    int64_t newTime = (int64_t)rtcValue * CPU_TIMER_CLOCK;
    int64_t systemTime;
    Core->ReadDouble(0x800030d8, (uint64_t *)&systemTime);
    systemTime += newTime - Core->regs.tb.sval;
    Core->WriteDouble(0x800030d8, (uint64_t *)&systemTime);
    Core->regs.tb.sval = newTime;
    Report(Channel::HLE, "new timer: 0x%llx\n\n", Core->GetTicks());
}

void BootROM(bool dvd, bool rtc, uint32_t consoleVer)
{
    // set initial MMU state, according with BS2/Dolphin OS
    for(int sr=0; sr<16; sr++)
    {
        Core->regs.sr[sr] = 0x80000000;
    }
    // DBATs
    Core->regs.spr[(int)Gekko::SPR::DBAT0U] = 0x80001fff; Core->regs.spr[(int)Gekko::SPR::DBAT0L] = 0x00000002;   // 0x80000000, 256mb, Write-back cached
    Core->regs.spr[(int)Gekko::SPR::DBAT1U] = 0xc0001fff; Core->regs.spr[(int)Gekko::SPR::DBAT1L] = 0x0000002a;   // 0xC0000000, 256mb, Cache inhibited, Guarded
    Core->regs.spr[(int)Gekko::SPR::DBAT2U] = 0x00000000; Core->regs.spr[(int)Gekko::SPR::DBAT2L] = 0x00000000;   // undefined
    Core->regs.spr[(int)Gekko::SPR::DBAT3U] = 0x00000000; Core->regs.spr[(int)Gekko::SPR::DBAT3L] = 0x00000000;   // undefined
    // IBATs
    Core->regs.spr[(int)Gekko::SPR::IBAT0U] = Core->regs.spr[(int)Gekko::SPR::DBAT0U];
    Core->regs.spr[(int)Gekko::SPR::IBAT0L] = Core->regs.spr[(int)Gekko::SPR::DBAT0L];
    Core->regs.spr[(int)Gekko::SPR::IBAT1U] = Core->regs.spr[(int)Gekko::SPR::DBAT1U];
    Core->regs.spr[(int)Gekko::SPR::IBAT1L] = Core->regs.spr[(int)Gekko::SPR::DBAT1L];
    Core->regs.spr[(int)Gekko::SPR::IBAT2U] = Core->regs.spr[(int)Gekko::SPR::DBAT2U];
    Core->regs.spr[(int)Gekko::SPR::IBAT2L] = Core->regs.spr[(int)Gekko::SPR::DBAT2L];
    Core->regs.spr[(int)Gekko::SPR::IBAT3U] = Core->regs.spr[(int)Gekko::SPR::DBAT3U];
    Core->regs.spr[(int)Gekko::SPR::IBAT3L] = Core->regs.spr[(int)Gekko::SPR::DBAT3L];
    // MSR MMU bits
    Core->regs.msr |= (MSR_IR | MSR_DR);               // enable translation
    // page table
    Core->regs.spr[(int)Gekko::SPR::SDR1] = 0;

    Core->regs.msr &= ~MSR_EE;                         // disable interrupts/DEC
    Core->regs.msr |= MSR_FP;                          // enable FP

    // from gc-linux dev mailing list
    Core->regs.spr[(int)Gekko::SPR::PVR] = 0x00083214;

    // RTC -> TBR
    __SyncTime(rtc);

    // modify important OS low memory variables (lomem) (BS)
    Core->WriteWord(0x8000002c, consoleVer);   // console type
    Core->WriteWord(0x80000028, RAMSIZE);      // memsize
    Core->WriteWord(0x800000f0, RAMSIZE);      // simmemsize
    Core->WriteWord(0x800000f8, CPU_BUS_CLOCK);
    Core->WriteWord(0x800000fc, CPU_CORE_CLOCK);

    // install default syscall. not important for Dolphin OS,
    // but should be installed to avoid crash on SC opcode.
    memcpy( &mi.ram[(int)Gekko::Exception::EXCEPTION_SYSTEM_CALL],
            default_syscall, 
            sizeof(default_syscall) );

    // set stack
    Core->regs.gpr[1] = 0x816ffffc;
    Core->regs.gpr[13] = 0x81100000;      // Fake sda1

    // simulate or boot apploader, if dvd
    if(dvd)
    {
        // read disk ID information to 0x80000000
        DVD::Seek(0);
        DVD::Read(mi.ram, 32);

        // additional PAL/NTSC selection hack for old VIConfigure()
        char *id = (char *)mi.ram;
        if(id[3] == 'P') Core->WriteWord(0x800000CC, 1);   // set to PAL
        else Core->WriteWord(0x800000CC, 0);

        BootApploader();
    }
    else
    {
        Core->WriteWord(0x80000034, Core->regs.gpr[1] - 0x10000);

        ReadFST(); // load FST, for demos
    }
}
