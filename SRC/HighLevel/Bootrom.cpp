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
    DVD::Seek(DVD_BB2_OFFSET);
    DVD::Read((uint8_t *)bb2, 32);

    // rounding is not important, but present in new apploaders.
    // FST memory address is calculated, by adjusting bb[4] with "DOL LIMIT";
    // DOL limit is fixed to 4 mb, for most apploaders (in release date range
    // from AnimalCrossing to Zelda: Wind Waker).
    fstOffs = _byteswap_ulong(bb2[1]);
    fstSize = ROUND32(_byteswap_ulong(bb2[2]));
    fstMaxSize = ROUND32(_byteswap_ulong(bb2[3]));
    fstAddr = _byteswap_ulong(bb2[4]);      // Ignore this

    uint32_t ArenaHi = 0;
    Gekko::Gekko->ReadWord(0x80000034, &ArenaHi);
    ArenaHi -= fstSize;
    Gekko::Gekko->WriteWord(0x80000034, ArenaHi);

    // load FST into memory
    DVD::Seek(fstOffs);
    DVD::Read(&mi.ram[ArenaHi & RAMMASK], fstSize);

    // save fst configuration in lomem
    Gekko::Gekko->WriteWord(0x80000038, ArenaHi);
    Gekko::Gekko->WriteWord(0x8000003c, fstMaxSize);

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

    DBReport2( DbgChannel::HLE, "booting apploader..\n");

    // set OSReport dummy
    Gekko::Gekko->WriteWord(0x81300000, 0x4e800020 /* blr opcode */);

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
    Gekko::Gekko->regs.gpr[3] = 0x81300004;            // save apploader _prolog offset
    Gekko::Gekko->regs.gpr[4] = 0x81300008;            // main
    Gekko::Gekko->regs.gpr[5] = 0x8130000c;            // _epilog

    // execute entrypoint
    Gekko::Gekko->regs.pc = appEntryPoint;
    Gekko::Gekko->regs.spr[(int)Gekko::SPR::LR] = 0;
    while (Gekko::Gekko->regs.pc)
    {
        Gekko::Gekko->Step();
    }

    // get apploader interface offsets
    Gekko::Gekko->ReadWord(0x81300004, &_prolog);
    Gekko::Gekko->ReadWord(0x81300008, &_main);
    Gekko::Gekko->ReadWord(0x8130000c, &_epilog);

    DBReport2(DbgChannel::HLE, "apploader interface : init : %08X main : %08X close : %08X\n",
              _prolog, _main, _epilog );

    // execute apploader prolog
    Gekko::Gekko->regs.gpr[3] = 0x81300000;            // OSReport callback as parameter
    Gekko::Gekko->regs.pc = _prolog;
    Gekko::Gekko->regs.spr[(int)Gekko::SPR::LR] = 0;
    while (Gekko::Gekko->regs.pc)
    {
        Gekko::Gekko->Step();
    }

    // execute apploader main
    do
    {
        // apploader main parameters
        Gekko::Gekko->regs.gpr[3] = 0x81300004;        // memory address
        Gekko::Gekko->regs.gpr[4] = 0x81300008;        // size
        Gekko::Gekko->regs.gpr[5] = 0x8130000c;        // disk offset

        Gekko::Gekko->regs.pc = _main;
        Gekko::Gekko->regs.spr[(int)Gekko::SPR::LR] = 0;
        while (Gekko::Gekko->regs.pc)
        {
            Gekko::Gekko->Step();
        }

        Gekko::Gekko->ReadWord(0x81300004, &addr);
        Gekko::Gekko->ReadWord(0x81300008, &size);
        Gekko::Gekko->ReadWord(0x8130000c, &offs);

        if(size)
        {
            DVD::Seek(offs);
            DVD::Read(&mi.ram[addr & RAMMASK], size);

            DBReport2(DbgChannel::HLE, "apploader read : offs : %08X size : %08X addr : %08X\n",
                      offs, size, addr );
        }

    } while(Gekko::Gekko->regs.gpr[3] != 0);

    // execute apploader epilog
    Gekko::Gekko->regs.pc = _epilog;
    Gekko::Gekko->regs.spr[(int)Gekko::SPR::LR] = 0;
    while (Gekko::Gekko->regs.pc)
    {
        Gekko::Gekko->Step();
    }

    Gekko::Gekko->regs.pc = Gekko::Gekko->regs.gpr[3];
    DBReport("\n");
}

// RTC -> TBR
static void __SyncTime(bool rtc)
{
    if(!rtc)
    {
        Gekko::Gekko->regs.tb.uval = 0;
        return;
    }

    RTCUpdate();

    DBReport2(DbgChannel::HLE, "updating timer value..\n");

    int32_t counterBias = (int32_t)_byteswap_ulong(exi.sram.counterBias);
    int32_t rtcValue = exi.rtcVal + counterBias;
    DBReport2(DbgChannel::HLE, "counter bias: %i, real-time clock: %i\n", counterBias, exi.rtcVal);

    int64_t newTime = (int64_t)rtcValue * CPU_TIMER_CLOCK;
    int64_t systemTime;
    Gekko::Gekko->ReadDouble(0x800030d8, (uint64_t *)&systemTime);
    systemTime += newTime - Gekko::Gekko->regs.tb.sval;
    Gekko::Gekko->WriteDouble(0x800030d8, (uint64_t *)&systemTime);
    Gekko::Gekko->regs.tb.sval = newTime;
    DBReport2(DbgChannel::HLE, "new timer: 0x%llx\n\n", Gekko::Gekko->GetTicks());
}

