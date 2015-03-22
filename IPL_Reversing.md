very first code started from 0x81300000:

## `__start` ##

```
__start
81300000  48000099  bl          0x81300098              ; __init_registers
81300004  48000171  bl          0x81300174              ; __init_hardware
81300008  3800FFFF  li          r0, -1
8130000C  9421FFF8  stwu        r1, -8 (r1)
81300010  90010004  stw         r0, 4 (r1)
81300014  90010000  stw         r0, 0 (r1)
81300018  4800009D  bl          0x813000B4              ; __init_data
8130001C  3CC08000  lis         r6, 0x8000
81300020  38C600F4  addi        r6, r6, 244
81300024  80A60000  lwz         r5, 0 (r6)
81300028  28050000  cmplwi      r5, 0
8130002C  41A20054  beq+        0x81300080 
81300030  38C50008  addi        r6, r5, 8
81300034  80C60000  lwz         r6, 0 (r6)
81300038  28060000  cmplwi      r6, 0
8130003C  41A20044  beq+        0x81300080 
81300040  7CC53214  add         r6, r5, r6
81300044  80660000  lwz         r3, 0 (r6)
81300048  28030000  cmplwi      r3, 0
8130004C  41820034  beq-        0x81300080 
81300050  38860004  addi        r4, r6, 4
81300054  7C6903A6  mtctr       r3
81300058  38C60004  addi        r6, r6, 4
8130005C  80E60000  lwz         r7, 0 (r6)
81300060  7CE72A14  add         r7, r7, r5
81300064  90E60000  stw         r7, 0 (r6)
81300068  4200FFF0  bdnz+       0x81300058 
8130006C  3CA08000  lis         r5, 0x8000
81300070  38A50034  addi        r5, r5, 52
81300074  54870034  rlwinm      r7, r4, 0, 0, 26            mask:0xFFFFFFE0
81300078  90E50000  stw         r7, 0 (r5)
8130007C  4800000C  b           0x81300088 
81300080  38600000  li          r3, 0
81300084  38800000  li          r4, 0
81300088  4806415D  bl          0x813641E4              ; DBInit
8130008C  480640C5  bl          0x81364150              ; [0x78647801]
81300090  48000455  bl          0x813004E4              ; main
81300094  4807B300  b           0x8137B394 
```

**No OSInit since memory is garbaged by first bootstrap stage**

## main ##

```
main
813004E4  7C0802A6  mflr        r0
813004E8  90010004  stw         r0, 4 (r1)
813004EC  9421FFF8  stwu        r1, -8 (r1)
813004F0  4BFFFF6D  bl          0x8130045C              ; BS2Init
813004F4  4805B221  bl          0x8135B714              ; OSInit
813004F8  4805EA9D  bl          0x8135EF94              ; AD16Init
813004FC  3C600800  lis         r3, 0x0800
81300500  4805EBD1  bl          0x8135F0D0              ; AD16WriteReg
81300504  48065CBD  bl          0x813661C0              ; DVDInit
81300508  3C600900  lis         r3, 0x0900
8130050C  4805EBC5  bl          0x8135F0D0              ; AD16WriteReg
81300510  4806ED79  bl          0x8136F288              ; CARDInit
81300514  3C600A00  lis         r3, 0x0A00
81300518  4805EBB9  bl          0x8135F0D0              ; AD16WriteReg
8130051C  480019F1  bl          0x81301F0C 
81300520  38600004  li          r3, 4
81300524  48068AA9  bl          0x81368FCC              ; __VIInit
81300528  48068C8D  bl          0x813691B4              ; VIInit
8130052C  3C600B00  lis         r3, 0x0B00
81300530  4805EBA1  bl          0x8135F0D0              ; AD16WriteReg
81300534  3C8009A8  lis         r4, 0x09A8
81300538  3C601CF8  lis         r3, 0x1CF8
8130053C  38A4EC80  subi        r5, r4, 0x1380
81300540  3C808000  lis         r4, 0x8000
81300544  3803C580  subi        r0, r3, 0x3A80
81300548  90A400F8  stw         r5, 248 (r4)            ; __OSBusClock = 162000000
8130054C  900400FC  stw         r0, 252 (r4)            ; __OSCoreClock = 486000000
81300550  48001AE5  bl          0x81302034 
81300554  48001B75  bl          0x813020C8 
81300558  38600005  li          r3, 5
8130055C  4806B579  bl          0x8136BAD4              ; PADSetSpec
81300560  4806AF01  bl          0x8136B460              ; PADInit
81300564  3C600C00  lis         r3, 0x0C00
81300568  4805EB69  bl          0x8135F0D0              ; AD16WriteReg
8130056C  48000A89  bl          0x81300FF4              ; BS2Main
81300570  3C608138  lis         r3, 0x8138
81300574  4CC63182  crclr       6
81300578  38A30DE0  addi        r5, r3, 0x0DE0
8130057C  386D8000  subi        r3, r13, -0x8000
81300580  388000A3  li          r4, 163
81300584  4805D421  bl          0x8135D9A4              ; OSPanic
81300588  8001000C  lwz         r0, 12 (r1)
8130058C  38210008  addi        r1, r1, 8
81300590  7C0803A6  mtlr        r0
81300594  4E800020  blr
```

```
void main ()
{
    BS2Init ();
    OSInit ();
    AD16Init ();
    AD16WriteReg (0x08000000);

    DVDInit ();
    AD16WriteReg (0x09000000);

    CARDInit ();
    AD16WriteReg (0x0A000000);

    sub_81301F0C ();
    __VIInit (VI_TVMODE_PAL_INT);
    VIInit ();
    AD16WriteReg (0x0B000000);

    __OSBusClock = 162000000;
    __OSCoreClock = 486000000;
    sub_81302034 ();
    sub_813020C8 ();
    PADSetSpec (PAD_SPEC_5);
    PADInit ();
    AD16WriteReg (0x0C000000);

    BS2Main ();
    OSHalt ("BS2 ERROR >>> SHOULD NEVER REACH HERE");
}
```

