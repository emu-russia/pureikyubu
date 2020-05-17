# AX UCode from Dolphin SDK AX.lib

Disassembled Data\dspucode_19e0.bin

```
0000 00 00       	nop  	
0001 00 00       	nop  	
0002 02 9F 0C 10 	j    	$0x0C10
0004 02 9F 0C 1F 	j    	$0x0C1F
0006 02 9F 0C 3B 	j    	$0x0C3B
0008 02 9F 0C 4A 	j    	$0x0C4A
000A 02 9F 0C 50 	j    	$0x0C50
000C 02 9F 0C 82 	j    	$0x0C82
000E 02 9F 0C 88 	j    	$0x0C88
```

```
0010 13 02       	sbclr	8
0011 13 03       	sbclr	9
0012 12 04       	sbset	10
0013 13 05       	sbclr	11
0014 13 06       	sbclr	12
0015 8E 00       	clr40	                	     	
0016 8C 00       	clr15	                	     	
0017 8B 00       	m0   	                	     	
0018 00 92 00 FF 	lri  	bank, #0x00FF
001A 81 00       	clr  	ac0             	     	
001B 89 00       	clr  	ac1             	     	
001C 00 9E 0E 80 	lri  	ac0.m, #0x0E80
001E 00 FE 0E 1B 	sr   	$0x0E1B, ac0.m
0020 81 00       	clr  	ac0             	     	
0021 00 FE 0E 31 	sr   	$0x0E31, ac0.m
0023 16 FC DC D1 	si   	$(DMBH), #0xDCD1
0025 16 FD 00 00 	si   	$(DMBL), #0x0000
0027 16 FB 00 01 	si   	$(DIRQ), #0x0001
0029 26 FC       	lrs  	ac0.m, $(DMBH)
002A 02 A0 80 00 	tclr 	ac0.m, #0x8000
002C 02 9C 00 29 	jnok 	$0x0029
002E 02 9F 00 45 	j    	$0x0045
```

```
0030 13 02       	sbclr	8
0031 13 03       	sbclr	9
0032 12 04       	sbset	10
0033 13 05       	sbclr	11
0034 13 06       	sbclr	12
0035 8E 00       	clr40	                	     	
0036 8C 00       	clr15	                	     	
0037 8B 00       	m0   	                	     	
0038 00 92 00 FF 	lri  	bank, #0x00FF
003A 16 FC DC D1 	si   	$(DMBH), #0xDCD1
003C 16 FD 00 01 	si   	$(DMBL), #0x0001
003E 16 FB 00 01 	si   	$(DIRQ), #0x0001
0040 26 FC       	lrs  	ac0.m, $(DMBH)
0041 02 A0 80 00 	tclr 	ac0.m, #0x8000
0043 02 9C 00 40 	jnok 	$0x0040
0045 8E 00       	clr40	                	     	
0046 81 00       	clr  	ac0             	     	
0047 89 00       	clr  	ac1             	     	
0048 00 9F BA BE 	lri  	ac1.m, #0xBABE
004A 26 FE       	lrs  	ac0.m, $(CMBH)
004B 02 C0 80 00 	tset 	ac0.m, #0x8000
004D 02 9C 00 4A 	jnok 	$0x004A
004F 82 00       	cmp  	                	     	
0050 02 94 00 4A 	jne  	$0x004A
0052 23 FF       	lrs  	ax1.h, $(CMBL)
0053 81 00       	clr  	ac0             	     	
0054 26 FE       	lrs  	ac0.m, $(CMBH)
0055 02 C0 80 00 	tset 	ac0.m, #0x8000
0057 02 9C 00 54 	jnok 	$0x0054
0059 27 FF       	lrs  	ac1.m, $(CMBL)
005A 02 40 7F FF 	andi 	ac0.m, #0x7FFF
005C 2E CE       	srs  	$(DSMAH), ac0.m
005D 2F CF       	srs  	$(DSMAL), ac1.m
005E 16 CD 0C 00 	si   	$(DSPA), #0x0C00
0060 81 00       	clr  	ac0             	     	
0061 2E C9       	srs  	$(DSCR), ac0.m
0062 1F FB       	mrr  	ac1.m, ax1.h
0063 2F CB       	srs  	$(DSBL), ac1.m
0064 02 BF 05 5C 	call 	$0x055C
0066 00 80 0C 00 	lri  	ar0, #0x0C00
0068 8E 00       	clr40	                	     	
0069 81 00       	clr  	ac0             	     	
006A 89 70       	clr  	ac1             	l    	ac0.m, @ar0
006B B1 00       	tst  	ac0             	     	
006C 02 91 00 7E 	jl   	$0x007E
006E 0A 12       	lris 	ax0.h, 18
006F C1 00       	cmpar	ac0.m, ax0.h    	     	
0070 02 92 00 7E 	jg   	$0x007E
0072 00 9F 0A FF 	lri  	ac1.m, #0x0AFF
0074 4C 00       	add  	ac0, ac1        	     	
0075 1C 7E       	mrr  	ar3, ac0.m
0076 02 13       	ilrr 	ac0.m, @ar3
0077 1C 7E       	mrr  	ar3, ac0.m
0078 17 6F       	jmpr 	ar3
```

```
0079 16 FC FB AD 	si   	$(DMBH), #0xFBAD
007B 16 FD 80 80 	si   	$(DMBL), #0x8080
007D 00 21       	halt 	
007E 16 FC BA AD 	si   	$(DMBH), #0xBAAD
0080 2E FD       	srs  	$(DMBL), ac0.m
0081 00 21       	halt 	
```

## Command 0

```
0082 81 00       	clr  	ac0             	     	
0083 89 70       	clr  	ac1             	l    	ac0.m, @ar0
0084 8E 78       	clr40	                	l    	ac1.m, @ar0
0085 2E CE       	srs  	$(DSMAH), ac0.m
0086 2F CF       	srs  	$(DSMAL), ac1.m
0087 00 9E 0E 44 	lri  	ac0.m, #0x0E44
0089 2E CD       	srs  	$(DSPA), ac0.m
008A 0E 00       	lris 	ac0.m, 0
008B 2E C9       	srs  	$(DSCR), ac0.m
008C 00 9E 00 40 	lri  	ac0.m, #0x0040
008E 2E CB       	srs  	$(DSBL), ac0.m
008F 00 81 0E 44 	lri  	ar1, #0x0E44
0091 00 82 00 00 	lri  	ar2, #0x0000
0093 00 9B 00 9F 	lri  	ax1.h, #0x009F
0095 00 9A 01 40 	lri  	ax0.h, #0x0140
0097 81 00       	clr  	ac0             	     	
0098 89 00       	clr  	ac1             	     	
0099 8F 00       	set40	                	     	
009A 02 BF 05 5C 	call 	$0x055C
009C 19 3E       	lrri 	ac0.m, @ar1
009D 19 3C       	lrri 	ac0.l, @ar1
009E B1 00       	tst  	ac0             	     	
009F 19 3F       	lrri 	ac1.m, @ar1
00A0 02 94 00 A6 	jne  	$0x00A6
00A2 00 5A       	loop 	ax0.h
00A3 1B 5E       	srri 	@ar2, ac0.m
00A4 02 9F 00 AE 	j    	$0x00AE
00A6 99 00       	asr16	ac1             	     	
00A7 1B 5E       	srri 	@ar2, ac0.m
00A8 1B 5C       	srri 	@ar2, ac0.l
00A9 00 7B 00 AD 	bloop	ax1.h, $0x00AD
00AB 4C 00       	add  	ac0, ac1        	     	
00AC 1B 5E       	srri 	@ar2, ac0.m
00AD 1B 5C       	srri 	@ar2, ac0.l
00AE 19 3E       	lrri 	ac0.m, @ar1
00AF 19 3C       	lrri 	ac0.l, @ar1
00B0 B1 00       	tst  	ac0             	     	
00B1 19 3F       	lrri 	ac1.m, @ar1
00B2 02 94 00 B8 	jne  	$0x00B8
00B4 00 5A       	loop 	ax0.h
00B5 1B 5E       	srri 	@ar2, ac0.m
00B6 02 9F 00 C0 	j    	$0x00C0
00B8 99 00       	asr16	ac1             	     	
00B9 1B 5E       	srri 	@ar2, ac0.m
00BA 1B 5C       	srri 	@ar2, ac0.l
00BB 00 7B 00 BF 	bloop	ax1.h, $0x00BF
00BD 4C 00       	add  	ac0, ac1        	     	
00BE 1B 5E       	srri 	@ar2, ac0.m
00BF 1B 5C       	srri 	@ar2, ac0.l
00C0 19 3E       	lrri 	ac0.m, @ar1
00C1 19 3C       	lrri 	ac0.l, @ar1
00C2 B1 00       	tst  	ac0             	     	
00C3 19 3F       	lrri 	ac1.m, @ar1
00C4 02 94 00 CA 	jne  	$0x00CA
00C6 00 5A       	loop 	ax0.h
00C7 1B 5E       	srri 	@ar2, ac0.m
00C8 02 9F 00 D2 	j    	$0x00D2
00CA 99 00       	asr16	ac1             	     	
00CB 1B 5E       	srri 	@ar2, ac0.m
00CC 1B 5C       	srri 	@ar2, ac0.l
00CD 00 7B 00 D1 	bloop	ax1.h, $0x00D1
00CF 4C 00       	add  	ac0, ac1        	     	
00D0 1B 5E       	srri 	@ar2, ac0.m
00D1 1B 5C       	srri 	@ar2, ac0.l
00D2 00 82 04 00 	lri  	ar2, #0x0400
00D4 19 3E       	lrri 	ac0.m, @ar1
00D5 19 3C       	lrri 	ac0.l, @ar1
00D6 B1 79       	tst  	ac0             	l    	ac1.m, @ar1
00D7 02 94 00 DD 	jne  	$0x00DD
00D9 00 5A       	loop 	ax0.h
00DA 1B 5E       	srri 	@ar2, ac0.m
00DB 02 9F 00 E5 	j    	$0x00E5
00DD 99 00       	asr16	ac1             	     	
00DE 1B 5E       	srri 	@ar2, ac0.m
00DF 1B 5C       	srri 	@ar2, ac0.l
00E0 00 7B 00 E4 	bloop	ax1.h, $0x00E4
00E2 4C 00       	add  	ac0, ac1        	     	
00E3 1B 5E       	srri 	@ar2, ac0.m
00E4 1B 5C       	srri 	@ar2, ac0.l
00E5 19 3E       	lrri 	ac0.m, @ar1
00E6 19 3C       	lrri 	ac0.l, @ar1
00E7 B1 79       	tst  	ac0             	l    	ac1.m, @ar1
00E8 02 94 00 EE 	jne  	$0x00EE
00EA 00 5A       	loop 	ax0.h
00EB 1B 5E       	srri 	@ar2, ac0.m
00EC 02 9F 00 F6 	j    	$0x00F6
00EE 99 00       	asr16	ac1             	     	
00EF 1B 5E       	srri 	@ar2, ac0.m
00F0 1B 5C       	srri 	@ar2, ac0.l
00F1 00 7B 00 F5 	bloop	ax1.h, $0x00F5
00F3 4C 00       	add  	ac0, ac1        	     	
00F4 1B 5E       	srri 	@ar2, ac0.m
00F5 1B 5C       	srri 	@ar2, ac0.l
00F6 19 3E       	lrri 	ac0.m, @ar1
00F7 19 3C       	lrri 	ac0.l, @ar1
00F8 B1 79       	tst  	ac0             	l    	ac1.m, @ar1
00F9 02 94 00 FF 	jne  	$0x00FF
00FB 00 5A       	loop 	ax0.h
00FC 1B 5E       	srri 	@ar2, ac0.m
00FD 02 9F 01 07 	j    	$0x0107
00FF 99 00       	asr16	ac1             	     	
0100 1B 5E       	srri 	@ar2, ac0.m
0101 1B 5C       	srri 	@ar2, ac0.l
0102 00 7B 01 06 	bloop	ax1.h, $0x0106
0104 4C 00       	add  	ac0, ac1        	     	
0105 1B 5E       	srri 	@ar2, ac0.m
0106 1B 5C       	srri 	@ar2, ac0.l
0107 00 82 07 C0 	lri  	ar2, #0x07C0
0109 19 3E       	lrri 	ac0.m, @ar1
010A 19 3C       	lrri 	ac0.l, @ar1
010B B1 79       	tst  	ac0             	l    	ac1.m, @ar1
010C 02 94 01 12 	jne  	$0x0112
010E 00 5A       	loop 	ax0.h
010F 1B 5E       	srri 	@ar2, ac0.m
0110 02 9F 01 1A 	j    	$0x011A
0112 99 00       	asr16	ac1             	     	
0113 1B 5E       	srri 	@ar2, ac0.m
0114 1B 5C       	srri 	@ar2, ac0.l
0115 00 7B 01 19 	bloop	ax1.h, $0x0119
0117 4C 00       	add  	ac0, ac1        	     	
0118 1B 5E       	srri 	@ar2, ac0.m
0119 1B 5C       	srri 	@ar2, ac0.l
011A 19 3E       	lrri 	ac0.m, @ar1
011B 19 3C       	lrri 	ac0.l, @ar1
011C B1 79       	tst  	ac0             	l    	ac1.m, @ar1
011D 02 94 01 23 	jne  	$0x0123
011F 00 5A       	loop 	ax0.h
0120 1B 5E       	srri 	@ar2, ac0.m
0121 02 9F 01 2B 	j    	$0x012B
0123 99 00       	asr16	ac1             	     	
0124 1B 5E       	srri 	@ar2, ac0.m
0125 1B 5C       	srri 	@ar2, ac0.l
0126 00 7B 01 2A 	bloop	ax1.h, $0x012A
0128 4C 00       	add  	ac0, ac1        	     	
0129 1B 5E       	srri 	@ar2, ac0.m
012A 1B 5C       	srri 	@ar2, ac0.l
012B 19 3E       	lrri 	ac0.m, @ar1
012C 19 3C       	lrri 	ac0.l, @ar1
012D B1 79       	tst  	ac0             	l    	ac1.m, @ar1
012E 02 94 01 34 	jne  	$0x0134
0130 00 5A       	loop 	ax0.h
0131 1B 5E       	srri 	@ar2, ac0.m
0132 02 9F 01 3C 	j    	$0x013C
0134 99 00       	asr16	ac1             	     	
0135 1B 5E       	srri 	@ar2, ac0.m
0136 1B 5C       	srri 	@ar2, ac0.l
0137 00 7B 01 3B 	bloop	ax1.h, $0x013B
0139 4C 00       	add  	ac0, ac1        	     	
013A 1B 5E       	srri 	@ar2, ac0.m
013B 1B 5C       	srri 	@ar2, ac0.l
013C 02 9F 00 68 	j    	$0x0068
```

## Command 1

```
013E 00 85 FF FF 	lri  	ix1, #0xFFFF
0140 81 50       	clr  	ac0             	l    	ax0.h, @ar0
0141 89 40       	clr  	ac1             	l    	ax0.l, @ar0
0142 8E 48       	clr40	                	l    	ax1.l, @ar0
0143 00 FA 0E 17 	sr   	$0x0E17, ax0.h
0145 00 F8 0E 18 	sr   	$0x0E18, ax0.l
0147 00 81 00 00 	lri  	ar1, #0x0000
0149 02 BF 04 F1 	call 	$0x04F1
014B 00 DA 0E 17 	lr   	ax0.h, $0x0E17
014D 00 D8 0E 18 	lr   	ax0.l, $0x0E18
014F 89 48       	clr  	ac1             	l    	ax1.l, @ar0
0150 00 81 04 00 	lri  	ar1, #0x0400
0152 02 BF 04 F1 	call 	$0x04F1
0154 00 DA 0E 17 	lr   	ax0.h, $0x0E17
0156 00 D8 0E 18 	lr   	ax0.l, $0x0E18
0158 89 48       	clr  	ac1             	l    	ax1.l, @ar0
0159 00 81 07 C0 	lri  	ar1, #0x07C0
015B 02 BF 04 F1 	call 	$0x04F1
015D 02 9F 00 68 	j    	$0x0068
```

## Command 9

```
015F 00 86 07 C0 	lri  	ix2, #0x07C0
0161 02 BF 04 84 	call 	$0x0484
0163 02 9F 00 68 	j    	$0x0068
```

## Command 6

```
0165 81 00       	clr  	ac0             	     	
0166 8E 00       	clr40	                	     	
0167 19 1E       	lrri 	ac0.m, @ar0
0168 19 1C       	lrri 	ac0.l, @ar0
0169 2E CE       	srs  	$(DSMAH), ac0.m
016A 2C CF       	srs  	$(DSMAL), ac0.l
016B 16 CD 00 00 	si   	$(DSPA), #0x0000
016D 16 C9 00 01 	si   	$(DSCR), #0x0001
016F 16 CB 07 80 	si   	$(DSBL), #0x0780
0171 02 BF 05 5C 	call 	$0x055C
0173 02 9F 00 68 	j    	$0x0068
```

## Command 0x11

```
0175 81 00       	clr  	ac0             	     	
0176 89 70       	clr  	ac1             	l    	ac0.m, @ar0
0177 8E 60       	clr40	                	l    	ac0.l, @ar0
0178 2E CE       	srs  	$(DSMAH), ac0.m
0179 2C CF       	srs  	$(DSMAL), ac0.l
017A 16 CD 0E 44 	si   	$(DSPA), #0x0E44
017C 16 C9 00 00 	si   	$(DSCR), #0x0000
017E 89 00       	clr  	ac1             	     	
017F 0D 20       	lris 	ac1.l, 32
0180 2D CB       	srs  	$(DSBL), ac1.l
0181 4C 00       	add  	ac0, ac1        	     	
0182 1C 80       	mrr  	ix0, ar0
0183 00 80 02 80 	lri  	ar0, #0x0280
0185 00 81 00 00 	lri  	ar1, #0x0000
0187 00 82 01 40 	lri  	ar2, #0x0140
0189 00 83 0E 44 	lri  	ar3, #0x0E44
018B 0A 00       	lris 	ax0.h, 0
018C 27 C9       	lrs  	ac1.m, $(DSCR)
018D 03 A0 00 04 	tclr 	ac1.m, #0x0004
018F 02 9C 01 8C 	jnok 	$0x018C
0191 2E CE       	srs  	$(DSMAH), ac0.m
0192 2C CF       	srs  	$(DSMAL), ac0.l
0193 16 CD 0E 54 	si   	$(DSPA), #0x0E54
0195 16 C9 00 00 	si   	$(DSCR), #0x0000
0197 16 CB 02 60 	si   	$(DSBL), #0x0260
0199 00 9F 00 A0 	lri  	ac1.m, #0x00A0
019B 8F 00       	set40	                	     	
019C 00 7F 01 A5 	bloop	ac1.m, $0x01A5
	019E 19 7E       	lrri 	ac0.m, @ar3
	019F 1B 1A       	srri 	@ar0, ax0.h
	01A0 19 7C       	lrri 	ac0.l, @ar3
	01A1 1B 1A       	srri 	@ar0, ax0.h
	01A2 1B 5E       	srri 	@ar2, ac0.m
	01A3 7C 22       	neg  	ac0             	s    	@ar2, ac0.l
	01A4 1B 3E       	srri 	@ar1, ac0.m
	01A5 1B 3C       	srri 	@ar1, ac0.l
01A6 1C 04       	mrr  	ar0, ix0
01A7 02 9F 00 68 	j    	$0x0068
```

