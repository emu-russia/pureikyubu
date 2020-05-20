# Disassembled AX Ucode from Bust-a-Move 3000

(fixed version)

I will consider only interesting places, comparing with work in the emulator.
The main reversal was a bitch because Duddie had ax0.h and ax1.l mixed up.

```
0000 00 00       	nop  	
0001 00 00       	nop  	
0002 02 9F 0E B3 	j    	$0x0EB3
0004 02 9F 0E C2 	j    	$0x0EC2
0006 02 9F 0E DE 	j    	$0x0EDE
0008 02 9F 0E ED 	j    	$0x0EED
000A 02 9F 0E F3 	j    	$0x0EF3
000C 02 9F 0F 25 	j    	$0x0F25
000E 02 9F 0F 2B 	j    	$0x0F2B
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
0064 02 BF 06 94 	call 	$0x0694
0066 00 80 0C 00 	lri  	ar0, #0x0C00
0068 8E 00       	clr40	                	     	
0069 81 00       	clr  	ac0             	     	
006A 89 70       	clr  	ac1             	l    	ac0.m, @ar0
006B B1 00       	tst  	ac0             	     	
006C 02 91 00 7E 	jl   	$0x007E
006E 0A 13       	lris 	ax0.h, 19
006F C1 00       	cmpar	ac0.m, ax0.h    	     	
0070 02 92 00 7E 	jg   	$0x007E
0072 00 9F 0C C8 	lri  	ac1.m, #0x0CC8
0074 4C 00       	add  	ac0, ac1        	     	
0075 1C 7E       	mrr  	ar3, ac0.m
0076 02 13       	ilrr 	ac0.m, @ar3
0077 1C 7E       	mrr  	ar3, ac0.m
0078 17 6F       	jmpr 	ar3
0079 16 FC FB AD 	si   	$(DMBH), #0xFBAD
007B 16 FD 80 80 	si   	$(DMBL), #0x8080
007D 00 21       	halt 	
007E 16 FC BA AD 	si   	$(DMBH), #0xBAAD
0080 2E FD       	srs  	$(DMBL), ac0.m
0081 00 21       	halt 	
```

## Command 0x12

```
[00 12] [80 00] [00 0A] [80 0D F9 80]

800DF980  7F A1 7F 43 7E E6 7E 88  7E 2B 7D CE 7D 72 7D 16  	
800DF990  7C BA 7C 5E 7C 02 7B A7  7B 4C 7A F1 7A 97 7A 3D
800DF9A0  79 E3 79 89 79 30 78 D6  78 7E 78 25 77 CD 77 74
800DF9B0  77 1C 76 C5 76 6D 76 16  75 BF 75 69 75 12 74 BC
800DF9C0  74 66 74 11 73 BB 73 66  73 11 72 BD 72 68 72 14
800DF9D0  71 C0 71 6C 71 19 70 C6  70 73 70 20 6F CD 6F 7B
800DF9E0  6F 29 6E D7 6E 86 6E 35  6D E3 6D 93 6D 42 6C F2
800DF9F0  6C A1 6C 52 6C 02 6B B2  6B 63 6B 14 6A C5 6A 77
800DFA00  6A 28 69 DA 69 8C 69 3F  68 F1 68 A4 68 57 68 0A
800DFA10  67 BE 67 71 67 25 66 D9  66 8E 66 42 65 F7 65 AC
800DFA20  65 61 65 17 64 CC 64 82  64 38 63 EE 63 A5 63 5C
800DFA30  63 12 62 CA 62 81 62 38  61 F0 61 A8 61 60 61 19
800DFA40  60 D1 60 8A 60 43 5F FC  5F B5 5F 6F 5F 29 5E E3
800DFA50  5E 9D 5E 57 5E 12 5D CD  5D 88 5D 43 5C FE 5C BA

...

```

```
0082 8D 00       	set15	                	     	
0083 8F 00       	set40	                	     	
0084 8A 00       	m2   	                	     	
0085 89 00       	clr  	ac1             	     	
0086 81 68       	clr  	ac0             	l    	ac1.l, @ar0
0087 00 98 00 00 	lri  	ax0.l, #0x0000
0089 00 99 00 01 	lri  	ax1.l, #0x0001
008B 00 81 00 00 	lri  	ar1, #0x0000
008D 00 00       	nop  	
008E 19 3E       	lrri 	ac0.m, @ar1
008F 19 3C       	lrri 	ac0.l, @ar1
0090 11 A0 00 9B 	bloopi	#0xA0, $0x009B
	0092 A1 00       	abs  	ac0             	     	
	0093 82 71       	cmp  	                	l    	ac0.m, @ar1
	0094 02 77       	ifc  	
	0095 1F 19       	mrr  	ax0.l, ax1.l
	0096 19 3C       	lrri 	ac0.l, @ar1
	0097 A1 00       	abs  	ac0             	     	
	0098 82 71       	cmp  	                	l    	ac0.m, @ar1
	0099 02 77       	ifc  	
	009A 1F 19       	mrr  	ax0.l, ax1.l
	009B 19 3C       	lrri 	ac0.l, @ar1
009C 1F D8       	mrr  	ac0.m, ax0.l
009D B1 00       	tst  	ac0             	     	
009E 02 94 00 CD 	jne  	$0x00CD
00A0 00 DE 0E 44 	lr   	ac0.m, $0x0E44
00A2 00 00       	nop  	
00A3 B1 00       	tst  	ac0             	     	
00A4 02 94 00 AD 	jne  	$0x00AD
00A6 19 1C       	lrri 	ac0.l, @ar0
00A7 19 1C       	lrri 	ac0.l, @ar0
00A8 19 1C       	lrri 	ac0.l, @ar0
00A9 00 E0 0E 45 	sr   	$0x0E45, ar0
00AB 02 9F 01 18 	j    	$0x0118

00AD 8B 00       	m0   	                	     	
00AE 7A 00       	dec  	ac0             	     	
00AF 00 FE 0E 44 	sr   	$0x0E44, ac0.m
00B1 84 00       	clrp 	                	     	
00B2 00 99 01 40 	lri  	ax1.l, #0x0140
00B4 1F 1E       	mrr  	ax0.l, ac0.m
00B5 A0 00       	mulx 	ax0.l, ax1.l    	     	
00B6 19 1E       	lrri 	ac0.m, @ar0
00B7 19 1E       	lrri 	ac0.m, @ar0
00B8 19 1C       	lrri 	ac0.l, @ar0
00B9 00 E0 0E 45 	sr   	$0x0E45, ar0
00BB 00 9A 00 00 	lri  	ax0.h, #0x0000
00BD 00 98 0D C0 	lri  	ax0.l, #0x0DC0
00BF 4E 00       	addp 	ac0             	     	
00C0 48 00       	addax	ac0, ax0        	     	
00C1 2E CE       	srs  	$(DSMAH), ac0.m
00C2 2C CF       	srs  	$(DSMAL), ac0.l
00C3 00 9E 0E 48 	lri  	ac0.m, #0x0E48 			// Temp
00C5 2E CD       	srs  	$(DSPA), ac0.m
00C6 0E 00       	lris 	ac0.m, 0
00C7 2E C9       	srs  	$(DSCR), ac0.m 			// MMEM -> DRAM
00C8 00 9E 01 40 	lri  	ac0.m, #0x0140
00CA 2E CB       	srs  	$(DSBL), ac0.m 			// 0x140
00CB 02 9F 00 E7 	j    	$0x00E7

00CD 8B 00       	m0   	                	     	
00CE 00 D8 0E 44 	lr   	ax0.l, $0x0E44
00D0 00 99 01 40 	lri  	ax1.l, #0x0140
00D2 00 00       	nop  	
00D3 A0 00       	mulx 	ax0.l, ax1.l    	     	
00D4 19 1E       	lrri 	ac0.m, @ar0
00D5 00 00       	nop  	
00D6 00 FE 0E 44 	sr   	$0x0E44, ac0.m
00D8 19 1E       	lrri 	ac0.m, @ar0
00D9 19 1C       	lrri 	ac0.l, @ar0
00DA 00 E0 0E 45 	sr   	$0x0E45, ar0
00DC 4E 00       	addp 	ac0             	     	
00DD 2E CE       	srs  	$(DSMAH), ac0.m
00DE 2C CF       	srs  	$(DSMAL), ac0.l
00DF 00 9E 0E 48 	lri  	ac0.m, #0x0E48
00E1 2E CD       	srs  	$(DSPA), ac0.m 			// Temp
00E2 0E 00       	lris 	ac0.m, 0
00E3 2E C9       	srs  	$(DSCR), ac0.m 			// MMEM -> DRAM
00E4 00 9E 01 40 	lri  	ac0.m, #0x0140
00E6 2E CB       	srs  	$(DSBL), ac0.m 			// 0x140

00E7 02 BF 06 94 	call 	$0x0694 										// WaitDspDma
00E9 8A 48       	m2   	                	l    	ax1.l, @ar0

00EA 00 83 0E 48 	lri  	ar3, #0x0E48
00EC 00 80 00 00 	lri  	ar0, #0x0000
00EE 00 81 00 00 	lri  	ar1, #0x0000
00F0 19 79       	lrri 	ax1.l, @ar3
00F1 19 3A       	lrri 	ax0.h, @ar1
00F2 B0 41       	mulx 	ax0.h, ax1.l    	l    	ax0.l, @ar1
00F3 A6 4B       	mulxmv	ax0.l, ax1.l, ac0	l    	ax1.l, @ar3
00F4 F0 51       	lsl16	ac0             	l    	ax0.h, @ar1
00F5 B4 41       	mulxac	ax0.h, ax1.l, ac0	l    	ax0.l, @ar1
00F6 91 00       	asr16	ac0             	     	
00F7 11 50 01 00 	bloopi	#0x50, $0x0100
	00F9 A7 92       	mulxmv	ax0.l, ax1.l, ac1	sl   	ac0.m, ax1.l
	00FA F1 51       	lsl16	ac1             	l    	ax0.h, @ar1
	00FB B5 20       	mulxac	ax0.h, ax1.l, ac1	s    	@ar0, ac0.l
	00FC 99 41       	asr16	ac1             	l    	ax0.l, @ar1
	00FD A6 93       	mulxmv	ax0.l, ax1.l, ac0	sl   	ac1.m, ax1.l
	00FE F0 51       	lsl16	ac0             	l    	ax0.h, @ar1
	00FF B4 28       	mulxac	ax0.h, ax1.l, ac0	s    	@ar0, ac1.l
	0100 91 41       	asr16	ac0             	l    	ax0.l, @ar1
	
0101 00 83 0E 48 	lri  	ar3, #0x0E48
0103 00 80 01 40 	lri  	ar0, #0x0140
0105 00 81 01 40 	lri  	ar1, #0x0140
0107 19 79       	lrri 	ax1.l, @ar3
0108 19 3A       	lrri 	ax0.h, @ar1
0109 B0 41       	mulx 	ax0.h, ax1.l    	l    	ax0.l, @ar1
010A A6 4B       	mulxmv	ax0.l, ax1.l, ac0	l    	ax1.l, @ar3
010B F0 51       	lsl16	ac0             	l    	ax0.h, @ar1
010C B4 41       	mulxac	ax0.h, ax1.l, ac0	l    	ax0.l, @ar1
010D 91 00       	asr16	ac0             	     	
010E 11 50 01 17 	bloopi	#0x50, $0x0117
	0110 A7 92       	mulxmv	ax0.l, ax1.l, ac1	sl   	ac0.m, ax1.l
	0111 F1 51       	lsl16	ac1             	l    	ax0.h, @ar1
	0112 B5 20       	mulxac	ax0.h, ax1.l, ac1	s    	@ar0, ac0.l
	0113 99 41       	asr16	ac1             	l    	ax0.l, @ar1
	0114 A6 93       	mulxmv	ax0.l, ax1.l, ac0	sl   	ac1.m, ax1.l
	0115 F0 51       	lsl16	ac0             	l    	ax0.h, @ar1
	0116 B4 28       	mulxac	ax0.h, ax1.l, ac0	s    	@ar0, ac1.l
	0117 91 41       	asr16	ac0             	l    	ax0.l, @ar1

0118 00 C0 0E 45 	lr   	ar0, $0x0E45
011A 02 9F 00 68 	j    	$0x0068
```

