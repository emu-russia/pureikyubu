# Disassembled GameCube NTSC 1.0 BIOS JAudio ucode
# DspUcode_1280.bin

EDIT: This disassembly got screwed because Duddie has incorrect register index:
- 0x19: ax0.h  -> Should be ax1.l
- 0x1A: ax1.l  -> Should be ax0.h


## Overview

- Command exchanges are synchronous using Mailbox polling. DSP interrupt is not used.
- Looks easier than AX Ucode
- IROM is called only in one place (it looks like it is Sample Rate Converter). DSP Coefficients not used (DROM 0x1000)

Note for emulator authors (for me): 
JAudio (Zelda) microcodes should not block application execution. Since they are designed in such a way that the command exchange is performed by the Mailbox polling - an error in the execution of the microcode followed by the DSP shutdown causes the JAudio DspSendCommand method to freeze in the endless Mailbox poll, and the application also freezes.

## Memory usage

## Interrupts

All interrupt handlers except #0 are stubs.

```
0000 02 9F 00 10 	j    	$0x0010
0002 00 00       	nop  	
0003 00 00       	nop  	
0004 02 FF       	rti
0005 00 00       	nop  	
0006 02 FF       	rti
0007 00 00       	nop  	
0008 02 FF       	rti
0009 00 00       	nop  	
000A 02 FF       	rti
000B 00 00       	nop  	
000C 02 FF       	rti
000D 00 00       	nop  	
000E 02 FF       	rti
000F 00 00       	nop  	
```

## Start

```
0010 13 02       	sbclr	8
0011 13 03       	sbclr	9
0012 12 04       	sbset	10
0013 13 05       	sbclr	11
0014 13 06       	sbclr	12
0015 8E 00       	clr40	                	     	
0016 8C 00       	clr15	                	     	
0017 8B 00       	m0   	                	     	
0018 00 9E FF FF 	lri  	ac0.m, #0xFFFF
001A 1D 1E       	mrr  	lm0, ac0.m
001B 1D 3E       	mrr  	lm1, ac0.m
001C 1D 5E       	mrr  	lm2, ac0.m
001D 1D 7E       	mrr  	lm3, ac0.m
001E 00 92 00 FF 	lri  	bank, #0x00FF
0020 81 00       	clr  	ac0             	     	
0021 00 9F 10 00 	lri  	ac1.m, #0x1000
0023 00 80 00 00 	lri  	ar0, #0x0000
0025 00 5F       	loop 	ac1.m
0026 1B 1E       	srri 	@ar0, ac0.m
0027 26 FF       	lrs  	ac0.m, $(CMBL)
0028 16 FC 88 88 	si   	$(DMBH), #0x8888
002A 16 FD 11 11 	si   	$(DMBL), #0x1111
002C 26 FC       	lrs  	ac0.m, $(DMBH) 					// Wait message read by CPU
002D 02 A0 80 00 	tclr 	ac0.m, #0x8000
002F 02 9C 00 2C 	jnok 	$0x002C
```

```c++
Start()			// 0010
{
	sr.hwz = 0;
	sr.ie = 0;			// Interrupt disable
	sr.unk10 = 1;
	sr.eie = 0;			// External interrupt disable
	sr.unk12 = 0;

	clr40();
	clr15();
	m0();

	lm0 = lm1 = lm2 = lm3 = 0xFFFF;
	bank = 0xFF; 		// BankReg at 0xFF00 (IFX)

	// Clear DRAM (8 KBytes)

	ac1m = 0x1000;
	ar0 = 0;
	while (ac1m--)
	{
		*ar0++ = 0;
	}

	ac0m = CMBL; 		// Discard unreplied CPU message

	DMBH = 0x8888;		// "Anyone?" message from ucode
	DMBL = 0x1111;

	// Wait message read by CPU
	while ( (DMBH & 0x8000) != 0 ) ;
}
```

## MainLoop

```
0031 81 00       	clr  	ac0             	     	
0032 89 00       	clr  	ac1             	     	
0033 26 FE       	lrs  	ac0.m, $(CMBH)
0034 02 C0 80 00 	tset 	ac0.m, #0x8000
0036 02 9C 00 31 	jnok 	$0x0031
0038 27 FF       	lrs  	ac1.m, $(CMBL)
0039 00 FF 03 45 	sr   	$0x0345, ac1.m
003B 1F FE       	mrr  	ac1.m, ac0.m
003C 03 40 00 FF 	andi 	ac1.m, #0x00FF
003E 00 FF 03 44 	sr   	$0x0344, ac1.m
0040 14 79       	lsr  	ac0, -7
0041 02 40 00 7E 	andi 	ac0.m, #0x007E
0043 02 00 00 62 	addi 	ac0.m, #0x0062 				// Switch
0045 00 FE 03 43 	sr   	$0x0343, ac0.m
0047 1C 1E       	mrr  	ar0, ac0.m
0048 17 0F       	jmpr 	ar0
```

```c++
void MainLoop() 		// 0031
{
	// Wait CPU message
	ac0 = ac1 = 0;
	while ( ((ac0m = CMBH) & 0x8000) == 0 );     // CMBH: | 10[cc cccc] [pppp pppp] |

	*(0x0345) = CMBL;

	ac1m = ac0m & 0xFF;
	*(0x0344) = ac1m;	 			// pp (Lower 8 bits of CMBH)

	ac0m = 0x0062 + (ac0m >> 7) & 0x7E; 		// Case (cc). Shift with a mask automatically multiply case by 2 for the correct jump
	*(0x0343) = ac0m; 		// Jump Table 1 case address

	jmp  ac0m; 			// Switch-case
}
```

## Case 0 - Nothing, reply OK

```
0049 00 9E 80 00 	lri  	ac0.m, #0x8000
004B 00 DC 03 43 	lr   	ac0.l, $0x0343
004D 02 BF 00 5A 	call 	$0x005A 				// WriteDspMailbox (0x80000000 | *(uint16_t *)0x343)
004F 02 9F 00 31 	j    	$0x0031				// MainLoop
```

## ReadCpuMailbox

```
0051 26 FE       	lrs  	ac0.m, $(CMBH)
0052 02 C0 80 00 	tset 	ac0.m, #0x8000
0054 02 9C 00 51 	jnok 	$0x0051
0056 24 FF       	lrs  	ac0.l, $(CMBL)
0057 1B 1E       	srri 	@ar0, ac0.m
0058 1B 1C       	srri 	@ar0, ac0.l
0059 02 DF       	ret  
```

```c++
uint16_t *ReadCpuMailbox (uint16_t * ar0)
{
	while ( ((ac0m = CMBH) & 0x8000) == 0 ) ; 				// Wait message
	ac0l = CMBL;
	*ar0++ = ac0m; 		// High
	*ar0++ = ac0l;		// Low
	return ar0;
}
```

## WriteDspMailbox

```
005A 2E FC       	srs  	$(DMBH), ac0.m
005B 2C FD       	srs  	$(DMBL), ac0.l
005C 26 FC       	lrs  	ac0.m, $(DMBH) 					// Wait read by CPU
005D 02 A0 80 00 	tclr 	ac0.m, #0x8000
005F 02 9C 00 5C 	jnok 	$0x005C
0061 02 DF       	ret  	
```

## Jump Table 1

```
0062 02 9F 00 49 	j    	$0x0049 			// 0: Nothing, reply OK
0064 02 9F 02 BD 	j    	$0x02BD 			// 1: Load VPB
0066 02 9F 04 70 	j    	$0x0470 			// 2: Process
0068 02 9F 00 31 	j    	$0x0031 			// 3: Another command (goto MainLoop)
006A 02 9F 00 DF 	j    	$0x00DF  			// 4: DmemLoad (MainMem -> DMEM)
006C 02 9F 00 F1 	j    	$0x00F1 			// 5: DmemSave (DMEM -> MainMem)
006E 02 9F 05 BB 	j    	$0x05BB 			// 6: ReadARAM
0070 02 9F 05 6F 	j    	$0x056F 			// 7: Memcpy (Make memcpy in main memory)
0072 02 9F 05 D7 	j    	$0x05D7 			// 8: 
0074 02 9F 05 9F 	j    	$0x059F 			// 9: SmallMemcpy
0076 02 9F 07 41 	j    	$0x0741  			// 10: 
0078 02 9F 06 18 	j    	$0x0618 			// 11: Call_IROM_SRCPossible
007A 02 9F 02 03 	j    	$0x0203  			// 12: Tone Generators (?)
```

## ----------------------------------------------------------------------------------------------------------------

## DSP DMA

### DspReadMainMemByPtr

ar1 - pointer to main memory address
ac1m - Dsp address
ar0 - Num word (bytes * 2)s

```
007C 19 3E       	lrri 	ac0.m, @ar1 				// ar1 - pointer to MainMem Addr
007D 19 3C       	lrri 	ac0.l, @ar1
```

### DspReadMainMem (ac0 - MainMem address)

```
007E 2F CD       	srs  	$(DSPA), ac1.m 				// DSP Addr = ac1m
007F 0F 00       	lris 	ac1.m, 0 					// Mode 0: MainMem -> Dmem
0080 2F C9       	srs  	$(DSCR), ac1.m 				
0081 2E CE       	srs  	$(DSMAH), ac0.m 			// MainMemAddr = (ac0m << 16) | ac0l
0082 2C CF       	srs  	$(DSMAL), ac0.l
0083 1F E0       	mrr  	ac1.m, ar0 					// ar0 - Num words
0084 15 01       	lsl  	ac1, #0x01 					// ac1 = numWords * 2
0085 2F CB       	srs  	$(DSBL), ac1.m 				// BlockSize = numWords (ar0) * 2
0086 02 BF 00 8F 	call 	$0x008F 					// WaitDspDma2
0088 02 DF       	ret  	
```

### DspWriteMainMemByPtr

```
0089 19 3E       	lrri 	ac0.m, @ar1 				// ar1 - pointer to MainMem Addr
008A 19 3C       	lrri 	ac0.l, @ar1
``

### DspWriteMainMem (ac0: MainMemAddr)

``
008B 2F CD       	srs  	$(DSPA), ac1.m 				// DSP Addr = ac1m
008C 0F 01       	lris 	ac1.m, 1 					// Mode 1: Dmem -> MainMem
008D 02 9F 00 80 	j    	$0x0080
```

### WaitDspDma2

Used internally by DspReadMainMem and DspWriteMainMem.

```
008F 26 C9       	lrs  	ac0.m, $(DSCR)
0090 02 A0 00 04 	tclr 	ac0.m, #0x0004
0092 02 9C 00 8F 	jnok 	$0x008F
0094 02 DF       	ret  	
```

## DspReadMainMemNoWait

Start Dma and continue background job.

```
0095 19 3E       	lrri 	ac0.m, @ar1 			// ar1 - pointer to MainMem Addr
0096 19 3C       	lrri 	ac0.l, @ar1
0097 00 FF FF CD 	sr   	$(DSPA), ac1.m 					// DSP Addr = ac1m
0099 0F 00       	lris 	ac1.m, 0
009A 00 FF FF C9 	sr   	$(DSCR), ac1.m 					// Mode 0: MainMem -> Dmem
009C 00 FE FF CE 	sr   	$(DSMAH), ac0.m
009E 00 FC FF CF 	sr   	$(DSMAL), ac0.l
00A0 1F E0       	mrr  	ac1.m, ar0
00A1 15 01       	lsl  	ac1, #0x01
00A2 00 FF FF CB 	sr   	$(DSBL), ac1.m 			// BlockSize = numWords (ar0) * 2
00A4 02 DF       	ret  	
```

## WaitDspDma

Wait until Dsp Dma completed.

```
00A5 00 DE FF C9 	lr   	ac0.m, $(DSCR)
00A7 02 A0 00 04 	tclr 	ac0.m, #0x0004
00A9 02 9C 00 A5 	jnok 	$0x00A5
00AB 02 DF       	ret  	
```

## ----------------------------------------------------------------------------------------------------------------

## Aram Interface

### AramReadByPtr (not used)

```
00AC 19 3E       	lrri 	ac0.m, @ar1
00AD 19 3C       	lrri 	ac0.l, @ar1
```

### AramRead

```
00AE 02 40 7F FF 	andi 	ac0.m, #0x7FFF
00B0 02 BF 00 BA 	call 	$0x00BA 					// SetAcceleratorAddress
00B2 00 7A 00 B8 	bloop	ax1.l, $0x00B8
	00B4 26 D3       	lrs  	ac0.m, $(ACDAT2)
	00B5 1B 3E       	srri 	@ar1, ac0.m
	00B6 00 00       	nop  	
	00B7 00 00       	nop  	
	00B8 00 00       	nop  	
00B9 02 DF       	ret  	
```

### SetAcceleratorAddress

```
00BA 1C 3F       	mrr  	ar1, ac1.m
00BB 00 9F 00 05 	lri  	ac1.m, #0x0005
00BD 2F D1       	srs  	$(ACFMT), ac1.m 			// AC Mode: RawByte
00BE 1F 5E       	mrr  	ax1.l, ac0.m
00BF 1F 1C       	mrr  	ax0.l, ac0.l
00C0 2E D4       	srs  	$(ACSAH), ac0.m
00C1 2C D5       	srs  	$(ACSAL), ac0.l
00C2 89 00       	clr  	ac1             	     	
00C3 1F A0       	mrr  	ac1.l, ar0
00C4 4C 00       	add  	ac0, ac1        	     	
00C5 02 00 00 30 	addi 	ac0.m, #0x0030
00C7 2E D6       	srs  	$(ACEAH), ac0.m
00C8 2C D7       	srs  	$(ACEAL), ac0.l
00C9 1F DA       	mrr  	ac0.m, ax1.l
00CA 1F 98       	mrr  	ac0.l, ax0.l
00CB 14 7F       	lsr  	ac0, -1
00CC 2E D8       	srs  	$(ACCAH), ac0.m
00CD 2C D9       	srs  	$(ACCAL), ac0.l
00CE 1F 40       	mrr  	ax1.l, ar0
00CF 02 DF       	ret  	
```

### AramWrite

```
00D0 19 3E       	lrri 	ac0.m, @ar1
00D1 19 3C       	lrri 	ac0.l, @ar1
00D2 02 60 80 00 	ori  	ac0.m, #0x8000
00D4 02 BF 00 BA 	call 	$0x00BA 					// SetAcceleratorAddress
00D6 00 7A 00 DD 	bloop	ax1.l, $0x00DD
	00D8 19 3E       	lrri 	ac0.m, @ar1
	00D9 2E D3       	srs  	$(ACDAT2), ac0.m
	00DA 00 00       	nop  	
	00DB 00 00       	nop  	
	00DC 00 00       	nop  	
	00DD 00 00       	nop  	
00DE 02 DF       	ret  	
```

