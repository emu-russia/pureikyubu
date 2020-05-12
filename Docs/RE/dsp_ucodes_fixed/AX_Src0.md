## SRC 0 - N64 type polyphase filter (4-tap)

```

Load accelerator and adpcm decoder registers.

06E0 00 82 0B B8 	lri  	ar2, #0x0BB8				
06E2 19 5E       	lrri 	ac0.m, @ar2
06E3 2E D1       	srs  	$(ACFMT), ac0.m 			// AXPBADDR.format
06E4 19 5E       	lrri 	ac0.m, @ar2
06E5 2E D4       	srs  	$(ACSAH), ac0.m 			// AXPBADDR.loopAddressHi
06E6 19 5E       	lrri 	ac0.m, @ar2
06E7 2E D5       	srs  	$(ACSAL), ac0.m 			// AXPBADDR.loopAddressLo
06E8 19 5E       	lrri 	ac0.m, @ar2
06E9 2E D6       	srs  	$(ACEAH), ac0.m 			// AXPBADDR.endAddressHi
06EA 19 5E       	lrri 	ac0.m, @ar2
06EB 2E D7       	srs  	$(ACEAL), ac0.m 			// AXPBADDR.endAddressLo
06EC 19 5E       	lrri 	ac0.m, @ar2
06ED 2E D8       	srs  	$(ACCAH), ac0.m 			// AXPBADDR.currentAddressHi
06EE 19 5E       	lrri 	ac0.m, @ar2
06EF 2E D9       	srs  	$(ACCAL), ac0.m 			// AXPBADDR.currentAddressLo
06F0 19 5E       	lrri 	ac0.m, @ar2
06F1 2E A0       	srs  	$(ADPCM_A00), ac0.m 		// AXPBADPCM.a[0][0]
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
0711 2E DE       	srs  	$(ACGAN), ac0.m 			// AXPBADPCM.gain
0712 19 5E       	lrri 	ac0.m, @ar2
0713 2E DA       	srs  	$(ACPDS), ac0.m 			// AXPBADPCM.pred_scale
0714 19 5E       	lrri 	ac0.m, @ar2
0715 2E DB       	srs  	$(ACYN1), ac0.m 			// AXPBADPCM.yn1
0716 19 5E       	lrri 	ac0.m, @ar2
0717 2E DC       	srs  	$(ACYN2), ac0.m 			// AXPBADPCM.yn2
0718 8C 00       	clr15	                	     	
0719 8A 00       	m2   	                	     	
071A 8E 00       	clr40	                	     	


071B 00 D8 0E 16 	lr   	ax0.l, $0x0E16  			// 0xE16 | Coefficient Table Ptr

071D 19 5B       	lrri 	ax1.h, @ar2 				// AXPBSRC.ratioHi 		sampling ratio, integer
071E 19 59       	lrri 	ax1.l, @ar2 				// AXPBSRC.ratioLo 		sampling ratio, fraction
071F 81 00       	clr  	ac0             	     	
0720 19 5C       	lrri 	ac0.l, @ar2 				// AXPBSRC.currentAddressFrac  		current fractional sample position

0721 00 80 0E 48 	lri  	ar0, #0x0E48 				// Temp
0723 19 5F       	lrri 	ac1.m, @ar2 				// AXPBSRC.last_samples[0]
0724 1B 1F       	srri 	@ar0, ac1.m
0725 19 5F       	lrri 	ac1.m, @ar2 				// AXPBSRC.last_samples[1]
0726 1B 1F       	srri 	@ar0, ac1.m
0727 19 5F       	lrri 	ac1.m, @ar2 				// AXPBSRC.last_samples[2]
0728 1B 1F       	srri 	@ar0, ac1.m
0729 18 5F       	lrr  	ac1.m, @ar2 				// AXPBSRC.last_samples[3]
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
0739 00 9A 01 FC 	lri  	ax0.h, #0x01FC 				// const
073B 00 9E 0E 48 	lri  	ac0.m, #0x0E48
073D 00 81 FF DD 	lri  	ar1, #0xFFDD 				// Decoded ADPCM data
073F 00 83 0D 80 	lri  	ar3, #0x0D80 

0741 00 64 07 52 	bloop	ix0, $0x0752
	0743 18 27       	lrr  	ix3, @ar1 						// Read sample
	0744 1B 07       	srri 	@ar0, ix3 
	0745 4A 00       	addax	ac0, ax1        	     	
	0746 1F FC       	mrr  	ac1.m, ac0.l
	0747 18 27       	lrr  	ix3, @ar1 						// Read sample
	0748 1B 07       	srri 	@ar0, ix3
	0749 15 79       	lsr  	ac1, -7
	074A 35 00       	andr 	ac1.m, ax0.h    	 	// 0x1FC 
	074B 18 27       	lrr  	ix3, @ar1 						// Read sample
	074C 1B 07       	srri 	@ar0, ix3
	074D 41 00       	addr 	ac1, ax0.l      	     	
	074E 1B 7E       	srri 	@ar3, ac0.m
	074F 18 27       	lrr  	ix3, @ar1 						// Read sample
	0750 1B 07       	srri 	@ar0, ix3
	0751 1B 7F       	srri 	@ar3, ac1.m
	0752 00 00       	nop  	


0753 00 65 07 58 	bloop	ix1, $0x0758
	0755 18 27       	lrr  	ix3, @ar1 						// Read sample
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

Save last SRC state (last_samples and currentAddressFrac)

0763 00 04       	dar  	ar0
0764 18 9F       	lrrd 	ac1.m, @ar0
0765 1A DF       	srrd 	@ar2, ac1.m 			// last_samples[3]
0766 18 9F       	lrrd 	ac1.m, @ar0
0767 1A DF       	srrd 	@ar2, ac1.m 			// last_samples[2]
0768 18 9F       	lrrd 	ac1.m, @ar0
0769 1A DF       	srrd 	@ar2, ac1.m 			// last_samples[1]
076A 18 9F       	lrrd 	ac1.m, @ar0
076B 1A DF       	srrd 	@ar2, ac1.m 			// last_samples[0]
076C 1A DC       	srrd 	@ar2, ac0.l 					// currentAddressFrac


Save last accelerator / decoder state to VPB


076D 00 82 0B D2 	lri  	ar2, #0x0BD2 					
076F 27 DC       	lrs  	ac1.m, $(ACYN2)
0770 1A DF       	srrd 	@ar2, ac1.m 			// AXPBADPCM.yn2
0771 27 DB       	lrs  	ac1.m, $(ACYN1)
0772 1A DF       	srrd 	@ar2, ac1.m 			// AXPBADPCM.yn1
0773 27 DA       	lrs  	ac1.m, $(ACPDS)
0774 1A DF       	srrd 	@ar2, ac1.m 			// AXPBADPCM.pred_scale
0775 00 82 0B BE 	lri  	ar2, #0x0BBE
0777 27 D9       	lrs  	ac1.m, $(ACCAL) 
0778 1A DF       	srrd 	@ar2, ac1.m 			// AXPBADDR.currentAddressLo
0779 27 D8       	lrs  	ac1.m, $(ACCAH)
077A 1A DF       	srrd 	@ar2, ac1.m 			// AXPBADDR.currentAddressHi



077B 8F 00       	set40	                	     	
077C 00 C1 0E 42 	lr   	ar1, $0x0E42 			/// 0xCE0 - Sample-rate converted Output  (Load running pointer)
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
0792 00 E1 0E 42 	sr   	$0x0E42, ar1 			// Save running pointer
0794 02 DF       	ret  	
```