```
011C 81 00       	clr  	ac0             	     	
011D 89 70       	clr  	ac1             	l    	ac0.m, @ar0
011E 8E 78       	clr40	                	l    	ac1.m, @ar0
011F 2E CE       	srs  	$(DSMAH), ac0.m
0120 2F CF       	srs  	$(DSMAL), ac1.m
0121 00 9E 0E 48 	lri  	ac0.m, #0x0E48
0123 2E CD       	srs  	$(DSPA), ac0.m
0124 0E 00       	lris 	ac0.m, 0
0125 2E C9       	srs  	$(DSCR), ac0.m
0126 00 9E 00 40 	lri  	ac0.m, #0x0040
0128 2E CB       	srs  	$(DSBL), ac0.m
0129 00 81 0E 48 	lri  	ar1, #0x0E48
012B 00 82 00 00 	lri  	ar2, #0x0000
012D 00 9B 00 9F 	lri  	ax1.h, #0x009F
012F 00 9A 01 40 	lri  	ax0.h, #0x0140
0131 81 00       	clr  	ac0             	     	
0132 89 00       	clr  	ac1             	     	
0133 8F 00       	set40	                	     	
0134 02 BF 06 94 	call 	$0x0694
0136 19 3E       	lrri 	ac0.m, @ar1
0137 19 3C       	lrri 	ac0.l, @ar1
0138 B1 00       	tst  	ac0             	     	
0139 19 3F       	lrri 	ac1.m, @ar1
013A 02 94 01 40 	jne  	$0x0140
013C 00 5A       	loop 	ax0.h
013D 1B 5E       	srri 	@ar2, ac0.m
013E 02 9F 01 48 	j    	$0x0148
0140 99 00       	asr16	ac1             	     	
0141 1B 5E       	srri 	@ar2, ac0.m
0142 1B 5C       	srri 	@ar2, ac0.l
0143 00 7B 01 47 	bloop	ax1.h, $0x0147
0145 4C 00       	add  	ac0, ac1        	     	
0146 1B 5E       	srri 	@ar2, ac0.m
0147 1B 5C       	srri 	@ar2, ac0.l
0148 19 3E       	lrri 	ac0.m, @ar1
0149 19 3C       	lrri 	ac0.l, @ar1
014A B1 00       	tst  	ac0             	     	
014B 19 3F       	lrri 	ac1.m, @ar1
014C 02 94 01 52 	jne  	$0x0152
014E 00 5A       	loop 	ax0.h
014F 1B 5E       	srri 	@ar2, ac0.m
0150 02 9F 01 5A 	j    	$0x015A
0152 99 00       	asr16	ac1             	     	
0153 1B 5E       	srri 	@ar2, ac0.m
0154 1B 5C       	srri 	@ar2, ac0.l
0155 00 7B 01 59 	bloop	ax1.h, $0x0159
0157 4C 00       	add  	ac0, ac1        	     	
0158 1B 5E       	srri 	@ar2, ac0.m
0159 1B 5C       	srri 	@ar2, ac0.l
015A 19 3E       	lrri 	ac0.m, @ar1
015B 19 3C       	lrri 	ac0.l, @ar1
015C B1 00       	tst  	ac0             	     	
015D 19 3F       	lrri 	ac1.m, @ar1
015E 02 94 01 64 	jne  	$0x0164
0160 00 5A       	loop 	ax0.h
0161 1B 5E       	srri 	@ar2, ac0.m
0162 02 9F 01 6C 	j    	$0x016C
0164 99 00       	asr16	ac1             	     	
0165 1B 5E       	srri 	@ar2, ac0.m
0166 1B 5C       	srri 	@ar2, ac0.l
0167 00 7B 01 6B 	bloop	ax1.h, $0x016B
0169 4C 00       	add  	ac0, ac1        	     	
016A 1B 5E       	srri 	@ar2, ac0.m
016B 1B 5C       	srri 	@ar2, ac0.l
016C 00 82 04 00 	lri  	ar2, #0x0400
016E 19 3E       	lrri 	ac0.m, @ar1
016F 19 3C       	lrri 	ac0.l, @ar1
0170 B1 79       	tst  	ac0             	l    	ac1.m, @ar1
0171 02 94 01 77 	jne  	$0x0177
0173 00 5A       	loop 	ax0.h
0174 1B 5E       	srri 	@ar2, ac0.m
0175 02 9F 01 7F 	j    	$0x017F
0177 99 00       	asr16	ac1             	     	
0178 1B 5E       	srri 	@ar2, ac0.m
0179 1B 5C       	srri 	@ar2, ac0.l
017A 00 7B 01 7E 	bloop	ax1.h, $0x017E
017C 4C 00       	add  	ac0, ac1        	     	
017D 1B 5E       	srri 	@ar2, ac0.m
017E 1B 5C       	srri 	@ar2, ac0.l
017F 19 3E       	lrri 	ac0.m, @ar1
0180 19 3C       	lrri 	ac0.l, @ar1
0181 B1 79       	tst  	ac0             	l    	ac1.m, @ar1
0182 02 94 01 88 	jne  	$0x0188
0184 00 5A       	loop 	ax0.h
0185 1B 5E       	srri 	@ar2, ac0.m
0186 02 9F 01 90 	j    	$0x0190
0188 99 00       	asr16	ac1             	     	
0189 1B 5E       	srri 	@ar2, ac0.m
018A 1B 5C       	srri 	@ar2, ac0.l
018B 00 7B 01 8F 	bloop	ax1.h, $0x018F
018D 4C 00       	add  	ac0, ac1        	     	
018E 1B 5E       	srri 	@ar2, ac0.m
018F 1B 5C       	srri 	@ar2, ac0.l
0190 19 3E       	lrri 	ac0.m, @ar1
0191 19 3C       	lrri 	ac0.l, @ar1
0192 B1 79       	tst  	ac0             	l    	ac1.m, @ar1
0193 02 94 01 99 	jne  	$0x0199
0195 00 5A       	loop 	ax0.h
0196 1B 5E       	srri 	@ar2, ac0.m
0197 02 9F 01 A1 	j    	$0x01A1
0199 99 00       	asr16	ac1             	     	
019A 1B 5E       	srri 	@ar2, ac0.m
019B 1B 5C       	srri 	@ar2, ac0.l
019C 00 7B 01 A0 	bloop	ax1.h, $0x01A0
019E 4C 00       	add  	ac0, ac1        	     	
019F 1B 5E       	srri 	@ar2, ac0.m
01A0 1B 5C       	srri 	@ar2, ac0.l
01A1 00 82 07 C0 	lri  	ar2, #0x07C0
01A3 19 3E       	lrri 	ac0.m, @ar1
01A4 19 3C       	lrri 	ac0.l, @ar1
01A5 B1 79       	tst  	ac0             	l    	ac1.m, @ar1
01A6 02 94 01 AC 	jne  	$0x01AC
01A8 00 5A       	loop 	ax0.h
01A9 1B 5E       	srri 	@ar2, ac0.m
01AA 02 9F 01 B4 	j    	$0x01B4
01AC 99 00       	asr16	ac1             	     	
01AD 1B 5E       	srri 	@ar2, ac0.m
01AE 1B 5C       	srri 	@ar2, ac0.l
01AF 00 7B 01 B3 	bloop	ax1.h, $0x01B3
01B1 4C 00       	add  	ac0, ac1        	     	
01B2 1B 5E       	srri 	@ar2, ac0.m
01B3 1B 5C       	srri 	@ar2, ac0.l
01B4 19 3E       	lrri 	ac0.m, @ar1
01B5 19 3C       	lrri 	ac0.l, @ar1
01B6 B1 79       	tst  	ac0             	l    	ac1.m, @ar1
01B7 02 94 01 BD 	jne  	$0x01BD
01B9 00 5A       	loop 	ax0.h
01BA 1B 5E       	srri 	@ar2, ac0.m
01BB 02 9F 01 C5 	j    	$0x01C5
01BD 99 00       	asr16	ac1             	     	
01BE 1B 5E       	srri 	@ar2, ac0.m
01BF 1B 5C       	srri 	@ar2, ac0.l
01C0 00 7B 01 C4 	bloop	ax1.h, $0x01C4
01C2 4C 00       	add  	ac0, ac1        	     	
01C3 1B 5E       	srri 	@ar2, ac0.m
01C4 1B 5C       	srri 	@ar2, ac0.l
01C5 19 3E       	lrri 	ac0.m, @ar1
01C6 19 3C       	lrri 	ac0.l, @ar1
01C7 B1 79       	tst  	ac0             	l    	ac1.m, @ar1
01C8 02 94 01 CE 	jne  	$0x01CE
01CA 00 5A       	loop 	ax0.h
01CB 1B 5E       	srri 	@ar2, ac0.m
01CC 02 9F 01 D6 	j    	$0x01D6
01CE 99 00       	asr16	ac1             	     	
01CF 1B 5E       	srri 	@ar2, ac0.m
01D0 1B 5C       	srri 	@ar2, ac0.l
01D1 00 7B 01 D5 	bloop	ax1.h, $0x01D5
01D3 4C 00       	add  	ac0, ac1        	     	
01D4 1B 5E       	srri 	@ar2, ac0.m
01D5 1B 5C       	srri 	@ar2, ac0.l
01D6 02 9F 00 68 	j    	$0x0068
01D8 00 85 FF FF 	lri  	ix1, #0xFFFF
01DA 81 50       	clr  	ac0             	l    	ax0.h, @ar0
01DB 89 40       	clr  	ac1             	l    	ax0.l, @ar0
01DC 8E 48       	clr40	                	l    	ax1.l, @ar0
01DD 00 FA 0E 17 	sr   	$0x0E17, ax0.h
01DF 00 F8 0E 18 	sr   	$0x0E18, ax0.l
01E1 00 81 00 00 	lri  	ar1, #0x0000
01E3 02 BF 06 29 	call 	$0x0629
01E5 00 DA 0E 17 	lr   	ax0.h, $0x0E17
01E7 00 D8 0E 18 	lr   	ax0.l, $0x0E18
01E9 89 48       	clr  	ac1             	l    	ax1.l, @ar0
01EA 00 81 04 00 	lri  	ar1, #0x0400
01EC 02 BF 06 29 	call 	$0x0629
01EE 00 DA 0E 17 	lr   	ax0.h, $0x0E17
01F0 00 D8 0E 18 	lr   	ax0.l, $0x0E18
01F2 89 48       	clr  	ac1             	l    	ax1.l, @ar0
01F3 00 81 07 C0 	lri  	ar1, #0x07C0
01F5 02 BF 06 29 	call 	$0x0629
01F7 02 9F 00 68 	j    	$0x0068
01F9 00 86 07 C0 	lri  	ix2, #0x07C0
01FB 02 BF 05 BC 	call 	$0x05BC
01FD 02 9F 00 68 	j    	$0x0068
01FF 81 00       	clr  	ac0             	     	
0200 8E 00       	clr40	                	     	
0201 19 1E       	lrri 	ac0.m, @ar0
0202 19 1C       	lrri 	ac0.l, @ar0
0203 2E CE       	srs  	$(DSMAH), ac0.m
0204 2C CF       	srs  	$(DSMAL), ac0.l
0205 16 CD 00 00 	si   	$(DSPA), #0x0000
0207 16 C9 00 01 	si   	$(DSCR), #0x0001
0209 16 CB 07 80 	si   	$(DSBL), #0x0780
020B 02 BF 06 94 	call 	$0x0694
020D 02 9F 00 68 	j    	$0x0068
020F 81 00       	clr  	ac0             	     	
0210 89 70       	clr  	ac1             	l    	ac0.m, @ar0
0211 8E 60       	clr40	                	l    	ac0.l, @ar0
0212 2E CE       	srs  	$(DSMAH), ac0.m
0213 2C CF       	srs  	$(DSMAL), ac0.l
0214 16 CD 0E 48 	si   	$(DSPA), #0x0E48
0216 16 C9 00 00 	si   	$(DSCR), #0x0000
0218 89 00       	clr  	ac1             	     	
0219 0D 20       	lris 	ac1.l, 32
021A 2D CB       	srs  	$(DSBL), ac1.l
021B 4C 00       	add  	ac0, ac1        	     	
021C 1C 80       	mrr  	ix0, ar0
021D 00 80 02 80 	lri  	ar0, #0x0280
021F 00 81 00 00 	lri  	ar1, #0x0000
0221 00 82 01 40 	lri  	ar2, #0x0140
0223 00 83 0E 48 	lri  	ar3, #0x0E48
0225 0A 00       	lris 	ax0.h, 0
0226 27 C9       	lrs  	ac1.m, $(DSCR)
0227 03 A0 00 04 	tclr 	ac1.m, #0x0004
0229 02 9C 02 26 	jnok 	$0x0226
022B 2E CE       	srs  	$(DSMAH), ac0.m
022C 2C CF       	srs  	$(DSMAL), ac0.l
022D 16 CD 0E 58 	si   	$(DSPA), #0x0E58
022F 16 C9 00 00 	si   	$(DSCR), #0x0000
0231 16 CB 02 60 	si   	$(DSBL), #0x0260
0233 00 9F 00 A0 	lri  	ac1.m, #0x00A0
0235 8F 00       	set40	                	     	
0236 00 7F 02 3F 	bloop	ac1.m, $0x023F
0238 19 7E       	lrri 	ac0.m, @ar3
0239 1B 1A       	srri 	@ar0, ax0.h
023A 19 7C       	lrri 	ac0.l, @ar3
023B 1B 1A       	srri 	@ar0, ax0.h
023C 1B 5E       	srri 	@ar2, ac0.m
023D 7C 22       	neg  	ac0             	s    	@ar2, ac0.l
023E 1B 3E       	srri 	@ar1, ac0.m
023F 1B 3C       	srri 	@ar1, ac0.l
0240 1C 04       	mrr  	ar0, ix0
0241 02 9F 00 68 	j    	$0x0068
0243 8E 70       	clr40	                	l    	ac0.m, @ar0
0244 89 60       	clr  	ac1             	l    	ac0.l, @ar0
0245 19 1F       	lrri 	ac1.m, @ar0
0246 2E CE       	srs  	$(DSMAH), ac0.m
0247 2C CF       	srs  	$(DSMAL), ac0.l
0248 16 CD 0C 00 	si   	$(DSPA), #0x0C00
024A 16 C9 00 00 	si   	$(DSCR), #0x0000
024C 05 03       	addis	ac1.m, 3
024D 03 40 FF F0 	andi 	ac1.m, #0xFFF0
024F 2F CB       	srs  	$(DSBL), ac1.m
0250 02 BF 06 94 	call 	$0x0694
0252 00 80 0C 00 	lri  	ar0, #0x0C00
0254 02 9F 00 68 	j    	$0x0068
0256 81 00       	clr  	ac0             	     	
0257 89 70       	clr  	ac1             	l    	ac0.m, @ar0
0258 8E 78       	clr40	                	l    	ac1.m, @ar0
0259 2E CE       	srs  	$(DSMAH), ac0.m
025A 2F CF       	srs  	$(DSMAL), ac1.m
025B 16 CD 0B 80 	si   	$(DSPA), #0x0B80
025D 16 C9 00 00 	si   	$(DSCR), #0x0000
025F 16 CB 00 D0 	si   	$(DSBL), #0x00D0
0261 00 82 0E 08 	lri  	ar2, #0x0E08
0263 00 9F 00 00 	lri  	ac1.m, #0x0000
0265 1B 5F       	srri 	@ar2, ac1.m
0266 00 9F 01 40 	lri  	ac1.m, #0x0140
0268 1B 5F       	srri 	@ar2, ac1.m
0269 00 9F 02 80 	lri  	ac1.m, #0x0280
026B 1B 5F       	srri 	@ar2, ac1.m
026C 00 9F 04 00 	lri  	ac1.m, #0x0400
026E 1B 5F       	srri 	@ar2, ac1.m
026F 00 9F 05 40 	lri  	ac1.m, #0x0540
0271 1B 5F       	srri 	@ar2, ac1.m
0272 00 9F 06 80 	lri  	ac1.m, #0x0680
0274 1B 5F       	srri 	@ar2, ac1.m
0275 00 9F 07 C0 	lri  	ac1.m, #0x07C0
0277 1B 5F       	srri 	@ar2, ac1.m
0278 00 9F 09 00 	lri  	ac1.m, #0x0900
027A 1B 5F       	srri 	@ar2, ac1.m
027B 00 9F 0A 40 	lri  	ac1.m, #0x0A40
027D 1B 5F       	srri 	@ar2, ac1.m
027E 02 BF 06 94 	call 	$0x0694
0280 00 DE 0B A7 	lr   	ac0.m, $0x0BA7
0282 00 DF 0B A8 	lr   	ac1.m, $0x0BA8
0284 2E CE       	srs  	$(DSMAH), ac0.m
0285 2F CF       	srs  	$(DSMAL), ac1.m
0286 16 CD 03 C0 	si   	$(DSPA), #0x03C0
0288 16 C9 00 00 	si   	$(DSCR), #0x0000
028A 16 CB 00 80 	si   	$(DSBL), #0x0080
028C 81 00       	clr  	ac0             	     	
028D 89 00       	clr  	ac1             	     	
028E 00 DE 0B 84 	lr   	ac0.m, $0x0B84
0290 00 9F 0D 4C 	lri  	ac1.m, #0x0D4C
0292 4C 00       	add  	ac0, ac1        	     	
0293 1C 7E       	mrr  	ar3, ac0.m
0294 02 13       	ilrr 	ac0.m, @ar3
0295 00 FE 0E 15 	sr   	$0x0E15, ac0.m
0297 00 DE 0B 85 	lr   	ac0.m, $0x0B85
0299 00 9F 0D 4F 	lri  	ac1.m, #0x0D4F
029B 4C 00       	add  	ac0, ac1        	     	
029C 1C 7E       	mrr  	ar3, ac0.m
029D 02 13       	ilrr 	ac0.m, @ar3
029E 00 FE 0E 16 	sr   	$0x0E16, ac0.m
02A0 00 DE 0B 86 	lr   	ac0.m, $0x0B86
02A2 00 9A 00 0F 	lri  	ax0.h, #0x000F
02A4 00 9F 0C DC 	lri  	ac1.m, #0x0CDC
02A6 34 00       	andr 	ac0.m, ax0.h    	     	
02A7 4C 00       	add  	ac0, ac1        	     	
02A8 1C 7E       	mrr  	ar3, ac0.m
02A9 02 13       	ilrr 	ac0.m, @ar3
02AA 00 FE 0E 14 	sr   	$0x0E14, ac0.m
02AC 00 DE 0B 86 	lr   	ac0.m, $0x0B86
02AE 00 9A 00 1F 	lri  	ax0.h, #0x001F
02B0 00 9F 0C EC 	lri  	ac1.m, #0x0CEC
02B2 14 FC       	asr  	ac0, -4
02B3 34 00       	andr 	ac0.m, ax0.h    	     	
02B4 4C 00       	add  	ac0, ac1        	     	
02B5 1C 7E       	mrr  	ar3, ac0.m
02B6 02 13       	ilrr 	ac0.m, @ar3
02B7 00 FE 0E 46 	sr   	$0x0E46, ac0.m
02B9 00 DE 0B 86 	lr   	ac0.m, $0x0B86
02BB 00 9F 0D 0C 	lri  	ac1.m, #0x0D0C
02BD 14 F7       	asr  	ac0, -9
02BE 4C 00       	add  	ac0, ac1        	     	
02BF 1C 7E       	mrr  	ar3, ac0.m
02C0 02 13       	ilrr 	ac0.m, @ar3
02C1 00 FE 0E 47 	sr   	$0x0E47, ac0.m
02C3 81 00       	clr  	ac0             	     	
02C4 00 DE 0B 9B 	lr   	ac0.m, $0x0B9B
02C6 B1 00       	tst  	ac0             	     	
02C7 02 95 02 EE 	jeq  	$0x02EE
02C9 89 00       	clr  	ac1             	     	
02CA 00 DF 0B 9E 	lr   	ac1.m, $0x0B9E
02CC 03 00 0C C0 	addi 	ac1.m, #0x0CC0
02CE 00 FF 0E 40 	sr   	$0x0E40, ac1.m
02D0 00 DF 0B 9F 	lr   	ac1.m, $0x0B9F
02D2 03 00 0C C0 	addi 	ac1.m, #0x0CC0
02D4 00 FF 0E 41 	sr   	$0x0E41, ac1.m
02D6 00 9F 0C E0 	lri  	ac1.m, #0x0CE0
02D8 00 FF 0E 42 	sr   	$0x0E42, ac1.m
02DA 00 FF 0E 43 	sr   	$0x0E43, ac1.m
02DC 02 BF 06 94 	call 	$0x0694
02DE 00 DE 0B 9C 	lr   	ac0.m, $0x0B9C
02E0 2E CE       	srs  	$(DSMAH), ac0.m
02E1 00 DE 0B 9D 	lr   	ac0.m, $0x0B9D
02E3 2E CF       	srs  	$(DSMAL), ac0.m
02E4 16 CD 0C C0 	si   	$(DSPA), #0x0CC0
02E6 16 C9 00 00 	si   	$(DSCR), #0x0000
02E8 16 CB 00 40 	si   	$(DSBL), #0x0040
02EA 02 BF 06 94 	call 	$0x0694
02EC 02 9F 00 68 	j    	$0x0068
02EE 00 9F 0C E0 	lri  	ac1.m, #0x0CE0
02F0 00 FF 0E 42 	sr   	$0x0E42, ac1.m
02F2 00 FF 0E 40 	sr   	$0x0E40, ac1.m
02F4 00 FF 0E 41 	sr   	$0x0E41, ac1.m
02F6 00 FF 0E 43 	sr   	$0x0E43, ac1.m
02F8 02 BF 06 94 	call 	$0x0694
02FA 02 9F 00 68 	j    	$0x0068
02FC 8E 00       	clr40	                	     	
02FD 00 E0 0E 07 	sr   	$0x0E07, ar0
02FF 00 80 0B A2 	lri  	ar0, #0x0BA2
0301 00 81 03 C0 	lri  	ar1, #0x03C0
0303 0E 05       	lris 	ac0.m, 5
0304 00 FE 0E 04 	sr   	$0x0E04, ac0.m
0306 89 00       	clr  	ac1             	     	
0307 81 50       	clr  	ac0             	l    	ax0.h, @ar0
0308 00 9F 0B 80 	lri  	ac1.m, #0x0B80
030A 00 7A 03 0F 	bloop	ax0.h, $0x030F
030C 19 3E       	lrri 	ac0.m, @ar1
030D 4C 49       	add  	ac0, ac1        	l    	ax1.l, @ar1
030E 1C 5E       	mrr  	ar2, ac0.m
030F 1A 59       	srr  	@ar2, ax1.l
0310 00 83 0E 05 	lri  	ar3, #0x0E05
0312 1B 61       	srri 	@ar3, ar1
0313 1B 60       	srri 	@ar3, ar0
0314 00 DE 0B 87 	lr   	ac0.m, $0x0B87
0316 06 01       	cmpis	ac0.m, 1
0317 02 95 03 1B 	jeq  	$0x031B
0319 02 9F 04 50 	j    	$0x0450
031B 00 DE 0E 42 	lr   	ac0.m, $0x0E42
031D 00 FE 0E 1C 	sr   	$0x0E1C, ac0.m
031F 00 C3 0E 15 	lr   	ar3, $0x0E15
0321 17 7F       	callr	ar3
0322 8E 00       	clr40	                	     	
0323 8A 00       	m2   	                	     	
0324 81 00       	clr  	ac0             	     	
0325 89 00       	clr  	ac1             	     	
0326 00 DE 0B B3 	lr   	ac0.m, $0x0BB3
0328 00 DF 0B B2 	lr   	ac1.m, $0x0BB2
032A 1F 1F       	mrr  	ax0.l, ac1.m
032B 4D 00       	add  	ac1, ac0        	     	
032C 14 81       	asl  	ac0, #0x01
032D 8D 1E       	set15	                	mv   	ax1.h, ac0.m
032E 1F D8       	mrr  	ac0.m, ax0.l
032F 00 98 80 00 	lri  	ax0.l, #0x8000
0331 00 80 0E 48 	lri  	ar0, #0x0E48
0333 A8 30       	mulx 	ax0.l, ax1.h    	s    	@ar0, ac0.m
0334 AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
0335 AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
0336 AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
0337 AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
0338 AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
0339 AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
033A AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
033B AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
033C AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
033D AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
033E AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
033F AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
0340 AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
0341 AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
0342 AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
0343 AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
0344 AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
0345 AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
0346 AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
0347 AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
0348 AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
0349 AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
034A AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
034B AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
034C AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
034D AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
034E AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
034F AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
0350 AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
0351 AD 30       	mulxac	ax0.l, ax1.h, ac1	s    	@ar0, ac0.m
0352 AC 38       	mulxac	ax0.l, ax1.h, ac0	s    	@ar0, ac1.m
0353 00 FE 0B B2 	sr   	$0x0BB2, ac0.m
0355 8F 00       	set40	                	     	
0356 00 80 0E 48 	lri  	ar0, #0x0E48
0358 00 C1 0E 43 	lr   	ar1, $0x0E43
035A 1C 61       	mrr  	ar3, ar1
035B 19 3A       	lrri 	ax0.h, @ar1
035C 19 18       	lrri 	ax0.l, @ar0
035D 90 59       	mul  	ax0.l, ax0.h    	l    	ax1.h, @ar1
035E 19 19       	lrri 	ax1.l, @ar0
035F 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
0360 80 80       	nx   	                	ls   	ax0.l, ac0.m
0361 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
0362 80 91       	nx   	                	ls   	ax1.l, ac1.m
0363 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
0364 80 80       	nx   	                	ls   	ax0.l, ac0.m
0365 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
0366 80 91       	nx   	                	ls   	ax1.l, ac1.m
0367 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
0368 80 80       	nx   	                	ls   	ax0.l, ac0.m
0369 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
036A 80 91       	nx   	                	ls   	ax1.l, ac1.m
036B 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
036C 80 80       	nx   	                	ls   	ax0.l, ac0.m
036D 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
036E 80 91       	nx   	                	ls   	ax1.l, ac1.m
036F 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
0370 80 80       	nx   	                	ls   	ax0.l, ac0.m
0371 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
0372 80 91       	nx   	                	ls   	ax1.l, ac1.m
0373 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
0374 80 80       	nx   	                	ls   	ax0.l, ac0.m
0375 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
0376 80 91       	nx   	                	ls   	ax1.l, ac1.m
0377 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
0378 80 80       	nx   	                	ls   	ax0.l, ac0.m
0379 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
037A 80 91       	nx   	                	ls   	ax1.l, ac1.m
037B 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
037C 80 80       	nx   	                	ls   	ax0.l, ac0.m
037D 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
037E 80 91       	nx   	                	ls   	ax1.l, ac1.m
037F 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
0380 80 80       	nx   	                	ls   	ax0.l, ac0.m
0381 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
0382 80 91       	nx   	                	ls   	ax1.l, ac1.m
0383 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
0384 80 80       	nx   	                	ls   	ax0.l, ac0.m
0385 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
0386 80 91       	nx   	                	ls   	ax1.l, ac1.m
0387 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
0388 80 80       	nx   	                	ls   	ax0.l, ac0.m
0389 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
038A 80 91       	nx   	                	ls   	ax1.l, ac1.m
038B 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
038C 80 80       	nx   	                	ls   	ax0.l, ac0.m
038D 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
038E 80 91       	nx   	                	ls   	ax1.l, ac1.m
038F 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
0390 80 80       	nx   	                	ls   	ax0.l, ac0.m
0391 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
0392 80 91       	nx   	                	ls   	ax1.l, ac1.m
0393 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
0394 80 80       	nx   	                	ls   	ax0.l, ac0.m
0395 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
0396 80 91       	nx   	                	ls   	ax1.l, ac1.m
0397 9E 51       	mulmv	ax1.l, ax1.h, ac0	l    	ax0.h, @ar1
0398 80 80       	nx   	                	ls   	ax0.l, ac0.m
0399 97 59       	mulmv	ax0.l, ax0.h, ac1	l    	ax1.h, @ar1
039A 80 91       	nx   	                	ls   	ax1.l, ac1.m
039B 9E 00       	mulmv	ax1.l, ax1.h, ac0	     	
039C 6F 33       	movp 	ac1             	s    	@ar3, ac0.m
039D 1B 7F       	srri 	@ar3, ac1.m
039E 02 9F 04 02 	j    	$0x0402
03A0 81 00       	clr  	ac0             	     	
03A1 00 DE 0B DD 	lr   	ac0.m, $0x0BDD
03A3 00 00       	nop  	
03A4 B1 00       	tst  	ac0             	     	
03A5 02 95 04 02 	jeq  	$0x0402
03A7 8D 00       	set15	                	     	
03A8 8B 00       	m0   	                	     	
03A9 00 C0 0E 43 	lr   	ar0, $0x0E43
03AB 00 82 0E 48 	lri  	ar2, #0x0E48
03AD 00 83 0B DE 	lri  	ar3, #0x0BDE
03AF 00 84 00 00 	lri  	ix0, #0x0000
03B1 00 87 FF FE 	lri  	ix3, #0xFFFE
03B3 00 DA 0B E3 	lr   	ax0.h, $0x0BE3
03B5 19 7B       	lrri 	ax1.h, @ar3
03B6 B8 00       	mulx 	ax0.h, ax1.h    	     	
03B7 00 DA 0B E4 	lr   	ax0.h, $0x0BE4
03B9 19 7B       	lrri 	ax1.h, @ar3
03BA E3 FC       	maddx	ax0.h, ax1.h    	ldnm 	ax0.h, ax1.h, @ar0
03BB E3 5B       	maddx	ax0.h, ax1.h    	l    	ax1.h, @ar3
03BC 00 DA 0B E4 	lr   	ax0.h, $0x0BE4
03BE 00 84 FF FF 	lri  	ix0, #0xFFFF
03C0 BE F0       	mulxmv	ax0.h, ax1.h, ac0	ld   	ax0.h, ax1.h, @ar0
03C1 E3 FC       	maddx	ax0.h, ax1.h    	ldnm 	ax0.h, ax1.h, @ar0
03C2 E3 F0       	maddx	ax0.h, ax1.h    	ld   	ax0.h, ax1.h, @ar0
03C3 14 F2       	asr  	ac0, -14
03C4 1B 5C       	srri 	@ar2, ac0.l
03C5 11 1D 03 CB 	bloopi	#0x1D, $0x03CB
03C7 BE F0       	mulxmv	ax0.h, ax1.h, ac0	ld   	ax0.h, ax1.h, @ar0
03C8 E3 FC       	maddx	ax0.h, ax1.h    	ldnm 	ax0.h, ax1.h, @ar0
03C9 E3 F0       	maddx	ax0.h, ax1.h    	ld   	ax0.h, ax1.h, @ar0
03CA 14 F2       	asr  	ac0, -14
03CB 1B 5C       	srri 	@ar2, ac0.l
03CC BE F0       	mulxmv	ax0.h, ax1.h, ac0	ld   	ax0.h, ax1.h, @ar0
03CD 00 FA 0B E3 	sr   	$0x0BE3, ax0.h
03CF E3 FC       	maddx	ax0.h, ax1.h    	ldnm 	ax0.h, ax1.h, @ar0
03D0 00 FA 0B E4 	sr   	$0x0BE4, ax0.h
03D2 E3 00       	maddx	ax0.h, ax1.h    	     	
03D3 14 F2       	asr  	ac0, -14
03D4 1B 5C       	srri 	@ar2, ac0.l
03D5 6E 00       	movp 	ac0             	     	
03D6 14 F2       	asr  	ac0, -14
03D7 1B 5C       	srri 	@ar2, ac0.l
03D8 00 C0 0E 43 	lr   	ar0, $0x0E43
03DA 00 C1 0E 43 	lr   	ar1, $0x0E43
03DC 00 82 0E 48 	lri  	ar2, #0x0E48
03DE 00 83 0B E1 	lri  	ar3, #0x0BE1
03E0 00 84 00 00 	lri  	ix0, #0x0000
03E2 00 87 FF FF 	lri  	ix3, #0xFFFF
03E4 00 DA 0B E5 	lr   	ax0.h, $0x0BE5
03E6 19 7B       	lrri 	ax1.h, @ar3
03E7 B8 00       	mulx 	ax0.h, ax1.h    	     	
03E8 00 DA 0B E6 	lr   	ax0.h, $0x0BE6
03EA 89 5F       	clr  	ac1             	ln   	ax1.h, @ar3
03EB E3 5B       	maddx	ax0.h, ax1.h    	l    	ax1.h, @ar3
03EC 6E 6A       	movp 	ac0             	l    	ac1.l, @ar2
03ED 14 F2       	asr  	ac0, -14
03EE 4C 00       	add  	ac0, ac1        	     	
03EF 89 21       	clr  	ac1             	s    	@ar1, ac0.l
03F0 11 1E 03 F7 	bloopi	#0x1E, $0x03F7
03F2 B8 FC       	mulx 	ax0.h, ax1.h    	ldnm 	ax0.h, ax1.h, @ar0
03F3 E3 F0       	maddx	ax0.h, ax1.h    	ld   	ax0.h, ax1.h, @ar0
03F4 6E 6A       	movp 	ac0             	l    	ac1.l, @ar2
03F5 14 F2       	asr  	ac0, -14
03F6 4C 00       	add  	ac0, ac1        	     	
03F7 89 21       	clr  	ac1             	s    	@ar1, ac0.l
03F8 00 FC 0B E5 	sr   	$0x0BE5, ac0.l
03FA B8 FC       	mulx 	ax0.h, ax1.h    	ldnm 	ax0.h, ax1.h, @ar0
03FB E3 F0       	maddx	ax0.h, ax1.h    	ld   	ax0.h, ax1.h, @ar0
03FC 6E 6A       	movp 	ac0             	l    	ac1.l, @ar2
03FD 14 F2       	asr  	ac0, -14
03FE 4C 00       	add  	ac0, ac1        	     	
03FF 00 FC 0B E6 	sr   	$0x0BE6, ac0.l
0401 89 21       	clr  	ac1             	s    	@ar1, ac0.l
0402 00 C3 0E 14 	lr   	ar3, $0x0E14
0404 8A 00       	m2   	                	     	
0405 17 7F       	callr	ar3
0406 00 C3 0E 46 	lr   	ar3, $0x0E46
0408 8A 00       	m2   	                	     	
0409 17 7F       	callr	ar3
040A 00 C3 0E 47 	lr   	ar3, $0x0E47
040C 8A 00       	m2   	                	     	
040D 17 7F       	callr	ar3
040E 81 00       	clr  	ac0             	     	
040F 00 DE 0B 9B 	lr   	ac0.m, $0x0B9B
0411 B1 00       	tst  	ac0             	     	
0412 02 95 04 48 	jeq  	$0x0448
0414 00 DE 0E 42 	lr   	ac0.m, $0x0E42
0416 00 FE 0E 43 	sr   	$0x0E43, ac0.m
0418 81 00       	clr  	ac0             	     	
0419 89 00       	clr  	ac1             	     	
041A 00 DE 0B 9E 	lr   	ac0.m, $0x0B9E
041C 00 DF 0B A0 	lr   	ac1.m, $0x0BA0
041E 82 00       	cmp  	                	     	
041F 02 93 04 24 	jle  	$0x0424
0421 78 00       	decm 	ac0             	     	
0422 02 9F 04 27 	j    	$0x0427
0424 02 95 04 27 	jeq  	$0x0427
0426 74 00       	incm 	ac0             	     	
0427 00 FE 0B 9E 	sr   	$0x0B9E, ac0.m
0429 00 DF 0E 43 	lr   	ac1.m, $0x0E43
042B 05 E0       	addis	ac1.m, -32
042C 4C 00       	add  	ac0, ac1        	     	
042D 00 FE 0E 40 	sr   	$0x0E40, ac0.m
042F 81 00       	clr  	ac0             	     	
0430 89 00       	clr  	ac1             	     	
0431 00 DE 0B 9F 	lr   	ac0.m, $0x0B9F
0433 00 DF 0B A1 	lr   	ac1.m, $0x0BA1
0435 82 00       	cmp  	                	     	
0436 02 93 04 3B 	jle  	$0x043B
0438 78 00       	decm 	ac0             	     	
0439 02 9F 04 3E 	j    	$0x043E
043B 02 95 04 3E 	jeq  	$0x043E
043D 74 00       	incm 	ac0             	     	
043E 00 FE 0B 9F 	sr   	$0x0B9F, ac0.m
0440 00 DF 0E 43 	lr   	ac1.m, $0x0E43
0442 05 E0       	addis	ac1.m, -32
0443 4C 00       	add  	ac0, ac1        	     	
0444 00 FE 0E 41 	sr   	$0x0E41, ac0.m
0446 02 9F 04 50 	j    	$0x0450
0448 00 DE 0E 42 	lr   	ac0.m, $0x0E42
044A 00 FE 0E 40 	sr   	$0x0E40, ac0.m
044C 00 FE 0E 41 	sr   	$0x0E41, ac0.m
044E 00 FE 0E 43 	sr   	$0x0E43, ac0.m
0450 81 00       	clr  	ac0             	     	
0451 8E 00       	clr40	                	     	
0452 84 00       	clrp 	                	     	
0453 89 00       	clr  	ac1             	     	
0454 1E FE       	mrr  	prod.m2, ac0.m
0455 0E 40       	lris 	ac0.m, 64
0456 1E BE       	mrr  	prod.m1, ac0.m
0457 00 83 0E 08 	lri  	ar3, #0x0E08
0459 1C 03       	mrr  	ar0, ar3
045A 1F F5       	mrr  	ac1.m, prod.m1
045B 19 1A       	lrri 	ax0.h, @ar0
045C F8 58       	addpaxz	ac0, ax0.h    	l    	ax1.h, @ar0
045D FB A0       	addpaxz	ac1, ax1.h    	ls   	ax0.h, ac0.m
045E F8 B1       	addpaxz	ac0, ax0.h    	ls   	ax1.h, ac1.m
045F FB A0       	addpaxz	ac1, ax1.h    	ls   	ax0.h, ac0.m
0460 F8 B1       	addpaxz	ac0, ax0.h    	ls   	ax1.h, ac1.m
0461 FB A0       	addpaxz	ac1, ax1.h    	ls   	ax0.h, ac0.m
0462 F8 B1       	addpaxz	ac0, ax0.h    	ls   	ax1.h, ac1.m
0463 FB A0       	addpaxz	ac1, ax1.h    	ls   	ax0.h, ac0.m
0464 F8 3B       	addpaxz	ac0, ax0.h    	s    	@ar3, ac1.m
0465 1B 7E       	srri 	@ar3, ac0.m
0466 00 83 0E 04 	lri  	ar3, #0x0E04
0468 81 00       	clr  	ac0             	     	
0469 89 73       	clr  	ac1             	l    	ac0.m, @ar3
046A 19 61       	lrri 	ar1, @ar3
046B 19 60       	lrri 	ar0, @ar3
046C 78 00       	decm 	ac0             	     	
046D 00 FE 0E 04 	sr   	$0x0E04, ac0.m
046F 02 94 03 07 	jne  	$0x0307
0471 8E 00       	clr40	                	     	
0472 81 00       	clr  	ac0             	     	
0473 00 DE 0B 9B 	lr   	ac0.m, $0x0B9B
0475 B1 00       	tst  	ac0             	     	
0476 02 95 04 88 	jeq  	$0x0488
0478 00 DE 0B 9C 	lr   	ac0.m, $0x0B9C
047A 00 DC 0B 9D 	lr   	ac0.l, $0x0B9D
047C 2E CE       	srs  	$(DSMAH), ac0.m
047D 2C CF       	srs  	$(DSMAL), ac0.l
047E 81 00       	clr  	ac0             	     	
047F 00 DE 0E 1C 	lr   	ac0.m, $0x0E1C
0481 2E CD       	srs  	$(DSPA), ac0.m
0482 16 C9 00 01 	si   	$(DSCR), #0x0001
0484 16 CB 00 40 	si   	$(DSBL), #0x0040
0486 02 BF 06 94 	call 	$0x0694
0488 81 00       	clr  	ac0             	     	
0489 89 00       	clr  	ac1             	     	
048A 00 DE 0B 82 	lr   	ac0.m, $0x0B82
048C 00 DF 0B 83 	lr   	ac1.m, $0x0B83
048E 2E CE       	srs  	$(DSMAH), ac0.m
048F 2F CF       	srs  	$(DSMAL), ac1.m
0490 16 CD 0B 80 	si   	$(DSPA), #0x0B80
0492 16 C9 00 01 	si   	$(DSCR), #0x0001
0494 16 CB 00 D0 	si   	$(DSBL), #0x00D0
0496 02 BF 06 94 	call 	$0x0694
0498 81 00       	clr  	ac0             	     	
0499 00 DE 0B 80 	lr   	ac0.m, $0x0B80
049B 00 DC 0B 81 	lr   	ac0.l, $0x0B81
049D B1 00       	tst  	ac0             	     	
049E 02 94 04 A4 	jne  	$0x04A4
04A0 00 C0 0E 07 	lr   	ar0, $0x0E07
04A2 02 9F 00 68 	j    	$0x0068
04A4 2E CE       	srs  	$(DSMAH), ac0.m
04A5 2C CF       	srs  	$(DSMAL), ac0.l
04A6 16 CD 0B 80 	si   	$(DSPA), #0x0B80
04A8 16 C9 00 00 	si   	$(DSCR), #0x0000
04AA 16 CB 00 D0 	si   	$(DSBL), #0x00D0
04AC 00 82 0E 08 	lri  	ar2, #0x0E08
04AE 00 9F 00 00 	lri  	ac1.m, #0x0000
04B0 1B 5F       	srri 	@ar2, ac1.m
04B1 00 9F 01 40 	lri  	ac1.m, #0x0140
04B3 1B 5F       	srri 	@ar2, ac1.m
04B4 00 9F 02 80 	lri  	ac1.m, #0x0280
04B6 1B 5F       	srri 	@ar2, ac1.m
04B7 00 9F 04 00 	lri  	ac1.m, #0x0400
04B9 1B 5F       	srri 	@ar2, ac1.m
04BA 00 9F 05 40 	lri  	ac1.m, #0x0540
04BC 1B 5F       	srri 	@ar2, ac1.m
04BD 00 9F 06 80 	lri  	ac1.m, #0x0680
04BF 1B 5F       	srri 	@ar2, ac1.m
04C0 00 9F 07 C0 	lri  	ac1.m, #0x07C0
04C2 1B 5F       	srri 	@ar2, ac1.m
04C3 00 9F 09 00 	lri  	ac1.m, #0x0900
04C5 1B 5F       	srri 	@ar2, ac1.m
04C6 00 9F 0A 40 	lri  	ac1.m, #0x0A40
04C8 1B 5F       	srri 	@ar2, ac1.m
04C9 02 BF 06 94 	call 	$0x0694
04CB 00 DE 0B A7 	lr   	ac0.m, $0x0BA7
04CD 00 DF 0B A8 	lr   	ac1.m, $0x0BA8
04CF 2E CE       	srs  	$(DSMAH), ac0.m
04D0 2F CF       	srs  	$(DSMAL), ac1.m
04D1 16 CD 03 C0 	si   	$(DSPA), #0x03C0
04D3 16 C9 00 00 	si   	$(DSCR), #0x0000
04D5 16 CB 00 80 	si   	$(DSBL), #0x0080
04D7 81 00       	clr  	ac0             	     	
04D8 89 00       	clr  	ac1             	     	
04D9 00 DE 0B 84 	lr   	ac0.m, $0x0B84
04DB 00 9F 0D 4C 	lri  	ac1.m, #0x0D4C
04DD 4C 00       	add  	ac0, ac1        	     	
04DE 1C 7E       	mrr  	ar3, ac0.m
04DF 02 13       	ilrr 	ac0.m, @ar3
04E0 00 FE 0E 15 	sr   	$0x0E15, ac0.m
04E2 00 DE 0B 85 	lr   	ac0.m, $0x0B85
04E4 00 9F 0D 4F 	lri  	ac1.m, #0x0D4F
04E6 4C 00       	add  	ac0, ac1        	     	
04E7 1C 7E       	mrr  	ar3, ac0.m
04E8 02 13       	ilrr 	ac0.m, @ar3
04E9 00 FE 0E 16 	sr   	$0x0E16, ac0.m
04EB 00 DE 0B 86 	lr   	ac0.m, $0x0B86
04ED 00 9A 00 0F 	lri  	ax0.h, #0x000F
04EF 00 9F 0C DC 	lri  	ac1.m, #0x0CDC
04F1 34 00       	andr 	ac0.m, ax0.h    	     	
04F2 4C 00       	add  	ac0, ac1        	     	
04F3 1C 7E       	mrr  	ar3, ac0.m
04F4 02 13       	ilrr 	ac0.m, @ar3
04F5 00 FE 0E 14 	sr   	$0x0E14, ac0.m
04F7 00 DE 0B 86 	lr   	ac0.m, $0x0B86
04F9 00 9A 00 1F 	lri  	ax0.h, #0x001F
04FB 00 9F 0C EC 	lri  	ac1.m, #0x0CEC
04FD 14 FC       	asr  	ac0, -4
04FE 34 00       	andr 	ac0.m, ax0.h    	     	
04FF 4C 00       	add  	ac0, ac1        	     	
0500 1C 7E       	mrr  	ar3, ac0.m
0501 02 13       	ilrr 	ac0.m, @ar3
0502 00 FE 0E 46 	sr   	$0x0E46, ac0.m
0504 00 DE 0B 86 	lr   	ac0.m, $0x0B86
0506 00 9F 0D 0C 	lri  	ac1.m, #0x0D0C
0508 14 F7       	asr  	ac0, -9
0509 4C 00       	add  	ac0, ac1        	     	
050A 1C 7E       	mrr  	ar3, ac0.m
050B 02 13       	ilrr 	ac0.m, @ar3
050C 00 FE 0E 47 	sr   	$0x0E47, ac0.m
050E 81 00       	clr  	ac0             	     	
050F 00 DE 0B 9B 	lr   	ac0.m, $0x0B9B
0511 B1 00       	tst  	ac0             	     	
0512 02 95 05 3B 	jeq  	$0x053B
0514 89 00       	clr  	ac1             	     	
0515 00 DF 0B 9E 	lr   	ac1.m, $0x0B9E
0517 03 00 0C C0 	addi 	ac1.m, #0x0CC0
0519 00 FF 0E 40 	sr   	$0x0E40, ac1.m
051B 00 DF 0B 9F 	lr   	ac1.m, $0x0B9F
051D 03 00 0C C0 	addi 	ac1.m, #0x0CC0
051F 00 FF 0E 41 	sr   	$0x0E41, ac1.m
0521 00 9F 0C E0 	lri  	ac1.m, #0x0CE0
0523 00 FF 0E 42 	sr   	$0x0E42, ac1.m
0525 00 FF 0E 43 	sr   	$0x0E43, ac1.m
0527 02 BF 06 94 	call 	$0x0694
0529 00 DE 0B 9C 	lr   	ac0.m, $0x0B9C
052B 2E CE       	srs  	$(DSMAH), ac0.m
052C 00 DE 0B 9D 	lr   	ac0.m, $0x0B9D
052E 2E CF       	srs  	$(DSMAL), ac0.m
052F 16 CD 0C C0 	si   	$(DSPA), #0x0CC0
0531 16 C9 00 00 	si   	$(DSCR), #0x0000
0533 16 CB 00 40 	si   	$(DSBL), #0x0040
0535 02 BF 06 94 	call 	$0x0694
0537 00 C0 0E 07 	lr   	ar0, $0x0E07
0539 02 9F 02 FC 	j    	$0x02FC
053B 00 9F 0C E0 	lri  	ac1.m, #0x0CE0
053D 00 FF 0E 42 	sr   	$0x0E42, ac1.m
053F 00 FF 0E 40 	sr   	$0x0E40, ac1.m
0541 00 FF 0E 41 	sr   	$0x0E41, ac1.m
0543 00 FF 0E 43 	sr   	$0x0E43, ac1.m
0545 02 BF 06 94 	call 	$0x0694
0547 00 C0 0E 07 	lr   	ar0, $0x0E07
0549 02 9F 02 FC 	j    	$0x02FC
054B 8E 00       	clr40	                	     	
054C 00 86 04 00 	lri  	ix2, #0x0400
054E 81 00       	clr  	ac0             	     	
054F 89 70       	clr  	ac1             	l    	ac0.m, @ar0
0550 19 1C       	lrri 	ac0.l, @ar0
0551 2E CE       	srs  	$(DSMAH), ac0.m
0552 2C CF       	srs  	$(DSMAL), ac0.l
0553 1F C6       	mrr  	ac0.m, ix2
0554 2E CD       	srs  	$(DSPA), ac0.m
0555 16 C9 00 01 	si   	$(DSCR), #0x0001
0557 16 CB 07 80 	si   	$(DSBL), #0x0780
0559 02 BF 06 94 	call 	$0x0694
055B 02 BF 05 BC 	call 	$0x05BC
055D 02 9F 00 68 	j    	$0x0068
055F 8E 00       	clr40	                	     	
0560 00 86 07 C0 	lri  	ix2, #0x07C0
0562 81 00       	clr  	ac0             	     	
0563 89 70       	clr  	ac1             	l    	ac0.m, @ar0
0564 19 1C       	lrri 	ac0.l, @ar0
0565 2E CE       	srs  	$(DSMAH), ac0.m
0566 2C CF       	srs  	$(DSMAL), ac0.l
0567 1F C6       	mrr  	ac0.m, ix2
0568 2E CD       	srs  	$(DSPA), ac0.m
0569 16 C9 00 01 	si   	$(DSCR), #0x0001
056B 16 CB 07 80 	si   	$(DSBL), #0x0780
056D 02 BF 06 94 	call 	$0x0694
056F 02 BF 05 BC 	call 	$0x05BC
0571 02 9F 00 68 	j    	$0x0068
```

