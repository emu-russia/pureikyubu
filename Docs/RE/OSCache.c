#include <dolphin.h>

/*
 * L1 Locked Cache.
*/

asm void __LCEnable(void)
{
    nofralloc;

    // Allow machine check exception.
    mfmsr   r5
    ori     r5, r5, MSR_ME
    mtmsr   r5

    lis     r3, 0x8000
    li      r4, 1024
    mtctr   r4
FlushDataCache:
    dcbt    r0, r3
    dcbst   r0, r3
    addi    r3, r3, 32
    bdnz+   FlushDataCache

    // Enable locked cache.
    mfspr   r4, HID2
    oris    r4, r4, (HID2_LCE | HID2_DCHEE | HID2_DNCEE | HID2_DCMEE | HID2_DQOEE) >> 16
    mtspr   HID2, r4

    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop

    lis     r3, 0xE000
    ori     r3, r3, 0x0002
    mtspr   DBAT3L, r3
    ori     r3, r3, 0x01FE
    mtspr   DBAT3U, r3
    isync

    lis     r3, 0xE000
    li      r6, 512
    mtctr   r6
    li      r6, 0
ClearLockedCache:
    dcbz_l  r6, r3
    addi    r3, r3, 32
    bdnz+   ClearLockedCache

    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop

    blr
}

void LCEnable(void)
{
    BOOL old = OSDisableInterrupts();
    __LCEnable();
    OSRestoreInterrupts(old);
}


/*
 * OS Cache Initialization.
*/

void __OSCacheInit(void)
{
    u32 old_msr;

    // ICache.
    if( (PPCMfhid0() & HID0_ICE) == 0 )
    {
        ICEnable();
        DBPrintf("L1 i-caches initialized\n");
    }

    // DCache.
    if( (PPCMfhid0() & HID0_DCE) == 0 )
    {
        DCEnable();
        DBPrintf("L1 d-caches initialized\n");
    }

    // L2
    if( (PPCMfl2cr() & L2CR_L2E) == 0 )
    {
        old_msr = PPCMfmsr();
        __sync();
        PPCMtmsr(MSR_IR | MSR_DR);
        __sync();
        __sync();
        PPCMtl2cr( PPCMfl2cr() & ~L2CR_L2E);
        __sync();
        L2GlobalInvalidate();
        PPCMtmsr(old_msr);
        PPCMtl2cr( (PPCMfl2cr() | L2CR_L2E) & ~L2CR_L2I);
        DBPrintf("L2 cache initialized\n");
    }

    // Locked D-cache.
    OSSetErrorHandler(OS_ERROR_MACHINE_CHECK, DMAErrorHandler);
    DBPrintf("Locked cache machine check handler installed\n");
}
