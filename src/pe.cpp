// PE - pixel engine
#include "pch.h"

using namespace Debug;

// Handling access to the PE registers available to the CPU and EFB

namespace GFX
{
	void PixelEngine::PE_DONE_INT()
	{
		if (peregs.sr & PE_SR_DONEMSK)
		{
			peregs.sr |= PE_SR_DONE;
			Flipper::HW->pi->PIAssertInt(PI_INTERRUPT_PE_FINISH);
		}
	}

	void PixelEngine::PE_TOKEN_INT()
	{
		if (peregs.sr & PE_SR_TOKENMSK)
		{
			peregs.sr |= PE_SR_TOKEN;
			Flipper::HW->pi->PIAssertInt(PI_INTERRUPT_PE_TOKEN);
		}
	}

	// Currently, Cpu2Efb emulation is not supported. It is planned to be forwarded to the graphic Backend.

	uint32_t PixelEngine::EfbPeek(uint32_t addr)
	{
		Report(Channel::GP, "EfbPeek, address: 0x%08X\n", addr);
		return 0;
	}

	void PixelEngine::EfbPoke(uint32_t addr, uint32_t value)
	{
		Report(Channel::GP, "EfbPoke, address: 0x%08X, value: 0x%08X\n", addr, value);
	}

	// sel:0 - file, sel:1 - memory
	void PixelEngine::GL_DoSnapshot(bool sel, FILE* f, uint8_t* dst, int width, int height)
	{
		uint8_t      hdr[14 + 40];   // bmp header
		uint16_t* phdr;
		uint16_t     s, t;
		uint8_t* buf, * ptr;
		float   ds, dt, d0, d1;
		bool    linear = false;

		// allocate temporary buffer
		buf = (uint8_t*)malloc(gfx->scr_w * gfx->scr_h * 3);

		// calculate aspects
		ds = (float)gfx->scr_w / (float)width;
		dt = (float)gfx->scr_h / (float)height;
		if (ds != 1.0f) linear = true;

		// write hardcoded header
		memset(hdr, 0, sizeof(hdr));
		hdr[0] = 'B'; hdr[1] = 'M'; hdr[2] = 0x36;
		hdr[4] = 0x20; hdr[10] = 0x36;
		hdr[14] = 40;
		phdr = (uint16_t*)(&hdr[0x12]); *phdr = (uint16_t)width;
		phdr = (uint16_t*)(&hdr[0x16]); *phdr = (uint16_t)height;
		hdr[26] = 1; hdr[28] = 24; hdr[36] = 0x20;
		if (sel)
		{
			memcpy(dst, hdr, sizeof(hdr));
			dst += sizeof(hdr);
		}
		else fwrite(hdr, 1, sizeof(hdr), f);

		// read opengl buffer
		glReadPixels(0, 0, gfx->scr_w, gfx->scr_h, GL_RGB, GL_UNSIGNED_BYTE, buf);

		// write texture image
		for (t = 0, d0 = 0; t < height; t++, d0 += dt)
		{
			for (s = 0, d1 = 0; s < width; s++, d1 += ds)
			{
				uint8_t  prev[3] = { 0 };
				uint8_t  rgb[3];     // RGB triplet
				ptr = &buf[3 * (gfx->scr_w * (int)d0 + (int)d1)];
				{
					// linear filter
					if (s && linear)
					{
						rgb[2] = (*ptr++ + prev[2]) >> 1;
						rgb[1] = (*ptr++ + prev[1]) >> 1;
						rgb[0] = (*ptr++ + prev[0]) >> 1;
					}
					else
					{
						rgb[2] = *ptr++;
						rgb[1] = *ptr++;
						rgb[0] = *ptr++;
					}

					if (linear)
					{
						prev[2] = rgb[2];
						prev[1] = rgb[1];
						prev[0] = rgb[0];
					}

					if (sel) { memcpy(dst, rgb, 3); dst += 3; }
					else fwrite(rgb, 1, 3, f);
				}
			}
		}

		free(buf);
	}

	void PixelEngine::GL_MakeSnapshot(char* path)
	{
		if (gfx->make_shot) return;
		gfx->snap_w = gfx->scr_w;
		gfx->snap_h = gfx->scr_h;
		// create new file    
		if (gfx->snap_file)
		{
			fclose(gfx->snap_file);
			gfx->snap_file = NULL;
		}
		gfx->snap_file = fopen(path, "wb");
		if (gfx->snap_file) gfx->make_shot = true;
	}

	// make small snapshot for savestate
	// new size 160x120
	void PixelEngine::GL_SaveBitmap(uint8_t* buf)
	{
		GL_DoSnapshot(true, NULL, buf, 160, 120);
	}