## Command 0xE  - OUTPUT Copy out 640B + 640B bytes (2 Frames)

```
0573 8C 00       	clr15	                	     	
0574 8A 00       	m2   	                	     	
0575 81 00       	clr  	ac0             	     	
0576 89 70       	clr  	ac1             	l    	ac0.m, @ar0
0577 19 1F       	lrri 	ac1.m, @ar0
0578 2E CE       	srs  	$(DSMAH), ac0.m
0579 2F CF       	srs  	$(DSMAL), ac1.m
057A 16 CD 02 80 	si   	$(DSPA), #0x0280
057C 16 C9 00 01 	si   	$(DSCR), #0x0001
057E 16 CB 02 80 	si   	$(DSBL), #0x0280

// Interleave L/R. Source: #0x0 (Left, 160 * 4 byte per sample), #0x140 (Right, 160 * 4 byte per sample). Dest: #0x400 (320 16-bit L/R sample pairs)

0580 8F 50       	set40	                	l    	ax0.h, @ar0
0581 81 40       	clr  	ac0             	l    	ax0.l, @ar0
0582 00 81 04 00 	lri  	ar1, #0x0400
0584 00 83 00 00 	lri  	ar3, #0x0000
0586 00 82 01 40 	lri  	ar2, #0x0140
0588 00 99 00 80 	lri  	ax1.l, #0x0080
058A 02 BF 06 94 	call 	$0x0694
058C 11 05 05 A4 	bloopi	#0x05, $0x05A4
	058E 1F 61       	mrr  	ax1.h, ar1
	058F 11 20 05 96 	bloopi	#0x20, $0x0596
		0591 89 72       	clr  	ac1             	l    	ac0.m, @ar2
		0592 19 5C       	lrri 	ac0.l, @ar2
		0593 F0 7B       	lsl16	ac0             	l    	ac1.m, @ar3
		0594 19 7D       	lrri 	ac1.l, @ar3
		0595 F1 31       	lsl16	ac1             	s    	@ar1, ac0.m
		0596 81 39       	clr  	ac0             	s    	@ar1, ac1.m
	0597 89 00       	clr  	ac1             	     	
	0598 68 00       	movax	ac0, ax0        	     	
	0599 2E CE       	srs  	$(DSMAH), ac0.m
	059A 2C CF       	srs  	$(DSMAL), ac0.l
	059B 1F FB       	mrr  	ac1.m, ax1.h
	059C 2F CD       	srs  	$(DSPA), ac1.m
	059D 0F 01       	lris 	ac1.m, 1
	059E 2F C9       	srs  	$(DSCR), ac1.m
	059F 1F F9       	mrr  	ac1.m, ax1.l
	05A0 2F CB       	srs  	$(DSBL), ac1.m
	05A1 72 00       	addaxl	ac0, ax1.l     	     	
	05A2 1F 5E       	mrr  	ax0.h, ac0.m
	05A3 1F 1C       	mrr  	ax0.l, ac0.l
	05A4 81 00       	clr  	ac0             	     	

// Wait DSP DMA

05A5 26 C9       	lrs  	ac0.m, $(DSCR)
05A6 02 A0 00 04 	tclr 	ac0.m, #0x0004
05A8 02 9C 05 A5 	jnok 	$0x05A5
05AA 02 9F 00 68 	j    	$0x0068
```

