
```
# Brief runflow of boot stage execution:
#
#   - Init Flipper (ARAM, MI, reset DVD).
#   - Init Gekko (enable cache, set Dolphin OS memory model, enable translation).
#   - Probe AD16.
#   - Test memory. Halt CPU, if test failed.
#   - Load IPL into main memory and disable bootrom scrambler.
#   - Jump to IPL's __start.

.text
.org 0              # Will be actually 0xFFF0_0100 later.

boot:

# This will initialize Gekko implementation specifics.
        lis         r4, 0x0011          # XXX: This must be proved somehow..
        addi        r4, r4, 0x0C64
        mtspr       HID0, r4

# This will initialize CPU program model.        
        lis         r4, 0x0000
        addi        r4, r4, 0x2000
        mtmsr       r4

# Initialize auxiliary memory (ARAM).        
        lis         r4, 0x0C00
        addi        r4, r4, 0x5012
        li          r5, 67
        sth         r5, 0 (r4)          # Set 0x5012 register value to 0x43.
        li          r5, 156
        sth         r5, 8 (r4)          # Set ARAM refresh value.        

# Initialize Flipper memory interface.
        lis         r3, 0x0C00
        ori         r3, r3, 0x4000
        li          r4, 64
        sth         r4, 38 (r3)         # Set 0x4026 register value to 0x40.
        nop
        nop

# Enable data and instruction cache.
        mfspr       r3, HID0
        ori         r4, r3, 0xC000
        mtspr       HID0, r4
        nop
        nop
        nop
        isync

# Initialize CPU memory model. Clear BATs and segment registers.        
        li          r4, 0
        mtspr       DBAT0U, r4
        mtspr       DBAT1U, r4
        mtspr       DBAT2U, r4
        mtspr       DBAT3U, r4
        mtspr       IBAT0U, r4
        mtspr       IBAT1U, r4
        mtspr       IBAT2U, r4
        mtspr       IBAT3U, r4
        isync
        lis         r4, 0x8000
        addi        r4, r4, 0
        mtsr        0, r4
        mtsr        1, r4
        mtsr        2, r4
        mtsr        3, r4
        mtsr        4, r4
        mtsr        5, r4
        mtsr        6, r4
        mtsr        7, r4
        mtsr        8, r4
        mtsr        9, r4
        mtsr        10, r4
        mtsr        11, r4
        mtsr        12, r4
        mtsr        13, r4
        mtsr        14, r4
        mtsr        15, r4

# Configure memory model:
#
# DBAT0: 80001FFF 00000002    Write-back cached main memory, 256MB block.
# DBAT1: C0001FFF 0000002A    Write-through cached main memory, 256MB block.
# DBAT2: 00000000 xxxxxxxx    Dont care, reserved.
# DBAT3: FFF0001F FFF00001    Bootrom, 1MB block (temporary for BS)
#
# IBAT0: 80001FFF 00000002    Write-back cached main memory, 256MB block.
# IBAT1: 00000000 xxxxxxxx    Dont care, reserved.
# IBAT2: 00000000 xxxxxxxx    Dont care, reserved.
# IBAT3: FFF0001F FFF00001    Bootrom, 1MB block (temporary for BS)
        lis         r4, 0x0000
        addi        r4, r4, 2
        lis         r3, 0x8000
        addi        r3, r3, 0x1FFF
        mtspr       DBAT0L, r4
        mtspr       DBAT0U, r3
        isync
        mtspr       IBAT0L, r4
        mtspr       IBAT0U, r3
        isync
        lis         r4, 0x0000
        addi        r4, r4, 42
        lis         r3, 0xC000
        addi        r3, r3, 0x1FFF
        mtspr       DBAT1L, r4
        mtspr       DBAT1U, r3
        isync
        lis         r4, 0xFFF0
        addi        r4, r4, 1
        lis         r3, 0xFFF0
        addi        r3, r3, 31
        mtspr       DBAT3L, r4
        mtspr       DBAT3U, r3
        isync
        mtspr       IBAT3L, r4
        mtspr       IBAT3U, r3
        isync

# Enable instruction and data translation.
        mfmsr       r4
        ori         r4, r4, 0x0030      # Enable address translation.
        mtmsr       r4
        isync

# Write 0x0245248A to 0x3030 register. Meaning is unknown. Register is unknown.
        lis         r3, 0xCC00
        ori         r3, r3, 0x3000
        lis         r4, 0x0245
        ori         r4, r4, 0x248A
        stw         r4, 48 (r3)         # Write 0x0245248A to 0x3030 register.

# Reset DVD, through PI reset register.
        lwz         r4, 36 (r3)         # Read PI reset register.
        ori         r4, r4, 0x0001
        rlwinm      r4, r4, 0, 31, 28   # Set bit 31, clear bit 29.
        stw         r4, 36 (r3)         # Write new value in reset register.
        mftb        r5, TBL
WaitDVDReset:        
        mftb        r6, TBL
        sub         r7, r6, r5
        cmplwi      r7, 4388
        blt+        WaitDVDReset        # Wait ~9 us (with 486MHz clock)
        ori         r4, r4, 0x0003      # Set bit 31, set bit 29.
        stw         r4, 36 (r3)         # Write new value in reset register.

# Allow 32MHz EXI clock setting by CPU.
        lis         r14, 0xCC00
        ori         r14, r14, 0x6400
        li          r4, 0
        stw         r4, 60 (r14)        # SI EXICLK[LOCK] = 1

# To probe AD16 we must read its EXI ID. It should be 0x04120000. Place it to R20.
        lis         r2, 0xCC00
        ori         r2, r2, 0x6800
        lis         r22, 0x0000
        ori         r22, r22, 0x00BA
        li          r8, 1
        li          r10, 0
        lis         r21, 0x0412
        ori         r21, r21, 0x0000
        lis         r3, 0x0000
        ori         r3, r3, 0x0000
        lis         r7, 0x0000
        ori         r7, r7, 0x0015
        stw         r3, 56 (r2)         # EXI2 DATA = 0 (Get ID command)
        stw         r22, 40 (r2)        # Select AD16, through EXI2 CSR.
        lwz         r16, 40 (r2)
        stw         r7, 52 (r2)         # Write immediate 2 bytes from DATA.
WaitAd16_0:
        lwz         r16, 52 (r2)        # |
        and.        r16, r16, r8        # | Wait until transfer complete.
        bgt+        WaitAd16_0          # |
        lis         r7, 0x0000
        ori         r7, r7, 0x0031
        stw         r7, 52 (r2)         # Read immediate 4 bytes to DATA (ID).
WaitAd16_1:        
        lwz         r16, 52 (r2)        # |
        and.        r16, r16, r8        # | Wait until transfer complete.
        bgt+        WaitAd16_1          # |
        stw         r10, 40 (r2)        # Deselect device.
        lwz         r16, 40 (r2)        # Read EXI2 CSR twice. Why? No idea.
        lwz         r16, 40 (r2)        # Maybe its deselect attribute..
        lwz         r20, 56 (r2)        # r20 = DATA. It should contain ID.
        b           Jump_0
        .word       0
        .word       0

# Write "trace step" value to AD16. Only when probe was success (R20 = AD16 ID).
# Input value (trace step) must be in R15.        
Jump_0:
        b           Jump_1
DoAD16Write:
        lis         r3, 0xA000
        ori         r3, r3, 0x0000
        lis         r7, 0x0000
        ori         r7, r7, 0x0005
        stw         r3, 56 (r2)         # EXI2 DATA = 0xA0000000 (Write AD16 command)
        stw         r22, 40 (r2)        # Select AD16, through EXI2 CSR.
        lwz         r16, 40 (r2)
        b           AD16Write_0  
Jump_1:
        b           Jump_2
AD16Write_0:        
        stw         r7, 52 (r2)         # Write immediate 1 byte from DATA.
WaitAd16Write_0:
        lwz         r16, 52 (r2)        # |
        and.        r16, r16, r8        # | Wait until transfer complete.
        bgt+        WaitAd16Write_0     # |
        nop
        nop
        b           AD16Write_1  
Jump_2:        
        b           Jump_3
AD16Write_1:        
        lis         r7, 0x0000
        ori         r7, r7, 0x0035
        stw         r15, 56 (r2)        # EXI2 DATA = trace step
        stw         r7, 52 (r2)         # Write immediate 4 bytes from DATA.
WaitAd16Write_1:        
        lwz         r16, 52 (r2)        # |
        and.        r16, r16, r8        # | Wait until transfer complete.
        b           AD16Write_2         # |
Jump_3:        
        b           Jump_4
AD16Write_2:        
        bgt+        WaitAd16Write_1
        stw         r10, 40 (r2)        # Deselect device.
        lwz         r16, 40 (r2)
        lwz         r16, 40 (r2)
        blr
        .word       0
Jump_4:        
        b           Trace_01  
AD16Write:        
        cmplw       r20, r21            # If AD16 probe failed, then skip.
        beq+        DoAD16Write  
        blr

# Trace step 0x01 - Nothing ?
Trace_01:
        lis         r15, 0x0100         # AD16 = 0x01000000
        bl          AD16Write
        nop
        nop
        nop
        nop
        nop
        nop
        nop
        b           Trace_02
        .word       0
        .word       0

# Trace step 0x02 - Nothing ?
Trace_02:
        nop
        nop
        nop
        nop
        nop
        lis         r15, 0x0200         # AD16 = 0x02000000
        bl          AD16Write
        b           Jump_5

# Memory self test with given pattern.
Jump_5:
        b           Jump_6
TestMem:
        nop
        nop
        nop
        nop
        nop
        mr          r23, r25
        lis         r24, 0x0180         # Main memory size (24MB)
        b           TestMem_0
Jump_6:
        b           Jump_7
TestMem_0:        
        rlwinm      r24, r24, 27, 5, 31 # Fill memory (by 32 byte portions).
        mtctr       r24
FillMem:
        stw         r26, 0 (r23)
        stw         r26, 4 (r23)
        stw         r26, 8 (r23)
        stw         r26, 12 (r23)
        b           TestMem_1
Jump_7:
        b           Jump_8
TestMem_1:
        stw         r26, 16 (r23)
        stw         r26, 20 (r23)
        stw         r26, 24 (r23)
        stw         r26, 28 (r23)
        addi        r23, r23, 32
        bdnz+       FillMem
        b           TestMem_2
Jump_8:
        b           Jump_9
TestMem_2:        
        mr          r23, r25
        lis         r24, 0x0180
        rlwinm      r24, r24, 30, 2, 31
        mtctr       r24
TestLoop:
        lwz         r15, 0 (r23)        # Begin to test.
        cmplw       r15, r26
        b           TestMem_3
Jump_9:
        b           Jump_10
TestMem_3:        
        beq-        NextIteration
        rlwinm      r15, r23, 14, 18, 31
        andi.       r15, r15, 0x001F
        li          r16, 1
        slw         r16, r16, r15
        or          r17, r17, r16
        b           TestMem_4
Jump_10:
        b           Jump_11
TestMem_4:
        rlwinm      r15, r23, 14, 18, 31
        subi        r15, r15, 32
        andi.       r15, r15, 0x001F
        li          r16, 1
        slw         r16, r16, r15
        or          r18, r18, r16
        b           TestMem_5
Jump_11:
        b           Jump_12
TestMem_5:
        rlwinm      r15, r23, 14, 18, 31
        subi        r15, r15, 64
        andi.       r15, r15, 0x001F
        li          r16, 1
        slw         r16, r16, r15
        or          r19, r19, r16
        b           TestMem_6
Jump_12:
        b           Jump_13
TestMem_6:
        rlwinm      r15, r23, 0, 28, 31
        cmplwi      r15, 8
        bge-        ReadErrorBank0
ReadErrorBank0:        
        addi        r28, r28, 1
        b           NextIteration
ReadErrorBank1:
        addi        r27, r27, 1
        b           TestMem_7
Jump_13:
        b           Trace_03
TestMem_7:
        cmplw       r29, r23
        bge-        NextIteration
        mr          r29, r23
NextIteration:
        addi        r23, r23, 4
        bdnz+       TestLoop
        blr

# Clear registers for memory test (see next). Trace step 0x03 - Nothing ?
Trace_03:
        li          r17, 0
        li          r18, 0
        li          r19, 0
        li          r27, 0
        li          r28, 0
        li          r29, 0
        lis         r15, 0x0300         # AD16 = 0x03000000
        bl          AD16Write
        b           Jump_14
        .word       0
        .word       0
        .word       0
        .word       0
        .word       0
        .word       0
        .word       0

# Test memory by some patterns.
Jump_14:
        b           Jump_15
MemorySelfTest:
        lis         r25, 0x8000
        lis         r26, 0xAAAA
        ori         r26, r26, 0xAAAA
        bl          TestMem             # Test memory with 0xAA pattern.
        not         r26, r26
        bl          TestMem             # Test memory with 0x55 pattern.
        nop
        b           MemorySelfTest_0
Jump_15:
        b           Jump_16
MemorySelfTest_0:
        lis         r15, 0x0400
        mr.         r16, r27
        beq-        ReadError_8_F
ReadError_0_7:        
        oris        r15, r15, 0x0200
ReadError_8_F:
        mr.         r16, r28
        beq-        MemorySelfTestWriteAd
        b           MemorySelfTest_1
Jump_16:
        b           Jump_17
MemorySelfTest_1:
        oris        r15, r15, 0x0100
MemorySelfTestWriteAd:
        rlwinm      r29, r29, 30, 2, 31
        or          r15, r15, r29
        bl          AD16Write           # Set AD16 value.
        nop
        nop
        b           Halt

# Halt execution if memory test failed.
Jump_17:
        cmplw       r20, r21
        beq+        MemorySelfTest
Halt:
        mr.         r16, r27            # Bad address with last digit 8-F ?
        bne+        Halt
        mr.         r16, r28            # Bad address with last digit 0-7 ?
        bne+        Halt

# Prepare GPR registers for IPL loading.
        lis         r2, 0xCC00          # EXI registers base
        ori         r2, r2, 0x6800
        lis         r6, 0x0000          # EXI0 CSR setup: device 1, 32MHz
        ori         r6, r6, 0x0150
        lis         r7, 0x0000
        ori         r7, r7, 0x0035
        li          r8, 1
        lis         r9, 0x0000
        ori         r9, r9, 0x0003
        li          r10, 0
        lis         r11, 0x0000         # Max length of single transfer
        ori         r11, r11, 0x0400
        lis         r12, 0x0001
        ori         r12, r12, 0x0000
        lis         r3, 0x0002          # Bootrom starting offset (0x800)
        ori         r3, r3, 0x0000
        lis         r4, 0x012F          # Main memory starting address
        ori         r4, r4, 0xFFE0
        lis         r13, 0x0017         # Transfer length
        ori         r13, r13, 0x0000
        b           Jump_18
        .word       0
        .word       0
        .word       0
        .word       0
Jump_18:
        b           Jump_19

# Transfer IPL to main memory. Bootrom scrambler is decrypting data on the fly.
# Starting memory address: 0x012FFFE0.
# Starting bootrom offset: 0x800.
# Common length of transfer: 0x170000 bytes.
TransferIPL:
        cmpwi       r13, 0              # All bytes transferred ?
        beq-        SetupEntrypoint
        mr          r5, r11
        cmplw       r13, r5
        bgt-        SelectEXI
        mr          r5, r13
SelectEXI:
        stw         r6, 0 (r2)          # Select bootrom, through EXI0 CSR.
        b           TransferIPL_0
Jump_19:
        b           Jump_20
TransferIPL_0:
        stw         r3, 16 (r2)         # EXI0 DATA - offset in bootrom + write command
        lwz         r16, 0 (r2) 
        stw         r7, 12 (r2)         # Write immediate 4 bytes from DATA.
WaitTransferIPL_0:
        lwz         r16, 12 (r2)        # |
        and.        r16, r16, r8        # | Wait until transfer complete.
        bgt+        WaitTransferIPL_0   # |
        b           TransferIPL_1
Jump_20:
        b           Jump_21
TransferIPL_1:
        stw         r4, 4 (r2)          # EXI0 MAR - DMA memory address.
        lwz         r4, 4 (r2) 
        stw         r5, 8 (r2)          # EXI0 LEN - DMA transfer length.
        lwz         r5, 8 (r2) 
        stw         r9, 12 (r2)         # Start EXI0 DMA write transfer.
WaitTransferIPL_1:
        lwz         r16, 12 (r2)
        b           TransferIPL_2
Jump_21:
        b           Jump_22
TransferIPL_2:
        and.        r16, r16, r8        # | Wait until transfer complete.
        bgt+        WaitTransferIPL_1   # |
        stw         r10, 0 (r2)         # Deselect device.
        lwz         r16, 0 (r2)
        lwz         r16, 0 (r2)
        add         r3, r3, r12
        b           TransferIPL_3
Jump_22:
        b           Jump_23
TransferIPL_3:
        add         r4, r4, r11         # Advance pointers.
        sub         r13, r13, r5
        b           TransferIPL

# Set link register to IPL entrypoint.
SetupEntrypoint:
        lis         r4, 0x8130
        ori         r4, r4, 0x0000
        mtlr        r4                  # LR = 0x81300000
        b           DisableScrambler

# Disable bootrom decryption logic and disallow 32MHz EXI clock setting by CPU.        
Jump_23:
        b           Jump_24
DisableScrambler:
        lis         r6, 0x0000
        ori         r6, r6, 0x2000
        stw         r6, 0 (r2)          # Set ROMDIS bit in EXI0 CSR.
        li          r4, 1
        stw         r4, 60 (r14)        # SI EXICLK[LOCK] = 1
        lwz         r4, 60 (r14)
        b           StartExecuteIPL
Jump_24:
        b           Jump_25

# Clear OS pointer to DVD BI2 location. Jump to IPL entrypoint.
StartExecuteIPL:
        lis         r4, 0x8000
        li          r3, 0
        stw         r3, 0x00F4 (r4)
        blr                             # !! IPL START TO EXECUTE !!
        .word       0
        .word       0

# This how may look BS from the scratch.. Actual code fills zeroed words.
Padding:
Jump_25:
        b           Jump_26
        .word       0
        .word       0
        .word       0
        .word       0
        .word       0
        .word       0
        .word       0
Jump_26:
        b           Jump_27
        .word       0
        .word       0
        .word       0
        .word       0
        .word       0
        .word       0
        .word       0
Jump_27:        
        b           Jump_28
        .word       0
        .word       0
        .word       0
        .word       0
        .word       0
        .word       0
        .word       0
Jump_28:        
        b           Jump_29
        .word       0
        .word       0
        .word       0
        .word       0
        .word       0
        .word       0
        .word       0
Jump_29:        
        b           Jump_30
        .word       0
        .word       0
        .word       0
        .word       0
        .word       0
        .word       0
        .word       0
Jump_30:        
        b           Jump_31
        .word       0
        .word       0
        .word       0
        .word       0
        .word       0
        .word       0
        .word       0
Jump_31:        
        b           TransferIPL
        .word       0
        .word       0
        .word       0
        .word       0
        .word       0
        .word       0
        .word       0

# Note by tmbinc:
# Why BS is jumping around? The jumping is because the way the instructions are
# fetched. They must be fetched in exact linear order, otherwise the scrambling
# goes out of sync. So in order to do any loops, BS enable the icache and 
# to fill the icache, its jump to the first location in each icache line.
# That's why BS jump in 0x20 byte steps.

```