	void PixelEngine::PERegRead(uint32_t addr, uint32_t* reg, void* context)
	{
		PixelEngine* pe = (PixelEngine*)context;
		switch (addr & 0xFF)
		{
			case PE_PI_INTRCTRL:
				*reg = pe->peregs.sr;
				break;

			case PE_PI_TOKEN:
				*reg = pe->pe.token.token;
				break;

			default:
				*reg = 0;
				break;
		}
	}

	void PixelEngine::PERegWrite(uint32_t addr, uint32_t data, void* context)
	{
		PixelEngine* pe = (PixelEngine*)context;
		switch (addr & 0xFF)
		{
			case PE_PI_INTRCTRL:

				// clear interrupts
				if (pe->peregs.sr & PE_SR_DONE)
				{
					pe->peregs.sr &= ~PE_SR_DONE;
					Flipper::HW->pi->PIClearInt(PI_INTERRUPT_PE_FINISH);
				}
				if (pe->peregs.sr & PE_SR_TOKEN)
				{
					pe->peregs.sr &= ~PE_SR_TOKEN;
					Flipper::HW->pi->PIClearInt(PI_INTERRUPT_PE_TOKEN);
				}

				// set mask bits
				if (data & PE_SR_DONEMSK) pe->peregs.sr |= PE_SR_DONEMSK;
				else pe->peregs.sr &= ~PE_SR_DONEMSK;
				if (data & PE_SR_TOKENMSK) pe->peregs.sr |= PE_SR_TOKENMSK;
				else pe->peregs.sr &= ~PE_SR_TOKENMSK;

				break;

			default:
				break;
		}
	}