```
05AC 02 9F 00 68 	j    	$0x0068
05AE 02 9F 00 68 	j    	$0x0068
05B0 02 9F 00 68 	j    	$0x0068
05B2 16 FC DC D1 	si   	$(DMBH), #0xDCD1
05B4 16 FD 00 02 	si   	$(DMBL), #0x0002
05B6 16 FB 00 01 	si   	$(DIRQ), #0x0001
05B8 02 9F 0F 34 	j    	$0x0F34
05BA 02 9F 00 45 	j    	$0x0045
05BC 8E 00       	clr40	                	     	
05BD 19 1F       	lrri 	ac1.m, @ar0
05BE 19 1D       	lrri 	ac1.l, @ar0
05BF 1F 5F       	mrr  	ax0.h, ac1.m
05C0 1F 1D       	mrr  	ax0.l, ac1.l
05C1 2F CE       	srs  	$(DSMAH), ac1.m
05C2 2D CF       	srs  	$(DSMAL), ac1.l
05C3 89 00       	clr  	ac1             	     	
05C4 1F A6       	mrr  	ac1.l, ix2
05C5 2D CD       	srs  	$(DSPA), ac1.l
05C6 0E 00       	lris 	ac0.m, 0
05C7 2E C9       	srs  	$(DSCR), ac0.m
05C8 81 00       	clr  	ac0             	     	
05C9 00 9C 00 C0 	lri  	ac0.l, #0x00C0
05CB 2C CB       	srs  	$(DSBL), ac0.l
05CC 1C A0       	mrr  	ix1, ar0
05CD 00 81 0E 48 	lri  	ar1, #0x0E48
05CF 48 00       	addax	ac0, ax0        	     	
05D0 1B 3E       	srri 	@ar1, ac0.m
05D1 1B 3C       	srri 	@ar1, ac0.l
05D2 0B 00       	lris 	ax1.h, 0
05D3 00 99 00 60 	lri  	ax1.l, #0x0060
05D5 4B 00       	addax	ac1, ax1        	     	
05D6 1B 3D       	srri 	@ar1, ac1.l
05D7 00 81 0E 48 	lri  	ar1, #0x0E48
05D9 1C 06       	mrr  	ar0, ix2
05DA 00 83 00 00 	lri  	ar3, #0x0000
05DC 1C 43       	mrr  	ar2, ar3
05DD 27 C9       	lrs  	ac1.m, $(DSCR)
05DE 03 A0 00 04 	tclr 	ac1.m, #0x0004
05E0 02 9C 05 DD 	jnok 	$0x05DD
05E2 11 09 06 12 	bloopi	#0x09, $0x0612
05E4 8E 00       	clr40	                	     	
05E5 19 3A       	lrri 	ax0.h, @ar1
05E6 19 38       	lrri 	ax0.l, @ar1
05E7 69 00       	movax	ac1, ax0        	     	
05E8 2F CE       	srs  	$(DSMAH), ac1.m
05E9 2D CF       	srs  	$(DSMAL), ac1.l
05EA 89 00       	clr  	ac1             	     	
05EB 19 3D       	lrri 	ac1.l, @ar1
05EC 2D CD       	srs  	$(DSPA), ac1.l
05ED 16 C9 00 00 	si   	$(DSCR), #0x0000
05EF 81 00       	clr  	ac0             	     	
05F0 00 9C 00 C0 	lri  	ac0.l, #0x00C0
05F2 2C CB       	srs  	$(DSBL), ac0.l
05F3 00 81 0E 48 	lri  	ar1, #0x0E48
05F5 48 00       	addax	ac0, ax0        	     	
05F6 1B 3E       	srri 	@ar1, ac0.m
05F7 1B 3C       	srri 	@ar1, ac0.l
05F8 0B 00       	lris 	ax1.h, 0
05F9 09 60       	lris 	ax1.l, 96
05FA 4B 00       	addax	ac1, ax1        	     	
05FB 1B 3D       	srri 	@ar1, ac1.l
05FC 00 81 0E 48 	lri  	ar1, #0x0E48
05FE 8F 00       	set40	                	     	
05FF 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
0600 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
0601 6A 00       	movax	ac0, ax1        	     	
0602 48 00       	addax	ac0, ax0        	     	
0603 11 17 06 0C 	bloopi	#0x17, $0x060C
0605 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
0606 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
0607 6B 32       	movax	ac1, ax1        	s    	@ar2, ac0.m
0608 49 22       	addax	ac1, ax0        	s    	@ar2, ac0.l
0609 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
060A 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
060B 6A 3A       	movax	ac0, ax1        	s    	@ar2, ac1.m
060C 48 2A       	addax	ac0, ax0        	s    	@ar2, ac1.l
060D 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
060E 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
060F 6B 32       	movax	ac1, ax1        	s    	@ar2, ac0.m
0610 49 22       	addax	ac1, ax0        	s    	@ar2, ac0.l
0611 1B 5F       	srri 	@ar2, ac1.m
0612 1B 5D       	srri 	@ar2, ac1.l
0613 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
0614 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
0615 6A 00       	movax	ac0, ax1        	     	
0616 48 00       	addax	ac0, ax0        	     	
0617 11 17 06 20 	bloopi	#0x17, $0x0620
0619 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
061A 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
061B 6B 32       	movax	ac1, ax1        	s    	@ar2, ac0.m
061C 49 22       	addax	ac1, ax0        	s    	@ar2, ac0.l
061D 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
061E 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
061F 6A 3A       	movax	ac0, ax1        	s    	@ar2, ac1.m
0620 48 2A       	addax	ac0, ax0        	s    	@ar2, ac1.l
0621 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
0622 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
0623 6B 32       	movax	ac1, ax1        	s    	@ar2, ac0.m
0624 49 22       	addax	ac1, ax0        	s    	@ar2, ac0.l
0625 1B 5F       	srri 	@ar2, ac1.m
0626 1B 5D       	srri 	@ar2, ac1.l
0627 1C 05       	mrr  	ar0, ix1
0628 02 DF       	ret  	
0629 8E 00       	clr40	                	     	
062A 00 9B 0E 48 	lri  	ax1.h, #0x0E48
062C 00 9D 00 C0 	lri  	ac1.l, #0x00C0
062E 02 BF 06 79 	call 	$0x0679
0630 49 00       	addax	ac1, ax0        	     	
0631 00 FF 0E 1D 	sr   	$0x0E1D, ac1.m
0633 00 FD 0E 1E 	sr   	$0x0E1E, ac1.l
0635 89 00       	clr  	ac1             	     	
0636 02 BF 06 94 	call 	$0x0694
0638 11 04 06 64 	bloopi	#0x04, $0x0664
063A 00 DA 0E 1D 	lr   	ax0.h, $0x0E1D
063C 00 D8 0E 1E 	lr   	ax0.l, $0x0E1E
063E 00 9B 0E A8 	lri  	ax1.h, #0x0EA8
0640 00 9D 00 C0 	lri  	ac1.l, #0x00C0
0642 02 BF 06 79 	call 	$0x0679
0644 49 00       	addax	ac1, ax0        	     	
0645 00 FF 0E 1D 	sr   	$0x0E1D, ac1.m
0647 00 FD 0E 1E 	sr   	$0x0E1E, ac1.l
0649 00 83 0E 48 	lri  	ar3, #0x0E48
064B 02 BF 06 84 	call 	$0x0684
064D 89 00       	clr  	ac1             	     	
064E 00 DA 0E 1D 	lr   	ax0.h, $0x0E1D
0650 00 D8 0E 1E 	lr   	ax0.l, $0x0E1E
0652 00 9B 0E 48 	lri  	ax1.h, #0x0E48
0654 00 9D 00 C0 	lri  	ac1.l, #0x00C0
0656 02 BF 06 79 	call 	$0x0679
0658 49 00       	addax	ac1, ax0        	     	
0659 00 FF 0E 1D 	sr   	$0x0E1D, ac1.m
065B 00 FD 0E 1E 	sr   	$0x0E1E, ac1.l
065D 00 83 0E A8 	lri  	ar3, #0x0EA8
065F 02 BF 06 84 	call 	$0x0684
0661 00 00       	nop  	
0662 00 00       	nop  	
0663 8E 00       	clr40	                	     	
0664 89 00       	clr  	ac1             	     	
0665 00 DA 0E 1D 	lr   	ax0.h, $0x0E1D
0667 00 D8 0E 1E 	lr   	ax0.l, $0x0E1E
0669 00 9B 0E A8 	lri  	ax1.h, #0x0EA8
066B 00 9D 00 C0 	lri  	ac1.l, #0x00C0
066D 02 BF 06 79 	call 	$0x0679
066F 49 00       	addax	ac1, ax0        	     	
0670 00 83 0E 48 	lri  	ar3, #0x0E48
0672 02 BF 06 84 	call 	$0x0684
0674 00 83 0E A8 	lri  	ar3, #0x0EA8
0676 02 BF 06 84 	call 	$0x0684
0678 02 DF       	ret  	
0679 8E 00       	clr40	                	     	
067A 00 FA FF CE 	sr   	$(DSMAH), ax0.h
067C 00 F8 FF CF 	sr   	$(DSMAL), ax0.l
067E 00 FB FF CD 	sr   	$(DSPA), ax1.h
0680 16 C9 00 00 	si   	$(DSCR), #0x0000
0682 2D CB       	srs  	$(DSBL), ac1.l
0683 02 DF       	ret  	
0684 8F 00       	set40	                	     	
0685 8D 00       	set15	                	     	
0686 8A 00       	m2   	                	     	
0687 19 7A       	lrri 	ax0.h, @ar3
0688 19 78       	lrri 	ax0.l, @ar3
0689 A0 00       	mulx 	ax0.l, ax1.l    	     	
068A B6 00       	mulxmv	ax0.h, ax1.l, ac0	     	
068B 11 30 06 92 	bloopi	#0x30, $0x0692
068D 91 79       	asr16	ac0             	l    	ac1.m, @ar1
068E 4E 6D       	addp 	ac0             	ln   	ac1.l, @ar1
068F 19 7A       	lrri 	ax0.h, @ar3
0690 4D 43       	add  	ac1, ac0        	l    	ax0.l, @ar3
0691 A0 39       	mulx 	ax0.l, ax1.l    	s    	@ar1, ac1.m
0692 B6 29       	mulxmv	ax0.h, ax1.l, ac0	s    	@ar1, ac1.l
0693 02 DF       	ret  	
0694 26 C9       	lrs  	ac0.m, $(DSCR)
0695 02 A0 00 04 	tclr 	ac0.m, #0x0004
0697 02 9C 06 94 	jnok 	$0x0694
0699 02 DF       	ret  	
069A 26 FE       	lrs  	ac0.m, $(CMBH)
069B 02 C0 80 00 	tset 	ac0.m, #0x8000
069D 02 9C 06 9A 	jnok 	$0x069A
069F 02 DF       	ret  	
06A0 26 FC       	lrs  	ac0.m, $(DMBH)
06A1 02 A0 80 00 	tclr 	ac0.m, #0x8000
06A3 02 9C 06 A0 	jnok 	$0x06A0
06A5 02 DF       	ret  	
06A6 26 FC       	lrs  	ac0.m, $(DMBH)
06A7 02 A0 80 00 	tclr 	ac0.m, #0x8000
06A9 02 9C 06 A6 	jnok 	$0x06A6
06AB 02 DF       	ret  	
06AC 81 00       	clr  	ac0             	     	
06AD 89 70       	clr  	ac1             	l    	ac0.m, @ar0
06AE 8E 60       	clr40	                	l    	ac0.l, @ar0
06AF 2E CE       	srs  	$(DSMAH), ac0.m
06B0 2C CF       	srs  	$(DSMAL), ac0.l
06B1 16 CD 0E 48 	si   	$(DSPA), #0x0E48
06B3 16 C9 00 00 	si   	$(DSCR), #0x0000
06B5 89 00       	clr  	ac1             	     	
06B6 0D 20       	lris 	ac1.l, 32
06B7 2D CB       	srs  	$(DSBL), ac1.l
06B8 4C 00       	add  	ac0, ac1        	     	
06B9 1C 80       	mrr  	ix0, ar0
06BA 00 80 02 80 	lri  	ar0, #0x0280
06BC 00 81 00 00 	lri  	ar1, #0x0000
06BE 00 82 01 40 	lri  	ar2, #0x0140
06C0 00 83 0E 48 	lri  	ar3, #0x0E48
06C2 0A 00       	lris 	ax0.h, 0
06C3 27 C9       	lrs  	ac1.m, $(DSCR)
06C4 03 A0 00 04 	tclr 	ac1.m, #0x0004
06C6 02 9C 06 C3 	jnok 	$0x06C3
06C8 2E CE       	srs  	$(DSMAH), ac0.m
06C9 2C CF       	srs  	$(DSMAL), ac0.l
06CA 16 CD 0E 58 	si   	$(DSPA), #0x0E58
06CC 16 C9 00 00 	si   	$(DSCR), #0x0000
06CE 16 CB 02 60 	si   	$(DSBL), #0x0260
06D0 00 9F 00 A0 	lri  	ac1.m, #0x00A0
06D2 8F 00       	set40	                	     	
06D3 00 7F 06 DC 	bloop	ac1.m, $0x06DC
06D5 19 7E       	lrri 	ac0.m, @ar3
06D6 1B 1A       	srri 	@ar0, ax0.h
06D7 19 7C       	lrri 	ac0.l, @ar3
06D8 1B 1A       	srri 	@ar0, ax0.h
06D9 1B 5E       	srri 	@ar2, ac0.m
06DA 1B 5C       	srri 	@ar2, ac0.l
06DB 1B 3E       	srri 	@ar1, ac0.m
06DC 1B 3C       	srri 	@ar1, ac0.l
06DD 1C 04       	mrr  	ar0, ix0
06DE 02 9F 00 68 	j    	$0x0068
```

```
06E0 00 82 0B B8 	lri  	ar2, #0x0BB8
06E2 19 5E       	lrri 	ac0.m, @ar2
06E3 2E D1       	srs  	$(ACFMT), ac0.m
06E4 19 5E       	lrri 	ac0.m, @ar2
06E5 2E D4       	srs  	$(ACSAH), ac0.m
06E6 19 5E       	lrri 	ac0.m, @ar2
06E7 2E D5       	srs  	$(ACSAL), ac0.m
06E8 19 5E       	lrri 	ac0.m, @ar2
06E9 2E D6       	srs  	$(ACEAH), ac0.m
06EA 19 5E       	lrri 	ac0.m, @ar2
06EB 2E D7       	srs  	$(ACEAL), ac0.m
06EC 19 5E       	lrri 	ac0.m, @ar2
06ED 2E D8       	srs  	$(ACCAH), ac0.m
06EE 19 5E       	lrri 	ac0.m, @ar2
06EF 2E D9       	srs  	$(ACCAL), ac0.m
06F0 19 5E       	lrri 	ac0.m, @ar2
06F1 2E A0       	srs  	$(ADPCM_A00), ac0.m
06F2 19 5E       	lrri 	ac0.m, @ar2
06F3 2E A1       	srs  	$(ADPCM_A10), ac0.m
06F4 19 5E       	lrri 	ac0.m, @ar2
06F5 2E A2       	srs  	$(ADPCM_A20), ac0.m
06F6 19 5E       	lrri 	ac0.m, @ar2
06F7 2E A3       	srs  	$(ADPCM_A30), ac0.m
06F8 19 5E       	lrri 	ac0.m, @ar2
06F9 2E A4       	srs  	$(ADPCM_A40), ac0.m
06FA 19 5E       	lrri 	ac0.m, @ar2
06FB 2E A5       	srs  	$(ADPCM_A50), ac0.m
06FC 19 5E       	lrri 	ac0.m, @ar2
06FD 2E A6       	srs  	$(ADPCM_A60), ac0.m
06FE 19 5E       	lrri 	ac0.m, @ar2
06FF 2E A7       	srs  	$(ADPCM_A70), ac0.m
0700 19 5E       	lrri 	ac0.m, @ar2
0701 2E A8       	srs  	$(ADPCM_A01), ac0.m
0702 19 5E       	lrri 	ac0.m, @ar2
0703 2E A9       	srs  	$(ADPCM_A11), ac0.m
0704 19 5E       	lrri 	ac0.m, @ar2
0705 2E AA       	srs  	$(ADPCM_A21), ac0.m
0706 19 5E       	lrri 	ac0.m, @ar2
0707 2E AB       	srs  	$(ADPCM_A31), ac0.m
0708 19 5E       	lrri 	ac0.m, @ar2
0709 2E AC       	srs  	$(ADPCM_A41), ac0.m
070A 19 5E       	lrri 	ac0.m, @ar2
070B 2E AD       	srs  	$(ADPCM_A51), ac0.m
070C 19 5E       	lrri 	ac0.m, @ar2
070D 2E AE       	srs  	$(ADPCM_A61), ac0.m
070E 19 5E       	lrri 	ac0.m, @ar2
070F 2E AF       	srs  	$(ADPCM_A71), ac0.m
0710 19 5E       	lrri 	ac0.m, @ar2
0711 2E DE       	srs  	$(ACGAN), ac0.m
0712 19 5E       	lrri 	ac0.m, @ar2
0713 2E DA       	srs  	$(ACPDS), ac0.m
0714 19 5E       	lrri 	ac0.m, @ar2
0715 2E DB       	srs  	$(ACYN1), ac0.m
0716 19 5E       	lrri 	ac0.m, @ar2
0717 2E DC       	srs  	$(ACYN2), ac0.m
0718 8C 00       	clr15	                	     	
0719 8A 00       	m2   	                	     	
071A 8E 00       	clr40	                	     	
071B 00 D8 0E 16 	lr   	ax0.l, $0x0E16
071D 19 5B       	lrri 	ax1.h, @ar2
071E 19 59       	lrri 	ax1.l, @ar2
071F 81 00       	clr  	ac0             	     	
0720 19 5C       	lrri 	ac0.l, @ar2
0721 00 80 0E 48 	lri  	ar0, #0x0E48
0723 19 5F       	lrri 	ac1.m, @ar2
0724 1B 1F       	srri 	@ar0, ac1.m
0725 19 5F       	lrri 	ac1.m, @ar2
0726 1B 1F       	srri 	@ar0, ac1.m
0727 19 5F       	lrri 	ac1.m, @ar2
0728 1B 1F       	srri 	@ar0, ac1.m
0729 18 5F       	lrr  	ac1.m, @ar2
072A 1B 1F       	srri 	@ar0, ac1.m
072B 6B 00       	movax	ac1, ax1        	     	
072C 15 05       	lsl  	ac1, #0x05
072D 4D 00       	add  	ac1, ac0        	     	
072E 15 7E       	lsr  	ac1, -2
072F 1C 9F       	mrr  	ix0, ac1.m
0730 1C BD       	mrr  	ix1, ac1.l
0731 05 E0       	addis	ac1.m, -32
0732 99 00       	asr16	ac1             	     	
0733 7D 00       	neg  	ac1             	     	
0734 1C DD       	mrr  	ix2, ac1.l
0735 89 00       	clr  	ac1             	     	
0736 1F A5       	mrr  	ac1.l, ix1
0737 15 02       	lsl  	ac1, #0x02
0738 1C BF       	mrr  	ix1, ac1.m
0739 00 9A 01 FC 	lri  	ax0.h, #0x01FC
073B 00 9E 0E 48 	lri  	ac0.m, #0x0E48
073D 00 81 FF DD 	lri  	ar1, #0xFFDD
073F 00 83 0D 80 	lri  	ar3, #0x0D80
0741 00 64 07 52 	bloop	ix0, $0x0752
0743 18 27       	lrr  	ix3, @ar1
0744 1B 07       	srri 	@ar0, ix3
0745 4A 00       	addax	ac0, ax1        	     	
0746 1F FC       	mrr  	ac1.m, ac0.l
0747 18 27       	lrr  	ix3, @ar1
0748 1B 07       	srri 	@ar0, ix3
0749 15 79       	lsr  	ac1, -7
074A 35 00       	andr 	ac1.m, ax0.h    	     	
074B 18 27       	lrr  	ix3, @ar1
074C 1B 07       	srri 	@ar0, ix3
074D 41 00       	addr 	ac1, ax0.l      	     	
074E 1B 7E       	srri 	@ar3, ac0.m
074F 18 27       	lrr  	ix3, @ar1
0750 1B 07       	srri 	@ar0, ix3
0751 1B 7F       	srri 	@ar3, ac1.m
0752 00 00       	nop  	
0753 00 65 07 58 	bloop	ix1, $0x0758
0755 18 27       	lrr  	ix3, @ar1
0756 1B 07       	srri 	@ar0, ix3
0757 00 00       	nop  	
0758 00 00       	nop  	
0759 00 07       	dar  	ar3
075A 18 7F       	lrr  	ac1.m, @ar3
075B 00 66 07 61 	bloop	ix2, $0x0761
075D 4A 3B       	addax	ac0, ax1        	s    	@ar3, ac1.m
075E 1F FC       	mrr  	ac1.m, ac0.l
075F 15 79       	lsr  	ac1, -7
0760 35 33       	andr 	ac1.m, ax0.h    	s    	@ar3, ac0.m
0761 41 00       	addr 	ac1, ax0.l      	     	
0762 1B 7F       	srri 	@ar3, ac1.m
0763 00 04       	dar  	ar0
0764 18 9F       	lrrd 	ac1.m, @ar0
0765 1A DF       	srrd 	@ar2, ac1.m
0766 18 9F       	lrrd 	ac1.m, @ar0
0767 1A DF       	srrd 	@ar2, ac1.m
0768 18 9F       	lrrd 	ac1.m, @ar0
0769 1A DF       	srrd 	@ar2, ac1.m
076A 18 9F       	lrrd 	ac1.m, @ar0
076B 1A DF       	srrd 	@ar2, ac1.m
076C 1A DC       	srrd 	@ar2, ac0.l
076D 00 82 0B D2 	lri  	ar2, #0x0BD2
076F 27 DC       	lrs  	ac1.m, $(ACYN2)
0770 1A DF       	srrd 	@ar2, ac1.m
0771 27 DB       	lrs  	ac1.m, $(ACYN1)
0772 1A DF       	srrd 	@ar2, ac1.m
0773 27 DA       	lrs  	ac1.m, $(ACPDS)
0774 1A DF       	srrd 	@ar2, ac1.m
0775 00 82 0B BE 	lri  	ar2, #0x0BBE
0777 27 D9       	lrs  	ac1.m, $(ACCAL)
0778 1A DF       	srrd 	@ar2, ac1.m
0779 27 D8       	lrs  	ac1.m, $(ACCAH)
077A 1A DF       	srrd 	@ar2, ac1.m
077B 8F 00       	set40	                	     	
077C 00 C1 0E 42 	lr   	ar1, $0x0E42
077E 00 82 0D 80 	lri  	ar2, #0x0D80
0780 19 40       	lrri 	ar0, @ar2
0781 19 43       	lrri 	ar3, @ar2
0782 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
0783 B8 C0       	mulx 	ax0.h, ax1.h    	ld   	ax0.l, ax1.l, @ar0
0784 11 1F 07 8C 	bloopi	#0x1F, $0x078C
0786 A6 F0       	mulxmv	ax0.l, ax1.l, ac0	ld   	ax0.h, ax1.h, @ar0
0787 BC F0       	mulxac	ax0.h, ax1.h, ac0	ld   	ax0.h, ax1.h, @ar0
0788 19 40       	lrri 	ar0, @ar2
0789 19 43       	lrri 	ar3, @ar2
078A BC F0       	mulxac	ax0.h, ax1.h, ac0	ld   	ax0.h, ax1.h, @ar0
078B 4E C0       	addp 	ac0             	ld   	ax0.l, ax1.l, @ar0
078C B8 31       	mulx 	ax0.h, ax1.h    	s    	@ar1, ac0.m
078D A6 F0       	mulxmv	ax0.l, ax1.l, ac0	ld   	ax0.h, ax1.h, @ar0
078E BC F0       	mulxac	ax0.h, ax1.h, ac0	ld   	ax0.h, ax1.h, @ar0
078F BC 00       	mulxac	ax0.h, ax1.h, ac0	     	
0790 4E 00       	addp 	ac0             	     	
0791 1B 3E       	srri 	@ar1, ac0.m
0792 00 E1 0E 42 	sr   	$0x0E42, ar1
0794 02 DF       	ret  	
```

