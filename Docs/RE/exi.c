#include "exi.h"

/* EXISelect: enable chip select, set speed */
void EXISelect(int channel, int device, int freq)
{
	volatile unsigned long *exi = (volatile unsigned long *)0xCC006800;
	long d;
	// EXISelect
	d = exi[channel * 5];
	d &= 0x405;
	d |= ((1<<device)<<7) | (freq << 4);
	exi[channel*5] = d;
}

/* disable chipselect */
void EXIDeselect(int channel)
{
	volatile unsigned long *exi = (volatile unsigned long *)0xCC006800;
	exi[channel * 5] &= 0x405;
}

/* dirty way for asynchronous reads */
static void *exi_last_addr;
static int   exi_last_len;

/* mode?Read:Write len bytes to/from channel */
/* when read, data will be written back in EXISync */
void EXIImm(int channel, void *data, int len, int mode, int zero)
{
	volatile unsigned long *exi = (volatile unsigned long *)0xCC006800;
	if (mode == EXI_WRITE)
		exi[channel * 5 + 4] = *(unsigned long*)data;
	exi[channel * 5 + 3] = ((len-1)<<4)|(mode<<2)|1;
	if (mode == EXI_READ)
	{
		exi_last_addr = data;
		exi_last_len = len;
	} else
	{
		exi_last_addr = 0;
		exi_last_len = 0;
	}
}

/* Wait until transfer is done, write back data */
void EXISync(int channel)
{
	volatile unsigned long *exi = (volatile unsigned long *)0xCC006800;
	while (exi[channel * 5 + 3] & 1);

	if (exi_last_addr)
	{	
		int i;
		unsigned long d;
		d = exi[channel * 5 + 4];
		for (i=0; i<exi_last_len; ++i)
			((unsigned char*)exi_last_addr)[i] = (d >> ((3-i)*8)) & 0xFF;
	}
}

/* simple wrapper for transfers > 4bytes */
void EXIImmEx(int channel, void *data, int len, int mode)
{
	unsigned char *d = (unsigned char*)data;
	while (len)
	{
		int tc = len;
		if (tc > 4)
			tc = 4;
		EXIImm(channel, d, tc, mode, 0);
		EXISync(channel);
		len-=tc;
		d+=tc;
	}
}