## ----------------------------------------------------------------------------------------------------------------

## Case 4 - DmemLoad

```
00DF 00 80 03 46 	lri  	ar0, #0x0346 
00E1 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
00E3 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
00E5 00 81 03 46 	lri  	ar1, #0x0346
00E7 00 DF 03 49 	lr   	ac1.m, $0x0349
00E9 03 40 FF FF 	andi 	ac1.m, #0xFFFF
00EB 00 C0 03 45 	lr   	ar0, $0x0345
00ED 02 BF 00 7C 	call 	$0x007C 				// DspReadMainMemByPtr
00EF 02 9F 00 49 	j    	$0x0049
```

## Case 5 - DmemSave

```
00F1 00 80 03 46 	lri  	ar0, #0x0346
00F3 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
00F5 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
00F7 00 81 03 46 	lri  	ar1, #0x0346
00F9 00 DF 03 49 	lr   	ac1.m, $0x0349
00FB 03 40 FF FF 	andi 	ac1.m, #0xFFFF
00FD 00 C0 03 45 	lr   	ar0, $0x0345
00FF 02 BF 00 89 	call 	$0x0089 				// DspWriteMainMemByPtr
0101 02 9F 00 49 	j    	$0x0049
```

## DspAcc stuff

```
0103 00 92 00 FF 	lri  	bank, #0x00FF
0105 2F D1       	srs  	$(ACFMT), ac1.m
0106 03 40 00 03 	andi 	ac1.m, #0x0003
0108 1F 7F       	mrr  	ax1.h, ac1.m
0109 1F 5E       	mrr  	ax1.l, ac0.m
010A 1F 1C       	mrr  	ax0.l, ac0.l
010B 02 00 00 10 	addi 	ac0.m, #0x0010
010D 2E D4       	srs  	$(ACSAH), ac0.m
010E 2C D5       	srs  	$(ACSAL), ac0.l
010F 89 00       	clr  	ac1             	     	
0110 1F A0       	mrr  	ac1.l, ar0
0111 4C 00       	add  	ac0, ac1        	     	
0112 02 00 00 30 	addi 	ac0.m, #0x0030
0114 2E D6       	srs  	$(ACEAH), ac0.m
0115 2C D7       	srs  	$(ACEAL), ac0.l
0116 1F DA       	mrr  	ac0.m, ax1.l
0117 1F 98       	mrr  	ac0.l, ax0.l
0118 1F FB       	mrr  	ac1.m, ax1.h
0119 79 00       	decm 	ac1             	     	
011A 02 CA       	lsn  	
011B 2E D8       	srs  	$(ACCAH), ac0.m
011C 2C D9       	srs  	$(ACCAL), ac0.l
011D 02 DF       	ret  	



011E 1C 23       	mrr  	ar1, ar3
011F 19 7E       	lrri 	ac0.m, @ar3
0120 19 1B       	lrri 	ax1.h, @ar0
0121 D8 58       	mulc 	ac1.m, ax1.h    	l    	ax1.h, @ar0
0122 11 28 01 28 	bloopi	#0x28, $0x0128
	0124 DC D3       	mulcac	ac1.m, ax1.h, ac0	ldax 	ax1, @ar0
	0125 62 31       	movr 	ac0, ax0.h      	s    	@ar1, ac0.m
	0126 DC D3       	mulcac	ac1.m, ax1.h, ac0	ldax 	ax1, @ar0
	0127 62 31       	movr 	ac0, ax0.h      	s    	@ar1, ac0.m
	0128 49 00       	addax	ac1, ax0        	     	
0129 02 DF       	ret  	
```

## Very suspicious loop

Why is ORR doing here??

```
012A 8F 00       	set40	                	     	
012B 1C 03       	mrr  	ar0, ar3
012C 00 DB 03 8E 	lr   	ax1.h, $0x038E
012E 00 9A 00 04 	lri  	ax1.l, #0x0004
0130 19 78       	lrri 	ax0.l, @ar3
0131 A8 43       	mulx 	ax0.l, ax1.h    	l    	ax0.l, @ar3
0132 AE 00       	mulxmv	ax0.l, ax1.h, ac0	     	
0133 11 28 01 38 	bloopi	#0x28, $0x0138
	0135 38 C3       	orr  	ac0.m, ax0.h    	ldax 	ax0, @ar0
	0136 AE 30       	mulxmv	ax0.l, ax1.h, ac0	s    	@ar0, ac0.m
	0137 38 C3       	orr  	ac0.m, ax0.h    	ldax 	ax0, @ar0
	0138 AE 30       	mulxmv	ax0.l, ax1.h, ac0	s    	@ar0, ac0.m
0139 8E 00       	clr40	                	     	
013A 02 DF       	ret  	
```


```
013B 00 F9 03 61 	sr   	$0x0361, ax0.h
013D 1F C0       	mrr  	ac0.m, ar0
013E 02 00 FF FC 	addi 	ac0.m, #0xFFFC
0140 1C 1E       	mrr  	ar0, ac0.m
0141 1C 5E       	mrr  	ar2, ac0.m
0142 00 83 04 24 	lri  	ar3, #0x0424
0144 19 7E       	lrri 	ac0.m, @ar3
0145 19 7F       	lrri 	ac1.m, @ar3
0146 80 A2       	sl   	ac0.m, ax1.l
0147 64 A3       	movr 	ac0, ax1.l      	sl   	ac1.m, ax1.l
0148 65 30       	movr 	ac1, ax1.l      	s    	@ar0, ac0.m
0149 1B 1F       	srri 	@ar0, ac1.m
014A 1C 02       	mrr  	ar0, ar2
014B 81 00       	clr  	ac0             	     	
014C 00 DE 04 02 	lr   	ac0.m, $0x0402
014E 00 FE 03 62 	sr   	$0x0362, ac0.m
0150 14 74       	lsr  	ac0, -12
0151 1F 7E       	mrr  	ax1.h, ac0.m
0152 1F 3C       	mrr  	ax0.h, ac0.l
0153 89 00       	clr  	ac1             	     	
0154 00 DD 04 18 	lr   	ac1.l, $0x0418
0156 15 04       	lsl  	ac1, #0x04
0157 06 04       	cmpis	ac0.m, 4
0158 02 90 01 B0 	jge  	$0x01B0
015A 1F DD       	mrr  	ac0.m, ac1.l
015B 00 82 0C 00 	lri  	ar2, #0x0C00
015D 10 50       	loopi	#0x50
015E 4B 2A       	addax	ac1, ax1        	s    	@ar2, ac1.l
015F 1F BE       	mrr  	ac1.l, ac0.m
0160 00 FE 03 60 	sr   	$0x0360, ac0.m
0162 89 00       	clr  	ac1             	     	
0163 1F BE       	mrr  	ac1.l, ac0.m
0164 00 9A FF F8 	lri  	ax1.l, #0xFFF8
0166 00 9B 00 FC 	lri  	ax1.h, #0x00FC
0168 00 D8 03 61 	lr   	ax0.l, $0x0361
016A 00 82 0C 00 	lri  	ar2, #0x0C00
016C 00 83 0C 00 	lri  	ar3, #0x0C00
016E 19 5E       	lrri 	ac0.m, @ar2
016F 34 80       	andr 	ac0.m, ax0.h    	ls   	ax0.l, ac0.m
0170 11 28 01 75 	bloopi	#0x28, $0x0175
	0172 36 7A       	andr 	ac0.m, ax1.h    	l    	ac1.m, @ar2
	0173 35 B3       	andr 	ac1.m, ax0.h    	sl   	ac1.m, ax1.h
	0174 37 72       	andr 	ac1.m, ax1.h    	l    	ac0.m, @ar2
	0175 34 BB       	andr 	ac0.m, ax0.h    	slm  	ac1.m, ax1.h
0176 8A 00       	m2   	                	     	
0177 00 82 0C 00 	lri  	ar2, #0x0C00
0179 00 DD 04 18 	lr   	ac1.l, $0x0418
017B 15 04       	lsl  	ac1, #0x04
017C 1F E0       	mrr  	ac1.m, ar0
017D 81 00       	clr  	ac0             	     	
017E 00 DE 03 62 	lr   	ac0.m, $0x0362
0180 14 74       	lsr  	ac0, -12
0181 1F 7E       	mrr  	ax1.h, ac0.m
0182 1F 3C       	mrr  	ax0.h, ac0.l
0183 8F 00       	set40	                	     	
0184 19 43       	lrri 	ar3, @ar2
0185 4B C3       	addax	ac1, ax1        	ldax 	ax0, @ar0
0186 90 C3       	mul  	ax0.l, ax0.h    	ldax 	ax0, @ar0
0187 F2 C3       	madd 	ax0.l, ax0.h    	ldax 	ax0, @ar0
0188 F2 C3       	madd 	ax0.l, ax0.h    	ldax 	ax0, @ar0
0189 F2 00       	madd 	ax0.l, ax0.h    	     	
018A FE 00       	movpz	ac0             	     	
018B 1C 1F       	mrr  	ar0, ac1.m
018C 19 43       	lrri 	ar3, @ar2
018D 4B C3       	addax	ac1, ax1        	ldax 	ax0, @ar0
018E 90 C3       	mul  	ax0.l, ax0.h    	ldax 	ax0, @ar0
018F 11 4E 01 97 	bloopi	#0x4E, $0x0197
	0191 F2 C3       	madd 	ax0.l, ax0.h    	ldax 	ax0, @ar0
	0192 F2 C3       	madd 	ax0.l, ax0.h    	ldax 	ax0, @ar0
	0193 F2 31       	madd 	ax0.l, ax0.h    	s    	@ar1, ac0.m
	0194 1C 1F       	mrr  	ar0, ac1.m
	0195 19 43       	lrri 	ar3, @ar2
	0196 4B C3       	addax	ac1, ax1        	ldax 	ax0, @ar0
	0197 92 C3       	mulmvz	ax0.l, ax0.h, ac0	ldax 	ax0, @ar0
0198 F2 C3       	madd 	ax0.l, ax0.h    	ldax 	ax0, @ar0
0199 F2 C3       	madd 	ax0.l, ax0.h    	ldax 	ax0, @ar0
019A F2 31       	madd 	ax0.l, ax0.h    	s    	@ar1, ac0.m
019B FE 00       	movpz	ac0             	     	
019C 1B 3E       	srri 	@ar1, ac0.m
019D 8B 00       	m0   	                	     	
019E 8E 00       	clr40	                	     	
019F 00 FE 04 1B 	sr   	$0x041B, ac0.m
01A1 1C 1F       	mrr  	ar0, ac1.m
01A2 15 0C       	lsl  	ac1, #0x0C
01A3 03 40 0F FF 	andi 	ac1.m, #0x0FFF
01A5 00 FF 04 18 	sr   	$0x0418, ac1.m
01A7 00 83 04 24 	lri  	ar3, #0x0424
01A9 19 1E       	lrri 	ac0.m, @ar0
01AA 19 1F       	lrri 	ac1.m, @ar0
01AB 80 A0       	ls   	ax1.l, ac0.m
01AC 64 A1       	movr 	ac0, ax1.l      	ls   	ax1.l, ac1.m
01AD 65 33       	movr 	ac1, ax1.l      	s    	@ar3, ac0.m
01AE 1B 7F       	srri 	@ar3, ac1.m
01AF 02 DF       	ret  	
01B0 1F E0       	mrr  	ac1.m, ar0
01B1 1C 1F       	mrr  	ar0, ac1.m
01B2 11 28 01 B9 	bloopi	#0x28, $0x01B9
	01B4 4B 70       	addax	ac1, ax1        	l    	ac0.m, @ar0
	01B5 1B 3E       	srri 	@ar1, ac0.m
	01B6 1C 1F       	mrr  	ar0, ac1.m
	01B7 4B 70       	addax	ac1, ax1        	l    	ac0.m, @ar0
	01B8 1B 3E       	srri 	@ar1, ac0.m
	01B9 1C 1F       	mrr  	ar0, ac1.m
01BA 02 9F 01 9F 	j    	$0x019F
```

```
01BC 8A 00       	m2   	                	     	
01BD 00 88 00 07 	lri  	lm0, #0x0007
01BF 11 50 01 CC 	bloopi	#0x50, $0x01CC
	01C1 1C 61       	mrr  	ar3, ar1
	01C2 84 C3       	clrp 	                	ldax 	ax0, @ar0
	01C3 F2 C3       	madd 	ax0.l, ax0.h    	ldax 	ax0, @ar0
	01C4 F2 C3       	madd 	ax0.l, ax0.h    	ldax 	ax0, @ar0
	01C5 F2 C3       	madd 	ax0.l, ax0.h    	ldax 	ax0, @ar0
	01C6 F2 C3       	madd 	ax0.l, ax0.h    	ldax 	ax0, @ar0
	01C7 F2 C3       	madd 	ax0.l, ax0.h    	ldax 	ax0, @ar0
	01C8 F2 C3       	madd 	ax0.l, ax0.h    	ldax 	ax0, @ar0
	01C9 F2 C3       	madd 	ax0.l, ax0.h    	ldax 	ax0, @ar0
	01CA F2 00       	madd 	ax0.l, ax0.h    	     	
	01CB FE 00       	movpz	ac0             	     	
	01CC 1B 3E       	srri 	@ar1, ac0.m
01CD 00 88 FF FF 	lri  	lm0, #0xFFFF
01CF 8B 00       	m0   	                	     	
01D0 02 DF       	ret  	
```