```
0795 00 82 0B B8 	lri  	ar2, #0x0BB8
0797 19 5E       	lrri 	ac0.m, @ar2
0798 2E D1       	srs  	$(ACFMT), ac0.m
0799 19 5E       	lrri 	ac0.m, @ar2
079A 2E D4       	srs  	$(ACSAH), ac0.m
079B 19 5E       	lrri 	ac0.m, @ar2
079C 2E D5       	srs  	$(ACSAL), ac0.m
079D 19 5E       	lrri 	ac0.m, @ar2
079E 2E D6       	srs  	$(ACEAH), ac0.m
079F 19 5E       	lrri 	ac0.m, @ar2
07A0 2E D7       	srs  	$(ACEAL), ac0.m
07A1 19 5E       	lrri 	ac0.m, @ar2
07A2 2E D8       	srs  	$(ACCAH), ac0.m
07A3 19 5E       	lrri 	ac0.m, @ar2
07A4 2E D9       	srs  	$(ACCAL), ac0.m
07A5 19 5E       	lrri 	ac0.m, @ar2
07A6 2E A0       	srs  	$(ADPCM_A00), ac0.m
07A7 19 5E       	lrri 	ac0.m, @ar2
07A8 2E A1       	srs  	$(ADPCM_A10), ac0.m
07A9 19 5E       	lrri 	ac0.m, @ar2
07AA 2E A2       	srs  	$(ADPCM_A20), ac0.m
07AB 19 5E       	lrri 	ac0.m, @ar2
07AC 2E A3       	srs  	$(ADPCM_A30), ac0.m
07AD 19 5E       	lrri 	ac0.m, @ar2
07AE 2E A4       	srs  	$(ADPCM_A40), ac0.m
07AF 19 5E       	lrri 	ac0.m, @ar2
07B0 2E A5       	srs  	$(ADPCM_A50), ac0.m
07B1 19 5E       	lrri 	ac0.m, @ar2
07B2 2E A6       	srs  	$(ADPCM_A60), ac0.m
07B3 19 5E       	lrri 	ac0.m, @ar2
07B4 2E A7       	srs  	$(ADPCM_A70), ac0.m
07B5 19 5E       	lrri 	ac0.m, @ar2
07B6 2E A8       	srs  	$(ADPCM_A01), ac0.m
07B7 19 5E       	lrri 	ac0.m, @ar2
07B8 2E A9       	srs  	$(ADPCM_A11), ac0.m
07B9 19 5E       	lrri 	ac0.m, @ar2
07BA 2E AA       	srs  	$(ADPCM_A21), ac0.m
07BB 19 5E       	lrri 	ac0.m, @ar2
07BC 2E AB       	srs  	$(ADPCM_A31), ac0.m
07BD 19 5E       	lrri 	ac0.m, @ar2
07BE 2E AC       	srs  	$(ADPCM_A41), ac0.m
07BF 19 5E       	lrri 	ac0.m, @ar2
07C0 2E AD       	srs  	$(ADPCM_A51), ac0.m
07C1 19 5E       	lrri 	ac0.m, @ar2
07C2 2E AE       	srs  	$(ADPCM_A61), ac0.m
07C3 19 5E       	lrri 	ac0.m, @ar2
07C4 2E AF       	srs  	$(ADPCM_A71), ac0.m
07C5 19 5E       	lrri 	ac0.m, @ar2
07C6 2E DE       	srs  	$(ACGAN), ac0.m
07C7 19 5E       	lrri 	ac0.m, @ar2
07C8 2E DA       	srs  	$(ACPDS), ac0.m
07C9 19 5E       	lrri 	ac0.m, @ar2
07CA 2E DB       	srs  	$(ACYN1), ac0.m
07CB 19 5E       	lrri 	ac0.m, @ar2
07CC 2E DC       	srs  	$(ACYN2), ac0.m
07CD 8C 00       	clr15	                	     	
07CE 8A 00       	m2   	                	     	
07CF 8E 00       	clr40	                	     	
07D0 19 5B       	lrri 	ax1.h, @ar2
07D1 19 59       	lrri 	ax1.l, @ar2
07D2 81 00       	clr  	ac0             	     	
07D3 19 5C       	lrri 	ac0.l, @ar2
07D4 00 80 0E 48 	lri  	ar0, #0x0E48
07D6 19 5F       	lrri 	ac1.m, @ar2
07D7 19 5F       	lrri 	ac1.m, @ar2
07D8 19 5F       	lrri 	ac1.m, @ar2
07D9 1B 1F       	srri 	@ar0, ac1.m
07DA 18 5F       	lrr  	ac1.m, @ar2
07DB 1B 1F       	srri 	@ar0, ac1.m
07DC 6B 00       	movax	ac1, ax1        	     	
07DD 15 05       	lsl  	ac1, #0x05
07DE 4D 00       	add  	ac1, ac0        	     	
07DF 15 7E       	lsr  	ac1, -2
07E0 1C 9F       	mrr  	ix0, ac1.m
07E1 1C BD       	mrr  	ix1, ac1.l
07E2 05 E0       	addis	ac1.m, -32
07E3 99 00       	asr16	ac1             	     	
07E4 7D 00       	neg  	ac1             	     	
07E5 1C DD       	mrr  	ix2, ac1.l
07E6 89 00       	clr  	ac1             	     	
07E7 1F A5       	mrr  	ac1.l, ix1
07E8 15 02       	lsl  	ac1, #0x02
07E9 1C BF       	mrr  	ix1, ac1.m
07EA 00 9A 01 FC 	lri  	ax0.h, #0x01FC
07EC 00 9E 0E 49 	lri  	ac0.m, #0x0E49
07EE 00 81 FF DD 	lri  	ar1, #0xFFDD
07F0 00 83 0D 80 	lri  	ar3, #0x0D80
07F2 00 64 08 03 	bloop	ix0, $0x0803
07F4 18 27       	lrr  	ix3, @ar1
07F5 1B 07       	srri 	@ar0, ix3
07F6 4A 00       	addax	ac0, ax1        	     	
07F7 1B 7E       	srri 	@ar3, ac0.m
07F8 18 27       	lrr  	ix3, @ar1
07F9 1B 07       	srri 	@ar0, ix3
07FA 1B 7C       	srri 	@ar3, ac0.l
07FB 00 00       	nop  	
07FC 18 27       	lrr  	ix3, @ar1
07FD 1B 07       	srri 	@ar0, ix3
07FE 00 00       	nop  	
07FF 00 00       	nop  	
0800 18 27       	lrr  	ix3, @ar1
0801 1B 07       	srri 	@ar0, ix3
0802 00 00       	nop  	
0803 00 00       	nop  	
0804 00 65 08 09 	bloop	ix1, $0x0809
0806 18 27       	lrr  	ix3, @ar1
0807 1B 07       	srri 	@ar0, ix3
0808 00 00       	nop  	
0809 00 00       	nop  	
080A 00 66 08 0E 	bloop	ix2, $0x080E
080C 4A 00       	addax	ac0, ax1        	     	
080D 1B 7E       	srri 	@ar3, ac0.m
080E 1B 7C       	srri 	@ar3, ac0.l
080F 00 04       	dar  	ar0
0810 18 9F       	lrrd 	ac1.m, @ar0
0811 1A DF       	srrd 	@ar2, ac1.m
0812 18 9F       	lrrd 	ac1.m, @ar0
0813 1A DF       	srrd 	@ar2, ac1.m
0814 18 9F       	lrrd 	ac1.m, @ar0
0815 1A DF       	srrd 	@ar2, ac1.m
0816 18 9F       	lrrd 	ac1.m, @ar0
0817 1A DF       	srrd 	@ar2, ac1.m
0818 1A DC       	srrd 	@ar2, ac0.l
0819 00 82 0B D2 	lri  	ar2, #0x0BD2
081B 27 DC       	lrs  	ac1.m, $(ACYN2)
081C 1A DF       	srrd 	@ar2, ac1.m
081D 27 DB       	lrs  	ac1.m, $(ACYN1)
081E 1A DF       	srrd 	@ar2, ac1.m
081F 27 DA       	lrs  	ac1.m, $(ACPDS)
0820 1A DF       	srrd 	@ar2, ac1.m
0821 00 82 0B BE 	lri  	ar2, #0x0BBE
0823 27 D9       	lrs  	ac1.m, $(ACCAL)
0824 1A DF       	srrd 	@ar2, ac1.m
0825 27 D8       	lrs  	ac1.m, $(ACCAH)
0826 1A DF       	srrd 	@ar2, ac1.m
0827 8D 00       	set15	                	     	
0828 8B 00       	m0   	                	     	
0829 8F 00       	set40	                	     	
082A 00 C1 0E 42 	lr   	ar1, $0x0E42
082C 00 82 0D 80 	lri  	ar2, #0x0D80
082E 81 00       	clr  	ac0             	     	
082F 11 20 08 3B 	bloopi	#0x20, $0x083B
0831 89 00       	clr  	ac1             	     	
0832 19 40       	lrri 	ar0, @ar2
0833 18 9E       	lrrd 	ac0.m, @ar0
0834 18 1B       	lrr  	ax1.h, @ar0
0835 19 9A       	lrrn 	ax0.h, @ar0
0836 54 00       	subr 	ac0, ax0.h      	     	
0837 1F 5E       	mrr  	ax0.h, ac0.m
0838 19 59       	lrri 	ax1.l, @ar2
0839 B0 00       	mulx 	ax0.h, ax1.l    	     	
083A FB 00       	addpaxz	ac1, ax1.h    	     	
083B 81 39       	clr  	ac0             	s    	@ar1, ac1.m
083C 00 E1 0E 42 	sr   	$0x0E42, ar1
083E 02 DF       	ret  	
083F 00 82 0B B8 	lri  	ar2, #0x0BB8
0841 19 5E       	lrri 	ac0.m, @ar2
0842 2E D1       	srs  	$(ACFMT), ac0.m
0843 19 5E       	lrri 	ac0.m, @ar2
0844 2E D4       	srs  	$(ACSAH), ac0.m
0845 19 5E       	lrri 	ac0.m, @ar2
0846 2E D5       	srs  	$(ACSAL), ac0.m
0847 19 5E       	lrri 	ac0.m, @ar2
0848 2E D6       	srs  	$(ACEAH), ac0.m
0849 19 5E       	lrri 	ac0.m, @ar2
084A 2E D7       	srs  	$(ACEAL), ac0.m
084B 19 5E       	lrri 	ac0.m, @ar2
084C 2E D8       	srs  	$(ACCAH), ac0.m
084D 19 5E       	lrri 	ac0.m, @ar2
084E 2E D9       	srs  	$(ACCAL), ac0.m
084F 19 5E       	lrri 	ac0.m, @ar2
0850 2E A0       	srs  	$(ADPCM_A00), ac0.m
0851 19 5E       	lrri 	ac0.m, @ar2
0852 2E A1       	srs  	$(ADPCM_A10), ac0.m
0853 19 5E       	lrri 	ac0.m, @ar2
0854 2E A2       	srs  	$(ADPCM_A20), ac0.m
0855 19 5E       	lrri 	ac0.m, @ar2
0856 2E A3       	srs  	$(ADPCM_A30), ac0.m
0857 19 5E       	lrri 	ac0.m, @ar2
0858 2E A4       	srs  	$(ADPCM_A40), ac0.m
0859 19 5E       	lrri 	ac0.m, @ar2
085A 2E A5       	srs  	$(ADPCM_A50), ac0.m
085B 19 5E       	lrri 	ac0.m, @ar2
085C 2E A6       	srs  	$(ADPCM_A60), ac0.m
085D 19 5E       	lrri 	ac0.m, @ar2
085E 2E A7       	srs  	$(ADPCM_A70), ac0.m
085F 19 5E       	lrri 	ac0.m, @ar2
0860 2E A8       	srs  	$(ADPCM_A01), ac0.m
0861 19 5E       	lrri 	ac0.m, @ar2
0862 2E A9       	srs  	$(ADPCM_A11), ac0.m
0863 19 5E       	lrri 	ac0.m, @ar2
0864 2E AA       	srs  	$(ADPCM_A21), ac0.m
0865 19 5E       	lrri 	ac0.m, @ar2
0866 2E AB       	srs  	$(ADPCM_A31), ac0.m
0867 19 5E       	lrri 	ac0.m, @ar2
0868 2E AC       	srs  	$(ADPCM_A41), ac0.m
0869 19 5E       	lrri 	ac0.m, @ar2
086A 2E AD       	srs  	$(ADPCM_A51), ac0.m
086B 19 5E       	lrri 	ac0.m, @ar2
086C 2E AE       	srs  	$(ADPCM_A61), ac0.m
086D 19 5E       	lrri 	ac0.m, @ar2
086E 2E AF       	srs  	$(ADPCM_A71), ac0.m
086F 19 5E       	lrri 	ac0.m, @ar2
0870 2E DE       	srs  	$(ACGAN), ac0.m
0871 19 5E       	lrri 	ac0.m, @ar2
0872 2E DA       	srs  	$(ACPDS), ac0.m
0873 19 5E       	lrri 	ac0.m, @ar2
0874 2E DB       	srs  	$(ACYN1), ac0.m
0875 19 5E       	lrri 	ac0.m, @ar2
0876 2E DC       	srs  	$(ACYN2), ac0.m
0877 00 C0 0E 42 	lr   	ar0, $0x0E42
0879 00 81 FF DD 	lri  	ar1, #0xFFDD
087B 11 20 08 80 	bloopi	#0x20, $0x0880
087D 18 24       	lrr  	ix0, @ar1
087E 1B 04       	srri 	@ar0, ix0
087F 00 00       	nop  	
0880 00 00       	nop  	
0881 00 E0 0E 42 	sr   	$0x0E42, ar0
0883 00 82 0B D9 	lri  	ar2, #0x0BD9
0885 00 04       	dar  	ar0
0886 18 9F       	lrrd 	ac1.m, @ar0
0887 1A DF       	srrd 	@ar2, ac1.m
0888 18 9F       	lrrd 	ac1.m, @ar0
0889 1A DF       	srrd 	@ar2, ac1.m
088A 18 9F       	lrrd 	ac1.m, @ar0
088B 1A DF       	srrd 	@ar2, ac1.m
088C 18 9F       	lrrd 	ac1.m, @ar0
088D 1A DF       	srrd 	@ar2, ac1.m
088E 89 00       	clr  	ac1             	     	
088F 1A DC       	srrd 	@ar2, ac0.l
0890 27 DC       	lrs  	ac1.m, $(ACYN2)
0891 00 FF 0B D2 	sr   	$0x0BD2, ac1.m
0893 27 DB       	lrs  	ac1.m, $(ACYN1)
0894 00 FF 0B D1 	sr   	$0x0BD1, ac1.m
0896 27 DA       	lrs  	ac1.m, $(ACPDS)
0897 00 FF 0B D0 	sr   	$0x0BD0, ac1.m
0899 27 D9       	lrs  	ac1.m, $(ACCAL)
089A 00 FF 0B BE 	sr   	$0x0BBE, ac1.m
089C 27 D8       	lrs  	ac1.m, $(ACCAH)
089D 00 FF 0B BD 	sr   	$0x0BBD, ac1.m
089F 02 DF       	ret  	
08A0 02 DF       	ret  	
08A1 00 C0 0E 40 	lr   	ar0, $0x0E40
08A3 00 81 0B 89 	lri  	ar1, #0x0B89
08A5 00 C2 0E 08 	lr   	ar2, $0x0E08
08A7 1C 62       	mrr  	ar3, ar2
08A8 02 BF 81 F9 	call 	$0x81F9
08AA 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
08AC 02 DF       	ret  	
08AD 00 C0 0E 41 	lr   	ar0, $0x0E41
08AF 00 81 0B 8B 	lri  	ar1, #0x0B8B
08B1 00 C2 0E 09 	lr   	ar2, $0x0E09
08B3 1C 62       	mrr  	ar3, ar2
08B4 02 BF 81 F9 	call 	$0x81F9
08B6 00 F8 0B AC 	sr   	$0x0BAC, ax0.l
08B8 02 DF       	ret  	
```

```
08B9 00 C0 0E 40 	lr   	ar0, $0x0E40 					// 0x0CE0 ...
08BB 00 81 0B 89 	lri  	ar1, #0x0B89 					// AXPBMIX.vL / vDeltaL / vR / vDeltaR
08BD 00 C2 0E 08 	lr   	ar2, $0x0E08 					// Left channel samples ptr
08BF 1C 62       	mrr  	ar3, ar2
08C0 00 C4 0E 41 	lr   	ix0, $0x0E41 					// 0x0CE0 ...
08C2 00 C5 0E 09 	lr   	ix1, $0x0E09 					// Right channel samples ptr
08C4 02 BF 80 E7 	call 	$0x80E7
08C6 00 F8 0B A9 	sr   	$0x0BA9, ax0.l 					// AXPBDPOP.aL
08C8 00 FB 0B AC 	sr   	$0x0BAC, ax1.h 					// AXPBDPOP.aR
08CA 02 DF       	ret  	
```

