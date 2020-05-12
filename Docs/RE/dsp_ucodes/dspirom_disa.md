# Disassembled dsp_irom.bin

EDIT: This disassembly got screwed because Duddie has incorrect register index:
- 0x19: ax0.h  -> Should be ax1.l
- 0x1A: ax1.l  -> Should be ax0.h


## Send sync mail (0x8071FEED).

```
8000 00 92 00 FF    lri     config, #0x00FF
8002 12 06          sbset   12
8003 12 02          sbset   8
8004 12 03          sbset   9
8005 12 04          sbset   10
8006 12 05          sbset   11
8007 8E 00          clr40                               
8008 8C 00          clr15                               
8009 8B 00          m0                                  
800A 16 FC 80 71    si      $(DMBH), #0x8071
800C 16 FD FE ED    si      $(DMBL), #0xFEED
```

## Microcode FEED Loop

```
800E 81 00          clr     ac0                         
800F 89 00          clr     ac1                         
8010 02 BF 80 78    call    WaitCpuMailbox
8012 00 9F 80 F3    lri     ac1.m, #0x80F3
8014 82 00          cmp                                 
8015 02 95 80 1F    jeq     $0x801F
8017 27 FF          lrs     ac1.m, $(CMBL)
8018 16 FC FE EE    si      $(DMBH), #0xFEEE
801A 2E FD          srs     $(DMBL), ac0.m
801B 02 BF 80 7E    call    WaitDspMailbox
801D 02 9F 80 0E    j       $0x800E
801F 26 FF          lrs     ac0.m, $(CMBL)
8020 00 9F A0 01    lri     ac1.m, #0xA001
8022 82 00          cmp                                 
8023 02 94 80 2C    jne     $0x802C
8025 02 BF 80 78    call    WaitCpuMailbox
8027 27 FF          lrs     ac1.m, $(CMBL)
8028 1C 9E          mrr     ix0, ac0.m
8029 1C BF          mrr     ix1, ac1.m
802A 02 9F 80 0E    j       $0x800E
802C 00 9F A0 02    lri     ac1.m, #0xA002
802E 82 00          cmp                                 
802F 02 94 80 37    jne     $0x8037
8031 02 BF 80 78    call    WaitCpuMailbox
8033 27 FF          lrs     ac1.m, $(CMBL)
8034 1C FF          mrr     ix3, ac1.m
8035 02 9F 80 0E    j       $0x800E
8037 00 9F C0 02    lri     ac1.m, #0xC002
8039 82 00          cmp                                 
803A 02 94 80 42    jne     $0x8042
803C 02 BF 80 78    call    WaitCpuMailbox
803E 27 FF          lrs     ac1.m, $(CMBL)
803F 1C DF          mrr     ix2, ac1.m
8040 02 9F 80 0E    j       $0x800E
8042 00 9F B0 01    lri     ac1.m, #0xB001
8044 82 00          cmp                                 
8045 02 94 80 4E    jne     $0x804E
8047 02 BF 80 78    call    WaitCpuMailbox
8049 27 FF          lrs     ac1.m, $(CMBL)
804A 1F 5E          mrr     ax1.l, ac0.m
804B 1F 1F          mrr     ax0.l, ac1.m
804C 02 9F 80 0E    j       $0x800E
804E 00 9F B0 02    lri     ac1.m, #0xB002
8050 82 00          cmp                                 
8051 02 94 80 59    jne     $0x8059
8053 02 BF 80 78    call    WaitCpuMailbox
8055 27 FF          lrs     ac1.m, $(CMBL)
8056 1F 3F          mrr     ax0.h, ac1.m
8057 02 9F 80 0E    j       $0x800E
8059 00 9F C0 01    lri     ac1.m, #0xC001
805B 82 00          cmp                                 
805C 02 94 80 64    jne     $0x8064
805E 02 BF 80 78    call    WaitCpuMailbox
8060 27 FF          lrs     ac1.m, $(CMBL)
8061 1F 7F          mrr     ax1.h, ac1.m
8062 02 9F 80 0E    j       $0x800E
8064 00 9F D0 01    lri     ac1.m, #0xD001
8066 82 00          cmp                                 
8067 02 94 80 71    jne     $0x8071
8069 02 BF 80 78    call    WaitCpuMailbox
806B 81 00          clr     ac0                         
806C 26 FF          lrs     ac0.m, $(CMBL)
806D 1C 1E          mrr     ar0, ac0.m
806E 02 9F 80 B5    j       $0x80B5                 // Run microcode
8070 00 21          halt    
8071 16 FC FA AA    si      $(DMBH), #0xFAAA
8073 2E FD          srs     $(DMBL), ac0.m
8074 02 BF 80 7E    call    WaitDspMailbox
8076 02 9F 80 0E    j       $0x800E
```

```c++

void DspMain ()         // 800E
{
    while (true)
    {
        ac0 = ac1 = 0;
        ac0m = WaitCpuMailbox ();       // High part here

        // First Mail should be always 0x80F3xxxx, following by command sequence mail

        if (ac0m != 0x80F3)
        {
            DMBH = 0xFEEE;          // Invalid 1st message (FEEE)
            DMBL = ac0m;
            WaitDspMailbox ();
            continue;
        }

        // Try command sequence mail

        ac0m = CMBL;
        switch (ac0m)
        {
            // Set ix0,1 registers      (iram_mmem_addr)
            case 0xA001:
                ac0m = WaitCpuMailbox ();
                ix0 = ac0m;
                ix1 = CMBL;
                break;

            // Set ix3          (iram_length bytes)
            case 0xA002:
                ac0m = WaitCpuMailbox();
                ix3 = CMBL;
                break;

            // Set ix2          (iram_addr)
            case 0xc002:
                ac0m = WaitCpuMailbox();
                ix2 = CMBL;         
                break;

            // Set ax1,0 Low            (not used by __DSP_boot_task)
            case 0xB001:
                ac0m = WaitCpuMailbox();
                ax1.l = ac0m;
                ax0.l = CMBL;
                break;

            // Set ax0 High             (always 0 in __DSP_boot_task)
            case 0xB002:
                ac0m = WaitCpuMailbox();
                ax0.h = CMBL;
                break;

            // Set ax1 High             (not used by __DSP_boot_task)
            case 0xC001:
                ac0m = WaitCpuMailbox();
                ax1.h = CMBL;
                break;

            // Execute microcode        (dsp_init_vector)
            case 0xD001:
                WaitCpuMailbox ();
                ar0 = CMBL;
                LoadRunUcode (ar0);

                halt;                   // Never reach here

                break;

            // Unknown command sequence
            default:
                DMBH = 0xFAAA;          // Invalid 2nd message (FAAA)
                DMBL = ac0m;        // Return command to caller

                WaitDspMailbox ();
                break;

        }
    }
}

```

## Mailbox polling

Wait for message from the processor.

```
8078 26 FE          lrs     ac0.m, $(CMBH)                  ; ac0m = CMBH
8079 02 C0 80 00    tset    ac0.m, #0x8000                  ; ok = (ac0m & 0x8000) != 0
807B 02 9C 80 78    jnok    $0x8078                         ; if (!ok) goto 0x8078;
807D 02 DF          ret     
```

```c++
// Wait for message from the processor
uint16_t WaitCpuMailbox()       // 8078
{
    uint16_t acm0;
    while ( ((acm0 = *(uint16_t *)(CMBH)) & 0x8000) == 0 )
    { }
    return ac0m;
}

```

Wait until processor read both the upper and lower parts of DSP Mailbox.

```
807E 26 FC          lrs     ac0.m, $(DMBH)                  ; ac0m = DMBH
807F 02 A0 80 00    tclr    ac0.m, #0x8000                  ; ok = (ac0m & 0x8000) == 0
8081 02 9C 80 7E    jnok    $0x807E                         ; 
8083 02 DF          ret     
```

```c++
// Wait until processor read mailbox
uint16_t WaitDspMailbox()           // 807E
{
    uint16_t ac0m;
    while ( ((ac0m = *(uint16_t *)(DMBH)) & 0x8000) != 0 )
    { }
    return ac0m;
}
```

## Mailbox DSP -> CPU execution chart

```
| Processor side (AX.lib)                   | DSP Side (IROM)                                           |
|-------------------------------------------|-----------------------------------------------------------|
| while(DSPCheckMailFromDSP() == 0) ;       |                                                           |       CPU Waits DPS message  (DSP_INMBOXH.M)
|                                           |                                                           |
|                                           | 800A 16 FC 80 71  si      $(DMBH), #0x8071                |       DSP sends message  (Welcome SYNC)   --> Go to Shadow HI
|                                           |                                                           |
|                                           | 800C 16 FD FE ED  si      $(DMBL), #0xFEED                |       Nom-nom.   Shadow HI -> go to real HI
|                                           |                                                           |
|                                           | while ( ((ac0m = *(uint16_t *)(DMBH)) & 0x8000) != 0 )    |       DSP waits CPU read message
|                                           |                                                           |
| mail = DSPReadMailFromDSP();              |                                                           |       CPU read message
| ASSERT ((uint16_t)mail == 0xFEED);        |                                                           |



```

## DspMem -> MainMem

Unused(?)

```
8084 00 21          halt    
8085 8E 00          clr40                               
8086 81 00          clr     ac0                         
8087 1F D9          mrr     ac0.m, ax0.h
8088 B1 00          tst     ac0                         
8089 02 95 80 9D    jeq     $0x809D
808B 00 FA FF CE    sr      $(DSMAH), ax1.l
808D 00 F8 FF CF    sr      $(DSMAL), ax0.l
808F 00 9E 00 01    lri     ac0.m, #0x0001
8091 00 FE FF C9    sr      $(DSCR), ac0.m
8093 00 FB FF CD    sr      $(DSPA), ax1.h
8095 00 F9 FF CB    sr      $(DSBL), ax0.h
8097 00 DE FF C9    lr      ac0.m, $(DSCR)
8099 02 A0 00 04    tclr    ac0.m, #0x0004
809B 02 9C 80 97    jnok    $0x8097
809D 81 00          clr     ac0                         
809E 1F C7          mrr     ac0.m, ix3
809F B1 00          tst     ac0                         
80A0 02 95 80 B4    jeq     $0x80B4
80A2 00 E4 FF CE    sr      $(DSMAH), ix0
80A4 00 E5 FF CF    sr      $(DSMAL), ix1
80A6 00 9E 00 03    lri     ac0.m, #0x0003
80A8 00 FE FF C9    sr      $(DSCR), ac0.m
80AA 00 E6 FF CD    sr      $(DSPA), ix2
80AC 00 E7 FF CB    sr      $(DSBL), ix3
80AE 00 DE FF C9    lr      ac0.m, $(DSCR)
80B0 02 A0 00 04    tclr    ac0.m, #0x0004
80B2 02 9C 80 AE    jnok    $0x80AE
80B4 02 DF          ret     
```

## Load and Run microcode

```
80B5 8E 00          clr40                               
80B6 81 00          clr     ac0                         
80B7 89 00          clr     ac1                         
80B8 1F F9          mrr     ac1.m, ax0.h
80B9 B9 00          tst     ac1                         
80BA 02 95 80 CE    jeq     $0x80CE
80BC 00 FA FF CE    sr      $(DSMAH), ax1.l         // dram_mmem_addr
80BE 00 F8 FF CF    sr      $(DSMAL), ax0.l
80C0 00 9E 00 00    lri     ac0.m, #0x0000
80C2 00 FE FF C9    sr      $(DSCR), ac0.m
80C4 00 FB FF CD    sr      $(DSPA), ax1.h          // dram_addr
80C6 00 F9 FF CB    sr      $(DSBL), ax0.h          // dram_length
80C8 00 DE FF C9    lr      ac0.m, $(DSCR)
80CA 02 A0 00 04    tclr    ac0.m, #0x0004
80CC 02 9C 80 C8    jnok    $0x80C8

80CE 89 00          clr     ac1                         
80CF 1F E7          mrr     ac1.m, ix3
80D0 B9 00          tst     ac1                         
80D1 02 95 80 E5    jeq     $0x80E5
80D3 00 E4 FF CE    sr      $(DSMAH), ix0            // iram_mmem_addr
80D5 00 E5 FF CF    sr      $(DSMAL), ix1
80D7 00 9E 00 02    lri     ac0.m, #0x0002
80D9 00 FE FF C9    sr      $(DSCR), ac0.m
80DB 00 E6 FF CD    sr      $(DSPA), ix2            // iram_addr
80DD 00 E7 FF CB    sr      $(DSBL), ix3            // iram_length
80DF 00 DE FF C9    lr      ac0.m, $(DSCR)
80E1 02 A0 00 04    tclr    ac0.m, #0x0004
80E3 02 9C 80 DF    jnok    $0x80DF
80E5 17 0F          jmpr    ar0                     // Goto ucode entrypoint  (dsp_init_vector)
80E6 00 21          halt    
```

```c++
void LoadRunUcode(ar0)          // 80B5
{
    clr40();
    ac0 = ac1 = 0;
    ac1m = ax0h;

    if (ac1)
    {
        DSMAH = ax1l;
        DSMAL = ax0l;
        DSCR = 0;           // DMEM Dma, Mmem->DSP
        DSPA = ax1h;
        DSBL = ax0h;

        while (DSCR & 4) ;      // Wait Dma
    }

    ac1 = 0;
    ac1m = ix3;

    if (ac1)
    {
        DSMAH = ix0;
        DSMAL = ix1;
        DSCR = 2;           // IMEM Dma, Mmem->DSP
        DSPA = ix2;
        DSBL = ix3;

        while (DSCR & 4) ;      // Wait Dma
    }

    jmpr ar0;       // Goto ucode entrypoint
    halt;
}

```

// -----------------------------------------------------------------------------------------------------

## Mixing Routines

They only look scary, in fact, they simply mix 32 samples with a duration of 1 ms.

This trick is called unroll the loop.

