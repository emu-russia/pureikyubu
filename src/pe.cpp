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

	PixelEngine::PixelEngine(Flipper::Flipper* flipper, HWConfig* config, GFXCore* parent_gfx)
	{
		gfx = parent_gfx;

		Report(Channel::CP, "Pixel Engine (for GFX)\n");

		// Pixel Engine
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_ZMODE, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_CMODE0, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_CMODE1, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_ALPHA_THRES, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_CONTROL, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_INTRCTRL, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_TOKEN, PERegRead, PERegWrite, this);

		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_XBOUND0, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_XBOUND1, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_YBOUND0, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_YBOUND1, PERegRead, PERegWrite, this);

		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_0L, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_0H, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_1L, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_1H, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_2L, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_2H, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_3L, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_3H, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_4L, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_4H, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_5L, PERegRead, PERegWrite, this);
		flipper->pi->PISetTrap(PI_REGSPACE_PE | PE_PI_PERF_COUNTER_5H, PERegRead, PERegWrite, this);
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
				pe.zmode.bits = value;
				ApplyZMode();
				break;

			case PE_CMODE0_ID:
				pe.cmode0.bits = value;
				ApplyColorMode();
				break;

			case PE_CMODE1_ID:
				pe.cmode1.bits = value;
				ApplyColorMode();
				break;

			// The EFB pixel type / Z compression format. Neither has an equivalent in the OpenGL
			// backend: the render target is always the RGBA8 colour buffer with a 24-bit depth buffer,
			// which is what the uncompressed ("none") pixel types describe.
			case PE_CONTROL_ID:
				pe.control.bits = value;
				break;

			case PE_FIELD_MASK_ID:
				pe.field_mask.bits = value;
				break;

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

			// token
			case PE_TOKEN_INT_ID:
				pe.token_int.bits = value;
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

			case PE_REFRESH_ID:
				pe.refresh.bits = value;
				break;

			case PE_COPY_SRC_ADDR_ID:
				pe.copy_src_addr.bits = value;
				break;

			case PE_COPY_SRC_SIZE_ID:
				pe.copy_src_size.bits = value;
				break;

			case PE_COPY_DST_BASE0_ID:
				pe.copy_dst_base[0].bits = value;
				break;

			case PE_COPY_DST_BASE1_ID:
				pe.copy_dst_base[1].bits = value;
				break;

			case PE_COPY_DST_STRIDE_ID:
				pe.copy_dst_stride.bits = value;
				break;

			case PE_COPY_SCALE_ID:
				pe.copy_scale.bits = value;
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

			// The copy command is the trigger of the whole copy engine. Of its operations only the
			// clear is something the OpenGL backend can honour: the copy to main memory (display copy
			// and texture copy) needs the EFB to be readable as a texture, which the emulator does not
			// emulate (it renders to the back buffer, see gfx.cpp).
			//
			// The clear is only *recorded* here, it is performed by the frame begin. The copy command
			// is issued at the end of a frame (to hand the finished EFB over to the display and to
			// prepare it for the next one), while the presented frame is swapped on PE_FINISH, which
			// comes later. Clearing right away would therefore erase the frame that is still to be
			// displayed.
			case PE_COPY_CMD_ID:
				pe.copy_cmd.bits = value;
				if (pe.copy_cmd.clear)
				{
					copy_clear_pending = true;
				}
				break;

			case PE_COPY_VFILTER0_ID:
				pe.vfilter_0.bits = value;
				break;

			case PE_COPY_VFILTER1_ID:
				pe.vfilter_1.bits = value;
				break;

			case PE_XBOUND_ID:
				pe.xbound.bits = value;
				break;

			case PE_YBOUND_ID:
				pe.ybound.bits = value;
				break;

			case PE_PERFMODE_ID:
				pe.perfmode.bits = value;
				break;

			case PE_CHICKEN_ID:
				pe.chicken.bits = value;
				break;

			case PE_QUAD_OFFSET_ID:
				pe.quad_offset.bits = value;
				break;

			default:
				// The BP register walk continues in the bump/indirect unit, then the texture unit and
				// the TEV (see gfx.md 10.1)
				gfx->bump->loadBUMPReg(index, value);
				break;
		}
	}

	// -------------------------------------------------------------------------------------------
	// The GL state the PE owns
	// -------------------------------------------------------------------------------------------

	// PE_ZMODE: enable, compare function and Z write mask. GEN_MODE.zfreeze freezes the Z buffer
	// after the first primitive, which in this backend means that no depth is written any more
	// (gfx-su.md 3.6, gfx-pe.md 4.2).
	void PixelEngine::ApplyZMode()
	{
		static const GLenum glzf[8] = {
			GL_NEVER, GL_LESS, GL_EQUAL, GL_LEQUAL, GL_GREATER, GL_NOTEQUAL, GL_GEQUAL, GL_ALWAYS
		};

		if (pe.zmode.enable)
		{
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(glzf[pe.zmode.func & 7]);
		}
		else
		{
			glDisable(GL_DEPTH_TEST);
		}

		bool zfreeze = (gfx != nullptr) && (gfx->genmode.zfreeze != 0);
		glDepthMask((pe.zmode.mask && !zfreeze) ? GL_TRUE : GL_FALSE);
	}

	void PixelEngine::ApplyColorMode()
	{
		// Blend factors (gfx-pe.md 6.2). The first four differ between the source and the
		// destination factor; the last four are shared.
		static const GLenum glsf[8] = {
			GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR,
			GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA
		};

		static const GLenum gldf[8] = {
			GL_ZERO, GL_ONE, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR,
			GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA
		};

		if (pe.cmode0.blend_en)
		{
			glEnable(GL_BLEND);

			GLenum src = glsf[pe.cmode0.sfactor & 7];
			GLenum dst = gldf[pe.cmode0.dfactor & 7];

			// PE_CMODE1.const_alpha replaces the destination alpha factor with a constant
			if (pe.cmode1.const_alpha_en)
			{
				glBlendColor(1.0f, 1.0f, 1.0f, (float)pe.cmode1.const_alpha / 255.0f);

				if (dst == GL_DST_ALPHA) dst = GL_CONSTANT_ALPHA;
				else if (dst == GL_ONE_MINUS_DST_ALPHA) dst = GL_ONE_MINUS_CONSTANT_ALPHA;
			}

			glBlendFunc(src, dst);

			// PE_CMODE0.blendop: 0 = add, 1 = subtract (destination minus source)
			glBlendEquation(pe.cmode0.blendop ? GL_FUNC_REVERSE_SUBTRACT : GL_FUNC_ADD);
		}
		else
		{
			glDisable(GL_BLEND);
		}

		static const GLenum logop[16] = {
			GL_CLEAR, GL_AND, GL_AND_REVERSE, GL_COPY, GL_AND_INVERTED, GL_NOOP, GL_XOR, GL_OR,
			GL_NOR, GL_EQUIV, GL_INVERT, GL_OR_REVERSE, GL_COPY_INVERTED, GL_OR_INVERTED, GL_NAND, GL_SET
		};

		if (pe.cmode0.logop_en)
		{
			glEnable(GL_COLOR_LOGIC_OP);
			glLogicOp(logop[pe.cmode0.logop & 0xf]);
		}
		else
		{
			glDisable(GL_COLOR_LOGIC_OP);
		}

		// The two mask bits are "update enabled" flags, not "do not update": GXSetColorUpdate and
		// GXSetAlphaUpdate store their argument straight into them, so a set bit writes the plane.
		glColorMask(
			pe.cmode0.col_mask ? GL_TRUE : GL_FALSE,
			pe.cmode0.col_mask ? GL_TRUE : GL_FALSE,
			pe.cmode0.col_mask ? GL_TRUE : GL_FALSE,
			pe.cmode0.alpha_mask ? GL_TRUE : GL_FALSE);

		// Dither (PE_CMODE0.dither_en, gfx-pe.md 6.3). GL_DITHER is set from the register so that the
		// state is not silently dropped, but it cannot reproduce the hardware dither in this backend:
		// the Flipper dithers with a 4x4 ordered matrix when the EFB pixel format stores fewer bits
		// per channel than the TEV produces (RGB565 or RGBA6), while the render target of this
		// backend is 8 bits per channel for every format. For the RGB8 EFB format - the only one the
		// backend implements - the dither matrix is the identity, and for the narrower ones the
		// backend has no render target to quantise into. A faithful implementation has to start with
		// the EFB pixel format (PE_ZCONF / PE_CONTROL.pixel_format), which the emulator does not
		// decode yet. See wiki/gfx.md.
		if (pe.cmode0.dither_en)
			glEnable(GL_DITHER);
		else
			glDisable(GL_DITHER);
	}

	// The copy bounds come from PE_XBOUND / PE_YBOUND (gfx-pe.md 6.17). The clamp bits of
	// PE_COPY_CMD belong to the vertical filter and not to the bounds, so they are not consulted.
	void PixelEngine::CopyBounds(int* x, int* y, int* width, int* height)
	{
		int left = (int)pe.xbound.left;
		int right = (int)pe.xbound.right;
		int top = (int)pe.ybound.top;
		int bottom = (int)pe.ybound.bottom;

		if (right < left)
		{
			int t = left; left = right; right = t;
		}
		if (bottom < top)
		{
			int t = top; top = bottom; bottom = t;
		}

		*x = left;
		*y = top;
		*width = right - left + 1;
		*height = bottom - top + 1;
	}

	// The copy engine's clear fills the (optionally bounded) EFB region with the PE clear colour and
	// the clear Z, without depth testing or blending.
	void PixelEngine::ApplyCopyClear()
	{
		int x = 0, y = 0, w = (int)gfx->scr_w, h = (int)gfx->scr_h;
		CopyBounds(&x, &y, &w, &h);

		glScissor(x, (int)gfx->scr_h - (y + h), w, h);
		glDisable(GL_BLEND);
		glDisable(GL_COLOR_LOGIC_OP);
		glDisable(GL_DEPTH_TEST);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

		// The copy engine writes the whole region, so the Z update mask does not apply either: a GL
		// clear of the depth buffer would otherwise be skipped when the scene left the mask disabled.
		// ApplyZMode() below puts the mask back.
		glDepthMask(GL_TRUE);

		glClearColor(
			(float)pe.copy_clear_ar.red / 255.0f,
			(float)pe.copy_clear_gb.green / 255.0f,
			(float)pe.copy_clear_gb.blue / 255.0f,
			(float)pe.copy_clear_ar.alpha / 255.0f);
		glClearDepth((double)(pe.copy_clear_z.value / 16777215.0));
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// The clear bypassed the GL state the registers describe, so it is re-applied
		ApplyZMode();
		ApplyColorMode();

		glScissor(0, 0, (GLsizei)gfx->scr_w, (GLsizei)gfx->scr_h);
	}

	bool PixelEngine::TakePendingCopyClear()
	{
		bool pending = copy_clear_pending;
		copy_clear_pending = false;
		return pending;
	}

	void PixelEngine::Reset()
	{
		pe = PEState{};
		peregs = PERegs{};
		copy_clear_pending = false;

		// The hardware reset values that the specification states (gfx-pe.md 6.5, 6.8, 6.20)
		pe.field_mask.bits = 0x3;
		// The colour and alpha update bits are set as well: a program that never programs
		// PE_CMODE0 (the GX SDK does, in GXInit) would otherwise be unable to draw at all.
		pe.cmode0.col_mask = 1;
		pe.cmode0.alpha_mask = 1;
		pe.quad_offset.bits = 0xAAAAAAAA;
		pe.token_int.bits = 0xFFFF;

		frames = 0;
		pe_done_num = 0;

		if (gfx != nullptr && gfx->BackendStarted())
		{
			gfx->ApplyDefaultGLState();
			ApplyZMode();
			ApplyColorMode();
		}
	}
}