```
08CB 00 C0 0E 43 	lr   	ar0, $0x0E43
08CD 00 81 0B 97 	lri  	ar1, #0x0B97
08CF 00 C2 0E 0A 	lr   	ar2, $0x0E0A
08D1 1C 62       	mrr  	ar3, ar2
08D2 02 BF 81 F9 	call 	$0x81F9
08D4 00 F8 0B AF 	sr   	$0x0BAF, ax0.l
08D6 02 DF       	ret  	
08D7 00 C0 0E 40 	lr   	ar0, $0x0E40
08D9 00 81 0B 89 	lri  	ar1, #0x0B89
08DB 00 C2 0E 08 	lr   	ar2, $0x0E08
08DD 1C 62       	mrr  	ar3, ar2
08DE 02 BF 81 F9 	call 	$0x81F9
08E0 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
08E2 00 C0 0E 43 	lr   	ar0, $0x0E43
08E4 00 81 0B 97 	lri  	ar1, #0x0B97
08E6 00 C2 0E 0A 	lr   	ar2, $0x0E0A
08E8 1C 62       	mrr  	ar3, ar2
08E9 02 BF 81 F9 	call 	$0x81F9
08EB 00 F8 0B AF 	sr   	$0x0BAF, ax0.l
08ED 02 DF       	ret  	
08EE 00 C0 0E 41 	lr   	ar0, $0x0E41
08F0 00 81 0B 8B 	lri  	ar1, #0x0B8B
08F2 00 C2 0E 09 	lr   	ar2, $0x0E09
08F4 1C 62       	mrr  	ar3, ar2
08F5 02 BF 81 F9 	call 	$0x81F9
08F7 00 F8 0B AC 	sr   	$0x0BAC, ax0.l
08F9 00 C0 0E 43 	lr   	ar0, $0x0E43
08FB 00 81 0B 97 	lri  	ar1, #0x0B97
08FD 00 C2 0E 0A 	lr   	ar2, $0x0E0A
08FF 1C 62       	mrr  	ar3, ar2
0900 02 BF 81 F9 	call 	$0x81F9
0902 00 F8 0B AF 	sr   	$0x0BAF, ax0.l
0904 02 DF       	ret  	
0905 00 C0 0E 40 	lr   	ar0, $0x0E40
0907 00 81 0B 89 	lri  	ar1, #0x0B89
0909 00 C2 0E 08 	lr   	ar2, $0x0E08
090B 1C 62       	mrr  	ar3, ar2
090C 00 C4 0E 41 	lr   	ix0, $0x0E41
090E 00 C5 0E 09 	lr   	ix1, $0x0E09
0910 02 BF 80 E7 	call 	$0x80E7
0912 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
0914 00 FB 0B AC 	sr   	$0x0BAC, ax1.h
0916 00 C0 0E 43 	lr   	ar0, $0x0E43
0918 00 81 0B 97 	lri  	ar1, #0x0B97
091A 00 C2 0E 0A 	lr   	ar2, $0x0E0A
091C 1C 62       	mrr  	ar3, ar2
091D 02 BF 81 F9 	call 	$0x81F9
091F 00 F8 0B AF 	sr   	$0x0BAF, ax0.l
0921 02 DF       	ret  	
0922 00 C0 0E 40 	lr   	ar0, $0x0E40
0924 00 81 0B 89 	lri  	ar1, #0x0B89
0926 00 C2 0E 08 	lr   	ar2, $0x0E08
0928 00 83 0E 48 	lri  	ar3, #0x0E48
092A 02 BF 84 5D 	call 	$0x845D
092C 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
092E 02 DF       	ret  	
092F 00 C0 0E 41 	lr   	ar0, $0x0E41
0931 00 81 0B 8B 	lri  	ar1, #0x0B8B
0933 00 C2 0E 09 	lr   	ar2, $0x0E09
0935 00 83 0E 48 	lri  	ar3, #0x0E48
0937 02 BF 84 5D 	call 	$0x845D
0939 00 F8 0B AC 	sr   	$0x0BAC, ax0.l
093B 02 DF       	ret  	
093C 00 C0 0E 40 	lr   	ar0, $0x0E40
093E 00 81 0B 89 	lri  	ar1, #0x0B89
0940 00 C2 0E 08 	lr   	ar2, $0x0E08
0942 00 83 0E 48 	lri  	ar3, #0x0E48
0944 00 C4 0E 41 	lr   	ix0, $0x0E41
0946 00 C5 0E 09 	lr   	ix1, $0x0E09
0948 02 BF 82 82 	call 	$0x8282
094A 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
094C 00 FB 0B AC 	sr   	$0x0BAC, ax1.h
094E 02 DF       	ret  	
094F 00 C0 0E 43 	lr   	ar0, $0x0E43
0951 00 81 0B 97 	lri  	ar1, #0x0B97
0953 00 C2 0E 0A 	lr   	ar2, $0x0E0A
0955 00 83 0E 48 	lri  	ar3, #0x0E48
0957 02 BF 84 5D 	call 	$0x845D
0959 00 F8 0B AF 	sr   	$0x0BAF, ax0.l
095B 02 DF       	ret  	
095C 00 C0 0E 40 	lr   	ar0, $0x0E40
095E 00 81 0B 89 	lri  	ar1, #0x0B89
0960 00 C2 0E 08 	lr   	ar2, $0x0E08
0962 00 83 0E 48 	lri  	ar3, #0x0E48
0964 02 BF 84 5D 	call 	$0x845D
0966 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
0968 00 C0 0E 43 	lr   	ar0, $0x0E43
096A 00 81 0B 97 	lri  	ar1, #0x0B97
096C 00 C2 0E 0A 	lr   	ar2, $0x0E0A
096E 00 83 0E 48 	lri  	ar3, #0x0E48
0970 02 BF 84 5D 	call 	$0x845D
0972 00 F8 0B AF 	sr   	$0x0BAF, ax0.l
0974 02 DF       	ret  	
0975 00 C0 0E 41 	lr   	ar0, $0x0E41
0977 00 81 0B 8B 	lri  	ar1, #0x0B8B
0979 00 C2 0E 09 	lr   	ar2, $0x0E09
097B 00 83 0E 48 	lri  	ar3, #0x0E48
097D 02 BF 84 5D 	call 	$0x845D
097F 00 F8 0B AC 	sr   	$0x0BAC, ax0.l
0981 00 C0 0E 43 	lr   	ar0, $0x0E43
0983 00 81 0B 97 	lri  	ar1, #0x0B97
0985 00 C2 0E 0A 	lr   	ar2, $0x0E0A
0987 00 83 0E 48 	lri  	ar3, #0x0E48
0989 02 BF 84 5D 	call 	$0x845D
098B 00 F8 0B AF 	sr   	$0x0BAF, ax0.l
098D 02 DF       	ret  	
098E 00 C0 0E 40 	lr   	ar0, $0x0E40
0990 00 81 0B 89 	lri  	ar1, #0x0B89
0992 00 C2 0E 08 	lr   	ar2, $0x0E08
0994 00 83 0E 48 	lri  	ar3, #0x0E48
0996 00 C4 0E 41 	lr   	ix0, $0x0E41
0998 00 C5 0E 09 	lr   	ix1, $0x0E09
099A 02 BF 82 82 	call 	$0x8282
099C 00 F8 0B A9 	sr   	$0x0BA9, ax0.l
099E 00 FB 0B AC 	sr   	$0x0BAC, ax1.h
09A0 00 C0 0E 43 	lr   	ar0, $0x0E43
09A2 00 81 0B 97 	lri  	ar1, #0x0B97
09A4 00 C2 0E 0A 	lr   	ar2, $0x0E0A
09A6 00 83 0E 48 	lri  	ar3, #0x0E48
09A8 02 BF 84 5D 	call 	$0x845D
09AA 00 F8 0B AF 	sr   	$0x0BAF, ax0.l
09AC 02 DF       	ret  	
09AD 00 C0 0E 40 	lr   	ar0, $0x0E40
09AF 00 81 0B 8D 	lri  	ar1, #0x0B8D
09B1 00 C2 0E 0B 	lr   	ar2, $0x0E0B
09B3 1C 62       	mrr  	ar3, ar2
09B4 02 BF 81 F9 	call 	$0x81F9
09B6 00 F8 0B AA 	sr   	$0x0BAA, ax0.l
09B8 02 DF       	ret  	
09B9 00 C0 0E 41 	lr   	ar0, $0x0E41
09BB 00 81 0B 8F 	lri  	ar1, #0x0B8F
09BD 00 C2 0E 0C 	lr   	ar2, $0x0E0C
09BF 1C 62       	mrr  	ar3, ar2
09C0 02 BF 81 F9 	call 	$0x81F9
09C2 00 F8 0B AD 	sr   	$0x0BAD, ax0.l
09C4 02 DF       	ret  	
09C5 00 C0 0E 40 	lr   	ar0, $0x0E40
09C7 00 81 0B 8D 	lri  	ar1, #0x0B8D
09C9 00 C2 0E 0B 	lr   	ar2, $0x0E0B
09CB 1C 62       	mrr  	ar3, ar2
09CC 00 C4 0E 41 	lr   	ix0, $0x0E41
09CE 00 C5 0E 0C 	lr   	ix1, $0x0E0C
09D0 02 BF 80 E7 	call 	$0x80E7
09D2 00 F8 0B AA 	sr   	$0x0BAA, ax0.l
09D4 00 FB 0B AD 	sr   	$0x0BAD, ax1.h
09D6 02 DF       	ret  	
09D7 00 C0 0E 40 	lr   	ar0, $0x0E40
09D9 00 81 0B 8D 	lri  	ar1, #0x0B8D
09DB 00 C2 0E 0B 	lr   	ar2, $0x0E0B
09DD 00 83 0E 48 	lri  	ar3, #0x0E48
09DF 02 BF 84 5D 	call 	$0x845D
09E1 00 F8 0B AA 	sr   	$0x0BAA, ax0.l
09E3 02 DF       	ret  	
09E4 00 C0 0E 41 	lr   	ar0, $0x0E41
09E6 00 81 0B 8F 	lri  	ar1, #0x0B8F
09E8 00 C2 0E 0C 	lr   	ar2, $0x0E0C
09EA 00 83 0E 48 	lri  	ar3, #0x0E48
09EC 02 BF 84 5D 	call 	$0x845D
09EE 00 F8 0B AD 	sr   	$0x0BAD, ax0.l
09F0 02 DF       	ret  	
09F1 00 C0 0E 40 	lr   	ar0, $0x0E40
09F3 00 81 0B 8D 	lri  	ar1, #0x0B8D
09F5 00 C2 0E 0B 	lr   	ar2, $0x0E0B
09F7 00 83 0E 48 	lri  	ar3, #0x0E48
09F9 00 C4 0E 41 	lr   	ix0, $0x0E41
09FB 00 C5 0E 0C 	lr   	ix1, $0x0E0C
09FD 02 BF 82 82 	call 	$0x8282
09FF 00 F8 0B AA 	sr   	$0x0BAA, ax0.l
0A01 00 FB 0B AD 	sr   	$0x0BAD, ax1.h
0A03 02 DF       	ret  	
0A04 00 C0 0E 43 	lr   	ar0, $0x0E43
0A06 00 81 0B 99 	lri  	ar1, #0x0B99
0A08 00 C2 0E 0D 	lr   	ar2, $0x0E0D
0A0A 1C 62       	mrr  	ar3, ar2
0A0B 02 BF 81 F9 	call 	$0x81F9
0A0D 00 F8 0B B0 	sr   	$0x0BB0, ax0.l
0A0F 02 DF       	ret  	
0A10 00 C0 0E 43 	lr   	ar0, $0x0E43
0A12 00 81 0B 99 	lri  	ar1, #0x0B99
0A14 00 C2 0E 0D 	lr   	ar2, $0x0E0D
0A16 1C 62       	mrr  	ar3, ar2
0A17 02 BF 81 F9 	call 	$0x81F9
0A19 00 F8 0B B0 	sr   	$0x0BB0, ax0.l
0A1B 02 9F 09 AD 	j    	$0x09AD
0A1D 00 C0 0E 43 	lr   	ar0, $0x0E43
0A1F 00 81 0B 99 	lri  	ar1, #0x0B99
0A21 00 C2 0E 0D 	lr   	ar2, $0x0E0D
0A23 1C 62       	mrr  	ar3, ar2
0A24 02 BF 81 F9 	call 	$0x81F9
0A26 00 F8 0B B0 	sr   	$0x0BB0, ax0.l
0A28 02 9F 09 B9 	j    	$0x09B9
0A2A 00 C0 0E 43 	lr   	ar0, $0x0E43
0A2C 00 81 0B 99 	lri  	ar1, #0x0B99
0A2E 00 C2 0E 0D 	lr   	ar2, $0x0E0D
0A30 1C 62       	mrr  	ar3, ar2
0A31 02 BF 81 F9 	call 	$0x81F9
0A33 00 F8 0B B0 	sr   	$0x0BB0, ax0.l
0A35 02 9F 09 C5 	j    	$0x09C5
0A37 00 C0 0E 43 	lr   	ar0, $0x0E43
0A39 00 81 0B 99 	lri  	ar1, #0x0B99
0A3B 00 C2 0E 0D 	lr   	ar2, $0x0E0D
0A3D 1C 62       	mrr  	ar3, ar2
0A3E 02 BF 81 F9 	call 	$0x81F9
0A40 00 F8 0B B0 	sr   	$0x0BB0, ax0.l
0A42 02 9F 09 D7 	j    	$0x09D7
0A44 00 C0 0E 43 	lr   	ar0, $0x0E43
0A46 00 81 0B 99 	lri  	ar1, #0x0B99
0A48 00 C2 0E 0D 	lr   	ar2, $0x0E0D
0A4A 1C 62       	mrr  	ar3, ar2
0A4B 02 BF 81 F9 	call 	$0x81F9
0A4D 00 F8 0B B0 	sr   	$0x0BB0, ax0.l
0A4F 02 9F 09 E4 	j    	$0x09E4
0A51 00 C0 0E 43 	lr   	ar0, $0x0E43
0A53 00 81 0B 99 	lri  	ar1, #0x0B99
0A55 00 C2 0E 0D 	lr   	ar2, $0x0E0D
0A57 1C 62       	mrr  	ar3, ar2
0A58 02 BF 81 F9 	call 	$0x81F9
0A5A 00 F8 0B B0 	sr   	$0x0BB0, ax0.l
0A5C 02 9F 09 F1 	j    	$0x09F1
0A5E 00 C0 0E 43 	lr   	ar0, $0x0E43
0A60 00 81 0B 99 	lri  	ar1, #0x0B99
0A62 00 C2 0E 0D 	lr   	ar2, $0x0E0D
0A64 00 83 0E 48 	lri  	ar3, #0x0E48
0A66 02 BF 84 5D 	call 	$0x845D
0A68 00 F8 0B B0 	sr   	$0x0BB0, ax0.l
0A6A 02 DF       	ret  	
0A6B 00 C0 0E 43 	lr   	ar0, $0x0E43
0A6D 00 81 0B 99 	lri  	ar1, #0x0B99
0A6F 00 C2 0E 0D 	lr   	ar2, $0x0E0D
0A71 00 83 0E 48 	lri  	ar3, #0x0E48
0A73 02 BF 84 5D 	call 	$0x845D
0A75 00 F8 0B B0 	sr   	$0x0BB0, ax0.l
0A77 02 9F 09 AD 	j    	$0x09AD
0A79 00 C0 0E 43 	lr   	ar0, $0x0E43
0A7B 00 81 0B 99 	lri  	ar1, #0x0B99
0A7D 00 C2 0E 0D 	lr   	ar2, $0x0E0D
0A7F 00 83 0E 48 	lri  	ar3, #0x0E48
0A81 02 BF 84 5D 	call 	$0x845D
0A83 00 F8 0B B0 	sr   	$0x0BB0, ax0.l
0A85 02 9F 09 B9 	j    	$0x09B9
0A87 00 C0 0E 43 	lr   	ar0, $0x0E43
0A89 00 81 0B 99 	lri  	ar1, #0x0B99
0A8B 00 C2 0E 0D 	lr   	ar2, $0x0E0D
0A8D 00 83 0E 48 	lri  	ar3, #0x0E48
0A8F 02 BF 84 5D 	call 	$0x845D
0A91 00 F8 0B B0 	sr   	$0x0BB0, ax0.l
0A93 02 9F 09 C5 	j    	$0x09C5
0A95 00 C0 0E 43 	lr   	ar0, $0x0E43
0A97 00 81 0B 99 	lri  	ar1, #0x0B99
0A99 00 C2 0E 0D 	lr   	ar2, $0x0E0D
0A9B 00 83 0E 48 	lri  	ar3, #0x0E48
0A9D 02 BF 84 5D 	call 	$0x845D
0A9F 00 F8 0B B0 	sr   	$0x0BB0, ax0.l
0AA1 02 9F 09 D7 	j    	$0x09D7
0AA3 00 C0 0E 43 	lr   	ar0, $0x0E43
0AA5 00 81 0B 99 	lri  	ar1, #0x0B99
0AA7 00 C2 0E 0D 	lr   	ar2, $0x0E0D
0AA9 00 83 0E 48 	lri  	ar3, #0x0E48
0AAB 02 BF 84 5D 	call 	$0x845D
0AAD 00 F8 0B B0 	sr   	$0x0BB0, ax0.l
0AAF 02 9F 09 E4 	j    	$0x09E4
0AB1 00 C0 0E 43 	lr   	ar0, $0x0E43
0AB3 00 81 0B 99 	lri  	ar1, #0x0B99
0AB5 00 C2 0E 0D 	lr   	ar2, $0x0E0D
0AB7 00 83 0E 48 	lri  	ar3, #0x0E48
0AB9 02 BF 84 5D 	call 	$0x845D
0ABB 00 F8 0B B0 	sr   	$0x0BB0, ax0.l
0ABD 02 9F 09 F1 	j    	$0x09F1
0ABF 00 C0 0E 40 	lr   	ar0, $0x0E40
0AC1 00 81 0B 91 	lri  	ar1, #0x0B91
0AC3 00 C2 0E 0E 	lr   	ar2, $0x0E0E
0AC5 1C 62       	mrr  	ar3, ar2
0AC6 02 BF 81 F9 	call 	$0x81F9
0AC8 00 F8 0B AB 	sr   	$0x0BAB, ax0.l
0ACA 02 DF       	ret  	
0ACB 00 C0 0E 41 	lr   	ar0, $0x0E41
0ACD 00 81 0B 93 	lri  	ar1, #0x0B93
0ACF 00 C2 0E 0F 	lr   	ar2, $0x0E0F
0AD1 1C 62       	mrr  	ar3, ar2
0AD2 02 BF 81 F9 	call 	$0x81F9
0AD4 00 F8 0B AE 	sr   	$0x0BAE, ax0.l
0AD6 02 DF       	ret  	
0AD7 00 C0 0E 40 	lr   	ar0, $0x0E40
0AD9 00 81 0B 91 	lri  	ar1, #0x0B91
0ADB 00 C2 0E 0E 	lr   	ar2, $0x0E0E
0ADD 1C 62       	mrr  	ar3, ar2
0ADE 00 C4 0E 41 	lr   	ix0, $0x0E41
0AE0 00 C5 0E 0F 	lr   	ix1, $0x0E0F
0AE2 02 BF 80 E7 	call 	$0x80E7
0AE4 00 F8 0B AB 	sr   	$0x0BAB, ax0.l
0AE6 00 FB 0B AE 	sr   	$0x0BAE, ax1.h
0AE8 02 DF       	ret  	
0AE9 00 C0 0E 40 	lr   	ar0, $0x0E40
0AEB 00 81 0B 91 	lri  	ar1, #0x0B91
0AED 00 C2 0E 0E 	lr   	ar2, $0x0E0E
0AEF 00 83 0E 48 	lri  	ar3, #0x0E48
0AF1 02 BF 84 5D 	call 	$0x845D
0AF3 00 F8 0B AB 	sr   	$0x0BAB, ax0.l
0AF5 02 DF       	ret  	
0AF6 00 C0 0E 41 	lr   	ar0, $0x0E41
0AF8 00 81 0B 93 	lri  	ar1, #0x0B93
0AFA 00 C2 0E 0F 	lr   	ar2, $0x0E0F
0AFC 00 83 0E 48 	lri  	ar3, #0x0E48
0AFE 02 BF 84 5D 	call 	$0x845D
0B00 00 F8 0B AE 	sr   	$0x0BAE, ax0.l
0B02 02 DF       	ret  	
0B03 00 C0 0E 40 	lr   	ar0, $0x0E40
0B05 00 81 0B 91 	lri  	ar1, #0x0B91
0B07 00 C2 0E 0E 	lr   	ar2, $0x0E0E
0B09 00 83 0E 48 	lri  	ar3, #0x0E48
0B0B 00 C4 0E 41 	lr   	ix0, $0x0E41
0B0D 00 C5 0E 0F 	lr   	ix1, $0x0E0F
0B0F 02 BF 82 82 	call 	$0x8282
0B11 00 F8 0B AB 	sr   	$0x0BAB, ax0.l
0B13 00 FB 0B AE 	sr   	$0x0BAE, ax1.h
0B15 02 DF       	ret  	
0B16 00 C0 0E 43 	lr   	ar0, $0x0E43
0B18 00 81 0B 95 	lri  	ar1, #0x0B95
0B1A 00 C2 0E 10 	lr   	ar2, $0x0E10
0B1C 1C 62       	mrr  	ar3, ar2
0B1D 02 BF 81 F9 	call 	$0x81F9
0B1F 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0B21 02 DF       	ret  	
0B22 00 C0 0E 43 	lr   	ar0, $0x0E43
0B24 00 81 0B 95 	lri  	ar1, #0x0B95
0B26 00 C2 0E 10 	lr   	ar2, $0x0E10
0B28 1C 62       	mrr  	ar3, ar2
0B29 02 BF 81 F9 	call 	$0x81F9
0B2B 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0B2D 02 9F 0A BF 	j    	$0x0ABF
0B2F 00 C0 0E 43 	lr   	ar0, $0x0E43
0B31 00 81 0B 95 	lri  	ar1, #0x0B95
0B33 00 C2 0E 10 	lr   	ar2, $0x0E10
0B35 1C 62       	mrr  	ar3, ar2
0B36 02 BF 81 F9 	call 	$0x81F9
0B38 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0B3A 02 9F 0A CB 	j    	$0x0ACB
0B3C 00 C0 0E 43 	lr   	ar0, $0x0E43
0B3E 00 81 0B 95 	lri  	ar1, #0x0B95
0B40 00 C2 0E 10 	lr   	ar2, $0x0E10
0B42 1C 62       	mrr  	ar3, ar2
0B43 02 BF 81 F9 	call 	$0x81F9
0B45 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0B47 02 9F 0A D7 	j    	$0x0AD7
0B49 00 C0 0E 43 	lr   	ar0, $0x0E43
0B4B 00 81 0B 95 	lri  	ar1, #0x0B95
0B4D 00 C2 0E 10 	lr   	ar2, $0x0E10
0B4F 1C 62       	mrr  	ar3, ar2
0B50 02 BF 81 F9 	call 	$0x81F9
0B52 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0B54 02 9F 0A E9 	j    	$0x0AE9
0B56 00 C0 0E 43 	lr   	ar0, $0x0E43
0B58 00 81 0B 95 	lri  	ar1, #0x0B95
0B5A 00 C2 0E 10 	lr   	ar2, $0x0E10
0B5C 1C 62       	mrr  	ar3, ar2
0B5D 02 BF 81 F9 	call 	$0x81F9
0B5F 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0B61 02 9F 0A F6 	j    	$0x0AF6
0B63 00 C0 0E 43 	lr   	ar0, $0x0E43
0B65 00 81 0B 95 	lri  	ar1, #0x0B95
0B67 00 C2 0E 10 	lr   	ar2, $0x0E10
0B69 1C 62       	mrr  	ar3, ar2
0B6A 02 BF 81 F9 	call 	$0x81F9
0B6C 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0B6E 02 9F 0B 03 	j    	$0x0B03
0B70 00 C0 0E 43 	lr   	ar0, $0x0E43
0B72 00 81 0B 95 	lri  	ar1, #0x0B95
0B74 00 C2 0E 10 	lr   	ar2, $0x0E10
0B76 00 83 0E 48 	lri  	ar3, #0x0E48
0B78 02 BF 84 5D 	call 	$0x845D
0B7A 02 DF       	ret  	
0B7B 00 C0 0E 43 	lr   	ar0, $0x0E43
0B7D 00 81 0B 95 	lri  	ar1, #0x0B95
0B7F 00 C2 0E 10 	lr   	ar2, $0x0E10
0B81 00 83 0E 48 	lri  	ar3, #0x0E48
0B83 02 BF 84 5D 	call 	$0x845D
0B85 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0B87 02 9F 0A BF 	j    	$0x0ABF
0B89 00 C0 0E 43 	lr   	ar0, $0x0E43
0B8B 00 81 0B 95 	lri  	ar1, #0x0B95
0B8D 00 C2 0E 10 	lr   	ar2, $0x0E10
0B8F 00 83 0E 48 	lri  	ar3, #0x0E48
0B91 02 BF 84 5D 	call 	$0x845D
0B93 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0B95 02 9F 0A CB 	j    	$0x0ACB
0B97 00 C0 0E 43 	lr   	ar0, $0x0E43
0B99 00 81 0B 95 	lri  	ar1, #0x0B95
0B9B 00 C2 0E 10 	lr   	ar2, $0x0E10
0B9D 00 83 0E 48 	lri  	ar3, #0x0E48
0B9F 02 BF 84 5D 	call 	$0x845D
0BA1 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0BA3 02 9F 0A D7 	j    	$0x0AD7
0BA5 00 C0 0E 43 	lr   	ar0, $0x0E43
0BA7 00 81 0B 95 	lri  	ar1, #0x0B95
0BA9 00 C2 0E 10 	lr   	ar2, $0x0E10
0BAB 00 83 0E 48 	lri  	ar3, #0x0E48
0BAD 02 BF 84 5D 	call 	$0x845D
0BAF 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0BB1 02 9F 0A E9 	j    	$0x0AE9
0BB3 00 C0 0E 43 	lr   	ar0, $0x0E43
0BB5 00 81 0B 95 	lri  	ar1, #0x0B95
0BB7 00 C2 0E 10 	lr   	ar2, $0x0E10
0BB9 00 83 0E 48 	lri  	ar3, #0x0E48
0BBB 02 BF 84 5D 	call 	$0x845D
0BBD 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0BBF 02 9F 0A F6 	j    	$0x0AF6
0BC1 00 C0 0E 43 	lr   	ar0, $0x0E43
0BC3 00 81 0B 95 	lri  	ar1, #0x0B95
0BC5 00 C2 0E 10 	lr   	ar2, $0x0E10
0BC7 00 83 0E 48 	lri  	ar3, #0x0E48
0BC9 02 BF 84 5D 	call 	$0x845D
0BCB 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0BCD 02 9F 0B 03 	j    	$0x0B03
0BCF 00 C0 0E 43 	lr   	ar0, $0x0E43
0BD1 00 81 0B 91 	lri  	ar1, #0x0B91
0BD3 00 C2 0E 0E 	lr   	ar2, $0x0E0E
0BD5 1C 62       	mrr  	ar3, ar2
0BD6 02 BF 81 F9 	call 	$0x81F9
0BD8 00 F8 0B AB 	sr   	$0x0BAB, ax0.l
0BDA 02 DF       	ret  	
0BDB 00 C0 0E 43 	lr   	ar0, $0x0E43
0BDD 00 81 0B 93 	lri  	ar1, #0x0B93
0BDF 00 C2 0E 0F 	lr   	ar2, $0x0E0F
0BE1 1C 62       	mrr  	ar3, ar2
0BE2 02 BF 81 F9 	call 	$0x81F9
0BE4 00 F8 0B AE 	sr   	$0x0BAE, ax0.l
0BE6 02 DF       	ret  	
0BE7 00 C0 0E 43 	lr   	ar0, $0x0E43
0BE9 00 81 0B 91 	lri  	ar1, #0x0B91
0BEB 00 C2 0E 0E 	lr   	ar2, $0x0E0E
0BED 1C 62       	mrr  	ar3, ar2
0BEE 00 C4 0E 43 	lr   	ix0, $0x0E43
0BF0 00 C5 0E 0F 	lr   	ix1, $0x0E0F
0BF2 02 BF 80 E7 	call 	$0x80E7
0BF4 00 F8 0B AB 	sr   	$0x0BAB, ax0.l
0BF6 00 FB 0B AE 	sr   	$0x0BAE, ax1.h
0BF8 02 DF       	ret  	
0BF9 00 C0 0E 43 	lr   	ar0, $0x0E43
0BFB 00 81 0B 91 	lri  	ar1, #0x0B91
0BFD 00 C2 0E 0E 	lr   	ar2, $0x0E0E
0BFF 00 83 0E 48 	lri  	ar3, #0x0E48
0C01 02 BF 84 5D 	call 	$0x845D
0C03 00 F8 0B AB 	sr   	$0x0BAB, ax0.l
0C05 02 DF       	ret  	
0C06 00 C0 0E 43 	lr   	ar0, $0x0E43
0C08 00 81 0B 93 	lri  	ar1, #0x0B93
0C0A 00 C2 0E 0F 	lr   	ar2, $0x0E0F
0C0C 00 83 0E 48 	lri  	ar3, #0x0E48
0C0E 02 BF 84 5D 	call 	$0x845D
0C10 00 F8 0B AE 	sr   	$0x0BAE, ax0.l
0C12 02 DF       	ret  	
0C13 00 C0 0E 43 	lr   	ar0, $0x0E43
0C15 00 81 0B 91 	lri  	ar1, #0x0B91
0C17 00 C2 0E 0E 	lr   	ar2, $0x0E0E
0C19 00 83 0E 48 	lri  	ar3, #0x0E48
0C1B 00 C4 0E 43 	lr   	ix0, $0x0E43
0C1D 00 C5 0E 0F 	lr   	ix1, $0x0E0F
0C1F 02 BF 82 82 	call 	$0x8282
0C21 00 F8 0B AB 	sr   	$0x0BAB, ax0.l
0C23 00 FB 0B AE 	sr   	$0x0BAE, ax1.h
0C25 02 DF       	ret  	
0C26 00 C0 0E 43 	lr   	ar0, $0x0E43
0C28 00 81 0B 95 	lri  	ar1, #0x0B95
0C2A 00 C2 0E 10 	lr   	ar2, $0x0E10
0C2C 1C 62       	mrr  	ar3, ar2
0C2D 02 BF 81 F9 	call 	$0x81F9
0C2F 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0C31 02 9F 0B CF 	j    	$0x0BCF
0C33 00 C0 0E 43 	lr   	ar0, $0x0E43
0C35 00 81 0B 95 	lri  	ar1, #0x0B95
0C37 00 C2 0E 10 	lr   	ar2, $0x0E10
0C39 1C 62       	mrr  	ar3, ar2
0C3A 02 BF 81 F9 	call 	$0x81F9
0C3C 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0C3E 02 9F 0B DB 	j    	$0x0BDB
0C40 00 C0 0E 43 	lr   	ar0, $0x0E43
0C42 00 81 0B 95 	lri  	ar1, #0x0B95
0C44 00 C2 0E 10 	lr   	ar2, $0x0E10
0C46 1C 62       	mrr  	ar3, ar2
0C47 02 BF 81 F9 	call 	$0x81F9
0C49 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0C4B 02 9F 0B E7 	j    	$0x0BE7
0C4D 00 C0 0E 43 	lr   	ar0, $0x0E43
0C4F 00 81 0B 95 	lri  	ar1, #0x0B95
0C51 00 C2 0E 10 	lr   	ar2, $0x0E10
0C53 1C 62       	mrr  	ar3, ar2
0C54 02 BF 81 F9 	call 	$0x81F9
0C56 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0C58 02 9F 0B F9 	j    	$0x0BF9
0C5A 00 C0 0E 43 	lr   	ar0, $0x0E43
0C5C 00 81 0B 95 	lri  	ar1, #0x0B95
0C5E 00 C2 0E 10 	lr   	ar2, $0x0E10
0C60 1C 62       	mrr  	ar3, ar2
0C61 02 BF 81 F9 	call 	$0x81F9
0C63 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0C65 02 9F 0C 06 	j    	$0x0C06
0C67 00 C0 0E 43 	lr   	ar0, $0x0E43
0C69 00 81 0B 95 	lri  	ar1, #0x0B95
0C6B 00 C2 0E 10 	lr   	ar2, $0x0E10
0C6D 1C 62       	mrr  	ar3, ar2
0C6E 02 BF 81 F9 	call 	$0x81F9
0C70 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0C72 02 9F 0C 13 	j    	$0x0C13
0C74 00 C0 0E 43 	lr   	ar0, $0x0E43
0C76 00 81 0B 95 	lri  	ar1, #0x0B95
0C78 00 C2 0E 10 	lr   	ar2, $0x0E10
0C7A 00 83 0E 48 	lri  	ar3, #0x0E48
0C7C 02 BF 84 5D 	call 	$0x845D
0C7E 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0C80 02 9F 0B CF 	j    	$0x0BCF
0C82 00 C0 0E 43 	lr   	ar0, $0x0E43
0C84 00 81 0B 95 	lri  	ar1, #0x0B95
0C86 00 C2 0E 10 	lr   	ar2, $0x0E10
0C88 00 83 0E 48 	lri  	ar3, #0x0E48
0C8A 02 BF 84 5D 	call 	$0x845D
0C8C 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0C8E 02 9F 0B DB 	j    	$0x0BDB
0C90 00 C0 0E 43 	lr   	ar0, $0x0E43
0C92 00 81 0B 95 	lri  	ar1, #0x0B95
0C94 00 C2 0E 10 	lr   	ar2, $0x0E10
0C96 00 83 0E 48 	lri  	ar3, #0x0E48
0C98 02 BF 84 5D 	call 	$0x845D
0C9A 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0C9C 02 9F 0B E7 	j    	$0x0BE7
0C9E 00 C0 0E 43 	lr   	ar0, $0x0E43
0CA0 00 81 0B 95 	lri  	ar1, #0x0B95
0CA2 00 C2 0E 10 	lr   	ar2, $0x0E10
0CA4 00 83 0E 48 	lri  	ar3, #0x0E48
0CA6 02 BF 84 5D 	call 	$0x845D
0CA8 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0CAA 02 9F 0B F9 	j    	$0x0BF9
0CAC 00 C0 0E 43 	lr   	ar0, $0x0E43
0CAE 00 81 0B 95 	lri  	ar1, #0x0B95
0CB0 00 C2 0E 10 	lr   	ar2, $0x0E10
0CB2 00 83 0E 48 	lri  	ar3, #0x0E48
0CB4 02 BF 84 5D 	call 	$0x845D
0CB6 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0CB8 02 9F 0C 06 	j    	$0x0C06
0CBA 00 C0 0E 43 	lr   	ar0, $0x0E43
0CBC 00 81 0B 95 	lri  	ar1, #0x0B95
0CBE 00 C2 0E 10 	lr   	ar2, $0x0E10
0CC0 00 83 0E 48 	lri  	ar3, #0x0E48
0CC2 02 BF 84 5D 	call 	$0x845D
0CC4 00 F8 0B B1 	sr   	$0x0BB1, ax0.l
0CC6 02 9F 0C 13 	j    	$0x0C13
0CC8 01 1C       	??? 011C
0CC9 01 D8       	??? 01D8
0CCA 02 56       	??? 0256
0CCB 02 FC       	??? 02FC
0CCC 05 4B       	addis	ac1.m, 75
0CCD 05 5F       	addis	ac1.m, 95
0CCE 01 FF       	??? 01FF
0CCF 06 AC       	cmpis	ac0.m, -84
0CD0 0D 52       	lris 	ac1.l, 82
0CD1 01 F9       	??? 01F9
0CD2 05 B0       	addis	ac1.m, -80
0CD3 05 AC       	addis	ac1.m, -84
0CD4 05 AE       	addis	ac1.m, -82
0CD5 02 43       	??? 0243
0CD6 05 73       	addis	ac1.m, 115
0CD7 05 B2       	addis	ac1.m, -78
0CD8 0D CC       	lris 	ac1.l, -52
0CD9 02 0F       	??? 020F
0CDA 00 82 0E 42 	lri  	ar2, #0x0E42
0CDC 08 A0       	lris 	ax0.l, -96
0CDD 08 A1       	lris 	ax0.l, -95
0CDE 08 AD       	lris 	ax0.l, -83
0CDF 08 B9       	lris 	ax0.l, -71
0CE0 08 CB       	lris 	ax0.l, -53
0CE1 08 D7       	lris 	ax0.l, -41
0CE2 08 EE       	lris 	ax0.l, -18
0CE3 09 05       	lris 	ax1.l, 5
0CE4 08 A0       	lris 	ax0.l, -96
0CE5 09 22       	lris 	ax1.l, 34
0CE6 09 2F       	lris 	ax1.l, 47
0CE7 09 3C       	lris 	ax1.l, 60
0CE8 09 4F       	lris 	ax1.l, 79
0CE9 09 5C       	lris 	ax1.l, 92
0CEA 09 75       	lris 	ax1.l, 117
0CEB 09 8E       	lris 	ax1.l, -114
0CEC 08 A0       	lris 	ax0.l, -96
0CED 09 AD       	lris 	ax1.l, -83
0CEE 09 B9       	lris 	ax1.l, -71
0CEF 09 C5       	lris 	ax1.l, -59
0CF0 08 A0       	lris 	ax0.l, -96
0CF1 09 D7       	lris 	ax1.l, -41
0CF2 09 E4       	lris 	ax1.l, -28
0CF3 09 F1       	lris 	ax1.l, -15
0CF4 0A 04       	lris 	ax0.h, 4
0CF5 0A 10       	lris 	ax0.h, 16
0CF6 0A 1D       	lris 	ax0.h, 29
0CF7 0A 2A       	lris 	ax0.h, 42
0CF8 0A 04       	lris 	ax0.h, 4
0CF9 0A 37       	lris 	ax0.h, 55
0CFA 0A 44       	lris 	ax0.h, 68
0CFB 0A 51       	lris 	ax0.h, 81
0CFC 08 A0       	lris 	ax0.l, -96
0CFD 09 AD       	lris 	ax1.l, -83
0CFE 09 B9       	lris 	ax1.l, -71
0CFF 09 C5       	lris 	ax1.l, -59
0D00 08 A0       	lris 	ax0.l, -96
0D01 09 D7       	lris 	ax1.l, -41
0D02 09 E4       	lris 	ax1.l, -28
0D03 09 F1       	lris 	ax1.l, -15
0D04 0A 5E       	lris 	ax0.h, 94
0D05 0A 6B       	lris 	ax0.h, 107
0D06 0A 79       	lris 	ax0.h, 121
0D07 0A 87       	lris 	ax0.h, -121
0D08 0A 5E       	lris 	ax0.h, 94
0D09 0A 95       	lris 	ax0.h, -107
0D0A 0A A3       	lris 	ax0.h, -93
0D0B 0A B1       	lris 	ax0.h, -79
0D0C 08 A0       	lris 	ax0.l, -96
0D0D 0A BF       	lris 	ax0.h, -65
0D0E 0A CB       	lris 	ax0.h, -53
0D0F 0A D7       	lris 	ax0.h, -41
0D10 08 A0       	lris 	ax0.l, -96
0D11 0A E9       	lris 	ax0.h, -23
0D12 0A F6       	lris 	ax0.h, -10
0D13 0B 03       	lris 	ax1.h, 3
0D14 0B 16       	lris 	ax1.h, 22
0D15 0B 22       	lris 	ax1.h, 34
0D16 0B 2F       	lris 	ax1.h, 47
0D17 0B 3C       	lris 	ax1.h, 60
0D18 0B 16       	lris 	ax1.h, 22
0D19 0B 49       	lris 	ax1.h, 73
0D1A 0B 56       	lris 	ax1.h, 86
0D1B 0B 63       	lris 	ax1.h, 99
0D1C 08 A0       	lris 	ax0.l, -96
0D1D 0A BF       	lris 	ax0.h, -65
0D1E 0A CB       	lris 	ax0.h, -53
0D1F 0A D7       	lris 	ax0.h, -41
0D20 08 A0       	lris 	ax0.l, -96
0D21 0A E9       	lris 	ax0.h, -23
0D22 0A F6       	lris 	ax0.h, -10
0D23 0B 03       	lris 	ax1.h, 3
0D24 0B 70       	lris 	ax1.h, 112
0D25 0B 7B       	lris 	ax1.h, 123
0D26 0B 89       	lris 	ax1.h, -119
0D27 0B 97       	lris 	ax1.h, -105
0D28 0B 70       	lris 	ax1.h, 112
0D29 0B A5       	lris 	ax1.h, -91
0D2A 0B B3       	lris 	ax1.h, -77
0D2B 0B C1       	lris 	ax1.h, -63
0D2C 08 A0       	lris 	ax0.l, -96
0D2D 0B CF       	lris 	ax1.h, -49
0D2E 0B DB       	lris 	ax1.h, -37
0D2F 0B E7       	lris 	ax1.h, -25
0D30 08 A0       	lris 	ax0.l, -96
0D31 0B F9       	lris 	ax1.h, -7
0D32 0C 06       	lris 	ac0.l, 6
0D33 0C 13       	lris 	ac0.l, 19
0D34 0B 16       	lris 	ax1.h, 22
0D35 0C 26       	lris 	ac0.l, 38
0D36 0C 33       	lris 	ac0.l, 51
0D37 0C 40       	lris 	ac0.l, 64
0D38 0B 16       	lris 	ax1.h, 22
0D39 0C 4D       	lris 	ac0.l, 77
0D3A 0C 5A       	lris 	ac0.l, 90
0D3B 0C 67       	lris 	ac0.l, 103
0D3C 08 A0       	lris 	ax0.l, -96
0D3D 0B CF       	lris 	ax1.h, -49
0D3E 0B DB       	lris 	ax1.h, -37
0D3F 0B E7       	lris 	ax1.h, -25
0D40 08 A0       	lris 	ax0.l, -96
0D41 0B F9       	lris 	ax1.h, -7
0D42 0C 06       	lris 	ac0.l, 6
0D43 0C 13       	lris 	ac0.l, 19
0D44 0B 70       	lris 	ax1.h, 112
0D45 0C 74       	lris 	ac0.l, 116
0D46 0C 82       	lris 	ac0.l, -126
0D47 0C 90       	lris 	ac0.l, -112
0D48 0B 70       	lris 	ax1.h, 112
0D49 0C 9E       	lris 	ac0.l, -98
0D4A 0C AC       	lris 	ac0.l, -84
0D4B 0C BA       	lris 	ac0.l, -70
0D4C 06 E0       	cmpis	ac0.m, -32
0D4D 07 95       	cmpis	ac1.m, -107
0D4E 08 3F       	lris 	ax0.l, 63
0D4F 10 00       	loopi	#0x00
0D50 12 00       	sbset	6
0D51 14 00       	lsl  	ac0, #0x00
0D52 8E 00       	clr40	                	     	
0D53 81 00       	clr  	ac0             	     	
0D54 89 70       	clr  	ac1             	l    	ac0.m, @ar0
0D55 19 1C       	lrri 	ac0.l, @ar0
0D56 2E CE       	srs  	$(DSMAH), ac0.m
0D57 2C CF       	srs  	$(DSMAL), ac0.l
0D58 16 CD 0E 80 	si   	$(DSPA), #0x0E80
0D5A 16 C9 00 00 	si   	$(DSCR), #0x0000
0D5C 16 CB 01 00 	si   	$(DSBL), #0x0100
0D5E 1F 7E       	mrr  	ax1.h, ac0.m
0D5F 1F 3C       	mrr  	ax1.l, ac0.l
0D60 81 00       	clr  	ac0             	     	
0D61 26 C9       	lrs  	ac0.m, $(DSCR)
0D62 02 A0 00 04 	tclr 	ac0.m, #0x0004
0D64 02 9C 0D 61 	jnok 	$0x0D61
0D66 19 1E       	lrri 	ac0.m, @ar0
0D67 19 1C       	lrri 	ac0.l, @ar0
0D68 2E CE       	srs  	$(DSMAH), ac0.m
0D69 2C CF       	srs  	$(DSMAL), ac0.l
0D6A 16 CD 02 80 	si   	$(DSPA), #0x0280
0D6C 16 C9 00 00 	si   	$(DSCR), #0x0000
0D6E 16 CB 02 80 	si   	$(DSBL), #0x0280
0D70 1C 80       	mrr  	ix0, ar0
0D71 00 80 02 80 	lri  	ar0, #0x0280
0D73 00 C1 0E 1B 	lr   	ar1, $0x0E1B
0D75 00 85 00 00 	lri  	ix1, #0x0000
0D77 00 89 00 7F 	lri  	lm1, #0x007F
0D79 00 82 0F 00 	lri  	ar2, #0x0F00
0D7B 00 83 16 B4 	lri  	ar3, #0x16B4
0D7D 1C E3       	mrr  	ix3, ar3
0D7E 81 00       	clr  	ac0             	     	
0D7F 26 C9       	lrs  	ac0.m, $(DSCR)
0D80 02 A0 00 04 	tclr 	ac0.m, #0x0004
0D82 02 9C 0D 7F 	jnok 	$0x0D7F
0D84 8F 00       	set40	                	     	
0D85 8A 78       	m2   	                	l    	ac1.m, @ar0
0D86 8C 68       	clr15	                	l    	ac1.l, @ar0
0D87 F1 00       	lsl16	ac1             	     	
0D88 1A 3F       	srr  	@ar1, ac1.m
0D89 84 E3       	clrp 	                	ldax 	ax0, @ar1
0D8A 10 7E       	loopi	#0x7E
0D8B F2 E3       	madd 	ax0.l, ax0.h    	ldax 	ax0, @ar1
0D8C F2 E7       	madd 	ax0.l, ax0.h    	ldaxn	ax0, @ar1
0D8D F2 78       	madd 	ax0.l, ax0.h    	l    	ac1.m, @ar0
0D8E 6E 68       	movp 	ac0             	l    	ac1.l, @ar0
0D8F F1 32       	lsl16	ac1             	s    	@ar2, ac0.m
0D90 1A 3F       	srr  	@ar1, ac1.m
0D91 11 9E 0D 9B 	bloopi	#0x9E, $0x0D9B
0D93 1C 67       	mrr  	ar3, ix3
0D94 84 E3       	clrp 	                	ldax 	ax0, @ar1
0D95 10 7E       	loopi	#0x7E
0D96 F2 E3       	madd 	ax0.l, ax0.h    	ldax 	ax0, @ar1
0D97 F2 E7       	madd 	ax0.l, ax0.h    	ldaxn	ax0, @ar1
0D98 F2 78       	madd 	ax0.l, ax0.h    	l    	ac1.m, @ar0
0D99 6E 68       	movp 	ac0             	l    	ac1.l, @ar0
0D9A F1 32       	lsl16	ac1             	s    	@ar2, ac0.m
0D9B 1A 3F       	srr  	@ar1, ac1.m
0D9C 1C 67       	mrr  	ar3, ix3
0D9D 84 E3       	clrp 	                	ldax 	ax0, @ar1
0D9E 10 7E       	loopi	#0x7E
0D9F F2 E3       	madd 	ax0.l, ax0.h    	ldax 	ax0, @ar1
0DA0 F2 E7       	madd 	ax0.l, ax0.h    	ldaxn	ax0, @ar1
0DA1 F2 00       	madd 	ax0.l, ax0.h    	     	
0DA2 6E 00       	movp 	ac0             	     	
0DA3 1B 5E       	srri 	@ar2, ac0.m
0DA4 00 E1 0E 1B 	sr   	$0x0E1B, ar1
0DA6 00 80 02 80 	lri  	ar0, #0x0280
0DA8 00 83 0F 00 	lri  	ar3, #0x0F00
0DAA 00 81 00 00 	lri  	ar1, #0x0000
0DAC 00 82 01 40 	lri  	ar2, #0x0140
0DAE 00 89 FF FF 	lri  	lm1, #0xFFFF
0DB0 89 00       	clr  	ac1             	     	
0DB1 81 00       	clr  	ac0             	     	
0DB2 8F 00       	set40	                	     	
0DB3 11 A0 0D BB 	bloopi	#0xA0, $0x0DBB
0DB5 19 7F       	lrri 	ac1.m, @ar3
0DB6 99 30       	asr16	ac1             	s    	@ar0, ac0.m
0DB7 1B 1E       	srri 	@ar0, ac0.m
0DB8 1B 3F       	srri 	@ar1, ac1.m
0DB9 7D 29       	neg  	ac1             	s    	@ar1, ac1.l
0DBA 1B 5F       	srri 	@ar2, ac1.m
0DBB 1B 5D       	srri 	@ar2, ac1.l
0DBC 8E 00       	clr40	                	     	
0DBD 1F DB       	mrr  	ac0.m, ax1.h
0DBE 1F 99       	mrr  	ac0.l, ax1.l
0DBF 2E CE       	srs  	$(DSMAH), ac0.m
0DC0 2C CF       	srs  	$(DSMAL), ac0.l
0DC1 16 CD 0E 80 	si   	$(DSPA), #0x0E80
0DC3 16 C9 00 01 	si   	$(DSCR), #0x0001
0DC5 16 CB 01 00 	si   	$(DSBL), #0x0100
0DC7 02 BF 06 94 	call 	$0x0694
0DC9 1C 04       	mrr  	ar0, ix0
0DCA 02 9F 00 68 	j    	$0x0068
0DCC 8E 00       	clr40	                	     	
0DCD 81 00       	clr  	ac0             	     	
0DCE 89 70       	clr  	ac1             	l    	ac0.m, @ar0
0DCF 19 1C       	lrri 	ac0.l, @ar0
0DD0 2E CE       	srs  	$(DSMAH), ac0.m
0DD1 2C CF       	srs  	$(DSMAL), ac0.l
0DD2 16 CD 07 C0 	si   	$(DSPA), #0x07C0
0DD4 16 C9 00 01 	si   	$(DSCR), #0x0001
0DD6 16 CB 05 00 	si   	$(DSBL), #0x0500
0DD8 02 BF 06 94 	call 	$0x0694
0DDA 81 00       	clr  	ac0             	     	
0DDB 89 70       	clr  	ac1             	l    	ac0.m, @ar0
0DDC 19 1C       	lrri 	ac0.l, @ar0
0DDD 2E CE       	srs  	$(DSMAH), ac0.m
0DDE 2C CF       	srs  	$(DSMAL), ac0.l
0DDF 16 CD 07 C0 	si   	$(DSPA), #0x07C0
0DE1 16 C9 00 00 	si   	$(DSCR), #0x0000
0DE3 89 00       	clr  	ac1             	     	
0DE4 0D 20       	lris 	ac1.l, 32
0DE5 2D CB       	srs  	$(DSBL), ac1.l
0DE6 4C 00       	add  	ac0, ac1        	     	
0DE7 1C 80       	mrr  	ix0, ar0
0DE8 00 80 07 C0 	lri  	ar0, #0x07C0
0DEA 00 83 00 00 	lri  	ar3, #0x0000
0DEC 1C 43       	mrr  	ar2, ar3
0DED 0A 00       	lris 	ax0.h, 0
0DEE 27 C9       	lrs  	ac1.m, $(DSCR)
0DEF 03 A0 00 04 	tclr 	ac1.m, #0x0004
0DF1 02 9C 0D EE 	jnok 	$0x0DEE
0DF3 2E CE       	srs  	$(DSMAH), ac0.m
0DF4 2C CF       	srs  	$(DSMAL), ac0.l
0DF5 16 CD 07 D0 	si   	$(DSPA), #0x07D0
0DF7 16 C9 00 00 	si   	$(DSCR), #0x0000
0DF9 16 CB 04 E0 	si   	$(DSBL), #0x04E0
0DFB 8F 00       	set40	                	     	
0DFC 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
0DFD 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
0DFE 6A 00       	movax	ac0, ax1        	     	
0DFF 48 00       	addax	ac0, ax0        	     	
0E00 11 4F 0E 09 	bloopi	#0x4F, $0x0E09
0E02 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
0E03 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
0E04 6B 32       	movax	ac1, ax1        	s    	@ar2, ac0.m
0E05 49 22       	addax	ac1, ax0        	s    	@ar2, ac0.l
0E06 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
0E07 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
0E08 6A 3A       	movax	ac0, ax1        	s    	@ar2, ac1.m
0E09 48 2A       	addax	ac0, ax0        	s    	@ar2, ac1.l
0E0A 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
0E0B 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
0E0C 6B 32       	movax	ac1, ax1        	s    	@ar2, ac0.m
0E0D 49 22       	addax	ac1, ax0        	s    	@ar2, ac0.l
0E0E 1B 5F       	srri 	@ar2, ac1.m
0E0F 1B 5D       	srri 	@ar2, ac1.l
0E10 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
0E11 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
0E12 68 00       	movax	ac0, ax0        	     	
0E13 7C 00       	neg  	ac0             	     	
0E14 4A 00       	addax	ac0, ax1        	     	
0E15 11 4F 0E 20 	bloopi	#0x4F, $0x0E20
0E17 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
0E18 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
0E19 69 32       	movax	ac1, ax0        	s    	@ar2, ac0.m
0E1A 7D 00       	neg  	ac1             	     	
0E1B 4B 22       	addax	ac1, ax1        	s    	@ar2, ac0.l
0E1C 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
0E1D 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
0E1E 68 3A       	movax	ac0, ax0        	s    	@ar2, ac1.m
0E1F 7C 00       	neg  	ac0             	     	
0E20 4A 2A       	addax	ac0, ax1        	s    	@ar2, ac1.l
0E21 80 F0       	nx   	                	ld   	ax0.h, ax1.h, @ar0
0E22 80 C0       	nx   	                	ld   	ax0.l, ax1.l, @ar0
0E23 69 32       	movax	ac1, ax0        	s    	@ar2, ac0.m
0E24 7D 00       	neg  	ac1             	     	
0E25 4B 22       	addax	ac1, ax1        	s    	@ar2, ac0.l
0E26 1B 5F       	srri 	@ar2, ac1.m
0E27 1B 5D       	srri 	@ar2, ac1.l
0E28 1C 04       	mrr  	ar0, ix0
0E29 02 9F 00 68 	j    	$0x0068
0E2B 8F 00       	set40	                	     	
0E2C 80 F1       	nx   	                	ld   	ax0.h, ax1.h, @ar1
0E2D 80 C1       	nx   	                	ld   	ax0.l, ax1.l, @ar1
0E2E 6A 00       	movax	ac0, ax1        	     	
0E2F 48 00       	addax	ac0, ax0        	     	
0E30 11 4F 0E 39 	bloopi	#0x4F, $0x0E39
0E32 80 F1       	nx   	                	ld   	ax0.h, ax1.h, @ar1
0E33 80 C1       	nx   	                	ld   	ax0.l, ax1.l, @ar1
0E34 6B 32       	movax	ac1, ax1        	s    	@ar2, ac0.m
0E35 49 22       	addax	ac1, ax0        	s    	@ar2, ac0.l
0E36 80 F1       	nx   	                	ld   	ax0.h, ax1.h, @ar1
0E37 80 C1       	nx   	                	ld   	ax0.l, ax1.l, @ar1
0E38 6A 3A       	movax	ac0, ax1        	s    	@ar2, ac1.m
0E39 48 2A       	addax	ac0, ax0        	s    	@ar2, ac1.l
0E3A 80 F1       	nx   	                	ld   	ax0.h, ax1.h, @ar1
0E3B 80 C1       	nx   	                	ld   	ax0.l, ax1.l, @ar1
0E3C 6B 32       	movax	ac1, ax1        	s    	@ar2, ac0.m
0E3D 49 22       	addax	ac1, ax0        	s    	@ar2, ac0.l
0E3E 1B 5F       	srri 	@ar2, ac1.m
0E3F 1B 5D       	srri 	@ar2, ac1.l
0E40 8E 00       	clr40	                	     	
0E41 02 DF       	ret  	
0E42 8E 00       	clr40	                	     	
0E43 81 00       	clr  	ac0             	     	
0E44 89 70       	clr  	ac1             	l    	ac0.m, @ar0
0E45 19 1C       	lrri 	ac0.l, @ar0
0E46 2E CE       	srs  	$(DSMAH), ac0.m
0E47 2C CF       	srs  	$(DSMAL), ac0.l
0E48 16 CD 04 00 	si   	$(DSPA), #0x0400
0E4A 16 C9 00 01 	si   	$(DSCR), #0x0001
0E4C 16 CB 07 80 	si   	$(DSBL), #0x0780
0E4E 02 BF 06 94 	call 	$0x0694
0E50 81 00       	clr  	ac0             	     	
0E51 89 70       	clr  	ac1             	l    	ac0.m, @ar0
0E52 19 1C       	lrri 	ac0.l, @ar0
0E53 2E CE       	srs  	$(DSMAH), ac0.m
0E54 2C CF       	srs  	$(DSMAL), ac0.l
0E55 16 CD 0A 40 	si   	$(DSPA), #0x0A40
0E57 16 C9 00 01 	si   	$(DSCR), #0x0001
0E59 16 CB 02 80 	si   	$(DSBL), #0x0280
0E5B 02 BF 06 94 	call 	$0x0694
0E5D 81 00       	clr  	ac0             	     	
0E5E 89 70       	clr  	ac1             	l    	ac0.m, @ar0
0E5F 19 1C       	lrri 	ac0.l, @ar0
0E60 2E CE       	srs  	$(DSMAH), ac0.m
0E61 2C CF       	srs  	$(DSMAL), ac0.l
0E62 16 CD 0E 48 	si   	$(DSPA), #0x0E48
0E64 16 C9 00 00 	si   	$(DSCR), #0x0000
0E66 16 CB 02 80 	si   	$(DSBL), #0x0280
0E68 00 81 0E 48 	lri  	ar1, #0x0E48
0E6A 00 82 00 00 	lri  	ar2, #0x0000
0E6C 00 83 00 00 	lri  	ar3, #0x0000
0E6E 02 BF 06 94 	call 	$0x0694
0E70 02 BF 0E 2B 	call 	$0x0E2B
0E72 81 00       	clr  	ac0             	     	
0E73 89 70       	clr  	ac1             	l    	ac0.m, @ar0
0E74 19 1C       	lrri 	ac0.l, @ar0
0E75 2E CE       	srs  	$(DSMAH), ac0.m
0E76 2C CF       	srs  	$(DSMAL), ac0.l
0E77 16 CD 0E 48 	si   	$(DSPA), #0x0E48
0E79 16 C9 00 00 	si   	$(DSCR), #0x0000
0E7B 16 CB 02 80 	si   	$(DSBL), #0x0280
0E7D 00 81 0E 48 	lri  	ar1, #0x0E48
0E7F 00 82 01 40 	lri  	ar2, #0x0140
0E81 00 83 01 40 	lri  	ar3, #0x0140
0E83 02 BF 06 94 	call 	$0x0694
0E85 02 BF 0E 2B 	call 	$0x0E2B
0E87 81 00       	clr  	ac0             	     	
0E88 89 70       	clr  	ac1             	l    	ac0.m, @ar0
0E89 19 1C       	lrri 	ac0.l, @ar0
0E8A 2E CE       	srs  	$(DSMAH), ac0.m
0E8B 2C CF       	srs  	$(DSMAL), ac0.l
0E8C 16 CD 0E 48 	si   	$(DSPA), #0x0E48
0E8E 16 C9 00 00 	si   	$(DSCR), #0x0000
0E90 16 CB 02 80 	si   	$(DSBL), #0x0280
0E92 00 81 0E 48 	lri  	ar1, #0x0E48
0E94 00 82 07 C0 	lri  	ar2, #0x07C0
0E96 00 83 07 C0 	lri  	ar3, #0x07C0
0E98 02 BF 06 94 	call 	$0x0694
0E9A 02 BF 0E 2B 	call 	$0x0E2B
0E9C 81 00       	clr  	ac0             	     	
0E9D 89 70       	clr  	ac1             	l    	ac0.m, @ar0
0E9E 19 1C       	lrri 	ac0.l, @ar0
0E9F 2E CE       	srs  	$(DSMAH), ac0.m
0EA0 2C CF       	srs  	$(DSMAL), ac0.l
0EA1 16 CD 0E 48 	si   	$(DSPA), #0x0E48
0EA3 16 C9 00 00 	si   	$(DSCR), #0x0000
0EA5 16 CB 02 80 	si   	$(DSBL), #0x0280
0EA7 00 81 0E 48 	lri  	ar1, #0x0E48
0EA9 00 82 09 00 	lri  	ar2, #0x0900
0EAB 00 83 09 00 	lri  	ar3, #0x0900
0EAD 02 BF 06 94 	call 	$0x0694
0EAF 02 BF 0E 2B 	call 	$0x0E2B
0EB1 02 9F 00 68 	j    	$0x0068
0EB3 8E 00       	clr40	                	     	
0EB4 16 FC EC C0 	si   	$(DMBH), #0xECC0
0EB6 1F CC       	mrr  	ac0.m, st0
0EB7 1D 9E       	mrr  	st0, ac0.m
0EB8 2E FD       	srs  	$(DMBL), ac0.m
0EB9 26 FC       	lrs  	ac0.m, $(DMBH)
0EBA 02 A0 80 00 	tclr 	ac0.m, #0x8000
0EBC 02 9C 0E B9 	jnok 	$0x0EB9
0EBE 00 00       	nop  	
0EBF 00 00       	nop  	
0EC0 00 00       	nop  	
0EC1 02 FF       	rti  	
0EC2 8E 00       	clr40	                	     	
0EC3 00 F0 0E 17 	sr   	$0x0E17, ac0.h
0EC5 00 FE 0E 18 	sr   	$0x0E18, ac0.m
0EC7 00 FC 0E 19 	sr   	$0x0E19, ac0.l
0EC9 1F CC       	mrr  	ac0.m, st0
0ECA 1D 9E       	mrr  	st0, ac0.m
0ECB 16 FC FE ED 	si   	$(DMBH), #0xFEED
0ECD 2E FD       	srs  	$(DMBL), ac0.m
0ECE 26 FC       	lrs  	ac0.m, $(DMBH)
0ECF 02 A0 80 00 	tclr 	ac0.m, #0x8000
0ED1 02 9C 0E CE 	jnok 	$0x0ECE
0ED3 00 D0 0E 17 	lr   	ac0.h, $0x0E17
0ED5 00 DE 0E 18 	lr   	ac0.m, $0x0E18
0ED7 00 DC 0E 19 	lr   	ac0.l, $0x0E19
0ED9 00 00       	nop  	
0EDA 00 00       	nop  	
0EDB 00 00       	nop  	
0EDC 00 00       	nop  	
0EDD 02 FF       	rti  	
0EDE 8E 00       	clr40	                	     	
0EDF 1D BC       	mrr  	st1, ac0.l
0EE0 1D BE       	mrr  	st1, ac0.m
0EE1 81 00       	clr  	ac0             	     	
0EE2 00 DE 0B B7 	lr   	ac0.m, $0x0BB7
0EE4 06 01       	cmpis	ac0.m, 1
0EE5 02 95 0E EA 	jeq  	$0x0EEA
0EE7 0E 00       	lris 	ac0.m, 0
0EE8 00 FE 0B 87 	sr   	$0x0B87, ac0.m
0EEA 1F CD       	mrr  	ac0.m, st1
0EEB 1F 8D       	mrr  	ac0.l, st1
0EEC 02 FF       	rti  	
0EED 00 00       	nop  	
0EEE 00 00       	nop  	
0EEF 00 00       	nop  	
0EF0 00 00       	nop  	
0EF1 00 00       	nop  	
0EF2 02 FF       	rti  	
0EF3 8E 00       	clr40	                	     	
0EF4 1D BC       	mrr  	st1, ac0.l
0EF5 1D BE       	mrr  	st1, ac0.m
0EF6 81 00       	clr  	ac0             	     	
0EF7 00 DE 0B B7 	lr   	ac0.m, $0x0BB7
0EF9 06 01       	cmpis	ac0.m, 1
0EFA 02 95 0F 02 	jeq  	$0x0F02
0EFC 0E 00       	lris 	ac0.m, 0
0EFD 00 FE 0B 87 	sr   	$0x0B87, ac0.m
0EFF 1F CD       	mrr  	ac0.m, st1
0F00 1F 8D       	mrr  	ac0.l, st1
0F01 02 FF       	rti  	
0F02 81 00       	clr  	ac0             	     	
0F03 00 DE 0B 88 	lr   	ac0.m, $0x0B88
0F05 06 01       	cmpis	ac0.m, 1
0F06 02 95 0F 14 	jeq  	$0x0F14
0F08 00 DE 0B DA 	lr   	ac0.m, $0x0BDA
0F0A 2E DA       	srs  	$(ACPDS), ac0.m
0F0B 00 DE 0B DB 	lr   	ac0.m, $0x0BDB
0F0D 2E DB       	srs  	$(ACYN1), ac0.m
0F0E 00 DE 0B DC 	lr   	ac0.m, $0x0BDC
0F10 2E DC       	srs  	$(ACYN2), ac0.m
0F11 1F CD       	mrr  	ac0.m, st1
0F12 1F 8D       	mrr  	ac0.l, st1
0F13 02 FF       	rti  	
0F14 00 DE 0B DA 	lr   	ac0.m, $0x0BDA
0F16 2E DA       	srs  	$(ACPDS), ac0.m
0F17 26 DB       	lrs  	ac0.m, $(ACYN1)
0F18 2E DB       	srs  	$(ACYN1), ac0.m
0F19 26 DC       	lrs  	ac0.m, $(ACYN2)
0F1A 2E DC       	srs  	$(ACYN2), ac0.m
0F1B 81 00       	clr  	ac0             	     	
0F1C 00 DC 0B E7 	lr   	ac0.l, $0x0BE7
0F1E 76 00       	inc  	ac0             	     	
0F1F 00 FC 0B E7 	sr   	$0x0BE7, ac0.l
0F21 81 00       	clr  	ac0             	     	
0F22 1F CD       	mrr  	ac0.m, st1
0F23 1F 8D       	mrr  	ac0.l, st1
0F24 02 FF       	rti  	
0F25 00 00       	nop  	
0F26 00 00       	nop  	
0F27 00 00       	nop  	
0F28 00 00       	nop  	
0F29 00 00       	nop  	
0F2A 02 FF       	rti  	
0F2B 00 00       	nop  	
0F2C 00 00       	nop  	
0F2D 00 00       	nop  	
0F2E 00 00       	nop  	
0F2F 02 FF       	rti  	
0F30 0F 42       	lris 	ac1.m, 66
0F31 0F 45       	lris 	ac1.m, 69
0F32 0F 7D       	lris 	ac1.m, 125
0F33 0F 80       	lris 	ac1.m, -128
0F34 8E 00       	clr40	                	     	
0F35 81 00       	clr  	ac0             	     	
0F36 89 00       	clr  	ac1             	     	
0F37 02 BF 0F 83 	call 	$0x0F83
0F39 27 FF       	lrs  	ac1.m, $(CMBL)
0F3A 00 9E 0F 30 	lri  	ac0.m, #0x0F30
0F3C 4C 00       	add  	ac0, ac1        	     	
0F3D 1C 7E       	mrr  	ar3, ac0.m
0F3E 03 13       	ilrr 	ac1.m, @ar3
0F3F 1C 7F       	mrr  	ar3, ac1.m
0F40 17 6F       	jmpr 	ar3
0F41 00 21       	halt 	
0F42 02 9F 00 30 	j    	$0x0030
0F44 00 21       	halt 	
0F45 81 00       	clr  	ac0             	     	
0F46 89 00       	clr  	ac1             	     	
0F47 02 BF 0F 83 	call 	$0x0F83
0F49 24 FF       	lrs  	ac0.l, $(CMBL)
0F4A 02 BF 0F 89 	call 	$0x0F89
0F4C 25 FF       	lrs  	ac1.l, $(CMBL)
0F4D 02 BF 0F 89 	call 	$0x0F89
0F4F 27 FF       	lrs  	ac1.m, $(CMBL)
0F50 2E CE       	srs  	$(DSMAH), ac0.m
0F51 2C CF       	srs  	$(DSMAL), ac0.l
0F52 16 C9 00 01 	si   	$(DSCR), #0x0001
0F54 2F CD       	srs  	$(DSPA), ac1.m
0F55 2D CB       	srs  	$(DSBL), ac1.l
0F56 81 00       	clr  	ac0             	     	
0F57 89 00       	clr  	ac1             	     	
0F58 02 BF 0F 83 	call 	$0x0F83
0F5A 24 FF       	lrs  	ac0.l, $(CMBL)
0F5B 1C 9E       	mrr  	ix0, ac0.m
0F5C 1C BC       	mrr  	ix1, ac0.l
0F5D 02 BF 0F 89 	call 	$0x0F89
0F5F 25 FF       	lrs  	ac1.l, $(CMBL)
0F60 02 BF 0F 89 	call 	$0x0F89
0F62 27 FF       	lrs  	ac1.m, $(CMBL)
0F63 1C DF       	mrr  	ix2, ac1.m
0F64 1C FD       	mrr  	ix3, ac1.l
0F65 81 00       	clr  	ac0             	     	
0F66 02 BF 0F 83 	call 	$0x0F83
0F68 26 FF       	lrs  	ac0.m, $(CMBL)
0F69 1C 1E       	mrr  	ar0, ac0.m
0F6A 89 00       	clr  	ac1             	     	
0F6B 02 BF 0F 89 	call 	$0x0F89
0F6D 20 FF       	lrs  	ax0.l, $(CMBL)
0F6E 1F 5F       	mrr  	ax0.h, ac1.m
0F6F 02 BF 0F 83 	call 	$0x0F83
0F71 21 FF       	lrs  	ax1.l, $(CMBL)
0F72 02 BF 0F 83 	call 	$0x0F83
0F74 23 FF       	lrs  	ax1.h, $(CMBL)
0F75 26 C9       	lrs  	ac0.m, $(DSCR)
0F76 02 A0 00 04 	tclr 	ac0.m, #0x0004
0F78 02 9C 0F 75 	jnok 	$0x0F75
0F7A 02 9F 80 B5 	j    	$0x80B5
0F7C 00 21       	halt 	
0F7D 02 9F 80 00 	j    	$0x8000
0F7F 00 21       	halt 	
0F80 02 9F 00 45 	j    	$0x0045
0F82 00 21       	halt 	
0F83 26 FE       	lrs  	ac0.m, $(CMBH)
0F84 02 C0 80 00 	tset 	ac0.m, #0x8000
0F86 02 9C 0F 83 	jnok 	$0x0F83
0F88 02 DF       	ret  	
0F89 27 FE       	lrs  	ac1.m, $(CMBH)
0F8A 03 C0 80 00 	tset 	ac1.m, #0x8000
0F8C 02 9C 0F 89 	jnok 	$0x0F89
0F8E 02 DF       	ret  	
0F8F 00 00       	nop  	

```