```
80E7 81 50          clr     ac0                 l       ax1.l, @ar0
80E8 89 49          clr     ac1                 l       ax0.h, @ar1
80E9 B0 72          mulx    ax0.h, ax1.l        l       ac0.m, @ar2
80EA 89 62          clr     ac1                 l       ac0.l, @ar2
80EB F0 7A          lsl16   ac0                 l       ac1.m, @ar2
80EC 19 1A          lrri    ax1.l, @ar0
80ED B4 6A          mulxac  ax0.h, ax1.l, ac0   l       ac1.l, @ar2
80EE 91 00          asr16   ac0                         
80EF F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
80F0 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
80F1 99 72          asr16   ac1                 l       ac0.m, @ar2
80F2 19 5C          lrri    ac0.l, @ar2
80F3 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
80F4 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
80F5 91 7A          asr16   ac0                 l       ac1.m, @ar2
80F6 19 5D          lrri    ac1.l, @ar2
80F7 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
80F8 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
80F9 99 72          asr16   ac1                 l       ac0.m, @ar2
80FA 19 5C          lrri    ac0.l, @ar2
80FB F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
80FC B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
80FD 91 7A          asr16   ac0                 l       ac1.m, @ar2
80FE 19 5D          lrri    ac1.l, @ar2
80FF F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8100 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8101 99 72          asr16   ac1                 l       ac0.m, @ar2
8102 19 5C          lrri    ac0.l, @ar2
8103 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8104 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8105 91 7A          asr16   ac0                 l       ac1.m, @ar2
8106 19 5D          lrri    ac1.l, @ar2
8107 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8108 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8109 99 72          asr16   ac1                 l       ac0.m, @ar2
810A 19 5C          lrri    ac0.l, @ar2
810B F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
810C B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
810D 91 7A          asr16   ac0                 l       ac1.m, @ar2
810E 19 5D          lrri    ac1.l, @ar2
810F F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8110 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8111 99 72          asr16   ac1                 l       ac0.m, @ar2
8112 19 5C          lrri    ac0.l, @ar2
8113 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8114 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8115 91 7A          asr16   ac0                 l       ac1.m, @ar2
8116 19 5D          lrri    ac1.l, @ar2
8117 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8118 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8119 99 72          asr16   ac1                 l       ac0.m, @ar2
811A 19 5C          lrri    ac0.l, @ar2
811B F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
811C B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
811D 91 7A          asr16   ac0                 l       ac1.m, @ar2
811E 19 5D          lrri    ac1.l, @ar2
811F F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8120 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8121 99 72          asr16   ac1                 l       ac0.m, @ar2
8122 19 5C          lrri    ac0.l, @ar2
8123 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8124 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8125 91 7A          asr16   ac0                 l       ac1.m, @ar2
8126 19 5D          lrri    ac1.l, @ar2
8127 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8128 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8129 99 72          asr16   ac1                 l       ac0.m, @ar2
812A 19 5C          lrri    ac0.l, @ar2
812B F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
812C B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
812D 91 7A          asr16   ac0                 l       ac1.m, @ar2
812E 19 5D          lrri    ac1.l, @ar2
812F F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8130 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8131 99 72          asr16   ac1                 l       ac0.m, @ar2
8132 19 5C          lrri    ac0.l, @ar2
8133 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8134 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8135 91 7A          asr16   ac0                 l       ac1.m, @ar2
8136 19 5D          lrri    ac1.l, @ar2
8137 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8138 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8139 99 72          asr16   ac1                 l       ac0.m, @ar2
813A 19 5C          lrri    ac0.l, @ar2
813B F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
813C B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
813D 91 7A          asr16   ac0                 l       ac1.m, @ar2
813E 19 5D          lrri    ac1.l, @ar2
813F F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8140 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8141 99 72          asr16   ac1                 l       ac0.m, @ar2
8142 19 5C          lrri    ac0.l, @ar2
8143 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8144 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8145 91 7A          asr16   ac0                 l       ac1.m, @ar2
8146 19 5D          lrri    ac1.l, @ar2
8147 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8148 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8149 99 72          asr16   ac1                 l       ac0.m, @ar2
814A 19 5C          lrri    ac0.l, @ar2
814B F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
814C B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
814D 91 7A          asr16   ac0                 l       ac1.m, @ar2
814E 19 5D          lrri    ac1.l, @ar2
814F F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8150 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8151 99 72          asr16   ac1                 l       ac0.m, @ar2
8152 19 5C          lrri    ac0.l, @ar2
8153 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8154 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8155 91 7A          asr16   ac0                 l       ac1.m, @ar2
8156 19 5D          lrri    ac1.l, @ar2
8157 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8158 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8159 99 72          asr16   ac1                 l       ac0.m, @ar2
815A 19 5C          lrri    ac0.l, @ar2
815B F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
815C B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
815D 91 7A          asr16   ac0                 l       ac1.m, @ar2
815E 19 5D          lrri    ac1.l, @ar2
815F F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8160 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8161 99 72          asr16   ac1                 l       ac0.m, @ar2
8162 19 5C          lrri    ac0.l, @ar2
8163 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8164 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8165 91 7A          asr16   ac0                 l       ac1.m, @ar2
8166 19 5D          lrri    ac1.l, @ar2
8167 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8168 1B 7C          srri    @ar3, ac0.l
8169 6E 00          movp    ac0                         
816A B5 12          mulxac  ax0.h, ax1.l, ac1   mv      ax0.l, ac0.m
816B 99 09          asr16   ac1                 ir      ar1
816C 1B 7F          srri    @ar3, ac1.m
816D 81 2B          clr     ac0                 s       @ar3, ac1.l
816E 1C 04          mrr     ar0, ix0
816F 1C 45          mrr     ar2, ix1
8170 1C 62          mrr     ar3, ar2
8171 81 50          clr     ac0                 l       ax1.l, @ar0
8172 89 49          clr     ac1                 l       ax0.h, @ar1
8173 B0 72          mulx    ax0.h, ax1.l        l       ac0.m, @ar2
8174 89 62          clr     ac1                 l       ac0.l, @ar2
8175 F0 7A          lsl16   ac0                 l       ac1.m, @ar2
8176 19 1A          lrri    ax1.l, @ar0
8177 B4 6A          mulxac  ax0.h, ax1.l, ac0   l       ac1.l, @ar2
8178 91 00          asr16   ac0                         
8179 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
817A B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
817B 99 72          asr16   ac1                 l       ac0.m, @ar2
817C 19 5C          lrri    ac0.l, @ar2
817D F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
817E B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
817F 91 7A          asr16   ac0                 l       ac1.m, @ar2
8180 19 5D          lrri    ac1.l, @ar2
8181 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8182 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8183 99 72          asr16   ac1                 l       ac0.m, @ar2
8184 19 5C          lrri    ac0.l, @ar2
8185 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8186 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8187 91 7A          asr16   ac0                 l       ac1.m, @ar2
8188 19 5D          lrri    ac1.l, @ar2
8189 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
818A B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
818B 99 72          asr16   ac1                 l       ac0.m, @ar2
818C 19 5C          lrri    ac0.l, @ar2
818D F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
818E B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
818F 91 7A          asr16   ac0                 l       ac1.m, @ar2
8190 19 5D          lrri    ac1.l, @ar2
8191 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8192 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8193 99 72          asr16   ac1                 l       ac0.m, @ar2
8194 19 5C          lrri    ac0.l, @ar2
8195 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8196 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8197 91 7A          asr16   ac0                 l       ac1.m, @ar2
8198 19 5D          lrri    ac1.l, @ar2
8199 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
819A B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
819B 99 72          asr16   ac1                 l       ac0.m, @ar2
819C 19 5C          lrri    ac0.l, @ar2
819D F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
819E B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
819F 91 7A          asr16   ac0                 l       ac1.m, @ar2
81A0 19 5D          lrri    ac1.l, @ar2
81A1 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
81A2 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
81A3 99 72          asr16   ac1                 l       ac0.m, @ar2
81A4 19 5C          lrri    ac0.l, @ar2
81A5 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
81A6 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
81A7 91 7A          asr16   ac0                 l       ac1.m, @ar2
81A8 19 5D          lrri    ac1.l, @ar2
81A9 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
81AA B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
81AB 99 72          asr16   ac1                 l       ac0.m, @ar2
81AC 19 5C          lrri    ac0.l, @ar2
81AD F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
81AE B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
81AF 91 7A          asr16   ac0                 l       ac1.m, @ar2
81B0 19 5D          lrri    ac1.l, @ar2
81B1 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
81B2 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
81B3 99 72          asr16   ac1                 l       ac0.m, @ar2
81B4 19 5C          lrri    ac0.l, @ar2
81B5 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
81B6 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
81B7 91 7A          asr16   ac0                 l       ac1.m, @ar2
81B8 19 5D          lrri    ac1.l, @ar2
81B9 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
81BA B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
81BB 99 72          asr16   ac1                 l       ac0.m, @ar2
81BC 19 5C          lrri    ac0.l, @ar2
81BD F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
81BE B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
81BF 91 7A          asr16   ac0                 l       ac1.m, @ar2
81C0 19 5D          lrri    ac1.l, @ar2
81C1 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
81C2 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
81C3 99 72          asr16   ac1                 l       ac0.m, @ar2
81C4 19 5C          lrri    ac0.l, @ar2
81C5 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
81C6 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
81C7 91 7A          asr16   ac0                 l       ac1.m, @ar2
81C8 19 5D          lrri    ac1.l, @ar2
81C9 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
81CA B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
81CB 99 72          asr16   ac1                 l       ac0.m, @ar2
81CC 19 5C          lrri    ac0.l, @ar2
81CD F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
81CE B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
81CF 91 7A          asr16   ac0                 l       ac1.m, @ar2
81D0 19 5D          lrri    ac1.l, @ar2
81D1 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
81D2 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
81D3 99 72          asr16   ac1                 l       ac0.m, @ar2
81D4 19 5C          lrri    ac0.l, @ar2
81D5 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
81D6 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
81D7 91 7A          asr16   ac0                 l       ac1.m, @ar2
81D8 19 5D          lrri    ac1.l, @ar2
81D9 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
81DA B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
81DB 99 72          asr16   ac1                 l       ac0.m, @ar2
81DC 19 5C          lrri    ac0.l, @ar2
81DD F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
81DE B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
81DF 91 7A          asr16   ac0                 l       ac1.m, @ar2
81E0 19 5D          lrri    ac1.l, @ar2
81E1 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
81E2 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
81E3 99 72          asr16   ac1                 l       ac0.m, @ar2
81E4 19 5C          lrri    ac0.l, @ar2
81E5 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
81E6 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
81E7 91 7A          asr16   ac0                 l       ac1.m, @ar2
81E8 19 5D          lrri    ac1.l, @ar2
81E9 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
81EA B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
81EB 99 72          asr16   ac1                 l       ac0.m, @ar2
81EC 19 5C          lrri    ac0.l, @ar2
81ED F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
81EE B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
81EF 91 7A          asr16   ac0                 l       ac1.m, @ar2
81F0 19 5D          lrri    ac1.l, @ar2
81F1 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
81F2 1B 7C          srri    @ar3, ac0.l
81F3 6E 00          movp    ac0                         
81F4 B5 1E          mulxac  ax0.h, ax1.l, ac1   mv      ax1.h, ac0.m
81F5 99 09          asr16   ac1                 ir      ar1
81F6 1B 7F          srri    @ar3, ac1.m
81F7 81 2B          clr     ac0                 s       @ar3, ac1.l
81F8 02 DF          ret     
```

## 