```
01D1 00 88 00 03 	lri  	lm0, #0x0003
01D3 00 85 00 00 	lri  	ix1, #0x0000
01D5 00 87 00 00 	lri  	ix3, #0x0000
01D7 1F C2       	mrr  	ac0.m, ar2
01D8 19 5B       	lrri 	ax1.h, @ar2
01D9 19 59       	lrri 	ax0.h, @ar2
01DA 19 5F       	lrri 	ac1.m, @ar2
01DB 19 5A       	lrri 	ax1.l, @ar2
01DC 1C 5E       	mrr  	ar2, ac0.m
01DD 1F DA       	mrr  	ac0.m, ax1.l
01DE 1C 61       	mrr  	ar3, ar1
01DF 8A 00       	m2   	                	     	
01E0 8F 00       	set40	                	     	
01E1 19 1A       	lrri 	ax1.l, @ar0
01E2 B8 50       	mulx 	ax0.h, ax1.h    	l    	ax1.l, @ar0
01E3 E2 50       	maddx	ax1.l, ax0.h    	l    	ax1.l, @ar0
01E4 EA 50       	maddc	ac1.m, ax0.h    	l    	ax1.l, @ar0
01E5 E8 E8       	maddc	ac0.m, ax0.h    	ldm  	ax0.h, ax1.l, @ar0
01E6 B6 50       	mulxmv	ax0.h, ax1.l, ac0	l    	ax1.l, @ar0
01E7 11 27 01 F2 	bloopi	#0x27, $0x01F2
	01E9 E3 A8       	maddx	ax1.l, ax1.h    	lsm  	ax1.l, ac0.m
	01EA 19 7E       	lrri 	ac0.m, @ar3
	01EB E8 50       	maddc	ac0.m, ax0.h    	l    	ax1.l, @ar0
	01EC EA F8       	maddc	ac1.m, ax0.h    	ldm  	ax0.h, ax1.h, @ar0
	01ED BF 50       	mulxmv	ax0.h, ax1.h, ac1	l    	ax1.l, @ar0
	01EE E2 A9       	maddx	ax1.l, ax0.h    	lsm  	ax1.l, ac1.m
	01EF 19 7F       	lrri 	ac1.m, @ar3
	01F0 EA 50       	maddc	ac1.m, ax0.h    	l    	ax1.l, @ar0
	01F1 E8 E8       	maddc	ac0.m, ax0.h    	ldm  	ax0.h, ax1.l, @ar0
01F2 B6 50       	mulxmv	ax0.h, ax1.l, ac0	l    	ax1.l, @ar0
01F3 E3 A8       	maddx	ax1.l, ax1.h    	lsm  	ax1.l, ac0.m
01F4 19 7E       	lrri 	ac0.m, @ar3
01F5 E8 50       	maddc	ac0.m, ax0.h    	l    	ax1.l, @ar0
01F6 EA F8       	maddc	ac1.m, ax0.h    	ldm  	ax0.h, ax1.h, @ar0
01F7 BF 00       	mulxmv	ax0.h, ax1.h, ac1	     	
01F8 1B FF       	srrn 	@ar3, ac1.m
01F9 19 7F       	lrri 	ac1.m, @ar3
01FA 8E 00       	clr40	                	     	
01FB 8B 00       	m0   	                	     	
01FC 00 88 FF FF 	lri  	lm0, #0xFFFF
01FE 1B 5B       	srri 	@ar2, ax1.h
01FF 1B 59       	srri 	@ar2, ax0.h
0200 1B 5F       	srri 	@ar2, ac1.m
0201 1B 5E       	srri 	@ar2, ac0.m
0202 02 DF       	ret  	
```

## ----------------------------------------------------------------------------------------------------------------

## Case 12 - Tone Generators (?)

```
0203 00 80 03 46 	lri  	ar0, #0x0346
0205 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
0207 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
0209 00 81 03 46 	lri  	ar1, #0x0346
020B 00 9F 05 80 	lri  	ac1.m, #0x0580
020D 00 80 00 80 	lri  	ar0, #0x0080
020F 02 BF 00 7C 	call 	$0x007C 				// DspReadMainMemByPtr
0211 00 81 03 48 	lri  	ar1, #0x0348
0213 00 9F 0C 00 	lri  	ac1.m, #0x0C00
0215 00 80 00 80 	lri  	ar0, #0x0080
0217 02 BF 00 7C 	call 	$0x007C 				// DspReadMainMemByPtr
0219 00 80 0C 00 	lri  	ar0, #0x0C00
021B 00 81 05 80 	lri  	ar1, #0x0580
021D 02 BF 01 D1 	call 	$0x01D1
021F 00 81 03 46 	lri  	ar1, #0x0346
0221 00 9F 05 80 	lri  	ac1.m, #0x0580
0223 00 80 00 80 	lri  	ar0, #0x0080
0225 02 BF 00 89 	call 	$0x0089 				// DspWriteMainMemByPtr
0227 00 81 03 48 	lri  	ar1, #0x0348
0229 00 9F 0C 00 	lri  	ac1.m, #0x0C00
022B 00 80 00 80 	lri  	ar0, #0x0080
022D 02 BF 00 89 	call 	$0x0089 				// DspWriteMainMemByPtr
022F 02 9F 00 49 	j    	$0x0049
0231 81 00       	clr  	ac0             	     	
0232 1F 5E       	mrr  	ax1.l, ac0.m
0233 00 D8 04 02 	lr   	ax0.l, $0x0402
0235 00 DC 04 18 	lr   	ac0.l, $0x0418
0237 00 80 05 20 	lri  	ar0, #0x0520
0239 00 DF 04 40 	lr   	ac1.m, $0x0440
023B 15 01       	lsl  	ac1, #0x01
023C 03 40 00 7E 	andi 	ac1.m, #0x007E
023E 03 00 02 46 	addi 	ac1.m, #0x0246 					// Jump Table 2
0240 1C 5F       	mrr  	ar2, ac1.m
0241 17 5F       	callr	ar2
0242 00 FC 04 18 	sr   	$0x0418, ac0.l
0244 02 9F 04 E7 	j    	$0x04E7
```

## Jump Table 2

```
0246 02 9F 02 57 	j    	$0x0257
0248 02 9F 02 8F 	j    	$0x028F
024A 02 9F 02 77 	j    	$0x0277
024C 02 9F 02 67 	j    	$0x0267
024E 02 9F 02 92 	j    	$0x0292
0250 02 9F 02 56 	j    	$0x0256  			// ret
0252 02 9F 02 B1 	j    	$0x02B1
0254 02 9F 02 AE 	j    	$0x02AE
```

```
0256 02 DF       	ret  	
```

```
0257 14 01       	lsl  	ac0, #0x01
0258 00 9B C0 00 	lri  	ax1.h, #0xC000
025A 00 99 40 00 	lri  	ax0.h, #0x4000
025C 11 50 02 64 	bloopi	#0x50, $0x0264
	025E 02 C0 00 01 	tset 	ac0.m, #0x0001
	0260 02 7C       	ifnok	
	0261 1B 1B       	srri 	@ar0, ax1.h
	0262 02 7D       	ifok 	
	0263 1B 19       	srri 	@ar0, ax0.h
	0264 48 00       	addax	ac0, ax0
0265 14 7F       	lsr  	ac0, -1
0266 02 DF       	ret  	
```

```
0267 14 01       	lsl  	ac0, #0x01
0268 00 9B C0 00 	lri  	ax1.h, #0xC000
026A 00 99 40 00 	lri  	ax0.h, #0x4000
026C 11 50 02 74 	bloopi	#0x50, $0x0274
	026E 02 C0 00 03 	tset 	ac0.m, #0x0003
	0270 02 7C       	ifnok	
	0271 1B 1B       	srri 	@ar0, ax1.h
	0272 02 7D       	ifok 	
	0273 1B 19       	srri 	@ar0, ax0.h
	0274 48 00       	addax	ac0, ax0        	     	
0275 14 7F       	lsr  	ac0, -1
0276 02 DF       	ret  	
```

```
0277 14 01       	lsl  	ac0, #0x01
0278 00 81 0C A0 	lri  	ar1, #0x0CA0
027A 00 9B C0 00 	lri  	ax1.h, #0xC000
027C 00 99 40 00 	lri  	ax0.h, #0x4000
027E 89 00       	clr  	ac1             	     	
027F 00 82 00 00 	lri  	ar2, #0x0000
0281 11 50 02 8C 	bloopi	#0x50, $0x028C
	0283 02 C0 00 01 	tset 	ac0.m, #0x0001
	0285 02 7C       	ifnok	
	0286 1B 1B       	srri 	@ar0, ax1.h
	0287 02 7D       	ifok 	
	0288 1B 19       	srri 	@ar0, ax0.h
	0289 18 3D       	lrr  	ac1.l, @ar1
	028A 49 00       	addax	ac1, ax0        	     	
	028B 1F E2       	mrr  	ac1.m, ar2
	028C 4C 39       	add  	ac0, ac1        	s    	@ar1, ac1.m
028D 14 7F       	lsr  	ac0, -1
028E 02 DF       	ret  	
```

```
028F 10 50       	loopi	#0x50
0290 48 20       	addax	ac0, ax0        	s    	@ar0, ac0.l
0291 02 DF       	ret  	
```

```
0292 00 82 01 40 	lri  	ar2, #0x0140
0294 00 8A 00 3F 	lri  	r0a, #0x003F
0296 00 86 00 00 	lri  	ix2, #0x0000
0298 14 06       	lsl  	ac0, #0x06
0299 89 00       	clr  	ac1             	     	
029A 1F B8       	mrr  	ac1.l, ax0.l
029B 15 06       	lsl  	ac1, #0x06
029C 00 9B 00 3F 	lri  	ax1.h, #0x003F
029E 00 9A 00 00 	lri  	ax1.l, #0x0000
02A0 36 00       	andr 	ac0.m, ax1.h    	     	
02A1 1C DE       	mrr  	ix2, ac0.m
02A2 00 1A       	addarn	ar2, ix2
02A3 34 00       	andr 	ac0.m, ax0.h    	     	
02A4 11 50 02 AA 	bloopi	#0x50, $0x02AA
	02A6 4C 00       	add  	ac0, ac1        	     	
	02A7 36 4A       	andr 	ac0.m, ax1.h    	l    	ax0.h, @ar2
	02A8 1C DE       	mrr  	ix2, ac0.m
	02A9 34 0E       	andr 	ac0.m, ax0.h    	nr   	ar2, ix2
	02AA 1B 19       	srri 	@ar0, ax0.h
02AB 1F C2       	mrr  	ac0.m, ar2
02AC 14 7A       	lsr  	ac0, -6
02AD 02 DF       	ret  	
```

```
02AE 10 50       	loopi	#0x50
02AF 1B 18       	srri 	@ar0, ax0.l
02B0 02 DF       	ret  	
```

```
02B1 00 83 00 00 	lri  	ar3, #0x0000
02B3 14 0F       	lsl  	ac0, #0x0F
02B4 48 53       	addax	ac0, ax0        	l    	ax1.l, @ar3
02B5 11 14 02 BA 	bloopi	#0x14, $0x02BA
	02B7 48 A2       	addax	ac0, ax0        	sl   	ac0.m, ax1.l
	02B8 48 A2       	addax	ac0, ax0        	sl   	ac0.m, ax1.l
	02B9 48 A2       	addax	ac0, ax0        	sl   	ac0.m, ax1.l
	02BA 48 A2       	addax	ac0, ax0        	sl   	ac0.m, ax1.l
02BB 14 6F       	lsr  	ac0, -17
02BC 02 DF       	ret  	
```

## ----------------------------------------------------------------------------------------------------------------

## Case 1 - Load VPB

```
02BD 00 80 03 80 	lri  	ar0, #0x0380
02BF 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
02C1 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
02C3 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
02C5 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
02C7 00 81 03 82 	lri  	ar1, #0x0382
02C9 00 9F 00 00 	lri  	ac1.m, #0x0000 		// Dsp addr  0x0
02CB 00 80 02 00 	lri  	ar0, #0x0200 		// 0x400 bytes
02CD 02 BF 00 7C 	call 	$0x007C 				// DspReadMainMemByPtr
02CF 00 81 03 84 	lri  	ar1, #0x0384
02D1 00 9F 03 00 	lri  	ac1.m, #0x0300 		// Dsp addr 0x300
02D3 00 80 00 20 	lri  	ar0, #0x0020 		// 0x40 bytes
02D5 02 BF 00 7C 	call 	$0x007C 				// DspReadMainMemByPtr
02D7 02 BF 03 51 	call 	$0x0351 										// LoadAdpcmDecoderCoef
02D9 00 DE 03 45 	lr   	ac0.m, $0x0345
02DB 00 FE 03 42 	sr   	$0x0342, ac0.m 			// *0x342 = *0x345
02DD 02 9F 00 49 	j    	$0x0049
```

## Not Used

```
02DF 00 DE 03 44 	lr   	ac0.m, $0x0344
02E1 14 04       	lsl  	ac0, #0x04
02E2 02 00 03 A8 	addi 	ac0.m, #0x03A8
02E4 1C 1E       	mrr  	ar0, ac0.m
02E5 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
02E7 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
02E9 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
02EB 00 DE 03 45 	lr   	ac0.m, $0x0345
02ED 1B 1E       	srri 	@ar0, ac0.m
02EE 00 DE 03 44 	lr   	ac0.m, $0x0344
02F0 02 00 03 A4 	addi 	ac0.m, #0x03A4
02F2 1C 1E       	mrr  	ar0, ac0.m
02F3 81 00       	clr  	ac0             	     	
02F4 1B 1E       	srri 	@ar0, ac0.m
02F5 02 DF       	ret  	
```

## Not Used

```
02F6 00 DE 03 44 	lr   	ac0.m, $0x0344
02F8 14 04       	lsl  	ac0, #0x04
02F9 02 00 03 B0 	addi 	ac0.m, #0x03B0
02FB 1C 1E       	mrr  	ar0, ac0.m
02FC 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
02FE 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
0300 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
0302 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
0304 02 DF       	ret  	
```

## Case2_Sub1

```
0305 00 81 03 4C 	lri  	ar1, #0x034C
0307 00 9F 04 00 	lri  	ac1.m, #0x0400
0309 00 80 00 80 	lri  	ar0, #0x0080
030B 02 BF 00 7C 	call 	$0x007C 				// DspReadMainMemByPtr
030D 02 DF       	ret  	
```

## Not Used

```
030E 00 81 03 4C 	lri  	ar1, #0x034C
0310 00 9F 0A 00 	lri  	ac1.m, #0x0A00
0312 00 80 00 04 	lri  	ar0, #0x0004
0314 02 BF 00 A5 	call 	$0x00A5 				// WaitDspDma
0316 02 BF 00 7C 	call 	$0x007C 				// DspReadMainMemByPtr
0318 00 81 03 4C 	lri  	ar1, #0x034C
031A 00 9F 04 00 	lri  	ac1.m, #0x0400
031C 00 80 00 80 	lri  	ar0, #0x0080
031E 02 BF 00 95 	call 	$0x0095 					// DspReadMainMemNoWait
0320 02 DF       	ret  	
```

## Case2_Sub2

```
0321 00 81 03 4C 	lri  	ar1, #0x034C
0323 00 9F 04 00 	lri  	ac1.m, #0x0400 				// DspAddr 0x400
0325 00 80 00 40 	lri  	ar0, #0x0040
0327 00 81 03 4C 	lri  	ar1, #0x034C
0329 19 3E       	lrri 	ac0.m, @ar1
032A 19 3C       	lrri 	ac0.l, @ar1
032B 00 98 00 00 	lri  	ax0.l, #0x0000
032D 70 00       	addaxl	ac0, ax0.l     	     	
032E 02 BF 00 8B 	call 	$0x008B 					// DspWriteMainMem (ac0: MainMemAddr)
0330 02 DF       	ret  	
```