## Command 0xD

```
01A9 8E 70       	clr40	                	l    	ac0.m, @ar0
01AA 89 60       	clr  	ac1             	l    	ac0.l, @ar0
01AB 19 1F       	lrri 	ac1.m, @ar0
01AC 2E CE       	srs  	$(DSMAH), ac0.m
01AD 2C CF       	srs  	$(DSMAL), ac0.l
01AE 16 CD 0C 00 	si   	$(DSPA), #0x0C00
01B0 16 C9 00 00 	si   	$(DSCR), #0x0000
01B2 05 03       	addis	ac1.m, 3
01B3 03 40 FF F0 	andi 	ac1.m, #0xFFF0
01B5 2F CB       	srs  	$(DSBL), ac1.m
01B6 02 BF 05 5C 	call 	$0x055C
01B8 00 80 0C 00 	lri  	ar0, #0x0C00
01BA 02 9F 00 68 	j    	$0x0068
```

## Command 2

```
01BC 81 00       	clr  	ac0             	     	
01BD 89 70       	clr  	ac1             	l    	ac0.m, @ar0
01BE 8E 78       	clr40	                	l    	ac1.m, @ar0
01BF 2E CE       	srs  	$(DSMAH), ac0.m
01C0 2F CF       	srs  	$(DSMAL), ac1.m
01C1 16 CD 0B 80 	si   	$(DSPA), #0x0B80
01C3 16 C9 00 00 	si   	$(DSCR), #0x0000
01C5 16 CB 00 C0 	si   	$(DSBL), #0x00C0
01C7 00 82 0E 08 	lri  	ar2, #0x0E08
01C9 00 9F 00 00 	lri  	ac1.m, #0x0000
01CB 1B 5F       	srri 	@ar2, ac1.m
01CC 00 9F 01 40 	lri  	ac1.m, #0x0140
01CE 1B 5F       	srri 	@ar2, ac1.m
01CF 00 9F 02 80 	lri  	ac1.m, #0x0280
01D1 1B 5F       	srri 	@ar2, ac1.m
01D2 00 9F 04 00 	lri  	ac1.m, #0x0400
01D4 1B 5F       	srri 	@ar2, ac1.m
01D5 00 9F 05 40 	lri  	ac1.m, #0x0540
01D7 1B 5F       	srri 	@ar2, ac1.m
01D8 00 9F 06 80 	lri  	ac1.m, #0x0680
01DA 1B 5F       	srri 	@ar2, ac1.m
01DB 00 9F 07 C0 	lri  	ac1.m, #0x07C0
01DD 1B 5F       	srri 	@ar2, ac1.m
01DE 00 9F 09 00 	lri  	ac1.m, #0x0900
01E0 1B 5F       	srri 	@ar2, ac1.m
01E1 00 9F 0A 40 	lri  	ac1.m, #0x0A40
01E3 1B 5F       	srri 	@ar2, ac1.m
01E4 02 BF 05 5C 	call 	$0x055C
01E6 00 DE 0B A7 	lr   	ac0.m, $0x0BA7
01E8 00 DF 0B A8 	lr   	ac1.m, $0x0BA8
01EA 2E CE       	srs  	$(DSMAH), ac0.m
01EB 2F CF       	srs  	$(DSMAL), ac1.m
01EC 16 CD 03 C0 	si   	$(DSPA), #0x03C0
01EE 16 C9 00 00 	si   	$(DSCR), #0x0000
01F0 16 CB 00 80 	si   	$(DSBL), #0x0080
01F2 81 00       	clr  	ac0             	     	
01F3 89 00       	clr  	ac1             	     	
01F4 00 DE 0B 84 	lr   	ac0.m, $0x0B84
01F6 00 9F 0B 31 	lri  	ac1.m, #0x0B31
01F8 4C 00       	add  	ac0, ac1        	     	
01F9 1C 7E       	mrr  	ar3, ac0.m
01FA 02 13       	ilrr 	ac0.m, @ar3
01FB 00 FE 0E 15 	sr   	$0x0E15, ac0.m
01FD 00 DE 0B 85 	lr   	ac0.m, $0x0B85
01FF 00 9F 0B 34 	lri  	ac1.m, #0x0B34
0201 4C 00       	add  	ac0, ac1        	     	
0202 1C 7E       	mrr  	ar3, ac0.m
0203 02 13       	ilrr 	ac0.m, @ar3
0204 00 FE 0E 16 	sr   	$0x0E16, ac0.m
0206 00 DE 0B 86 	lr   	ac0.m, $0x0B86
0208 00 9F 0B 11 	lri  	ac1.m, #0x0B11
020A 4C 00       	add  	ac0, ac1        	     	
020B 1C 7E       	mrr  	ar3, ac0.m
020C 02 13       	ilrr 	ac0.m, @ar3
020D 00 FE 0E 14 	sr   	$0x0E14, ac0.m
020F 81 00       	clr  	ac0             	     	
0210 00 DE 0B 9B 	lr   	ac0.m, $0x0B9B
0212 B1 00       	tst  	ac0             	     	
0213 02 95 02 3A 	jeq  	$0x023A
0215 89 00       	clr  	ac1             	     	
0216 00 DF 0B 9E 	lr   	ac1.m, $0x0B9E
0218 03 00 0C C0 	addi 	ac1.m, #0x0CC0
021A 00 FF 0E 40 	sr   	$0x0E40, ac1.m
021C 00 DF 0B 9F 	lr   	ac1.m, $0x0B9F
021E 03 00 0C C0 	addi 	ac1.m, #0x0CC0
0220 00 FF 0E 41 	sr   	$0x0E41, ac1.m
0222 00 9F 0C E0 	lri  	ac1.m, #0x0CE0
0224 00 FF 0E 42 	sr   	$0x0E42, ac1.m
0226 00 FF 0E 43 	sr   	$0x0E43, ac1.m
0228 02 BF 05 5C 	call 	$0x055C
022A 00 DE 0B 9C 	lr   	ac0.m, $0x0B9C
022C 2E CE       	srs  	$(DSMAH), ac0.m
022D 00 DE 0B 9D 	lr   	ac0.m, $0x0B9D
022F 2E CF       	srs  	$(DSMAL), ac0.m
0230 16 CD 0C C0 	si   	$(DSPA), #0x0CC0
0232 16 C9 00 00 	si   	$(DSCR), #0x0000
0234 16 CB 00 40 	si   	$(DSBL), #0x0040
0236 02 BF 05 5C 	call 	$0x055C
0238 02 9F 00 68 	j    	$0x0068
023A 00 9F 0C E0 	lri  	ac1.m, #0x0CE0
023C 00 FF 0E 42 	sr   	$0x0E42, ac1.m
023E 00 FF 0E 40 	sr   	$0x0E40, ac1.m
0240 00 FF 0E 41 	sr   	$0x0E41, ac1.m
0242 00 FF 0E 43 	sr   	$0x0E43, ac1.m
0244 02 BF 05 5C 	call 	$0x055C
0246 02 9F 00 68 	j    	$0x0068
```

## Command 3

```
0248 8E 00       	clr40	                	     	
0249 00 E0 0E 07 	sr   	$0x0E07, ar0
024B 00 80 0B A2 	lri  	ar0, #0x0BA2
024D 00 81 03 C0 	lri  	ar1, #0x03C0
024F 0E 05       	lris 	ac0.m, 5
0250 00 FE 0E 04 	sr   	$0x0E04, ac0.m
0252 89 00       	clr  	ac1             	     	
0253 81 50       	clr  	ac0             	l    	ax0.h, @ar0
0254 00 9F 0B 80 	lri  	ac1.m, #0x0B80
0256 00 7A 02 5B 	bloop	ax0.h, $0x025B
0258 19 3E       	lrri 	ac0.m, @ar1
0259 4C 49       	add  	ac0, ac1        	l    	ax1.l, @ar1
025A 1C 5E       	mrr  	ar2, ac0.m
025B 1A 59       	srr  	@ar2, ax1.l
025C 00 83 0E 05 	lri  	ar3, #0x0E05
025E 1B 61       	srri 	@ar3, ar1
025F 1B 60       	srri 	@ar3, ar0
0260 00 DE 0B 87 	lr   	ac0.m, $0x0B87
0262 06 01       	cmpis	ac0.m, 1
0263 02 95 02 67 	jeq  	$0x0267
0265 02 9F 03 32 	j    	$0x0332
0267 00 DE 0E 42 	lr   	ac0.m, $0x0E42
0269 00 FE 0E 1C 	sr   	$0x0E1C, ac0.m
026B 00 C3 0E 15 	lr   	ar3, $0x0E15
026D 17 7F       	callr	ar3
026E 8E 00       	clr40	                	     	
026F 8A 00       	m2   	                	     	
0270 81 00       	clr  	ac0             	     	
0271 89 00       	clr  	ac1             	     	
0272 00 DE 0B B3 	lr   	ac0.m, $0x0BB3 								// AXPBVE.currentDelta  - signed per sample delta
0274 00 DF 0B B2 	lr   	ac1.m, $0x0BB2 								// AXPBVE.currentVolume - .15 volume at start of frame
0276 1F 1F       	mrr  	ax0.l, ac1.m
0277 4D 00       	add  	ac1, ac0        	     	
0278 14 81       	asl  	ac0, #0x01
0279 8D 1E       	set15	                	mv   	ax1.h, ac0.m
027A 1F D8       	mrr  	ac0.m, ax0.l
027B 00 98 80 00 	lri  	ax0.l, #0x8000
027D 00 80 0E 44 	lri  	ar0, #0x0E44
027F A8 30       	mulx 	ax0.l, ax1.h    	s    	@ar0, ac0.m
0280 AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
0281 AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
0282 AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
0283 AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
0284 AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
0285 AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
0286 AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
0287 AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
0288 AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
0289 AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
028A AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
028B AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
028C AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
028D AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
028E AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
028F AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
0290 AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
0291 AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
0292 AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
0293 AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
0294 AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
0295 AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
0296 AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
0297 AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
0298 AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
0299 AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
029A AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
029B AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
029C AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
029D AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
029E AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
029F 00 FE 0B B2 	sr   	$0x0BB2, ac0.m
02A1 8F 00       	set40	                	     	
02A2 00 80 0E 44 	lri  	ar0, #0x0E44
02A4 00 C1 0E 43 	lr   	ar1, $0x0E43
02A6 1C 61       	mrr  	ar3, ar1
02A7 19 3A       	lrri 	ax0.h, @ar1
02A8 19 18       	lrri 	ax0.l, @ar0
02A9 90 59       	mul  	ax0.l, ax0.h    	l    	ax1.h, @ar1
02AA 19 19       	lrri 	ax1.l, @ar0
02AB 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
02AC 80 80       	nx   	                	ls   	ax0.l, ac0.m
02AD 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
02AE 80 91       	nx   	                	ls   	ax1.l, ac1.m
02AF 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
02B0 80 80       	nx   	                	ls   	ax0.l, ac0.m
02B1 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
02B2 80 91       	nx   	                	ls   	ax1.l, ac1.m
02B3 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
02B4 80 80       	nx   	                	ls   	ax0.l, ac0.m
02B5 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
02B6 80 91       	nx   	                	ls   	ax1.l, ac1.m
02B7 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
02B8 80 80       	nx   	                	ls   	ax0.l, ac0.m
02B9 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
02BA 80 91       	nx   	                	ls   	ax1.l, ac1.m
02BB 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
02BC 80 80       	nx   	                	ls   	ax0.l, ac0.m
02BD 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
02BE 80 91       	nx   	                	ls   	ax1.l, ac1.m
02BF 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
02C0 80 80       	nx   	                	ls   	ax0.l, ac0.m
02C1 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
02C2 80 91       	nx   	                	ls   	ax1.l, ac1.m
02C3 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
02C4 80 80       	nx   	                	ls   	ax0.l, ac0.m
02C5 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
02C6 80 91       	nx   	                	ls   	ax1.l, ac1.m
02C7 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
02C8 80 80       	nx   	                	ls   	ax0.l, ac0.m
02C9 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
02CA 80 91       	nx   	                	ls   	ax1.l, ac1.m
02CB 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
02CC 80 80       	nx   	                	ls   	ax0.l, ac0.m
02CD 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
02CE 80 91       	nx   	                	ls   	ax1.l, ac1.m
02CF 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
02D0 80 80       	nx   	                	ls   	ax0.l, ac0.m
02D1 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
02D2 80 91       	nx   	                	ls   	ax1.l, ac1.m
02D3 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
02D4 80 80       	nx   	                	ls   	ax0.l, ac0.m
02D5 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
02D6 80 91       	nx   	                	ls   	ax1.l, ac1.m
02D7 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
02D8 80 80       	nx   	                	ls   	ax0.l, ac0.m
02D9 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
02DA 80 91       	nx   	                	ls   	ax1.l, ac1.m
02DB 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
02DC 80 80       	nx   	                	ls   	ax0.l, ac0.m
02DD 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
02DE 80 91       	nx   	                	ls   	ax1.l, ac1.m
02DF 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
02E0 80 80       	nx   	                	ls   	ax0.l, ac0.m
02E1 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
02E2 80 91       	nx   	                	ls   	ax1.l, ac1.m
02E3 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
02E4 80 80       	nx   	                	ls   	ax0.l, ac0.m
02E5 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
02E6 80 91       	nx   	                	ls   	ax1.l, ac1.m
02E7 9E 00       	mulmv	ax1.l, ax1.h, ac0	     	
02E8 6F 33       	movp 	ac1             	s    	@ar3, ac0.m
02E9 1B 7F       	srri 	@ar3, ac1.m
02EA 00 C3 0E 14 	lr   	ar3, $0x0E14
02EC 8F 00       	set40	                	     	
02ED 8D 00       	set15	                	     	
02EE 8A 00       	m2   	                	     	
02EF 17 7F       	callr	ar3
02F0 81 00       	clr  	ac0             	     	
02F1 00 DE 0B 9B 	lr   	ac0.m, $0x0B9B
02F3 B1 00       	tst  	ac0             	     	
02F4 02 95 03 2A 	jeq  	$0x032A
02F6 00 DE 0E 42 	lr   	ac0.m, $0x0E42
02F8 00 FE 0E 43 	sr   	$0x0E43, ac0.m
02FA 81 00       	clr  	ac0             	     	
02FB 89 00       	clr  	ac1             	     	
02FC 00 DE 0B 9E 	lr   	ac0.m, $0x0B9E
02FE 00 DF 0B A0 	lr   	ac1.m, $0x0BA0
0300 82 00       	cmp  	                	     	
0301 02 93 03 06 	jle  	$0x0306
0303 78 00       	decm 	ac0             	     	
0304 02 9F 03 09 	j    	$0x0309
0306 02 95 03 09 	jeq  	$0x0309
0308 74 00       	incm 	ac0             	     	
0309 00 FE 0B 9E 	sr   	$0x0B9E, ac0.m
030B 00 DF 0E 43 	lr   	ac1.m, $0x0E43
030D 05 E0       	addis	ac1.m, -32
030E 4C 00       	add  	ac0, ac1        	     	
030F 00 FE 0E 40 	sr   	$0x0E40, ac0.m
0311 81 00       	clr  	ac0             	     	
0312 89 00       	clr  	ac1             	     	
0313 00 DE 0B 9F 	lr   	ac0.m, $0x0B9F
0315 00 DF 0B A1 	lr   	ac1.m, $0x0BA1
0317 82 00       	cmp  	                	     	
0318 02 93 03 1D 	jle  	$0x031D
031A 78 00       	decm 	ac0             	     	
031B 02 9F 03 20 	j    	$0x0320
031D 02 95 03 20 	jeq  	$0x0320
031F 74 00       	incm 	ac0             	     	
0320 00 FE 0B 9F 	sr   	$0x0B9F, ac0.m
0322 00 DF 0E 43 	lr   	ac1.m, $0x0E43
0324 05 E0       	addis	ac1.m, -32
0325 4C 00       	add  	ac0, ac1        	     	
0326 00 FE 0E 41 	sr   	$0x0E41, ac0.m
0328 02 9F 03 32 	j    	$0x0332
032A 00 DE 0E 42 	lr   	ac0.m, $0x0E42
032C 00 FE 0E 40 	sr   	$0x0E40, ac0.m
032E 00 FE 0E 41 	sr   	$0x0E41, ac0.m
0330 00 FE 0E 43 	sr   	$0x0E43, ac0.m
0332 81 00       	clr  	ac0             	     	
0333 8E 00       	clr40	                	     	
0334 84 00       	clrp 	                	     	
0335 89 00       	clr  	ac1             	     	
0336 1E FE       	mrr  	prod.m2, ac0.m
0337 0E 40       	lris 	ac0.m, 64
0338 1E BE       	mrr  	prod.m1, ac0.m
0339 00 83 0E 08 	lri  	ar3, #0x0E08
033B 1C 03       	mrr  	ar0, ar3
033C 1F F5       	mrr  	ac1.m, prod.m1
033D 19 1A       	lrri 	ax0.h, @ar0
033E F8 58       	addpaxz	ac0, ax0.h    	l    	ax1.h, @ar0
033F FB A0       	addpaxz	ac1, ax1.h    	ls   	ax0.h, ac0.m
0340 F8 B1       	addpaxz	ac0, ax0.h    	ls   	ax1.h, ac1.m
0341 FB A0       	addpaxz	ac1, ax1.h    	ls   	ax0.h, ac0.m
0342 F8 B1       	addpaxz	ac0, ax0.h    	ls   	ax1.h, ac1.m
0343 FB A0       	addpaxz	ac1, ax1.h    	ls   	ax0.h, ac0.m
0344 F8 B1       	addpaxz	ac0, ax0.h    	ls   	ax1.h, ac1.m
0345 FB A0       	addpaxz	ac1, ax1.h    	ls   	ax0.h, ac0.m
0346 F8 3B       	addpaxz	ac0, ax0.h    	s    	@ar3, ac1.m
0347 1B 7E       	srri 	@ar3, ac0.m
0348 00 83 0E 04 	lri  	ar3, #0x0E04
034A 81 00       	clr  	ac0             	     	
034B 89 73       	clr  	ac1             	l    	ac0.m, @ar3
034C 19 61       	lrri 	ar1, @ar3
034D 19 60       	lrri 	ar0, @ar3
034E 78 00       	decm 	ac0             	     	
034F 00 FE 0E 04 	sr   	$0x0E04, ac0.m
0351 02 94 02 53 	jne  	$0x0253
0353 8E 00       	clr40	                	     	
0354 81 00       	clr  	ac0             	     	
0355 00 DE 0B 9B 	lr   	ac0.m, $0x0B9B
0357 B1 00       	tst  	ac0             	     	
0358 02 95 03 6A 	jeq  	$0x036A
035A 00 DE 0B 9C 	lr   	ac0.m, $0x0B9C
035C 00 DC 0B 9D 	lr   	ac0.l, $0x0B9D
035E 2E CE       	srs  	$(DSMAH), ac0.m
035F 2C CF       	srs  	$(DSMAL), ac0.l
0360 81 00       	clr  	ac0             	     	
0361 00 DE 0E 1C 	lr   	ac0.m, $0x0E1C
0363 2E CD       	srs  	$(DSPA), ac0.m
0364 16 C9 00 01 	si   	$(DSCR), #0x0001
0366 16 CB 00 40 	si   	$(DSBL), #0x0040
0368 02 BF 05 5C 	call 	$0x055C
036A 81 00       	clr  	ac0             	     	
036B 89 00       	clr  	ac1             	     	
036C 00 DE 0B 82 	lr   	ac0.m, $0x0B82
036E 00 DF 0B 83 	lr   	ac1.m, $0x0B83
0370 2E CE       	srs  	$(DSMAH), ac0.m
0371 2F CF       	srs  	$(DSMAL), ac1.m
0372 16 CD 0B 80 	si   	$(DSPA), #0x0B80
0374 16 C9 00 01 	si   	$(DSCR), #0x0001
0376 16 CB 00 C0 	si   	$(DSBL), #0x00C0
0378 02 BF 05 5C 	call 	$0x055C
037A 81 00       	clr  	ac0             	     	
037B 00 DE 0B 80 	lr   	ac0.m, $0x0B80
037D 00 DC 0B 81 	lr   	ac0.l, $0x0B81
037F B1 00       	tst  	ac0             	     	
0380 02 94 03 86 	jne  	$0x0386
0382 00 C0 0E 07 	lr   	ar0, $0x0E07
0384 02 9F 00 68 	j    	$0x0068
0386 2E CE       	srs  	$(DSMAH), ac0.m
0387 2C CF       	srs  	$(DSMAL), ac0.l
0388 16 CD 0B 80 	si   	$(DSPA), #0x0B80
038A 16 C9 00 00 	si   	$(DSCR), #0x0000
038C 16 CB 00 C0 	si   	$(DSBL), #0x00C0
038E 00 82 0E 08 	lri  	ar2, #0x0E08
0390 00 9F 00 00 	lri  	ac1.m, #0x0000
0392 1B 5F       	srri 	@ar2, ac1.m
0393 00 9F 01 40 	lri  	ac1.m, #0x0140
0395 1B 5F       	srri 	@ar2, ac1.m
0396 00 9F 02 80 	lri  	ac1.m, #0x0280
0398 1B 5F       	srri 	@ar2, ac1.m
0399 00 9F 04 00 	lri  	ac1.m, #0x0400
039B 1B 5F       	srri 	@ar2, ac1.m
039C 00 9F 05 40 	lri  	ac1.m, #0x0540
039E 1B 5F       	srri 	@ar2, ac1.m
039F 00 9F 06 80 	lri  	ac1.m, #0x0680
03A1 1B 5F       	srri 	@ar2, ac1.m
03A2 00 9F 07 C0 	lri  	ac1.m, #0x07C0
03A4 1B 5F       	srri 	@ar2, ac1.m
03A5 00 9F 09 00 	lri  	ac1.m, #0x0900
03A7 1B 5F       	srri 	@ar2, ac1.m
03A8 00 9F 0A 40 	lri  	ac1.m, #0x0A40
03AA 1B 5F       	srri 	@ar2, ac1.m
03AB 02 BF 05 5C 	call 	$0x055C
03AD 00 DE 0B A7 	lr   	ac0.m, $0x0BA7
03AF 00 DF 0B A8 	lr   	ac1.m, $0x0BA8
03B1 2E CE       	srs  	$(DSMAH), ac0.m
03B2 2F CF       	srs  	$(DSMAL), ac1.m
03B3 16 CD 03 C0 	si   	$(DSPA), #0x03C0
03B5 16 C9 00 00 	si   	$(DSCR), #0x0000
03B7 16 CB 00 80 	si   	$(DSBL), #0x0080
03B9 81 00       	clr  	ac0             	     	
03BA 89 00       	clr  	ac1             	     	
03BB 00 DE 0B 84 	lr   	ac0.m, $0x0B84
03BD 00 9F 0B 31 	lri  	ac1.m, #0x0B31
03BF 4C 00       	add  	ac0, ac1        	     	
03C0 1C 7E       	mrr  	ar3, ac0.m
03C1 02 13       	ilrr 	ac0.m, @ar3
03C2 00 FE 0E 15 	sr   	$0x0E15, ac0.m
03C4 00 DE 0B 85 	lr   	ac0.m, $0x0B85
03C6 00 9F 0B 34 	lri  	ac1.m, #0x0B34
03C8 4C 00       	add  	ac0, ac1        	     	
03C9 1C 7E       	mrr  	ar3, ac0.m
03CA 02 13       	ilrr 	ac0.m, @ar3
03CB 00 FE 0E 16 	sr   	$0x0E16, ac0.m
03CD 00 DE 0B 86 	lr   	ac0.m, $0x0B86
03CF 00 9F 0B 11 	lri  	ac1.m, #0x0B11
03D1 4C 00       	add  	ac0, ac1        	     	
03D2 1C 7E       	mrr  	ar3, ac0.m
03D3 02 13       	ilrr 	ac0.m, @ar3
03D4 00 FE 0E 14 	sr   	$0x0E14, ac0.m
03D6 81 00       	clr  	ac0             	     	
03D7 00 DE 0B 9B 	lr   	ac0.m, $0x0B9B
03D9 B1 00       	tst  	ac0             	     	
03DA 02 95 04 03 	jeq  	$0x0403
03DC 89 00       	clr  	ac1             	     	
03DD 00 DF 0B 9E 	lr   	ac1.m, $0x0B9E
03DF 03 00 0C C0 	addi 	ac1.m, #0x0CC0
03E1 00 FF 0E 40 	sr   	$0x0E40, ac1.m
03E3 00 DF 0B 9F 	lr   	ac1.m, $0x0B9F
03E5 03 00 0C C0 	addi 	ac1.m, #0x0CC0
03E7 00 FF 0E 41 	sr   	$0x0E41, ac1.m
03E9 00 9F 0C E0 	lri  	ac1.m, #0x0CE0
03EB 00 FF 0E 42 	sr   	$0x0E42, ac1.m
03ED 00 FF 0E 43 	sr   	$0x0E43, ac1.m
03EF 02 BF 05 5C 	call 	$0x055C
03F1 00 DE 0B 9C 	lr   	ac0.m, $0x0B9C
03F3 2E CE       	srs  	$(DSMAH), ac0.m
03F4 00 DE 0B 9D 	lr   	ac0.m, $0x0B9D
03F6 2E CF       	srs  	$(DSMAL), ac0.m
03F7 16 CD 0C C0 	si   	$(DSPA), #0x0CC0
03F9 16 C9 00 00 	si   	$(DSCR), #0x0000
03FB 16 CB 00 40 	si   	$(DSBL), #0x0040
03FD 02 BF 05 5C 	call 	$0x055C
03FF 00 C0 0E 07 	lr   	ar0, $0x0E07
0401 02 9F 02 48 	j    	$0x0248
0403 00 9F 0C E0 	lri  	ac1.m, #0x0CE0
0405 00 FF 0E 42 	sr   	$0x0E42, ac1.m
0407 00 FF 0E 40 	sr   	$0x0E40, ac1.m
0409 00 FF 0E 41 	sr   	$0x0E41, ac1.m
040B 00 FF 0E 43 	sr   	$0x0E43, ac1.m
040D 02 BF 05 5C 	call 	$0x055C
040F 00 C0 0E 07 	lr   	ar0, $0x0E07
0411 02 9F 02 48 	j    	$0x0248
```