```
81F9 81 50          clr     ac0                 l       ax1.l, @ar0
81FA 89 49          clr     ac1                 l       ax0.h, @ar1
81FB B0 72          mulx    ax0.h, ax1.l        l       ac0.m, @ar2
81FC 89 62          clr     ac1                 l       ac0.l, @ar2
81FD F0 7A          lsl16   ac0                 l       ac1.m, @ar2
81FE 19 1A          lrri    ax1.l, @ar0
81FF B4 6A          mulxac  ax0.h, ax1.l, ac0   l       ac1.l, @ar2
8200 91 00          asr16   ac0                         
8201 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8202 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8203 99 72          asr16   ac1                 l       ac0.m, @ar2
8204 19 5C          lrri    ac0.l, @ar2
8205 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8206 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8207 91 7A          asr16   ac0                 l       ac1.m, @ar2
8208 19 5D          lrri    ac1.l, @ar2
8209 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
820A B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
820B 99 72          asr16   ac1                 l       ac0.m, @ar2
820C 19 5C          lrri    ac0.l, @ar2
820D F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
820E B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
820F 91 7A          asr16   ac0                 l       ac1.m, @ar2
8210 19 5D          lrri    ac1.l, @ar2
8211 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8212 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8213 99 72          asr16   ac1                 l       ac0.m, @ar2
8214 19 5C          lrri    ac0.l, @ar2
8215 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8216 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8217 91 7A          asr16   ac0                 l       ac1.m, @ar2
8218 19 5D          lrri    ac1.l, @ar2
8219 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
821A B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
821B 99 72          asr16   ac1                 l       ac0.m, @ar2
821C 19 5C          lrri    ac0.l, @ar2
821D F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
821E B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
821F 91 7A          asr16   ac0                 l       ac1.m, @ar2
8220 19 5D          lrri    ac1.l, @ar2
8221 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8222 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8223 99 72          asr16   ac1                 l       ac0.m, @ar2
8224 19 5C          lrri    ac0.l, @ar2
8225 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8226 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8227 91 7A          asr16   ac0                 l       ac1.m, @ar2
8228 19 5D          lrri    ac1.l, @ar2
8229 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
822A B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
822B 99 72          asr16   ac1                 l       ac0.m, @ar2
822C 19 5C          lrri    ac0.l, @ar2
822D F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
822E B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
822F 91 7A          asr16   ac0                 l       ac1.m, @ar2
8230 19 5D          lrri    ac1.l, @ar2
8231 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8232 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8233 99 72          asr16   ac1                 l       ac0.m, @ar2
8234 19 5C          lrri    ac0.l, @ar2
8235 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8236 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8237 91 7A          asr16   ac0                 l       ac1.m, @ar2
8238 19 5D          lrri    ac1.l, @ar2
8239 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
823A B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
823B 99 72          asr16   ac1                 l       ac0.m, @ar2
823C 19 5C          lrri    ac0.l, @ar2
823D F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
823E B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
823F 91 7A          asr16   ac0                 l       ac1.m, @ar2
8240 19 5D          lrri    ac1.l, @ar2
8241 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8242 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8243 99 72          asr16   ac1                 l       ac0.m, @ar2
8244 19 5C          lrri    ac0.l, @ar2
8245 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8246 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8247 91 7A          asr16   ac0                 l       ac1.m, @ar2
8248 19 5D          lrri    ac1.l, @ar2
8249 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
824A B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
824B 99 72          asr16   ac1                 l       ac0.m, @ar2
824C 19 5C          lrri    ac0.l, @ar2
824D F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
824E B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
824F 91 7A          asr16   ac0                 l       ac1.m, @ar2
8250 19 5D          lrri    ac1.l, @ar2
8251 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8252 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8253 99 72          asr16   ac1                 l       ac0.m, @ar2
8254 19 5C          lrri    ac0.l, @ar2
8255 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8256 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8257 91 7A          asr16   ac0                 l       ac1.m, @ar2
8258 19 5D          lrri    ac1.l, @ar2
8259 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
825A B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
825B 99 72          asr16   ac1                 l       ac0.m, @ar2
825C 19 5C          lrri    ac0.l, @ar2
825D F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
825E B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
825F 91 7A          asr16   ac0                 l       ac1.m, @ar2
8260 19 5D          lrri    ac1.l, @ar2
8261 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8262 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8263 99 72          asr16   ac1                 l       ac0.m, @ar2
8264 19 5C          lrri    ac0.l, @ar2
8265 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8266 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8267 91 7A          asr16   ac0                 l       ac1.m, @ar2
8268 19 5D          lrri    ac1.l, @ar2
8269 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
826A B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
826B 99 72          asr16   ac1                 l       ac0.m, @ar2
826C 19 5C          lrri    ac0.l, @ar2
826D F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
826E B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
826F 91 7A          asr16   ac0                 l       ac1.m, @ar2
8270 19 5D          lrri    ac1.l, @ar2
8271 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8272 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8273 99 72          asr16   ac1                 l       ac0.m, @ar2
8274 19 5C          lrri    ac0.l, @ar2
8275 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8276 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8277 91 7A          asr16   ac0                 l       ac1.m, @ar2
8278 19 5D          lrri    ac1.l, @ar2
8279 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
827A 1B 7C          srri    @ar3, ac0.l
827B 6E 00          movp    ac0                         
827C B5 12          mulxac  ax0.h, ax1.l, ac1   mv      ax0.l, ac0.m
827D 99 09          asr16   ac1                 ir      ar1
827E 1B 7F          srri    @ar3, ac1.m
827F 81 2B          clr     ac0                 s       @ar3, ac1.l
8280 1F 63          mrr     ax1.h, ar3
8281 02 DF          ret     
```