void BootROM(bool dvd, bool rtc, uint32_t consoleVer)
{
    // set initial MMU state, according with BS2/Dolphin OS
    for(int sr=0; sr<16; sr++)
    {
        Gekko::Gekko->regs.sr[sr] = 0x80000000;
    }
    // DBATs
    Gekko::Gekko->regs.spr[(int)Gekko::SPR::DBAT0U] = 0x80001fff; Gekko::Gekko->regs.spr[(int)Gekko::SPR::DBAT0L] = 0x00000002;   // 0x80000000, 256mb, Write-back cached
    Gekko::Gekko->regs.spr[(int)Gekko::SPR::DBAT1U] = 0xc0001fff; Gekko::Gekko->regs.spr[(int)Gekko::SPR::DBAT1L] = 0x0000002a;   // 0xC0000000, 256mb, Cache inhibited, Guarded
    Gekko::Gekko->regs.spr[(int)Gekko::SPR::DBAT2U] = 0x00000000; Gekko::Gekko->regs.spr[(int)Gekko::SPR::DBAT2L] = 0x00000000;   // undefined
    Gekko::Gekko->regs.spr[(int)Gekko::SPR::DBAT3U] = 0x00000000; Gekko::Gekko->regs.spr[(int)Gekko::SPR::DBAT3L] = 0x00000000;   // undefined
    // IBATs
    Gekko::Gekko->regs.spr[(int)Gekko::SPR::IBAT0U] = Gekko::Gekko->regs.spr[(int)Gekko::SPR::DBAT0U];
    Gekko::Gekko->regs.spr[(int)Gekko::SPR::IBAT0L] = Gekko::Gekko->regs.spr[(int)Gekko::SPR::DBAT0L];
    Gekko::Gekko->regs.spr[(int)Gekko::SPR::IBAT1U] = Gekko::Gekko->regs.spr[(int)Gekko::SPR::DBAT1U];
    Gekko::Gekko->regs.spr[(int)Gekko::SPR::IBAT1L] = Gekko::Gekko->regs.spr[(int)Gekko::SPR::DBAT1L];
    Gekko::Gekko->regs.spr[(int)Gekko::SPR::IBAT2U] = Gekko::Gekko->regs.spr[(int)Gekko::SPR::DBAT2U];
    Gekko::Gekko->regs.spr[(int)Gekko::SPR::IBAT2L] = Gekko::Gekko->regs.spr[(int)Gekko::SPR::DBAT2L];
    Gekko::Gekko->regs.spr[(int)Gekko::SPR::IBAT3U] = Gekko::Gekko->regs.spr[(int)Gekko::SPR::DBAT3U];
    Gekko::Gekko->regs.spr[(int)Gekko::SPR::IBAT3L] = Gekko::Gekko->regs.spr[(int)Gekko::SPR::DBAT3L];
    // MSR MMU bits
    Gekko::Gekko->regs.msr |= (MSR_IR | MSR_DR);               // enable translation
    // page table
    Gekko::Gekko->regs.spr[(int)Gekko::SPR::SDR1] = 0;

    Gekko::Gekko->regs.msr &= ~MSR_EE;                         // disable interrupts/DEC
    Gekko::Gekko->regs.msr |= MSR_FP;                          // enable FP

    // from gc-linux dev mailing list
    Gekko::Gekko->regs.spr[(int)Gekko::SPR::PVR] = 0x00083214;

    // RTC -> TBR
    __SyncTime(rtc);

    // modify important OS low memory variables (lomem) (BS)
    Gekko::Gekko->WriteWord(0x8000002c, consoleVer);   // console type
    Gekko::Gekko->WriteWord(0x80000028, RAMSIZE);      // memsize
    Gekko::Gekko->WriteWord(0x800000f0, RAMSIZE);      // simmemsize
    Gekko::Gekko->WriteWord(0x800000f8, CPU_BUS_CLOCK);
    Gekko::Gekko->WriteWord(0x800000fc, CPU_CORE_CLOCK);

    // install default syscall. not important for Dolphin OS,
    // but should be installed to avoid crash on SC opcode.
    memcpy( &mi.ram[(int)Gekko::Exception::SYSCALL],
            default_syscall, 
            sizeof(default_syscall) );

    // set stack
    Gekko::Gekko->regs.gpr[1] = 0x816ffffc;
    Gekko::Gekko->regs.gpr[13] = 0x81100000;      // Fake sda1

    // simulate or boot apploader, if dvd
    if(dvd)
    {
        // read disk ID information to 0x80000000
        DVD::Seek(0);
        DVD::Read(mi.ram, 32);

        // additional PAL/NTSC selection hack for old VIConfigure()
        char *id = (char *)mi.ram;
        if(id[3] == 'P') Gekko::Gekko->WriteWord(0x800000CC, 1);   // set to PAL
        else Gekko::Gekko->WriteWord(0x800000CC, 0);

        BootApploader();
    }
    else
    {
        Gekko::Gekko->WriteWord(0x80000034, Gekko::Gekko->regs.gpr[1] - 0x10000);

        ReadFST(); // load FST, for demos
    }

    // Enable data cache
    Gekko::Gekko->regs.spr[(int)Gekko::SPR::HID0] = HID0_DCE;
    Gekko::Gekko->cache.Enable(true);
}