## BS2Init ##

```
8130045C  7C0802A6  mflr        r0
81300460  90010004  stw         r0, 4 (r1)
81300464  9421FFE8  stwu        r1, -24 (r1)
81300468  93E10014  stw         r31, 20 (r1)
8130046C  3C608000  lis         r3, 0x8000
81300470  38800000  li          r4, 0
81300474  38A00100  li          r5, 256
81300478  4BFFFD51  bl          0x813001C8              ; memset
8130047C  3FE08000  lis         r31, 0x8000
81300480  387F3000  addi        r3, r31, 0x3000
81300484  38800000  li          r4, 0
81300488  38A00100  li          r5, 256
8130048C  4BFFFD3D  bl          0x813001C8              ; memset
81300490  4BFFFF11  bl          0x813003A0              ; ClearUnusedBatRegisters
81300494  3C000180  lis         r0, 0x0180
81300498  901F0028  stw         r0, 40 (r31)
8130049C  38000001  li          r0, 1
813004A0  3C60CC00  lis         r3, 0xCC00
813004A4  901F002C  stw         r0, 44 (r31)
813004A8  38633000  addi        r3, r3, 0x3000
813004AC  3800FFFF  li          r0, -1
813004B0  8063002C  lwz         r3, 44 (r3)
813004B4  809F002C  lwz         r4, 44 (r31)
813004B8  54630006  rlwinm      r3, r3, 0, 0, 3         mask:0xF0000000
813004BC  5463273E  rlwinm      r3, r3, 4, 28, 31           mask:0x0000000F
813004C0  7C641A14  add         r3, r4, r3
813004C4  907F002C  stw         r3, 44 (r31)
813004C8  900D8908  stw         r0, -0x76F8 (r13)
813004CC  4BFFFF0D  bl          0x813003D8              ; ClearFPURegisters
813004D0  8001001C  lwz         r0, 28 (r1)
813004D4  83E10014  lwz         r31, 20 (r1)
813004D8  38210018  addi        r1, r1, 24
813004DC  7C0803A6  mtlr        r0
813004E0  4E800020  blr
```

```
void BS2Init (void)
{
    // Clear OS low memory.
    memset ( 0x80000000, 0, 256 );
    memset ( 0x80003000, 0, 256 );

    ClearUnusedBatRegisters ();
    __OSPhysMemSize = 24*1024*1024;
    __OSConsoleType = OS_CONSOLE_RETAIL1;
    __OSConsoleType += ([0xCC00302C] >> 28) & 0xF;
    var_814AD8C8 = 0xFFFFFFFF;

    ClearFPURegisters ();
}
```

## ClearUnusedBatRegisters ##

```
813003A0  4C00012C  isync       
813003A4  38800000  li          r4, 0
813003A8  7C9D83A6  mtspr       DBAT2L, r4
813003AC  7C9C83A6  mtspr       DBAT2U, r4
813003B0  7C9F83A6  mtspr       DBAT3L, r4
813003B4  7C9E83A6  mtspr       DBAT3U, r4
813003B8  7C9383A6  mtspr       IBAT1L, r4
813003BC  7C9283A6  mtspr       IBAT1U, r4
813003C0  7C9583A6  mtspr       IBAT2L, r4
813003C4  7C9483A6  mtspr       IBAT2U, r4
813003C8  7C9783A6  mtspr       IBAT3L, r4
813003CC  7C9683A6  mtspr       IBAT3U, r4
813003D0  4C00012C  isync       
813003D4  4E800020  blr
```

## ClearFPURegisters ##

```
813003D8  C80D82A0  lfd         fr0, -0x7D60 (r13)           ; 0.0f
813003DC  FC200090  fmr         fr1, fr0
813003E0  FC400090  fmr         fr2, fr0
813003E4  FC600090  fmr         fr3, fr0
813003E8  FC800090  fmr         fr4, fr0
813003EC  FCA00090  fmr         fr5, fr0
813003F0  FCC00090  fmr         fr6, fr0
813003F4  FCE00090  fmr         fr7, fr0
813003F8  FD000090  fmr         fr8, fr0
813003FC  FD200090  fmr         fr9, fr0
81300400  FD400090  fmr         fr10, fr0
81300404  FD600090  fmr         fr11, fr0
81300408  FD800090  fmr         fr12, fr0
8130040C  FDA00090  fmr         fr13, fr0
81300410  FDC00090  fmr         fr14, fr0
81300414  FDE00090  fmr         fr15, fr0
81300418  FE000090  fmr         fr16, fr0
8130041C  FE200090  fmr         fr17, fr0
81300420  FE400090  fmr         fr18, fr0
81300424  FE600090  fmr         fr19, fr0
81300428  FE800090  fmr         fr20, fr0
8130042C  FEA00090  fmr         fr21, fr0
81300430  FEC00090  fmr         fr22, fr0
81300434  FEE00090  fmr         fr23, fr0
81300438  FF000090  fmr         fr24, fr0
8130043C  FF200090  fmr         fr25, fr0
81300440  FF400090  fmr         fr26, fr0
81300444  FF600090  fmr         fr27, fr0
81300448  FF800090  fmr         fr28, fr0
8130044C  FFA00090  fmr         fr29, fr0
81300450  FFC00090  fmr         fr30, fr0
81300454  FFE00090  fmr         fr31, fr0
81300458  4E800020  blr
```