```
8282 1C E3          mrr     ix3, ar3
8283 81 00          clr     ac0                         
8284 89 71          clr     ac1                 l       ac0.m, @ar1
8285 18 BF          lrrd    ac1.m, @ar1
8286 1B 7E          srri    @ar3, ac0.m
8287 4C 00          add     ac0, ac1                    
8288 1B 7E          srri    @ar3, ac0.m
8289 4C 00          add     ac0, ac1                    
828A 1B 7E          srri    @ar3, ac0.m
828B 4C 00          add     ac0, ac1                    
828C 1B 7E          srri    @ar3, ac0.m
828D 4C 00          add     ac0, ac1                    
828E 1B 7E          srri    @ar3, ac0.m
828F 4C 00          add     ac0, ac1                    
8290 1B 7E          srri    @ar3, ac0.m
8291 4C 00          add     ac0, ac1                    
8292 1B 7E          srri    @ar3, ac0.m
8293 4C 00          add     ac0, ac1                    
8294 1B 7E          srri    @ar3, ac0.m
8295 4C 00          add     ac0, ac1                    
8296 1B 7E          srri    @ar3, ac0.m
8297 4C 00          add     ac0, ac1                    
8298 1B 7E          srri    @ar3, ac0.m
8299 4C 00          add     ac0, ac1                    
829A 1B 7E          srri    @ar3, ac0.m
829B 4C 00          add     ac0, ac1                    
829C 1B 7E          srri    @ar3, ac0.m
829D 4C 00          add     ac0, ac1                    
829E 1B 7E          srri    @ar3, ac0.m
829F 4C 00          add     ac0, ac1                    
82A0 1B 7E          srri    @ar3, ac0.m
82A1 4C 00          add     ac0, ac1                    
82A2 1B 7E          srri    @ar3, ac0.m
82A3 4C 00          add     ac0, ac1                    
82A4 1B 7E          srri    @ar3, ac0.m
82A5 4C 00          add     ac0, ac1                    
82A6 1B 7E          srri    @ar3, ac0.m
82A7 4C 00          add     ac0, ac1                    
82A8 1B 7E          srri    @ar3, ac0.m
82A9 4C 00          add     ac0, ac1                    
82AA 1B 7E          srri    @ar3, ac0.m
82AB 4C 00          add     ac0, ac1                    
82AC 1B 7E          srri    @ar3, ac0.m
82AD 4C 00          add     ac0, ac1                    
82AE 1B 7E          srri    @ar3, ac0.m
82AF 4C 00          add     ac0, ac1                    
82B0 1B 7E          srri    @ar3, ac0.m
82B1 4C 00          add     ac0, ac1                    
82B2 1B 7E          srri    @ar3, ac0.m
82B3 4C 00          add     ac0, ac1                    
82B4 1B 7E          srri    @ar3, ac0.m
82B5 4C 00          add     ac0, ac1                    
82B6 1B 7E          srri    @ar3, ac0.m
82B7 4C 00          add     ac0, ac1                    
82B8 1B 7E          srri    @ar3, ac0.m
82B9 4C 00          add     ac0, ac1                    
82BA 1B 7E          srri    @ar3, ac0.m
82BB 4C 00          add     ac0, ac1                    
82BC 1B 7E          srri    @ar3, ac0.m
82BD 4C 00          add     ac0, ac1                    
82BE 1B 7E          srri    @ar3, ac0.m
82BF 4C 00          add     ac0, ac1                    
82C0 1B 7E          srri    @ar3, ac0.m
82C1 4C 00          add     ac0, ac1                    
82C2 1B 7E          srri    @ar3, ac0.m
82C3 4C 00          add     ac0, ac1                    
82C4 1B 7E          srri    @ar3, ac0.m
82C5 4C 00          add     ac0, ac1                    
82C6 89 31          clr     ac1                 s       @ar1, ac0.m
82C7 81 09          clr     ac0                 ir      ar1
82C8 19 3E          lrri    ac0.m, @ar1
82C9 18 BF          lrrd    ac1.m, @ar1
82CA 1B 7E          srri    @ar3, ac0.m
82CB 4C 00          add     ac0, ac1                    
82CC 1B 7E          srri    @ar3, ac0.m
82CD 4C 00          add     ac0, ac1                    
82CE 1B 7E          srri    @ar3, ac0.m
82CF 4C 00          add     ac0, ac1                    
82D0 1B 7E          srri    @ar3, ac0.m
82D1 4C 00          add     ac0, ac1                    
82D2 1B 7E          srri    @ar3, ac0.m
82D3 4C 00          add     ac0, ac1                    
82D4 1B 7E          srri    @ar3, ac0.m
82D5 4C 00          add     ac0, ac1                    
82D6 1B 7E          srri    @ar3, ac0.m
82D7 4C 00          add     ac0, ac1                    
82D8 1B 7E          srri    @ar3, ac0.m
82D9 4C 00          add     ac0, ac1                    
82DA 1B 7E          srri    @ar3, ac0.m
82DB 4C 00          add     ac0, ac1                    
82DC 1B 7E          srri    @ar3, ac0.m
82DD 4C 00          add     ac0, ac1                    
82DE 1B 7E          srri    @ar3, ac0.m
82DF 4C 00          add     ac0, ac1                    
82E0 1B 7E          srri    @ar3, ac0.m
82E1 4C 00          add     ac0, ac1                    
82E2 1B 7E          srri    @ar3, ac0.m
82E3 4C 00          add     ac0, ac1                    
82E4 1B 7E          srri    @ar3, ac0.m
82E5 4C 00          add     ac0, ac1                    
82E6 1B 7E          srri    @ar3, ac0.m
82E7 4C 00          add     ac0, ac1                    
82E8 1B 7E          srri    @ar3, ac0.m
82E9 4C 00          add     ac0, ac1                    
82EA 1B 7E          srri    @ar3, ac0.m
82EB 4C 00          add     ac0, ac1                    
82EC 1B 7E          srri    @ar3, ac0.m
82ED 4C 00          add     ac0, ac1                    
82EE 1B 7E          srri    @ar3, ac0.m
82EF 4C 00          add     ac0, ac1                    
82F0 1B 7E          srri    @ar3, ac0.m
82F1 4C 00          add     ac0, ac1                    
82F2 1B 7E          srri    @ar3, ac0.m
82F3 4C 00          add     ac0, ac1                    
82F4 1B 7E          srri    @ar3, ac0.m
82F5 4C 00          add     ac0, ac1                    
82F6 1B 7E          srri    @ar3, ac0.m
82F7 4C 00          add     ac0, ac1                    
82F8 1B 7E          srri    @ar3, ac0.m
82F9 4C 00          add     ac0, ac1                    
82FA 1B 7E          srri    @ar3, ac0.m
82FB 4C 00          add     ac0, ac1                    
82FC 1B 7E          srri    @ar3, ac0.m
82FD 4C 00          add     ac0, ac1                    
82FE 1B 7E          srri    @ar3, ac0.m
82FF 4C 00          add     ac0, ac1                    
8300 1B 7E          srri    @ar3, ac0.m
8301 4C 00          add     ac0, ac1                    
8302 1B 7E          srri    @ar3, ac0.m
8303 4C 00          add     ac0, ac1                    
8304 1B 7E          srri    @ar3, ac0.m
8305 4C 00          add     ac0, ac1                    
8306 1B 7E          srri    @ar3, ac0.m
8307 4C 00          add     ac0, ac1                    
8308 1B 7E          srri    @ar3, ac0.m
8309 4C 00          add     ac0, ac1                    
830A 1B 3E          srri    @ar1, ac0.m
830B 1C 27          mrr     ar1, ix3
830C 1C 62          mrr     ar3, ar2
830D 81 50          clr     ac0                 l       ax1.l, @ar0
830E 89 49          clr     ac1                 l       ax0.h, @ar1
830F B0 72          mulx    ax0.h, ax1.l        l       ac0.m, @ar2
8310 89 62          clr     ac1                 l       ac0.l, @ar2
8311 F0 7A          lsl16   ac0                 l       ac1.m, @ar2
8312 19 1A          lrri    ax1.l, @ar0
8313 19 39          lrri    ax0.h, @ar1
8314 B4 6A          mulxac  ax0.h, ax1.l, ac0   l       ac1.l, @ar2
8315 91 00          asr16   ac0                         
8316 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8317 19 39          lrri    ax0.h, @ar1
8318 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8319 99 72          asr16   ac1                 l       ac0.m, @ar2
831A 19 5C          lrri    ac0.l, @ar2
831B F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
831C 19 39          lrri    ax0.h, @ar1
831D B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
831E 91 7A          asr16   ac0                 l       ac1.m, @ar2
831F 19 5D          lrri    ac1.l, @ar2
8320 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8321 19 39          lrri    ax0.h, @ar1
8322 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8323 99 72          asr16   ac1                 l       ac0.m, @ar2
8324 19 5C          lrri    ac0.l, @ar2
8325 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8326 19 39          lrri    ax0.h, @ar1
8327 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8328 91 7A          asr16   ac0                 l       ac1.m, @ar2
8329 19 5D          lrri    ac1.l, @ar2
832A F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
832B 19 39          lrri    ax0.h, @ar1
832C B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
832D 99 72          asr16   ac1                 l       ac0.m, @ar2
832E 19 5C          lrri    ac0.l, @ar2
832F F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8330 19 39          lrri    ax0.h, @ar1
8331 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8332 91 7A          asr16   ac0                 l       ac1.m, @ar2
8333 19 5D          lrri    ac1.l, @ar2
8334 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8335 19 39          lrri    ax0.h, @ar1
8336 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8337 99 72          asr16   ac1                 l       ac0.m, @ar2
8338 19 5C          lrri    ac0.l, @ar2
8339 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
833A 19 39          lrri    ax0.h, @ar1
833B B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
833C 91 7A          asr16   ac0                 l       ac1.m, @ar2
833D 19 5D          lrri    ac1.l, @ar2
833E F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
833F 19 39          lrri    ax0.h, @ar1
8340 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8341 99 72          asr16   ac1                 l       ac0.m, @ar2
8342 19 5C          lrri    ac0.l, @ar2
8343 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8344 19 39          lrri    ax0.h, @ar1
8345 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8346 91 7A          asr16   ac0                 l       ac1.m, @ar2
8347 19 5D          lrri    ac1.l, @ar2
8348 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8349 19 39          lrri    ax0.h, @ar1
834A B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
834B 99 72          asr16   ac1                 l       ac0.m, @ar2
834C 19 5C          lrri    ac0.l, @ar2
834D F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
834E 19 39          lrri    ax0.h, @ar1
834F B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8350 91 7A          asr16   ac0                 l       ac1.m, @ar2
8351 19 5D          lrri    ac1.l, @ar2
8352 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8353 19 39          lrri    ax0.h, @ar1
8354 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8355 99 72          asr16   ac1                 l       ac0.m, @ar2
8356 19 5C          lrri    ac0.l, @ar2
8357 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8358 19 39          lrri    ax0.h, @ar1
8359 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
835A 91 7A          asr16   ac0                 l       ac1.m, @ar2
835B 19 5D          lrri    ac1.l, @ar2
835C F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
835D 19 39          lrri    ax0.h, @ar1
835E B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
835F 99 72          asr16   ac1                 l       ac0.m, @ar2
8360 19 5C          lrri    ac0.l, @ar2
8361 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8362 19 39          lrri    ax0.h, @ar1
8363 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8364 91 7A          asr16   ac0                 l       ac1.m, @ar2
8365 19 5D          lrri    ac1.l, @ar2
8366 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8367 19 39          lrri    ax0.h, @ar1
8368 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8369 99 72          asr16   ac1                 l       ac0.m, @ar2
836A 19 5C          lrri    ac0.l, @ar2
836B F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
836C 19 39          lrri    ax0.h, @ar1
836D B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
836E 91 7A          asr16   ac0                 l       ac1.m, @ar2
836F 19 5D          lrri    ac1.l, @ar2
8370 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8371 19 39          lrri    ax0.h, @ar1
8372 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8373 99 72          asr16   ac1                 l       ac0.m, @ar2
8374 19 5C          lrri    ac0.l, @ar2
8375 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8376 19 39          lrri    ax0.h, @ar1
8377 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8378 91 7A          asr16   ac0                 l       ac1.m, @ar2
8379 19 5D          lrri    ac1.l, @ar2
837A F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
837B 19 39          lrri    ax0.h, @ar1
837C B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
837D 99 72          asr16   ac1                 l       ac0.m, @ar2
837E 19 5C          lrri    ac0.l, @ar2
837F F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8380 19 39          lrri    ax0.h, @ar1
8381 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8382 91 7A          asr16   ac0                 l       ac1.m, @ar2
8383 19 5D          lrri    ac1.l, @ar2
8384 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8385 19 39          lrri    ax0.h, @ar1
8386 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8387 99 72          asr16   ac1                 l       ac0.m, @ar2
8388 19 5C          lrri    ac0.l, @ar2
8389 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
838A 19 39          lrri    ax0.h, @ar1
838B B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
838C 91 7A          asr16   ac0                 l       ac1.m, @ar2
838D 19 5D          lrri    ac1.l, @ar2
838E F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
838F 19 39          lrri    ax0.h, @ar1
8390 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8391 99 72          asr16   ac1                 l       ac0.m, @ar2
8392 19 5C          lrri    ac0.l, @ar2
8393 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8394 19 39          lrri    ax0.h, @ar1
8395 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8396 91 7A          asr16   ac0                 l       ac1.m, @ar2
8397 19 5D          lrri    ac1.l, @ar2
8398 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8399 19 39          lrri    ax0.h, @ar1
839A B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
839B 99 72          asr16   ac1                 l       ac0.m, @ar2
839C 19 5C          lrri    ac0.l, @ar2
839D F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
839E 19 39          lrri    ax0.h, @ar1
839F B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
83A0 91 7A          asr16   ac0                 l       ac1.m, @ar2
83A1 19 5D          lrri    ac1.l, @ar2
83A2 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
83A3 19 39          lrri    ax0.h, @ar1
83A4 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
83A5 99 72          asr16   ac1                 l       ac0.m, @ar2
83A6 19 5C          lrri    ac0.l, @ar2
83A7 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
83A8 19 39          lrri    ax0.h, @ar1
83A9 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
83AA 91 7A          asr16   ac0                 l       ac1.m, @ar2
83AB 19 5D          lrri    ac1.l, @ar2
83AC F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
83AD 1B 7C          srri    @ar3, ac0.l
83AE 6E 00          movp    ac0                         
83AF B5 12          mulxac  ax0.h, ax1.l, ac1   mv      ax0.l, ac0.m
83B0 99 00          asr16   ac1                         
83B1 1B 7F          srri    @ar3, ac1.m
83B2 81 2B          clr     ac0                 s       @ar3, ac1.l
83B3 1C 04          mrr     ar0, ix0
83B4 1C 45          mrr     ar2, ix1
83B5 1C 62          mrr     ar3, ar2
83B6 81 50          clr     ac0                 l       ax1.l, @ar0
83B7 89 49          clr     ac1                 l       ax0.h, @ar1
83B8 B0 72          mulx    ax0.h, ax1.l        l       ac0.m, @ar2
83B9 89 62          clr     ac1                 l       ac0.l, @ar2
83BA F0 7A          lsl16   ac0                 l       ac1.m, @ar2
83BB 19 1A          lrri    ax1.l, @ar0
83BC 19 39          lrri    ax0.h, @ar1
83BD B4 6A          mulxac  ax0.h, ax1.l, ac0   l       ac1.l, @ar2
83BE 91 00          asr16   ac0                         
83BF F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
83C0 19 39          lrri    ax0.h, @ar1
83C1 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
83C2 99 72          asr16   ac1                 l       ac0.m, @ar2
83C3 19 5C          lrri    ac0.l, @ar2
83C4 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
83C5 19 39          lrri    ax0.h, @ar1
83C6 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
83C7 91 7A          asr16   ac0                 l       ac1.m, @ar2
83C8 19 5D          lrri    ac1.l, @ar2
83C9 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
83CA 19 39          lrri    ax0.h, @ar1
83CB B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
83CC 99 72          asr16   ac1                 l       ac0.m, @ar2
83CD 19 5C          lrri    ac0.l, @ar2
83CE F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
83CF 19 39          lrri    ax0.h, @ar1
83D0 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
83D1 91 7A          asr16   ac0                 l       ac1.m, @ar2
83D2 19 5D          lrri    ac1.l, @ar2
83D3 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
83D4 19 39          lrri    ax0.h, @ar1
83D5 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
83D6 99 72          asr16   ac1                 l       ac0.m, @ar2
83D7 19 5C          lrri    ac0.l, @ar2
83D8 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
83D9 19 39          lrri    ax0.h, @ar1
83DA B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
83DB 91 7A          asr16   ac0                 l       ac1.m, @ar2
83DC 19 5D          lrri    ac1.l, @ar2
83DD F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
83DE 19 39          lrri    ax0.h, @ar1
83DF B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
83E0 99 72          asr16   ac1                 l       ac0.m, @ar2
83E1 19 5C          lrri    ac0.l, @ar2
83E2 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
83E3 19 39          lrri    ax0.h, @ar1
83E4 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
83E5 91 7A          asr16   ac0                 l       ac1.m, @ar2
83E6 19 5D          lrri    ac1.l, @ar2
83E7 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
83E8 19 39          lrri    ax0.h, @ar1
83E9 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
83EA 99 72          asr16   ac1                 l       ac0.m, @ar2
83EB 19 5C          lrri    ac0.l, @ar2
83EC F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
83ED 19 39          lrri    ax0.h, @ar1
83EE B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
83EF 91 7A          asr16   ac0                 l       ac1.m, @ar2
83F0 19 5D          lrri    ac1.l, @ar2
83F1 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
83F2 19 39          lrri    ax0.h, @ar1
83F3 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
83F4 99 72          asr16   ac1                 l       ac0.m, @ar2
83F5 19 5C          lrri    ac0.l, @ar2
83F6 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
83F7 19 39          lrri    ax0.h, @ar1
83F8 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
83F9 91 7A          asr16   ac0                 l       ac1.m, @ar2
83FA 19 5D          lrri    ac1.l, @ar2
83FB F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
83FC 19 39          lrri    ax0.h, @ar1
83FD B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
83FE 99 72          asr16   ac1                 l       ac0.m, @ar2
83FF 19 5C          lrri    ac0.l, @ar2
8400 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8401 19 39          lrri    ax0.h, @ar1
8402 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8403 91 7A          asr16   ac0                 l       ac1.m, @ar2
8404 19 5D          lrri    ac1.l, @ar2
8405 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8406 19 39          lrri    ax0.h, @ar1
8407 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8408 99 72          asr16   ac1                 l       ac0.m, @ar2
8409 19 5C          lrri    ac0.l, @ar2
840A F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
840B 19 39          lrri    ax0.h, @ar1
840C B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
840D 91 7A          asr16   ac0                 l       ac1.m, @ar2
840E 19 5D          lrri    ac1.l, @ar2
840F F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8410 19 39          lrri    ax0.h, @ar1
8411 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8412 99 72          asr16   ac1                 l       ac0.m, @ar2
8413 19 5C          lrri    ac0.l, @ar2
8414 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8415 19 39          lrri    ax0.h, @ar1
8416 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8417 91 7A          asr16   ac0                 l       ac1.m, @ar2
8418 19 5D          lrri    ac1.l, @ar2
8419 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
841A 19 39          lrri    ax0.h, @ar1
841B B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
841C 99 72          asr16   ac1                 l       ac0.m, @ar2
841D 19 5C          lrri    ac0.l, @ar2
841E F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
841F 19 39          lrri    ax0.h, @ar1
8420 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8421 91 7A          asr16   ac0                 l       ac1.m, @ar2
8422 19 5D          lrri    ac1.l, @ar2
8423 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8424 19 39          lrri    ax0.h, @ar1
8425 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8426 99 72          asr16   ac1                 l       ac0.m, @ar2
8427 19 5C          lrri    ac0.l, @ar2
8428 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8429 19 39          lrri    ax0.h, @ar1
842A B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
842B 91 7A          asr16   ac0                 l       ac1.m, @ar2
842C 19 5D          lrri    ac1.l, @ar2
842D F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
842E 19 39          lrri    ax0.h, @ar1
842F B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8430 99 72          asr16   ac1                 l       ac0.m, @ar2
8431 19 5C          lrri    ac0.l, @ar2
8432 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8433 19 39          lrri    ax0.h, @ar1
8434 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8435 91 7A          asr16   ac0                 l       ac1.m, @ar2
8436 19 5D          lrri    ac1.l, @ar2
8437 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8438 19 39          lrri    ax0.h, @ar1
8439 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
843A 99 72          asr16   ac1                 l       ac0.m, @ar2
843B 19 5C          lrri    ac0.l, @ar2
843C F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
843D 19 39          lrri    ax0.h, @ar1
843E B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
843F 91 7A          asr16   ac0                 l       ac1.m, @ar2
8440 19 5D          lrri    ac1.l, @ar2
8441 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8442 19 39          lrri    ax0.h, @ar1
8443 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8444 99 72          asr16   ac1                 l       ac0.m, @ar2
8445 19 5C          lrri    ac0.l, @ar2
8446 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8447 19 39          lrri    ax0.h, @ar1
8448 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8449 91 7A          asr16   ac0                 l       ac1.m, @ar2
844A 19 5D          lrri    ac1.l, @ar2
844B F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
844C 19 39          lrri    ax0.h, @ar1
844D B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
844E 99 72          asr16   ac1                 l       ac0.m, @ar2
844F 19 5C          lrri    ac0.l, @ar2
8450 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8451 19 39          lrri    ax0.h, @ar1
8452 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8453 91 7A          asr16   ac0                 l       ac1.m, @ar2
8454 19 5D          lrri    ac1.l, @ar2
8455 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8456 1B 7C          srri    @ar3, ac0.l
8457 6E 00          movp    ac0                         
8458 B5 1E          mulxac  ax0.h, ax1.l, ac1   mv      ax1.h, ac0.m
8459 99 00          asr16   ac1                         
845A 1B 7F          srri    @ar3, ac1.m
845B 81 2B          clr     ac0                 s       @ar3, ac1.l
845C 02 DF          ret     
```

##