## Command 4

```
0413 8E 00       	clr40	                	     	
0414 00 86 04 00 	lri  	ix2, #0x0400
0416 81 00       	clr  	ac0             	     	
0417 89 70       	clr  	ac1             	l    	ac0.m, @ar0
0418 19 1C       	lrri 	ac0.l, @ar0
0419 2E CE       	srs  	$(DSMAH), ac0.m
041A 2C CF       	srs  	$(DSMAL), ac0.l
041B 1F C6       	mrr  	ac0.m, ix2
041C 2E CD       	srs  	$(DSPA), ac0.m
041D 16 C9 00 01 	si   	$(DSCR), #0x0001
041F 16 CB 07 80 	si   	$(DSBL), #0x0780
0421 02 BF 05 5C 	call 	$0x055C
0423 02 BF 04 84 	call 	$0x0484
0425 02 9F 00 68 	j    	$0x0068
```

## Command 5

```
0427 8E 00       	clr40	                	     	
0428 00 86 07 C0 	lri  	ix2, #0x07C0
042A 81 00       	clr  	ac0             	     	
042B 89 70       	clr  	ac1             	l    	ac0.m, @ar0
042C 19 1C       	lrri 	ac0.l, @ar0
042D 2E CE       	srs  	$(DSMAH), ac0.m
042E 2C CF       	srs  	$(DSMAL), ac0.l
042F 1F C6       	mrr  	ac0.m, ix2
0430 2E CD       	srs  	$(DSPA), ac0.m
0431 16 C9 00 01 	si   	$(DSCR), #0x0001
0433 16 CB 07 80 	si   	$(DSBL), #0x0780
0435 02 BF 05 5C 	call 	$0x055C
0437 02 BF 04 84 	call 	$0x0484
0439 02 9F 00 68 	j    	$0x0068
```

## Command 0xE

Interleave L/R. Source: #0x0 (Left, 160 * 4 byte per sample), #0x140 (Right, 160 * 4 byte per sample). Dest: #0x400 (320 16-bit L/R sample pairs)

```
043B 8C 00       	clr15	                	     	
043C 8A 00       	m2   	                	     	
043D 81 00       	clr  	ac0             	     	
043E 89 70       	clr  	ac1             	l    	ac0.m, @ar0
043F 19 1F       	lrri 	ac1.m, @ar0
0440 2E CE       	srs  	$(DSMAH), ac0.m
0441 2F CF       	srs  	$(DSMAL), ac1.m
0442 16 CD 02 80 	si   	$(DSPA), #0x0280
0444 16 C9 00 01 	si   	$(DSCR), #0x0001
0446 16 CB 02 80 	si   	$(DSBL), #0x0280
0448 8F 50       	set40	                	l    	ax0.h, @ar0
0449 81 40       	clr  	ac0             	l    	ax0.l, @ar0
044A 00 81 04 00 	lri  	ar1, #0x0400
044C 00 83 00 00 	lri  	ar3, #0x0000
044E 00 82 01 40 	lri  	ar2, #0x0140
0450 00 99 00 80 	lri  	ax1.l, #0x0080
0452 02 BF 05 5C 	call 	$0x055C
0454 11 05 04 6C 	bloopi	#0x05, $0x046C
	0456 1F 61       	mrr  	ax1.h, ar1
	0457 11 20 04 5E 	bloopi	#0x20, $0x045E
		0459 89 72       	clr  	ac1             	l    	ac0.m, @ar2
		045A 19 5C       	lrri 	ac0.l, @ar2
		045B F0 7B       	lsl16	ac0             	l    	ac1.m, @ar3
		045C 19 7D       	lrri 	ac1.l, @ar3
		045D F1 31       	lsl16	ac1             	s    	@ar1, ac0.m
		045E 81 39       	clr  	ac0             	s    	@ar1, ac1.m
	045F 89 00       	clr  	ac1             	     	
	0460 68 00       	movax	ac0, ax0        	     	
	0461 2E CE       	srs  	$(DSMAH), ac0.m
	0462 2C CF       	srs  	$(DSMAL), ac0.l
	0463 1F FB       	mrr  	ac1.m, ax1.h
	0464 2F CD       	srs  	$(DSPA), ac1.m
	0465 0F 01       	lris 	ac1.m, 1
	0466 2F C9       	srs  	$(DSCR), ac1.m
	0467 1F F9       	mrr  	ac1.m, ax1.l
	0468 2F CB       	srs  	$(DSBL), ac1.m
	0469 72 00       	addaxl	ac0, ax1.l     	     	
	046A 1F 5E       	mrr  	ax0.h, ac0.m
	046B 1F 1C       	mrr  	ax0.l, ac0.l
	046C 81 00       	clr  	ac0             	     	
046D 26 C9       	lrs  	ac0.m, $(DSCR)
046E 02 A0 00 04 	tclr 	ac0.m, #0x0004
0470 02 9C 04 6D 	jnok 	$0x046D
0472 02 9F 00 68 	j    	$0x0068
```

## Command 0xB

```
0474 02 9F 00 68 	j    	$0x0068
```

## Command 0xC

```
0476 02 9F 00 68 	j    	$0x0068
```

## Command 0xA

```
0478 02 9F 00 68 	j    	$0x0068
```

## Command 0xF

```
047A 16 FC DC D1 	si   	$(DMBH), #0xDCD1
047C 16 FD 00 02 	si   	$(DMBL), #0x0002
047E 16 FB 00 01 	si   	$(DIRQ), #0x0001
0480 02 9F 0C 91 	j    	$0x0C91
0482 02 9F 00 45 	j    	$0x0045
```

```
0484 8E 00       	clr40	                	     	
0485 19 1F       	lrri 	ac1.m, @ar0
0486 19 1D       	lrri 	ac1.l, @ar0
0487 1F 5F       	mrr  	ax0.h, ac1.m
0488 1F 1D       	mrr  	ax0.l, ac1.l
0489 2F CE       	srs  	$(DSMAH), ac1.m
048A 2D CF       	srs  	$(DSMAL), ac1.l
048B 89 00       	clr  	ac1             	     	
048C 1F A6       	mrr  	ac1.l, ix2
048D 2D CD       	srs  	$(DSPA), ac1.l
048E 0E 00       	lris 	ac0.m, 0
048F 2E C9       	srs  	$(DSCR), ac0.m
0490 81 00       	clr  	ac0             	     	
0491 00 9C 00 C0 	lri  	ac0.l, #0x00C0
0493 2C CB       	srs  	$(DSBL), ac0.l
0494 1C A0       	mrr  	ix1, ar0
0495 00 81 0E 44 	lri  	ar1, #0x0E44
0497 48 00       	addax	ac0, ax0        	     	
0498 1B 3E       	srri 	@ar1, ac0.m
0499 1B 3C       	srri 	@ar1, ac0.l
049A 0B 00       	lris 	ax1.h, 0
049B 00 99 00 60 	lri  	ax1.l, #0x0060
049D 4B 00       	addax	ac1, ax1        	     	
049E 1B 3D       	srri 	@ar1, ac1.l
049F 00 81 0E 44 	lri  	ar1, #0x0E44
04A1 1C 06       	mrr  	ar0, ix2
04A2 00 83 00 00 	lri  	ar3, #0x0000
04A4 1C 43       	mrr  	ar2, ar3
04A5 27 C9       	lrs  	ac1.m, $(DSCR)
04A6 03 A0 00 04 	tclr 	ac1.m, #0x0004
04A8 02 9C 04 A5 	jnok 	$0x04A5
04AA 11 09 04 DA 	bloopi	#0x09, $0x04DA
	04AC 8E 00       	clr40	                	     	
	04AD 19 3A       	lrri 	ax0.h, @ar1
	04AE 19 38       	lrri 	ax0.l, @ar1
	04AF 69 00       	movax	ac1, ax0        	     	
	04B0 2F CE       	srs  	$(DSMAH), ac1.m
	04B1 2D CF       	srs  	$(DSMAL), ac1.l
	04B2 89 00       	clr  	ac1             	     	
	04B3 19 3D       	lrri 	ac1.l, @ar1
	04B4 2D CD       	srs  	$(DSPA), ac1.l
	04B5 16 C9 00 00 	si   	$(DSCR), #0x0000
	04B7 81 00       	clr  	ac0             	     	
	04B8 00 9C 00 C0 	lri  	ac0.l, #0x00C0
	04BA 2C CB       	srs  	$(DSBL), ac0.l
	04BB 00 81 0E 44 	lri  	ar1, #0x0E44
	04BD 48 00       	addax	ac0, ax0        	     	
	04BE 1B 3E       	srri 	@ar1, ac0.m
	04BF 1B 3C       	srri 	@ar1, ac0.l
	04C0 0B 00       	lris 	ax1.h, 0
	04C1 09 60       	lris 	ax1.l, 96
	04C2 4B 00       	addax	ac1, ax1        	     	
	04C3 1B 3D       	srri 	@ar1, ac1.l
	04C4 00 81 0E 44 	lri  	ar1, #0x0E44
	04C6 8F 00       	set40	                	     	
	04C7 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
	04C8 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
	04C9 6A 00       	movax	ac0, ax1        	     	
	04CA 48 00       	addax	ac0, ax0        	     	
	04CB 11 17 04 D4 	bloopi	#0x17, $0x04D4
		04CD 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
		04CE 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
		04CF 6B 32       	movax	ac1, ax1        	s    	@ar2, ac0.m
		04D0 49 22       	addax	ac1, ax0        	s    	@ar2, ac0.l
		04D1 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
		04D2 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
		04D3 6A 3A       	movax	ac0, ax1        	s    	@ar2, ac1.m
		04D4 48 2A       	addax	ac0, ax0        	s    	@ar2, ac1.l
	04D5 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
	04D6 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
	04D7 6B 32       	movax	ac1, ax1        	s    	@ar2, ac0.m
	04D8 49 22       	addax	ac1, ax0        	s    	@ar2, ac0.l
	04D9 1B 5F       	srri 	@ar2, ac1.m
	04DA 1B 5D       	srri 	@ar2, ac1.l
04DB 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
04DC 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
04DD 6A 00       	movax	ac0, ax1        	     	
04DE 48 00       	addax	ac0, ax0        	     	
04DF 11 17 04 E8 	bloopi	#0x17, $0x04E8
	04E1 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
	04E2 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
	04E3 6B 32       	movax	ac1, ax1        	s    	@ar2, ac0.m
	04E4 49 22       	addax	ac1, ax0        	s    	@ar2, ac0.l
	04E5 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
	04E6 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
	04E7 6A 3A       	movax	ac0, ax1        	s    	@ar2, ac1.m
	04E8 48 2A       	addax	ac0, ax0        	s    	@ar2, ac1.l
04E9 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
04EA 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
04EB 6B 32       	movax	ac1, ax1        	s    	@ar2, ac0.m
04EC 49 22       	addax	ac1, ax0        	s    	@ar2, ac0.l
04ED 1B 5F       	srri 	@ar2, ac1.m
04EE 1B 5D       	srri 	@ar2, ac1.l
04EF 1C 05       	mrr  	ar0, ix1
04F0 02 DF       	ret  	
```