## ???

```
0331 19 1E       	lrri 	ac0.m, @ar0
0332 19 1A       	lrri 	ax1.l, @ar0
0333 00 5F       	loop 	ac1.m
0334 64 A0       	movr 	ac0, ax1.l      	ls   	ax1.l, ac0.m
0335 1B 7E       	srri 	@ar3, ac0.m
0336 1B 7A       	srri 	@ar3, ax1.l
0337 02 DF       	ret  	
```

## XorSub

ac1m - Loop counter (usually 0x28 -> 80 bytes processed)

```
0338 19 1E       	lrri 	ac0.m, @ar0
0339 19 1A       	lrri 	ax1.l, @ar0
033A 00 7F 03 3F 	bloop	ac1.m, $0x033F
	033C 32 B2       	xorr 	ac0.m, ax1.h    	sl   	ac0.m, ax1.h
	033D 65 A0       	movr 	ac1, ax1.l      	ls   	ax1.l, ac0.m
	033E 33 BA       	xorr 	ac1.m, ax1.h    	slm  	ac0.m, ax1.h
	033F 64 A1       	movr 	ac0, ax1.l      	ls   	ax1.l, ac1.m
0340 00 00       	nop  	
0341 02 DF       	ret  	
```

```c++
XorSub(int ac1m, void *ar0, void *ar3) 		// 0338
{
	ac0m = *ar0++;
	ax1l = *ar0++;

	// ax1h - where?? 	Garbage
	// ix3 - where??   Garbage (0x1280 after IROM)

	while (ac1m--)
	{
		// Ver1

		// xorr 	ac0.m, ax1.h    	sl   	ac0.m, ax1.h
		ac0m ^= ax1h;
		*ar0++ = ac0m;
		ax1h = *ar3++;

		// movr 	ac1, ax1.l      	ls   	ax1.l, ac0.m
		ac1m = ax1l;
		ax1l = *ar0++;
		*ar3++ = ac0m;

		// xorr 	ac1.m, ax1.h    	slm  	ac0.m, ax1.h
		ac1m ^= ax1h;
		*ar0++ = ac0m;
		ax1h = *ar3;
		ar3 += ix3;

		// movr 	ac0, ax1.l      	ls   	ax1.l, ac1.m
		ac0m = ax1l;
		ax1l = *ar0++;
		*ar3++ = ac1m;
	}
}
```


```
0342 8A 00       	m2   	                	     	
0343 15 7F       	lsr  	ac1, -1
0344 1C 20       	mrr  	ar1, ar0
0345 1C 03       	mrr  	ar0, ar3
0346 19 3A       	lrri 	ax1.l, @ar1
0347 90 51       	mul  	ax0.l, ax0.h    	l    	ax1.l, @ar1
0348 92 5B       	mulmvz	ax0.l, ax0.h, ac0	l    	ax1.h, @ar3
0349 00 7F 03 4E 	bloop	ac1.m, $0x034E
	034B 46 51       	addr 	ac0, ax1.h      	l    	ax1.l, @ar1
	034C 92 B2       	mulmvz	ax0.l, ax0.h, ac0	sl   	ac0.m, ax1.h
	034D 46 51       	addr 	ac0, ax1.h      	l    	ax1.l, @ar1
	034E 92 B2       	mulmvz	ax0.l, ax0.h, ac0	sl   	ac0.m, ax1.h
034F 8B 00       	m0   	                	     	
0350 02 DF       	ret  	
```

## LoadAdpcmDecoderCoef

```
0351 00 83 FF A0 	lri  	ar3, #0xFFA0   				// ADPCM coef table start
0353 00 80 03 00 	lri  	ar0, #0x0300
0355 00 9F 00 0E 	lri  	ac1.m, #0x000E
0357 11 08 03 5C 	bloopi	#0x08, $0x035C
	0359 19 1E       	lrri 	ac0.m, @ar0
	035A 1B 7E       	srri 	@ar3, ac0.m
	035B 19 1E       	lrri 	ac0.m, @ar0
	035C 1B 7E       	srri 	@ar3, ac0.m
035D 02 DF       	ret  	
```

## Case2_Sub3_Xor

```
035E 00 80 0F 40 	lri  	ar0, #0x0F40
0360 00 82 0D 00 	lri  	ar2, #0x0D00  			// Suspicious ... 
0362 00 83 0D 60 	lri  	ar3, #0x0D60
0364 00 9F 00 28 	lri  	ac1.m, #0x0028
0366 02 BF 03 38 	call 	$0x0338 				// XorSub
0368 89 00       	clr  	ac1             	     	
0369 00 9E 00 50 	lri  	ac0.m, #0x0050
036B 00 80 0C A0 	lri  	ar0, #0x0CA0
036D 00 5E       	loop 	ac0.m
036E 1B 1F       	srri 	@ar0, ac1.m
036F 00 80 0F 40 	lri  	ar0, #0x0F40
0371 00 5E       	loop 	ac0.m
0372 1B 1F       	srri 	@ar0, ac1.m
0373 00 80 0F A0 	lri  	ar0, #0x0FA0
0375 00 5E       	loop 	ac0.m
0376 1B 1F       	srri 	@ar0, ac1.m
0377 02 DF       	ret  	
```

```
0378 00 80 0D C0 	lri  	ar0, #0x0DC0
037A 00 9E 01 80 	lri  	ac0.m, #0x0180
037C 89 00       	clr  	ac1             	     	
037D 00 5E       	loop 	ac0.m
037E 1B 1F       	srri 	@ar0, ac1.m
037F 02 DF       	ret  	
```

```
0380 00 C0 03 A0 	lr   	ar0, $0x03A0
0382 19 1A       	lrri 	ax1.l, @ar0
0383 00 DF 03 A1 	lr   	ac1.m, $0x03A1
0385 00 9B 00 A0 	lri  	ax1.h, #0x00A0
0387 00 81 03 93 	lri  	ar1, #0x0393
0389 18 BC       	lrrd 	ac0.l, @ar1
038A B8 71       	mulx 	ax0.h, ax1.h    	l    	ac0.m, @ar1
038B BC 00       	mulxac	ax0.h, ax1.h, ac0	     	
038C 00 80 00 50 	lri  	ar0, #0x0050
038E 05 08       	addis	ac1.m, 8
038F 02 BF 00 7E 	call 	$0x007E 				// DspReadMainMem (ac0 - MainMem address)
0391 00 DE 03 90 	lr   	ac0.m, $0x0390
0393 02 A0 00 01 	tclr 	ac0.m, #0x0001
0395 02 9D 03 9F 	jok  	$0x039F
0397 00 80 03 98 	lri  	ar0, #0x0398
0399 00 9E 00 08 	lri  	ac0.m, #0x0008
039B 00 C1 03 A1 	lr   	ar1, $0x03A1
039D 02 BF 01 BC 	call 	$0x01BC
039F 00 9F 00 50 	lri  	ac1.m, #0x0050
03A1 00 C0 03 A1 	lr   	ar0, $0x03A1
03A3 81 00       	clr  	ac0             	     	
03A4 00 DE 03 94 	lr   	ac0.m, $0x0394
03A6 B1 00       	tst  	ac0             	     	
03A7 02 95 03 AE 	jeq  	$0x03AE
03A9 1C 7E       	mrr  	ar3, ac0.m
03AA 00 D8 03 95 	lr   	ax0.l, $0x0395
03AC 02 BF 03 42 	call 	$0x0342
03AE 00 9F 00 50 	lri  	ac1.m, #0x0050
03B0 00 C0 03 A1 	lr   	ar0, $0x03A1
03B2 81 00       	clr  	ac0             	     	
03B3 00 DE 03 96 	lr   	ac0.m, $0x0396
03B5 B1 00       	tst  	ac0             	     	
03B6 02 95 03 BD 	jeq  	$0x03BD
03B8 1C 7E       	mrr  	ar3, ac0.m
03B9 00 D8 03 97 	lr   	ax0.l, $0x0397
03BB 02 BF 03 42 	call 	$0x0342
03BD 00 DE 03 90 	lr   	ac0.m, $0x0390
03BF 02 A0 00 02 	tclr 	ac0.m, #0x0002
03C1 02 DD       	retok	
03C2 00 80 03 98 	lri  	ar0, #0x0398
03C4 00 9E 00 08 	lri  	ac0.m, #0x0008
03C6 00 C1 03 A1 	lr   	ar1, $0x03A1
03C8 02 BF 01 BC 	call 	$0x01BC
03CA 02 DF       	ret  	
```

## Case2_Sub4

```
03CB 00 9F 0D C0 	lri  	ac1.m, #0x0DC0
03CD 00 FF 03 A1 	sr   	$0x03A1, ac1.m
03CF 00 9F 03 A8 	lri  	ac1.m, #0x03A8
03D1 00 FF 03 A2 	sr   	$0x03A2, ac1.m
03D3 00 9F 03 A4 	lri  	ac1.m, #0x03A4
03D5 00 FF 03 A0 	sr   	$0x03A0, ac1.m
03D7 11 04 04 00 	bloopi	#0x04, $0x0400
	03D9 00 C0 03 A2 	lr   	ar0, $0x03A2
	03DB 00 83 03 90 	lri  	ar3, #0x0390
	03DD 00 9F 00 0E 	lri  	ac1.m, #0x000E
	03DF 02 BF 03 31 	call 	$0x0331
	03E1 00 DA 03 90 	lr   	ax1.l, $0x0390
	03E3 86 00       	tstaxh	ax0.h          	     	
	03E4 02 95 03 F1 	jeq  	$0x03F1
	03E6 00 DF 03 A1 	lr   	ac1.m, $0x03A1
	03E8 1C 7F       	mrr  	ar3, ac1.m
	03E9 05 50       	addis	ac1.m, 80
	03EA 1C 1F       	mrr  	ar0, ac1.m
	03EB 00 9F 00 06 	lri  	ac1.m, #0x0006
	03ED 02 BF 03 31 	call 	$0x0331
	03EF 02 BF 03 80 	call 	$0x0380
	03F1 00 DE 03 A2 	lr   	ac0.m, $0x03A2
	03F3 04 10       	addis	ac0.m, 16
	03F4 00 FE 03 A2 	sr   	$0x03A2, ac0.m
	03F6 00 DE 03 A1 	lr   	ac0.m, $0x03A1
	03F8 04 60       	addis	ac0.m, 96
	03F9 00 FE 03 A1 	sr   	$0x03A1, ac0.m
	03FB 00 DE 03 A0 	lr   	ac0.m, $0x03A0
	03FD 74 00       	incm 	ac0             	     	
	03FE 00 FE 03 A0 	sr   	$0x03A0, ac0.m
	0400 00 00       	nop  	
0401 02 DF       	ret  	
```

```
0402 00 C0 03 A0 	lr   	ar0, $0x03A0
0404 18 1A       	lrr  	ax1.l, @ar0
0405 81 00       	clr  	ac0             	     	
0406 18 1E       	lrr  	ac0.m, @ar0
0407 00 DB 03 91 	lr   	ax1.h, $0x0391
0409 74 00       	incm 	ac0             	     	
040A D1 00       	cmpar	ac0.m, ax1.h      	     	
040B 02 70       	ifge 	
040C 81 00       	clr  	ac0             	     	
040D 1B 1E       	srri 	@ar0, ac0.m
040E 00 DF 03 A1 	lr   	ac1.m, $0x03A1  			// DspAddr 
0410 00 9B 00 A0 	lri  	ax1.h, #0x00A0
0412 00 81 03 93 	lri  	ar1, #0x0393
0414 18 BC       	lrrd 	ac0.l, @ar1
0415 B8 71       	mulx 	ax0.h, ax1.h    	l    	ac0.m, @ar1
0416 BC 00       	mulxac	ax0.h, ax1.h, ac0	     	
0417 00 80 00 50 	lri  	ar0, #0x0050
0419 02 BF 00 8B 	call 	$0x008B 						// DspWriteMainMem (ac0: MainMemAddr)
041B 02 DF       	ret
```

```
041C 00 9F 0D C0 	lri  	ac1.m, #0x0DC0
041E 00 FF 03 A1 	sr   	$0x03A1, ac1.m
0420 00 9F 03 A8 	lri  	ac1.m, #0x03A8
0422 00 FF 03 A2 	sr   	$0x03A2, ac1.m
0424 00 9F 03 A4 	lri  	ac1.m, #0x03A4
0426 00 FF 03 A0 	sr   	$0x03A0, ac1.m
0428 11 04 04 48 	bloopi	#0x04, $0x0448
	042A 00 C0 03 A2 	lr   	ar0, $0x03A2
	042C 00 83 03 90 	lri  	ar3, #0x0390
	042E 00 9F 00 0E 	lri  	ac1.m, #0x000E
	0430 02 BF 03 31 	call 	$0x0331
	0432 00 DA 03 90 	lr   	ax1.l, $0x0390
	0434 86 00       	tstaxh	ax0.h          	     	
	0435 02 95 04 39 	jeq  	$0x0439
	0437 02 BF 04 02 	call 	$0x0402
	0439 00 DE 03 A2 	lr   	ac0.m, $0x03A2
	043B 04 10       	addis	ac0.m, 16
	043C 00 FE 03 A2 	sr   	$0x03A2, ac0.m
	043E 00 DE 03 A1 	lr   	ac0.m, $0x03A1
	0440 04 60       	addis	ac0.m, 96
	0441 00 FE 03 A1 	sr   	$0x03A1, ac0.m
	0443 00 DE 03 A0 	lr   	ac0.m, $0x03A0
	0445 74 00       	incm 	ac0             	     	
	0446 00 FE 03 A0 	sr   	$0x03A0, ac0.m
	0448 00 00       	nop  	
0449 02 DF       	ret  	
```

## Command2_ReadMainMem

```
044A 00 81 03 86 	lri  	ar1, #0x0386
044C 00 9F 03 A8 	lri  	ac1.m, #0x03A8
044E 00 80 00 40 	lri  	ar0, #0x0040
0450 02 BF 00 7C 	call 	$0x007C 					// DspReadMainMemByPtr
0452 02 DF       	ret  	
```

```
0453 19 1E       	lrri 	ac0.m, @ar0
0454 18 9C       	lrrd 	ac0.l, @ar0
0455 48 00       	addax	ac0, ax0        	     	
0456 1B 1E       	srri 	@ar0, ac0.m
0457 1B 1C       	srri 	@ar0, ac0.l
0458 02 DF       	ret  	
```

## Command2_ReadCpuMailbox