```
845D 1C E3          mrr     ix3, ar3
845E 81 00          clr     ac0                         
845F 89 71          clr     ac1                 l       ac0.m, @ar1
8460 18 BF          lrrd    ac1.m, @ar1
8461 1B 7E          srri    @ar3, ac0.m
8462 4C 00          add     ac0, ac1                    
8463 1B 7E          srri    @ar3, ac0.m
8464 4C 00          add     ac0, ac1                    
8465 1B 7E          srri    @ar3, ac0.m
8466 4C 00          add     ac0, ac1                    
8467 1B 7E          srri    @ar3, ac0.m
8468 4C 00          add     ac0, ac1                    
8469 1B 7E          srri    @ar3, ac0.m
846A 4C 00          add     ac0, ac1                    
846B 1B 7E          srri    @ar3, ac0.m
846C 4C 00          add     ac0, ac1                    
846D 1B 7E          srri    @ar3, ac0.m
846E 4C 00          add     ac0, ac1                    
846F 1B 7E          srri    @ar3, ac0.m
8470 4C 00          add     ac0, ac1                    
8471 1B 7E          srri    @ar3, ac0.m
8472 4C 00          add     ac0, ac1                    
8473 1B 7E          srri    @ar3, ac0.m
8474 4C 00          add     ac0, ac1                    
8475 1B 7E          srri    @ar3, ac0.m
8476 4C 00          add     ac0, ac1                    
8477 1B 7E          srri    @ar3, ac0.m
8478 4C 00          add     ac0, ac1                    
8479 1B 7E          srri    @ar3, ac0.m
847A 4C 00          add     ac0, ac1                    
847B 1B 7E          srri    @ar3, ac0.m
847C 4C 00          add     ac0, ac1                    
847D 1B 7E          srri    @ar3, ac0.m
847E 4C 00          add     ac0, ac1                    
847F 1B 7E          srri    @ar3, ac0.m
8480 4C 00          add     ac0, ac1                    
8481 1B 7E          srri    @ar3, ac0.m
8482 4C 00          add     ac0, ac1                    
8483 1B 7E          srri    @ar3, ac0.m
8484 4C 00          add     ac0, ac1                    
8485 1B 7E          srri    @ar3, ac0.m
8486 4C 00          add     ac0, ac1                    
8487 1B 7E          srri    @ar3, ac0.m
8488 4C 00          add     ac0, ac1                    
8489 1B 7E          srri    @ar3, ac0.m
848A 4C 00          add     ac0, ac1                    
848B 1B 7E          srri    @ar3, ac0.m
848C 4C 00          add     ac0, ac1                    
848D 1B 7E          srri    @ar3, ac0.m
848E 4C 00          add     ac0, ac1                    
848F 1B 7E          srri    @ar3, ac0.m
8490 4C 00          add     ac0, ac1                    
8491 1B 7E          srri    @ar3, ac0.m
8492 4C 00          add     ac0, ac1                    
8493 1B 7E          srri    @ar3, ac0.m
8494 4C 00          add     ac0, ac1                    
8495 1B 7E          srri    @ar3, ac0.m
8496 4C 00          add     ac0, ac1                    
8497 1B 7E          srri    @ar3, ac0.m
8498 4C 00          add     ac0, ac1                    
8499 1B 7E          srri    @ar3, ac0.m
849A 4C 00          add     ac0, ac1                    
849B 1B 7E          srri    @ar3, ac0.m
849C 4C 00          add     ac0, ac1                    
849D 1B 7E          srri    @ar3, ac0.m
849E 4C 00          add     ac0, ac1                    
849F 1B 7E          srri    @ar3, ac0.m
84A0 4C 00          add     ac0, ac1                    
84A1 89 31          clr     ac1                 s       @ar1, ac0.m
84A2 1C 27          mrr     ar1, ix3
84A3 1C 62          mrr     ar3, ar2
84A4 81 50          clr     ac0                 l       ax1.l, @ar0
84A5 19 39          lrri    ax0.h, @ar1
84A6 B0 72          mulx    ax0.h, ax1.l        l       ac0.m, @ar2
84A7 89 62          clr     ac1                 l       ac0.l, @ar2
84A8 F0 7A          lsl16   ac0                 l       ac1.m, @ar2
84A9 19 1A          lrri    ax1.l, @ar0
84AA 19 39          lrri    ax0.h, @ar1
84AB B4 6A          mulxac  ax0.h, ax1.l, ac0   l       ac1.l, @ar2
84AC 91 00          asr16   ac0                         
84AD F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
84AE 19 39          lrri    ax0.h, @ar1
84AF B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
84B0 99 72          asr16   ac1                 l       ac0.m, @ar2
84B1 19 5C          lrri    ac0.l, @ar2
84B2 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
84B3 19 39          lrri    ax0.h, @ar1
84B4 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
84B5 91 7A          asr16   ac0                 l       ac1.m, @ar2
84B6 19 5D          lrri    ac1.l, @ar2
84B7 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
84B8 19 39          lrri    ax0.h, @ar1
84B9 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
84BA 99 72          asr16   ac1                 l       ac0.m, @ar2
84BB 19 5C          lrri    ac0.l, @ar2
84BC F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
84BD 19 39          lrri    ax0.h, @ar1
84BE B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
84BF 91 7A          asr16   ac0                 l       ac1.m, @ar2
84C0 19 5D          lrri    ac1.l, @ar2
84C1 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
84C2 19 39          lrri    ax0.h, @ar1
84C3 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
84C4 99 72          asr16   ac1                 l       ac0.m, @ar2
84C5 19 5C          lrri    ac0.l, @ar2
84C6 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
84C7 19 39          lrri    ax0.h, @ar1
84C8 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
84C9 91 7A          asr16   ac0                 l       ac1.m, @ar2
84CA 19 5D          lrri    ac1.l, @ar2
84CB F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
84CC 19 39          lrri    ax0.h, @ar1
84CD B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
84CE 99 72          asr16   ac1                 l       ac0.m, @ar2
84CF 19 5C          lrri    ac0.l, @ar2
84D0 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
84D1 19 39          lrri    ax0.h, @ar1
84D2 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
84D3 91 7A          asr16   ac0                 l       ac1.m, @ar2
84D4 19 5D          lrri    ac1.l, @ar2
84D5 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
84D6 19 39          lrri    ax0.h, @ar1
84D7 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
84D8 99 72          asr16   ac1                 l       ac0.m, @ar2
84D9 19 5C          lrri    ac0.l, @ar2
84DA F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
84DB 19 39          lrri    ax0.h, @ar1
84DC B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
84DD 91 7A          asr16   ac0                 l       ac1.m, @ar2
84DE 19 5D          lrri    ac1.l, @ar2
84DF F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
84E0 19 39          lrri    ax0.h, @ar1
84E1 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
84E2 99 72          asr16   ac1                 l       ac0.m, @ar2
84E3 19 5C          lrri    ac0.l, @ar2
84E4 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
84E5 19 39          lrri    ax0.h, @ar1
84E6 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
84E7 91 7A          asr16   ac0                 l       ac1.m, @ar2
84E8 19 5D          lrri    ac1.l, @ar2
84E9 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
84EA 19 39          lrri    ax0.h, @ar1
84EB B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
84EC 99 72          asr16   ac1                 l       ac0.m, @ar2
84ED 19 5C          lrri    ac0.l, @ar2
84EE F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
84EF 19 39          lrri    ax0.h, @ar1
84F0 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
84F1 91 7A          asr16   ac0                 l       ac1.m, @ar2
84F2 19 5D          lrri    ac1.l, @ar2
84F3 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
84F4 19 39          lrri    ax0.h, @ar1
84F5 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
84F6 99 72          asr16   ac1                 l       ac0.m, @ar2
84F7 19 5C          lrri    ac0.l, @ar2
84F8 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
84F9 19 39          lrri    ax0.h, @ar1
84FA B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
84FB 91 7A          asr16   ac0                 l       ac1.m, @ar2
84FC 19 5D          lrri    ac1.l, @ar2
84FD F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
84FE 19 39          lrri    ax0.h, @ar1
84FF B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8500 99 72          asr16   ac1                 l       ac0.m, @ar2
8501 19 5C          lrri    ac0.l, @ar2
8502 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8503 19 39          lrri    ax0.h, @ar1
8504 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8505 91 7A          asr16   ac0                 l       ac1.m, @ar2
8506 19 5D          lrri    ac1.l, @ar2
8507 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8508 19 39          lrri    ax0.h, @ar1
8509 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
850A 99 72          asr16   ac1                 l       ac0.m, @ar2
850B 19 5C          lrri    ac0.l, @ar2
850C F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
850D 19 39          lrri    ax0.h, @ar1
850E B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
850F 91 7A          asr16   ac0                 l       ac1.m, @ar2
8510 19 5D          lrri    ac1.l, @ar2
8511 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8512 19 39          lrri    ax0.h, @ar1
8513 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8514 99 72          asr16   ac1                 l       ac0.m, @ar2
8515 19 5C          lrri    ac0.l, @ar2
8516 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8517 19 39          lrri    ax0.h, @ar1
8518 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8519 91 7A          asr16   ac0                 l       ac1.m, @ar2
851A 19 5D          lrri    ac1.l, @ar2
851B F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
851C 19 39          lrri    ax0.h, @ar1
851D B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
851E 99 72          asr16   ac1                 l       ac0.m, @ar2
851F 19 5C          lrri    ac0.l, @ar2
8520 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8521 19 39          lrri    ax0.h, @ar1
8522 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8523 91 7A          asr16   ac0                 l       ac1.m, @ar2
8524 19 5D          lrri    ac1.l, @ar2
8525 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8526 19 39          lrri    ax0.h, @ar1
8527 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8528 99 72          asr16   ac1                 l       ac0.m, @ar2
8529 19 5C          lrri    ac0.l, @ar2
852A F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
852B 19 39          lrri    ax0.h, @ar1
852C B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
852D 91 7A          asr16   ac0                 l       ac1.m, @ar2
852E 19 5D          lrri    ac1.l, @ar2
852F F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8530 19 39          lrri    ax0.h, @ar1
8531 B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
8532 99 72          asr16   ac1                 l       ac0.m, @ar2
8533 19 5C          lrri    ac0.l, @ar2
8534 F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
8535 19 39          lrri    ax0.h, @ar1
8536 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8537 91 7A          asr16   ac0                 l       ac1.m, @ar2
8538 19 5D          lrri    ac1.l, @ar2
8539 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
853A 19 39          lrri    ax0.h, @ar1
853B B5 23          mulxac  ax0.h, ax1.l, ac1   s       @ar3, ac0.l
853C 99 72          asr16   ac1                 l       ac0.m, @ar2
853D 19 5C          lrri    ac0.l, @ar2
853E F0 A1          lsl16   ac0                 ls      ax1.l, ac1.m
853F 19 39          lrri    ax0.h, @ar1
8540 B4 2B          mulxac  ax0.h, ax1.l, ac0   s       @ar3, ac1.l
8541 91 7A          asr16   ac0                 l       ac1.m, @ar2
8542 19 5D          lrri    ac1.l, @ar2
8543 F1 A0          lsl16   ac1                 ls      ax1.l, ac0.m
8544 1B 7C          srri    @ar3, ac0.l
8545 6E 00          movp    ac0                         
8546 B5 12          mulxac  ax0.h, ax1.l, ac1   mv      ax0.l, ac0.m
8547 99 00          asr16   ac1                         
8548 1B 7F          srri    @ar3, ac1.m
8549 81 2B          clr     ac0                 s       @ar3, ac1.l
854A 02 DF          ret     
```

// -----------------------------------------------------------------------------------------------------

## Accelerator

```
854B 8E 00          clr40                               
854C 00 80 08 00    lri     ar0, #0x0800
854E 00 92 00 FF    lri     config, #0x00FF
8550 00 C4 04 03    lr      ix0, $0x0403
8552 1F E4          mrr     ac1.m, ix0
8553 05 03          addis   ac1.m, 3
8554 15 6E          lsr     ac1, #0x2E
8555 15 02          lsl     ac1, #0x02
8556 29 C9          srs     $(DSCR), ax0.h
8557 00 DE 04 00    lr      ac0.m, $0x0400
8559 2E CE          srs     $(DSMAH), ac0.m
855A 00 DE 04 01    lr      ac0.m, $0x0401
855C 2E CF          srs     $(DSMAL), ac0.m
855D 00 E0 FF CD    sr      $(DSPA), ar0
855F 2D CB          srs     $(DSBL), ac1.l
8560 02 BF 86 3D    call    $0x863D                             // WaitDspDma
8562 29 D1          srs     $(ACFMT), ax0.h
8563 29 D4          srs     $(ACSAH), ax0.h
8564 29 D5          srs     $(ACSAL), ax0.h
8565 16 D6 01 FF    si      $(ACEAH), #0x01FF
8567 16 D7 FF FF    si      $(ACEAL), #0xFFFF
8569 00 DF 04 04    lr      ac1.m, $0x0404
856B 00 DD 04 05    lr      ac1.l, $0x0405
856D 15 7F          lsr     ac1, #0x3F
856E 03 60 80 00    ori     ac1.m, #0x8000
8570 2F D8          srs     $(ACCAH), ac1.m
8571 2D D9          srs     $(ACCAL), ac1.l
8572 00 82 FF D3    lri     ar2, #0xFFD3
8574 00 86 00 00    lri     ix2, #0x0000
8576 1F E4          mrr     ac1.m, ix0
8577 03 C0 00 01    tset    ac1.m, #0x0001
8579 15 7F          lsr     ac1, #0x3F
857A 1C BF          mrr     ix1, ac1.m
857B 00 9A FF F8    lri     ax1.l, #0xFFF8
857D 00 9B 00 18    lri     ax1.h, #0x0018
857F 81 78          clr     ac0                 l       ac1.m, @ar0
8580 00 65 85 86    bloop   ix1, $0x8586
8582 35 BE          andr    ac1.m, ax0.h        slnm    ac0.m, ax1.h
8583 37 93          andr    ac1.m, ax1.h        sl      ac1.m, ax0.h
8584 F5 00          lsr16   ac1                         
8585 70 17          addaxl  ac0, ax0.l      mv      ax0.h, ac1.m
8586 72 78          addaxl  ac0, ax1.l      l       ac1.m, @ar0
8587 02 9C 85 8C    jnok    $0x858C
8589 35 BE          andr    ac1.m, ax0.h        slnm    ac0.m, ax1.h
858A 1F 1F          mrr     ax0.l, ac1.m
858B 70 00          addaxl  ac0, ax0.l              
858C 6D 00          mov     ac1, ac0                    
858D 00 80 04 08    lri     ar0, #0x0408
858F 00 9A 12 DF    lri     ax1.l, #0x12DF
8591 00 98 AC BD    lri     ax0.l, #0xACBD
8593 48 00          addax   ac0, ax0                    
8594 1B 1E          srri    @ar0, ac0.m
8595 1B 1C          srri    @ar0, ac0.l
8596 00 9E FB CA    lri     ac0.m, #0xFBCA
8598 1B 1E          srri    @ar0, ac0.m
8599 00 9E DE B0    lri     ac0.m, #0xDEB0
859B 1B 1E          srri    @ar0, ac0.m
859C 00 9E FD E1    lri     ac0.m, #0xFDE1
859E 1B 1E          srri    @ar0, ac0.m
859F 00 9E FA CB    lri     ac0.m, #0xFACB
85A1 1B 1E          srri    @ar0, ac0.m
85A2 00 9E DE AD    lri     ac0.m, #0xDEAD
85A4 1B 1E          srri    @ar0, ac0.m
85A5 00 9E BE EF    lri     ac0.m, #0xBEEF
85A7 08 0D          lris    ax0.l, 13
85A8 71 30          addaxl  ac1, ax0.l      s       @ar0, ac0.m
85A9 1B 1D          srri    @ar0, ac1.l
85AA 1B 11          srri    @ar0, ac1.h
85AB 00 80 08 00    lri     ar0, #0x0800
85AD 00 81 04 09    lri     ar1, #0x0409
85AF 00 82 04 0F    lri     ar2, #0x040F
85B1 00 85 04 10    lri     ix1, #0x0410
85B3 00 86 04 0E    lri     ix2, #0x040E
85B5 00 87 FF FE    lri     ix3, #0xFFFE
85B7 16 D1 00 05    si      $(ACFMT), #0x0005                       // Format 5?
85B9 16 D4 00 00    si      $(ACSAH), #0x0000
85BB 16 D5 00 00    si      $(ACSAL), #0x0000
85BD 16 D6 00 00    si      $(ACEAH), #0x0000
85BF 16 D7 00 FF    si      $(ACEAL), #0x00FF
85C1 16 D8 00 00    si      $(ACCAH), #0x0000
85C3 16 D9 00 00    si      $(ACCAL), #0x0000
85C5 16 DA 00 00    si      $(ACPDS), #0x0000
85C7 16 A0 F9 B8    si      $(ADPCM_A00), #0xF9B8
85C9 16 A1 FE C7    si      $(ADPCM_A10), #0xFEC7
85CB 16 DE 08 00    si      $(ACGAN), #0x0800
85CD 16 DB 00 00    si      $(ACYN1), #0x0000
85CF 16 DC 00 00    si      $(ACYN2), #0x0000
85D1 1F E4          mrr     ac1.m, ix0
85D2 19 18          lrri    ax0.l, @ar0
85D3 00 F8 FF DF    sr      $(UnkHW_FFDF), ax0.l
85D5 1C 65          mrr     ar3, ix1
85D6 18 BC          lrrd    ac0.l, @ar1
85D7 19 3E          lrri    ac0.m, @ar1
85D8 00 D8 FF DD    lr      ax0.l, $(ACDAT)
85DA 70 00          addaxl  ac0, ax0.l              
85DB 1A BC          srrd    @ar1, ac0.l
85DC 79 31          decm    ac1                 s       @ar1, ac0.m
85DD 15 7F          lsr     ac1, #0x3F
85DE 00 7F 85 FD    bloop   ac1.m, $0x85FD
85E0 02 BF 86 11    call    $0x8611
85E2 19 1E          lrri    ac0.m, @ar0
85E3 31 60          xorr    ac1.m, ax0.h        l       ac0.l, @ar0
85E4 14 78          lsr     ac0, #0x38
85E5 00 FC FF DF    sr      $(UnkHW_FFDF), ac0.l
85E7 1C 65          mrr     ar3, ix1
85E8 18 BC          lrrd    ac0.l, @ar1
85E9 33 71          xorr    ac1.m, ax1.h        l       ac0.m, @ar1
85EA 00 D8 FF DD    lr      ax0.l, $(ACDAT)
85EC 70 2A          addaxl  ac0, ax0.l      s       @ar2, ac1.l
85ED 1A 5F          srr     @ar2, ac1.m
85EE 1A BC          srrd    @ar1, ac0.l
85EF 1B 3E          srri    @ar1, ac0.m
85F0 02 BF 86 11    call    $0x8611
85F2 31 40          xorr    ac1.m, ax0.h        l       ax0.l, @ar0
85F3 00 F8 FF DF    sr      $(UnkHW_FFDF), ax0.l
85F5 1C 65          mrr     ar3, ix1
85F6 18 BC          lrrd    ac0.l, @ar1
85F7 33 71          xorr    ac1.m, ax1.h        l       ac0.m, @ar1
85F8 00 D8 FF DD    lr      ax0.l, $(ACDAT)
85FA 70 2A          addaxl  ac0, ax0.l      s       @ar2, ac1.l
85FB 1A 5F          srr     @ar2, ac1.m
85FC 1A BC          srrd    @ar1, ac0.l
85FD 1B 3E          srri    @ar1, ac0.m
85FE 02 9D 86 02    jok     $0x8602
8600 02 BF 86 11    call    $0x8611
8602 16 C9 00 01    si      $(DSCR), #0x0001
8604 00 DE 04 06    lr      ac0.m, $0x0406
8606 2E CE          srs     $(DSMAH), ac0.m
8607 00 DE 04 07    lr      ac0.m, $0x0407
8609 2E CF          srs     $(DSMAL), ac0.m
860A 16 CD 04 0A    si      $(DSPA), #0x040A
860C 16 CB 00 04    si      $(DSBL), #0x0004
860E 02 BF 86 3D    call    $0x863D                             // WaitDspDma
8610 02 DF          ret     
```

