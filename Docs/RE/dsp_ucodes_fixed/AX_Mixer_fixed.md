## AX Ucode Mixer

```
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

// Do actual mixing. Here we mixing 1 ms of samples (32)

031B 00 DE 0E 42 	lr   	ac0.m, $0x0E42
031D 00 FE 0E 1C 	sr   	$0x0E1C, ac0.m
031F 00 C3 0E 15 	lr   	ar3, $0x0E15
0321 17 7F       	callr	ar3 							// Sample Rate Converter  (Result in #0x0CE0)
0322 8E 00       	clr40	                	     	 
0323 8A 00       	m2   	                	     	

// Looks like constructing volume envelope buffer for upcoming 32 samples

0324 81 00       	clr  	ac0             	     	
0325 89 00       	clr  	ac1             	     	
0326 00 DE 0B B3 	lr   	ac0.m, $0x0BB3 				// AXPBVE.currentDelta  (signed)   Usual: 0 (no change)
0328 00 DF 0B B2 	lr   	ac1.m, $0x0BB2 			// AXPBVE.currentVolume 		(.15 format)  Usual: 0x7FFF
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
0353 00 FE 0B B2 	sr   	$0x0BB2, ac0.m 								// Writeback ramped volume

// Apply volume envelope to sample-rate-converted samples (#0x0E48 - volume envelope per 32 samples, $0x0E43 - pointer to sample rate converted samples)

0355 8F 00       	set40	                	     	
0356 00 80 0E 48 	lri  	ar0, #0x0E48 						// Volume envelope buffer for 32 samples
0358 00 C1 0E 43 	lr   	ar1, $0x0E43 						// 0xCE0 ...
035A 1C 61       	mrr  	ar3, ar1 							// ar3 = ar1 (New values replaces old)
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

{
	MulMode(false); 	// Multiply result by 2
	Mode40(true); 		// Special processing for acX.m registers enabled
	ar0 = Temp; 		// #0xE48  - Volume envelope buffer
	ar1 = *(uint16_t)(0xE43); 		// Next 32 samples after SRC  (Usually initialized by #0xCE0, incremented by 32 after each SRC step)
	ar3 = ar1;

	for (int i=0; i<32; i++)
	{
		ax0l = *ar0++; 		
		ax0h = *ar1++;
		Prod = ax0l * ax0h * 2; 	// *2 comes from SR.AM bit
		ac0 = Prod;
		*ar3++ = Saturate(ac0m);	// Controlled by SR.XM bit
	}

	// Examples:
	// sample[0] = 0xFFFF, prod: 0xFFFFFFFFFFFF0002, saturated: 0xFFFF
	// sample[1] = 0x0002, prod: 0x1FFFC, saturated: 0x0001
	// sample[2] = 0x6D1E, prod: 0x6D1D25C4, saturated: 0x6D1D
	// sample[3] = 0xD6D4, prod: 0xFFFFFFFFD6D45258, saturated: 0xD6D4
	// sample[4] = 0xFEF8, prod: 0xFFFFFFFFFEF80210, saturated: 0xFEF8
	// sample[5] = 0x66A8, prod: 0x66A732B0, saturated: 0x66A7
	// sample[6] = 0xD40E, prod: 0xFFFFFFFFD40E57E4, saturated: 0xD40E
}

Skipped?

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

// Mixer Ctrl

// This is where the mixing of the Main bus, as well as AuxA and AuxB (if enabled) takes place. Per-bus volume control is also performed along with mixing.

// Input: #0xCE0 (usual)
// Output: #0x0 (Left) / #0x140 (Right) (usual)

0402 00 C3 0E 14 	lr   	ar3, $0x0E14
0404 8A 00       	m2   	                	     	 
0405 17 7F       	callr	ar3 							// JumpTable2[n] 
0406 00 C3 0E 46 	lr   	ar3, $0x0E46
0408 8A 00       	m2   	                	     	
0409 17 7F       	callr	ar3 							// JumpTable3[n]
040A 00 C3 0E 47 	lr   	ar3, $0x0E47
040C 8A 00       	m2   	                	     	
040D 17 7F       	callr	ar3 							// JumpTable4[n]
040E 81 00       	clr  	ac0             	     	

// Initial Time Delay processing

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

// Advance SampleBuf Pointers

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

// Loop above is repated 5 times (1ms * 5)

// Copy out ITD

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

// Copy out VPB

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

// Load next VPB (if exists)

0498 81 00       	clr  	ac0             	     	
0499 00 DE 0B 80 	lr   	ac0.m, $0x0B80
049B 00 DC 0B 81 	lr   	ac0.l, $0x0B81
049D B1 00       	tst  	ac0             	     	
049E 02 94 04 A4 	jne  	$0x04A4
04A0 00 C0 0E 07 	lr   	ar0, $0x0E07
04A2 02 9F 00 68 	j    	$0x0068

// Reinit SampleBuf pointers

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

// Load AXPBUPDATE Update Data

04CB 00 DE 0B A7 	lr   	ac0.m, $0x0BA7
04CD 00 DF 0B A8 	lr   	ac1.m, $0x0BA8
04CF 2E CE       	srs  	$(DSMAH), ac0.m
04D0 2F CF       	srs  	$(DSMAL), ac1.m
04D1 16 CD 03 C0 	si   	$(DSPA), #0x03C0
04D3 16 C9 00 00 	si   	$(DSCR), #0x0000
04D5 16 CB 00 80 	si   	$(DSBL), #0x0080

// Reload table pointers

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

// Load ITD Buffer

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
0527 02 BF 06 94 	call 	$0x0694 					// WaitDspDma
0529 00 DE 0B 9C 	lr   	ac0.m, $0x0B9C
052B 2E CE       	srs  	$(DSMAH), ac0.m
052C 00 DE 0B 9D 	lr   	ac0.m, $0x0B9D
052E 2E CF       	srs  	$(DSMAL), ac0.m
052F 16 CD 0C C0 	si   	$(DSPA), #0x0CC0
0531 16 C9 00 00 	si   	$(DSCR), #0x0000
0533 16 CB 00 40 	si   	$(DSBL), #0x0040
0535 02 BF 06 94 	call 	$0x0694 					// WaitDspDma
0537 00 C0 0E 07 	lr   	ar0, $0x0E07
0539 02 9F 02 FC 	j    	$0x02FC

053B 00 9F 0C E0 	lri  	ac1.m, #0x0CE0
053D 00 FF 0E 42 	sr   	$0x0E42, ac1.m
053F 00 FF 0E 40 	sr   	$0x0E40, ac1.m
0541 00 FF 0E 41 	sr   	$0x0E41, ac1.m
0543 00 FF 0E 43 	sr   	$0x0E43, ac1.m
0545 02 BF 06 94 	call 	$0x0694 					// WaitDspDma
0547 00 C0 0E 07 	lr   	ar0, $0x0E07
0549 02 9F 02 FC 	j    	$0x02FC
```
