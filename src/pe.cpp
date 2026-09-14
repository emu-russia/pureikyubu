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
				// GXDrawDone marks the end of the frame: the copied picture is complete.
				gfx->GPFrameDone();

				pe_done_num++;
				PE_DONE_INT();
			}
			break;

			// token
			case PE_TOKEN_INT_ID:
				pe.token_int.bits = value;
				break;

			// draw sync token
			//
			// The token is a synchronisation marker, not a frame boundary: a title loads it through
			// the pipe to learn when the PE has walked the FIFO up to that point, and several demos
			// put it in the middle of a frame (draw, sync, read the bounding box back, draw some
			// more). Presenting the frame here swapped half a frame out and left the other buffer -
			// which held only what was drawn after the token - as the finished picture, so
			// frb-bound-box came out black. The picture is complete at GXDrawDone (PE_FINISH above)
			// and at a full-frame display copy (GPDisplayCopy), which is where the frame is
			// presented; the token only raises its interrupt.
			case PE_TOKEN_ID:
			{
				pe.token.bits = value;
				if (pe.token.token == pe.token_int.token)
				{
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

			// The copy command is the trigger of the whole copy engine.
			//
			// A copy and its clear belong together, in the order the stream asks for them: the copy
			// reads the rectangle as it is, and a copy that asked for a clear turns the quads it
			// read into the clear colour (gfx-pe.md 5.1, the RMW of the colour unit), so whatever
			// follows sees the cleared EFB. See below for when the clear of each kind runs.
			//
			// The values are captured when the command arrives, because the game may reprogram the
			// registers for its next copy before the clear runs.
			case PE_COPY_CMD_ID:
			{
				pe.copy_cmd.bits = value;

				CopyClearState clear{};

				if (pe.copy_cmd.clear)
				{
					clear.ar = pe.copy_clear_ar;
					clear.gb = pe.copy_clear_gb;
					clear.z = pe.copy_clear_z;
					clear.x = (int)pe.copy_src_addr.x;
					clear.y = (int)pe.copy_src_addr.y;
					clear.w = (int)pe.copy_src_size.x + 1;
					clear.h = (int)pe.copy_src_size.y + 1;

					// The two kinds of copy clear differently. A texture copy prepares a render
					// target the title is about to draw into: its clear runs below, over the
					// rectangle it read, before those draws - a title that renders its frame in
					// several passes through the copy engine loses the passes drawn before it if
					// that clear wipes the whole buffer. A display copy presents the frame, and this
					// backend shows the EFB where a console shows the XFB, so its clear covers the
					// whole colour buffer and waits for the frame begin: running it here would wipe
					// the picture that is about to be shown, and leaving any part of the buffer
					// alone kept the previous frame in the lower half of the bootrom's splash.
					if (pe.copy_cmd.opcode == PE_COPY_CMD_DISPLAY)
					{
						clear.full = true;

						if (pending_clear_count < MaxPendingCopyClears)
						{
							pending_clears[pending_clear_count++] = clear;
						}
					}
				}

				// A display copy hands the finished EFB over to the video interface as the XFB
				// (gfx-pe.md 5.6), so the full-frame ones are a frame boundary of their own. The
				// backend displays the EFB instead of the XFB, so the picture has to be swapped
				// here for the titles whose movie player presents through the copy engine and waits
				// for the retrace without ever calling GXDrawDone (the SDK THP player does that; its
				// frames stayed on an unpresented back buffer, issue #349).
				//
				// A partial display copy is not a frame boundary: the bootrom and the 2D front ends
				// write the picture in several passes (one copy per display-list buffer) and call
				// PE_FINISH when the frame is complete. Presenting those would flicker the picture.
				//
				// A texture copy is an intermediate render target and never presents: it turns the
				// rectangle into a tiled texture in main memory (gfx-pe.md 5.7).
				if (pe.copy_cmd.opcode == PE_COPY_CMD_TEXTURE)
				{
					TextureCopy();
				}

				if (pe.copy_cmd.opcode == PE_COPY_CMD_DISPLAY &&
					pe.copy_src_addr.x == 0 && pe.copy_src_size.x + 1 >= gfx->RenderWidth())
				{
					gfx->GPDisplayCopy();
				}

				// A texture copy's clear belongs to the copy itself and runs right here: it only
				// prepares the EFB for the copies that follow, and it cannot disturb the frame a
				// display copy presents. The clear of a *display* copy stays deferred to the frame
				// begin: the backend shows the EFB in place of the XFB the hardware would have
				// written first, so clearing it here would wipe the picture that is about to be
				// shown (the bootrom screen went black that way).
				if (pe.copy_cmd.clear && pe.copy_cmd.opcode == PE_COPY_CMD_TEXTURE)
				{
					ApplyCopyClear(clear);
				}
				break;
			}

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

	//! Read a rectangle of the EFB into an RGB buffer, top row first. This is the only way back from
	//! the copy engine's round trip through the colour buffer, which is what makes it a pixel engine
	//! operation (gfx-pe.md 5).
	bool PixelEngine::ReadEfb(int x, int y, int width, int height, std::vector<uint8_t>& rgb)
	{
		if (width <= 0 || height <= 0 || gfx == nullptr || !gfx->HasGLContext())
		{
			return false;
		}

		rgb.resize((size_t)width * height * 3);

		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());

		// glReadPixels returns the bottom row first
		std::vector<uint8_t> flipped(rgb.size());
		for (int row = 0; row < height; row++)
		{
			memcpy(&flipped[(size_t)row * width * 3],
				&rgb[(size_t)(height - 1 - row) * width * 3], (size_t)width * 3);
		}
		rgb.swap(flipped);

		return true;
	}

	// The copy engine's clear fills the EFB with the PE clear colour and the clear Z, without depth
	// testing or blending, using the values the copy that asked for it was programmed with (see
	// CopyClearState). The hardware clears only the rectangle the copy read, but the backend displays
	// the whole EFB (a real console shows the scaled XFB instead), so the whole render target is
	// cleared here: leaving the rest of it alone smeared the previous frame into the part of the
	// picture the copy does not cover.
	// The copy engine turns a rectangle of the EFB into a tiled texture in main memory. The tiling
	// is the one the texture unit reads back (see tx.cpp) applied in reverse: the tiles of a
	// texture are stored row by row, and the shape of a tile follows the texel size of the
	// destination format. The tile rows are `PE_COPY_DST_STRIDE` cache lines apart, so they are
	// not necessarily contiguous (gfx-pe.md 5.7).
	void PixelEngine::TextureCopy()
	{
		int w = (int)pe.copy_src_size.x + 1;
		int h = (int)pe.copy_src_size.y + 1;
		int srcX = (int)pe.copy_src_addr.x;
		int srcY = (int)pe.copy_src_addr.y;
		int fmt = (int)pe.copy_cmd.tex_format | ((int)pe.copy_cmd.tex_format_h << 3);
		uint32_t dst = (uint32_t)pe.copy_dst_base[0].base << 5;

		if (w <= 0 || h <= 0)
			return;

		// A rectangle that runs off the EFB is clamped: the tiles it covers are written anyway.
		if (srcX < 0) { w += srcX; srcX = 0; }
		if (srcY < 0) { h += srcY; srcY = 0; }
		if (srcX + w > (int)gfx->RenderWidth()) w = (int)gfx->RenderWidth() - srcX;
		if (srcY + h > (int)gfx->RenderHeight()) h = (int)gfx->RenderHeight() - srcY;

		if (w <= 0 || h <= 0)
			return;

		// The shape of a tile follows the texel size of the format (gfx-pe.md 5.7).
		int tileW = 4, tileH = 4;

		switch (fmt)
		{
			case TF_I4:
			case TF_C4:
				tileW = 8; tileH = 8;
				break;

			case TF_I8:
			case TF_IA4:
			case TF_C8:
				tileW = 8; tileH = 4;
				break;

			case TF_IA8:
			case TF_RGB565:
			case TF_RGB5A3:
			case TF_C14:
			case TF_RGBA8:
				tileW = 4; tileH = 4;
				break;

			default:
				return;
		}

		// The copy engine reads whole tiles: the destination holds the tiles the rectangle covers,
		// so a rectangle that is not a multiple of the tile size is padded with the edge texels.
		int tilesX = (w + tileW - 1) / tileW;
		int tilesY = (h + tileH - 1) / tileH;

		// The copy engine reads its source out of the EFB, which is this unit's own colour buffer:
		// the rectangle is given in screen coordinates and glReadPixels counts from the bottom.
		std::vector<uint8_t> rgb;
		if (!ReadEfb(srcX, (int)gfx->RenderHeight() - srcY - h, w, h, rgb))
			return;

		auto texel = [&](int x, int y, int c) -> uint8_t
		{
			if (x >= w) x = w - 1;
			if (y >= h) y = h - 1;
			if (x < 0) x = 0;
			if (y < 0) y = 0;
			return rgb[((size_t)y * w + x) * 3 + c];
		};

		// The intensity formats take the luma of the EFB colour: the RGB to Y conversion of the
		// copy path is applied automatically for a copy into an intensity format (gfx-pe.md 5.4).
		auto luma = [&](int x, int y) -> uint8_t
		{
			int r = texel(x, y, 0), g = texel(x, y, 1), b = texel(x, y, 2);
			int y_ = (66 * r + 129 * g + 25 * b + 128) >> 8;
			return (uint8_t)(y_ < 0 ? 0 : (y_ > 255 ? 255 : y_));
		};

		uint8_t* dstPtr = (uint8_t*)Flipper::HW->mem->MIGetMemoryPointerForPI(dst);
		if (dstPtr == nullptr)
			return;

		// The stride is the distance between the first texel of a tile and the first texel of the
		// tile below it, in cache lines.
		size_t stride = (size_t)pe.copy_dst_stride.stride * 32;

		for (int ty = 0; ty < tilesY; ty++)
		{
			// A tile of a 32-bit format spans two cache lines, every other one is a single line.
			uint8_t* p = dstPtr + (size_t)ty * stride;

			for (int tx = 0; tx < tilesX; tx++)
			{
				int bx = tx * tileW;
				int by = ty * tileH;

				switch (fmt)
				{
					case TF_I4:
					{
						for (int v = 0; v < tileH; v++)
							for (int u = 0; u < tileW; u += 2)
							{
								uint8_t hi = luma(bx + u, by + v) >> 4;
								uint8_t lo = luma(bx + u + 1, by + v) >> 4;
								*p++ = (uint8_t)((hi << 4) | lo);
							}
						break;
					}

					case TF_I8:
					{
						for (int v = 0; v < tileH; v++)
							for (int u = 0; u < tileW; u++)
								*p++ = luma(bx + u, by + v);
						break;
					}

					case TF_IA4:
					{
						for (int v = 0; v < tileH; v++)
							for (int u = 0; u < tileW; u++)
								*p++ = (uint8_t)((texel(bx + u, by + v, 3) & 0xf0) | (luma(bx + u, by + v) >> 4));
						break;
					}

					case TF_IA8:
					{
						for (int v = 0; v < tileH; v++)
							for (int u = 0; u < tileW; u++)
							{
								*p++ = texel(bx + u, by + v, 3);
								*p++ = luma(bx + u, by + v);
							}
						break;
					}

					case TF_RGB565:
					{
						for (int v = 0; v < tileH; v++)
							for (int u = 0; u < tileW; u++)
							{
								uint16_t c = (uint16_t)(((texel(bx + u, by + v, 0) >> 3) << 11) |
									((texel(bx + u, by + v, 1) >> 2) << 5) |
									(texel(bx + u, by + v, 2) >> 3));
								*p++ = (uint8_t)(c >> 8);
								*p++ = (uint8_t)c;
							}
						break;
					}

					case TF_RGB5A3:
					{
						for (int v = 0; v < tileH; v++)
							for (int u = 0; u < tileW; u++)
							{
								int r = texel(bx + u, by + v, 0), g = texel(bx + u, by + v, 1), b = texel(bx + u, by + v, 2);
								int a = texel(bx + u, by + v, 3);
								uint16_t c;
								if (a > 0xe0)
								{
									c = (uint16_t)(0x8000 | ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3));
								}
								else
								{
									c = (uint16_t)(((a >> 5) << 12) | ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4));
								}
								*p++ = (uint8_t)(c >> 8);
								*p++ = (uint8_t)c;
							}
						break;
					}

					case TF_RGBA8:
					{
						// A 4x4 block of this format spans two cache lines: alpha/red pairs first,
						// then the green/blue ones.
						for (int v = 0; v < 4; v++)
							for (int u = 0; u < 4; u++)
							{
								*p++ = texel(bx + u, by + v, 3);
								*p++ = texel(bx + u, by + v, 0);
							}
						for (int v = 0; v < 4; v++)
							for (int u = 0; u < 4; u++)
							{
								*p++ = texel(bx + u, by + v, 1);
								*p++ = texel(bx + u, by + v, 2);
							}
						break;
					}

					default:
						return;
				}
			}
		}
	}

	void PixelEngine::ApplyCopyClear(const CopyClearState& clear)
	{
		// A texture copy's clear covers the rectangle the copy reads (gfx-pe.md 5.1): the copy engine
		// turns every quad it reads into the clear colour and leaves the rest of the EFB as it was.
		// A display copy's clear covers the whole buffer instead, because this backend shows the EFB
		// where a console scans out the XFB the copy wrote (see CopyClearState::full).
		int x = clear.x, y = clear.y, w = clear.w, h = clear.h;

		if (clear.full || w <= 0 || h <= 0)
		{
			x = 0; y = 0; w = (int)gfx->scr_w; h = (int)gfx->scr_h;
		}

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
			(float)clear.ar.red / 255.0f,
			(float)clear.gb.green / 255.0f,
			(float)clear.gb.blue / 255.0f,
			(float)clear.ar.alpha / 255.0f);
		glClearDepth((double)(clear.z.value / 16777215.0));
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// The clear bypassed the GL state the registers describe, so it is re-applied
		ApplyZMode();
		ApplyColorMode();

		glScissor(0, 0, (GLsizei)gfx->scr_w, (GLsizei)gfx->scr_h);
	}

	bool PixelEngine::ApplyPendingCopyClears()
	{
		if (pending_clear_count == 0)
		{
			return false;
		}

		for (size_t i = 0; i < pending_clear_count; i++)
		{
			ApplyCopyClear(pending_clears[i]);
		}

		pending_clear_count = 0;
		return true;
	}

	void PixelEngine::Reset()
	{
		pe = PEState{};
		peregs = PERegs{};
		pending_clear_count = 0;

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