## More LFSR?

```
8611 18 DA          lrrd    ax1.l, @ar2
8612 18 DB          lrrd    ax1.h, @ar2
8613 18 DD          lrrd    ac1.l, @ar2
8614 18 DF          lrrd    ac1.m, @ar2
8615 4C 04          add     ac0, ac1            dr      ar0
8616 1F FC          mrr     ac1.m, ac0.l
8617 31 43          xorr    ac1.m, ax0.h        l       ax0.l, @ar3
8618 F5 63          lsr16   ac1                 l       ac0.l, @ar3
8619 1F FE          mrr     ac1.m, ac0.m
861A 76 07          inc     ac0                 dr      ar3
861B 33 23          xorr    ac1.m, ax1.h        s       @ar3, ac0.l
861C 70 42          addaxl  ac0, ax0.l      l       ax0.l, @ar2
861D 14 23          lsl     ac0, #0x23
861E 14 6D          lsr     ac0, #0x2D
861F 1F 5E          mrr     ax1.l, ac0.m
8620 04 E0          addis   ac0.m, -32
8621 6C 1E          mov     ac0, ac1            mv      ax1.h, ac0.m
8622 1C 66          mrr     ar3, ix2
8623 34 86          andr    ac0.m, ax0.h        sln     ac0.m, ax0.l
8624 37 86          andr    ac1.m, ax1.h        sln     ac0.m, ax0.l
8625 4C 52          add     ac0, ac1            l       ax1.l, @ar2
8626 48 6B          addax   ac0, ax0            l       ac1.l, @ar3
8627 1A DC          srrd    @ar2, ac0.l
8628 1A 5E          srr     @ar2, ac0.m
8629 18 3E          lrr     ac0.m, @ar1
862A 18 BF          lrrd    ac1.m, @ar1
862B 33 D2          xorr    ac1.m, ax1.h        ld      ax0.l, ax1.h, @ar2
862C 19 5B          lrri    ax1.h, @ar2
862D 36 5F          andr    ac0.m, ax1.h        ln      ax1.h, @ar3
862E 37 1E          andr    ac1.m, ax1.h        mv      ax1.h, ac0.m
862F 3B 1D          orr     ac1.m, ax1.h        mv      ax1.h, ac1.l
8630 1A FF          srrd    @ar3, ac1.m
8631 18 3E          lrr     ac0.m, @ar1
8632 34 79          andr    ac0.m, ax0.h        l       ac1.m, @ar1
8633 33 9A          xorr    ac1.m, ax1.h        slm     ac0.m, ax0.h
8634 37 05          andr    ac1.m, ax1.h        dr      ar1
8635 39 0A          orr     ac1.m, ax0.h        ir      ar2
8636 1B FF          srrn    @ar3, ac1.m
8637 19 7B          lrri    ax1.h, @ar3
8638 33 59          xorr    ac1.m, ax1.h        l       ax1.h, @ar1
8639 33 5A          xorr    ac1.m, ax1.h        l       ax1.h, @ar2
863A F5 57          lsr16   ac1                 ln      ax1.l, @ar3
863B 19 7F          lrri    ac1.m, @ar3
863C 02 DF          ret     
```

## WaitDspDma


```
863D 00 DF FF C9    lr      ac1.m, $(DSCR)
863F 03 C0 00 04    tset    ac1.m, #0x0004
8641 02 9D 86 3D    jok     $0x863D
8643 02 DF          ret     
```

## 

Called from IPL, CardUnlock


```
8644 8E 00          clr40                               
8645 00 81 08 00    lri     ar1, #0x0800
8647 00 92 00 FF    lri     config, #0x00FF
8649 00 DF 04 03    lr      ac1.m, $0x0403
864B 05 03          addis   ac1.m, 3
864C 15 6E          lsr     ac1, #0x2E
864D 15 02          lsl     ac1, #0x02
864E 29 C9          srs     $(DSCR), ax0.h
864F 00 DE 04 00    lr      ac0.m, $0x0400
8651 2E CE          srs     $(DSMAH), ac0.m
8652 00 DE 04 01    lr      ac0.m, $0x0401
8654 2E CF          srs     $(DSMAL), ac0.m
8655 00 E1 FF CD    sr      $(DSPA), ar1
8657 2D CB          srs     $(DSBL), ac1.l
8658 02 BF 86 3D    call    $0x863D                         // WaitDspDma
865A 29 D1          srs     $(ACFMT), ax0.h
865B 29 D4          srs     $(ACSAH), ax0.h
865C 29 D5          srs     $(ACSAL), ax0.h
865D 16 D6 01 FF    si      $(ACEAH), #0x01FF
865F 16 D7 FF FF    si      $(ACEAL), #0xFFFF
8661 00 DF 04 04    lr      ac1.m, $0x0404
8663 00 DD 04 05    lr      ac1.l, $0x0405
8665 15 7F          lsr     ac1, #0x3F
8666 03 60 80 00    ori     ac1.m, #0x8000
8668 2F D8          srs     $(ACCAH), ac1.m
8669 2D D9          srs     $(ACCAL), ac1.l
866A 00 80 FF D3    lri     ar0, #0xFFD3                    // RAW accelerator data (R/W)
866C 00 84 00 00    lri     ix0, #0x0000
866E 00 DF 04 03    lr      ac1.m, $0x0403
8670 03 C0 00 01    tset    ac1.m, #0x0001
8672 15 7F          lsr     ac1, #0x3F
8673 1C DF          mrr     ix2, ac1.m
8674 00 9A FF F8    lri     ax1.l, #0xFFF8
8676 00 9B 00 18    lri     ax1.h, #0x0018
8678 81 79          clr     ac0                 l       ac1.m, @ar1
8679 00 66 86 7F    bloop   ix2, $0x867F
    867B 35 BC          andr    ac1.m, ax0.h        lsnm    ax1.h, ac0.m
    867C 37 93          andr    ac1.m, ax1.h        sl      ac1.m, ax0.h
    867D F5 00          lsr16   ac1                         
    867E 70 17          addaxl  ac0, ax0.l      mv      ax0.h, ac1.m
    867F 72 79          addaxl  ac0, ax1.l      l       ac1.m, @ar1
8680 02 9C 86 85    jnok    $0x8685
8682 35 BC          andr    ac1.m, ax0.h        lsnm    ax1.h, ac0.m
8683 1F 1F          mrr     ax0.l, ac1.m
8684 70 00          addaxl  ac0, ax0.l              
8685 6D 00          mov     ac1, ac0                    
8686 00 81 04 08    lri     ar1, #0x0408
8688 00 9A 17 0A    lri     ax1.l, #0x170A
868A 00 98 74 89    lri     ax0.l, #0x7489
868C 48 00          addax   ac0, ax0                    
868D 1B 3E          srri    @ar1, ac0.m
868E 1B 3C          srri    @ar1, ac0.l
868F 00 9E 05 EF    lri     ac0.m, #0x05EF
8691 1B 3E          srri    @ar1, ac0.m
8692 00 9E E0 AA    lri     ac0.m, #0xE0AA
8694 1B 3E          srri    @ar1, ac0.m
8695 00 9E DA F4    lri     ac0.m, #0xDAF4
8697 1B 3E          srri    @ar1, ac0.m
8698 00 9E B1 57    lri     ac0.m, #0xB157
869A 1B 3E          srri    @ar1, ac0.m
869B 00 9E 6B BE    lri     ac0.m, #0x6BBE
869D 1B 3E          srri    @ar1, ac0.m
869E 00 9E C3 B6    lri     ac0.m, #0xC3B6
86A0 08 08          lris    ax0.l, 8
86A1 71 31          addaxl  ac1, ax0.l      s       @ar1, ac0.m
86A2 1B 3D          srri    @ar1, ac1.l
86A3 1B 31          srri    @ar1, ac1.h
86A4 28 D1          srs     $(ACFMT), ax0.l
86A5 28 D4          srs     $(ACSAH), ax0.l
86A6 28 D5          srs     $(ACSAL), ax0.l
86A7 16 D6 07 FF    si      $(ACEAH), #0x07FF
86A9 16 D7 FF FF    si      $(ACEAL), #0xFFFF
86AB 00 DE 04 04    lr      ac0.m, $0x0404
86AD 00 DC 04 05    lr      ac0.l, $0x0405
86AF 14 01          lsl     ac0, #0x01
86B0 2E D8          srs     $(ACCAH), ac0.m
86B1 2C D9          srs     $(ACCAL), ac0.l
86B2 00 81 04 09    lri     ar1, #0x0409
86B4 00 82 04 0E    lri     ar2, #0x040E
86B6 00 85 04 10    lri     ix1, #0x0410
86B8 00 87 FF FE    lri     ix3, #0xFFFE
86BA 00 88 04 0E    lri     lm0, #0x040E
86BC 00 DF 04 03    lr      ac1.m, $0x0403
86BE 79 00          decm    ac1                         
86BF 15 7F          lsr     ac1, #0x3F
86C0 1F 3F          mrr     ax0.h, ac1.m
86C1 19 9D          lrrn    ac1.l, @ar0
86C2 19 9A          lrrn    ax1.l, @ar0
86C3 1C 65          mrr     ar3, ix1
86C4 00 79 86 CF    bloop   ax0.h, $0x86CF
86C6 02 BF 86 E5    call    $0x86E5
86C8 1F B9          mrr     ac1.l, ax0.h
86C9 1F 46          mrr     ax1.l, ix2
86CA 1C 65          mrr     ar3, ix1
86CB 02 BF 86 E5    call    $0x86E5
86CD 1F B9          mrr     ac1.l, ax0.h
86CE 1F 46          mrr     ax1.l, ix2
86CF 1C 65          mrr     ar3, ix1
86D0 02 9D 86 D4    jok     $0x86D4
86D2 02 BF 86 E5    call    $0x86E5
86D4 00 88 FF FF    lri     lm0, #0xFFFF
86D6 16 C9 00 01    si      $(DSCR), #0x0001
86D8 00 DE 04 06    lr      ac0.m, $0x0406
86DA 2E CE          srs     $(DSMAH), ac0.m
86DB 00 DE 04 07    lr      ac0.m, $0x0407
86DD 2E CF          srs     $(DSMAL), ac0.m
86DE 16 CD 04 0A    si      $(DSPA), #0x040A
86E0 16 CB 00 04    si      $(DSBL), #0x0004
86E2 02 BF 86 3D    call    $0x863D                     // WaitDspDma
86E4 02 DF          ret     
```