```
04F1 8E 00       	clr40	                	     	
04F2 00 9B 0E 44 	lri  	ax1.h, #0x0E44
04F4 00 9D 00 C0 	lri  	ac1.l, #0x00C0
04F6 02 BF 05 41 	call 	$0x0541
04F8 49 00       	addax	ac1, ax0        	     	
04F9 00 FF 0E 1D 	sr   	$0x0E1D, ac1.m
04FB 00 FD 0E 1E 	sr   	$0x0E1E, ac1.l
04FD 89 00       	clr  	ac1             	     	
04FE 02 BF 05 5C 	call 	$0x055C
0500 11 04 05 2C 	bloopi	#0x04, $0x052C
	0502 00 DA 0E 1D 	lr   	ax0.h, $0x0E1D
	0504 00 D8 0E 1E 	lr   	ax0.l, $0x0E1E
	0506 00 9B 0E A4 	lri  	ax1.h, #0x0EA4
	0508 00 9D 00 C0 	lri  	ac1.l, #0x00C0
	050A 02 BF 05 41 	call 	$0x0541
	050C 49 00       	addax	ac1, ax0        	     	
	050D 00 FF 0E 1D 	sr   	$0x0E1D, ac1.m
	050F 00 FD 0E 1E 	sr   	$0x0E1E, ac1.l
	0511 00 83 0E 44 	lri  	ar3, #0x0E44
	0513 02 BF 05 4C 	call 	$0x054C
	0515 89 00       	clr  	ac1             	     	
	0516 00 DA 0E 1D 	lr   	ax0.h, $0x0E1D
	0518 00 D8 0E 1E 	lr   	ax0.l, $0x0E1E
	051A 00 9B 0E 44 	lri  	ax1.h, #0x0E44
	051C 00 9D 00 C0 	lri  	ac1.l, #0x00C0
	051E 02 BF 05 41 	call 	$0x0541
	0520 49 00       	addax	ac1, ax0        	     	
	0521 00 FF 0E 1D 	sr   	$0x0E1D, ac1.m
	0523 00 FD 0E 1E 	sr   	$0x0E1E, ac1.l
	0525 00 83 0E A4 	lri  	ar3, #0x0EA4
	0527 02 BF 05 4C 	call 	$0x054C
	0529 00 00       	nop  	
	052A 00 00       	nop  	
	052B 8E 00       	clr40	                	     	
	052C 89 00       	clr  	ac1             	     	
052D 00 DA 0E 1D 	lr   	ax0.h, $0x0E1D
052F 00 D8 0E 1E 	lr   	ax0.l, $0x0E1E
0531 00 9B 0E A4 	lri  	ax1.h, #0x0EA4
0533 00 9D 00 C0 	lri  	ac1.l, #0x00C0
0535 02 BF 05 41 	call 	$0x0541
0537 49 00       	addax	ac1, ax0        	     	
0538 00 83 0E 44 	lri  	ar3, #0x0E44
053A 02 BF 05 4C 	call 	$0x054C
053C 00 83 0E A4 	lri  	ar3, #0x0EA4
053E 02 BF 05 4C 	call 	$0x054C
0540 02 DF       	ret  	
```

```
0541 8E 00       	clr40	                	     	
0542 00 FA FF CE 	sr   	$(DSMAH), ax0.h
0544 00 F8 FF CF 	sr   	$(DSMAL), ax0.l
0546 00 FB FF CD 	sr   	$(DSPA), ax1.h
0548 16 C9 00 00 	si   	$(DSCR), #0x0000
054A 2D CB       	srs  	$(DSBL), ac1.l
054B 02 DF       	ret  	
```

```
054C 8F 00       	set40	                	     	
054D 8D 00       	set15	                	     	
054E 8A 00       	m2   	                	     	
054F 19 7A       	lrri 	ax0.h, @ar3
0550 19 78       	lrri 	ax0.l, @ar3
0551 A0 00       	mulx 	ax0.l, ax1.l    	     	
0552 B6 00       	mulxmv	ax0.h, ax1.l, ac0	     	
0553 11 30 05 5A 	bloopi	#0x30, $0x055A
0555 91 79       	asr16	ac0             	l    	ac1.m, @ar1
0556 4E 6D       	addp 	ac0             	ln   	ac1.l, @ar1
0557 19 7A       	lrri 	ax0.h, @ar3
0558 4D 43       	add  	ac1, ac0        	l    	ax0.l, @ar3
0559 A0 39       	mulx 	ax0.l, ax1.l    	s    	@ar1, ac1.m
055A B6 29       	mulxmv	ax0.h, ax1.l, ac0	s    	@ar1, ac1.l
055B 02 DF       	ret  	
```

```
055C 26 C9       	lrs  	ac0.m, $(DSCR)
055D 02 A0 00 04 	tclr 	ac0.m, #0x0004
055F 02 9C 05 5C 	jnok 	$0x055C
0561 02 DF       	ret  	
```

```
0562 26 FE       	lrs  	ac0.m, $(CMBH)
0563 02 C0 80 00 	tset 	ac0.m, #0x8000
0565 02 9C 05 62 	jnok 	$0x0562
0567 02 DF       	ret  	
```

```
0568 26 FC       	lrs  	ac0.m, $(DMBH)
0569 02 A0 80 00 	tclr 	ac0.m, #0x8000
056B 02 9C 05 68 	jnok 	$0x0568
056D 02 DF       	ret  	
```

```
056E 26 FC       	lrs  	ac0.m, $(DMBH)
056F 02 A0 80 00 	tclr 	ac0.m, #0x8000
0571 02 9C 05 6E 	jnok 	$0x056E
0573 02 DF       	ret  	
```

## Command 7

```
0574 81 00       	clr  	ac0             	     	
0575 89 70       	clr  	ac1             	l    	ac0.m, @ar0
0576 8E 60       	clr40	                	l    	ac0.l, @ar0
0577 2E CE       	srs  	$(DSMAH), ac0.m
0578 2C CF       	srs  	$(DSMAL), ac0.l
0579 16 CD 0E 44 	si   	$(DSPA), #0x0E44
057B 16 C9 00 00 	si   	$(DSCR), #0x0000
057D 89 00       	clr  	ac1             	     	
057E 0D 20       	lris 	ac1.l, 32
057F 2D CB       	srs  	$(DSBL), ac1.l
0580 4C 00       	add  	ac0, ac1        	     	
0581 1C 80       	mrr  	ix0, ar0
0582 00 80 02 80 	lri  	ar0, #0x0280
0584 00 81 00 00 	lri  	ar1, #0x0000
0586 00 82 01 40 	lri  	ar2, #0x0140
0588 00 83 0E 44 	lri  	ar3, #0x0E44
058A 0A 00       	lris 	ax0.h, 0
058B 27 C9       	lrs  	ac1.m, $(DSCR)
058C 03 A0 00 04 	tclr 	ac1.m, #0x0004
058E 02 9C 05 8B 	jnok 	$0x058B
0590 2E CE       	srs  	$(DSMAH), ac0.m
0591 2C CF       	srs  	$(DSMAL), ac0.l
0592 16 CD 0E 54 	si   	$(DSPA), #0x0E54
0594 16 C9 00 00 	si   	$(DSCR), #0x0000
0596 16 CB 02 60 	si   	$(DSBL), #0x0260
0598 00 9F 00 A0 	lri  	ac1.m, #0x00A0
059A 8F 00       	set40	                	     	
059B 00 7F 05 A4 	bloop	ac1.m, $0x05A4
	059D 19 7E       	lrri 	ac0.m, @ar3
	059E 1B 1A       	srri 	@ar0, ax0.h
	059F 19 7C       	lrri 	ac0.l, @ar3
	05A0 1B 1A       	srri 	@ar0, ax0.h
	05A1 1B 5E       	srri 	@ar2, ac0.m
	05A2 1B 5C       	srri 	@ar2, ac0.l
	05A3 1B 3E       	srri 	@ar1, ac0.m
	05A4 1B 3C       	srri 	@ar1, ac0.l
05A5 1C 04       	mrr  	ar0, ix0
05A6 02 9F 00 68 	j    	$0x0068
```

```
05A8 00 82 0B B8 	lri  	ar2, #0x0BB8
05AA 19 5E       	lrri 	ac0.m, @ar2
05AB 2E D1       	srs  	$(ACFMT), ac0.m
05AC 19 5E       	lrri 	ac0.m, @ar2
05AD 2E D4       	srs  	$(ACSAH), ac0.m
05AE 19 5E       	lrri 	ac0.m, @ar2
05AF 2E D5       	srs  	$(ACSAL), ac0.m
05B0 19 5E       	lrri 	ac0.m, @ar2
05B1 2E D6       	srs  	$(ACEAH), ac0.m
05B2 19 5E       	lrri 	ac0.m, @ar2
05B3 2E D7       	srs  	$(ACEAL), ac0.m
05B4 19 5E       	lrri 	ac0.m, @ar2
05B5 2E D8       	srs  	$(ACCAH), ac0.m
05B6 19 5E       	lrri 	ac0.m, @ar2
05B7 2E D9       	srs  	$(ACCAL), ac0.m
05B8 19 5E       	lrri 	ac0.m, @ar2
05B9 2E A0       	srs  	$(ADPCM_A00), ac0.m
05BA 19 5E       	lrri 	ac0.m, @ar2
05BB 2E A1       	srs  	$(ADPCM_A10), ac0.m
05BC 19 5E       	lrri 	ac0.m, @ar2
05BD 2E A2       	srs  	$(ADPCM_A20), ac0.m
05BE 19 5E       	lrri 	ac0.m, @ar2
05BF 2E A3       	srs  	$(ADPCM_A30), ac0.m
05C0 19 5E       	lrri 	ac0.m, @ar2
05C1 2E A4       	srs  	$(ADPCM_A40), ac0.m
05C2 19 5E       	lrri 	ac0.m, @ar2
05C3 2E A5       	srs  	$(ADPCM_A50), ac0.m
05C4 19 5E       	lrri 	ac0.m, @ar2
05C5 2E A6       	srs  	$(ADPCM_A60), ac0.m
05C6 19 5E       	lrri 	ac0.m, @ar2
05C7 2E A7       	srs  	$(ADPCM_A70), ac0.m
05C8 19 5E       	lrri 	ac0.m, @ar2
05C9 2E A8       	srs  	$(ADPCM_A01), ac0.m
05CA 19 5E       	lrri 	ac0.m, @ar2
05CB 2E A9       	srs  	$(ADPCM_A11), ac0.m
05CC 19 5E       	lrri 	ac0.m, @ar2
05CD 2E AA       	srs  	$(ADPCM_A21), ac0.m
05CE 19 5E       	lrri 	ac0.m, @ar2
05CF 2E AB       	srs  	$(ADPCM_A31), ac0.m
05D0 19 5E       	lrri 	ac0.m, @ar2
05D1 2E AC       	srs  	$(ADPCM_A41), ac0.m
05D2 19 5E       	lrri 	ac0.m, @ar2
05D3 2E AD       	srs  	$(ADPCM_A51), ac0.m
05D4 19 5E       	lrri 	ac0.m, @ar2
05D5 2E AE       	srs  	$(ADPCM_A61), ac0.m
05D6 19 5E       	lrri 	ac0.m, @ar2
05D7 2E AF       	srs  	$(ADPCM_A71), ac0.m
05D8 19 5E       	lrri 	ac0.m, @ar2
05D9 2E DE       	srs  	$(ACGAN), ac0.m
05DA 19 5E       	lrri 	ac0.m, @ar2
05DB 2E DA       	srs  	$(ACPDS), ac0.m
05DC 19 5E       	lrri 	ac0.m, @ar2
05DD 2E DB       	srs  	$(ACYN1), ac0.m
05DE 19 5E       	lrri 	ac0.m, @ar2
05DF 2E DC       	srs  	$(ACYN2), ac0.m
05E0 8C 00       	clr15	                	     	
05E1 8A 00       	m2   	                	     	
05E2 8E 00       	clr40	                	     	
05E3 00 D8 0E 16 	lr   	ax0.l, $0x0E16
05E5 19 5B       	lrri 	ax1.h, @ar2
05E6 19 59       	lrri 	ax1.l, @ar2
05E7 81 00       	clr  	ac0             	     	
05E8 19 5C       	lrri 	ac0.l, @ar2
05E9 00 80 0E 44 	lri  	ar0, #0x0E44
05EB 19 5F       	lrri 	ac1.m, @ar2
05EC 1B 1F       	srri 	@ar0, ac1.m
05ED 19 5F       	lrri 	ac1.m, @ar2
05EE 1B 1F       	srri 	@ar0, ac1.m
05EF 19 5F       	lrri 	ac1.m, @ar2
05F0 1B 1F       	srri 	@ar0, ac1.m
05F1 18 5F       	lrr  	ac1.m, @ar2
05F2 1B 1F       	srri 	@ar0, ac1.m
05F3 6B 00       	movax	ac1, ax1        	     	
05F4 15 05       	lsl  	ac1, #0x05
05F5 4D 00       	add  	ac1, ac0        	     	
05F6 15 7E       	lsr  	ac1, -2
05F7 1C 9F       	mrr  	ix0, ac1.m
05F8 1C BD       	mrr  	ix1, ac1.l
05F9 05 E0       	addis	ac1.m, -32
05FA 99 00       	asr16	ac1             	     	
05FB 7D 00       	neg  	ac1             	     	
05FC 1C DD       	mrr  	ix2, ac1.l
05FD 89 00       	clr  	ac1             	     	
05FE 1F A5       	mrr  	ac1.l, ix1
05FF 15 02       	lsl  	ac1, #0x02
0600 1C BF       	mrr  	ix1, ac1.m
0601 00 9A 01 FC 	lri  	ax0.h, #0x01FC
0603 00 9E 0E 44 	lri  	ac0.m, #0x0E44
0605 00 81 FF DD 	lri  	ar1, #0xFFDD
0607 00 83 0D 80 	lri  	ar3, #0x0D80
0609 00 64 06 1A 	bloop	ix0, $0x061A
060B 18 27       	lrr  	ix3, @ar1
060C 1B 07       	srri 	@ar0, ix3
060D 4A 00       	addax	ac0, ax1        	     	
060E 1F FC       	mrr  	ac1.m, ac0.l
060F 18 27       	lrr  	ix3, @ar1
0610 1B 07       	srri 	@ar0, ix3
0611 15 79       	lsr  	ac1, -7
0612 35 00       	andr 	ac1.m, ax0.h    	     	
0613 18 27       	lrr  	ix3, @ar1
0614 1B 07       	srri 	@ar0, ix3
0615 41 00       	addr 	ac1, ax0.l      	     	
0616 1B 7E       	srri 	@ar3, ac0.m
0617 18 27       	lrr  	ix3, @ar1
0618 1B 07       	srri 	@ar0, ix3
0619 1B 7F       	srri 	@ar3, ac1.m
061A 00 00       	nop  	
061B 00 65 06 20 	bloop	ix1, $0x0620
061D 18 27       	lrr  	ix3, @ar1
061E 1B 07       	srri 	@ar0, ix3
061F 00 00       	nop  	
0620 00 00       	nop  	
0621 00 07       	dar  	ar3
0622 18 7F       	lrr  	ac1.m, @ar3
0623 00 66 06 29 	bloop	ix2, $0x0629
0625 4A 3B       	addax	ac0, ax1        	s    	@ar3, ac1.m
0626 1F FC       	mrr  	ac1.m, ac0.l
0627 15 79       	lsr  	ac1, -7
0628 35 33       	andr 	ac1.m, ax0.h    	s    	@ar3, ac0.m
0629 41 00       	addr 	ac1, ax0.l      	     	
062A 1B 7F       	srri 	@ar3, ac1.m
062B 00 04       	dar  	ar0
062C 18 9F       	lrrd 	ac1.m, @ar0
062D 1A DF       	srrd 	@ar2, ac1.m
062E 18 9F       	lrrd 	ac1.m, @ar0
062F 1A DF       	srrd 	@ar2, ac1.m
0630 18 9F       	lrrd 	ac1.m, @ar0
0631 1A DF       	srrd 	@ar2, ac1.m
0632 18 9F       	lrrd 	ac1.m, @ar0
0633 1A DF       	srrd 	@ar2, ac1.m
0634 1A DC       	srrd 	@ar2, ac0.l
0635 00 82 0B D2 	lri  	ar2, #0x0BD2
0637 27 DC       	lrs  	ac1.m, $(ACYN2)
0638 1A DF       	srrd 	@ar2, ac1.m
0639 27 DB       	lrs  	ac1.m, $(ACYN1)
063A 1A DF       	srrd 	@ar2, ac1.m
063B 27 DA       	lrs  	ac1.m, $(ACPDS)
063C 1A DF       	srrd 	@ar2, ac1.m
063D 00 82 0B BE 	lri  	ar2, #0x0BBE
063F 27 D9       	lrs  	ac1.m, $(ACCAL)
0640 1A DF       	srrd 	@ar2, ac1.m
0641 27 D8       	lrs  	ac1.m, $(ACCAH)
0642 1A DF       	srrd 	@ar2, ac1.m
0643 8F 00       	set40	                	     	
0644 00 C1 0E 42 	lr   	ar1, $0x0E42
0646 00 82 0D 80 	lri  	ar2, #0x0D80
0648 19 40       	lrri 	ar0, @ar2
0649 19 43       	lrri 	ar3, @ar2
064A 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
064B B8 C0       	mulx 	ax0.h, ax1.h    	ld   	ax0.l, ax1.l, @ar0
064C 11 1F 06 54 	bloopi	#0x1F, $0x0654
064E A6 F0       	mulxmv	ax0.l, ax1.l, ac0	ld   	ax0.h, ax1.h, @ar0
064F BC F0       	mulxac	ax0.h, ax1.h, ac0	ld   	ax0.h, ax1.h, @ar0
0650 19 40       	lrri 	ar0, @ar2
0651 19 43       	lrri 	ar3, @ar2
0652 BC F0       	mulxac	ax0.h, ax1.h, ac0	ld   	ax0.h, ax1.h, @ar0
0653 4E C0       	addp 	ac0             	ld   	ax0.l, ax1.l, @ar0
0654 B8 31       	mulx 	ax0.h, ax1.h    	s    	@ar1, ac0.m
0655 A6 F0       	mulxmv	ax0.l, ax1.l, ac0	ld   	ax0.h, ax1.h, @ar0
0656 BC F0       	mulxac	ax0.h, ax1.h, ac0	ld   	ax0.h, ax1.h, @ar0
0657 BC 00       	mulxac	ax0.h, ax1.h, ac0	     	
0658 4E 00       	addp 	ac0             	     	
0659 1B 3E       	srri 	@ar1, ac0.m
065A 00 E1 0E 42 	sr   	$0x0E42, ar1
065C 02 DF       	ret  	
```