```
0D80: 0E48 1160 0E49 10C0 0E4A 1020 0E4A 1180
0D88: 0E4B 10E1 0E4C 1044 0E4C 11A4 0E4D 1104
0D90: 0E4E 1064 0E4E 11C5 0E4F 1128 0E50 1088
0D98: 0E50 11E8 0E51 1148 0E52 10A9 0E53 100C
0DA0: 0E53 116C 0E54 10CC 0E55 102C 0E55 118D
0DA8: 0E56 10F0 0E57 1050 0E57 11B0 0E58 1110
0DB0: 0E59 1070 0E59 11D4 0E5A 1134 0E5B 1094
0DB8: 0E5B 11F4 0E5C 1154 0E5D 10B8 0E5E 1018
```

Filled by random adpcm data. First 4 words are last_samples[0...3]

```
0E48: 0000 0000 0000 0000 0029 0823 18BE 2784
0E50: 0AE1 3D6C 2CD6 32AE 2952 1F90 1649 2DF1
0E58: 1AF1 01BB 26E9 01EB 0BB3 2EA6 12DB 153C
0E60: 3E87 390C 0F3E 0099 0124 305E 040D 8000
0E68: 0000 8000 0000 8000 0000 8000 0000 8000
0E70: 0000 8000 0000 8000 0000 8000 0000 8000
```


### 077B

```c++

{
	set40();

	ar1 = *(uint16_t *)0x0E42; 		// 0xCE0
	ar2 = 0x0D80;

	ar0 = *ar2++;
	ar3 = *ar2++;

	ax0h = *ar0++; ax1h = *ar3++;

	prod = ax0h * ax1h;

	ax0l = *ar0++; ax1l = *ar3++;

}


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

```
