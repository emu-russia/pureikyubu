# Disassembled OSInitAudioSystem DSPInitCode

```
0000 02 9F 00 10 	j    	$0x0010
0002 02 9F 00 35 	j    	$0x0035
0004 02 9F 00 36 	j    	$0x0036
0006 02 9F 00 37 	j    	$0x0037
0008 02 9F 00 38 	j    	$0x0038
000A 02 9F 00 39 	j    	$0x0039
000C 02 9F 00 3A 	j    	$0x003A
000E 02 9F 00 3B 	j    	$0x003B
```

## Reset

```
0010 12 06       	sbset	12
0011 12 03       	sbset	9
0012 12 04       	sbset	10
0013 12 05       	sbset	11
0014 8E 00       	clr40	                	     	
0015 00 92 00 FF 	lri  	config, #0x00FF
0017 00 80 80 00 	lri  	ar0, #0x8000
0019 00 88 FF FF 	lri  	lm0, #0xFFFF
001B 00 84 10 00 	lri  	ix0, #0x1000

001D 00 64 00 20 	bloop	ix0, $0x0020
001F 02 18       	ilrri	ac0.m, @ar0  			// Probe IROM by ILRR instruction
0020 00 00       	nop

0021 00 80 10 00 	lri  	ar0, #0x1000
0023 00 88 FF FF 	lri  	lm0, #0xFFFF
0025 00 84 08 00 	lri  	ix0, #0x0800

0027 00 64 00 2A 	bloop	ix0, $0x002A
0029 19 1E       	lrri 	ac0.m, @ar0 			// Probe DROM
002A 00 00       	nop  	

002B 26 FC       	lrs  	ac0.m, $(DMBH)
002C 02 A0 80 00 	tclr 	ac0.m, #0x8000
002E 02 9C 00 2B 	jnok 	$0x002B

0030 16 FC 00 54 	si   	$(DMBH), #0x0054
0032 16 FD 43 48 	si   	$(DMBL), #0x4348
0034 00 21       	halt
```

```c++
void Reset () 		// 0010
{
	sr.unk12 = 1;
	sr.ie = 1;
	sr.eie = 1;
	sr.unk10 = 1;

	clr40();
	config = 0xFF;

	// Probe 0x8000 reads (8 Kbytes)  IROM

	ar0 = 0x8000;
	lm0 = 0xFFFF;
	ix0 = 0x1000;
	while (ix0--)
	{
		ac0m = *ar0++; 			// via ILRR
	}

	// Probe 0x1000 reads (4 Kbytes)   DROM

	ar0 = 0x1000;
	lm0 = 0xFFFF;
	ix0 = 0x800;
	while (ix0--)
	{
		ac0m = *ar0++;
	}

	while ( (DMBH & 0x8000) != 0 ) ;

	DMBH = 0x0054;
	DMBL = 0x4348;

	halt;
}
``` 	

## Interrupt stubs

```
0035 02 FF       	rti  	
0036 02 FF       	rti  	
0037 02 FF       	rti  	
0038 02 FF       	rti  	
0039 02 FF       	rti  	
003A 02 FF       	rti  	
003B 02 FF       	rti  	
003C 00 00       	nop  	
003D 00 00       	nop  	
003E 00 00       	nop  	
003F 00 00       	nop  	
```