```
065D 00 82 0B B8 	lri  	ar2, #0x0BB8
065F 19 5E       	lrri 	ac0.m, @ar2
0660 2E D1       	srs  	$(ACFMT), ac0.m
0661 19 5E       	lrri 	ac0.m, @ar2
0662 2E D4       	srs  	$(ACSAH), ac0.m
0663 19 5E       	lrri 	ac0.m, @ar2
0664 2E D5       	srs  	$(ACSAL), ac0.m
0665 19 5E       	lrri 	ac0.m, @ar2
0666 2E D6       	srs  	$(ACEAH), ac0.m
0667 19 5E       	lrri 	ac0.m, @ar2
0668 2E D7       	srs  	$(ACEAL), ac0.m
0669 19 5E       	lrri 	ac0.m, @ar2
066A 2E D8       	srs  	$(ACCAH), ac0.m
066B 19 5E       	lrri 	ac0.m, @ar2
066C 2E D9       	srs  	$(ACCAL), ac0.m
066D 19 5E       	lrri 	ac0.m, @ar2
066E 2E A0       	srs  	$(ADPCM_A00), ac0.m
066F 19 5E       	lrri 	ac0.m, @ar2
0670 2E A1       	srs  	$(ADPCM_A10), ac0.m
0671 19 5E       	lrri 	ac0.m, @ar2
0672 2E A2       	srs  	$(ADPCM_A20), ac0.m
0673 19 5E       	lrri 	ac0.m, @ar2
0674 2E A3       	srs  	$(ADPCM_A30), ac0.m
0675 19 5E       	lrri 	ac0.m, @ar2
0676 2E A4       	srs  	$(ADPCM_A40), ac0.m
0677 19 5E       	lrri 	ac0.m, @ar2
0678 2E A5       	srs  	$(ADPCM_A50), ac0.m
0679 19 5E       	lrri 	ac0.m, @ar2
067A 2E A6       	srs  	$(ADPCM_A60), ac0.m
067B 19 5E       	lrri 	ac0.m, @ar2
067C 2E A7       	srs  	$(ADPCM_A70), ac0.m
067D 19 5E       	lrri 	ac0.m, @ar2
067E 2E A8       	srs  	$(ADPCM_A01), ac0.m
067F 19 5E       	lrri 	ac0.m, @ar2
0680 2E A9       	srs  	$(ADPCM_A11), ac0.m
0681 19 5E       	lrri 	ac0.m, @ar2
0682 2E AA       	srs  	$(ADPCM_A21), ac0.m
0683 19 5E       	lrri 	ac0.m, @ar2
0684 2E AB       	srs  	$(ADPCM_A31), ac0.m
0685 19 5E       	lrri 	ac0.m, @ar2
0686 2E AC       	srs  	$(ADPCM_A41), ac0.m
0687 19 5E       	lrri 	ac0.m, @ar2
0688 2E AD       	srs  	$(ADPCM_A51), ac0.m
0689 19 5E       	lrri 	ac0.m, @ar2
068A 2E AE       	srs  	$(ADPCM_A61), ac0.m
068B 19 5E       	lrri 	ac0.m, @ar2
068C 2E AF       	srs  	$(ADPCM_A71), ac0.m
068D 19 5E       	lrri 	ac0.m, @ar2
068E 2E DE       	srs  	$(ACGAN), ac0.m
068F 19 5E       	lrri 	ac0.m, @ar2
0690 2E DA       	srs  	$(ACPDS), ac0.m
0691 19 5E       	lrri 	ac0.m, @ar2
0692 2E DB       	srs  	$(ACYN1), ac0.m
0693 19 5E       	lrri 	ac0.m, @ar2
0694 2E DC       	srs  	$(ACYN2), ac0.m
0695 8C 00       	clr15	                	     	
0696 8A 00       	m2   	                	     	
0697 8E 00       	clr40	                	     	
0698 19 5B       	lrri 	ax1.h, @ar2
0699 19 59       	lrri 	ax1.l, @ar2
069A 81 00       	clr  	ac0             	     	
069B 19 5C       	lrri 	ac0.l, @ar2
069C 00 80 0E 44 	lri  	ar0, #0x0E44
069E 19 5F       	lrri 	ac1.m, @ar2
069F 19 5F       	lrri 	ac1.m, @ar2
06A0 19 5F       	lrri 	ac1.m, @ar2
06A1 1B 1F       	srri 	@ar0, ac1.m
06A2 18 5F       	lrr  	ac1.m, @ar2
06A3 1B 1F       	srri 	@ar0, ac1.m
06A4 6B 00       	movax	ac1, ax1        	     	
06A5 15 05       	lsl  	ac1, #0x05
06A6 4D 00       	add  	ac1, ac0        	     	
06A7 15 7E       	lsr  	ac1, -2
06A8 1C 9F       	mrr  	ix0, ac1.m
06A9 1C BD       	mrr  	ix1, ac1.l
06AA 05 E0       	addis	ac1.m, -32
06AB 99 00       	asr16	ac1             	     	
06AC 7D 00       	neg  	ac1             	     	
06AD 1C DD       	mrr  	ix2, ac1.l
06AE 89 00       	clr  	ac1             	     	
06AF 1F A5       	mrr  	ac1.l, ix1
06B0 15 02       	lsl  	ac1, #0x02
06B1 1C BF       	mrr  	ix1, ac1.m
06B2 00 9A 01 FC 	lri  	ax0.h, #0x01FC
06B4 00 9E 0E 45 	lri  	ac0.m, #0x0E45
06B6 00 81 FF DD 	lri  	ar1, #0xFFDD
06B8 00 83 0D 80 	lri  	ar3, #0x0D80
06BA 00 64 06 CB 	bloop	ix0, $0x06CB
06BC 18 27       	lrr  	ix3, @ar1
06BD 1B 07       	srri 	@ar0, ix3
06BE 4A 00       	addax	ac0, ax1        	     	
06BF 1B 7E       	srri 	@ar3, ac0.m
06C0 18 27       	lrr  	ix3, @ar1
06C1 1B 07       	srri 	@ar0, ix3
06C2 1B 7C       	srri 	@ar3, ac0.l
06C3 00 00       	nop  	
06C4 18 27       	lrr  	ix3, @ar1
06C5 1B 07       	srri 	@ar0, ix3
06C6 00 00       	nop  	
06C7 00 00       	nop  	
06C8 18 27       	lrr  	ix3, @ar1
06C9 1B 07       	srri 	@ar0, ix3
06CA 00 00       	nop  	
06CB 00 00       	nop  	
06CC 00 65 06 D1 	bloop	ix1, $0x06D1
06CE 18 27       	lrr  	ix3, @ar1
06CF 1B 07       	srri 	@ar0, ix3
06D0 00 00       	nop  	
06D1 00 00       	nop  	
06D2 00 66 06 D6 	bloop	ix2, $0x06D6
06D4 4A 00       	addax	ac0, ax1        	     	
06D5 1B 7E       	srri 	@ar3, ac0.m
06D6 1B 7C       	srri 	@ar3, ac0.l
06D7 00 04       	dar  	ar0
06D8 18 9F       	lrrd 	ac1.m, @ar0
06D9 1A DF       	srrd 	@ar2, ac1.m
06DA 18 9F       	lrrd 	ac1.m, @ar0
06DB 1A DF       	srrd 	@ar2, ac1.m
06DC 18 9F       	lrrd 	ac1.m, @ar0
06DD 1A DF       	srrd 	@ar2, ac1.m
06DE 18 9F       	lrrd 	ac1.m, @ar0
06DF 1A DF       	srrd 	@ar2, ac1.m
06E0 1A DC       	srrd 	@ar2, ac0.l
06E1 00 82 0B D2 	lri  	ar2, #0x0BD2
06E3 27 DC       	lrs  	ac1.m, $(ACYN2)
06E4 1A DF       	srrd 	@ar2, ac1.m
06E5 27 DB       	lrs  	ac1.m, $(ACYN1)
06E6 1A DF       	srrd 	@ar2, ac1.m
06E7 27 DA       	lrs  	ac1.m, $(ACPDS)
06E8 1A DF       	srrd 	@ar2, ac1.m
06E9 00 82 0B BE 	lri  	ar2, #0x0BBE
06EB 27 D9       	lrs  	ac1.m, $(ACCAL)
06EC 1A DF       	srrd 	@ar2, ac1.m
06ED 27 D8       	lrs  	ac1.m, $(ACCAH)
06EE 1A DF       	srrd 	@ar2, ac1.m
06EF 8D 00       	set15	                	     	
06F0 8B 00       	m0   	                	     	
06F1 8F 00       	set40	                	     	
06F2 00 C1 0E 42 	lr   	ar1, $0x0E42
06F4 00 82 0D 80 	lri  	ar2, #0x0D80
06F6 81 00       	clr  	ac0             	     	
06F7 11 20 07 03 	bloopi	#0x20, $0x0703
06F9 89 00       	clr  	ac1             	     	
06FA 19 40       	lrri 	ar0, @ar2
06FB 18 9E       	lrrd 	ac0.m, @ar0
06FC 18 1B       	lrr  	ax1.h, @ar0
06FD 19 9A       	lrrn 	ax0.h, @ar0
06FE 54 00       	subr 	ac0, ax0.h      	     	
06FF 1F 5E       	mrr  	ax0.h, ac0.m
0700 19 59       	lrri 	ax1.l, @ar2
0701 B0 00       	mulx 	ax0.h, ax1.l    	     	
0702 FB 00       	addpaxz	ac1, ax1.h    	     	
0703 81 39       	clr  	ac0             	s    	@ar1, ac1.m
0704 00 E1 0E 42 	sr   	$0x0E42, ar1
0706 02 DF       	ret  	
```

## SRC[3] - None

```
0707 00 82 0B B8 	lri  	ar2, #0x0BB8
0709 19 5E       	lrri 	ac0.m, @ar2
070A 2E D1       	srs  	$(ACFMT), ac0.m
070B 19 5E       	lrri 	ac0.m, @ar2
070C 2E D4       	srs  	$(ACSAH), ac0.m
070D 19 5E       	lrri 	ac0.m, @ar2
070E 2E D5       	srs  	$(ACSAL), ac0.m
070F 19 5E       	lrri 	ac0.m, @ar2
0710 2E D6       	srs  	$(ACEAH), ac0.m
0711 19 5E       	lrri 	ac0.m, @ar2
0712 2E D7       	srs  	$(ACEAL), ac0.m
0713 19 5E       	lrri 	ac0.m, @ar2
0714 2E D8       	srs  	$(ACCAH), ac0.m
0715 19 5E       	lrri 	ac0.m, @ar2
0716 2E D9       	srs  	$(ACCAL), ac0.m
0717 19 5E       	lrri 	ac0.m, @ar2
0718 2E A0       	srs  	$(ADPCM_A00), ac0.m
0719 19 5E       	lrri 	ac0.m, @ar2
071A 2E A1       	srs  	$(ADPCM_A10), ac0.m
071B 19 5E       	lrri 	ac0.m, @ar2
071C 2E A2       	srs  	$(ADPCM_A20), ac0.m
071D 19 5E       	lrri 	ac0.m, @ar2
071E 2E A3       	srs  	$(ADPCM_A30), ac0.m
071F 19 5E       	lrri 	ac0.m, @ar2
0720 2E A4       	srs  	$(ADPCM_A40), ac0.m
0721 19 5E       	lrri 	ac0.m, @ar2
0722 2E A5       	srs  	$(ADPCM_A50), ac0.m
0723 19 5E       	lrri 	ac0.m, @ar2
0724 2E A6       	srs  	$(ADPCM_A60), ac0.m
0725 19 5E       	lrri 	ac0.m, @ar2
0726 2E A7       	srs  	$(ADPCM_A70), ac0.m
0727 19 5E       	lrri 	ac0.m, @ar2
0728 2E A8       	srs  	$(ADPCM_A01), ac0.m
0729 19 5E       	lrri 	ac0.m, @ar2
072A 2E A9       	srs  	$(ADPCM_A11), ac0.m
072B 19 5E       	lrri 	ac0.m, @ar2
072C 2E AA       	srs  	$(ADPCM_A21), ac0.m
072D 19 5E       	lrri 	ac0.m, @ar2
072E 2E AB       	srs  	$(ADPCM_A31), ac0.m
072F 19 5E       	lrri 	ac0.m, @ar2
0730 2E AC       	srs  	$(ADPCM_A41), ac0.m
0731 19 5E       	lrri 	ac0.m, @ar2
0732 2E AD       	srs  	$(ADPCM_A51), ac0.m
0733 19 5E       	lrri 	ac0.m, @ar2
0734 2E AE       	srs  	$(ADPCM_A61), ac0.m
0735 19 5E       	lrri 	ac0.m, @ar2
0736 2E AF       	srs  	$(ADPCM_A71), ac0.m
0737 19 5E       	lrri 	ac0.m, @ar2
0738 2E DE       	srs  	$(ACGAN), ac0.m
0739 19 5E       	lrri 	ac0.m, @ar2
073A 2E DA       	srs  	$(ACPDS), ac0.m
073B 19 5E       	lrri 	ac0.m, @ar2
073C 2E DB       	srs  	$(ACYN1), ac0.m
073D 19 5E       	lrri 	ac0.m, @ar2
073E 2E DC       	srs  	$(ACYN2), ac0.m
073F 00 C0 0E 42 	lr   	ar0, $0x0E42
0741 00 81 FF DD 	lri  	ar1, #0xFFDD
0743 11 20 07 48 	bloopi	#0x20, $0x0748
	0745 18 24       	lrr  	ix0, @ar1
	0746 1B 04       	srri 	@ar0, ix0
	0747 00 00       	nop  	
	0748 00 00       	nop  	
0749 00 E0 0E 42 	sr   	$0x0E42, ar0
074B 00 82 0B D9 	lri  	ar2, #0x0BD9
074D 00 04       	dar  	ar0
074E 18 9F       	lrrd 	ac1.m, @ar0
074F 1A DF       	srrd 	@ar2, ac1.m
0750 18 9F       	lrrd 	ac1.m, @ar0
0751 1A DF       	srrd 	@ar2, ac1.m
0752 18 9F       	lrrd 	ac1.m, @ar0
0753 1A DF       	srrd 	@ar2, ac1.m
0754 18 9F       	lrrd 	ac1.m, @ar0
0755 1A DF       	srrd 	@ar2, ac1.m
0756 89 00       	clr  	ac1             	     	
0757 1A DC       	srrd 	@ar2, ac0.l
0758 27 DC       	lrs  	ac1.m, $(ACYN2)
0759 00 FF 0B D2 	sr   	$0x0BD2, ac1.m
075B 27 DB       	lrs  	ac1.m, $(ACYN1)
075C 00 FF 0B D1 	sr   	$0x0BD1, ac1.m
075E 27 DA       	lrs  	ac1.m, $(ACPDS)
075F 00 FF 0B D0 	sr   	$0x0BD0, ac1.m
0761 27 D9       	lrs  	ac1.m, $(ACCAL)
0762 00 FF 0B BE 	sr   	$0x0BBE, ac1.m
0764 27 D8       	lrs  	ac1.m, $(ACCAH)
0765 00 FF 0B BD 	sr   	$0x0BBD, ac1.m
0767 02 DF       	ret  	
```