```
0459 81 00       	clr  	ac0             	     	
045A 26 FE       	lrs  	ac0.m, $(CMBH)
045B 02 C0 80 00 	tset 	ac0.m, #0x8000
045D 02 9C 04 5A 	jnok 	$0x045A
045F 26 FF       	lrs  	ac0.m, $(CMBL)
0460 02 DF       	ret  	
```

## ReadTwoMailboxes (Command 2)

```
0461 00 80 03 88 	lri  	ar0, #0x0388
0463 00 81 00 51 	lri  	ar1, #0x0051 				// ReadCpuMailbox
0465 17 3F       	callr	ar1
0466 00 DE 03 44 	lr   	ac0.m, $0x0344
0468 00 FE 03 41 	sr   	$0x0341, ac0.m
046A 00 DE 03 45 	lr   	ac0.m, $0x0345
046C 00 FE 03 8E 	sr   	$0x038E, ac0.m
046E 17 3F       	callr	ar1 						// ReadCpuMailbox
046F 02 DF       	ret  	
```


## Case 2 - Process

```
0470 02 BF 04 61 	call 	$0x0461 						// ReadTwoMailboxes
0472 00 9E 80 00 	lri  	ac0.m, #0x8000
0474 00 DC 03 41 	lr   	ac0.l, $0x0341
0476 02 BF 00 5A 	call 	$0x005A 				// WriteDspMailbox
0478 81 00       	clr  	ac0             	     	
0479 00 FE 03 55 	sr   	$0x0355, ac0.m
047B 02 BF 04 4A 	call 	$0x044A 						// Command2_ReadMainMem
047D 00 DE 03 41 	lr   	ac0.m, $0x0341
047F 00 7E 05 6C 	bloop	ac0.m, $0x056C
	0481 02 BF 03 5E 	call 	$0x035E 						// Case2_Sub3_Xor
	0483 02 BF 03 CB 	call 	$0x03CB 						// Case2_Sub4
	0485 02 BF 04 59 	call 	$0x0459 				// Command2_ReadCpuMailbox
	0487 81 00       	clr  	ac0             	     	
	0488 00 FE 03 54 	sr   	$0x0354, ac0.m
	048A 00 DE 03 42 	lr   	ac0.m, $0x0342
	048C 00 7E 05 38 	bloop	ac0.m, $0x0538
		048E 00 D8 03 54 	lr   	ax0.l, $0x0354
		0490 00 9A 01 00 	lri  	ax1.l, #0x0100
		0492 81 00       	clr  	ac0             	     	
		0493 00 DE 03 80 	lr   	ac0.m, $0x0380
		0495 00 DC 03 81 	lr   	ac0.l, $0x0381
		0497 90 00       	mul  	ax0.l, ax0.h    	     	
		0498 94 00       	mulac	ax0.l, ax0.h, ac0	     	
		0499 00 FE 03 4C 	sr   	$0x034C, ac0.m
		049B 00 FC 03 4D 	sr   	$0x034D, ac0.l
		049D 02 BF 03 05 	call 	$0x0305 						// Case2_Sub1
		049F 00 DA 04 00 	lr   	ax1.l, $0x0400
		04A1 86 00       	tstaxh	ax0.h          	     	
		04A2 02 95 05 33 	jeq  	$0x0533
		04A4 00 DA 04 01 	lr   	ax1.l, $0x0401
		04A6 86 00       	tstaxh	ax0.h          	     	
		04A7 02 94 05 33 	jne  	$0x0533
		04A9 00 DA 04 06 	lr   	ax1.l, $0x0406
		04AB 86 00       	tstaxh	ax0.h          	     	
		04AC 02 94 09 30 	jne  	$0x0930
		04AE 81 00       	clr  	ac0             	     	
		04AF 00 DE 04 40 	lr   	ac0.m, $0x0440
		04B1 06 07       	cmpis	ac0.m, 7
		04B2 02 93 02 31 	jle  	$0x0231
		04B4 06 20       	cmpis	ac0.m, 32
		04B5 02 95 07 9E 	jeq  	$0x079E
		04B7 06 21       	cmpis	ac0.m, 33
		04B8 02 95 07 A7 	jeq  	$0x07A7
		04BA 00 D8 04 02 	lr   	ax0.l, $0x0402
		04BC 81 00       	clr  	ac0             	     	
		04BD 89 00       	clr  	ac1             	     	
		04BE 00 DC 04 18 	lr   	ac0.l, $0x0418
		04C0 8D 00       	set15	                	     	
		04C1 00 99 00 50 	lri  	ax0.h, #0x0050
		04C3 A0 00       	mulx 	ax0.l, ax1.l    	     	
		04C4 A4 00       	mulxac	ax0.l, ax1.l, ac0	     	
		04C5 14 04       	lsl  	ac0, #0x04
		04C6 8C 00       	clr15	                	     	
		04C7 1F FE       	mrr  	ac1.m, ac0.m
		04C8 00 83 05 80 	lri  	ar3, #0x0580
		04CA 00 DA 04 41 	lr   	ax1.l, $0x0441
		04CC 86 00       	tstaxh	ax0.h          	     	
		04CD 02 95 04 DD 	jeq  	$0x04DD
		04CF 00 DA 04 49 	lr   	ax1.l, $0x0449
		04D1 81 00       	clr  	ac0             	     	
		04D2 00 DE 04 4B 	lr   	ac0.m, $0x044B
		04D4 38 00       	orr  	ac0.m, ax0.h    	     	
		04D5 02 40 00 0F 	andi 	ac0.m, #0x000F
		04D7 02 95 04 DD 	jeq  	$0x04DD
		04D9 02 BF 06 A6 	call 	$0x06A6 						// Command2_10_AnotherSub1
		04DB 02 9F 04 DF 	j    	$0x04DF
		04DD 02 BF 08 37 	call 	$0x0837
		04DF 00 80 05 80 	lri  	ar0, #0x0580
		04E1 00 81 05 20 	lri  	ar1, #0x0520
		04E3 00 99 00 00 	lri  	ax0.h, #0x0000
		04E5 02 BF 01 3B 	call 	$0x013B
		04E7 00 80 04 50 	lri  	ar0, #0x0450
		04E9 00 81 05 20 	lri  	ar1, #0x0520
		04EB 00 82 04 28 	lri  	ar2, #0x0428
		04ED 00 83 04 53 	lri  	ar3, #0x0453
		04EF 18 FA       	lrrd 	ax1.l, @ar3
		04F0 86 00       	tstaxh	ax0.h          	     	
		04F1 02 94 05 01 	jne  	$0x0501
		04F3 18 FA       	lrrd 	ax1.l, @ar3
		04F4 86 00       	tstaxh	ax0.h          	     	
		04F5 02 94 05 01 	jne  	$0x0501
		04F7 18 FA       	lrrd 	ax1.l, @ar3
		04F8 86 00       	tstaxh	ax0.h          	     	
		04F9 02 94 05 01 	jne  	$0x0501
		04FB 81 00       	clr  	ac0             	     	
		04FC 18 FE       	lrrd 	ac0.m, @ar3
		04FD 02 80 7F FF 	cmpi 	ac0.m, #0x7FFF
		04FF 02 95 05 05 	jeq  	$0x0505
		0501 02 BF 01 D1 	call 	$0x01D1
		0503 02 9F 05 05 	j    	$0x0505
		0505 81 00       	clr  	ac0             	     	
		0506 1C 9E       	mrr  	ix0, ac0.m
		0507 1C DE       	mrr  	ix2, ac0.m
		0508 74 00       	incm 	ac0             	     	
		0509 1C FE       	mrr  	ix3, ac0.m 					// ix3 = 1
		050A 8F 00       	set40	                	     	
		050B 00 86 00 02 	lri  	ix2, #0x0002
		050D 00 82 04 08 	lri  	ar2, #0x0408
		050F 11 04 05 2F 	bloopi	#0x04, $0x052F
			0511 81 00       	clr  	ac0             	     	
			0512 19 5E       	lrri 	ac0.m, @ar2
			0513 12 00       	sbset	6
			0514 B1 00       	tst  	ac0             	     	
			0515 02 75       	ifeq 	
			0516 13 00       	sbclr	6
			0517 1C 7E       	mrr  	ar3, ac0.m
			0518 19 5E       	lrri 	ac0.m, @ar2
			0519 14 FA       	asr  	ac0, -6
			051A 1F 5E       	mrr  	ax1.l, ac0.m
			051B 1F 1C       	mrr  	ax0.l, ac0.l
			051C 18 5F       	lrr  	ac1.m, @ar2
			051D 00 80 05 20 	lri  	ar0, #0x0520
			051F 02 9D 05 23 	jok  	$0x0523
			0521 02 BF 01 1E 	call 	$0x011E
			0523 1B 5F       	srri 	@ar2, ac1.m
			0524 81 00       	clr  	ac0             	     	
			0525 18 5E       	lrr  	ac0.m, @ar2
			0526 00 0E       	??? 000E
			0527 B1 00       	tst  	ac0             	     	
			0528 02 74       	ifne 	
			0529 78 00       	decm 	ac0             	     	
			052A B1 00       	tst  	ac0             	     	
			052B 89 00       	clr  	ac1             	     	
			052C 02 75       	ifeq 	
			052D 1A 5F       	srr  	@ar2, ac1.m
			052E 00 1A       	addarn	ar2, ix2
			052F 1B 5E       	srri 	@ar2, ac0.m
		0530 8E 00       	clr40	                	     	
		0531 02 BF 03 21 	call 	$0x0321 					// Case2_Sub2
		0533 00 DE 03 54 	lr   	ac0.m, $0x0354
		0535 74 00       	incm 	ac0             	     	
		0536 00 FE 03 54 	sr   	$0x0354, ac0.m
		0538 00 00       	nop  	
	0539 16 FB 00 01 	si   	$(DIRQ), #0x0001
	053B 00 83 0D 00 	lri  	ar3, #0x0D00
	053D 02 BF 01 2A 	call 	$0x012A
	053F 00 81 03 88 	lri  	ar1, #0x0388
	0541 00 9F 0D 00 	lri  	ac1.m, #0x0D00
	0543 00 80 00 50 	lri  	ar0, #0x0050
	0545 02 BF 00 89 	call 	$0x0089 					// DspWriteMainMemByPtr
	0547 00 80 0F A0 	lri  	ar0, #0x0FA0
	0549 00 83 0D 60 	lri  	ar3, #0x0D60
	054B 00 9F 00 50 	lri  	ac1.m, #0x0050
	054D 00 98 80 00 	lri  	ax0.l, #0x8000
	054F 02 BF 03 42 	call 	$0x0342
	0551 00 83 0D 60 	lri  	ar3, #0x0D60
	0553 02 BF 01 2A 	call 	$0x012A
	0555 00 81 03 8A 	lri  	ar1, #0x038A
	0557 00 9F 0D 60 	lri  	ac1.m, #0x0D60
	0559 00 80 00 50 	lri  	ar0, #0x0050
	055B 02 BF 00 89 	call 	$0x0089 					// DspWriteMainMemByPtr
	055D 00 9A 00 00 	lri  	ax1.l, #0x0000
	055F 00 98 00 A0 	lri  	ax0.l, #0x00A0
	0561 00 80 03 88 	lri  	ar0, #0x0388
	0563 02 BF 04 53 	call 	$0x0453
	0565 00 80 03 8A 	lri  	ar0, #0x038A
	0567 02 BF 04 53 	call 	$0x0453
	0569 02 BF 04 1C 	call 	$0x041C
	056B 00 00       	nop  	
	056C 00 00       	nop  	
056D 02 9F 00 31 	j    	$0x0031 		// MainLoop
```

## Case 7 - Memcpy

Make memcpy in main memory by Dsp DMA.

```
056F 00 80 03 46 	lri  	ar0, #0x0346
0571 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
0573 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
0575 00 81 03 46 	lri  	ar1, #0x0346
0577 19 3E       	lrri 	ac0.m, @ar1
0578 19 3C       	lrri 	ac0.l, @ar1
0579 00 9F 04 00 	lri  	ac1.m, #0x0400
057B 00 C0 03 45 	lr   	ar0, $0x0345
057D 02 BF 00 7E 	call 	$0x007E 				// DspReadMainMem (ac0 - MainMem address)
057F 00 81 03 48 	lri  	ar1, #0x0348
0581 19 3E       	lrri 	ac0.m, @ar1
0582 19 3C       	lrri 	ac0.l, @ar1
0583 00 9F 08 00 	lri  	ac1.m, #0x0800
0585 00 C0 03 45 	lr   	ar0, $0x0345
0587 02 BF 00 7E 	call 	$0x007E 				// DspReadMainMem (ac0 - MainMem address)
0589 00 81 03 46 	lri  	ar1, #0x0346
058B 19 3E       	lrri 	ac0.m, @ar1
058C 19 3C       	lrri 	ac0.l, @ar1
058D 00 9F 08 00 	lri  	ac1.m, #0x0800 		// DspAddr 0x800
058F 00 C0 03 45 	lr   	ar0, $0x0345
0591 02 BF 00 8B 	call 	$0x008B 					// DspWriteMainMem (ac0: MainMemAddr)
0593 00 81 03 48 	lri  	ar1, #0x0348
0595 19 3E       	lrri 	ac0.m, @ar1
0596 19 3C       	lrri 	ac0.l, @ar1
0597 00 9F 04 00 	lri  	ac1.m, #0x0400 		// DspAddr 0x400
0599 00 C0 03 45 	lr   	ar0, $0x0345
059B 02 BF 00 8B 	call 	$0x008B 					// DspWriteMainMem (ac0: MainMemAddr)
059D 02 9F 00 49 	j    	$0x0049
```

## Case 9 - SmallMemcpy

Make small memcpy in main memory by Dsp DMA.

```
059F 00 80 03 46 	lri  	ar0, #0x0346
05A1 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
05A3 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
05A5 00 81 03 46 	lri  	ar1, #0x0346
05A7 19 3E       	lrri 	ac0.m, @ar1
05A8 19 3C       	lrri 	ac0.l, @ar1
05A9 00 9F 04 00 	lri  	ac1.m, #0x0400
05AB 00 C0 03 45 	lr   	ar0, $0x0345
05AD 02 BF 00 7E 	call 	$0x007E 				// DspReadMainMem (ac0 - MainMem address)
05AF 00 81 03 48 	lri  	ar1, #0x0348
05B1 19 3E       	lrri 	ac0.m, @ar1
05B2 19 3C       	lrri 	ac0.l, @ar1
05B3 00 9F 04 00 	lri  	ac1.m, #0x0400    	// DspAddr 0x400
05B5 00 C0 03 45 	lr   	ar0, $0x0345
05B7 02 BF 00 8B 	call 	$0x008B 					// DspWriteMainMem (ac0: MainMemAddr)
05B9 02 9F 00 49 	j    	$0x0049
```

