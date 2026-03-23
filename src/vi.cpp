// VI - video interface (TV stuff).
#include "pch.h"

/* ---------------------------------------------------------------------------

	Timing
	------

	small note for VI frame timing :

	 ---------------------------------------------------
	|       |         PAL         | NTSC, MPAL, EURGB60 |
	| value +---------------------|---------------------|
	|       |   NIN    |   INT    |   NIN    |   INT    |
	|=======+==========+==========+==========+==========|
	|  hz   |   50     |    25    |    60    |    30    |
	|-------+----------+----------+----------+----------|
	| lines |  312.5   |   625    |  262.5   |   525    |
	|-------+----------+----------+----------+----------|
	| active|   262    |   574    |   218    |   480    |
	 -------+----------+----------+----------+----------

	NIN - non-intelaced (double-strike) mode (both odd and even lines in one frame)
	INT - interlaced mode (odd and even lines in alternating frames)
	25/50 hz refer to the frequency of which a full videoframe (==all lines of framebuffer) is displayed
	progressive = double-strike = non-interlaced;
	"field" is the tv-frame as in "odd- and even- field" makes one frame

--------------------------------------------------------------------------- */

using namespace Debug;

// VI state (registers and other data)
VIState vi;

// ---------------------------------------------------------------------------
// drawing of XFB

// YUV to RGB conversion
#define yuv2rs(y, u, v) ( (uint32_t)bound((76283*(y - 16) + 104595*(v - 128))>>16) )
#define yuv2gs(y, u, v) ( (uint32_t)bound((76283*(y - 16) - 53281 *(v - 128) - 25624*(u - 128))>>16) << 8 )
#define yuv2bs(y, u, v) ( (uint32_t)bound((76283*(y - 16) + 132252*(u - 128))>>16) << 16 )

// clamping routine
static inline int bound(int x)
{
	if (x < 0) x = 0;
	if (x > 255) x = 255;
	return x;
}

// copy XFB to screen
void YUVBlit(uint8_t* yuvbuf, RGB* dib)
{
	uint32_t* rgbbuf = (uint32_t*)dib;
	int count = 320 * 480;

	if (!yuvbuf || !rgbbuf) return;

	// simple blitting, without effects
	while (count--)
	{
		int y1 = *yuvbuf++,
			v = *yuvbuf++,
			y2 = *yuvbuf++,
			u = *yuvbuf++;

		*rgbbuf++ = yuv2bs(y1, u, v) | yuv2gs(y1, u, v) | yuv2rs(y1, u, v);
		*rgbbuf++ = yuv2bs(y2, u, v) | yuv2gs(y2, u, v) | yuv2rs(y2, u, v);
	}

	VideoOutRefresh();
}

// ---------------------------------------------------------------------------
// frame timing

// reset VI timing
static void vi_set_timing()
{
	uint16_t reg = vi.disp_cr;
	vi.inter = (reg & VI_CR_NIN) ? 0 : 1;
	vi.mode = VI_CR_FMT(reg);
	if (vi.mode == 2) vi.mode = VI_NTSC_LIKE; // MPAL same as NTSC
	vi.vtime = Core->GetTicks();

	switch (vi.mode)
	{
		case VI_NTSC_LIKE:
			vi.one_frame = vi.one_second / 30;
			vi.vcount = (vi.inter) ? VI_NTSC_INTER : VI_NTSC_NON_INTER;
			break;
		case VI_PAL_LIKE:
			vi.one_frame = vi.one_second / 25;
			vi.vcount = (vi.inter) ? VI_PAL_INTER : VI_PAL_NON_INTER;
			break;
	}
}

// step position counter(s), update XFB
void VIUpdate()
{
	if ((Core->GetTicks() - vi.vtime) >= (vi.one_frame / vi.vcount))
	{
		vi.vtime = Core->GetTicks();

		// generate VIINT ?
		vi.pos.vcount++;
		if (vi.pos.vcount == vi.int0.vcount)
		{
			vi.int0.status = 1;
			if (vi.int0.enabled)
			{
				if (vi.log) {
					Report(Channel::VI, "VI Int0\n");
				}
				Flipper::HW->pi->PIAssertInt(PI_INTERRUPT_VI);
			}
		}

		// vertical counter
		if (vi.pos.vcount >= vi.vcount)
		{
			vi.pos.vcount = 1;

			// draw XFB
			if (vi.xfb)
			{
				YUVBlit(vi.xfbbuf, vi.gfxbuf);
				vi.frames++;
			}
		}
	}
}

// ---------------------------------------------------------------------------
// accessing VI registers.

static void VIRegRead(uint32_t addr, uint32_t* reg, void *context)
{
	switch (addr & 0x7f)
	{
		case VI_DISP_CR:
			*reg = vi.disp_cr;
			break;
		case VI_TFBL:
			*reg = vi.tfbl >> 16;
			break;
		case VI_TFBL+2:
			*reg = (uint16_t)vi.tfbl;
			break;
		case VI_BFBL:
			*reg = vi.bfbl >> 16;
			break;
		case VI_BFBL+2:
			*reg = (uint16_t)vi.bfbl;
			break;
		case VI_DISP_POS:
			*reg = vi.pos.val >> 16;
			break;
		case VI_DISP_POS+2:
			*reg = (uint16_t)vi.pos.val;
			break;
		case VI_INT0:
			*reg = vi.int0.val >> 16;
			break;
		case VI_INT0+2:
			*reg = (uint16_t)vi.int0.val;
			break;
		default:
			*reg = 0;
			break;
	}
}