```
0768 00 C0 0E 40 	lr   	ar0, $0x0E40
076A 00 81 0B 89 	lri  	ar1, #0x0B89
076C 00 C2 0E 08 	lr   	ar2, $0x0E08
076E 1C 62       	mrr  	ar3, ar2
076F 00 C4 0E 41 	lr   	ix0, $0x0E41
0771 00 C5 0E 09 	lr   	ix1, $0x0E09
0773 02 BF 80 E7 	call 	$0x80E7
0775 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
0777 00 FB 0B AC 	sr   	$0x0BAC, ax1.h
0779 02 DF       	ret  	
077A 00 C0 0E 40 	lr   	ar0, $0x0E40
077C 00 81 0B 89 	lri  	ar1, #0x0B89
077E 00 C2 0E 08 	lr   	ar2, $0x0E08
0780 1C 62       	mrr  	ar3, ar2
0781 00 C4 0E 41 	lr   	ix0, $0x0E41
0783 00 C5 0E 09 	lr   	ix1, $0x0E09
0785 02 BF 80 E7 	call 	$0x80E7
0787 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
0789 00 FB 0B AC 	sr   	$0x0BAC, ax1.h
078B 00 C0 0E 40 	lr   	ar0, $0x0E40
078D 00 81 0B 8D 	lri  	ar1, #0x0B8D
078F 00 C2 0E 0B 	lr   	ar2, $0x0E0B
0791 1C 62       	mrr  	ar3, ar2
0792 00 C4 0E 41 	lr   	ix0, $0x0E41
0794 00 C5 0E 0C 	lr   	ix1, $0x0E0C
0796 02 BF 80 E7 	call 	$0x80E7
0798 00 F8 0B AA 	sr   	$0x0BAA, ax0.l
079A 00 FB 0B AD 	sr   	$0x0BAD, ax1.h
079C 02 DF       	ret  	
079D 00 C0 0E 40 	lr   	ar0, $0x0E40
079F 00 81 0B 89 	lri  	ar1, #0x0B89
07A1 00 C2 0E 08 	lr   	ar2, $0x0E08
07A3 1C 62       	mrr  	ar3, ar2
07A4 00 C4 0E 41 	lr   	ix0, $0x0E41
07A6 00 C5 0E 09 	lr   	ix1, $0x0E09
07A8 02 BF 80 E7 	call 	$0x80E7
07AA 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
07AC 00 FB 0B AC 	sr   	$0x0BAC, ax1.h
07AE 00 C0 0E 40 	lr   	ar0, $0x0E40
07B0 00 81 0B 91 	lri  	ar1, #0x0B91
07B2 00 C2 0E 0E 	lr   	ar2, $0x0E0E
07B4 1C 62       	mrr  	ar3, ar2
07B5 00 C4 0E 41 	lr   	ix0, $0x0E41
07B7 00 C5 0E 0F 	lr   	ix1, $0x0E0F
07B9 02 BF 80 E7 	call 	$0x80E7
07BB 00 F8 0B AB 	sr   	$0x0BAB, ax0.l
07BD 00 FB 0B AE 	sr   	$0x0BAE, ax1.h
07BF 02 DF       	ret  	
07C0 00 C0 0E 40 	lr   	ar0, $0x0E40
07C2 00 81 0B 89 	lri  	ar1, #0x0B89
07C4 00 C2 0E 08 	lr   	ar2, $0x0E08
07C6 1C 62       	mrr  	ar3, ar2
07C7 00 C4 0E 41 	lr   	ix0, $0x0E41
07C9 00 C5 0E 09 	lr   	ix1, $0x0E09
07CB 02 BF 80 E7 	call 	$0x80E7
07CD 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
07CF 00 FB 0B AC 	sr   	$0x0BAC, ax1.h
07D1 00 C0 0E 40 	lr   	ar0, $0x0E40
07D3 00 81 0B 8D 	lri  	ar1, #0x0B8D
07D5 00 C2 0E 0B 	lr   	ar2, $0x0E0B
07D7 1C 62       	mrr  	ar3, ar2
07D8 00 C4 0E 41 	lr   	ix0, $0x0E41
07DA 00 C5 0E 0C 	lr   	ix1, $0x0E0C
07DC 02 BF 80 E7 	call 	$0x80E7
07DE 00 F8 0B AA 	sr   	$0x0BAA, ax0.l
07E0 00 FB 0B AD 	sr   	$0x0BAD, ax1.h
07E2 00 C0 0E 40 	lr   	ar0, $0x0E40
07E4 00 81 0B 91 	lri  	ar1, #0x0B91
07E6 00 C2 0E 0E 	lr   	ar2, $0x0E0E
07E8 1C 62       	mrr  	ar3, ar2
07E9 00 C4 0E 41 	lr   	ix0, $0x0E41
07EB 00 C5 0E 0F 	lr   	ix1, $0x0E0F
07ED 02 BF 80 E7 	call 	$0x80E7
07EF 00 F8 0B AB 	sr   	$0x0BAB, ax0.l
07F1 00 FB 0B AE 	sr   	$0x0BAE, ax1.h
07F3 02 DF       	ret  	
07F4 00 C0 0E 40 	lr   	ar0, $0x0E40
07F6 00 81 0B 89 	lri  	ar1, #0x0B89
07F8 00 C2 0E 08 	lr   	ar2, $0x0E08
07FA 1C 62       	mrr  	ar3, ar2
07FB 00 C4 0E 41 	lr   	ix0, $0x0E41
07FD 00 C5 0E 09 	lr   	ix1, $0x0E09
07FF 02 BF 80 E7 	call 	$0x80E7
0801 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
0803 00 FB 0B AC 	sr   	$0x0BAC, ax1.h
0805 00 C0 0E 43 	lr   	ar0, $0x0E43
0807 00 81 0B 97 	lri  	ar1, #0x0B97
0809 00 C2 0E 0A 	lr   	ar2, $0x0E0A
080B 1C 62       	mrr  	ar3, ar2
080C 02 BF 81 F9 	call 	$0x81F9
080E 00 F8 0B AF 	sr   	$0x0BAF, ax0.l
0810 02 DF       	ret  	
0811 00 C0 0E 40 	lr   	ar0, $0x0E40
0813 00 81 0B 89 	lri  	ar1, #0x0B89
0815 00 C2 0E 08 	lr   	ar2, $0x0E08
0817 1C 62       	mrr  	ar3, ar2
0818 00 C4 0E 41 	lr   	ix0, $0x0E41
081A 00 C5 0E 09 	lr   	ix1, $0x0E09
081C 02 BF 80 E7 	call 	$0x80E7
081E 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
0820 00 FB 0B AC 	sr   	$0x0BAC, ax1.h
0822 00 C0 0E 40 	lr   	ar0, $0x0E40
0824 00 81 0B 8D 	lri  	ar1, #0x0B8D
0826 00 C2 0E 0B 	lr   	ar2, $0x0E0B
0828 1C 62       	mrr  	ar3, ar2
0829 00 C4 0E 41 	lr   	ix0, $0x0E41
082B 00 C5 0E 0C 	lr   	ix1, $0x0E0C
082D 02 BF 80 E7 	call 	$0x80E7
082F 00 F8 0B AA 	sr   	$0x0BAA, ax0.l
0831 00 FB 0B AD 	sr   	$0x0BAD, ax1.h
0833 00 C0 0E 43 	lr   	ar0, $0x0E43
0835 00 81 0B 97 	lri  	ar1, #0x0B97
0837 00 C2 0E 0A 	lr   	ar2, $0x0E0A
0839 1C 62       	mrr  	ar3, ar2
083A 1C 80       	mrr  	ix0, ar0
083B 00 C5 0E 0D 	lr   	ix1, $0x0E0D
083D 02 BF 80 E7 	call 	$0x80E7
083F 00 F8 0B AF 	sr   	$0x0BAF, ax0.l
0841 00 FB 0B B0 	sr   	$0x0BB0, ax1.h
0843 02 DF       	ret  	
0844 00 C0 0E 40 	lr   	ar0, $0x0E40
0846 00 81 0B 89 	lri  	ar1, #0x0B89
0848 00 C2 0E 08 	lr   	ar2, $0x0E08
084A 1C 62       	mrr  	ar3, ar2
084B 00 C4 0E 41 	lr   	ix0, $0x0E41
084D 00 C5 0E 09 	lr   	ix1, $0x0E09
084F 02 BF 80 E7 	call 	$0x80E7
0851 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
0853 00 FB 0B AC 	sr   	$0x0BAC, ax1.h
0855 00 C0 0E 40 	lr   	ar0, $0x0E40
0857 00 81 0B 91 	lri  	ar1, #0x0B91
0859 00 C2 0E 0E 	lr   	ar2, $0x0E0E
085B 1C 62       	mrr  	ar3, ar2
085C 00 C4 0E 41 	lr   	ix0, $0x0E41
085E 00 C5 0E 0F 	lr   	ix1, $0x0E0F
0860 02 BF 80 E7 	call 	$0x80E7
0862 00 F8 0B AB 	sr   	$0x0BAB, ax0.l
0864 00 FB 0B AE 	sr   	$0x0BAE, ax1.h
0866 00 C0 0E 43 	lr   	ar0, $0x0E43
0868 00 81 0B 95 	lri  	ar1, #0x0B95
086A 00 C2 0E 10 	lr   	ar2, $0x0E10
086C 1C 62       	mrr  	ar3, ar2
086D 1C 80       	mrr  	ix0, ar0
086E 00 C5 0E 0A 	lr   	ix1, $0x0E0A
0870 02 BF 80 E7 	call 	$0x80E7
0872 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0874 00 FB 0B AF 	sr   	$0x0BAF, ax1.h
0876 02 DF       	ret  	
0877 00 C0 0E 40 	lr   	ar0, $0x0E40
0879 00 81 0B 89 	lri  	ar1, #0x0B89
087B 00 C2 0E 08 	lr   	ar2, $0x0E08
087D 1C 62       	mrr  	ar3, ar2
087E 00 C4 0E 41 	lr   	ix0, $0x0E41
0880 00 C5 0E 09 	lr   	ix1, $0x0E09
0882 02 BF 80 E7 	call 	$0x80E7
0884 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
0886 00 FB 0B AC 	sr   	$0x0BAC, ax1.h
0888 00 C0 0E 40 	lr   	ar0, $0x0E40
088A 00 81 0B 8D 	lri  	ar1, #0x0B8D
088C 00 C2 0E 0B 	lr   	ar2, $0x0E0B
088E 1C 62       	mrr  	ar3, ar2
088F 00 C4 0E 41 	lr   	ix0, $0x0E41
0891 00 C5 0E 0C 	lr   	ix1, $0x0E0C
0893 02 BF 80 E7 	call 	$0x80E7
0895 00 F8 0B AA 	sr   	$0x0BAA, ax0.l
0897 00 FB 0B AD 	sr   	$0x0BAD, ax1.h
0899 00 C0 0E 40 	lr   	ar0, $0x0E40
089B 00 81 0B 91 	lri  	ar1, #0x0B91
089D 00 C2 0E 0E 	lr   	ar2, $0x0E0E
089F 1C 62       	mrr  	ar3, ar2
08A0 00 C4 0E 41 	lr   	ix0, $0x0E41
08A2 00 C5 0E 0F 	lr   	ix1, $0x0E0F
08A4 02 BF 80 E7 	call 	$0x80E7
08A6 00 F8 0B AB 	sr   	$0x0BAB, ax0.l
08A8 00 FB 0B AE 	sr   	$0x0BAE, ax1.h
08AA 00 C0 0E 43 	lr   	ar0, $0x0E43
08AC 00 81 0B 97 	lri  	ar1, #0x0B97
08AE 00 C2 0E 0A 	lr   	ar2, $0x0E0A
08B0 1C 62       	mrr  	ar3, ar2
08B1 1C 80       	mrr  	ix0, ar0
08B2 00 C5 0E 0D 	lr   	ix1, $0x0E0D
08B4 02 BF 80 E7 	call 	$0x80E7
08B6 00 F8 0B AF 	sr   	$0x0BAF, ax0.l
08B8 00 FB 0B B0 	sr   	$0x0BB0, ax1.h
08BA 00 C0 0E 43 	lr   	ar0, $0x0E43
08BC 00 81 0B 95 	lri  	ar1, #0x0B95
08BE 00 C2 0E 10 	lr   	ar2, $0x0E10
08C0 1C 62       	mrr  	ar3, ar2
08C1 02 BF 81 F9 	call 	$0x81F9
08C3 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
08C5 02 DF       	ret  	
08C6 00 C0 0E 40 	lr   	ar0, $0x0E40
08C8 00 81 0B 89 	lri  	ar1, #0x0B89
08CA 00 C2 0E 08 	lr   	ar2, $0x0E08
08CC 00 83 0E 44 	lri  	ar3, #0x0E44
08CE 00 C4 0E 41 	lr   	ix0, $0x0E41
08D0 00 C5 0E 09 	lr   	ix1, $0x0E09
08D2 02 BF 82 82 	call 	$0x8282
08D4 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
08D6 00 FB 0B AC 	sr   	$0x0BAC, ax1.h
08D8 02 DF       	ret  	
08D9 00 C0 0E 40 	lr   	ar0, $0x0E40
08DB 00 81 0B 89 	lri  	ar1, #0x0B89
08DD 00 C2 0E 08 	lr   	ar2, $0x0E08
08DF 00 83 0E 44 	lri  	ar3, #0x0E44
08E1 00 C4 0E 41 	lr   	ix0, $0x0E41
08E3 00 C5 0E 09 	lr   	ix1, $0x0E09
08E5 02 BF 82 82 	call 	$0x8282
08E7 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
08E9 00 FB 0B AC 	sr   	$0x0BAC, ax1.h
08EB 00 C0 0E 40 	lr   	ar0, $0x0E40
08ED 00 81 0B 8D 	lri  	ar1, #0x0B8D
08EF 00 C2 0E 0B 	lr   	ar2, $0x0E0B
08F1 00 83 0E 44 	lri  	ar3, #0x0E44
08F3 00 C4 0E 41 	lr   	ix0, $0x0E41
08F5 00 C5 0E 0C 	lr   	ix1, $0x0E0C
08F7 02 BF 82 82 	call 	$0x8282
08F9 00 F8 0B AA 	sr   	$0x0BAA, ax0.l
08FB 00 FB 0B AD 	sr   	$0x0BAD, ax1.h
08FD 02 DF       	ret  	
08FE 00 C0 0E 40 	lr   	ar0, $0x0E40
0900 00 81 0B 89 	lri  	ar1, #0x0B89
0902 00 C2 0E 08 	lr   	ar2, $0x0E08
0904 00 83 0E 44 	lri  	ar3, #0x0E44
0906 00 C4 0E 41 	lr   	ix0, $0x0E41
0908 00 C5 0E 09 	lr   	ix1, $0x0E09
090A 02 BF 82 82 	call 	$0x8282
090C 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
090E 00 FB 0B AC 	sr   	$0x0BAC, ax1.h
0910 00 C0 0E 40 	lr   	ar0, $0x0E40
0912 00 81 0B 91 	lri  	ar1, #0x0B91
0914 00 C2 0E 0E 	lr   	ar2, $0x0E0E
0916 00 83 0E 44 	lri  	ar3, #0x0E44
0918 00 C4 0E 41 	lr   	ix0, $0x0E41
091A 00 C5 0E 0F 	lr   	ix1, $0x0E0F
091C 02 BF 82 82 	call 	$0x8282
091E 00 F8 0B AB 	sr   	$0x0BAB, ax0.l
0920 00 FB 0B AE 	sr   	$0x0BAE, ax1.h
0922 02 DF       	ret  	
0923 00 C0 0E 40 	lr   	ar0, $0x0E40
0925 00 81 0B 89 	lri  	ar1, #0x0B89
0927 00 C2 0E 08 	lr   	ar2, $0x0E08
0929 00 83 0E 44 	lri  	ar3, #0x0E44
092B 00 C4 0E 41 	lr   	ix0, $0x0E41
092D 00 C5 0E 09 	lr   	ix1, $0x0E09
092F 02 BF 82 82 	call 	$0x8282
0931 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
0933 00 FB 0B AC 	sr   	$0x0BAC, ax1.h
0935 00 C0 0E 40 	lr   	ar0, $0x0E40
0937 00 81 0B 8D 	lri  	ar1, #0x0B8D
0939 00 C2 0E 0B 	lr   	ar2, $0x0E0B
093B 00 83 0E 44 	lri  	ar3, #0x0E44
093D 00 C4 0E 41 	lr   	ix0, $0x0E41
093F 00 C5 0E 0C 	lr   	ix1, $0x0E0C
0941 02 BF 82 82 	call 	$0x8282
0943 00 F8 0B AA 	sr   	$0x0BAA, ax0.l
0945 00 FB 0B AD 	sr   	$0x0BAD, ax1.h
0947 00 C0 0E 40 	lr   	ar0, $0x0E40
0949 00 81 0B 91 	lri  	ar1, #0x0B91
094B 00 C2 0E 0E 	lr   	ar2, $0x0E0E
094D 00 83 0E 44 	lri  	ar3, #0x0E44
094F 00 C4 0E 41 	lr   	ix0, $0x0E41
0951 00 C5 0E 0F 	lr   	ix1, $0x0E0F
0953 02 BF 82 82 	call 	$0x8282
0955 00 F8 0B AB 	sr   	$0x0BAB, ax0.l
0957 00 FB 0B AE 	sr   	$0x0BAE, ax1.h
0959 02 DF       	ret  	
095A 00 C0 0E 40 	lr   	ar0, $0x0E40
095C 00 81 0B 89 	lri  	ar1, #0x0B89
095E 00 C2 0E 08 	lr   	ar2, $0x0E08
0960 00 83 0E 44 	lri  	ar3, #0x0E44
0962 00 C4 0E 41 	lr   	ix0, $0x0E41
0964 00 C5 0E 09 	lr   	ix1, $0x0E09
0966 02 BF 82 82 	call 	$0x8282
0968 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
096A 00 FB 0B AC 	sr   	$0x0BAC, ax1.h
096C 00 C0 0E 43 	lr   	ar0, $0x0E43
096E 00 81 0B 97 	lri  	ar1, #0x0B97
0970 00 C2 0E 0A 	lr   	ar2, $0x0E0A
0972 00 83 0E 44 	lri  	ar3, #0x0E44
0974 02 BF 84 5D 	call 	$0x845D
0976 00 F8 0B AF 	sr   	$0x0BAF, ax0.l
0978 02 DF       	ret  	
0979 00 C0 0E 40 	lr   	ar0, $0x0E40
097B 00 81 0B 89 	lri  	ar1, #0x0B89
097D 00 C2 0E 08 	lr   	ar2, $0x0E08
097F 00 83 0E 44 	lri  	ar3, #0x0E44
0981 00 C4 0E 41 	lr   	ix0, $0x0E41
0983 00 C5 0E 09 	lr   	ix1, $0x0E09
0985 02 BF 82 82 	call 	$0x8282
0987 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
0989 00 FB 0B AC 	sr   	$0x0BAC, ax1.h
098B 00 C0 0E 40 	lr   	ar0, $0x0E40
098D 00 81 0B 8D 	lri  	ar1, #0x0B8D
098F 00 C2 0E 0B 	lr   	ar2, $0x0E0B
0991 00 83 0E 44 	lri  	ar3, #0x0E44
0993 00 C4 0E 41 	lr   	ix0, $0x0E41
0995 00 C5 0E 0C 	lr   	ix1, $0x0E0C
0997 02 BF 82 82 	call 	$0x8282
0999 00 F8 0B AA 	sr   	$0x0BAA, ax0.l
099B 00 FB 0B AD 	sr   	$0x0BAD, ax1.h
099D 00 C0 0E 43 	lr   	ar0, $0x0E43
099F 00 81 0B 97 	lri  	ar1, #0x0B97
09A1 00 C2 0E 0A 	lr   	ar2, $0x0E0A
09A3 00 83 0E 44 	lri  	ar3, #0x0E44
09A5 1C 80       	mrr  	ix0, ar0
09A6 00 C5 0E 0D 	lr   	ix1, $0x0E0D
09A8 02 BF 82 82 	call 	$0x8282
09AA 00 F8 0B AF 	sr   	$0x0BAF, ax0.l
09AC 00 FB 0B B0 	sr   	$0x0BB0, ax1.h
09AE 02 DF       	ret  	
09AF 00 C0 0E 40 	lr   	ar0, $0x0E40
09B1 00 81 0B 89 	lri  	ar1, #0x0B89
09B3 00 C2 0E 08 	lr   	ar2, $0x0E08
09B5 00 83 0E 44 	lri  	ar3, #0x0E44
09B7 00 C4 0E 41 	lr   	ix0, $0x0E41
09B9 00 C5 0E 09 	lr   	ix1, $0x0E09
09BB 02 BF 82 82 	call 	$0x8282
09BD 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
09BF 00 FB 0B AC 	sr   	$0x0BAC, ax1.h
09C1 00 C0 0E 40 	lr   	ar0, $0x0E40
09C3 00 81 0B 91 	lri  	ar1, #0x0B91
09C5 00 C2 0E 0E 	lr   	ar2, $0x0E0E
09C7 00 83 0E 44 	lri  	ar3, #0x0E44
09C9 00 C4 0E 41 	lr   	ix0, $0x0E41
09CB 00 C5 0E 0F 	lr   	ix1, $0x0E0F
09CD 02 BF 82 82 	call 	$0x8282
09CF 00 F8 0B AB 	sr   	$0x0BAB, ax0.l
09D1 00 FB 0B AE 	sr   	$0x0BAE, ax1.h
09D3 00 C0 0E 43 	lr   	ar0, $0x0E43
09D5 00 81 0B 95 	lri  	ar1, #0x0B95
09D7 00 C2 0E 10 	lr   	ar2, $0x0E10
09D9 00 83 0E 44 	lri  	ar3, #0x0E44
09DB 1C 80       	mrr  	ix0, ar0
09DC 00 C5 0E 0A 	lr   	ix1, $0x0E0A
09DE 02 BF 82 82 	call 	$0x8282
09E0 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
09E2 00 FB 0B AF 	sr   	$0x0BAF, ax1.h
09E4 02 DF       	ret  	
09E5 00 C0 0E 40 	lr   	ar0, $0x0E40
09E7 00 81 0B 89 	lri  	ar1, #0x0B89
09E9 00 C2 0E 08 	lr   	ar2, $0x0E08
09EB 00 83 0E 44 	lri  	ar3, #0x0E44
09ED 00 C4 0E 41 	lr   	ix0, $0x0E41
09EF 00 C5 0E 09 	lr   	ix1, $0x0E09
09F1 02 BF 82 82 	call 	$0x8282
09F3 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
09F5 00 FB 0B AC 	sr   	$0x0BAC, ax1.h
09F7 00 C0 0E 40 	lr   	ar0, $0x0E40
09F9 00 81 0B 8D 	lri  	ar1, #0x0B8D
09FB 00 C2 0E 0B 	lr   	ar2, $0x0E0B
09FD 00 83 0E 44 	lri  	ar3, #0x0E44
09FF 00 C0 0E 41 	lr   	ar0, $0x0E41
0A01 00 C5 0E 0C 	lr   	ix1, $0x0E0C
0A03 02 BF 82 82 	call 	$0x8282
0A05 00 F8 0B AA 	sr   	$0x0BAA, ax0.l
0A07 00 FB 0B AD 	sr   	$0x0BAD, ax1.h
0A09 00 C0 0E 40 	lr   	ar0, $0x0E40
0A0B 00 81 0B 91 	lri  	ar1, #0x0B91
0A0D 00 C2 0E 0E 	lr   	ar2, $0x0E0E
0A0F 00 83 0E 44 	lri  	ar3, #0x0E44
0A11 00 C4 0E 41 	lr   	ix0, $0x0E41
0A13 00 C5 0E 0F 	lr   	ix1, $0x0E0F
0A15 02 BF 82 82 	call 	$0x8282
0A17 00 F8 0B AB 	sr   	$0x0BAB, ax0.l
0A19 00 FB 0B AE 	sr   	$0x0BAE, ax1.h
0A1B 00 C0 0E 43 	lr   	ar0, $0x0E43
0A1D 00 81 0B 97 	lri  	ar1, #0x0B97
0A1F 00 C2 0E 0A 	lr   	ar2, $0x0E0A
0A21 00 83 0E 44 	lri  	ar3, #0x0E44
0A23 1C 80       	mrr  	ix0, ar0
0A24 00 C5 0E 0D 	lr   	ix1, $0x0E0D
0A26 02 BF 82 82 	call 	$0x8282
0A28 00 F8 0B AF 	sr   	$0x0BAF, ax0.l
0A2A 00 FB 0B B0 	sr   	$0x0BB0, ax1.h
0A2C 00 C0 0E 43 	lr   	ar0, $0x0E43
0A2E 00 81 0B 95 	lri  	ar1, #0x0B95
0A30 00 C2 0E 10 	lr   	ar2, $0x0E10
0A32 00 83 0E 44 	lri  	ar3, #0x0E44
0A34 02 BF 84 5D 	call 	$0x845D
0A36 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0A38 02 DF       	ret  	
0A39 00 C0 0E 40 	lr   	ar0, $0x0E40
0A3B 00 81 0B 89 	lri  	ar1, #0x0B89
0A3D 00 C2 0E 08 	lr   	ar2, $0x0E08
0A3F 1C 62       	mrr  	ar3, ar2
0A40 00 C4 0E 41 	lr   	ix0, $0x0E41
0A42 00 C5 0E 09 	lr   	ix1, $0x0E09
0A44 02 BF 80 E7 	call 	$0x80E7
0A46 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
0A48 00 FB 0B AC 	sr   	$0x0BAC, ax1.h
0A4A 00 C0 0E 43 	lr   	ar0, $0x0E43
0A4C 00 81 0B 91 	lri  	ar1, #0x0B91
0A4E 00 C2 0E 0E 	lr   	ar2, $0x0E0E
0A50 1C 62       	mrr  	ar3, ar2
0A51 1C 80       	mrr  	ix0, ar0
0A52 00 C5 0E 0F 	lr   	ix1, $0x0E0F
0A54 02 BF 80 E7 	call 	$0x80E7
0A56 00 F8 0B AB 	sr   	$0x0BAB, ax0.l
0A58 00 FB 0B AE 	sr   	$0x0BAE, ax1.h
0A5A 02 DF       	ret  	
0A5B 00 C0 0E 40 	lr   	ar0, $0x0E40
0A5D 00 81 0B 89 	lri  	ar1, #0x0B89
0A5F 00 C2 0E 08 	lr   	ar2, $0x0E08
0A61 1C 62       	mrr  	ar3, ar2
0A62 00 C4 0E 41 	lr   	ix0, $0x0E41
0A64 00 C5 0E 09 	lr   	ix1, $0x0E09
0A66 02 BF 80 E7 	call 	$0x80E7
0A68 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
0A6A 00 FB 0B AC 	sr   	$0x0BAC, ax1.h
0A6C 00 C0 0E 43 	lr   	ar0, $0x0E43
0A6E 00 81 0B 91 	lri  	ar1, #0x0B91
0A70 00 C2 0E 0E 	lr   	ar2, $0x0E0E
0A72 1C 62       	mrr  	ar3, ar2
0A73 1C 80       	mrr  	ix0, ar0
0A74 00 C5 0E 0F 	lr   	ix1, $0x0E0F
0A76 02 BF 80 E7 	call 	$0x80E7
0A78 00 F8 0B AB 	sr   	$0x0BAB, ax0.l
0A7A 00 FB 0B AE 	sr   	$0x0BAE, ax1.h
0A7C 00 C0 0E 40 	lr   	ar0, $0x0E40
0A7E 00 81 0B 8D 	lri  	ar1, #0x0B8D
0A80 00 C2 0E 0B 	lr   	ar2, $0x0E0B
0A82 1C 62       	mrr  	ar3, ar2
0A83 00 C4 0E 41 	lr   	ix0, $0x0E41
0A85 00 C5 0E 0C 	lr   	ix1, $0x0E0C
0A87 02 BF 80 E7 	call 	$0x80E7
0A89 00 F8 0B AA 	sr   	$0x0BAA, ax0.l
0A8B 00 FB 0B AD 	sr   	$0x0BAD, ax1.h
0A8D 00 C0 0E 43 	lr   	ar0, $0x0E43
0A8F 00 81 0B 99 	lri  	ar1, #0x0B99
0A91 00 C2 0E 0D 	lr   	ar2, $0x0E0D
0A93 1C 62       	mrr  	ar3, ar2
0A94 02 BF 81 F9 	call 	$0x81F9
0A96 00 F8 0B B0 	sr   	$0x0BB0, ax0.l
0A98 02 DF       	ret  	
0A99 00 C0 0E 40 	lr   	ar0, $0x0E40
0A9B 00 81 0B 89 	lri  	ar1, #0x0B89
0A9D 00 C2 0E 08 	lr   	ar2, $0x0E08
0A9F 00 83 0E 44 	lri  	ar3, #0x0E44
0AA1 00 C4 0E 41 	lr   	ix0, $0x0E41
0AA3 00 C5 0E 09 	lr   	ix1, $0x0E09
0AA5 02 BF 82 82 	call 	$0x8282
0AA7 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
0AA9 00 FB 0B AC 	sr   	$0x0BAC, ax1.h
0AAB 00 C0 0E 43 	lr   	ar0, $0x0E43
0AAD 00 81 0B 91 	lri  	ar1, #0x0B91
0AAF 00 C2 0E 0E 	lr   	ar2, $0x0E0E
0AB1 00 83 0E 44 	lri  	ar3, #0x0E44
0AB3 1C 80       	mrr  	ix0, ar0
0AB4 00 C5 0E 0F 	lr   	ix1, $0x0E0F
0AB6 02 BF 82 82 	call 	$0x8282
0AB8 00 F8 0B AB 	sr   	$0x0BAB, ax0.l
0ABA 00 FB 0B AE 	sr   	$0x0BAE, ax1.h
0ABC 02 DF       	ret  	
0ABD 00 C0 0E 40 	lr   	ar0, $0x0E40
0ABF 00 81 0B 89 	lri  	ar1, #0x0B89
0AC1 00 C2 0E 08 	lr   	ar2, $0x0E08
0AC3 00 83 0E 44 	lri  	ar3, #0x0E44
0AC5 00 C4 0E 41 	lr   	ix0, $0x0E41
0AC7 00 C5 0E 09 	lr   	ix1, $0x0E09
0AC9 02 BF 82 82 	call 	$0x8282
0ACB 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
0ACD 00 FB 0B AC 	sr   	$0x0BAC, ax1.h
0ACF 00 C0 0E 43 	lr   	ar0, $0x0E43
0AD1 00 81 0B 91 	lri  	ar1, #0x0B91
0AD3 00 C2 0E 0E 	lr   	ar2, $0x0E0E
0AD5 00 83 0E 44 	lri  	ar3, #0x0E44
0AD7 1C 80       	mrr  	ix0, ar0
0AD8 00 C5 0E 0F 	lr   	ix1, $0x0E0F
0ADA 02 BF 82 82 	call 	$0x8282
0ADC 00 F8 0B AB 	sr   	$0x0BAB, ax0.l
0ADE 00 FB 0B AE 	sr   	$0x0BAE, ax1.h
0AE0 00 C0 0E 40 	lr   	ar0, $0x0E40
0AE2 00 81 0B 8D 	lri  	ar1, #0x0B8D
0AE4 00 C2 0E 0B 	lr   	ar2, $0x0E0B
0AE6 00 83 0E 44 	lri  	ar3, #0x0E44
0AE8 00 C4 0E 41 	lr   	ix0, $0x0E41
0AEA 00 C5 0E 0C 	lr   	ix1, $0x0E0C
0AEC 02 BF 82 82 	call 	$0x8282
0AEE 00 F8 0B AA 	sr   	$0x0BAA, ax0.l
0AF0 00 FB 0B AD 	sr   	$0x0BAD, ax1.h
0AF2 00 C0 0E 43 	lr   	ar0, $0x0E43
0AF4 00 81 0B 99 	lri  	ar1, #0x0B99
0AF6 00 C2 0E 0D 	lr   	ar2, $0x0E0D
0AF8 00 83 0E 44 	lri  	ar3, #0x0E44
0AFA 02 BF 84 5D 	call 	$0x845D
0AFC 00 F8 0B B0 	sr   	$0x0BB0, ax0.l
0AFE 02 DF       	ret  	
```