## Case 6 - ReadARAM

```
05BB 00 80 03 46 	lri  	ar0, #0x0346
05BD 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
05BF 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
05C1 00 81 03 46 	lri  	ar1, #0x0346
05C3 19 3E       	lrri 	ac0.m, @ar1
05C4 19 3C       	lrri 	ac0.l, @ar1
05C5 00 9F 04 00 	lri  	ac1.m, #0x0400
05C7 00 C0 03 45 	lr   	ar0, $0x0345
05C9 02 BF 00 AE 	call 	$0x00AE 						// AramRead
05CB 00 81 03 48 	lri  	ar1, #0x0348
05CD 19 3E       	lrri 	ac0.m, @ar1
05CE 19 3C       	lrri 	ac0.l, @ar1
05CF 00 9F 04 00 	lri  	ac1.m, #0x0400 			// DspAddr 0x400
05D1 00 C0 03 45 	lr   	ar0, $0x0345
05D3 02 BF 00 8B 	call 	$0x008B 				// DspWriteMainMem (ac0: MainMemAddr)
05D5 02 9F 00 49 	j    	$0x0049
```

## Case 8

```
05D7 00 80 03 46 	lri  	ar0, #0x0346
05D9 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
05DB 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
05DD 00 81 03 46 	lri  	ar1, #0x0346
05DF 19 3E       	lrri 	ac0.m, @ar1
05E0 19 3C       	lrri 	ac0.l, @ar1
05E1 00 9F 04 00 	lri  	ac1.m, #0x0400
05E3 00 C0 03 44 	lr   	ar0, $0x0344
05E5 02 BF 00 7E 	call 	$0x007E 				// DspReadMainMem (ac0 - MainMem address)
05E7 00 81 03 48 	lri  	ar1, #0x0348
05E9 19 3E       	lrri 	ac0.m, @ar1
05EA 19 3C       	lrri 	ac0.l, @ar1
05EB 00 9F 08 00 	lri  	ac1.m, #0x0800
05ED 00 C0 03 44 	lr   	ar0, $0x0344
05EF 02 BF 00 7E 	call 	$0x007E 				// DspReadMainMem (ac0 - MainMem address)
05F1 00 80 04 00 	lri  	ar0, #0x0400
05F3 00 83 08 00 	lri  	ar3, #0x0800
05F5 00 84 00 00 	lri  	ix0, #0x0000
05F7 00 DA 03 45 	lr   	ax1.l, $0x0345
05F9 00 DF 03 44 	lr   	ac1.m, $0x0344
05FB 8F 00       	set40	                	     	
05FC 19 7B       	lrri 	ax1.h, @ar3
05FD B8 00       	mulx 	ax0.h, ax1.h    	     	
05FE 19 7B       	lrri 	ax1.h, @ar3
05FF 00 7F 06 04 	bloop	ac1.m, $0x0604
	0601 19 9E       	lrrn 	ac0.m, @ar0
	0602 BC 00       	mulxac	ax0.h, ax1.h, ac0	     	
	0603 80 B2       	sl   	ac0.m, ax1.h
	0604 00 00       	nop  	
0605 8E 00       	clr40	                	     	
0606 00 81 03 46 	lri  	ar1, #0x0346
0608 19 3E       	lrri 	ac0.m, @ar1
0609 19 3C       	lrri 	ac0.l, @ar1
060A 00 9F 04 00 	lri  	ac1.m, #0x0400 		// DspAddr 0x400
060C 00 C0 03 44 	lr   	ar0, $0x0344
060E 02 BF 00 8B 	call 	$0x008B 				// DspWriteMainMem (ac0: MainMemAddr)
0610 00 9E 82 00 	lri  	ac0.m, #0x8200
0612 00 DC 03 44 	lr   	ac0.l, $0x0344
0614 02 BF 00 5A 	call 	$0x005A 				// WriteDspMailbox
0616 02 9F 00 31 	j    	$0x0031 		// MainLoop
```

## Case 11 - Call_IROM_SRCPossible

```
0618 00 80 03 46 	lri  	ar0, #0x0346
061A 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
061C 00 81 03 46 	lri  	ar1, #0x0346
061E 00 9F 04 00 	lri  	ac1.m, #0x0400 			// DspAddr 0x400
0620 00 C0 03 45 	lr   	ar0, $0x0345
0622 02 BF 00 7C 	call 	$0x007C 				// DspReadMainMemByPtr
0624 02 BF 86 44 	call 	$0x8644 					// Call IROM  (SRC?)
0626 02 9F 00 49 	j    	$0x0049
```

```
0628 00 9E 04 30 	lri  	ac0.m, #0x0430
062A 22 19       	lrs  	ax1.l, $0x0019
062B 44 00       	addr 	ac0, ax1.l      	     	
062C 1C 1E       	mrr  	ar0, ac0.m
062D 1F DA       	mrr  	ac0.m, ax1.l
062E 32 80       	xorr 	ac0.m, ax1.h    	ls   	ax0.l, ac0.m
062F 74 00       	incm 	ac0             	     	
0630 22 1A       	lrs  	ax1.l, $0x001A
0631 44 00       	addr 	ac0, ax1.l      	     	
0632 00 90 00 00 	lri  	ac0.h, #0x0000
0634 02 9F 06 45 	j    	$0x0645
0636 00 9E 04 30 	lri  	ac0.m, #0x0430
0638 22 19       	lrs  	ax1.l, $0x0019
0639 44 00       	addr 	ac0, ax1.l      	     	
063A 1C 1E       	mrr  	ar0, ac0.m
063B 1F DA       	mrr  	ac0.m, ax1.l
063C 32 80       	xorr 	ac0.m, ax1.h    	ls   	ax0.l, ac0.m
063D 74 00       	incm 	ac0             	     	
063E 22 1A       	lrs  	ax1.l, $0x001A
063F 44 00       	addr 	ac0, ax1.l      	     	
0640 00 90 00 00 	lri  	ac0.h, #0x0000
0642 82 00       	cmp  	                	     	
0643 02 70       	ifge 	
0644 1F DF       	mrr  	ac0.m, ac1.m
0645 1F 3E       	mrr  	ax0.h, ac0.m
0646 02 BF 06 99 	call 	$0x0699
0648 26 1C       	lrs  	ac0.m, $0x001C
0649 24 1D       	lrs  	ac0.l, $0x001D
064A 72 00       	addaxl	ac0, ax1.l     	     	
064B 53 00       	subr 	ac1, ax0.h      	     	
064C 2E 1C       	srs  	$0x001C, ac0.m
064D 2C 1D       	srs  	$0x001D, ac0.l
064E 02 DF       	ret  	
064F 81 00       	clr  	ac0             	     	
0650 22 1C       	lrs  	ax1.l, $0x001C
0651 20 1D       	lrs  	ax0.l, $0x001D
0652 48 00       	addax	ac0, ax0        	     	
0653 14 7C       	lsr  	ac0, -4
0654 2E 1E       	srs  	$0x001E, ac0.m
0655 2C 1F       	srs  	$0x001F, ac0.l
0656 23 40       	lrs  	ax1.h, $0x0040
0657 C8 14       	mulc 	ac0.m, ax1.h    	mv   	ax0.h, ac0.l
0658 9E 00       	mulmv	ax1.l, ax1.h, ac0	     	
0659 F0 00       	lsl16	ac0             	     	
065A 4E 00       	addp 	ac0             	     	
065B 23 4C       	lrs  	ax1.h, $0x004C
065C 21 4D       	lrs  	ax0.h, $0x004D
065D 4A 00       	addax	ac0, ax1        	     	
065E 2E 20       	srs  	$0x0020, ac0.m
065F 2C 21       	srs  	$0x0021, ac0.l
0660 1F D8       	mrr  	ac0.m, ax0.l
0661 02 40 00 0F 	andi 	ac0.m, #0x000F
0663 2E 19       	srs  	$0x0019, ac0.m
0664 26 4A       	lrs  	ac0.m, $0x004A
0665 24 4B       	lrs  	ac0.l, $0x004B
0666 58 00       	subax	ac0, ax0        	     	
0667 2E 22       	srs  	$0x0022, ac0.m
0668 2C 23       	srs  	$0x0023, ac0.l
0669 02 DF       	ret  	
066A 22 1E       	lrs  	ax1.l, $0x001E
066B 20 1F       	lrs  	ax0.l, $0x001F
066C 81 00       	clr  	ac0             	     	
066D 26 4A       	lrs  	ac0.m, $0x004A
066E 24 4B       	lrs  	ac0.l, $0x004B
066F 14 7C       	lsr  	ac0, -4
0670 58 00       	subax	ac0, ax0        	     	
0671 02 95 06 7A 	jeq  	$0x067A
0673 02 BF 06 EC 	call 	$0x06EC
0675 0E 10       	lris 	ac0.m, 16
0676 2E 1A       	srs  	$0x001A, ac0.m
0677 81 00       	clr  	ac0             	     	
0678 2E 19       	srs  	$0x0019, ac0.m
0679 02 DF       	ret  	
067A 22 4A       	lrs  	ax1.l, $0x004A
067B 20 4B       	lrs  	ax0.l, $0x004B
067C 81 00       	clr  	ac0             	     	
067D 26 1C       	lrs  	ac0.m, $0x001C
067E 24 1D       	lrs  	ac0.l, $0x001D
067F 58 00       	subax	ac0, ax0        	     	
0680 02 90 06 87 	jge  	$0x0687
0682 02 BF 06 EC 	call 	$0x06EC
0684 26 23       	lrs  	ac0.m, $0x0023
0685 02 9F 06 76 	j    	$0x0676
0687 26 48       	lrs  	ac0.m, $0x0048
0688 24 49       	lrs  	ac0.l, $0x0049
0689 2E 1C       	srs  	$0x001C, ac0.m
068A 2C 1D       	srs  	$0x001D, ac0.l
068B 0E 10       	lris 	ac0.m, 16
068C 2E 1A       	srs  	$0x001A, ac0.m
068D 02 BF 06 4F 	call 	$0x064F
068F 26 42       	lrs  	ac0.m, $0x0042
0690 2E 3F       	srs  	$0x003F, ac0.m
0691 26 43       	lrs  	ac0.m, $0x0043
0692 2E 3E       	srs  	$0x003E, ac0.m
0693 81 00       	clr  	ac0             	     	
0694 00 FE 03 62 	sr   	$0x0362, ac0.m
0696 02 BF 06 EC 	call 	$0x06EC
0698 02 DF       	ret  	
0699 B1 00       	tst  	ac0             	     	
069A 02 D5       	reteq	
069B 04 FE       	addis	ac0.m, -2
069C 1F 1E       	mrr  	ax0.l, ac0.m
069D 19 1E       	lrri 	ac0.m, @ar0
069E 02 91 06 A4 	jl   	$0x06A4
06A0 19 1A       	lrri 	ax1.l, @ar0
06A1 00 58       	loop 	ax0.l
06A2 64 A0       	movr 	ac0, ax1.l      	ls   	ax1.l, ac0.m
06A3 64 33       	movr 	ac0, ax1.l      	s    	@ar3, ac0.m
06A4 1B 7E       	srri 	@ar3, ac0.m
06A5 02 DF       	ret  	
```

## Command2_10_AnotherSub1