```
86E5 19 99          lrrn    ax0.h, @ar0
86E6 19 9C          lrrn    ac0.l, @ar0
86E7 1C DC          mrr     ix2, ac0.l
86E8 14 14          lsl     ac0, #0x14
86E9 38 5A          orr     ac0.m, ax0.h        l       ax1.h, @ar2
86EA F0 52          lsl16   ac0                 l       ax1.l, @ar2
86EB 91 06          asr16   ac0                 dr      ar2
86EC 15 18          lsl     ac1, #0x18
86ED 30 86          xorr    ac0.m, ax0.h        sln     ac0.m, ax0.l
86EE 1F F9          mrr     ac1.m, ax0.h
86EF 15 0C          lsl     ac1, #0x0C
86F0 30 86          xorr    ac0.m, ax0.h        sln     ac0.m, ax0.l
86F1 1F 1E          mrr     ax0.l, ac0.m
86F2 18 BC          lrrd    ac0.l, @ar1
86F3 19 3E          lrri    ac0.m, @ar1
86F4 70 00          addaxl  ac0, ax0.l              
86F5 1A BC          srrd    @ar1, ac0.l
86F6 18 DF          lrrd    ac1.m, @ar2
86F7 31 31          xorr    ac1.m, ax0.h        s       @ar1, ac0.m
86F8 F5 43          lsr16   ac1                 l       ax0.l, @ar3
86F9 18 DF          lrrd    ac1.m, @ar2
86FA 33 00          xorr    ac1.m, ax1.h                
86FB 4D 63          add     ac1, ac0            l       ac0.l, @ar3
86FC 76 07          inc     ac0                 dr      ar3
86FD 1B 7C          srri    @ar3, ac0.l
86FE 70 42          addaxl  ac0, ax0.l      l       ax0.l, @ar2
86FF 14 23          lsl     ac0, #0x23
8700 14 5D          lsr     ac0, #0x1D
8701 7C 00          neg     ac0                         
8702 F0 00          lsl16   ac0                         
8703 04 F8          addis   ac0.m, -8
8704 1F 5E          mrr     ax1.l, ac0.m
8705 04 28          addis   ac0.m, 40
8706 6C 1E          mov     ac0, ac1            mv      ax1.h, ac0.m
8707 14 08          lsl     ac0, #0x08
8708 1C 68          mrr     ar3, lm0
8709 34 86          andr    ac0.m, ax0.h        sln     ac0.m, ax0.l
870A 37 86          andr    ac1.m, ax1.h        sln     ac0.m, ax0.l
870B 4C 52          add     ac0, ac1            l       ax1.l, @ar2
870C 48 6B          addax   ac0, ax0            l       ac1.l, @ar3
870D 1A DC          srrd    @ar2, ac0.l
870E 1A 5E          srr     @ar2, ac0.m
870F 18 3E          lrr     ac0.m, @ar1
8710 18 BF          lrrd    ac1.m, @ar1
8711 33 D2          xorr    ac1.m, ax1.h        ld      ax0.l, ax1.h, @ar2
8712 19 FB          lrrn    ax1.h, @ar3
8713 36 5A          andr    ac0.m, ax1.h        l       ax1.h, @ar2
8714 37 1E          andr    ac1.m, ax1.h        mv      ax1.h, ac0.m
8715 3B 1D          orr     ac1.m, ax1.h        mv      ax1.h, ac1.l
8716 1A FF          srrd    @ar3, ac1.m
8717 18 3E          lrr     ac0.m, @ar1
8718 36 79          andr    ac0.m, ax1.h        l       ac1.m, @ar1
8719 33 9E          xorr    ac1.m, ax1.h        slnm    ac0.m, ax0.h
871A 35 05          andr    ac1.m, ax0.h        dr      ar1
871B 3B 0A          orr     ac1.m, ax1.h        ir      ar2
871C 1B FF          srrn    @ar3, ac1.m
871D 19 7B          lrri    ax1.h, @ar3
871E 33 59          xorr    ac1.m, ax1.h        l       ax1.h, @ar1
871F 33 5A          xorr    ac1.m, ax1.h        l       ax1.h, @ar2
8720 F5 57          lsr16   ac1                 ln      ax1.l, @ar3
8721 19 7F          lrri    ac1.m, @ar3
8722 31 2A          xorr    ac1.m, ax0.h        s       @ar2, ac1.l
8723 33 00          xorr    ac1.m, ax1.h                
8724 1A DF          srrd    @ar2, ac1.m
8725 02 DF          ret     
```


```
8726 8E 00          clr40                               
8727 00 81 08 00    lri     ar1, #0x0800
8729 00 92 00 FF    lri     config, #0x00FF
872B 00 DF 04 03    lr      ac1.m, $0x0403
872D F5 00          lsr16   ac1                         
872E 29 C9          srs     $(DSCR), ax0.h
872F 00 DE 04 00    lr      ac0.m, $0x0400
8731 2E CE          srs     $(DSMAH), ac0.m
8732 00 DE 04 01    lr      ac0.m, $0x0401
8734 2E CF          srs     $(DSMAL), ac0.m
8735 00 E1 FF CD    sr      $(DSPA), ar1
8737 2D CB          srs     $(DSBL), ac1.l
8738 02 BF 86 3D    call    $0x863D                         // WaitDspDma
873A 29 D1          srs     $(ACFMT), ax0.h
873B 29 D4          srs     $(ACSAH), ax0.h
873C 29 D5          srs     $(ACSAL), ax0.h
873D 16 D6 01 FF    si      $(ACEAH), #0x01FF
873F 16 D7 FF FF    si      $(ACEAL), #0xFFFF
8741 00 DF 04 04    lr      ac1.m, $0x0404
8743 00 DD 04 05    lr      ac1.l, $0x0405
8745 15 7F          lsr     ac1, #0x3F
8746 03 60 80 00    ori     ac1.m, #0x8000
8748 2F D8          srs     $(ACCAH), ac1.m
8749 2D D9          srs     $(ACCAL), ac1.l
874A 00 80 FF D3    lri     ar0, #0xFFD3
874C 00 84 00 00    lri     ix0, #0x0000
874E 00 DF 04 03    lr      ac1.m, $0x0403
8750 15 7F          lsr     ac1, #0x3F
8751 1C DF          mrr     ix2, ac1.m
8752 00 9A FF F8    lri     ax1.l, #0xFFF8
8754 00 9B 00 18    lri     ax1.h, #0x0018
8756 81 79          clr     ac0                 l       ac1.m, @ar1
8757 00 66 87 5D    bloop   ix2, $0x875D
8759 35 BC          andr    ac1.m, ax0.h        lsnm    ax1.h, ac0.m
875A 37 93          andr    ac1.m, ax1.h        sl      ac1.m, ax0.h
875B F5 00          lsr16   ac1                         
875C 70 17          addaxl  ac0, ax0.l      mv      ax0.h, ac1.m
875D 72 79          addaxl  ac0, ax1.l      l       ac1.m, @ar1
875E 6D 00          mov     ac1, ac0                    
875F 00 81 04 08    lri     ar1, #0x0408
8761 00 9A 29 8F    lri     ax1.l, #0x298F
8763 00 98 0B 7F    lri     ax0.l, #0x0B7F
8765 48 00          addax   ac0, ax0                    
8766 1B 3E          srri    @ar1, ac0.m
8767 1B 3C          srri    @ar1, ac0.l
8768 00 9E 4B F9    lri     ac0.m, #0x4BF9
876A 1B 3E          srri    @ar1, ac0.m
876B 00 9E C9 B1    lri     ac0.m, #0xC9B1
876D 1B 3E          srri    @ar1, ac0.m
876E 00 9E D3 0D    lri     ac0.m, #0xD30D
8770 1B 3E          srri    @ar1, ac0.m
8771 00 9E 6B 99    lri     ac0.m, #0x6B99
8773 1B 3E          srri    @ar1, ac0.m
8774 00 9E 19 1D    lri     ac0.m, #0x191D
8776 1B 3E          srri    @ar1, ac0.m
8777 00 9E 31 DD    lri     ac0.m, #0x31DD
8779 08 12          lris    ax0.l, 18
877A 71 31          addaxl  ac1, ax0.l      s       @ar1, ac0.m
877B 1B 3D          srri    @ar1, ac1.l
877C 1B 31          srri    @ar1, ac1.h
877D 28 D1          srs     $(ACFMT), ax0.l
877E 28 D4          srs     $(ACSAH), ax0.l
877F 28 D5          srs     $(ACSAL), ax0.l
8780 16 D6 07 FF    si      $(ACEAH), #0x07FF
8782 16 D7 FF FF    si      $(ACEAL), #0xFFFF
8784 00 DE 04 04    lr      ac0.m, $0x0404
8786 00 DC 04 05    lr      ac0.l, $0x0405
8788 76 00          inc     ac0                         
8789 14 01          lsl     ac0, #0x01
878A 2E D8          srs     $(ACCAH), ac0.m
878B 2C D9          srs     $(ACCAL), ac0.l
878C 00 DE 08 00    lr      ac0.m, $0x0800
878E 14 78          lsr     ac0, #0x38
878F 2E DA          srs     $(ACPDS), ac0.m
8790 16 A0 01 BA    si      $(ADPCM_A00), #0x01BA
8792 16 A1 04 B0    si      $(ADPCM_A10), #0x04B0
8794 16 A2 04 4D    si      $(ADPCM_A20), #0x044D
8796 16 A3 01 E7    si      $(ADPCM_A30), #0x01E7
8798 16 A4 02 DA    si      $(ADPCM_A40), #0x02DA
879A 16 A5 04 52    si      $(ADPCM_A50), #0x0452
879C 16 A6 05 7A    si      $(ADPCM_A60), #0x057A
879E 16 A7 01 BF    si      $(ADPCM_A70), #0x01BF
87A0 28 DB          srs     $(ACYN1), ax0.l
87A1 28 DC          srs     $(ACYN2), ax0.l
87A2 00 80 FF DD    lri     ar0, #0xFFDD                            // Decoded ADPCM data
87A4 00 81 04 09    lri     ar1, #0x0409
87A6 00 82 04 0F    lri     ar2, #0x040F
87A8 00 85 04 10    lri     ix1, #0x0410
87AA 00 86 FF FF    lri     ix2, #0xFFFF
87AC 00 87 FF FE    lri     ix3, #0xFFFE
87AE 8B 00          m0                                  
87AF 8C 00          clr15                               
87B0 00 DE 04 03    lr      ac0.m, $0x0403
87B2 14 7D          lsr     ac0, #0x3D
87B3 0A 07          lris    ax1.l, 7
87B4 C0 00          mulc    ac0.m, ax0.h                
87B5 6E 00          movp    ac0                         
87B6 7A 00          dec     ac0                         
87B7 1F 3C          mrr     ax0.h, ac0.l
87B8 19 9D          lrrn    ac1.l, @ar0
87B9 18 BC          lrrd    ac0.l, @ar1
87BA 19 3E          lrri    ac0.m, @ar1
87BB 19 DA          lrrn    ax1.l, @ar2
87BC 1C 65          mrr     ar3, ix1
87BD 19 9F          lrrn    ac1.m, @ar0
87BE 4C 5E          add     ac0, ac1            ln      ax1.h, @ar2
87BF 1A BC          srrd    @ar1, ac0.l
87C0 1B 3E          srri    @ar1, ac0.m
87C1 00 79 87 CD    bloop   ax0.h, $0x87CD
87C3 02 BF 87 DF    call    $0x87DF
87C5 19 9D          lrrn    ac1.l, @ar0
87C6 18 BC          lrrd    ac0.l, @ar1
87C7 19 3E          lrri    ac0.m, @ar1
87C8 19 DA          lrrn    ax1.l, @ar2
87C9 1C 65          mrr     ar3, ix1
87CA 19 9F          lrrn    ac1.m, @ar0
87CB 4C 5E          add     ac0, ac1            ln      ax1.h, @ar2
87CC 1A BC          srrd    @ar1, ac0.l
87CD 1B 3E          srri    @ar1, ac0.m
87CE 02 BF 87 DF    call    $0x87DF
87D0 16 C9 00 01    si      $(DSCR), #0x0001
87D2 00 DE 04 06    lr      ac0.m, $0x0406
87D4 2E CE          srs     $(DSMAH), ac0.m
87D5 00 DE 04 07    lr      ac0.m, $0x0407
87D7 2E CF          srs     $(DSMAL), ac0.m
87D8 16 CD 04 0A    si      $(DSPA), #0x040A
87DA 16 CB 00 04    si      $(DSBL), #0x0004
87DC 02 BF 86 3D    call    $0x863D                     // WaitDspDma
87DE 02 DF          ret     
```

## LFSR ? Crypto ?