```
0AFF 0082 		Command 0
0B00 013E 		Command 1
0B01 01BC 		Command 2
0B02 0248 		Command 3
0B03 0413 		Command 4
0B04 0427 		Command 5
0B05 0165 		Command 6
0B06 0574 		Command 7
0B07 0B37 		Command 8
0B08 015F 		Command 9
0B09 0478 		Command 0xA    	Bogus
0B0A 0474  		Command 0xB  	Bogus
0B0B 0476 		Command 0xC 	Bogus
0B0C 01A9 		Command 0xD
0B0D 043B 		Command 0xE
0B0E 047A 		Command 0xF
0B0F 0BB1 		Command 0x10
0B10 0175 		Command 0x11
```

```
0B11 0768
0B12 077A
0B13 079D
0B14 07C0
0B15 07F4
0B16 0811
0B17 0844
0B18 0877
0B19 08C6
0B1A 08D9
0B1B 08FE
0B1C 0923
0B1D 095A
0B1E 0979
0B1F 09AF
0B20 09E5
0B21 0A39
0B22 0A5B
0B23 0768
0B24 0768
0B25 0768
0B26 0768
0B27 0768
0B28 0768
0B29 0A99
0B2A 0ABD
0B2B 0768
0B2C 0768
0B2D 0768
0B2E 0768
0B2F 0768
0B30 0768
```

## Sample Rate Converter (SRC) Switch

```
0B31 05A8 				// 4-tap
0B32 065D 				// linear
0B33 0707 				// none
```

## Coef Table Switch

```
0B34 1000 				// 8KHz low pass response
0B35 1200 				// 12.8KHz N64 type response
0B36 1400 				// 16KHz response
```

## Command 8

```
0B37 8E 00       	clr40	                	     	
0B38 81 00       	clr  	ac0             	     	
0B39 89 70       	clr  	ac1             	l    	ac0.m, @ar0
0B3A 19 1C       	lrri 	ac0.l, @ar0
0B3B 2E CE       	srs  	$(DSMAH), ac0.m
0B3C 2C CF       	srs  	$(DSMAL), ac0.l
0B3D 16 CD 0E 80 	si   	$(DSPA), #0x0E80
0B3F 16 C9 00 00 	si   	$(DSCR), #0x0000
0B41 16 CB 01 00 	si   	$(DSBL), #0x0100
0B43 1F 7E       	mrr  	ax1.h, ac0.m
0B44 1F 3C       	mrr  	ax1.l, ac0.l
0B45 81 00       	clr  	ac0             	     	
0B46 26 C9       	lrs  	ac0.m, $(DSCR)
0B47 02 A0 00 04 	tclr 	ac0.m, #0x0004
0B49 02 9C 0B 46 	jnok 	$0x0B46
0B4B 19 1E       	lrri 	ac0.m, @ar0
0B4C 19 1C       	lrri 	ac0.l, @ar0
0B4D 2E CE       	srs  	$(DSMAH), ac0.m
0B4E 2C CF       	srs  	$(DSMAL), ac0.l
0B4F 16 CD 02 80 	si   	$(DSPA), #0x0280
0B51 16 C9 00 00 	si   	$(DSCR), #0x0000
0B53 16 CB 02 80 	si   	$(DSBL), #0x0280
0B55 1C 80       	mrr  	ix0, ar0
0B56 00 80 02 80 	lri  	ar0, #0x0280
0B58 00 C1 0E 1B 	lr   	ar1, $0x0E1B
0B5A 00 85 00 00 	lri  	ix1, #0x0000
0B5C 00 89 00 7F 	lri  	lm1, #0x007F
0B5E 00 82 0F 00 	lri  	ar2, #0x0F00
0B60 00 83 16 B4 	lri  	ar3, #0x16B4
0B62 1C E3       	mrr  	ix3, ar3
0B63 81 00       	clr  	ac0             	     	
0B64 26 C9       	lrs  	ac0.m, $(DSCR)
0B65 02 A0 00 04 	tclr 	ac0.m, #0x0004
0B67 02 9C 0B 64 	jnok 	$0x0B64
0B69 8F 00       	set40	                	     	
0B6A 8A 78       	m2   	                	l    	ac1.m, @ar0
0B6B 8C 68       	clr15	                	l    	ac1.l, @ar0
0B6C F1 00       	lsl16	ac1             	     	
0B6D 1A 3F       	srr  	@ar1, ac1.m
0B6E 84 E3       	clrp 	                	ldax 	ax0, @ar1
0B6F 10 7E       	loopi	#0x7E
0B70 F2 E3       	madd 	ax0.l, ax0.h    	ldax 	ax0, @ar1
0B71 F2 E7       	madd 	ax0.l, ax0.h    	ldaxn	ax0, @ar1
0B72 F2 78       	madd 	ax0.l, ax0.h    	l    	ac1.m, @ar0
0B73 6E 68       	movp 	ac0             	l    	ac1.l, @ar0
0B74 F1 32       	lsl16	ac1             	s    	@ar2, ac0.m
0B75 1A 3F       	srr  	@ar1, ac1.m
0B76 11 9E 0B 80 	bloopi	#0x9E, $0x0B80
0B78 1C 67       	mrr  	ar3, ix3
0B79 84 E3       	clrp 	                	ldax 	ax0, @ar1
0B7A 10 7E       	loopi	#0x7E
0B7B F2 E3       	madd 	ax0.l, ax0.h    	ldax 	ax0, @ar1
0B7C F2 E7       	madd 	ax0.l, ax0.h    	ldaxn	ax0, @ar1
0B7D F2 78       	madd 	ax0.l, ax0.h    	l    	ac1.m, @ar0
0B7E 6E 68       	movp 	ac0             	l    	ac1.l, @ar0
0B7F F1 32       	lsl16	ac1             	s    	@ar2, ac0.m
0B80 1A 3F       	srr  	@ar1, ac1.m
0B81 1C 67       	mrr  	ar3, ix3
0B82 84 E3       	clrp 	                	ldax 	ax0, @ar1
0B83 10 7E       	loopi	#0x7E
0B84 F2 E3       	madd 	ax0.l, ax0.h    	ldax 	ax0, @ar1
0B85 F2 E7       	madd 	ax0.l, ax0.h    	ldaxn	ax0, @ar1
0B86 F2 00       	madd 	ax0.l, ax0.h    	     	
0B87 6E 00       	movp 	ac0             	     	
0B88 1B 5E       	srri 	@ar2, ac0.m
0B89 00 E1 0E 1B 	sr   	$0x0E1B, ar1
0B8B 00 80 02 80 	lri  	ar0, #0x0280
0B8D 00 83 0F 00 	lri  	ar3, #0x0F00
0B8F 00 81 00 00 	lri  	ar1, #0x0000
0B91 00 82 01 40 	lri  	ar2, #0x0140
0B93 00 89 FF FF 	lri  	lm1, #0xFFFF
0B95 89 00       	clr  	ac1             	     	
0B96 81 00       	clr  	ac0             	     	
0B97 8F 00       	set40	                	     	
0B98 11 A0 0B A0 	bloopi	#0xA0, $0x0BA0
0B9A 19 7F       	lrri 	ac1.m, @ar3
0B9B 99 30       	asr16	ac1             	s    	@ar0, ac0.m
0B9C 1B 1E       	srri 	@ar0, ac0.m
0B9D 1B 3F       	srri 	@ar1, ac1.m
0B9E 7D 29       	neg  	ac1             	s    	@ar1, ac1.l
0B9F 1B 5F       	srri 	@ar2, ac1.m
0BA0 1B 5D       	srri 	@ar2, ac1.l
0BA1 8E 00       	clr40	                	     	
0BA2 1F DB       	mrr  	ac0.m, ax1.h
0BA3 1F 99       	mrr  	ac0.l, ax1.l
0BA4 2E CE       	srs  	$(DSMAH), ac0.m
0BA5 2C CF       	srs  	$(DSMAL), ac0.l
0BA6 16 CD 0E 80 	si   	$(DSPA), #0x0E80
0BA8 16 C9 00 01 	si   	$(DSCR), #0x0001
0BAA 16 CB 01 00 	si   	$(DSBL), #0x0100
0BAC 02 BF 05 5C 	call 	$0x055C
0BAE 1C 04       	mrr  	ar0, ix0
0BAF 02 9F 00 68 	j    	$0x0068
```