```
06A6 00 92 00 04 	lri  	bank, #0x0004 			// BankReg at 0x400 
06A8 02 BF 06 4F 	call 	$0x064F
06AA 81 00       	clr  	ac0             	     	
06AB 00 FE 03 62 	sr   	$0x0362, ac0.m
06AD 81 00       	clr  	ac0             	     	
06AE 26 22       	lrs  	ac0.m, $0x0022
06AF 24 23       	lrs  	ac0.l, $0x0023
06B0 B1 00       	tst  	ac0             	     	
06B1 02 94 06 C3 	jne  	$0x06C3
06B3 02 BF 06 6A 	call 	$0x066A
06B5 22 19       	lrs  	ax1.l, $0x0019
06B6 86 00       	tstaxh	ax0.h          	     	
06B7 02 94 06 C0 	jne  	$0x06C0
06B9 02 BF 06 36 	call 	$0x0636
06BB B9 00       	tst  	ac1             	     	
06BC 02 95 06 E9 	jeq  	$0x06E9
06BE 02 BF 06 4F 	call 	$0x064F
06C0 81 00       	clr  	ac0             	     	
06C1 26 22       	lrs  	ac0.m, $0x0022
06C2 24 23       	lrs  	ac0.l, $0x0023
06C3 1F 1F       	mrr  	ax0.l, ac1.m
06C4 00 9A 00 00 	lri  	ax1.l, #0x0000
06C6 58 00       	subax	ac0, ax0        	     	
06C7 02 90 06 D6 	jge  	$0x06D6
06C9 81 00       	clr  	ac0             	     	
06CA 26 19       	lrs  	ac0.m, $0x0019
06CB B1 00       	tst  	ac0             	     	
06CC 02 94 06 D0 	jne  	$0x06D0
06CE 02 BF 06 6A 	call 	$0x066A
06D0 02 BF 06 28 	call 	$0x0628
06D2 02 BF 06 4F 	call 	$0x064F
06D4 02 9F 06 AD 	j    	$0x06AD
06D6 81 00       	clr  	ac0             	     	
06D7 26 19       	lrs  	ac0.m, $0x0019
06D8 B1 00       	tst  	ac0             	     	
06D9 02 94 06 DD 	jne  	$0x06DD
06DB 02 BF 06 6A 	call 	$0x066A
06DD 02 BF 06 36 	call 	$0x0636
06DF B9 00       	tst  	ac1             	     	
06E0 02 95 06 E9 	jeq  	$0x06E9
06E2 02 BF 06 4F 	call 	$0x064F
06E4 02 9F 06 D6 	j    	$0x06D6
06E6 81 00       	clr  	ac0             	     	
06E7 00 5F       	loop 	ac1.m
06E8 1B 7E       	srri 	@ar3, ac0.m
06E9 00 92 00 FF 	lri  	bank, #0x00FF
06EB 02 DF       	ret  	
06EC 00 FF 03 60 	sr   	$0x0360, ac1.m
06EE 00 DA 03 62 	lr   	ax1.l, $0x0362
06F0 86 00       	tstaxh	ax0.h          	     	
06F1 02 94 06 FE 	jne  	$0x06FE
06F3 0A 01       	lris 	ax1.l, 1
06F4 00 FA 03 62 	sr   	$0x0362, ax1.l
06F6 26 20       	lrs  	ac0.m, $0x0020
06F7 24 21       	lrs  	ac0.l, $0x0021
06F8 00 9F 00 05 	lri  	ac1.m, #0x0005 					// AC Mode
06FA 02 BF 01 03 	call 	$0x0103
06FC 00 92 00 04 	lri  	bank, #0x0004 		// BankReg at 0x400
06FE 00 80 FF D3 	lri  	ar0, #0xFFD3       			// ACDAT2
0700 00 84 00 00 	lri  	ix0, #0x0000
0702 19 9E       	lrrn 	ac0.m, @ar0
0703 1F FE       	mrr  	ac1.m, ac0.m
0704 14 01       	lsl  	ac0, #0x01
0705 02 40 00 1E 	andi 	ac0.m, #0x001E
0707 02 00 03 00 	addi 	ac0.m, #0x0300
0709 1C 3E       	mrr  	ar1, ac0.m
070A 15 7C       	lsr  	ac1, -4
070B 03 40 00 0F 	andi 	ac1.m, #0x000F
070D 0A 11       	lris 	ax1.l, 17
070E 55 00       	subr 	ac1, ax1.l      	     	
070F 00 9A 00 F0 	lri  	ax1.l, #0x00F0
0711 00 9B 00 0F 	lri  	ax1.h, #0x000F
0713 00 82 03 70 	lri  	ar2, #0x0370
0715 19 98       	lrrn 	ax0.l, @ar0
0716 60 00       	movr 	ac0, ax0.l      	     	
0717 11 07 07 1E 	bloopi	#0x07, $0x071E
	0719 34 00       	andr 	ac0.m, ax0.h    	     	
	071A 14 08       	lsl  	ac0, #0x08
	071B 60 32       	movr 	ac0, ax0.l      	s    	@ar2, ac0.m
	071C 36 44       	andr 	ac0.m, ax1.h    	ln   	ax0.l, @ar0
	071D 14 0C       	lsl  	ac0, #0x0C
	071E 60 32       	movr 	ac0, ax0.l      	s    	@ar2, ac0.m
071F 34 00       	andr 	ac0.m, ax0.h    	     	
0720 14 08       	lsl  	ac0, #0x08
0721 60 32       	movr 	ac0, ax0.l      	s    	@ar2, ac0.m
0722 36 00       	andr 	ac0.m, ax1.h    	     	
0723 14 0C       	lsl  	ac0, #0x0C
0724 1B 5E       	srri 	@ar2, ac0.m
0725 8F 00       	set40	                	     	
0726 1F 7F       	mrr  	ax1.h, ac1.m
0727 20 3E       	lrs  	ax0.l, $0x003E
0728 27 3F       	lrs  	ac1.m, $0x003F
0729 19 3A       	lrri 	ax1.l, @ar1
072A 19 39       	lrri 	ax0.h, @ar1
072B 00 80 03 70 	lri  	ar0, #0x0370
072D 00 81 04 30 	lri  	ar1, #0x0430
072F 1C 80       	mrr  	ix0, ar0
0730 A0 00       	mulx 	ax0.l, ax1.l    	     	
0731 EA 70       	maddc	ac1.m, ax0.h    	l    	ac0.m, @ar0
0732 11 08 07 3B 	bloopi	#0x08, $0x073B
	0734 3A 93       	orr  	ac0.m, ax1.h    	sl   	ac1.m, ax0.h
	0735 A4 78       	mulxac	ax0.l, ax1.l, ac0	l    	ac1.m, @ar0
	0736 14 85       	asl  	ac0, #0x05
	0737 E8 31       	maddc	ac0.m, ax0.h    	s    	@ar1, ac0.m
	0738 3B 92       	orr  	ac1.m, ax1.h    	sl   	ac0.m, ax0.h
	0739 A5 70       	mulxac	ax0.l, ax1.l, ac1	l    	ac0.m, @ar0
	073A 15 85       	asl  	ac1, #0x05
	073B EA 39       	maddc	ac1.m, ax0.h    	s    	@ar1, ac1.m
073C 8E 00       	clr40	                	     	
073D 89 00       	clr  	ac1             	     	
073E 00 DF 03 60 	lr   	ac1.m, $0x0360
0740 02 DF       	ret  	
```

## Case 10

```
0741 00 80 03 46 	lri  	ar0, #0x0346
0743 02 BF 00 51 	call 	$0x0051 				// ReadCpuMailbox
0745 81 00       	clr  	ac0             	     	
0746 00 80 04 30 	lri  	ar0, #0x0430
0748 10 10       	loopi	#0x10
0749 1B 1E       	srri 	@ar0, ac0.m
074A 00 FE 04 42 	sr   	$0x0442, ac0.m
074C 00 FE 04 43 	sr   	$0x0443, ac0.m
074E 00 9C 00 00 	lri  	ac0.l, #0x0000
0750 00 FE 04 1C 	sr   	$0x041C, ac0.m
0752 00 FC 04 1D 	sr   	$0x041D, ac0.l
0754 00 9E 01 00 	lri  	ac0.m, #0x0100
0756 00 9C F1 00 	lri  	ac0.l, #0xF100
0758 00 FE 04 4E 	sr   	$0x044E, ac0.m
075A 00 FC 04 4F 	sr   	$0x044F, ac0.l
075C 00 9E 00 40 	lri  	ac0.m, #0x0040
075E 00 9C 00 00 	lri  	ac0.l, #0x0000
0760 00 FE 04 4C 	sr   	$0x044C, ac0.m
0762 00 FC 04 4D 	sr   	$0x044D, ac0.l
0764 00 9E 00 09 	lri  	ac0.m, #0x0009
0766 00 FE 04 40 	sr   	$0x0440, ac0.m
0768 00 9E 00 10 	lri  	ac0.m, #0x0010
076A 00 FE 04 1A 	sr   	$0x041A, ac0.m
076C 00 9E 01 00 	lri  	ac0.m, #0x0100
076E 00 9C F2 50 	lri  	ac0.l, #0xF250
0770 00 FE 04 4A 	sr   	$0x044A, ac0.m
0772 00 FC 04 4B 	sr   	$0x044B, ac0.l
0774 00 9C 00 00 	lri  	ac0.l, #0x0000
0776 00 FE 04 48 	sr   	$0x0448, ac0.m
0778 00 FC 04 49 	sr   	$0x0449, ac0.l
077A 00 9E 00 01 	lri  	ac0.m, #0x0001
077C 00 FE 04 41 	sr   	$0x0441, ac0.m
077E 89 00       	clr  	ac1             	     	
077F 00 FF 04 01 	sr   	$0x0401, ac1.m
0781 11 80 07 9B 	bloopi	#0x80, $0x079B
	0783 00 83 05 80 	lri  	ar3, #0x0580
	0785 00 9F 01 00 	lri  	ac1.m, #0x0100
	0787 02 BF 06 A6 	call 	$0x06A6 					// Command2_10_AnotherSub1
	0789 00 81 03 46 	lri  	ar1, #0x0346
	078B 19 3E       	lrri 	ac0.m, @ar1
	078C 18 BC       	lrrd 	ac0.l, @ar1
	078D 00 9F 05 80 	lri  	ac1.m, #0x0580  		// DspAddr 0x580
	078F 00 80 01 00 	lri  	ar0, #0x0100
	0791 02 BF 00 8B 	call 	$0x008B 					// DspWriteMainMem (ac0: MainMemAddr)
	0793 00 81 03 46 	lri  	ar1, #0x0346
	0795 19 3E       	lrri 	ac0.m, @ar1
	0796 18 BC       	lrrd 	ac0.l, @ar1
	0797 00 98 02 00 	lri  	ax0.l, #0x0200
	0799 70 00       	addaxl	ac0, ax0.l     	     	
	079A 1B 3E       	srri 	@ar1, ac0.m
	079B 1A BC       	srrd 	@ar1, ac0.l
079C 02 9F 00 49 	j    	$0x0049
079E 89 00       	clr  	ac1             	     	
079F 00 9F 00 50 	lri  	ac1.m, #0x0050
07A1 00 83 05 20 	lri  	ar3, #0x0520
07A3 02 BF 07 B9 	call 	$0x07B9
07A5 02 9F 04 E7 	j    	$0x04E7
07A7 00 D8 04 02 	lr   	ax0.l, $0x0402
07A9 81 00       	clr  	ac0             	     	
07AA 89 00       	clr  	ac1             	     	
07AB 00 DC 04 18 	lr   	ac0.l, $0x0418
07AD 00 9A 00 50 	lri  	ax1.l, #0x0050
07AF 90 00       	mul  	ax0.l, ax0.h    	     	
07B0 94 00       	mulac	ax0.l, ax0.h, ac0	     	
07B1 14 04       	lsl  	ac0, #0x04
07B2 1F FE       	mrr  	ac1.m, ac0.m
07B3 00 83 05 80 	lri  	ar3, #0x0580
07B5 02 BF 07 B9 	call 	$0x07B9
07B7 02 9F 04 DF 	j    	$0x04DF
07B9 00 92 00 04 	lri  	bank, #0x0004 				// BankReg at 0x400
07BB 81 00       	clr  	ac0             	     	
07BC 26 22       	lrs  	ac0.m, $0x0022
07BD 24 23       	lrs  	ac0.l, $0x0023
07BE 1F 1F       	mrr  	ax0.l, ac1.m
07BF 00 9A 00 00 	lri  	ax1.l, #0x0000
07C1 58 00       	subax	ac0, ax0        	     	
07C2 02 90 07 D9 	jge  	$0x07D9
07C4 89 00       	clr  	ac1             	     	
07C5 00 C0 04 23 	lr   	ar0, $0x0423
07C7 02 BF 07 FE 	call 	$0x07FE
07C9 81 00       	clr  	ac0             	     	
07CA 1F D8       	mrr  	ac0.m, ax0.l
07CB 22 23       	lrs  	ax1.l, $0x0023
07CC 54 00       	subr 	ac0, ax1.l      	     	
07CD 00 07       	dar  	ar3
07CE 19 79       	lrri 	ax0.h, @ar3
07CF 00 5E       	loop 	ac0.m
07D0 1B 79       	srri 	@ar3, ax0.h
07D1 00 9F 00 01 	lri  	ac1.m, #0x0001
07D3 2F 01       	srs  	$0x0001, ac1.m
07D4 89 00       	clr  	ac1             	     	
07D5 2F 23       	srs  	$0x0023, ac1.m
07D6 00 92 00 FF 	lri  	bank, #0x00FF
07D8 02 DF       	ret  	
07D9 2E 22       	srs  	$0x0022, ac0.m
07DA 2C 23       	srs  	$0x0023, ac0.l
07DB 81 00       	clr  	ac0             	     	
07DC 89 00       	clr  	ac1             	     	
07DD 26 4A       	lrs  	ac0.m, $0x004A
07DE 27 1C       	lrs  	ac1.m, $0x001C
07DF 5C 00       	sub  	ac0, ac1        	     	
07E0 2E 1E       	srs  	$0x001E, ac0.m
07E1 50 00       	subr 	ac0, ax0.l      	     	
07E2 02 90 07 F8 	jge  	$0x07F8
07E4 00 C0 04 1E 	lr   	ar0, $0x041E
07E6 02 BF 07 FE 	call 	$0x07FE
07E8 81 00       	clr  	ac0             	     	
07E9 1F D8       	mrr  	ac0.m, ax0.l
07EA 22 1E       	lrs  	ax1.l, $0x001E
07EB 54 00       	subr 	ac0, ax1.l      	     	
07EC 1C 1E       	mrr  	ar0, ac0.m
07ED 81 00       	clr  	ac0             	     	
07EE 2E 1C       	srs  	$0x001C, ac0.m
07EF 26 48       	lrs  	ac0.m, $0x0048
07F0 24 49       	lrs  	ac0.l, $0x0049
07F1 2E 4C       	srs  	$0x004C, ac0.m
07F2 2C 4D       	srs  	$0x004D, ac0.l
07F3 02 BF 07 FE 	call 	$0x07FE
07F5 00 92 00 FF 	lri  	bank, #0x00FF
07F7 02 DF       	ret  	
07F8 1C 18       	mrr  	ar0, ax0.l
07F9 02 BF 07 FE 	call 	$0x07FE
07FB 00 92 00 FF 	lri  	bank, #0x00FF
07FD 02 DF       	ret  	
07FE 81 00       	clr  	ac0             	     	
07FF 1F C0       	mrr  	ac0.m, ar0
0800 B1 00       	tst  	ac0             	     	
0801 02 D5       	reteq	
0802 89 00       	clr  	ac1             	     	
0803 27 1C       	lrs  	ac1.m, $0x001C
0804 03 40 00 01 	andi 	ac1.m, #0x0001
0806 00 9B 00 00 	lri  	ax1.h, #0x0000
0808 1F 3F       	mrr  	ax0.h, ac1.m
0809 26 4C       	lrs  	ac0.m, $0x004C
080A 24 4D       	lrs  	ac0.l, $0x004D
080B 89 00       	clr  	ac1             	     	
080C 25 1C       	lrs  	ac1.l, $0x001C
080D 15 01       	lsl  	ac1, #0x01
080E 4C 00       	add  	ac0, ac1        	     	
080F 5A 00       	subax	ac0, ax1        	     	
0810 5A 00       	subax	ac0, ax1        	     	
0811 1C 20       	mrr  	ar1, ar0
0812 1F E0       	mrr  	ac1.m, ar0
0813 05 02       	addis	ac1.m, 2
0814 1C 1F       	mrr  	ar0, ac1.m
0815 00 9F 0A 00 	lri  	ac1.m, #0x0A00
0817 00 92 00 FF 	lri  	bank, #0x00FF
0819 02 BF 00 7E 	call 	$0x007E 					// DspReadMainMem (ac0 - MainMem address)
081B 00 92 00 04 	lri  	bank, #0x0004 			// BankReg at 0x400
081D 27 1C       	lrs  	ac1.m, $0x001C
081E 1F 61       	mrr  	ax1.h, ar1
081F 47 00       	addr 	ac1, ax1.h      	     	
0820 2F 1C       	srs  	$0x001C, ac1.m
0821 00 80 0A 00 	lri  	ar0, #0x0A00
0823 89 00       	clr  	ac1             	     	
0824 1F F9       	mrr  	ac1.m, ax0.h
0825 B9 00       	tst  	ac1             	     	
0826 02 74       	ifne 	
0827 00 08       	iar  	ar0
0828 89 00       	clr  	ac1             	     	
0829 1F E1       	mrr  	ac1.m, ar1
082A 19 1E       	lrri 	ac0.m, @ar0
082B 07 01       	cmpis	ac1.m, 1
082C 02 93 08 35 	jle  	$0x0835
082E 19 1A       	lrri 	ax1.l, @ar0
082F 05 FE       	addis	ac1.m, -2
0830 00 5F       	loop 	ac1.m
0831 64 A0       	movr 	ac0, ax1.l      	ls   	ax1.l, ac0.m
0832 1B 7E       	srri 	@ar3, ac0.m
0833 1B 7A       	srri 	@ar3, ax1.l
0834 02 DF       	ret  	
0835 1B 7E       	srri 	@ar3, ac0.m
0836 02 DF       	ret  	
0837 00 92 00 04 	lri  	bank, #0x0004 				// BankReg at 0x400
0839 22 01       	lrs  	ax1.l, $0x0001
083A 86 00       	tstaxh	ax0.h          	     	
083B 02 94 08 68 	jne  	$0x0868
083D 22 04       	lrs  	ax1.l, $0x0004
083E 86 00       	tstaxh	ax0.h          	     	
083F 02 B4 08 BC 	callne	$0x08BC 						// Command10_Sub1
0841 22 19       	lrs  	ax1.l, $0x0019
0842 86 00       	tstaxh	ax0.h          	     	
0843 02 95 08 5D 	jeq  	$0x085D
0845 00 9E 04 30 	lri  	ac0.m, #0x0430
0847 44 00       	addr 	ac0, ax1.l      	     	
0848 1C 1E       	mrr  	ar0, ac0.m
0849 0E 10       	lris 	ac0.m, 16
084A 54 00       	subr 	ac0, ax1.l      	     	
084B 1F 7E       	mrr  	ax1.h, ac0.m
084C 02 BF 06 99 	call 	$0x0699
084E D9 00       	cmpar	ac1.m, ax1.h      	     	
084F 02 92 08 5C 	jg   	$0x085C
0851 02 95 08 58 	jeq  	$0x0858
0853 26 19       	lrs  	ac0.m, $0x0019
0854 4C 00       	add  	ac0, ac1        	     	
0855 2E 19       	srs  	$0x0019, ac0.m
0856 02 9F 08 B9 	j    	$0x08B9
0858 81 00       	clr  	ac0             	     	
0859 2E 19       	srs  	$0x0019, ac0.m
085A 02 9F 08 B9 	j    	$0x08B9
085C 57 00       	subr 	ac1, ax1.h      	     	
085D 81 00       	clr  	ac0             	     	
085E 26 05       	lrs  	ac0.m, $0x0005
085F B1 00       	tst  	ac0             	     	
0860 02 95 08 79 	jeq  	$0x0879
0862 81 00       	clr  	ac0             	     	
0863 2E 05       	srs  	$0x0005, ac0.m
0864 22 41       	lrs  	ax1.l, $0x0041
0865 86 00       	tstaxh	ax0.h          	     	
0866 02 94 08 6F 	jne  	$0x086F
0868 81 00       	clr  	ac0             	     	
0869 00 5F       	loop 	ac1.m
086A 1B 7E       	srri 	@ar3, ac0.m
086B 74 00       	incm 	ac0             	     	
086C 2E 01       	srs  	$0x0001, ac0.m
086D 02 9F 08 B9 	j    	$0x08B9
086F 26 48       	lrs  	ac0.m, $0x0048
0870 24 49       	lrs  	ac0.l, $0x0049
0871 2E 1C       	srs  	$0x001C, ac0.m
0872 2C 1D       	srs  	$0x001D, ac0.l
0873 02 BF 08 C1 	call 	$0x08C1
0875 26 42       	lrs  	ac0.m, $0x0042
0876 24 43       	lrs  	ac0.l, $0x0043
0877 2E 3F       	srs  	$0x003F, ac0.m
0878 2C 3E       	srs  	$0x003E, ac0.l
0879 00 FF 03 60 	sr   	$0x0360, ac1.m
087B 26 20       	lrs  	ac0.m, $0x0020
087C 24 21       	lrs  	ac0.l, $0x0021
087D 00 9F 00 05 	lri  	ac1.m, #0x0005 					// AC Mode
087F 02 BF 01 03 	call 	$0x0103
0881 00 92 00 04 	lri  	bank, #0x0004 			// BankReg at 0x400
0883 89 00       	clr  	ac1             	     	
0884 00 FF 03 62 	sr   	$0x0362, ac1.m
0886 00 DF 03 60 	lr   	ac1.m, $0x0360
0888 02 BF 08 DB 	call 	$0x08DB
088A 81 00       	clr  	ac0             	     	
088B 00 DE 03 62 	lr   	ac0.m, $0x0362
088D 22 40       	lrs  	ax1.l, $0x0040
088E 44 00       	addr 	ac0, ax1.l      	     	
088F 00 FE 03 62 	sr   	$0x0362, ac0.m
0891 81 00       	clr  	ac0             	     	
0892 26 22       	lrs  	ac0.m, $0x0022
0893 24 23       	lrs  	ac0.l, $0x0023
0894 0A 01       	lris 	ax1.l, 1
0895 00 81 04 05 	lri  	ar1, #0x0405
0897 7A 00       	dec  	ac0             	     	
0898 B1 00       	tst  	ac0             	     	
0899 02 75       	ifeq 	
089A 1A 3A       	srr  	@ar1, ax1.l
089B 2E 22       	srs  	$0x0022, ac0.m
089C 2C 23       	srs  	$0x0023, ac0.l
089D 07 10       	cmpis	ac1.m, 16
089E 02 93 08 A7 	jle  	$0x08A7
08A0 05 F0       	addis	ac1.m, -16
08A1 22 05       	lrs  	ax1.l, $0x0005
08A2 86 00       	tstaxh	ax0.h          	     	
08A3 02 94 08 62 	jne  	$0x0862
08A5 02 9F 08 88 	j    	$0x0888
08A7 02 75       	ifeq 	
08A8 89 00       	clr  	ac1             	     	
08A9 2F 19       	srs  	$0x0019, ac1.m
08AA 1F C3       	mrr  	ac0.m, ar3
08AB 04 F0       	addis	ac0.m, -16
08AC 1C 1E       	mrr  	ar0, ac0.m
08AD 00 83 04 30 	lri  	ar3, #0x0430
08AF 0E 10       	lris 	ac0.m, 16
08B0 02 BF 06 99 	call 	$0x0699
08B2 26 20       	lrs  	ac0.m, $0x0020
08B3 24 21       	lrs  	ac0.l, $0x0021
08B4 00 D8 03 62 	lr   	ax0.l, $0x0362
08B6 70 00       	addaxl	ac0, ax0.l     	     	
08B7 2C 21       	srs  	$0x0021, ac0.l
08B8 2E 20       	srs  	$0x0020, ac0.m
08B9 00 92 00 FF 	lri  	bank, #0x00FF
08BB 02 DF       	ret  	
```