static void VIRegWrite(uint32_t addr, uint32_t data, void* context)
{
	if (vi.log) {
		Report(Channel::VI, "VIRegWrite 0x%x = 0x%04x\n", addr & 0x7f, data);
	}

	switch (addr & 0x7f)
	{
		case VI_DISP_CR:
			vi.disp_cr = (uint16_t)data;
			vi_set_timing();
			break;
		case VI_TFBL:
			vi.tfbl &= 0x0000ffff;
			vi.tfbl |= data << 16;
			if (vi.log)
			{
				Report(Channel::VI, "TFBL set to %08X (xof=%i)\n", vi.tfbl, (vi.tfbl >> 24) & 0xf);
			}
			vi.tfbl &= 0xffffff;
			vi.xfbbuf = (uint8_t*)Flipper::HW->mem->MIGetMemoryPointerForVI(vi.tfbl);
			break;
		case VI_TFBL+2:
			vi.tfbl &= 0xffff0000;
			vi.tfbl |= (uint16_t)data;
			if (vi.log)
			{
				Report(Channel::VI, "TFBL set to %08X (xof=%i)\n", vi.tfbl, (vi.tfbl >> 24) & 0xf);
			}
			vi.tfbl &= 0xffffff;
			vi.xfbbuf = (uint8_t*)Flipper::HW->mem->MIGetMemoryPointerForVI(vi.tfbl);
			break;
		case VI_BFBL:
			vi.bfbl &= 0x0000ffff;
			vi.bfbl |= data << 16;
			vi.bfbl &= 0xffffff;
			if (vi.log)
			{
				Report(Channel::VI, "BFBL set to %08X\n", vi.bfbl);
			}
			//if(vi.bfbl >= RAMSIZE) vi.xfbbuf = NULL;
			//else vi.xfbbuf = &RAM[vi.bfbl];
			break;
		case VI_BFBL+2:
			vi.bfbl &= 0xffff0000;
			vi.bfbl |= (uint16_t)data;
			vi.bfbl &= 0xffffff;
			if (vi.log)
			{
				Report(Channel::VI, "BFBL set to %08X\n", vi.bfbl);
			}
			//if(vi.bfbl >= RAMSIZE) vi.xfbbuf = NULL;
			//else vi.xfbbuf = &RAM[vi.bfbl];
			break;
		case VI_INT0:
			vi.int0.val &= 0x0000ffff;
			vi.int0.val |= data << 16;
			if ((vi.int0.val & VI_INT_INT) == 0)
			{
				Flipper::HW->pi->PIClearInt(PI_INTERRUPT_VI);
			}
			break;
		case VI_INT0+2:
			vi.int0.val &= 0xffff0000;
			vi.int0.val |= (uint16_t)data;
			break;
		default:
			break;
	}
}

// show VI info
void VIStats()
{
	Report(Channel::Norm, "    VI interrupt : [%d x x x]\n", vi.int0.status);
	Report(Channel::Norm, "    VI int mask  : [%d x x x]\n", vi.int0.enabled);
	Report(Channel::Norm, "    VI int pos   : %d == %d, x == x, x == x, x == x (line)\n", vi.pos.vcount, vi.int0.vcount);
	Report(Channel::Norm, "    VI XFB       : T%08X B%08X (phys), enabled: %d\n", vi.tfbl, vi.bfbl, vi.xfb);
}

// ---------------------------------------------------------------------------
// init

void VIOpen(HWConfig* config)
{
	Report(Channel::VI, "Video-out hardware interface\n");

	// clear VI regs
	memset(&vi, 0, sizeof(vi));

	vi.one_second = Core->OneSecond();

	// read VI settings
	vi.log = config->vi_log;
	vi.xfb = config->vi_xfb;
	vi.videoEncoderFuse = config->videoEncoderFuse;

	// reset VI timing
	vi_set_timing();

	// XFB is not yet specified
	vi.gfxbuf = NULL;
	vi.xfbbuf = NULL;

	// open GDI (if need)
	if (vi.xfb)
	{
		bool res = VideoOutOpen(config, 640, 480, &vi.gfxbuf);
		if (!res)
		{
			Report(Channel::VI, "VI cant startup VideoOut backend!\n");
			vi.xfb = false;
		}
	}

	// set traps to VI registers
	for (uint32_t ofs = 0; ofs < VI_REG_MAX; ofs += 2)
	{
		Flipper::HW->pi->PISetTrap(PI_REGSPACE_VI + ofs, VIRegRead, VIRegWrite);
	}
}

void VIClose()
{
	// XFB can be enabled during emulation,
	// so we must be sure, that GDI is closed
	// even if XFB wasn't enabled, before start
	VideoOutClose();
}

void VISetEncoderFuse(int value)
{
	vi.videoEncoderFuse = value;
}

void VIGunTrigger(int num)
{
	switch (num & 1) {
		case 0:
			vi.latch0.val = vi.pos.val;
			vi.latch0.status = 1;
			break;

		case 1:
			vi.latch1.val = vi.pos.val;
			vi.latch1.status = 1;
			break;
	}
}