	PixelEngine::PixelEngine(HWConfig* config, GFXCore* parent_gfx)
	{
		gfx = parent_gfx;

		Report(Channel::CP, "Pixel Engine (for GFX)\n");

		// Pixel Engine
		Flipper::HW->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_ZMODE, PERegRead, PERegWrite, this);
		Flipper::HW->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_CMODE0, PERegRead, PERegWrite, this);
		Flipper::HW->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_CMODE1, PERegRead, PERegWrite, this);
		Flipper::HW->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_ALPHA_THRES, PERegRead, PERegWrite, this);
		Flipper::HW->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_CONTROL, PERegRead, PERegWrite, this);
		Flipper::HW->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_INTRCTRL, PERegRead, PERegWrite, this);
		Flipper::HW->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_TOKEN, PERegRead, PERegWrite, this);

		Flipper::HW->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_XBOUND0, PERegRead, PERegWrite, this);
		Flipper::HW->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_XBOUND1, PERegRead, PERegWrite, this);
		Flipper::HW->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_YBOUND0, PERegRead, PERegWrite, this);
		Flipper::HW->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_YBOUND1, PERegRead, PERegWrite, this);

		Flipper::HW->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_0L, PERegRead, PERegWrite, this);
		Flipper::HW->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_0H, PERegRead, PERegWrite, this);
		Flipper::HW->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_1L, PERegRead, PERegWrite, this);
		Flipper::HW->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_1H, PERegRead, PERegWrite, this);
		Flipper::HW->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_2L, PERegRead, PERegWrite, this);
		Flipper::HW->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_2H, PERegRead, PERegWrite, this);
		Flipper::HW->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_3L, PERegRead, PERegWrite, this);
		Flipper::HW->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_3H, PERegRead, PERegWrite, this);
		Flipper::HW->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_4L, PERegRead, PERegWrite, this);
		Flipper::HW->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_4H, PERegRead, PERegWrite, this);
		Flipper::HW->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_5L, PERegRead, PERegWrite, this);
		Flipper::HW->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_5H, PERegRead, PERegWrite, this);
	}

	PixelEngine::~PixelEngine()
	{
	}

	void PixelEngine::loadPEReg(size_t index, uint32_t value)
	{
		switch (index)
		{
			// Pixel Engine block

			case PE_ZMODE_ID:
			{
				static const char* zf[] = {
					"NEVER",
					"LESS",
					"EQUAL",
					"LEQUAL",
					"GREATER",
					"NEQUAL",
					"GEQUAL",
					"ALWAYS"
				};

				static uint32_t glzf[] = {
					GL_NEVER,
					GL_LESS,
					GL_EQUAL,
					GL_LEQUAL,
					GL_GREATER,
					GL_NOTEQUAL,
					GL_GEQUAL,
					GL_ALWAYS
				};

				pe.zmode.bits = value;

				/*/
				GFXError(
					"z mode:\n"
					"compare: %s\n"
					"func: %s\n"
					"update: %s",
					(bpRegs.zmode.enable) ? ("yes") : ("no"),
					zf[bpRegs.zmode.func],
					(bpRegs.zmode.mask) ? ("yes") : ("no")
				);
				/*/

				if (pe.zmode.enable)
				{
					glEnable(GL_DEPTH_TEST);
					glDepthFunc(glzf[pe.zmode.func]);
					glDepthMask(pe.zmode.mask);
				}
				else glDisable(GL_DEPTH_TEST);
			}
			break;

			// set blending rules
			case PE_CMODE0_ID:
			{
				pe.cmode0.bits = value;

				static const char* logicop[] = {
					"clear",
					"and",
					"revand",
					"copy",
					"invand",
					"nop",
					"xor",
					"or",
					"nor",
					"eqv",
					"inv",
					"revor",
					"invcopy",
					"invor",
					"nand",
					"set"
				};

				static const char* sfactor[] = {
					"zero",
					"one",
					"srcclr",
					"invsrcclr",
					"srcalpha",
					"invsrcalpha",
					"dstalpha",
					"invdstalpha"
				};

				static const char* dfactor[] = {
					"zero",
					"one",
					"dstclr",
					"invdstclr",
					"srcalpha",
					"invsrcalpha",
					"dstalpha",
					"invdstalpha"
				};

				/*/
				GFXError(
					"blend rules\n\n"
					"blend:%s, logic:%s\n"
					"logic op : %s\n"
					"sfactor : %s\n"
					"dfactor : %s\n",
					(bpRegs.cmode0.blend_en) ? ("on") : ("off"),
					(bpRegs.cmode0.logop_en) ? ("on") : ("off"),
					logicop[bpRegs.cmode0.logop],
					sfactor[bpRegs.cmode0.sfactor],
					dfactor[bpRegs.cmode0.dfactor]
				);
				/*/

				static uint32_t glsf[] = {
					GL_ZERO,
					GL_ONE,
					GL_SRC_COLOR,
					GL_ONE_MINUS_SRC_COLOR,
					GL_SRC_ALPHA,
					GL_ONE_MINUS_SRC_ALPHA,
					GL_DST_ALPHA,
					GL_ONE_MINUS_DST_ALPHA
				};

				static uint32_t gldf[] = {
					GL_ZERO,
					GL_ONE,
					GL_DST_COLOR,
					GL_ONE_MINUS_DST_COLOR,
					GL_SRC_ALPHA,
					GL_ONE_MINUS_SRC_ALPHA,
					GL_DST_ALPHA,
					GL_ONE_MINUS_DST_ALPHA
				};

				// blend hack
				if (pe.cmode0.blend_en)
				{
					glEnable(GL_BLEND);
					glBlendFunc(glsf[pe.cmode0.sfactor], gldf[pe.cmode0.dfactor]);
				}
				else glDisable(GL_BLEND);

				static uint32_t logop[] = {
					GL_CLEAR,
					GL_AND,
					GL_AND_REVERSE,
					GL_COPY,
					GL_AND_INVERTED,
					GL_NOOP,
					GL_XOR,
					GL_OR,
					GL_NOR,
					GL_EQUIV,
					GL_INVERT,
					GL_OR_REVERSE,
					GL_COPY_INVERTED,
					GL_OR_INVERTED,
					GL_NAND,
					GL_SET
				};

				// logic operations
				if (pe.cmode0.logop_en)
				{
					glEnable(GL_COLOR_LOGIC_OP);
					glLogicOp(logop[pe.cmode0.logop]);
				}
				else glDisable(GL_COLOR_LOGIC_OP);
			}
			break;

			case PE_CMODE1_ID:
				pe.cmode1.bits = value;
				break;

			// TODO: Make a GP update when copying the frame buffer by Pixel Engine.

			// draw done
			case PE_FINISH_ID:
			{
				gfx->GPFrameDone();

				pe_done_num++;
				if (pe_done_num == 1)
				{
					Flipper::HW->vi->VIDisableXfb();	// disable VI output
				}
				PE_DONE_INT();
			}
			break;

			case PE_TOKEN_ID:
			{
				pe.token.bits = value;
				if (pe.token.token == pe.token_int.token)
				{
					gfx->GPFrameDone();

					Flipper::HW->vi->VIDisableXfb();	// disable VI output
					PE_TOKEN_INT();
				}
			}
			break;

			// token
			case PE_TOKEN_INT_ID:
				pe.token_int.bits = value;
				break;

			case PE_COPY_CLEAR_AR_ID:
				pe.copy_clear_ar.bits = value;
				break;

			case PE_COPY_CLEAR_GB_ID:
				pe.copy_clear_gb.bits = value;
				break;

			case PE_COPY_CLEAR_Z_ID:
				pe.copy_clear_z.bits = value;
				break;

			default:
				gfx->bump->loadBUMPReg(index, value);
				break;
		}
	}
}