## Command10_Sub1

```
08BC 81 00       	clr  	ac0             	     	
08BD 2E 1C       	srs  	$0x001C, ac0.m
08BE 2E 1D       	srs  	$0x001D, ac0.m
08BF 2E 3E       	srs  	$0x003E, ac0.m
08C0 2E 3F       	srs  	$0x003F, ac0.m
08C1 23 1C       	lrs  	ax1.h, $0x001C
08C2 21 1D       	lrs  	ax0.h, $0x001D
08C3 26 4A       	lrs  	ac0.m, $0x004A
08C4 24 4B       	lrs  	ac0.l, $0x004B
08C5 5A 00       	subax	ac0, ax1        	     	
08C6 14 7C       	lsr  	ac0, -4
08C7 2E 22       	srs  	$0x0022, ac0.m
08C8 2C 23       	srs  	$0x0023, ac0.l
08C9 26 1C       	lrs  	ac0.m, $0x001C
08CA 24 1D       	lrs  	ac0.l, $0x001D
08CB 14 7C       	lsr  	ac0, -4
08CC 22 40       	lrs  	ax1.l, $0x0040
08CD C0 10       	mulc 	ac0.m, ax0.h    	mv   	ax0.l, ac0.l
08CE 96 00       	mulmv	ax0.l, ax0.h, ac0	     	
08CF F0 00       	lsl16	ac0             	     	
08D0 4E 00       	addp 	ac0             	     	
08D1 23 4C       	lrs  	ax1.h, $0x004C
08D2 21 4D       	lrs  	ax0.h, $0x004D
08D3 4A 00       	addax	ac0, ax1        	     	
08D4 2E 20       	srs  	$0x0020, ac0.m
08D5 2C 21       	srs  	$0x0021, ac0.l
08D6 81 00       	clr  	ac0             	     	
08D7 2E 05       	srs  	$0x0005, ac0.m
08D8 2E 19       	srs  	$0x0019, ac0.m
08D9 2E 04       	srs  	$0x0004, ac0.m
08DA 02 DF       	ret  	
08DB 00 FF 03 60 	sr   	$0x0360, ac1.m
08DD 00 80 FF D3 	lri  	ar0, #0xFFD3 				// ACDAT2
08DF 00 84 00 00 	lri  	ix0, #0x0000
08E1 19 9E       	lrrn 	ac0.m, @ar0
08E2 1F FE       	mrr  	ac1.m, ac0.m
08E3 14 01       	lsl  	ac0, #0x01
08E4 02 40 00 1E 	andi 	ac0.m, #0x001E
08E6 02 00 03 00 	addi 	ac0.m, #0x0300
08E8 1C 3E       	mrr  	ar1, ac0.m
08E9 15 7C       	lsr  	ac1, -4
08EA 03 40 00 0F 	andi 	ac1.m, #0x000F
08EC 0A 11       	lris 	ax1.l, 17
08ED 55 00       	subr 	ac1, ax1.l      	     	
08EE 00 9A 00 F0 	lri  	ax1.l, #0x00F0
08F0 00 9B 00 0F 	lri  	ax1.h, #0x000F
08F2 00 82 03 70 	lri  	ar2, #0x0370
08F4 19 98       	lrrn 	ax0.l, @ar0
08F5 60 00       	movr 	ac0, ax0.l      	     	
08F6 11 07 08 FD 	bloopi	#0x07, $0x08FD
	08F8 34 00       	andr 	ac0.m, ax0.h    	     	
	08F9 14 08       	lsl  	ac0, #0x08
	08FA 60 32       	movr 	ac0, ax0.l      	s    	@ar2, ac0.m
	08FB 36 44       	andr 	ac0.m, ax1.h    	ln   	ax0.l, @ar0
	08FC 14 0C       	lsl  	ac0, #0x0C
	08FD 60 32       	movr 	ac0, ax0.l      	s    	@ar2, ac0.m
08FE 34 00       	andr 	ac0.m, ax0.h    	     	
08FF 14 08       	lsl  	ac0, #0x08
0900 60 32       	movr 	ac0, ax0.l      	s    	@ar2, ac0.m
0901 36 00       	andr 	ac0.m, ax1.h    	     	
0902 14 0C       	lsl  	ac0, #0x0C
0903 1B 5E       	srri 	@ar2, ac0.m
0904 8F 00       	set40	                	     	
0905 1F 7F       	mrr  	ax1.h, ac1.m
0906 20 3E       	lrs  	ax0.l, $0x003E
0907 27 3F       	lrs  	ac1.m, $0x003F
0908 19 3A       	lrri 	ax1.l, @ar1
0909 19 39       	lrri 	ax0.h, @ar1
090A 00 80 03 70 	lri  	ar0, #0x0370
090C 1C 80       	mrr  	ix0, ar0
090D A0 00       	mulx 	ax0.l, ax1.l    	     	
090E EA 70       	maddc	ac1.m, ax0.h    	l    	ac0.m, @ar0
090F 3A 93       	orr  	ac0.m, ax1.h    	sl   	ac1.m, ax0.h
0910 A4 78       	mulxac	ax0.l, ax1.l, ac0	l    	ac1.m, @ar0
0911 14 85       	asl  	ac0, #0x05
0912 E8 33       	maddc	ac0.m, ax0.h    	s    	@ar3, ac0.m
0913 3B 92       	orr  	ac1.m, ax1.h    	sl   	ac0.m, ax0.h
0914 A5 70       	mulxac	ax0.l, ax1.l, ac1	l    	ac0.m, @ar0
0915 15 85       	asl  	ac1, #0x05
0916 EA 3B       	maddc	ac1.m, ax0.h    	s    	@ar3, ac1.m
0917 11 06 09 20 	bloopi	#0x06, $0x0920
	0919 3A 93       	orr  	ac0.m, ax1.h    	sl   	ac1.m, ax0.h
	091A A4 78       	mulxac	ax0.l, ax1.l, ac0	l    	ac1.m, @ar0
	091B 14 85       	asl  	ac0, #0x05
	091C E8 33       	maddc	ac0.m, ax0.h    	s    	@ar3, ac0.m
	091D 3B 92       	orr  	ac1.m, ax1.h    	sl   	ac0.m, ax0.h
	091E A5 70       	mulxac	ax0.l, ax1.l, ac1	l    	ac0.m, @ar0
	091F 15 85       	asl  	ac1, #0x05
	0920 EA 3B       	maddc	ac1.m, ax0.h    	s    	@ar3, ac1.m
0921 3A 93       	orr  	ac0.m, ax1.h    	sl   	ac1.m, ax0.h
0922 A4 78       	mulxac	ax0.l, ax1.l, ac0	l    	ac1.m, @ar0
0923 14 85       	asl  	ac0, #0x05
0924 E8 33       	maddc	ac0.m, ax0.h    	s    	@ar3, ac0.m
0925 3B 92       	orr  	ac1.m, ax1.h    	sl   	ac0.m, ax0.h
0926 A5 00       	mulxac	ax0.l, ax1.l, ac1	     	
0927 15 85       	asl  	ac1, #0x05
0928 1B 7F       	srri 	@ar3, ac1.m
0929 2E 3E       	srs  	$0x003E, ac0.m
092A 2F 3F       	srs  	$0x003F, ac1.m
092B 8E 00       	clr40	                	     	
092C 89 00       	clr  	ac1             	     	
092D 00 DF 03 60 	lr   	ac1.m, $0x0360
092F 02 DF       	ret  	
```

```
0930 00 83 05 20 	lri  	ar3, #0x0520
0932 00 DE 04 1B 	lr   	ac0.m, $0x041B
0934 10 50       	loopi	#0x50
0935 1B 7E       	srri 	@ar3, ac0.m
0936 02 9F 04 E7 	j    	$0x04E7
0938 00 00       	nop  	
0939 00 00       	nop  	
093A 00 00       	nop  	
093B 00 00       	nop  	
093C 00 00       	nop  	
093D 00 00       	nop  	
093E 00 00       	nop  	
093F 00 00       	nop  	
```