```
87DF 1F FC          mrr     ac1.m, ac0.l
87E0 31 66          xorr    ac1.m, ax0.h        ln      ac0.l, @ar2
87E1 F5 43          lsr16   ac1                 l       ax0.l, @ar3
87E2 1F FE          mrr     ac1.m, ac0.m
87E3 33 76          xorr    ac1.m, ax1.h        ln      ac0.m, @ar2
87E4 4D 63          add     ac1, ac0            l       ac0.l, @ar3
87E5 76 07          inc     ac0                 dr      ar3
87E6 1B 7C          srri    @ar3, ac0.l
87E7 70 46          addaxl  ac0, ax0.l      ln      ax0.l, @ar2
87E8 14 23          lsl     ac0, #0x23
87E9 14 5D          lsr     ac0, #0x1D
87EA 7C 0F          neg     ac0                 nr      ar3, ix3
87EB F0 0F          lsl16   ac0                 nr      ar3, ix3
87EC 04 F8          addis   ac0.m, -8
87ED 1F 5E          mrr     ax1.l, ac0.m
87EE 04 28          addis   ac0.m, 40
87EF 6C 1E          mov     ac0, ac1            mv      ax1.h, ac0.m
87F0 14 08          lsl     ac0, #0x08
87F1 34 85          andr    ac0.m, ax0.h        lsn     ax0.l, ac1.m
87F2 37 D9          andr    ac1.m, ax1.h        ldm     ax0.l, ax1.h, @ar1
87F3 4C 52          add     ac0, ac1            l       ax1.l, @ar2
87F4 48 53          addax   ac0, ax0            l       ax1.l, @ar3
87F5 1B DC          srrn    @ar2, ac0.l
87F6 1B 5E          srri    @ar2, ac0.m
87F7 32 5F          xorr    ac0.m, ax1.h        ln      ax1.h, @ar3
87F8 30 51          xorr    ac0.m, ax0.h        l       ax1.l, @ar1
87F9 00 0A          iar     ar2
87FA F0 32          lsl16   ac0                 s       @ar2, ac0.m
87FB 30 05          xorr    ac0.m, ax0.h        dr      ar1
87FC 32 0F          xorr    ac0.m, ax1.h        nr      ar3, ix3
87FD 1B 5E          srri    @ar2, ac0.m
87FE 18 3B          lrr     ax1.h, @ar1
87FF 36 53          andr    ac0.m, ax1.h        l       ax1.l, @ar3
8800 18 BF          lrrd    ac1.m, @ar1
8801 33 9E          xorr    ac1.m, ax1.h        slnm    ac0.m, ax0.h
8802 35 71          andr    ac1.m, ax0.h        l       ac0.m, @ar1
8803 3B 05          orr     ac1.m, ax1.h        dr      ar1
8804 F5 57          lsr16   ac1                 ln      ax1.l, @ar3
8805 19 3F          lrri    ac1.m, @ar1
8806 34 5F          andr    ac0.m, ax0.h        ln      ax1.h, @ar3
8807 33 9A          xorr    ac1.m, ax1.h        slm     ac0.m, ax0.h
8808 37 0A          andr    ac1.m, ax1.h        ir      ar2
8809 39 2E          orr     ac1.m, ax0.h        sn      @ar2, ac1.l
880A 1B 5F          srri    @ar2, ac1.m
880B 02 DF          ret     
```


```
880C 8E 00          clr40                               
880D 00 81 08 00    lri     ar1, #0x0800
880F 00 92 00 FF    lri     config, #0x00FF
8811 00 DF 04 03    lr      ac1.m, $0x0403
8813 05 03          addis   ac1.m, 3
8814 15 6E          lsr     ac1, #0x2E
8815 15 02          lsl     ac1, #0x02
8816 29 C9          srs     $(DSCR), ax0.h
8817 00 DE 04 00    lr      ac0.m, $0x0400
8819 2E CE          srs     $(DSMAH), ac0.m
881A 00 DE 04 01    lr      ac0.m, $0x0401
881C 2E CF          srs     $(DSMAL), ac0.m
881D 00 E1 FF CD    sr      $(DSPA), ar1
881F 2D CB          srs     $(DSBL), ac1.l
8820 02 BF 86 3D    call    $0x863D                         // WaitDspDma
8822 29 D1          srs     $(ACFMT), ax0.h
8823 29 D4          srs     $(ACSAH), ax0.h
8824 29 D5          srs     $(ACSAL), ax0.h
8825 16 D6 01 FF    si      $(ACEAH), #0x01FF
8827 16 D7 FF FF    si      $(ACEAL), #0xFFFF
8829 00 DF 04 04    lr      ac1.m, $0x0404
882B 00 DD 04 05    lr      ac1.l, $0x0405
882D 15 7F          lsr     ac1, #0x3F
882E 03 60 80 00    ori     ac1.m, #0x8000
8830 2F D8          srs     $(ACCAH), ac1.m
8831 2D D9          srs     $(ACCAL), ac1.l
8832 00 80 FF D3    lri     ar0, #0xFFD3
8834 00 84 00 00    lri     ix0, #0x0000
8836 00 DF 04 03    lr      ac1.m, $0x0403
8838 03 C0 00 01    tset    ac1.m, #0x0001
883A 15 7F          lsr     ac1, #0x3F
883B 1C DF          mrr     ix2, ac1.m
883C 00 9A FF F8    lri     ax1.l, #0xFFF8
883E 00 9B 00 18    lri     ax1.h, #0x0018
8840 81 79          clr     ac0                 l       ac1.m, @ar1
8841 00 66 88 47    bloop   ix2, $0x8847
8843 35 BC          andr    ac1.m, ax0.h        lsnm    ax1.h, ac0.m
8844 37 93          andr    ac1.m, ax1.h        sl      ac1.m, ax0.h
8845 F5 00          lsr16   ac1                         
8846 70 17          addaxl  ac0, ax0.l      mv      ax0.h, ac1.m
8847 72 79          addaxl  ac0, ax1.l      l       ac1.m, @ar1
8848 02 9C 88 4D    jnok    $0x884D
884A 35 BC          andr    ac1.m, ax0.h        lsnm    ax1.h, ac0.m
884B 1F 1F          mrr     ax0.l, ac1.m
884C 70 00          addaxl  ac0, ax0.l              
884D 6D 00          mov     ac1, ac0                    
884E 00 81 04 08    lri     ar1, #0x0408
8850 00 9A 4E A2    lri     ax1.l, #0x4EA2
8852 00 98 1E 71    lri     ax0.l, #0x1E71
8854 48 00          addax   ac0, ax0                    
8855 1B 3E          srri    @ar1, ac0.m
8856 1B 3C          srri    @ar1, ac0.l
8857 00 9E CC 0A    lri     ac0.m, #0xCC0A
8859 1B 3E          srri    @ar1, ac0.m
885A 00 9E 14 4B    lri     ac0.m, #0x144B
885C 1B 3E          srri    @ar1, ac0.m
885D 00 9E F5 41    lri     ac0.m, #0xF541
885F 1B 3E          srri    @ar1, ac0.m
8860 00 9E 87 8D    lri     ac0.m, #0x878D
8862 1B 3E          srri    @ar1, ac0.m
8863 00 9E A3 BC    lri     ac0.m, #0xA3BC
8865 1B 3E          srri    @ar1, ac0.m
8866 00 9E 64 E4    lri     ac0.m, #0x64E4
8868 08 03          lris    ax0.l, 3
8869 71 31          addaxl  ac1, ax0.l      s       @ar1, ac0.m
886A 1B 3D          srri    @ar1, ac1.l
886B 1B 31          srri    @ar1, ac1.h
886C 16 D1 00 18    si      $(ACFMT), #0x0018
886E 28 D4          srs     $(ACSAH), ax0.l
886F 28 D5          srs     $(ACSAL), ax0.l
8870 16 D6 07 FF    si      $(ACEAH), #0x07FF
8872 16 D7 FF FF    si      $(ACEAL), #0xFFFF
8874 00 DE 04 04    lr      ac0.m, $0x0404
8876 00 DC 04 05    lr      ac0.l, $0x0405
8878 14 01          lsl     ac0, #0x01
8879 2E D8          srs     $(ACCAH), ac0.m
887A 2C D9          srs     $(ACCAL), ac0.l
887B 28 DA          srs     $(ACPDS), ax0.l
887C 16 A0 09 78    si      $(ADPCM_A00), #0x0978
887E 16 A1 E5 41    si      $(ADPCM_A10), #0xE541
8880 16 DE FC 82    si      $(ACGAN), #0xFC82
8882 28 DB          srs     $(ACYN1), ax0.l
8883 00 80 FF DD    lri     ar0, #0xFFDD                        // Decoded ADPCM data
8885 00 81 04 09    lri     ar1, #0x0409
8887 00 82 04 0F    lri     ar2, #0x040F
8889 00 85 04 10    lri     ix1, #0x0410
888B 00 86 FF FF    lri     ix2, #0xFFFF
888D 00 87 FF FC    lri     ix3, #0xFFFC
888F 28 DC          srs     $(ACYN2), ax0.l
8890 00 DE 04 03    lr      ac0.m, $0x0403
8892 78 00          decm    ac0                         
8893 1F 3E          mrr     ax0.h, ac0.m
8894 19 9F          lrrn    ac1.m, @ar0
8895 18 BC          lrrd    ac0.l, @ar1
8896 19 3E          lrri    ac0.m, @ar1
8897 19 DA          lrrn    ax1.l, @ar2
8898 1C 65          mrr     ar3, ix1
8899 19 9D          lrrn    ac1.l, @ar0
889A 4C 5A          add     ac0, ac1            l       ax1.h, @ar2
889B 1A BC          srrd    @ar1, ac0.l
889C 1B 3E          srri    @ar1, ac0.m
889D 00 79 88 A9    bloop   ax0.h, $0x88A9
889F 02 BF 88 BB    call    $0x88BB
88A1 19 9F          lrrn    ac1.m, @ar0
88A2 18 BC          lrrd    ac0.l, @ar1
88A3 19 3E          lrri    ac0.m, @ar1
88A4 19 DA          lrrn    ax1.l, @ar2
88A5 1C 65          mrr     ar3, ix1
88A6 19 9D          lrrn    ac1.l, @ar0
88A7 4C 5A          add     ac0, ac1            l       ax1.h, @ar2
88A8 1A BC          srrd    @ar1, ac0.l
88A9 1B 3E          srri    @ar1, ac0.m
88AA 02 BF 88 BB    call    $0x88BB
88AC 16 C9 00 01    si      $(DSCR), #0x0001
88AE 00 DE 04 06    lr      ac0.m, $0x0406
88B0 2E CE          srs     $(DSMAH), ac0.m
88B1 00 DE 04 07    lr      ac0.m, $0x0407
88B3 2E CF          srs     $(DSMAL), ac0.m
88B4 16 CD 04 0A    si      $(DSPA), #0x040A
88B6 16 CB 00 04    si      $(DSBL), #0x0004
88B8 02 BF 86 3D    call    $0x863D                         // WaitDspDma
88BA 02 DF          ret     
```

## LFSR ? Crypto ?


```
88BB 19 D8          lrrn    ax0.l, @ar2
88BC 19 DA          lrrn    ax1.l, @ar2
88BD 48 56          addax   ac0, ax0            ln      ax1.l, @ar2
88BE 1F FC          mrr     ac1.m, ac0.l
88BF 31 56          xorr    ac1.m, ax0.h        ln      ax1.l, @ar2
88C0 F5 43          lsr16   ac1                 l       ax0.l, @ar3
88C1 1F FE          mrr     ac1.m, ac0.m
88C2 31 63          xorr    ac1.m, ax0.h        l       ac0.l, @ar3
88C3 76 07          inc     ac0                 dr      ar3
88C4 1B 7C          srri    @ar3, ac0.l
88C5 70 46          addaxl  ac0, ax0.l      ln      ax0.l, @ar2
88C6 14 23          lsl     ac0, #0x23
88C7 14 6D          lsr     ac0, #0x2D
88C8 1F 5E          mrr     ax1.l, ac0.m
88C9 04 E0          addis   ac0.m, -32
88CA 00 1F          addarn  ar3, ix3
88CB 6C 1E          mov     ac0, ac1            mv      ax1.h, ac0.m
88CC 34 85          andr    ac0.m, ax0.h        lsn     ax0.l, ac1.m
88CD 37 D9          andr    ac1.m, ax1.h        ldm     ax0.l, ax1.h, @ar1
88CE 4C 52          add     ac0, ac1            l       ax1.l, @ar2
88CF 48 53          addax   ac0, ax0            l       ax1.l, @ar3
88D0 1B DC          srrn    @ar2, ac0.l
88D1 1B 5E          srri    @ar2, ac0.m
88D2 32 5F          xorr    ac0.m, ax1.h        ln      ax1.h, @ar3
88D3 30 51          xorr    ac0.m, ax0.h        l       ax1.l, @ar1
88D4 00 0A          iar     ar2
88D5 F0 32          lsl16   ac0                 s       @ar2, ac0.m
88D6 30 05          xorr    ac0.m, ax0.h        dr      ar1
88D7 32 00          xorr    ac0.m, ax1.h                
88D8 1B 5E          srri    @ar2, ac0.m
88D9 18 3F          lrr     ac1.m, @ar1
88DA 33 9E          xorr    ac1.m, ax1.h        slnm    ac0.m, ax0.h
88DB 18 BE          lrrd    ac0.m, @ar1
88DC 37 53          andr    ac1.m, ax1.h        l       ax1.l, @ar3
88DD 34 1F          andr    ac0.m, ax0.h        mv      ax1.h, ac1.m
88DE 3A 79          orr     ac0.m, ax1.h        l       ac1.m, @ar1
88DF F4 05          lsr16   ac0                 dr      ar1
88E0 33 D3          xorr    ac1.m, ax1.h        ldax    ax1, @ar0
88E1 35 71          andr    ac1.m, ax0.h        l       ac0.m, @ar1
88E2 00 09          iar     ar1
88E3 18 3B          lrr     ax1.h, @ar1
88E4 36 1B          andr    ac0.m, ax1.h        mv      ax1.l, ac1.m
88E5 38 7A          orr     ac0.m, ax0.h        l       ac1.m, @ar2
88E6 18 DD          lrrd    ac1.l, @ar2
88E7 4C 05          add     ac0, ac1            dr      ar1
88E8 1B 5E          srri    @ar2, ac0.m
88E9 1A 5C          srr     @ar2, ac0.l
88EA 02 DF          ret     
88EB 00 00          nop     
88EC 00 00          nop     
88ED 00 00          nop     
88EE 00 00          nop     
88EF 00 00          nop     
```

... [Address] = Address (filler)

// Possible checksum

```
8FFE 06 E2
8FFF 88 45
```