## Command 0x10

```
0BB1 8E 00       	clr40	                	     	
0BB2 81 00       	clr  	ac0             	     	
0BB3 89 70       	clr  	ac1             	l    	ac0.m, @ar0
0BB4 19 1C       	lrri 	ac0.l, @ar0
0BB5 2E CE       	srs  	$(DSMAH), ac0.m
0BB6 2C CF       	srs  	$(DSMAL), ac0.l
0BB7 16 CD 07 C0 	si   	$(DSPA), #0x07C0
0BB9 16 C9 00 01 	si   	$(DSCR), #0x0001
0BBB 16 CB 05 00 	si   	$(DSBL), #0x0500
0BBD 02 BF 05 5C 	call 	$0x055C
0BBF 81 00       	clr  	ac0             	     	
0BC0 89 70       	clr  	ac1             	l    	ac0.m, @ar0
0BC1 19 1C       	lrri 	ac0.l, @ar0
0BC2 2E CE       	srs  	$(DSMAH), ac0.m
0BC3 2C CF       	srs  	$(DSMAL), ac0.l
0BC4 16 CD 07 C0 	si   	$(DSPA), #0x07C0
0BC6 16 C9 00 00 	si   	$(DSCR), #0x0000
0BC8 89 00       	clr  	ac1             	     	
0BC9 0D 20       	lris 	ac1.l, 32
0BCA 2D CB       	srs  	$(DSBL), ac1.l
0BCB 4C 00       	add  	ac0, ac1        	     	
0BCC 1C 80       	mrr  	ix0, ar0
0BCD 00 80 07 C0 	lri  	ar0, #0x07C0
0BCF 00 83 00 00 	lri  	ar3, #0x0000
0BD1 1C 43       	mrr  	ar2, ar3
0BD2 0A 00       	lris 	ax0.h, 0
0BD3 27 C9       	lrs  	ac1.m, $(DSCR)
0BD4 03 A0 00 04 	tclr 	ac1.m, #0x0004
0BD6 02 9C 0B D3 	jnok 	$0x0BD3
0BD8 2E CE       	srs  	$(DSMAH), ac0.m
0BD9 2C CF       	srs  	$(DSMAL), ac0.l
0BDA 16 CD 07 D0 	si   	$(DSPA), #0x07D0
0BDC 16 C9 00 00 	si   	$(DSCR), #0x0000
0BDE 16 CB 04 E0 	si   	$(DSBL), #0x04E0
0BE0 8F 00       	set40	                	     	
0BE1 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
0BE2 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
0BE3 6A 00       	movax	ac0, ax1        	     	
0BE4 48 00       	addax	ac0, ax0        	     	
0BE5 11 4F 0B EE 	bloopi	#0x4F, $0x0BEE
0BE7 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
0BE8 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
0BE9 6B 32       	movax	ac1, ax1        	s    	@ar2, ac0.m
0BEA 49 22       	addax	ac1, ax0        	s    	@ar2, ac0.l
0BEB 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
0BEC 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
0BED 6A 3A       	movax	ac0, ax1        	s    	@ar2, ac1.m
0BEE 48 2A       	addax	ac0, ax0        	s    	@ar2, ac1.l
0BEF 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
0BF0 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
0BF1 6B 32       	movax	ac1, ax1        	s    	@ar2, ac0.m
0BF2 49 22       	addax	ac1, ax0        	s    	@ar2, ac0.l
0BF3 1B 5F       	srri 	@ar2, ac1.m
0BF4 1B 5D       	srri 	@ar2, ac1.l
0BF5 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
0BF6 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
0BF7 68 00       	movax	ac0, ax0        	     	
0BF8 7C 00       	neg  	ac0             	     	
0BF9 4A 00       	addax	ac0, ax1        	     	
0BFA 11 4F 0C 05 	bloopi	#0x4F, $0x0C05
0BFC 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
0BFD 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
0BFE 69 32       	movax	ac1, ax0        	s    	@ar2, ac0.m
0BFF 7D 00       	neg  	ac1             	     	
0C00 4B 22       	addax	ac1, ax1        	s    	@ar2, ac0.l
0C01 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
0C02 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
0C03 68 3A       	movax	ac0, ax0        	s    	@ar2, ac1.m
0C04 7C 00       	neg  	ac0             	     	
0C05 4A 2A       	addax	ac0, ax1        	s    	@ar2, ac1.l
0C06 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
0C07 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
0C08 69 32       	movax	ac1, ax0        	s    	@ar2, ac0.m
0C09 7D 00       	neg  	ac1             	     	
0C0A 4B 22       	addax	ac1, ax1        	s    	@ar2, ac0.l
0C0B 1B 5F       	srri 	@ar2, ac1.m
0C0C 1B 5D       	srri 	@ar2, ac1.l
0C0D 1C 04       	mrr  	ar0, ix0
0C0E 02 9F 00 68 	j    	$0x0068
0C10 8E 00       	clr40	                	     	
0C11 16 FC EC C0 	si   	$(DMBH), #0xECC0
0C13 1F CC       	mrr  	ac0.m, st0
0C14 1D 9E       	mrr  	st0, ac0.m
0C15 2E FD       	srs  	$(DMBL), ac0.m
0C16 26 FC       	lrs  	ac0.m, $(DMBH)
0C17 02 A0 80 00 	tclr 	ac0.m, #0x8000
0C19 02 9C 0C 16 	jnok 	$0x0C16
0C1B 00 00       	nop  	
0C1C 00 00       	nop  	
0C1D 00 00       	nop  	
0C1E 02 FF       	rti  	
0C1F 8E 00       	clr40	                	     	
0C20 00 F0 0E 17 	sr   	$0x0E17, ac0.h
0C22 00 FE 0E 18 	sr   	$0x0E18, ac0.m
0C24 00 FC 0E 19 	sr   	$0x0E19, ac0.l
0C26 1F CC       	mrr  	ac0.m, st0
0C27 1D 9E       	mrr  	st0, ac0.m
0C28 16 FC FE ED 	si   	$(DMBH), #0xFEED
0C2A 2E FD       	srs  	$(DMBL), ac0.m
0C2B 26 FC       	lrs  	ac0.m, $(DMBH)
0C2C 02 A0 80 00 	tclr 	ac0.m, #0x8000
0C2E 02 9C 0C 2B 	jnok 	$0x0C2B
0C30 00 D0 0E 17 	lr   	ac0.h, $0x0E17
0C32 00 DE 0E 18 	lr   	ac0.m, $0x0E18
0C34 00 DC 0E 19 	lr   	ac0.l, $0x0E19
0C36 00 00       	nop  	
0C37 00 00       	nop  	
0C38 00 00       	nop  	
0C39 00 00       	nop  	
0C3A 02 FF       	rti  	
0C3B 8E 00       	clr40	                	     	
0C3C 1D BC       	mrr  	st1, ac0.l
0C3D 1D BE       	mrr  	st1, ac0.m
0C3E 81 00       	clr  	ac0             	     	
0C3F 00 DE 0B B7 	lr   	ac0.m, $0x0BB7
0C41 06 01       	cmpis	ac0.m, 1
0C42 02 95 0C 47 	jeq  	$0x0C47
0C44 0E 00       	lris 	ac0.m, 0
0C45 00 FE 0B 87 	sr   	$0x0B87, ac0.m
0C47 1F CD       	mrr  	ac0.m, st1
0C48 1F 8D       	mrr  	ac0.l, st1
0C49 02 FF       	rti  	
0C4A 00 00       	nop  	
0C4B 00 00       	nop  	
0C4C 00 00       	nop  	
0C4D 00 00       	nop  	
0C4E 00 00       	nop  	
0C4F 02 FF       	rti  	
0C50 8E 00       	clr40	                	     	
0C51 1D BC       	mrr  	st1, ac0.l
0C52 1D BE       	mrr  	st1, ac0.m
0C53 81 00       	clr  	ac0             	     	
0C54 00 DE 0B B7 	lr   	ac0.m, $0x0BB7
0C56 06 01       	cmpis	ac0.m, 1
0C57 02 95 0C 5F 	jeq  	$0x0C5F
0C59 0E 00       	lris 	ac0.m, 0
0C5A 00 FE 0B 87 	sr   	$0x0B87, ac0.m
0C5C 1F CD       	mrr  	ac0.m, st1
0C5D 1F 8D       	mrr  	ac0.l, st1
0C5E 02 FF       	rti  	
0C5F 81 00       	clr  	ac0             	     	
0C60 00 DE 0B 88 	lr   	ac0.m, $0x0B88
0C62 06 01       	cmpis	ac0.m, 1
0C63 02 95 0C 71 	jeq  	$0x0C71
0C65 00 DE 0B DA 	lr   	ac0.m, $0x0BDA
0C67 2E DA       	srs  	$(ACPDS), ac0.m
0C68 00 DE 0B DB 	lr   	ac0.m, $0x0BDB
0C6A 2E DB       	srs  	$(ACYN1), ac0.m
0C6B 00 DE 0B DC 	lr   	ac0.m, $0x0BDC
0C6D 2E DC       	srs  	$(ACYN2), ac0.m
0C6E 1F CD       	mrr  	ac0.m, st1
0C6F 1F 8D       	mrr  	ac0.l, st1
0C70 02 FF       	rti  	
0C71 00 DE 0B DA 	lr   	ac0.m, $0x0BDA
0C73 2E DA       	srs  	$(ACPDS), ac0.m
0C74 26 DB       	lrs  	ac0.m, $(ACYN1)
0C75 2E DB       	srs  	$(ACYN1), ac0.m
0C76 26 DC       	lrs  	ac0.m, $(ACYN2)
0C77 2E DC       	srs  	$(ACYN2), ac0.m
0C78 81 00       	clr  	ac0             	     	
0C79 00 DC 0B DD 	lr   	ac0.l, $0x0BDD
0C7B 76 00       	inc  	ac0             	     	
0C7C 00 FC 0B DD 	sr   	$0x0BDD, ac0.l
0C7E 81 00       	clr  	ac0             	     	
0C7F 1F CD       	mrr  	ac0.m, st1
0C80 1F 8D       	mrr  	ac0.l, st1
0C81 02 FF       	rti  	
0C82 00 00       	nop  	
0C83 00 00       	nop  	
0C84 00 00       	nop  	
0C85 00 00       	nop  	
0C86 00 00       	nop  	
0C87 02 FF       	rti  	
0C88 00 00       	nop  	
0C89 00 00       	nop  	
0C8A 00 00       	nop  	
0C8B 00 00       	nop  	
0C8C 02 FF       	rti  	
0C8D 0C 9F       	lris 	ac0.l, -97
0C8E 0C A2       	lris 	ac0.l, -94
0C8F 0C DA       	lris 	ac0.l, -38
0C90 0C DD       	lris 	ac0.l, -35
0C91 8E 00       	clr40	                	     	
0C92 81 00       	clr  	ac0             	     	
0C93 89 00       	clr  	ac1             	     	
0C94 02 BF 0C E0 	call 	$0x0CE0
0C96 27 FF       	lrs  	ac1.m, $(CMBL)
0C97 00 9E 0C 8D 	lri  	ac0.m, #0x0C8D
0C99 4C 00       	add  	ac0, ac1        	     	
0C9A 1C 7E       	mrr  	ar3, ac0.m
0C9B 03 13       	ilrr 	ac1.m, @ar3
0C9C 1C 7F       	mrr  	ar3, ac1.m
0C9D 17 6F       	jmpr 	ar3
0C9E 00 21       	halt 	
0C9F 02 9F 00 30 	j    	$0x0030
0CA1 00 21       	halt 	
0CA2 81 00       	clr  	ac0             	     	
0CA3 89 00       	clr  	ac1             	     	
0CA4 02 BF 0C E0 	call 	$0x0CE0
0CA6 24 FF       	lrs  	ac0.l, $(CMBL)
0CA7 02 BF 0C E6 	call 	$0x0CE6
0CA9 25 FF       	lrs  	ac1.l, $(CMBL)
0CAA 02 BF 0C E6 	call 	$0x0CE6
0CAC 27 FF       	lrs  	ac1.m, $(CMBL)
0CAD 2E CE       	srs  	$(DSMAH), ac0.m
0CAE 2C CF       	srs  	$(DSMAL), ac0.l
0CAF 16 C9 00 01 	si   	$(DSCR), #0x0001
0CB1 2F CD       	srs  	$(DSPA), ac1.m
0CB2 2D CB       	srs  	$(DSBL), ac1.l
0CB3 81 00       	clr  	ac0             	     	
0CB4 89 00       	clr  	ac1             	     	
0CB5 02 BF 0C E0 	call 	$0x0CE0
0CB7 24 FF       	lrs  	ac0.l, $(CMBL)
0CB8 1C 9E       	mrr  	ix0, ac0.m
0CB9 1C BC       	mrr  	ix1, ac0.l
0CBA 02 BF 0C E6 	call 	$0x0CE6
0CBC 25 FF       	lrs  	ac1.l, $(CMBL)
0CBD 02 BF 0C E6 	call 	$0x0CE6
0CBF 27 FF       	lrs  	ac1.m, $(CMBL)
0CC0 1C DF       	mrr  	ix2, ac1.m
0CC1 1C FD       	mrr  	ix3, ac1.l
0CC2 81 00       	clr  	ac0             	     	
0CC3 02 BF 0C E0 	call 	$0x0CE0
0CC5 26 FF       	lrs  	ac0.m, $(CMBL)
0CC6 1C 1E       	mrr  	ar0, ac0.m
0CC7 89 00       	clr  	ac1             	     	
0CC8 02 BF 0C E6 	call 	$0x0CE6
0CCA 20 FF       	lrs  	ax0.l, $(CMBL)
0CCB 1F 5F       	mrr  	ax0.h, ac1.m
0CCC 02 BF 0C E0 	call 	$0x0CE0
0CCE 21 FF       	lrs  	ax1.l, $(CMBL)
0CCF 02 BF 0C E0 	call 	$0x0CE0
0CD1 23 FF       	lrs  	ax1.h, $(CMBL)
0CD2 26 C9       	lrs  	ac0.m, $(DSCR)
0CD3 02 A0 00 04 	tclr 	ac0.m, #0x0004
0CD5 02 9C 0C D2 	jnok 	$0x0CD2
0CD7 02 9F 80 B5 	j    	$0x80B5
0CD9 00 21       	halt 	
0CDA 02 9F 80 00 	j    	$0x8000
0CDC 00 21       	halt 	
0CDD 02 9F 00 45 	j    	$0x0045
0CDF 00 21       	halt 	
0CE0 26 FE       	lrs  	ac0.m, $(CMBH)
0CE1 02 C0 80 00 	tset 	ac0.m, #0x8000
0CE3 02 9C 0C E0 	jnok 	$0x0CE0
0CE5 02 DF       	ret  	
0CE6 27 FE       	lrs  	ac1.m, $(CMBH)
0CE7 03 C0 80 00 	tset 	ac1.m, #0x8000
0CE9 02 9C 0C E6 	jnok 	$0x0CE6
0CEB 02 DF       	ret  	
0CEC 00 00       	nop  	
0CED 00 00       	nop  	
0CEE 00 00       	nop  	
0CEF 00 00       	nop  	
```
